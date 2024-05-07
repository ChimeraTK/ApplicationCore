// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Application.h"

#include "CircularDependencyDetector.h"
#include "ConnectionMaker.h"
#include "DeviceManager.h"
#include "Utilities.h"
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
  if(_lifeCycleState == LifeCycleState::initialisation && !hasBeenShutdown) {
    // likely an exception has been thrown in the initialisation phase, in which case we better call shutdown to prevent
    // ApplicationBase from complaining and hiding the exception
    ApplicationBase::shutdown();
  }
}

/*********************************************************************************************************************/

void Application::enableTestableMode() {
  assert(not _initialiseCalled);
  _testableMode.enable();
}

/*********************************************************************************************************************/

void Application::registerThread(const std::string& name) {
  getInstance()._testableMode.setThreadName(name);
}

/*********************************************************************************************************************/

void Application::incrementDataLossCounter(const std::string& name) {
  if(getInstance()._debugDataLoss) {
    logger(Logger::Severity::debug, "DataLossCounter") << "Data loss in variable " << name;
  }
  getInstance()._dataLossCounter++;
}

/*********************************************************************************************************************/

size_t Application::getAndResetDataLossCounter() {
  size_t counter = getInstance()._dataLossCounter.load(std::memory_order_relaxed);
  while(!getInstance()._dataLossCounter.compare_exchange_weak(
      counter, 0, std::memory_order_release, std::memory_order_relaxed)) {
  }
  return counter;
}

/*********************************************************************************************************************/

void Application::initialise() {
  if(_initialiseCalled) {
    throw ChimeraTK::logic_error("Application::initialise() was already called before.");
  }

  if(!getPVManager()) {
    throw ChimeraTK::logic_error("Application::initialise() was called without an instance of ChimeraTK::PVManager.");
  }

  _cm.setDebugConnections(_enableDebugMakeConnections);
  _cm.finalise();

  _initialiseCalled = true;
}

/*********************************************************************************************************************/

void Application::optimiseUnmappedVariables(const std::set<std::string>& names) {
  if(!_initialiseCalled) {
    throw ChimeraTK::logic_error(
        "Application::initialise() must be called before Application::optimiseUnmappedVariables().");
  }

  _cm.optimiseUnmappedVariables(names);
}

/*********************************************************************************************************************/

void Application::run() {
  assert(not applicationName.empty());

  if(_testableMode.isEnabled()) {
    if(!_testFacilityRunApplicationCalled) {
      throw ChimeraTK::logic_error(
          "Testable mode enabled but Application::run() called directly. Call TestFacility::runApplication() instead.");
    }
  }

  if(_runCalled) {
    throw ChimeraTK::logic_error("Application::run() has already been called before.");
  }
  _runCalled = true;

  // realise the PV connections
  _cm.connect();

  // set all initial version numbers in the modules to the same value
  for(auto& module : getSubmoduleListRecursive()) {
    if(module->getModuleType() != ModuleType::ApplicationModule) {
      continue;
    }
    module->setCurrentVersionNumber(getStartVersion());
  }

  // prepare the modules
  for(auto& module : getSubmoduleListRecursive()) {
    module->prepare();
  }

  // Switch life-cycle state to run
  _lifeCycleState = LifeCycleState::run;

  // start the necessary threads for the FanOuts etc.
  for(auto& internalModule : _internalModuleList) {
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

  if(Application::getInstance().getTestableMode().isEnabled()) {
    for(auto& internalModule : _internalModuleList) {
      waitForTestableMode(internalModule.get());
    }

    for(auto& module : getSubmoduleListRecursive()) {
      waitForTestableMode(module);
    }
  }

  // Launch circular dependency detector thread
  _circularDependencyDetector.startDetectBlockedModules();
}

/*********************************************************************************************************************/

void Application::shutdown() {
  // switch life-cycle state
  _lifeCycleState = LifeCycleState::shutdown;

  // first allow to run the application threads again, if we are in testable
  // mode
  if(_testableMode.isEnabled() && _testableMode.testLock()) {
    _testableMode.unlock("shutdown");
  }

  // deactivate the FanOuts first, since they have running threads inside
  // accessing the modules etc. (note: the modules are members of the
  // Application implementation and thus get destroyed after this destructor)
  for(auto& internalModule : _internalModuleList) {
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

  _circularDependencyDetector.terminate();

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

void Application::generateDOT() {
  assert(not applicationName.empty());
  this->getModel().writeGraphViz(applicationName + ".dot");
}

/*********************************************************************************************************************/

Application& Application::getInstance() {
  return dynamic_cast<Application&>(ApplicationBase::getInstance());
}

/*********************************************************************************************************************/

boost::shared_ptr<DeviceManager> Application::getDeviceManager(const std::string& aliasOrCDD) {
  if(_deviceManagerMap.find(aliasOrCDD) == _deviceManagerMap.end()) {
    // Add initialisation handler below, since we also need to add it if the DeviceModule already exists
    _deviceManagerMap[aliasOrCDD] = boost::make_shared<DeviceManager>(&Application::getInstance(), aliasOrCDD);
  }
  return _deviceManagerMap.at(aliasOrCDD);
}

/*********************************************************************************************************************/
