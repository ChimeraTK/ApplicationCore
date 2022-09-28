// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "ApplicationModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/DummyRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/NDRegisterAccessor.h>

#include <future>

/**********************************************************************************************************************/
/**********************************************************************************************************************/

struct PollModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarPollInput<int> pollInput{this, "REG1", "", "", {"DEVICE"}};
  std::promise<void> p;
  void mainLoop() override { p.set_value(); }
};

/**********************************************************************************************************************/

struct PushModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "../REG1_PUSHED", "", ""};
    ChimeraTK::ScalarPushInput<int> pushInputCopy{this, "../REG1_PUSHED", "", ""};
  } reg1{this, "REG1", ""};

  std::promise<void> p;
  void mainLoop() override { p.set_value(); }
};

/**********************************************************************************************************************/

struct OutputModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<int> deviceRegister{this, "REG1", "", "", {"DEVICE"}};
  ChimeraTK::ScalarOutput<int> deviceRegister2{this, "REG2", "", "", {"DEVICE"}};
  ChimeraTK::ScalarOutput<int> deviceRegister3{this, "REG3", "", "", {"DEVICE"}};
  ChimeraTK::ScalarOutput<int> trigger{this, "trigger", "", ""}; // must not be connected to any device
  std::promise<void> p;
  void prepare() override { deviceRegister.write(); }
  void mainLoop() override { p.set_value(); }
};

/**********************************************************************************************************************/

struct DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test_with_push.map)";
  constexpr static const char* ExceptionDummyCDD2 = "(ExceptionDummy:2?map=test_with_push.map)";
  constexpr static const char* ExceptionDummyCDD3 = "(ExceptionDummy:3?map=test_with_push.map)";

  DummyApplication() : Application("DummyApplication") { // debugMakeConnections();
  }
  ~DummyApplication() override { shutdown(); }

  struct Group1 : ChimeraTK::ModuleGroup {
    using ChimeraTK::ModuleGroup::ModuleGroup;
    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
    PushModule pushModule{this, ".", ""};
    PollModule pollModule{this, ".", ""};
    OutputModule outputModule{this, ".", ""};
  } group1{this, "Group1", ""};

  struct Group2 : ChimeraTK::ModuleGroup {
    using ChimeraTK::ModuleGroup::ModuleGroup;
    ChimeraTK::DeviceModule device2{this, ExceptionDummyCDD2};
    PushModule pushModule2{this, ".", "With TriggerFanOut"};
    PushModule pushModule3{this, ".", "With ThreadedFanOut"};
    PollModule pollModule2{this, ".", ""};
    OutputModule outputModule2{this, ".", ""};
  } group2{this, "Group2", ""};

  struct Group3 : ChimeraTK::ModuleGroup {
    using ChimeraTK::ModuleGroup::ModuleGroup;
    ChimeraTK::DeviceModule device3{this, ExceptionDummyCDD3};
    PollModule pollModule3{this, ".", ""};
  } group3{this, "Group3", ""};

  /*
    ChimeraTK::ControlSystemModule cs;

    void defineConnections() override {
      findTag("DEVICE").excludeTag("DEV2").excludeTag("DEV3").flatten().connectTo(device);
      findTag("DEVICE").excludeTag("DEV").excludeTag("DEV3").flatten().connectTo(device2);
      findTag("DEVICE").excludeTag("DEV").excludeTag("DEV2").flatten().connectTo(device3);

         device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> pushModule.reg1.pushInput;
          device2("REG1")[device3("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push)] >>
              pushModule2.reg1.pushInput;
          device2("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> pushModule3.reg1.pushInput >>
              cs("dev2_reg1_push_read");
          findTag("DEVICE").excludeTag("DEV").connectTo(cs["Device2"]);
       dumpConnections();
    }*/
};

/**********************************************************************************************************************/
/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers = false, bool breakSecondDeviceAtStart = false>
struct fixture_with_poll_and_push_input {
  fixture_with_poll_and_push_input();

  ~fixture_with_poll_and_push_input();

