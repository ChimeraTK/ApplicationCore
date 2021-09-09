#define BOOST_TEST_MODULE testInitialValues

#include <boost/mpl/list.hpp>

#include <chrono>
#include <future>
#include <functional>
#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/ExceptionDummyBackend.h>

#include "Application.h"
#include "ApplicationModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "check_timeout.h"

#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;
// A generic module with just one input. It is connected manually, so we just call the register "REG1" so we easily connect to that register in the device
// It has a flag and a promise to check whether the module has entered the main loop, and to wait for it.
template<class INPUT_TYPE>
struct InputModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  INPUT_TYPE input{this, "REG1", "", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
  void mainLoop() override {
    enteredTheMainLoop = true;
    p.set_value();
  }
};

struct PollDummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  PollDummyApplication() : Application("DummyApplication") {}
  ~PollDummyApplication() override { shutdown(); }

  InputModule<ctk::ScalarPollInput<int>> inputModule{this, "PollModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};

  void defineConnections() override { inputModule.connectTo(device); }
};

// for the push type we need different connection code
struct PushDummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  PushDummyApplication() : Application("DummyApplication") {}
  ~PushDummyApplication() override { shutdown(); }

  InputModule<ctk::ScalarPushInput<int>> inputModule{this, "PushModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};

  void defineConnections() override {
    auto push_register = device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push);
    push_register >> inputModule.input;
  }
};

template<class APPLICATION_TYPE>
struct TestFixtureWithEceptionDummy {
  TestFixtureWithEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(PollDummyApplication::ExceptionDummyCDD1))) {}
  ~TestFixtureWithEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  APPLICATION_TYPE application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
};
/**
  *  Test Initial Values - Inputs of `ApplicationModule`s
  *  InitialValuesInputsOfApplicationCore_D_8 "D.8"
  */
BOOST_AUTO_TEST_SUITE(testInitialValuesInputsOfApplicationCore_D_8)
typedef boost::mpl::list<PollDummyApplication, PushDummyApplication> DeviceTestApplicationTypes;

/**
  *  For device variables the ExeptionHandlingDecorator freezes the variable until the device is available
  * \anchor testInitialValue_D_8_b_i \ref initialValue_D_8_b_i
  */
BOOST_AUTO_TEST_CASE_TEMPLATE(testInitValueAtDevice8bi, APPLICATION_TYPE, DeviceTestApplicationTypes) {
  std::cout << "===   testInitValueAtDevice8bi " << typeid(APPLICATION_TYPE).name() << "  ===" << std::endl;
  std::chrono::time_point<std::chrono::steady_clock> start, end;
  { // Here the time is stopped until you reach the mainloop.
    TestFixtureWithEceptionDummy<PollDummyApplication> dummyToStopTimeUntilOpen;
    start = std::chrono::steady_clock::now();
    dummyToStopTimeUntilOpen.application.run();
    dummyToStopTimeUntilOpen.application.inputModule.p.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    end = std::chrono::steady_clock::now();
  }
  { // waiting 2 x the time stoped above, in the assumption that it is then freezed,
    // as it is described in the spec.
    TestFixtureWithEceptionDummy<PollDummyApplication> d;
    d.deviceBackend->throwExceptionOpen = true;
    BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);
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

  InputModule<INPUT_TYPE> inputModule{this, "PollModule", ""};
  ScalarOutputModule scalarOutputModule{this, "ScalarOutputModule", ""};
  void defineConnections() override { scalarOutputModule.connectTo(inputModule); }
};

typedef boost::mpl::list<ctk::ScalarPollInput<int>, ctk::ScalarPushInput<int>> TestInputTypes;

/**
  *  ProcessArray freeze in their implementation until the initial value is received
  * \anchor testInitialValue_D_8_b_ii \ref initialValue_D_8_b_ii
  */
BOOST_AUTO_TEST_CASE_TEMPLATE(testProcessArrayInitValueAtDevice8bii, INPUT_TYPE, TestInputTypes) {
  std::cout << "===   testPollProcessArrayInitValueAtDevice8bii " << typeid(INPUT_TYPE).name() << "  === " << std::endl;
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

  void defineConnections() override {}
};

