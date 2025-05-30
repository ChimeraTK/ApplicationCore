// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testInitialValues

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/DummyRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

#include <VoidAccessor.h>

#include <chrono>
#include <functional>
#include <future>

namespace Tests::testInitialValues {

  /********************************************************************************************************************/

  // Base Application module that provides flags for the various phases
  // of module lifetime and full-fills a promise when the main loop has been reached
  struct NotifyingModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;

    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};
    std::atomic_bool enteredThePrepareLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
    void prepare() override { enteredThePrepareLoop = true; }
  };

  /********************************************************************************************************************/

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;
  // A generic module with just one input. It is connected manually, so we just call the register "REG1" so we easily
  // connect to that register in the device It has a flag and a promise to check whether the module has entered the main
  // loop, and to wait for it.
  template<class INPUT_TYPE>
  struct InputModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;
    INPUT_TYPE input{this, "/REG1", "", ""};
    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
  };

  struct PollDummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test-ro.map)";
    PollDummyApplication() : Application("DummyApplication") {}
    ~PollDummyApplication() override { shutdown(); }

    InputModule<ctk::ScalarPollInput<int>> inputModule{this, "PollModule", ""};
    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  };

  // for the push type we need different connection code
  struct PushDummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:2?map=test-async.map)";
    PushDummyApplication() : Application("DummyApplication") {}
    ~PushDummyApplication() override { shutdown(); }

    InputModule<ctk::ScalarPushInput<int>> inputModule{this, "PushModule", ""};
    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  };

  template<class APPLICATION_TYPE>
  struct TestFixtureWithEceptionDummy {
    TestFixtureWithEceptionDummy()
    : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
          ChimeraTK::BackendFactory::getInstance().createBackend(APPLICATION_TYPE::ExceptionDummyCDD1))) {}
    ~TestFixtureWithEceptionDummy() { deviceBackend->throwExceptionRead = false; }

    boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
    APPLICATION_TYPE application;
    ChimeraTK::TestFacility testFacility{application, false};
    ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  };
  /**
   *  Test Initial Values - Inputs of `ApplicationModule`s
   *  InitialValuesInputsOfApplicationCore_D_8 "D.8"
   */
  BOOST_AUTO_TEST_SUITE(testInitialValuesInputsOfApplicationCore_D_8)
  using DeviceTestApplicationTypes = boost::mpl::list<PushDummyApplication, PollDummyApplication>;

  /**
   *  For device variables the ExceptionHandlingDecorator freezes the variable until the device is available
   * \anchor testInitialValue_D_8_b_i \ref initialValue_D_8_b_i
   */
  BOOST_AUTO_TEST_CASE_TEMPLATE(testInitValueAtDevice8bi, APPLICATION_TYPE, DeviceTestApplicationTypes) {
    std::cout << "===   testInitValueAtDevice8bi " << typeid(APPLICATION_TYPE).name() << "  ===" << std::endl;
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    { // Here the time is stopped until you reach the mainloop.
      TestFixtureWithEceptionDummy<APPLICATION_TYPE> dummyToStopTimeUntilOpen;
      start = std::chrono::steady_clock::now();
      dummyToStopTimeUntilOpen.application.run();
      dummyToStopTimeUntilOpen.application.inputModule.p.get_future().wait();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      end = std::chrono::steady_clock::now();
    }
    { // waiting 2 x the time stopped above, in the assumption that it is then frozen,
      // as it is described in the spec.
      TestFixtureWithEceptionDummy<APPLICATION_TYPE> d;
      d.deviceBackend->throwExceptionOpen = true;
      BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);
      d.deviceBackend->throwExceptionOpen = true;
      d.application.run();
      auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
      BOOST_CHECK(d.application.inputModule.enteredTheMainLoop == false);
      std::this_thread::sleep_for(std::chrono::milliseconds(2 * elapsed_milliseconds));
      BOOST_CHECK(d.application.inputModule.enteredTheMainLoop == false);
      BOOST_CHECK(d.application.inputModule.input.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
      d.deviceBackend->throwExceptionOpen = false;
      d.application.inputModule.p.get_future().wait();
      BOOST_CHECK(d.application.inputModule.enteredTheMainLoop == true);
      BOOST_CHECK(d.application.inputModule.input.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct ScalarOutputModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;
    ChimeraTK::ScalarOutput<int> output{this, "REG1", "", ""};
    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
  };

  template<class INPUT_TYPE>
  struct ProcessArryDummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
    ProcessArryDummyApplication() : Application("DummyApplication") {}
    ~ProcessArryDummyApplication() override { shutdown(); }

    InputModule<INPUT_TYPE> inputModule{this, ".", ""};
    ScalarOutputModule scalarOutputModule{this, ".", ""};
  };

  using TestInputTypes = boost::mpl::list<ctk::ScalarPollInput<int>, ctk::ScalarPushInput<int>>;

  /**
   *  ProcessArray freeze in their implementation until the initial value is received
   * \anchor testInitialValue_D_8_b_ii \ref initialValue_D_8_b_ii
   */
  BOOST_AUTO_TEST_CASE_TEMPLATE(testProcessArrayInitValueAtDevice8bii, INPUT_TYPE, TestInputTypes) {
    std::cout << "===   testPollProcessArrayInitValueAtDevice8bii " << typeid(INPUT_TYPE).name()
              << "  === " << std::endl;
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    {
      // we don't need the exception dummy in this test. But no need to write a new fixture for it.
      TestFixtureWithEceptionDummy<ProcessArryDummyApplication<INPUT_TYPE>> dummyToStopTimeForApplicationStart;
      start = std::chrono::steady_clock::now();
      dummyToStopTimeForApplicationStart.application.run();
      dummyToStopTimeForApplicationStart.application.scalarOutputModule.output.write();
      dummyToStopTimeForApplicationStart.application.inputModule.p.get_future().wait();
      end = std::chrono::steady_clock::now();
    }
    {
      TestFixtureWithEceptionDummy<ProcessArryDummyApplication<INPUT_TYPE>> d;
      d.application.run();
      BOOST_CHECK(d.application.inputModule.enteredTheMainLoop == false);
      std::this_thread::sleep_for(end - start);
      BOOST_CHECK(d.application.inputModule.enteredTheMainLoop == false);
      BOOST_CHECK(d.application.inputModule.input.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
      d.application.scalarOutputModule.output.write();
      d.application.inputModule.p.get_future().wait();
      BOOST_CHECK(d.application.inputModule.enteredTheMainLoop == true);
      BOOST_CHECK(d.application.inputModule.input.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  template<class INPUT_TYPE>
  struct ConstantTestApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
    ConstantTestApplication() : Application("DummyApplication") {}
    ~ConstantTestApplication() override { shutdown(); }

    InputModule<INPUT_TYPE> inputModule{this, "constantPollModule", ""};
  };

  /**
   * Constants can be read exactly once in case of `AccessMode::wait_for_new_data`, so the initial value can be
   * received. \anchor testInitialValue_D_8_b_iii \ref initialValue_D_8_b_iii
   *
   * Note: "Constants" here refer to the ConstantAccessor, which is nowadays only used for unconnected inputs when the
   * control system connection has been optimised out (cf. Application::optimiseUnmappedVariables()).
   *
   */
  BOOST_AUTO_TEST_CASE_TEMPLATE(testConstantInitValueAtDevice8biii, INPUT_TYPE, TestInputTypes) {
    std::cout << "===   testConstantInitValueAtDevice8biii " << typeid(INPUT_TYPE).name() << "  === " << std::endl;
    TestFixtureWithEceptionDummy<ConstantTestApplication<INPUT_TYPE>> d;

    // make sure, inputModule.input is not connected to anything, not even the control system.
    d.application.optimiseUnmappedVariables({"/REG1"});

    d.application.run();
    d.application.inputModule.p.get_future().wait();

    BOOST_CHECK(d.application.inputModule.input.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
    if(d.application.inputModule.input.getAccessModeFlags().has(ctk::AccessMode::wait_for_new_data)) {
      BOOST_CHECK(d.application.inputModule.input.readNonBlocking() ==
          false); // no new data. Calling read() would block infinitely
    }
    else {
      BOOST_CHECK(d.application.inputModule.input.readNonBlocking() == true);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct PushModuleD91 : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "/REG1", "", ""};
    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
  };
  struct PushModuleD92 : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "/REG2", "", ""};
    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
  };

  struct PushD9DummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test-async.map)";
    PushD9DummyApplication() : Application("DummyApplication") {}
    ~PushD9DummyApplication() override { shutdown(); }

    PushModuleD91 pushModuleD91{this, "PushModule1", ""};
    PushModuleD92 pushModuleD92{this, "PushModule2", ""};

    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  };

  struct D9InitialValueEceptionDummy {
    D9InitialValueEceptionDummy()
    : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
          ChimeraTK::BackendFactory::getInstance().createBackend(PushD9DummyApplication::ExceptionDummyCDD1))) {}
    ~D9InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

    boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
    PushD9DummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
    ChimeraTK::ScalarPushInput<int>& pushVariable1{application.pushModuleD91.pushInput};
    ChimeraTK::ScalarPushInput<int>& pushVariable2{application.pushModuleD92.pushInput};
  };

  /**
   *  D 9 b for ThreadedFanOut
   * \anchor testInitialValueThreadedFanOut_D_9_b_ThreadedFanOut \ref initialValueThreadedFanOut_D_9_b
   */
  BOOST_AUTO_TEST_CASE(testPushInitValueAtDeviceD9) {
    std::cout << "===   testPushInitValueAtDeviceD9   === " << std::endl;
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    {
      D9InitialValueEceptionDummy dummyToStopTimeUntilOpen;
      start = std::chrono::steady_clock::now();
      dummyToStopTimeUntilOpen.application.run();
      dummyToStopTimeUntilOpen.application.pushModuleD91.p.get_future().wait();
      dummyToStopTimeUntilOpen.application.pushModuleD92.p.get_future().wait();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      end = std::chrono::steady_clock::now();
    }
    {
      D9InitialValueEceptionDummy d;
      d.deviceBackend->throwExceptionOpen = true;
      BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);
      d.application.run();
      BOOST_CHECK(d.application.pushModuleD91.enteredTheMainLoop == false);
      std::this_thread::sleep_for(2 * (end - start));
      BOOST_CHECK(d.application.pushModuleD91.enteredTheMainLoop == false);
      BOOST_CHECK(d.pushVariable1.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
      d.deviceBackend->throwExceptionOpen = false;
      d.application.pushModuleD91.p.get_future().wait();
      BOOST_CHECK(d.application.pushModuleD91.enteredTheMainLoop == true);
      BOOST_CHECK(d.pushVariable1.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct TriggerModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;
    ChimeraTK::VoidOutput trigger{this, "/TRIG1/PUSH_OUT", ""};
    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
  };

  struct TriggerFanOutD9DummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test-trigger-fanout-iv.map)";
    TriggerFanOutD9DummyApplication() : Application("DummyApplication") {}
    ~TriggerFanOutD9DummyApplication() override { shutdown(); }

    PushModuleD91 pushModuleD91{this, "PushModule1", ""};
    PushModuleD92 pushModuleD92{this, "PushModule2", ""};
    TriggerModule triggerModule{this, "TriggerModule", ""};

    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1, "/TRIG1/PUSH_OUT"};
  };

  struct TriggerFanOutInitialValueEceptionDummy {
    TriggerFanOutInitialValueEceptionDummy()
    : deviceBackend(
          boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(ChimeraTK::BackendFactory::getInstance().createBackend(
              TriggerFanOutD9DummyApplication::ExceptionDummyCDD1))) {}
    ~TriggerFanOutInitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

    boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
    TriggerFanOutD9DummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
    ChimeraTK::ScalarPushInput<int>& pushVariable1{application.pushModuleD91.pushInput};
    ChimeraTK::ScalarPushInput<int>& pushVariable2{application.pushModuleD92.pushInput};
  };

  /**
   *  D 9 b for TriggerFanOut
   * \anchor testInitialValueThreadedFanOut_D_9_b_TriggerFanOut \ref initialValueThreadedFanOut_D_9_b
   */
  BOOST_AUTO_TEST_CASE(testTriggerFanOutInitValueAtDeviceD9) {
    std::cout << "===   testTriggerFanOutInitValueAtDeviceD9   === " << std::endl;
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    {
      TriggerFanOutInitialValueEceptionDummy dummyToStopTimeUntilOpen;
      start = std::chrono::steady_clock::now();
      dummyToStopTimeUntilOpen.application.run();
      dummyToStopTimeUntilOpen.application.triggerModule.trigger.write();
      dummyToStopTimeUntilOpen.application.pushModuleD91.p.get_future().wait();
      dummyToStopTimeUntilOpen.application.pushModuleD92.p.get_future().wait();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      end = std::chrono::steady_clock::now();
    }
    {
      TriggerFanOutInitialValueEceptionDummy d;
      d.deviceBackend->throwExceptionOpen = true;
      BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);
      d.application.run();
      BOOST_CHECK(d.application.pushModuleD91.enteredTheMainLoop == false);
      std::this_thread::sleep_for(2 * (end - start));
      BOOST_CHECK(d.application.pushModuleD91.enteredTheMainLoop == false);
      BOOST_CHECK(d.pushVariable1.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
      d.deviceBackend->throwExceptionOpen = false;
      d.application.triggerModule.trigger.write();
      d.application.pushModuleD91.p.get_future().wait();
      BOOST_CHECK(d.application.pushModuleD91.enteredTheMainLoop == true);
      BOOST_CHECK(d.pushVariable1.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct ConstantModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;

    struct : ChimeraTK::VariableGroup {
      using ChimeraTK::VariableGroup::VariableGroup;
      ChimeraTK::ScalarPushInput<int> constant{this, "/REG1", "", ""};
    } reg1{this, ".", ""};

    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};

    void prepare() override {
      reg1.constant = 543; // some non-zero value to detect if the 0 constant is written later
    }

    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
  };

  struct ConstantD10DummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
    ConstantD10DummyApplication() : Application("DummyApplication") {}
    ~ConstantD10DummyApplication() override { shutdown(); }

    ConstantModule constantModule{this, "ConstantModule", ""};

    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  };

  struct ConstantD10InitialValueEceptionDummy {
    ConstantD10InitialValueEceptionDummy()
    : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
          ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:1?map=test.map)"))) {}
    ~ConstantD10InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

    boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
    ConstantD10DummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
  };

  /**
   *  D 10 for Constant
   * \anchor testConstantD10InitialValue_D_10 \ref initialValue_d_10
   */
  BOOST_AUTO_TEST_CASE(testConstantD10InitialValue) {
    std::cout << "===   testConstantD10InitialValue   === " << std::endl;
    ConstantD10InitialValueEceptionDummy d;
    d.application.optimiseUnmappedVariables({"/REG1"});

    ChimeraTK::Device dev("(ExceptionDummy:1?map=test.map)");
    dev.open();
    dev.write<int>("REG1", 1234); // place some value, we expect it to be overwritten with 0

    d.deviceBackend->throwExceptionOpen = true;
    BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);

    d.application.run();
    d.application.constantModule.p.get_future().wait();

    BOOST_CHECK(d.application.constantModule.enteredTheMainLoop == true);
    BOOST_CHECK(d.application.constantModule.reg1.constant == 0); // no longer at the value set in prepare()
    BOOST_CHECK(d.application.constantModule.reg1.constant.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));

    ChimeraTK::DummyRegisterAccessor<int> reg1(
        boost::dynamic_pointer_cast<ChimeraTK::DummyBackend>(dev.getBackend()).get(), "", "REG1");
    {
      auto lk = reg1.getBufferLock();
      BOOST_CHECK(reg1 == 1234);
    }
    d.deviceBackend->throwExceptionOpen = false;
    CHECK_TIMEOUT((reg1.getBufferLock(), reg1 == 0), 1000000);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct TestModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "/REG1", "", ""};
    ChimeraTK::ScalarOutput<int> output{this, "SomeOutput", "", ""};
    std::promise<void> p;
    std::atomic_bool enteredTheMainLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      p.set_value();
    }
  };

  struct TestDummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test-async.map)";
    TestDummyApplication() : Application("DummyApplication") {}
    ~TestDummyApplication() override { shutdown(); }

    TestModule testModule{this, "TestModule", ""};
    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  };

  struct TestInitialValueExceptionDummy {
    explicit TestInitialValueExceptionDummy()
    : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
          ChimeraTK::BackendFactory::getInstance().createBackend(TestDummyApplication::ExceptionDummyCDD1))) {}
    ~TestInitialValueExceptionDummy() { deviceBackend->throwExceptionRead = false; }

    boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
    TestDummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    ChimeraTK::ScalarPushInput<int>& pushVariable{application.testModule.pushInput};
    ChimeraTK::ScalarOutput<int>& outputVariable{application.testModule.output};
  };

  /**
   *  D 1 for DataValidity::faulty
   * \anchor testD1InitialValue_D_1 \ref testD1InitialValue_D_1
   */
  // Todo add missing tests for bi-directional variables
  BOOST_AUTO_TEST_CASE(testD1InitialValue) {
    std::cout << "===   testD1InitialValue   === " << std::endl;

    TestInitialValueExceptionDummy d;

    d.application.run();
    d.application.testModule.p.get_future().wait();
    BOOST_CHECK(d.application.testModule.enteredTheMainLoop == true);
    BOOST_CHECK(d.pushVariable.dataValidity() == ctk::DataValidity::ok);
    d.application.testModule.output.write();
    BOOST_CHECK(d.outputVariable.dataValidity() == ctk::DataValidity::ok);
  }

  /**
   *  D 2 for DataValidity::faulty
   * \anchor testD1InitialValue_D_2 \ref testD1InitialValue_D_2
   */
  BOOST_AUTO_TEST_CASE(testD2InitialValue) {
    std::cout << "===   testD2InitialValue   === " << std::endl;

    TestInitialValueExceptionDummy d;
    d.application.run();
    d.application.testModule.p.get_future().wait();
    d.application.testModule.output.write();
    BOOST_CHECK(d.application.testModule.enteredTheMainLoop == true);
    BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
    BOOST_CHECK(d.outputVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }

  /**
   *  D 3 for DataValidity::faulty
   * \anchor testD1InitialValue_D_3 \ref testD1InitialValue_D_3
   */
  BOOST_AUTO_TEST_CASE(testD3InitialValue) {
    std::cout << "===   testD3InitialValue   === " << std::endl;

    TestInitialValueExceptionDummy d;
    d.application.run();
    d.application.testModule.p.get_future().wait();
    BOOST_CHECK(d.application.testModule.enteredTheMainLoop == true);
    BOOST_CHECK(d.pushVariable.dataValidity() == ctk::DataValidity::ok);
    BOOST_CHECK(d.outputVariable.dataValidity() == ctk::DataValidity::ok);
    // Todo. The initial value can also be faulty. Change backend so that it allows to override the data validity
    // without going to an exception state.
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct WriterModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;
    ChimeraTK::ScalarOutput<int> output1{this, "/REG1", "", ""};
    ChimeraTK::ScalarOutput<int> output2{this, "/REG2", "", ""};
    std::atomic_bool enteredTheMainLoop{false};
    std::atomic_bool enteredThePrepareLoop{false};
    void mainLoop() override {
      enteredTheMainLoop = true;
      output2 = 555;
      output2.write();
    }
    void prepare() override {
      enteredThePrepareLoop = true;
      output1 = 777;
      output1.write();
    }
  };

  struct ReaderModule : NotifyingModule {
    using NotifyingModule::NotifyingModule;

    ChimeraTK::ScalarPushInput<int> reg1{this, "/REG1", "", ""};
    ChimeraTK::ScalarPushInput<int> reg2{this, "/REG2", "", ""};
  };

  struct Test7DummyApplication : ChimeraTK::Application {
    Test7DummyApplication() : Application("DummyApplication") {}
    ~Test7DummyApplication() override { shutdown(); }

    WriterModule writerModule{this, "WriterModule", ""};
    ReaderModule readerModule{this, "ReaderModule", ""};
  };

  /**
   *  D 7_1
   * \anchor testD7_1_InitialValue \ref testD7_1_InitialValue
   */

  BOOST_AUTO_TEST_CASE(testD7_1_InitialValue) {
    std::cout << "===   testD7_1_InitialValue   === " << std::endl;

    Test7DummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    application.run();
    BOOST_CHECK(application.writerModule.enteredThePrepareLoop == true);
    application.readerModule.p.get_future().wait();
    CHECK_TIMEOUT(application.readerModule.reg1 == 777, 500);
  }

  /**
   *  D 7_2
   * \anchor testD7_2_InitialValue \ref testD7_2_InitialValue
   */
  BOOST_AUTO_TEST_CASE(testD7_2_InitialValue) {
    std::cout << "===   testD7_2_InitialValue   === " << std::endl;

    Test7DummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    application.run();
    application.readerModule.p.get_future().wait();
    BOOST_CHECK(application.readerModule.enteredTheMainLoop == true);
    CHECK_TIMEOUT(application.readerModule.reg2 == 555, 500);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct Test6A1DummyApplication : ChimeraTK::Application {
    // This application connects the CS to the device and and the input of the readerModule
    constexpr static const char* CDD = "(dummy:1?map=test.map)";
    Test6A1DummyApplication() : Application("DummyApplication") {}
    ~Test6A1DummyApplication() override { shutdown(); }

    struct : NotifyingModule {
      using NotifyingModule::NotifyingModule;

      ChimeraTK::ScalarPushInput<int> reg1{this, "/REG1", "", ""};
    } readerModule{this, ".", ""};

    ChimeraTK::DeviceModule device{this, CDD};
  };

  struct Test6A1InitialValueEceptionDummy {
    Test6A1InitialValueEceptionDummy() = default;
    ~Test6A1InitialValueEceptionDummy() = default;

    Test6A1DummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg1};
  };

  /**
   *  D 6_a1 initial value from control system variable
   * \anchor testD6_a1_InitialValue \ref testD6_a1_InitialValue
   */

  BOOST_AUTO_TEST_CASE(testD6_a1_InitialValue) {
    std::cout << "===   testD6_a1_InitialValue   === " << std::endl;
    Test6A1InitialValueEceptionDummy d;
    d.application.run();
    BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
    d.testFacitiy.writeScalar<int>("REG1", 27);
    ChimeraTK::Device dev;
    dev.open(Test6A1DummyApplication::CDD);
    CHECK_TIMEOUT(dev.read<int>("REG1") == 27, 1000000);
    // wait until the main loop has been entered. then we know the version number of the inputs must not be 0.
    // FIXME: I think this does not belong into this test....
    d.application.readerModule.p.get_future().wait(); // synchronisation point for the thread sanitizer
    BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  struct Test6A2DummyApplication : ChimeraTK::Application {
    constexpr static const char* CDD1 = "(dummy:1?map=one-register.map)";
    constexpr static const char* CDD2 = "(dummy:2?map=test-ro.map)";
    Test6A2DummyApplication() : Application("DummyApplication") {}
    ~Test6A2DummyApplication() override { shutdown(); }

    struct : NotifyingModule {
      using NotifyingModule::NotifyingModule;
      ChimeraTK::ScalarPushInput<int> reg1{this, "/REG1", "", ""};
    } readerModule{this, "ReaderModule", ""};

    TriggerModule triggerModule{this, "ReaderModule", ""};
    ChimeraTK::DeviceModule device{this, CDD1};
    ChimeraTK::DeviceModule device2{this, CDD2, "/TRIG1/PUSH_OUT"};
  };

  struct Test6A2InitialValueEceptionDummy {
    Test6A2InitialValueEceptionDummy() = default;
    ~Test6A2InitialValueEceptionDummy() = default;

    Test6A2DummyApplication application;
    ChimeraTK::TestFacility testFacility{application, false};
    ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg1};
  };

  /**
   *  D 6_a2 initial value from device in poll mode
   * \anchor testD6_a2_InitialValue \ref testD6_a2_InitialValue
   *
   *  The push type variable dev2/REG1 is "directly" connected to dev1/REG2 through a trigger.
   *  Test that it is written as soon as the initial value is available, i.e. there has been a trigger.
   */

  BOOST_AUTO_TEST_CASE(testD6_a2_InitialValue) {
    std::cout << "===   testD6_a2_InitialValue   === " << std::endl;

    Test6A2InitialValueEceptionDummy d;

    ChimeraTK::Device dev2;
    dev2.open(Test6A2DummyApplication::CDD2);
    dev2.write<int>("REG1/DUMMY_WRITEABLE", 99); // value now in in dev2

    d.application.run();
    // no trigger yet and the value is not on dev1 yet
    BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
    ChimeraTK::Device dev;
    dev.open(Test6A2DummyApplication::CDD1);
    BOOST_CHECK(dev.read<int>("REG1") != 99);

    // send the trigger and check that the data arrives on the device
    d.application.triggerModule.trigger.write();

    CHECK_TIMEOUT(dev.read<int>("REG1") == 99, 1000000);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct Test6A3DummyApplication : ChimeraTK::Application {
    constexpr static const char* CDD1 = "(dummy:1?map=one-register.map)";
    constexpr static const char* CDD2 = "(dummy:2?map=test-async.map)";
    Test6A3DummyApplication() : Application("DummyApplication") {}
    ~Test6A3DummyApplication() override { shutdown(); }

    struct : NotifyingModule {
      using NotifyingModule::NotifyingModule;
      ChimeraTK::ScalarPushInput<int> reg1{this, "/REG1", "", ""};
    } readerModule{this, "ReaderModule", ""};

    ChimeraTK::DeviceModule device{this, CDD1};
    ChimeraTK::DeviceModule device2{this, CDD2};
  };

  struct Test6A3InitialValueEceptionDummy {
    Test6A3InitialValueEceptionDummy() = default;
    ~Test6A3InitialValueEceptionDummy() = default;

    Test6A3DummyApplication application;
    ChimeraTK::TestFacility testFacility{application, false};
    ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg1};
  };

  /**
   *  D 6_a3 initial value from device in push mode
   * \anchor testD6_a3_InitialValue \ref testD6_a3_InitialValue
   */
  BOOST_AUTO_TEST_CASE(testD6_a3_InitialValue) {
    std::cout << "===   testD6_a3_InitialValue   === " << std::endl;

    Test6A3InitialValueEceptionDummy d;

    ChimeraTK::Device dev2;
    dev2.open(Test6A3DummyApplication::CDD2);
    dev2.write<int>("REG1/DUMMY_WRITEABLE", 99);

    d.application.run();

    ChimeraTK::Device dev;
    dev.open(Test6A3DummyApplication::CDD1);
    CHECK_TIMEOUT(dev.read<int>("REG1") == 99, 1000000);
    d.application.readerModule.p.get_future().wait();
    BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct Test6A4DummyApplication : ChimeraTK::Application {
    Test6A4DummyApplication() : Application("DummyApplication") {}
    ~Test6A4DummyApplication() override { shutdown(); }

    struct : NotifyingModule {
      using NotifyingModule::NotifyingModule;
      ChimeraTK::ScalarPushInput<int> reg1{this, "/REG1", "", ""};
    } readerModule{this, "ReaderModule", ""};

    WriterModule writerModule{this, "WriterModule", ""};
  };

  struct Test6A4InitialValueEceptionDummy {
    Test6A4InitialValueEceptionDummy() = default;
    ~Test6A4InitialValueEceptionDummy() = default;

    Test6A4DummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg1};
  };

  /**
   *  D 6_a4 initial value from output
   * \anchor testD6_a4_InitialValue \ref testD6_a4_InitialValue
   */
  BOOST_AUTO_TEST_CASE(testD6_a4_InitialValue) {
    std::cout << "===   testD6_a4_InitialValue   === " << std::endl;

    Test6A4InitialValueEceptionDummy d;

    d.application.run();
    d.application.readerModule.p.get_future().wait();
    BOOST_CHECK(d.application.readerModule.enteredTheMainLoop == true);
    CHECK_TIMEOUT(d.pushVariable == 777, 100000);
    BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct PollModule : NotifyingModule {
    using NotifyingModule::NotifyingModule;

    ChimeraTK::ScalarPollInput<int> pollInput{this, "/REG1", "", ""};
  };

  struct Test6BDummyApplication : ChimeraTK::Application {
    constexpr static const char* CDD = "(dummy?map=test-ro.map)";
    Test6BDummyApplication() : Application("DummyApplication") {}
    ~Test6BDummyApplication() override { shutdown(); }

    PollModule pollModule{this, "PollModule", ""};
    ChimeraTK::DeviceModule device{this, CDD};
  };

  struct Test6BInitialValueEceptionDummy {
    Test6BInitialValueEceptionDummy() = default;
    ~Test6BInitialValueEceptionDummy() = default;

    Test6BDummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    ChimeraTK::ScalarPollInput<int>& pollVariable{application.pollModule.pollInput};
  };

  /**
   *  D 6_b initial value from device in poll mode
   * \anchor testD6_b_InitialValue \ref testD6_b_InitialValue
   * FIXME: Is this supposed to test push variables in poll mode or poll variables?
   */
  BOOST_AUTO_TEST_CASE(testD6_b_InitialValue) {
    std::cout << "===   testD6_b_InitialValue   === " << std::endl;

    Test6BInitialValueEceptionDummy d;

    d.application.run();

    ChimeraTK::Device dev;
    dev.open(Test6BDummyApplication::CDD);
    dev.write<int>("REG1/DUMMY_WRITEABLE", 99);
    d.application.pollModule.p.get_future().wait();
    BOOST_CHECK(d.application.pollModule.enteredTheMainLoop == true);
    BOOST_CHECK(d.pollVariable == 99);
    BOOST_CHECK(d.pollVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  struct Test6CDummyApplication : ChimeraTK::Application {
    constexpr static const char* ExceptionDummyCDD1 = "(dummy:1?map=test-async.map)";
    Test6CDummyApplication() : Application("DummyApplication") {}
    ~Test6CDummyApplication() override { shutdown(); }

    struct : NotifyingModule {
      using NotifyingModule::NotifyingModule;
      ChimeraTK::ScalarPushInput<int> reg1{this, "/REG1", "", ""};
    } readerModule{this, "ReaderModule", ""};

    ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  };

  struct Test6CInitialValueEceptionDummy {
    Test6CInitialValueEceptionDummy() = default;
    ~Test6CInitialValueEceptionDummy() = default;

    Test6CDummyApplication application;
    ChimeraTK::TestFacility testFacitiy{application, false};
    ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
    ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg1};
  };
  /**
   *  D 6_c initial value from device in push mode
   * \anchor testD6_c_InitialValue \ref testD6_c_InitialValue
   */
  BOOST_AUTO_TEST_CASE(testD6_c_InitialValue) {
    std::cout << "===   testD6_c_InitialValue   === " << std::endl;

    Test6CInitialValueEceptionDummy d;

    d.application.run();

    ChimeraTK::Device dev;
    dev.open(Test6CDummyApplication::ExceptionDummyCDD1);
    dev.write<int>("REG1/DUMMY_WRITEABLE", 99);
    dev.getVoidRegisterAccessor("/DUMMY_INTERRUPT_3").write();
    d.application.readerModule.p.get_future().wait();
    BOOST_CHECK(d.application.readerModule.enteredTheMainLoop == true);
    BOOST_TEST(d.pushVariable == 99);
    BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }

  BOOST_AUTO_TEST_SUITE_END()

} // namespace Tests::testInitialValues
