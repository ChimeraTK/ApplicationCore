// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Application.h"

#include "CircularDependencyDetector.h"
#include "ConnectionMaker.h"
#include "DeviceManager.h"
#include "Utilities.h"
#include "VariableNetworkGraphDumpingVisitor.h"
#include "XMLGeneratorVisitor.h"

#include <ChimeraTK/BackendFactory.h>

#include <boost/fusion/container/map.hpp>

#include <exception>
#include <fstream>
#include <string>
#include <thread>

using namespace ChimeraTK;

/*********************************************************************************************************************/

Application::Application(const std::string& name) : ApplicationBase(name), ModuleGroup(nullptr, name) {
  // Create the model and its root.
  Application::_model = Model::RootProxy(*this);

  // Make sure the ModuleGroup base class has the model, too.
  ModuleGroup::_model = Model::ModuleGroupProxy(_model);

  // check if the application name has been set
  if(applicationName.empty()) {
    Application::shutdown();
    throw ChimeraTK::logic_error("Error: An instance of Application must have its applicationName set.");
  }
  // check if application name contains illegal characters
  if(!Utilities::checkName(name, false)) {
    Application::shutdown();
    throw ChimeraTK::logic_error(
        "Error: The application name may only contain alphanumeric characters and underscores.");
  }
}

/*********************************************************************************************************************/

Application::~Application() {
  if(lifeCycleState == LifeCycleState::initialisation && !hasBeenShutdown) {
    // likely an exception has been thrown in the initialisation phase, in which case we better call shutdown to prevent
    // ApplicationBase from complaining and hiding the exception
    ApplicationBase::shutdown();
  }
}

/*********************************************************************************************************************/

void Application::enableTestableMode() {
  assert(not initialiseCalled);
  testableMode.enable();
}

/*********************************************************************************************************************/

void Application::registerThread(const std::string& name) {
  getInstance().testableMode.setThreadName(name);
  pthread_setname_np(pthread_self(), name.substr(0, std::min<std::string::size_type>(name.length(), 15)).c_str());
}

/*********************************************************************************************************************/

void Application::incrementDataLossCounter(const std::string& name) {
  if(getInstance().debugDataLoss) {
    std::cout << "Data loss in variable " << name << std::endl;
  }
  getInstance().dataLossCounter++;
}

/*********************************************************************************************************************/

size_t Application::getAndResetDataLossCounter() {
  size_t counter = getInstance().dataLossCounter.load(std::memory_order_relaxed);
  while(!getInstance().dataLossCounter.compare_exchange_weak(
      counter, 0, std::memory_order_release, std::memory_order_relaxed)) {
  }
  return counter;
}

/*********************************************************************************************************************/

void Application::initialise() {
  if(initialiseCalled) {
    throw ChimeraTK::logic_error("Application::initialise() was already called before.");
  }

  if(!getPVManager()) {
    throw ChimeraTK::logic_error("Application::initialise() was called without an instance of ChimeraTK::PVManager.");
  }

  ConnectionMaker cm(*this);
  cm.setDebugConnections(enableDebugMakeConnections);
  cm.connect();

  initialiseCalled = true;
}

/*********************************************************************************************************************/

void Application::optimiseUnmappedVariables(const std::set<std::string>& /*names*/) {
  // TODO: This is not working anymore
  // Follow up with https://redmine.msktools.desy.de/issues/10371
}

/*********************************************************************************************************************/

void Application::run() {
  assert(not applicationName.empty());

  if(testableMode.enabled) {
    if(!testFacilityRunApplicationCalled) {
      throw ChimeraTK::logic_error(
          "Testable mode enabled but Application::run() called directly. Call TestFacility::runApplication() instead.");
    }
  }

  if(runCalled) {
    throw ChimeraTK::logic_error("Application::run() has already been called before.");
  }
  runCalled = true;

  // set all initial version numbers in the modules to the same value
  for(auto& module : getSubmoduleListRecursive()) {
    if(module->getModuleType() != ModuleType::ApplicationModule) continue;
    module->setCurrentVersionNumber(getStartVersion());
  }

  // prepare the modules
  for(auto& module : getSubmoduleListRecursive()) {
    module->prepare();
  }

  // Switch life-cycle state to run
  lifeCycleState = LifeCycleState::run;

  // start the necessary threads for the FanOuts etc.
  for(auto& internalModule : internalModuleList) {
    internalModule->activate();
  }

  // start the threads for the modules
  for(auto& module : getSubmoduleListRecursive()) {
    module->run();
  }

  // When in testable mode, wait for all modules to report that they have reached the testable mode.
  // We have to start all module threads first because some modules might only send the initial
  // values in their main loop, and following modules need them to enter testable mode.

  // just a small helper lambda to avoid code repetition
  auto waitForTestableMode = [](EntityOwner* module) {
    while(!module->hasReachedTestableMode()) {
      Application::getInstance().getTestableMode().unlock("releaseForReachTestableMode");
      usleep(100);
      Application::getInstance().getTestableMode().lock("acquireForReachTestableMode");
    }
  };

  if(Application::getInstance().getTestableMode().enabled) {
    for(auto& internalModule : internalModuleList) {
      waitForTestableMode(internalModule.get());
    }

    for(auto& module : getSubmoduleListRecursive()) {
      waitForTestableMode(module);
    }
  }

  // Launch circular dependency detector thread
  circularDependencyDetector.startDetectBlockedModules();
}