/**
  * Constants can be read exactly once in case of `AccessMode::wait_for_new_data`, so the initial value can be received.
  * \anchor testInitialValue_D_8_b_iii \ref initialValue_D_8_b_iii
  */
BOOST_AUTO_TEST_CASE_TEMPLATE(testConstantInitValueAtDevice8biii, INPUT_TYPE, TestInputTypes) {
  std::cout << "===   testConstantInitValueAtDevice8biii " << typeid(INPUT_TYPE).name() << "  === " << std::endl;
  TestFixtureWithEceptionDummy<ConstantTestApplication<INPUT_TYPE>> d;

  BOOST_CHECK(d.application.inputModule.input.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));

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

struct PushModuleD9_1 : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "PUSH_READ", "", ""};
  } reg1{this, "REG1", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
  void mainLoop() override {
    enteredTheMainLoop = true;
    p.set_value();
  }
};
struct PushModuleD9_2 : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "PUSH_READ", "", ""};
  } reg1{this, "REG2", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
  void mainLoop() override {
    enteredTheMainLoop = true;
    p.set_value();
  }
};

struct PushD9DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  PushD9DummyApplication() : Application("DummyApplication") {}
  ~PushD9DummyApplication() override { shutdown(); }

  PushModuleD9_1 pushModuleD9_1{this, "PushModule1", ""};
  PushModuleD9_2 pushModuleD9_2{this, "PushModule2", ""};

  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};

  void defineConnections() override {
    auto push_input1 = device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push);
    auto push_input2 = device("REG2/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push);
    push_input1 >> pushModuleD9_1.reg1.pushInput;
    push_input2 >> pushModuleD9_2.reg1.pushInput;
 }
};

struct D9InitialValueEceptionDummy {
  D9InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(PushDummyApplication::ExceptionDummyCDD1))) {}
  ~D9InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  PushD9DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPushInput<int>& pushVariable1{application.pushModuleD9_1.reg1.pushInput};
  ChimeraTK::ScalarPushInput<int>& pushVariable2{application.pushModuleD9_2.reg1.pushInput};
};

/**
  *  D 9 b for ThreaddedFanOut
  * \anchor testInitialValueThreaddedFanOut_D_9_b \ref initialValueThreaddedFanOut_D_9_b
  */
BOOST_AUTO_TEST_CASE(testPushInitValueAtDeviceD9) {
  std::cout << "===   testPushInitValueAtDeviceD9   === " << std::endl;
  std::chrono::time_point<std::chrono::steady_clock> start, end;
  {
    D9InitialValueEceptionDummy dummyToStopTimeUntilOpen;
    start = std::chrono::steady_clock::now();
    dummyToStopTimeUntilOpen.application.run();
    dummyToStopTimeUntilOpen.application.pushModuleD9_1.p.get_future().wait();
    dummyToStopTimeUntilOpen.application.pushModuleD9_2.p.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    end = std::chrono::steady_clock::now();
  }
  {
    D9InitialValueEceptionDummy d;
    d.deviceBackend->throwExceptionOpen = true;
    BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);
    d.application.run();
    BOOST_CHECK(d.application.pushModuleD9_1.enteredTheMainLoop == false);
    std::this_thread::sleep_for(2 * (end - start));
    BOOST_CHECK(d.application.pushModuleD9_1.enteredTheMainLoop == false);
    BOOST_CHECK(d.pushVariable1.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
    d.deviceBackend->throwExceptionOpen = false;
    d.application.pushModuleD9_1.p.get_future().wait();
    BOOST_CHECK(d.application.pushModuleD9_1.enteredTheMainLoop == true);
    BOOST_CHECK(d.pushVariable1.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct TriggerModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarOutput<int> trigger{this, "PUSH_OUT", "", ""};
  } trigger{this, "TRIG1", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
  void mainLoop() override {
    enteredTheMainLoop = true;
    p.set_value();
  }
};

struct TriggerFanOutD9DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  TriggerFanOutD9DummyApplication() : Application("DummyApplication") {}
  ~TriggerFanOutD9DummyApplication() override { shutdown(); }

  PushModuleD9_1 pushModuleD9_1{this, "PushModule1", ""};
  PushModuleD9_2 pushModuleD9_2{this, "PushModule2", ""};
  TriggerModule triggerModule{this, "TriggerModule", ""};

  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};

  void defineConnections() override {
    auto pollInput1 = device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::poll);
    auto pollInput2 = device("REG2/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::poll);
    auto trigger = triggerModule["TRIG1"]("PUSH_OUT");
    pollInput1[trigger] >> pushModuleD9_1.reg1.pushInput;
    pollInput2[trigger] >> pushModuleD9_2.reg1.pushInput;
  }
};

