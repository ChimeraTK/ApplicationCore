/*
 * ApplicationModule.cc
 *
 *  Created on: Jun 17, 2016
 *      Author: Martin Hierholzer
 */

#include "ApplicationCore.h"
#include "ConfigReader.h"
#include <iterator>
#include <list>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  ApplicationModule::ApplicationModule(EntityOwner* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, hierarchyModifier, tags) {
    if(!dynamic_cast<ModuleGroup*>(owner) && !dynamic_cast<Application*>(owner)) {
      throw ChimeraTK::logic_error("ApplicationModules must be owned either by ModuleGroups or the Application!");
    }
    if(name.find_first_of("/") != std::string::npos) {
      throw ChimeraTK::logic_error(
          "Module names must not contain slashes: '" + name + " owned by '" + owner->getQualifiedName() + "'.");
    }
  }

  /*********************************************************************************************************************/

  ApplicationModule::ApplicationModule(EntityOwner* owner, const std::string& name, const std::string& description,
      bool eliminateHierarchy, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, eliminateHierarchy, tags) {
    if(!dynamic_cast<ModuleGroup*>(owner) && !dynamic_cast<Application*>(owner)) {
      throw ChimeraTK::logic_error("ApplicationModules must be owned either by ModuleGroups or the Application!");
    }
    if(name.find_first_of("/") != std::string::npos) {
      throw ChimeraTK::logic_error(
          "Module names must not contain slashes: '" + name + " owned by '" + owner->getQualifiedName() + "'.");
    }
  }

  /*********************************************************************************************************************/

  void ApplicationModule::run() {
    // start the module thread
    assert(!moduleThread.joinable());
    moduleThread = boost::thread(&ApplicationModule::mainLoopWrapper, this);
  }

  /*********************************************************************************************************************/

  void ApplicationModule::terminate() {
    if(moduleThread.joinable()) {
      moduleThread.interrupt();
      // try joining the thread
      while(!moduleThread.try_join_for(boost::chrono::milliseconds(10))) {
        // if thread is not yet joined, send interrupt() to all variables.
        for(auto& var : getAccessorListRecursive()) {
          auto el{var.getAppAccessorNoType().getHighLevelImplElement()};
          if(el->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
            el->interrupt();
          }
        }
        // it may not suffice to send interrupt() once, as the exception might get
        // overwritten in the queue, thus we repeat this until the thread was
        // joined.
      }
    }
    assert(!moduleThread.joinable());
  }

  /*********************************************************************************************************************/

  ApplicationModule::~ApplicationModule() { assert(!moduleThread.joinable()); }

  /*********************************************************************************************************************/

  void ApplicationModule::mainLoopWrapper() {
    Application::registerThread("AM_" + getName());

    // Acquire testable mode lock, so from this point on we are running only one user thread concurrently
    Application::testableModeLock("start");

    // Read all variables once to obtain the initial values from the devices and from the control system persistency
    // layer. This is done in two steps, first for all poll-type variables and then for all push-types, because
    // poll-type reads might trigger distribution of values to push-type variables via a ConsumingFanOut.
    for(auto& variable : getAccessorListRecursive()) {
      if(variable.getDirection().dir != VariableDirection::consuming) continue;
      if(variable.getMode() == UpdateMode::poll) {
        assert(!variable.getAppAccessorNoType().getHighLevelImplElement()->getAccessModeFlags().has(
            AccessMode::wait_for_new_data));
        Application::testableModeUnlock("Initial value read for poll-type " + variable.getName());
        Application::getInstance().circularDependencyDetector.registerDependencyWait(variable);
        variable.getAppAccessorNoType().read();
        Application::getInstance().circularDependencyDetector.unregisterDependencyWait(variable);
        if(!Application::testableModeTestLock()) {
          // The lock may have already been acquired if the above read() goes to a ConsumingFanOut, which sends out
          // the data to a slave decorated by a TestableModeAccessorDecorator. Hence we heer must acquire the lock only
          // if we do not have it.
          Application::testableModeLock("Initial value read for poll-type " + variable.getName());
        }
      }
    }
    for(auto& variable : getAccessorListRecursive()) {
      if(variable.getDirection().dir != VariableDirection::consuming) continue;
      if(variable.getMode() == UpdateMode::push) {
        Application::testableModeUnlock("Initial value read for push-type " + variable.getName());
        Application::getInstance().circularDependencyDetector.registerDependencyWait(variable);
        Application::testableModeLock("Initial value read for push-type " + variable.getName());
        variable.getAppAccessorNoType().read();
        Application::testableModeUnlock("Initial value read for push-type " + variable.getName());
        Application::getInstance().circularDependencyDetector.unregisterDependencyWait(variable);
        Application::testableModeLock("Initial value read for push-type " + variable.getName());
      }
    }

    // We are holding the testable mode lock, so we are sure the mechanism will work now.
    testableModeReached = true;

    // enter the main loop
    mainLoop();
    Application::testableModeUnlock("terminate");
  }

  /*********************************************************************************************************************/

  void ApplicationModule::incrementDataFaultCounter() { ++dataFaultCounter; }

  void ApplicationModule::decrementDataFaultCounter() {
    assert(dataFaultCounter > 0);
    --dataFaultCounter;
  }

  /*********************************************************************************************************************/

  std::list<EntityOwner*> ApplicationModule::getInputModulesRecursively(std::list<EntityOwner*> startList) {
    // If this module is already in the list we found a circular dependency.
    // Add this module again, so the caller will see also see the circle, and return.
    if(std::count(startList.begin(), startList.end(), this)) {
      startList.push_back(this);
      return startList;
    }

    // loop all inputs
    startList.push_back(this); // first we add this module to the start list. We will call all inputs with it.
    std::list<EntityOwner*> returnList{
        startList}; // prepare the return list. Deltas from the inputs will be added to it.
    for(auto& accessor : this->getAccessorListRecursive()) {
      if(accessor.getDirection().dir != VariableDirection::consuming) continue; // not an input (consuming from network)

      // find the feeder in the network
      auto feeder = accessor.getOwner().getFeedingNode();
      auto feedingModule = feeder.getOwningModule();
      // CS module and DeviceModule nodes don't have an owning module set. As they stop the recursion anyway we just continue.
      if(!feedingModule) {
        continue;
      }

      auto thisInputsRecursiveModuleList = feedingModule->getInputModulesRecursively(startList);
      // only add the modules that were added by the recursive search to the output list
      assert(startList.size() <= thisInputsRecursiveModuleList.size());
      auto copyStartIter = thisInputsRecursiveModuleList.begin();
      std::advance(copyStartIter, startList.size());

      returnList.insert(returnList.end(), copyStartIter, thisInputsRecursiveModuleList.end());
    }
    return returnList;
  }

  /*********************************************************************************************************************/

} /* namespace ChimeraTK */
