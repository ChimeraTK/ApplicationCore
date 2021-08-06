#pragma once

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/DummyRegisterAccessor.h>

#include "Application.h"
#include "ApplicationModule.h"
#include "ControlSystemModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "check_timeout.h"

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
  std::promise<void> p;
  void mainLoop() override { p.set_value(); }
};

/**********************************************************************************************************************/

struct DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  constexpr static const char* ExceptionDummyCDD2 = "(ExceptionDummy:2?map=test.map)";
  DummyApplication() : Application("DummyApplication") {}
  ~DummyApplication() { shutdown(); }

  PushModule pushModule{this, "", "", ChimeraTK::HierarchyModifier::none, {"DEV"}};
  PollModule pollModule{this, "", "", ChimeraTK::HierarchyModifier::none, {"DEV"}};
  OutputModule outputModule{this, "", "", ChimeraTK::HierarchyModifier::none, {"DEV"}};
  PushModule pushModule2{this, "", "", ChimeraTK::HierarchyModifier::none, {"DEV2"}};
  PollModule pollModule2{this, "", "", ChimeraTK::HierarchyModifier::none, {"DEV2"}};
  OutputModule outputModule2{this, "", "", ChimeraTK::HierarchyModifier::none, {"DEV2"}};

  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  ChimeraTK::DeviceModule device2{this, ExceptionDummyCDD2};

  void defineConnections() override {
    findTag("DEVICE").excludeTag("DEV2").connectTo(device);
    findTag("DEVICE").excludeTag("DEV").connectTo(device2);

    device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> pushModule.reg1.pushInput;
    device2("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> pushModule2.reg1.pushInput;
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
    exceptionDummyRegister(deviceBackend->getRawAccessor("", "REG1")),
    exceptionDummyRegister2(deviceBackend->getRawAccessor("", "REG2")),
    exceptionDummyRegister3(deviceBackend->getRawAccessor("", "REG3")) {
    if constexpr(addInitHandlers) {
      auto initHandler1 = [this](ChimeraTK::DeviceModule* dm) {
        if(dm == &application.device) {
          initHandler1Called = true;
        }
      };
      auto initHandler2 = [this](ChimeraTK::DeviceModule* dm) {
        if(dm == &application.device) {
          initHandler2Called = true;
        }
      };
      application.device.addInitialisationHandler(initHandler1);
      application.device.addInitialisationHandler(initHandler2);
    }

    deviceBackend2->throwExceptionOpen = breakSecondDeviceAtStart;

    testFacitiy.runApplication();

    status.replace(testFacitiy.getScalar<int>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD1 / "status"));
    message.replace(testFacitiy.getScalar<std::string>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD1 / "message"));
    deviceBecameFunctional.replace(testFacitiy.getScalar<int>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD1 / "deviceBecameFunctional"));

    status2.replace(testFacitiy.getScalar<int>(
        ChimeraTK::RegisterPath("/Devices") / DummyApplication::ExceptionDummyCDD2 / "status"));

    //  wait until all modules have been properly started, to ensure the initial value propagation is complete
    application.pollModule.p.get_future().wait();
    application.pushModule.p.get_future().wait();
    application.outputModule.p.get_future().wait();
    deviceBecameFunctional.read();
  }

  ~fixture_with_poll_and_push_input() {
    deviceBackend->throwExceptionRead = false;
    deviceBackend->throwExceptionWrite = false;
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
  DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{enableTestFacility};

  ChimeraTK::ScalarRegisterAccessor<int> status, status2;
  ChimeraTK::ScalarRegisterAccessor<int> deviceBecameFunctional;
  ChimeraTK::ScalarRegisterAccessor<std::string> message;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister2;
  ChimeraTK::DummyRegisterRawAccessor exceptionDummyRegister3;

  ChimeraTK::ScalarPushInput<int>& pushVariable{application.pushModule.reg1.pushInput};
  ChimeraTK::ScalarPollInput<int>& pollVariable{application.pollModule.pollInput};
  ChimeraTK::ScalarOutput<int>& outputVariable{application.outputModule.deviceRegister};
  ChimeraTK::ScalarOutput<int>& outputVariable2{application.outputModule.deviceRegister2};
  ChimeraTK::ScalarOutput<int>& outputVariable3{application.outputModule.deviceRegister3};

  std::atomic<bool> initHandler1Called{false};
  std::atomic<bool> initHandler2Called{false};
};

/**********************************************************************************************************************/