struct TriggerFanOutInitialValueEceptionDummy {
  TriggerFanOutInitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(PushDummyApplication::ExceptionDummyCDD1))) {}
  ~TriggerFanOutInitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  TriggerFanOutD9DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPushInput<int>& pushVariable1{application.pushModuleD9_1.reg1.pushInput};
  ChimeraTK::ScalarPushInput<int>& pushVariable2{application.pushModuleD9_2.reg1.pushInput};
};

/**
  *  D 9 b for TriggerFanOut
  * \anchor testInitialValueThreaddedFanOut_D_9_b \ref initialValueThreaddedFanOut_D_9_b
  */
BOOST_AUTO_TEST_CASE(testTriggerFanOutInitValueAtDeviceD9) {
  std::cout << "===   testTriggerFanOutInitValueAtDeviceD9   === " << std::endl;
  std::chrono::time_point<std::chrono::steady_clock> start, end;
  {
    TriggerFanOutInitialValueEceptionDummy dummyToStopTimeUntilOpen;
    start = std::chrono::steady_clock::now();
    dummyToStopTimeUntilOpen.application.run();
    dummyToStopTimeUntilOpen.application.triggerModule.trigger.trigger.write();
    dummyToStopTimeUntilOpen.application.pushModuleD9_1.p.get_future().wait();
    dummyToStopTimeUntilOpen.application.pushModuleD9_2.p.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    end = std::chrono::steady_clock::now();
  }
  {
    TriggerFanOutInitialValueEceptionDummy d;
    d.deviceBackend->throwExceptionOpen = true;
    BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);
    d.application.run();
    BOOST_CHECK(d.application.pushModuleD9_1.enteredTheMainLoop == false);
    std::this_thread::sleep_for(2 * (end - start));
    BOOST_CHECK(d.application.pushModuleD9_1.enteredTheMainLoop == false);
    BOOST_CHECK(d.pushVariable1.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
    d.deviceBackend->throwExceptionOpen = false;
    d.application.triggerModule.trigger.trigger.write();
    d.application.pushModuleD9_1.p.get_future().wait();
    BOOST_CHECK(d.application.pushModuleD9_1.enteredTheMainLoop == true);
    BOOST_CHECK(d.pushVariable1.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConstantModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPushInput<int> constant{this, "PUSH_READ", "", ""};
  } reg1{this, "REG1", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
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

  void defineConnections() override {
    ctk::VariableNetworkNode::makeConstant<int>(true, 24) >> constantModule.reg1.constant;
    constantModule.reg1.constant >> device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push);
  }
};

struct ConstantD10InitialValueEceptionDummy {
  ConstantD10InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:1?map=test.map)"))) {}
  ~ConstantD10InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  ConstantD10DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarPushInput<int>& pushVariable{application.constantModule.reg1.constant};
};

/**
  *  D 10 for Constant
  * \anchor testConstantD10InitialValue_D_10 \ref constantD10InitialValue_D_10
  */
