// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ApplicationModule.h"

#include "Application.h"
#include "CircularDependencyDetector.h"
#include "DeviceManager.h"
#include "Flags.h"
#include "ModuleGroup.h"

#include <ChimeraTK/SystemTags.h>
#include <ChimeraTK/Utilities.h>

#include <iterator>
#include <list>

namespace ChimeraTK {

  /********************************************************************************************************************/

  ApplicationModule::ApplicationModule(ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : VariableGroup(owner, name, description, tags) {
    if(!owner) {
      throw ChimeraTK::logic_error("ApplicationModule owner cannot be nullptr");
    }

    if(owner->getModel().isValid()) {
      _model = owner->getModel().add(*this);
      VariableGroup::_model = {};
      // Model::VariableGroupProxy(_model);
    }
  }

  /********************************************************************************************************************/

  ApplicationModule& ApplicationModule::operator=(ApplicationModule&& other) noexcept {
    assert(!_moduleThread.joinable()); // if the thread is already running, moving is no longer allowed!

    // Keep the model as is (except from updating the pointers to the C++ objects). To do so, we have to hide it from
    // unregisterModule() which is executed in Module::operator=(), because it would destroy the model.
    Model::ApplicationModuleProxy model = std::move(other._model);
    other._model = {};

    VariableGroup::operator=(std::move(other));

    if(model.isValid()) {
      model.informMove(*this);
    }
    _model = model;

    return *this;
  }

  /********************************************************************************************************************/

  void ApplicationModule::run() {
    // start the module thread
    assert(!_moduleThread.joinable());
    _moduleThread = boost::thread(&ApplicationModule::mainLoopWrapper, this);
  }

  /********************************************************************************************************************/

  void ApplicationModule::terminate() {
    if(_moduleThread.joinable()) {
      _moduleThread.interrupt();
      // try joining the thread
      while(!_moduleThread.try_join_for(boost::chrono::milliseconds(10))) {
        // if thread is not yet joined, send interrupt() to all variables.
        for(auto& var : getAccessorListRecursive()) {
          auto el{var.getAppAccessorNoType().getHighLevelImplElement()};
          el->interrupt();
        }
        // it may not suffice to send interrupt() once, as the exception might get
        // overwritten in the queue, thus we repeat this until the thread was
        // joined.
      }
    }
    assert(!_moduleThread.joinable());
  }

  /********************************************************************************************************************/

  ApplicationModule::~ApplicationModule() {
    assert(!_moduleThread.joinable());
  }

  /********************************************************************************************************************/

  void ApplicationModule::setCurrentVersionNumber(VersionNumber versionNumber) {
    if(versionNumber > _currentVersionNumber) {
      _currentVersionNumber = versionNumber;
    }
  }

  /********************************************************************************************************************/

  void ApplicationModule::mainLoopWrapper() {
    Application::registerThread("AM_" + className()); // + getName());

    // Testable mode: Determine whether a shared or an exclusive testable mode lock shall be used during the initial
    // startup phase of this ApplicationModule. Normal ApplicationModules use exclusive testable mode lock for the
    // startup phase for performance reasons, only DeviceManagers have to use a shared lock from the beginning to
    // avoid deadlocks in their RecoveryGroup barriers.
    // Once the actual mainLoop() is entered, we always use a shared lock, since it should be faster to allow parallel
    // execution of the ApplicationModules.
    bool useSharedTestLockForStartup = (dynamic_cast<DeviceManager*>(this) != nullptr);

    // Acquire testable mode lock, so from this point on we are excluding concurrent execution with the test thread.
    Application::getInstance().getTestableMode().lock("start", useSharedTestLockForStartup);

    auto accList = getAccessorListRecursive();

    // Read all variables once to obtain the initial values from the devices and from the control system persistency
    // layer. This is done in two steps, first for all poll-type variables and then for all push-types, because
    // poll-type reads might trigger distribution of values to push-type variables via a ConsumingFanOut.
    for(auto& variable : accList) {
      if(variable.getDirection().dir != VariableDirection::consuming) {
        continue;
      }
      if(variable.getMode() == UpdateMode::poll) {
        assert(!variable.getAppAccessorNoType().getHighLevelImplElement()->getAccessModeFlags().has(
            AccessMode::wait_for_new_data));
        Application::getInstance().getTestableMode().unlock("Initial value read for poll-type " + variable.getName());
        Application::getInstance()._circularDependencyDetector.registerDependencyWait(variable);
        variable.getAppAccessorNoType().read();
        Application::getInstance()._circularDependencyDetector.unregisterDependencyWait(variable);
        if(not Application::getInstance().getTestableMode().testLock()) {
          // The lock may have already been acquired if the above read() goes to a ConsumingFanOut, which sends out
          // the data to a slave decorated by a TestableModeAccessorDecorator. Hence we here must acquire the lock only
          // if we do not have it.
          Application::getInstance().getTestableMode().lock(
              "Initial value read for poll-type " + variable.getName(), useSharedTestLockForStartup);
        }
      }
    }

    for(auto& variable : accList) {
      // According to spec, readback values are not considered in the distribution of initial values
      // However, if they are declared as providing reverse recovery, they have to be considered.
      auto doNotSkipInIvDistribution = variable.getDirection().dir == VariableDirection::feeding &&
          variable.getDirection().withReturn && variable.getTags().contains(ChimeraTK::SystemTags::reverseRecovery);

      if(!doNotSkipInIvDistribution && variable.getDirection().dir != VariableDirection::consuming) {
        continue;
      }

      if(variable.getMode() == UpdateMode::push) {
        Application::getInstance().getTestableMode().unlock("Initial value read for push-type " + variable.getName());
        Application::getInstance()._circularDependencyDetector.registerDependencyWait(variable);
        // Will internally release and lock during the read, hence surround with lock/unlock
        Application::getInstance().getTestableMode().lock(
            "Initial value read for push-type " + variable.getName(), useSharedTestLockForStartup);
        variable.getAppAccessorNoType().read();
        Application::getInstance().getTestableMode().unlock("Initial value read for push-type " + variable.getName());
        Application::getInstance()._circularDependencyDetector.unregisterDependencyWait(variable);
        Application::getInstance().getTestableMode().lock(
            "Initial value read for push-type " + variable.getName(), useSharedTestLockForStartup);
      }
    }

    // downgrade to shared lock
    if(!useSharedTestLockForStartup) {
      Application::getInstance().getTestableMode().unlock("downgrade to shared lock");
      Application::getInstance().getTestableMode().lock("downgrade to shared lock", true);
    }

    // We are holding the testable mode lock, so we are sure the mechanism will work now.
    _testableModeReached = true;

    // enter the main loop
    mainLoop();
    Application::getInstance().getTestableMode().unlock("terminate");
  }

  /********************************************************************************************************************/

  void ApplicationModule::incrementDataFaultCounter() {
    ++_dataFaultCounter;
  }

  void ApplicationModule::decrementDataFaultCounter() {
    assert(_dataFaultCounter > 0);
    --_dataFaultCounter;
  }

  /********************************************************************************************************************/

  std::list<EntityOwner*> ApplicationModule::getInputModulesRecursively(std::list<EntityOwner*> startList) {
    if(_recursionStopper.recursionDetected()) {
      return startList;
    }

    // If this module is already in the list we found a circular dependency.
    // Remember this for the next time the recursive scan calls this function
    if(std::count(startList.begin(), startList.end(), this)) {
      _recursionStopper.setRecursionDetected();
    }

    // Whether a circular dependency has been detected or not, we must loop all inputs and add this module to the list
    // so the calling code sees the second instance and can also detect the circle. The reason why we have to scan all
    // inputs even if a circle is detected is this:
    // * A single input starts the scan by adding it's owning module. At this point not all inputs if that module are in
    // the circular network.
    // * When a circle is detected, it might only be one of multiple entangled circled. If we would break the recursion
    // and not scan all the
    //   inputs this is sufficient to identify that the particular input is in a circle. But at this point we have to
    //   tell in which network it is and have to scan the complete network to calculate the correct hash value.

    startList.push_back(this); // first we add this module to the start list. We will call all inputs with it.
    std::list<EntityOwner*> returnList{
        startList}; // prepare the return list. Deltas from the inputs will be added to it.
    for(auto& accessor : this->getAccessorListRecursive()) {
      // not consumed from network -> not an input, just continue
      if(accessor.getDirection().dir != VariableDirection::consuming) {
        continue;
      }

      // find the feeder in the network
      auto proxy = accessor.getModel().visit(Model::returnApplicationModule, Model::keepApplicationModules,
          Model::keepPvAccess, Model::adjacentInSearch, Model::returnFirstHit(Model::ApplicationModuleProxy{}));
      if(!proxy.isValid()) {
        continue;
      }
      auto& feedingModule = proxy.getApplicationModule();

      auto thisInputsRecursiveModuleList = feedingModule.getInputModulesRecursively(startList);
      // only add the modules that were added by the recursive search to the output list
      assert(startList.size() <= thisInputsRecursiveModuleList.size());
      auto copyStartIter = thisInputsRecursiveModuleList.begin();
      std::advance(copyStartIter, startList.size());

      returnList.insert(returnList.end(), copyStartIter, thisInputsRecursiveModuleList.end());
    }
    return returnList;
  }

  /********************************************************************************************************************/

  size_t ApplicationModule::getCircularNetworkHash() const {
    return _circularNetworkHash;
  }

  /********************************************************************************************************************/

  void ApplicationModule::setCircularNetworkHash(size_t circularNetworkHash) {
    if(_circularNetworkHash != 0 && _circularNetworkHash != circularNetworkHash) {
      throw ChimeraTK::logic_error(
          "Error: setCircularNetworkHash() called with different values for EntityOwner \"" + _name + "\" ");
    }
    if(Application::getInstance().getLifeCycleState() != LifeCycleState::initialisation) {
      throw ChimeraTK::logic_error("Error: setCircularNetworkHash() called after initialisation.");
    }
    _circularNetworkHash = circularNetworkHash;
  }

  /********************************************************************************************************************/

  DataValidity ApplicationModule::getDataValidity() const {
    if(_dataFaultCounter == 0) {
      return DataValidity::ok;
    }
    if(_circularNetworkHash != 0) {
      // In a circular dependency network, internal inputs are ignored.
      // If all external inputs (including the ones from this module) are OK, the
      // data validity is set to OK.
      if(Application::getInstance()._circularNetworkInvalidityCounters[_circularNetworkHash] == 0) {
        return DataValidity::ok;
      }
    }
    // not a circular network or invalidity counter not 0 -> keep the faulty flag
    return DataValidity::faulty;
  }

  /********************************************************************************************************************/

  void ApplicationModule::unregisterModule(Module* module) {
    VariableGroup::unregisterModule(module);
    if(_model.isValid()) {
      auto* vg = dynamic_cast<VariableGroup*>(module);
      if(!vg) {
        // during destruction unregisterModule is called from the base class destructor where the dynamic_cast already
        // fails.
        return;
      }
      _model.remove(*vg);
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