/*********************************************************************************************************************/

void Application::shutdown() {
  // switch life-cycle state
  lifeCycleState = LifeCycleState::shutdown;

  // first allow to run the application threads again, if we are in testable
  // mode
  if(testableMode.enabled && testableMode.testLock()) {
    testableMode.unlock("shutdown");
  }

  // deactivate the FanOuts first, since they have running threads inside
  // accessing the modules etc. (note: the modules are members of the
  // Application implementation and thus get destroyed after this destructor)
  for(auto& internalModule : internalModuleList) {
    internalModule->deactivate();
  }

  // shutdown all DeviceManagers, otherwise application modules might hang if still waiting for initial values from
  // devices
  for(auto& pair : _deviceManagerMap) {
    pair.second->terminate();
  }

  // next deactivate the modules, as they have running threads inside as well
  for(auto& module : getSubmoduleListRecursive()) {
    module->terminate();
  }

  circularDependencyDetector.terminate();

  ApplicationBase::shutdown();
}

/*********************************************************************************************************************/


/*********************************************************************************************************************/

void Application::generateXML() {
  assert(not applicationName.empty());

  XMLGenerator generator{*this};
  generator.run();
  generator.save(applicationName + ".xml");
}

/*********************************************************************************************************************/

void Application::dumpConnections(std::ostream& stream) { // LCOV_EXCL_LINE
#if 0
  stream << "==== List of all variable connections of the current Application =====" << std::endl; // LCOV_EXCL_LINE
  for(auto& network : networkList) {                                                               // LCOV_EXCL_LINE
    network.dump("", stream);                                                                      // LCOV_EXCL_LINE
  }                                                                                                // LCOV_EXCL_LINE
  stream << "==== List of all circular connections in the current Application ====" << std::endl;  // LCOV_EXCL_LINE
  for(auto& circularDependency : circularDependencyNetworks) {
    stream << "Circular dependency network " << circularDependency.first << " : ";
    for(auto& module : circularDependency.second) {
      stream << module->getName() << ", ";
    }
    stream << std::endl;
  }
  stream << "======================================================================" << std::endl; // LCOV_EXCL_LINE
#endif
} // LCOV_EXCL_LINE

/*********************************************************************************************************************/
/*
void Application::dumpConnectionGraph(const std::string& fileName) const {
  std::fstream file{fileName, std::ios_base::out};

  VariableNetworkGraphDumpingVisitor visitor{file};
  visitor.dispatch(*this);
}
*/
/*********************************************************************************************************************/
/*
void Application::dumpModuleConnectionGraph(const std::string& fileName) const {
  std::fstream file{fileName, std::ios_base::out};

  VariableNetworkModuleGraphDumpingVisitor visitor{file};
  visitor.dispatch(*this);
}
*/
/*********************************************************************************************************************/

/*
void Application::markCircularConsumers(VariableNetwork& variableNetwork) {
  for(auto& node : variableNetwork.getConsumingNodes()) {
    // A variable network is a tree-like network of VariableNetworkNodes (one feeder and one or more multiple consumers)
    // A circular network is a list of modules (EntityOwners) which have a circular dependency
    auto circularNetwork = node.scanForCircularDepencency();
    if(not circularNetwork.empty()) {
      auto circularNetworkHash = boost::hash_range(circularNetwork.begin(), circularNetwork.end());
      circularDependencyNetworks[circularNetworkHash] = circularNetwork;
      circularNetworkInvalidityCounters[circularNetworkHash] = 0;
    }
  }
}
*/

/*********************************************************************************************************************/

Application& Application::getInstance() {
  return dynamic_cast<Application&>(ApplicationBase::getInstance());
}

/*********************************************************************************************************************/

boost::shared_ptr<DeviceManager> Application::getDeviceManager(const std::string& aliasOrCDD) {
  auto& dmm = Application::getInstance()._deviceManagerMap;
  if(dmm.find(aliasOrCDD) == dmm.end()) {
    // Add initialisation handler below, since we also need to add it if the DeviceModule already exists
    dmm[aliasOrCDD] = boost::make_shared<DeviceManager>(&Application::getInstance(), aliasOrCDD);
  }
  return dmm.at(aliasOrCDD);
}

/*********************************************************************************************************************/