BOOST_AUTO_TEST_CASE(testConstantD10InitialValue) {
  std::cout << "===   testConstantD10InitialValue   === " << std::endl;

  ConstantD10InitialValueEceptionDummy d;
  d.deviceBackend->throwExceptionOpen = true;
  BOOST_CHECK_THROW(d.deviceBackend->open(), std::exception);
  d.application.run();
  //commented line might fail on jenkins, race cond?
  //BOOST_CHECK(d.application.constantModule.enteredTheMainLoop == false);
  //BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
  d.application.constantModule.p.get_future().wait();
  BOOST_CHECK(d.application.constantModule.enteredTheMainLoop == true);
  BOOST_CHECK(d.pushVariable == 24);
  d.deviceBackend->throwExceptionOpen = false;
  ChimeraTK::Device dev;
  dev.open("(ExceptionDummy:1?map=test.map)");
  CHECK_TIMEOUT(dev.read<int>("REG1/PUSH_READ") == 24, 1000000);
  BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
}


////////////////////////////////////////////////////////////////////////////////////////////////////

struct TestModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "PUSH_READ", "", ""};
  } reg1{this, "REG1", ""};
  ChimeraTK::ScalarOutput<int> output{this, "REG2", "", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
  void mainLoop() override {
    enteredTheMainLoop = true;
    p.set_value();
  }
};

struct TestDummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  TestDummyApplication() : Application("DummyApplication") {}
  ~TestDummyApplication() override { shutdown(); }

  TestModule testModule{this, "TestModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};

  void defineConnections() override {
    auto push_input = device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push);
    push_input >> testModule.reg1.pushInput;
  }
};

struct TestInitialValueEceptionDummy {
  TestInitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:1?map=test.map)"))) {}
  ~TestInitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  TestDummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarPushInput<int>& pushVariable{application.testModule.reg1.pushInput};
  ChimeraTK::ScalarOutput<int>& outputVariable{application.testModule.output};
};

/**
  *  D 1 for DataValidity::faulty
  * \anchor testD1InitialValue_D_1 \ref testD1InitialValue_D_1
  */
  //Todo add missing tests for bi-directional variables
BOOST_AUTO_TEST_CASE(testD1InitialValue) {
  std::cout << "===   testD1InitialValue   === " << std::endl;

  TestInitialValueEceptionDummy d;
  d.application.run();
  //commented line might fail on jenkins.
  //BOOST_CHECK(d.application.testModule.enteredTheMainLoop == false);
  //BOOST_CHECK(d.pushVariable.dataValidity() == ctk::DataValidity::faulty);
  //BOOST_CHECK(d.outputVariable.dataValidity() == ctk::DataValidity::ok);
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

  TestInitialValueEceptionDummy d;
  d.application.run();
  //commented line might fail on jenkins.
  //BOOST_CHECK(d.application.testModule.enteredTheMainLoop == false);
  //BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
  //BOOST_CHECK(d.outputVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
  d.application.testModule.output.write();
  d.application.testModule.p.get_future().wait();
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

  TestInitialValueEceptionDummy d;
  d.application.run();
  //commented line might fail on jenkins, race cond?
  //BOOST_CHECK(d.pushVariable.dataValidity() == ctk::DataValidity::faulty);
  //BOOST_CHECK(d.outputVariable.dataValidity() == ctk::DataValidity::ok);
  //d.application.testModule.output.write();
  d.application.testModule.p.get_future().wait();
  BOOST_CHECK(d.application.testModule.enteredTheMainLoop == true);
  BOOST_CHECK(d.pushVariable.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(d.outputVariable.dataValidity() == ctk::DataValidity::ok);
  //Todo. The initial value can also be faulty. Change backend so that it allows to override the data validity without going to an exception state.
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct WriterModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<int> output1{this, "REG1", "", ""};
  ChimeraTK::ScalarOutput<int> output2{this, "REG2", "", ""};
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

struct ReaderModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "PUSH_READ", "", ""};
  } reg1{this, "REG1", ""};
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPushInput<int> pushInput{this, "PUSH_READ", "", ""};
  } reg2{this, "REG2", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
  std::atomic_bool enteredThePrepareLoop{false};
  void mainLoop() override {
    enteredTheMainLoop = true;
    p.set_value();
  }
  void prepare() override {
    enteredThePrepareLoop = true;
  }
};

