// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "CircularDependencyDetector.h"

#include "Application.h"
#include "ApplicationModule.h"
#include "DeviceManager.h"

/**********************************************************************************************************************/

namespace ChimeraTK::detail {

  void CircularDependencyDetector::registerDependencyWait(VariableNetworkNode& node) {
    assert(node.getType() == NodeType::Application);
    std::unique_lock<std::mutex> lock(_mutex);

    auto* dependent = dynamic_cast<Module*>(node.getOwningModule())->findApplicationModule();

    // register the waiting node in the map (used for the detectBlockedModules() thread). This is done also for Device
    // variables etc.
    _awaitedNodes[dependent] = node;

    // checking of direct circular dependencies is only done for Application-type feeders
    auto feedingModule = node.getModel().visit(Model::returnApplicationModule, Model::keepPvAccess,
        Model::keepApplicationModules, Model::adjacentInSearch, Model::returnFirstHit(Model::ApplicationModuleProxy{}));
    if(!feedingModule.isValid()) {
      return;
    }
    Module* dependency = &feedingModule.getApplicationModule();

    // If a module depends on itself, the detector would always detect a circular dependency, even if it is resolved
    // by writing initial values in prepare(). Hence we do not check anything in this case.
    if(dependent == dependency) {
      return;
    }

    // Register dependent-dependency relation in map
    _awaitedVariables[dependent] = node.getQualifiedName();
    _waitMap[dependent] = dependency;

    // check for circular dependencies
    auto* depdep = dependency;
    while(_waitMap.find(depdep) != _waitMap.end()) {
      auto* depdep_prev = depdep;
      depdep = _waitMap[depdep];
      if(depdep == dependent) {
        // The detected circular dependency might still resolve itself, because registerDependencyWait() is called even
        // if initial values are already present in the queues.
        for(size_t i = 0; i < 1000; ++i) {
          // give other thread a chance to read their initial value
          lock.unlock();
          usleep(10000);
          lock.lock();
          // if the module depending on an initial value for "us" is no longer waiting for us to send an initial value,
          // the circular dependency is resolved.
          if(_waitMap.find(depdep_prev) == _waitMap.end() || _waitMap[depdep] != dependent) {
            return;
          }
        }

        std::cerr << "*** Circular dependency of ApplicationModules found while waiting for initial values!"
                  << std::endl;
        std::cerr << std::endl;

        std::cerr << dependent->getQualifiedNameWithType() << " waits for " << node.getQualifiedName()
                  << " from:" << std::endl;
        auto* depdep2 = dependency;
        while(_waitMap.find(depdep2) != _waitMap.end()) {
          auto waitsFor = _awaitedVariables[depdep2];
          std::cerr << depdep2->getQualifiedNameWithType();
          if(depdep2 == dependent) {
            break;
          }
          depdep2 = _waitMap[depdep2];
          std::cerr << " waits for " << waitsFor << " from:" << std::endl;
        }
        std::cerr << "." << std::endl;

        std::cerr << std::endl;
        std::cerr
            << "Please provide an initial value in the prepare() function of one of the involved ApplicationModules!"
            << std::endl;

        throw ChimeraTK::logic_error("Circular dependency of ApplicationModules while waiting for initial values");
      }
      // Give other threads a chance to add to the wait map
      lock.unlock();
      usleep(10000);
      lock.lock();
    }
  }

  /********************************************************************************************************************/

  void CircularDependencyDetector::printWaiters(std::ostream& stream) {
    if(_waitMap.empty()) {
      return;
    }
    stream << "The following modules are still waiting for initial values:" << std::endl;
    for(auto& waiters : _waitMap) {
      stream << waiters.first->getQualifiedNameWithType() << " waits for " << _awaitedVariables[waiters.first]
             << " from " << waiters.second->getQualifiedNameWithType() << std::endl;
    }
    stream << "(end of list)" << std::endl;
  }

  /********************************************************************************************************************/

  void CircularDependencyDetector::unregisterDependencyWait(VariableNetworkNode& node) {
    assert(node.getType() == NodeType::Application);
    std::lock_guard<std::mutex> lock(_mutex);
    auto* mod = dynamic_cast<Module*>(node.getOwningModule())->findApplicationModule();
    _waitMap.erase(mod);
    _awaitedVariables.erase(mod);
    _awaitedNodes.erase(mod);
  }

  /********************************************************************************************************************/

  void CircularDependencyDetector::startDetectBlockedModules() {
    _thread = boost::thread([this] { detectBlockedModules(); });
  }

  /********************************************************************************************************************/