  template<typename T>
  auto read(ChimeraTK::DummyRegisterRawAccessor& accessor);

  template<typename T>
  auto read(ChimeraTK::DummyRegisterRawAccessor&& accessor);

  template<typename T>
  void write(ChimeraTK::DummyRegisterRawAccessor& accessor, T value);

  template<typename T>
  void write(ChimeraTK::DummyRegisterRawAccessor&& accessor, T value);

  bool isDeviceInError();

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend2;
  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend3;
  DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{application, enableTestFacility};

  ChimeraTK::ScalarRegisterAccessor<int> status, status2;
  ChimeraTK::VoidRegisterAccessor deviceBecameFunctional;
  ChimeraTK::ScalarRegisterAccessor<std::string> message;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister2;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister3;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummy2Register;

  ChimeraTK::ScalarPushInput<int>& pushVariable{application.group1.pushModule.reg1.pushInput};
  ChimeraTK::ScalarPollInput<int>& pollVariable{application.group1.pollModule.pollInput};
  ChimeraTK::ScalarOutput<int>& outputVariable{application.group1.outputModule.deviceRegister};
  ChimeraTK::ScalarOutput<int>& outputVariable2{application.group1.outputModule.deviceRegister2};
  ChimeraTK::ScalarOutput<int>& outputVariable3{application.group1.outputModule.deviceRegister3};

  ChimeraTK::ScalarPushInput<int>& triggeredInput{application.group2.pushModule2.reg1.pushInput};
  ChimeraTK::ScalarPollInput<int>& pollVariable2{application.group2.pollModule2.pollInput};

  ChimeraTK::ScalarPushInput<int>& pushVariable3{application.group2.pushModule3.reg1.pushInput};
  ChimeraTK::ScalarPushInput<int>& pushVariable3copy{application.group2.pushModule3.reg1.pushInputCopy};
  ChimeraTK::ScalarPollInput<int>& pollVariable3{application.group3.pollModule3.pollInput};

  ChimeraTK::VoidRegisterAccessor interrupt;

  std::atomic<bool> initHandler1Throws{false};
  std::atomic<bool> initHandler2Throws{false};
  std::atomic<bool> initHandler1Called{false};
  std::atomic<bool> initHandler2Called{false};
};

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers, bool breakSecondDeviceAtStart>
fixture_with_poll_and_push_input<enableTestFacility, addInitHandlers,
    breakSecondDeviceAtStart>::fixture_with_poll_and_push_input()
: deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(DummyApplication::ExceptionDummyCDD1))),
  deviceBackend2(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(DummyApplication::ExceptionDummyCDD2))),
  deviceBackend3(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(DummyApplication::ExceptionDummyCDD3))),
  exceptionDummyRegister(deviceBackend->getRawAccessor("", "REG1")),
  exceptionDummyRegister2(deviceBackend->getRawAccessor("", "REG2")),
  exceptionDummyRegister3(deviceBackend->getRawAccessor("", "REG3")),
  exceptionDummy2Register(deviceBackend2->getRawAccessor("", "REG1")) {
  try {
    deviceBackend2->throwExceptionOpen = breakSecondDeviceAtStart;

    if constexpr(addInitHandlers) {
      auto initHandler1 = [this](ChimeraTK::DeviceManager* dm) {
        if(dm == &application.group1.device.getDeviceManager()) {
          initHandler1Called = true;
          if(initHandler1Throws) {
            throw ChimeraTK::runtime_error("Init handler 1 throws by request");
          }
        }
      };
      auto initHandler2 = [this](ChimeraTK::DeviceManager* dm) {
        if(dm == &application.group1.device.getDeviceManager()) {
          initHandler2Called = true;
          if(initHandler2Throws) {
            throw ChimeraTK::runtime_error("Init handler 2 throws by request");
          }
        }
      };
      application.group1.device.addInitialisationHandler(initHandler1);
      application.group1.device.addInitialisationHandler(initHandler2);
    }

    testFacitiy.runApplication();

    auto dm1 = ChimeraTK::Utilities::stripName(DummyApplication::ExceptionDummyCDD1, false);
    auto dm2 = ChimeraTK::Utilities::stripName(DummyApplication::ExceptionDummyCDD2, false);
    status.replace(testFacitiy.getScalar<int>(ChimeraTK::RegisterPath("/Devices") / dm1 / "status"));
    message.replace(testFacitiy.getScalar<std::string>(ChimeraTK::RegisterPath("/Devices") / dm1 / "status_message"));
    deviceBecameFunctional.replace(
        testFacitiy.getVoid(ChimeraTK::RegisterPath("/Devices") / dm1 / "deviceBecameFunctional"));

    status2.replace(testFacitiy.getScalar<int>(ChimeraTK::RegisterPath("/Devices") / dm2 / "status"));

    ChimeraTK::Device dev(DummyApplication::ExceptionDummyCDD1);
    interrupt.replace(dev.getVoidRegisterAccessor("DUMMY_INTERRUPT_1_0"));

    // wait until all modules have been properly started, to ensure the initial value propagation is complete
    application.group1.pollModule.p.get_future().wait();
    application.group1.pushModule.p.get_future().wait();
    application.group1.outputModule.p.get_future().wait();
    if(!breakSecondDeviceAtStart) {
      application.group2.outputModule2.p.get_future().wait();
      application.group2.pollModule2.p.get_future().wait();
      application.group2.pushModule2.p.get_future().wait();
    }
    deviceBecameFunctional.read();
  }
  catch(std::exception& e) {
    std::cout << "Exception caught in constructor: " << e.what() << std::endl;
    throw;
  }
}

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers, bool breakSecondDeviceAtStart>
fixture_with_poll_and_push_input<enableTestFacility, addInitHandlers,
    breakSecondDeviceAtStart>::~fixture_with_poll_and_push_input() {
  // make sure no exception throwing is still enabled from previous test
  deviceBackend->throwExceptionOpen = false;
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionWrite = false;
  deviceBackend2->throwExceptionOpen = false;
  deviceBackend2->throwExceptionRead = false;
  deviceBackend2->throwExceptionWrite = false;
  deviceBackend3->throwExceptionOpen = false;
  deviceBackend3->throwExceptionRead = false;
  deviceBackend3->throwExceptionWrite = false;
}

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers, bool breakSecondDeviceAtStart>
bool fixture_with_poll_and_push_input<enableTestFacility, addInitHandlers,
    breakSecondDeviceAtStart>::isDeviceInError() {
  // By definition, the DeviceModule has finished the recovery procedure when the status is 0 again.
  status.readLatest();
  return static_cast<int>(status);
}

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers, bool breakSecondDeviceAtStart>
template<typename T>
auto fixture_with_poll_and_push_input<enableTestFacility, addInitHandlers, breakSecondDeviceAtStart>::read(
    ChimeraTK::DummyRegisterRawAccessor& accessor) {
  auto lock = accessor.getBufferLock();
  return static_cast<T>(accessor);
}

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers, bool breakSecondDeviceAtStart>
template<typename T>
auto fixture_with_poll_and_push_input<enableTestFacility, addInitHandlers, breakSecondDeviceAtStart>::read(
    ChimeraTK::DummyRegisterRawAccessor&& accessor) {
  read<T>(accessor);
}

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers, bool breakSecondDeviceAtStart>
template<typename T>
void fixture_with_poll_and_push_input<enableTestFacility, addInitHandlers, breakSecondDeviceAtStart>::write(
    ChimeraTK::DummyRegisterRawAccessor& accessor, T value) {
  auto lock = accessor.getBufferLock();
  accessor = static_cast<int32_t>(value);
}

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers, bool breakSecondDeviceAtStart>
template<typename T>
void fixture_with_poll_and_push_input<enableTestFacility, addInitHandlers, breakSecondDeviceAtStart>::write(
    ChimeraTK::DummyRegisterRawAccessor&& accessor, T value) {
  write(accessor, value);
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/