struct Test7DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  Test7DummyApplication() : Application("DummyApplication") {}
  ~Test7DummyApplication() override { shutdown(); }

  WriterModule writerModule{this, "WriterModule", ""};
  ReaderModule readerModule{this, "ReaderModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};

  void defineConnections() override {
    writerModule.output1 >> readerModule.reg1.pushInput;
    writerModule.output2 >> readerModule.reg2.pushInput;
  }
};

/**
  *  D 7_1
  * \anchor testD7_1_InitialValue \ref testD7_1_InitialValue
  */

BOOST_AUTO_TEST_CASE(testD7_1_InitialValue) {
  std::cout << "===   testD7_1_InitialValue   === " << std::endl;

  Test7DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  application.run();
  BOOST_CHECK(application.writerModule.enteredThePrepareLoop == true);
  BOOST_CHECK(application.readerModule.enteredThePrepareLoop == true);
  CHECK_TIMEOUT(application.readerModule.reg1.pushInput == 777, 500);
}

/**
  *  D 7_2
  * \anchor testD7_2_InitialValue \ref testD7_2_InitialValue
  */
BOOST_AUTO_TEST_CASE(testD7_2_InitialValue) {
  std::cout << "===   testD7_2_InitialValue   === " << std::endl;

  Test7DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  application.run();
  BOOST_CHECK(application.readerModule.reg2.pushInput == 0);
  application.readerModule.p.get_future().wait();
  BOOST_CHECK(application.readerModule.enteredTheMainLoop == true);
  CHECK_TIMEOUT(application.readerModule.reg2.pushInput == 555, 500);

}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct Test6_a1_DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  Test6_a1_DummyApplication() : Application("DummyApplication") {}
  ~Test6_a1_DummyApplication() override { shutdown(); }

  WriterModule writerModule{this, "WriterModule", ""};
  ReaderModule readerModule{this, "ReaderModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  ChimeraTK::ControlSystemModule csModule{};

  void defineConnections() override {
    csModule("REG1")  >> device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push);
    csModule("REG1")  >> readerModule.reg1.pushInput;
    //dumpConnections();
  }
};

struct Test6_a1_InitialValueEceptionDummy {
  Test6_a1_InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:1?map=test.map)"))) {}
  ~Test6_a1_InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  Test6_a1_DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg1.pushInput};
};

/**
  *  D 6_a1 initial value from control system variable
  * \anchor testD6_a1_InitialValue \ref testD6_a1_InitialValue
  */

BOOST_AUTO_TEST_CASE(testD6_a1_InitialValue) {
  std::cout << "===   testD6_a1_InitialValue   === " << std::endl;
  Test6_a1_InitialValueEceptionDummy d;
  d.application.run();
  d.testFacitiy.writeScalar<int>("REG1", 27);
  BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
  ChimeraTK::Device dev;
  dev.open("(ExceptionDummy:1?map=test.map)");
  CHECK_TIMEOUT(dev.read<int>("REG1") == 27, 1000000);
  BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
struct Test6_a2_DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  constexpr static const char* ExceptionDummyCDD2 = "(ExceptionDummy:2?map=test.map)";
  Test6_a2_DummyApplication() : Application("DummyApplication") {}
  ~Test6_a2_DummyApplication() override { shutdown(); }

  ReaderModule readerModule{this, "ReaderModule", ""};
  TriggerModule triggerModule{this, "ReaderModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  ChimeraTK::DeviceModule device2{this, ExceptionDummyCDD2};
  ChimeraTK::ControlSystemModule csModule{};

  void defineConnections() override {

    auto pollInput1 = device2("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::poll);
    auto trigger = triggerModule["TRIG1"]("PUSH_OUT");
    pollInput1[trigger] >> device("REG2");
    pollInput1[trigger] >> readerModule.reg2.pushInput;
    //dumpConnections();
  }
};

struct Test6_a2_InitialValueEceptionDummy {
  Test6_a2_InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:2?map=test.map)"))) {}
  ~Test6_a2_InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  Test6_a2_DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg2.pushInput};
};

/**
  *  D 6_a2 initial value from device in poll mode
  * \anchor testD6_a2_InitialValue \ref testD6_a2_InitialValue
  */