  void CircularDependencyDetector::detectBlockedModules() {
    Application::registerThread("CircDepDetector");

    auto& app = Application::getInstance();
    while(true) {
      // wait some time to slow down this check
      boost::this_thread::sleep_for(boost::chrono::seconds(60));

      // check for modules which did not yet reach their mainLoop
      bool allModulesEnteredMainLoop = true;
      for(auto* module : app.getSubmoduleListRecursive()) {
        // Note: We are "abusing" this flag which was introduced for the testable mode. It actually just shows whether
        // the mainLoop() has been called already (resp. will be called very soon). It is not depending on the
        // testableMode. FIXME: Rename this flag!
        if(module->hasReachedTestableMode()) {
          continue;
        }

        // Found a module which is still waiting for initial values
        allModulesEnteredMainLoop = false;

        // require lock against concurrent accesses on the maps
        std::lock_guard<std::mutex> lock(_mutex);

        // Check if module has registered a dependency wait. If not, situation will either resolve soon or module will
        // register a dependency wait soon. Both cases can be found in the next iteration.
        if(_awaitedNodes.find(module) == _awaitedNodes.end()) {
          continue;
        }

        auto* appModule = dynamic_cast<ApplicationModule*>(module);
        if(!appModule) {
          // only ApplicationModule can have hasReachedTestableMode() == true, so it should not happen
          logger(Logger::Severity::warning, "CircularDependencyDetector")
              << "found non-application module: " << module->getQualifiedNameWithType() << std::endl;
          continue;
        }

        // Iteratively search for reason the module blocks
        std::set<Module*> searchedModules;
        std::function<void(const VariableNetworkNode& node)> iterativeSearch = [&](const VariableNetworkNode& node) {
          auto visitor = [&](auto proxy) {
            if constexpr(Model::isApplicationModule(proxy)) {
              // fed by other ApplicationModule: check if that one is waiting, too
              auto* feedingAppModule = dynamic_cast<ApplicationModule*>(&proxy.getApplicationModule());
              if(feedingAppModule->hasReachedTestableMode()) {
                // the feeding module has started its mainLoop(), but not sent us an initial value
                // FIXME: There is a race condition, if the situation has right now resolved and the feeding module just
                // not yet sent the initial value. Ideally we should wait another iteration before printing warnings!
                if(_modulesWeHaveWarnedAbout.find(feedingAppModule) == _modulesWeHaveWarnedAbout.end()) {
                  _modulesWeHaveWarnedAbout.insert(feedingAppModule);
                  logger(Logger::Severity::warning, "CircularDependencyDetector")
                      << "Note: ApplicationModule " << appModule->getQualifiedNameWithType() << " is waiting for an "
                      << "initial value, because " << feedingAppModule->getQualifiedNameWithType()
                      << " has not yet sent one." << std::endl;
                }
                return;
              }
              if(_awaitedNodes.find(feedingAppModule) == _awaitedNodes.end()) {
                // The other module is not right now waiting. It will either enter the mainLoop soon, or start
                // waiting soon. Let's just wait another iteration.
                return;
              }
              // The other module is right now waiting: continue iterative search
              bool notYetSearched = searchedModules.insert(feedingAppModule).second;
              if(notYetSearched) {
                iterativeSearch(_awaitedNodes.at(feedingAppModule));
              }
              else {
                if(_modulesWeHaveWarnedAbout.find(feedingAppModule) == _modulesWeHaveWarnedAbout.end()) {
                  _modulesWeHaveWarnedAbout.insert(feedingAppModule);
                  logger(Logger::Severity::warning, "CircularDependencyDetector")
                      << "Note: ApplicationModule " << appModule->getQualifiedNameWithType() << " and "
                      << feedingAppModule->getQualifiedNameWithType()
                      << " are both waiting, on initial value provided directly or indirectly by the other."
                      << std::endl;
                }
              }
            }
            else if constexpr(Model::isDeviceModule(proxy)) {
              // fed by device
              const auto& deviceName = proxy.getAliasOrCdd();
              if(_devicesWeHaveWarnedAbout.find(deviceName) == _devicesWeHaveWarnedAbout.end()) {
                _devicesWeHaveWarnedAbout.insert(deviceName);
                auto myLog = logger(Logger::Severity::warning, "CircularDependencyDetector");
                myLog << "Note: Still waiting for device " << deviceName << " to come up";
                auto dm = Application::getInstance()._deviceManagerMap[deviceName];
                std::unique_lock dmLock(dm->_errorMutex);
                if(dm->_deviceHasError) {
                  myLog << " (" << std::string(dm->_deviceError._message) << ")";
                }
                myLog << "...";
              }
            }
            else {
              // fed by anything else?
              logger(Logger::Severity::warning, "CircularDependencyDetector")
                  << "At least one ApplicationModule " << appModule->getQualifiedNameWithType() << " is waiting for an "
                  << "initial value from an unexpected source."
                  << "\n"
                  << "This is probably a BUG in the ChimeraTK framework.";
            }
          };

          node.getModel().visit(visitor, Model::keepPvAccess, Model::adjacentInSearch);
        };
        iterativeSearch(_awaitedNodes.at(appModule));
      }

      // if all modules are in the mainLoop, stop this thread
      if(allModulesEnteredMainLoop) {
        break;
      }
    }

    logger(Logger::Severity::info, "CircularDependencyDetector") << "All application modules are running.";

    // free some memory
    _waitMap.clear();
    _awaitedVariables.clear();
    _awaitedNodes.clear();
    _modulesWeHaveWarnedAbout.clear();
    _devicesWeHaveWarnedAbout.clear();
    _otherThingsWeHaveWarnedAbout.clear();
  }

  /********************************************************************************************************************/

  void CircularDependencyDetector::terminate() {
    if(_thread.joinable()) {
      _thread.interrupt();
      _thread.join();
    }
  }

  /********************************************************************************************************************/

  CircularDependencyDetector::~CircularDependencyDetector() {
    assert(!_thread.joinable());
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK::detail
