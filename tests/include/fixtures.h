// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "ControlSystemModule.h"
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
    ChimeraTK::ScalarPushInput<int> pushInput{this, "PUSH_READ", "", ""};
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
  void mainLoop() override { p.set_value(); }
};

/**********************************************************************************************************************/

struct DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  constexpr static const char* ExceptionDummyCDD2 = "(ExceptionDummy:2?map=test.map)";
  constexpr static const char* ExceptionDummyCDD3 = "(ExceptionDummy:3?map=test.map)";

  DummyApplication() : Application("DummyApplication") {}
  ~DummyApplication() override { shutdown(); }

  PushModule pushModule{this, "pushModule", "", ChimeraTK::TAGS{"DEV"}};
  PollModule pollModule{this, "pollModule", "", ChimeraTK::TAGS{"DEV"}};
  OutputModule outputModule{this, "outputModule", "", ChimeraTK::TAGS{"DEV"}};
  PushModule pushModule2{this, "pushModule2", "With TriggerFanOut", ChimeraTK::TAGS{"DEV2"}};
  PushModule pushModule3{this, "pushModule3", "With ThreadedFanOut", ChimeraTK::TAGS{"DEV2"}};
  PollModule pollModule2{this, "pollModule2", "", ChimeraTK::TAGS{"DEV2"}};
  OutputModule outputModule2{this, "outputModule2", "", ChimeraTK::TAGS{"DEV2"}};
  PollModule pollModule3{this, "pollModule3", "", ChimeraTK::TAGS{"DEV3"}};

  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  ChimeraTK::DeviceModule device2{this, ExceptionDummyCDD2};
  ChimeraTK::DeviceModule device3{this, ExceptionDummyCDD3};

  ChimeraTK::ControlSystemModule cs;

  void defineConnections() override {
    findTag("DEVICE").excludeTag("DEV2").excludeTag("DEV3").flatten().connectTo(device);
    findTag("DEVICE").excludeTag("DEV").excludeTag("DEV3").flatten().connectTo(device2);
    findTag("DEVICE").excludeTag("DEV").excludeTag("DEV2").flatten().connectTo(device3);

    /*    device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> pushModule.reg1.pushInput;
        device2("REG1")[device3("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push)] >>
            pushModule2.reg1.pushInput;
        device2("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> pushModule3.reg1.pushInput >>
            cs("dev2_reg1_push_read");
        findTag("DEVICE").excludeTag("DEV").connectTo(cs["Device2"]);*/
    // dumpConnections();
  }
};

/**********************************************************************************************************************/

template<bool enableTestFacility, bool addInitHandlers = false, bool breakSecondDeviceAtStart = false>
struct fixture_with_poll_and_push_input {
  fixture_with_poll_and_push_input()
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
    deviceBackend2->throwExceptionOpen = breakSecondDeviceAtStart;

    if constexpr(addInitHandlers) {
      auto initHandler1 = [this](ChimeraTK::DeviceManager* dm) {
        if(dm == &application.device.getDeviceManager()) {
          initHandler1Called = true;
          if(initHandler1Throws) {
            throw ChimeraTK::runtime_error("Init handler 1 throws by request");
          }
        }
      };
      auto initHandler2 = [this](ChimeraTK::DeviceManager* dm) {
        if(dm == &application.device.getDeviceManager()) {
          initHandler2Called = true;
          if(initHandler2Throws) {
            throw ChimeraTK::runtime_error("Init handler 2 throws by request");
          }
        }
      };
      application.device.addInitialisationHandler(initHandler1);
      application.device.addInitialisationHandler(initHandler2);
    }

    testFacitiy.runApplication();

    status.replace(testFacitiy.getScalar<int>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD1 / "status"));
    message.replace(testFacitiy.getScalar<std::string>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD1 / "status_message"));
    deviceBecameFunctional.replace(testFacitiy.getScalar<int>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD1 / "deviceBecameFunctional"));

    status2.replace(testFacitiy.getScalar<int>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD2 / "status"));

    pushVariable3copy.replace(testFacitiy.getScalar<int>("dev2_reg1_push_read"));

    //  wait until all modules have been properly started, to ensure the initial value propagation is complete
    application.pollModule.p.get_future().wait();
    application.pushModule.p.get_future().wait();
    application.outputModule.p.get_future().wait();
    if(!breakSecondDeviceAtStart) {
      application.outputModule2.p.get_future().wait();
      application.pollModule2.p.get_future().wait();
      application.pushModule2.p.get_future().wait();
    }
    deviceBecameFunctional.read();
  }

  ~fixture_with_poll_and_push_input() {
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

  template<typename T>
  auto read(ChimeraTK::DummyRegisterRawAccessor& accessor) {
    auto lock = accessor.getBufferLock();
    return static_cast<T>(accessor);
  }
  template<typename T>
  auto read(ChimeraTK::DummyRegisterRawAccessor&& accessor) {
    read<T>(accessor);
  }

  template<typename T>
  void write(ChimeraTK::DummyRegisterRawAccessor& accessor, T value) {
    auto lock = accessor.getBufferLock();
    accessor = static_cast<int32_t>(value);
  }
  template<typename T>
  void write(ChimeraTK::DummyRegisterRawAccessor&& accessor, T value) {
    write(accessor, value);
  }

  bool isDeviceInError() {
    // By definition, the DeviceModule has finished the recovery procedure when the status is 0 again.
    status.readLatest();
    return static_cast<int>(status);
  }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend2;
  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend3;
  DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{application, enableTestFacility};

  ChimeraTK::ScalarRegisterAccessor<int> status, status2;
  ChimeraTK::ScalarRegisterAccessor<int> deviceBecameFunctional;
  ChimeraTK::ScalarRegisterAccessor<std::string> message;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister2;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister3;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummy2Register;

  ChimeraTK::ScalarPushInput<int>& pushVariable{application.pushModule.reg1.pushInput};
  ChimeraTK::ScalarPollInput<int>& pollVariable{application.pollModule.pollInput};
  ChimeraTK::ScalarOutput<int>& outputVariable{application.outputModule.deviceRegister};
  ChimeraTK::ScalarOutput<int>& outputVariable2{application.outputModule.deviceRegister2};
  ChimeraTK::ScalarOutput<int>& outputVariable3{application.outputModule.deviceRegister3};

  ChimeraTK::ScalarPushInput<int>& triggeredInput{application.pushModule2.reg1.pushInput};
  ChimeraTK::ScalarPollInput<int>& pollVariable2{application.pollModule2.pollInput};

  ChimeraTK::ScalarPushInput<int>& pushVariable3{application.pushModule3.reg1.pushInput};
  ChimeraTK::ScalarRegisterAccessor<int> pushVariable3copy;
  ChimeraTK::ScalarPollInput<int>& pollVariable3{application.pollModule3.pollInput};

  std::atomic<bool> initHandler1Throws{false};
  std::atomic<bool> initHandler2Throws{false};
  std::atomic<bool> initHandler1Called{false};
  std::atomic<bool> initHandler2Called{false};
};

/**********************************************************************************************************************/