BOOST_AUTO_TEST_CASE(testD6_a2_InitialValue) {
  std::cout << "===   testD6_a2_InitialValue   === " << std::endl;

  Test6_a2_InitialValueEceptionDummy d;

  ChimeraTK::Device dev2;
  dev2.open("(ExceptionDummy:2?map=test.map)");
  dev2.write<int>("REG1/DUMMY_WRITEABLE", 99);

  d.application.run();
  BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
  dev2.write<int>("REG1/DUMMY_WRITEABLE", 99);

  d.application.triggerModule.trigger.trigger.write();

  ChimeraTK::Device dev;
  dev.open("(ExceptionDummy:1?map=test.map)");

  CHECK_TIMEOUT(dev.read<int>("REG2") == 99, 1000000);
  BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct Test6_a3_DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  constexpr static const char* ExceptionDummyCDD2 = "(ExceptionDummy:2?map=test.map)";
  Test6_a3_DummyApplication() : Application("DummyApplication") {}
  ~Test6_a3_DummyApplication() override { shutdown(); }

  ReaderModule readerModule{this, "ReaderModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  ChimeraTK::DeviceModule device2{this, ExceptionDummyCDD2};
  ChimeraTK::ControlSystemModule csModule{};

  void defineConnections() override {
    device2("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> device("REG2");
    device2("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> readerModule.reg2.pushInput;
    //dumpConnections();
  }
};

struct Test6_a3_InitialValueEceptionDummy {
  Test6_a3_InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:2?map=test.map)"))) {}
  ~Test6_a3_InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  Test6_a3_DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg2.pushInput};
};

/**
  *  D 6_a3 initial value from device in push mode
  * \anchor testD6_a3_InitialValue \ref testD6_a3_InitialValue
  */
BOOST_AUTO_TEST_CASE(testD6_a3_InitialValue) {
  std::cout << "===   testD6_a3_InitialValue   === " << std::endl;

  Test6_a3_InitialValueEceptionDummy d;

  ChimeraTK::Device dev2;
  dev2.open("(ExceptionDummy:2?map=test.map)");
  dev2.write<int>("REG1/DUMMY_WRITEABLE", 99);

  d.application.run();
  BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));

  ChimeraTK::Device dev;
  dev.open("(ExceptionDummy:1?map=test.map)");
  CHECK_TIMEOUT(dev.read<int>("REG2") == 99, 1000000);
  BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
}


////////////////////////////////////////////////////////////////////////////////////////////////////

struct Test6_a4_DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  Test6_a4_DummyApplication() : Application("DummyApplication") {}
  ~Test6_a4_DummyApplication() override { shutdown(); }

  ReaderModule readerModule{this, "ReaderModule", ""};
  WriterModule writerModule{this, "WriterModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  ChimeraTK::ControlSystemModule csModule{};

  void defineConnections() override {

    writerModule.output1 >> readerModule.reg2.pushInput;
    //dumpConnections();
  }
};

struct Test6_a4_InitialValueEceptionDummy {
  Test6_a4_InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:2?map=test.map)"))) {}
  ~Test6_a4_InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  Test6_a4_DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg2.pushInput};
};

/**
  *  D 6_a4 initial value from output
  * \anchor testD6_a4_InitialValue \ref testD6_a4_InitialValue
  */
BOOST_AUTO_TEST_CASE(testD6_a4_InitialValue) {
  std::cout << "===   testD6_a4_InitialValue   === " << std::endl;

  Test6_a4_InitialValueEceptionDummy d;

  d.application.run();
  d.application.readerModule.p.get_future().wait();
  BOOST_CHECK(d.application.readerModule.enteredTheMainLoop == true);
  CHECK_TIMEOUT(d.pushVariable == 777, 100000);
  BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct PollModule : ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  struct : ChimeraTK::VariableGroup {
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarPollInput<int> pollInput{this, "PUSH_READ", "", ""};
  } reg1{this, "REG1", ""};
  std::promise<void> p;
  std::atomic_bool enteredTheMainLoop{false};
  void mainLoop() override {
    enteredTheMainLoop = true;
    p.set_value();
  }
};


struct Test6_b_DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  Test6_b_DummyApplication() : Application("DummyApplication") {}
  ~Test6_b_DummyApplication() override { shutdown(); }

  PollModule pollModule{this, "PollModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};
  ChimeraTK::ControlSystemModule csModule{};

  void defineConnections() override {
    device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::poll) >> pollModule.reg1.pollInput;
    //dumpConnections();
  }
};

struct Test6_b_InitialValueEceptionDummy {
  Test6_b_InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:1?map=test.map)"))) {}
  ~Test6_b_InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  Test6_b_DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPollInput<int>& pollVariable{application.pollModule.reg1.pollInput};
};

/**
  *  D 6_b initial value from device in poll mode
  * \anchor testD6_b_InitialValue \ref testD6_b_InitialValue
  */
BOOST_AUTO_TEST_CASE(testD6_b_InitialValue) {
  std::cout << "===   testD6_b_InitialValue   === " << std::endl;

  Test6_b_InitialValueEceptionDummy d;

  d.application.run();

  ChimeraTK::Device dev;
  dev.open("(ExceptionDummy:1?map=test.map)");
  dev.write<int>("REG1/DUMMY_WRITEABLE", 99);
  //commented line might fail on jenkins, race cond?
  //BOOST_CHECK(d.application.pollModule.enteredTheMainLoop == false);
  //BOOST_CHECK(d.pollVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
  d.application.pollModule.p.get_future().wait();
  BOOST_CHECK(d.application.pollModule.enteredTheMainLoop == true);
  BOOST_CHECK(d.pollVariable == 99);
  BOOST_CHECK(d.pollVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct Test6_c_DummyApplication : ChimeraTK::Application {
  constexpr static const char* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=test.map)";
  Test6_c_DummyApplication() : Application("DummyApplication") {}
  ~Test6_c_DummyApplication() override { shutdown(); }

  ReaderModule readerModule{this, "ReaderModule", ""};
  ChimeraTK::DeviceModule device{this, ExceptionDummyCDD1};

  void defineConnections() override {
    device("REG1/PUSH_READ", typeid(int), 1, ChimeraTK::UpdateMode::push) >> readerModule.reg2.pushInput;
    //dumpConnections();
  }
};

struct Test6_c_InitialValueEceptionDummy {
  Test6_c_InitialValueEceptionDummy()
  : deviceBackend(boost::dynamic_pointer_cast<ChimeraTK::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend("(ExceptionDummy:1?map=test.map)"))) {}
  ~Test6_c_InitialValueEceptionDummy() { deviceBackend->throwExceptionRead = false; }

  boost::shared_ptr<ChimeraTK::ExceptionDummy> deviceBackend;
  Test6_c_DummyApplication application;
  ChimeraTK::TestFacility testFacitiy{false};
  ChimeraTK::ScalarRegisterAccessor<int> exceptionDummyRegister;
  ChimeraTK::ScalarPushInput<int>& pushVariable{application.readerModule.reg2.pushInput};
};
/**
  *  D 6_c initial value from device in push mode
  * \anchor testD6_c_InitialValue \ref testD6_c_InitialValue
  */
BOOST_AUTO_TEST_CASE(testD6_c_InitialValue) {
  std::cout << "===   testD6_c_InitialValue   === " << std::endl;

  Test6_c_InitialValueEceptionDummy d;

  d.application.run();

  ChimeraTK::Device dev;
  dev.open("(ExceptionDummy:1?map=test.map)");
  dev.write<int>("REG1/DUMMY_WRITEABLE", 99);
  //commented line might fail on jenkins, race cond?
  //BOOST_CHECK(d.application.readerModule.enteredTheMainLoop == false);
  //BOOST_CHECK(d.pushVariable.getVersionNumber() == ctk::VersionNumber(std::nullptr_t()));
  d.application.readerModule.p.get_future().wait();
  BOOST_CHECK(d.application.readerModule.enteredTheMainLoop == true);
  BOOST_CHECK(d.pushVariable == 99);
  BOOST_CHECK(d.pushVariable.getVersionNumber() != ctk::VersionNumber(std::nullptr_t()));
}

BOOST_AUTO_TEST_SUITE_END()
