#define BOOST_TEST_MODULE testExceptionHandling

#include <chrono>
#include <cstring>
#include <future>

#include <boost/mpl/list.hpp>

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
#include "fixtures.h"

// this #include must come last
#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;
using Fixture = fixture_with_poll_and_push_input<false>;

/*
 * This test suite checks behavior on a device related runtime error.
 */
BOOST_AUTO_TEST_SUITE(runtimeErrorHandling)

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_1 \ref exceptionHandling_b_2_1 "B.2.1"
 *
 * "The exception status is published as a process variable together with an error message."
 */
BOOST_FIXTURE_TEST_CASE(B_2_1, Fixture) {
  std::cout << "B_2_1 - fault indicators" << std::endl;

  // These are instantiated in the fixture:
  // status -> /Devices/(ExceptionDummy:1?map=test.map)/status
  // message -> /Devices/(ExceptionDummy:1?map=test.map)/message

  BOOST_CHECK_EQUAL(status, 0);
  BOOST_CHECK_EQUAL(static_cast<std::string>(message), "");

  deviceBackend->throwExceptionRead = true;
  application.pollModule.pollInput.read(); // causes device exception

  CHECK_TIMEOUT(status.readNonBlocking() == true, 10000);
  CHECK_TIMEOUT(message.readNonBlocking() == true, 10000);
  BOOST_CHECK_EQUAL(status, 1);
  BOOST_CHECK(!static_cast<std::string>(message).empty());

  deviceBackend->throwExceptionRead = false;

  CHECK_TIMEOUT(status.readNonBlocking() == true, 10000);
  CHECK_TIMEOUT(message.readNonBlocking() == true, 10000);
  BOOST_CHECK_EQUAL(status, 0);
  BOOST_CHECK_EQUAL(static_cast<std::string>(message), "");
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_2_poll \ref exceptionHandling_b_2_2_2 "B.2.2.2"
 * 
 * "The DataValidity::faulty flag resulting from the fault state is propagated once, even if the variable had the a
 * DataValidity::faulty flag already set previously for another reason."
 */

BOOST_FIXTURE_TEST_CASE(B_2_2_2_poll, Fixture) {
  std::cout << "B_2_2_2_poll - exception with previous DataValidity::faulty" << std::endl;

  // initialize to known value in deviceBackend register
  write(exceptionDummyRegister, 100);
  pollVariable.read();
  auto versionNumberBeforeRuntimeError = pollVariable.getVersionNumber();

  // Modify the validity flag of the application buffer. Note: This is not a 100% sane test, since in theory it could
  // make a difference whether the flag is actually coming from the device, but implementing such test is tedious. It
  // does not seem worth the effort, as it is unlikely that even a future, refactored implementation would be sensitive
  // to this difference (flag would need to be stored artifically in an additional place). It is only important to
  // change the validity on all decorator levels.
  pollVariable.setDataValidity(ctk::DataValidity::faulty);
  for(auto& e : pollVariable.getHardwareAccessingElements()) {
    e->setDataValidity(ctk::DataValidity::faulty);
  }

  // modify value in register after breaking the device
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 10);

  // This read should be skipped but obtain a new version number
  pollVariable.read();
  auto versionNumberOnRuntimeError = pollVariable.getVersionNumber();
  BOOST_CHECK_EQUAL(pollVariable, 100);
  BOOST_CHECK(pollVariable.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(versionNumberOnRuntimeError > versionNumberBeforeRuntimeError);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_2_push \ref exceptionHandling_b_2_2_2 "B.2.2.2"
 * 
 * "The DataValidity::faulty flag resulting from the fault state is propagated once, even if the variable had the a
 * DataValidity::faulty flag already set previously for another reason."
 */

BOOST_FIXTURE_TEST_CASE(B_2_2_2_push, Fixture) {
  std::cout << "B_2_2_2_push - exception with previous DataValidity::faulty" << std::endl;

  // verify normal operation
  // initialize to known value in deviceBackend register
  write(exceptionDummyRegister, 101);
  ctk::VersionNumber versionNumberBeforeRuntimeError = {};
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), versionNumberBeforeRuntimeError);
  pushVariable.read();

  // Modify the validity flag of the application buffer (see note above in poll-type test)
  pushVariable.setDataValidity(ctk::DataValidity::faulty);
  for(auto& e : pushVariable.getHardwareAccessingElements()) {
    e->setDataValidity(ctk::DataValidity::faulty);
  }

  // modify value in register after breaking the device
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 11);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), versionNumberBeforeRuntimeError);

  // This read should be skipped but obtain a new version number
  pushVariable.read();
  auto versionNumberOnRuntimeError = pushVariable.getVersionNumber();
  BOOST_CHECK_EQUAL(pushVariable, 101);
  BOOST_CHECK(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(versionNumberOnRuntimeError > versionNumberBeforeRuntimeError);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_3 \ref exceptionHandling_b_2_2_3 "B.2.2.3"
 * 
 * "Read operations without AccessMode::wait_for_new_data are skipped until the device is fully recovered again (cf.
 * 3.1). The first skipped read operation will have a new VersionNumber."
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_3, Fixture) {
  std::cout << "B_2_2_3 - skip poll type reads" << std::endl;

  // initialize to known value in deviceBackend register
  write(exceptionDummyRegister, 100);
  pollVariable.read();
  auto versionNumberBeforeRuntimeError = pollVariable.getVersionNumber();

  // modify value in register after breaking the device
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 10);

  // This read should be skipped but obtain a new version number
  pollVariable.read();
  auto versionNumberOnRuntimeError = pollVariable.getVersionNumber();
  BOOST_CHECK_EQUAL(pollVariable, 100);
  BOOST_CHECK(pollVariable.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(versionNumberOnRuntimeError > versionNumberBeforeRuntimeError);

  // This read should be skipped too, this time without a new version number
  pollVariable.read();
  BOOST_CHECK_EQUAL(pollVariable, 100);
  BOOST_CHECK(pollVariable.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(pollVariable.getVersionNumber(), versionNumberOnRuntimeError);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_blocking \ref exceptionHandling_b_2_2_4 "B.2.2.4"
 * 
 * "Read operations with AccessMode::wait_for_new_data will be skipped once for each accessor to propagate the
 * DataValidity::faulty flag (which counts as new data, i.e. readNonBlocking()/readLatest() will return true
 * (= hasNewData), and a new VersionNumber is obtained)."
 * 
 * This test is for blocking read().
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_blocking, Fixture) {
  std::cout << "B_2_2_4_blocking - first skip of blocking read" << std::endl;

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), version);

  // as soon as the fault state has arrived, the operation is skipped
  pushVariable.read();
  BOOST_CHECK_NE(pushVariable, 100); // value did not come through
  BOOST_CHECK(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  auto versionNumberOnRuntimeError = pushVariable.getVersionNumber();
  BOOST_CHECK(versionNumberOnRuntimeError > version);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_nonBlocking \ref exceptionHandling_b_2_2_4 "B.2.2.4"
 * 
 * "Read operations with AccessMode::wait_for_new_data will be skipped once for each accessor to propagate the
 * DataValidity::faulty flag (which counts as new data, i.e. readNonBlocking()/readLatest() will return true
 * (= hasNewData), and a new VersionNumber is obtained)."
 * 
 * This test is for readNonBlocking().
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_nonBlocking, Fixture) {
  std::cout << "B_2_2_4_nonBlocking - first skip of readNonBlocking" << std::endl;

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), version);

  // as soon as the fault state has arrived, the operation is skipped
  CHECK_TIMEOUT(pushVariable.readNonBlocking() == true, 10000);
  BOOST_CHECK_NE(pushVariable, 100); // value did not come through
  BOOST_CHECK(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  auto versionNumberOnRuntimeError = pushVariable.getVersionNumber();
  BOOST_CHECK(versionNumberOnRuntimeError > version);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_latest \ref exceptionHandling_b_2_2_4 "B.2.2.4"
 * 
 * "Read operations with AccessMode::wait_for_new_data will be skipped once for each accessor to propagate the
 * DataValidity::faulty flag (which counts as new data, i.e. readNonBlocking()/readLatest() will return true
 * (= hasNewData), and a new VersionNumber is obtained)."
 * 
 * This test is for readLatest().
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_latest, Fixture) {
  std::cout << "B_2_2_4_latest - first skip of readLatest" << std::endl;

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), version);

  // as soon as the fault state has arrived, the operation is skipped
  CHECK_TIMEOUT(pushVariable.readLatest() == true, 10000);
  BOOST_CHECK_NE(pushVariable, 100); // value did not come through
  BOOST_CHECK(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  auto versionNumberOnRuntimeError = pushVariable.getVersionNumber();
  BOOST_CHECK(versionNumberOnRuntimeError > version);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_1_nonBlocking \ref exceptionHandling_b_2_2_4_1 "B.2.2.4.1"
 * 
 * [After first skipped read operation in 2.2.4, the following] "non-blocking read operations (readNonBlocking() and
 * readLatest()) are skipped and return false (= no new data), until the device is recovered".
 * 
 * This test is for readNonBlocking().
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_1_nonBlocking, Fixture) {
  std::cout << "B_2_2_4_1_nonBlocking - following skip readNonBlocking" << std::endl;

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), version);

  // perform first skipped operation
  pushVariable.read();
  auto versionNumberOnRuntimeError = pushVariable.getVersionNumber();

  // subsequent calls to readLatest on runtime error are skipped.
  for(size_t i = 0; i < 5; ++i) {
    usleep(1000);
    BOOST_CHECK_EQUAL(pushVariable.readNonBlocking(), false);
    BOOST_CHECK(versionNumberOnRuntimeError == pushVariable.getVersionNumber());
    BOOST_CHECK(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  }
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_1_latest \ref exceptionHandling_b_2_2_4_1 "B.2.2.4.1"
 * 
 * [After first skipped read operation in 2.2.4, the following] "non-blocking read operations (readNonBlocking() and
 * readLatest()) are skipped and return false (= no new data), until the device is recovered".
 * 
 * This test is for readLatest().
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_1_latest, Fixture) {
  std::cout << "B_2_2_4_1_latest - following skip readLatest" << std::endl;

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), version);

  // perform first skipped operation
  pushVariable.read();
  auto versionNumberOnRuntimeError = pushVariable.getVersionNumber();

  // subsequent calls to readLatest on runtime error are skipped.
  for(size_t i = 0; i < 5; ++i) {
    usleep(1000);
    BOOST_CHECK_EQUAL(pushVariable.readLatest(), false);
    BOOST_CHECK(versionNumberOnRuntimeError == pushVariable.getVersionNumber());
    BOOST_CHECK(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  }
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_2 \ref exceptionHandling_b_2_2_4_2 "B.2.2.4.2"
 * 
 * [After first skipped read operation in 2.2.4, the following] "blocking read operations (read()) will be frozen until
 * the device is recovered."
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_2, Fixture) {
  std::cout << "B_2_2_4_2 - freeze blocking read" << std::endl;

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), version);

  // perform first skipped operation
  pushVariable.read();

  // subsequent read operations should be frozen
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"));
  auto f = std::async(std::launch::async, [&]() { pushVariable.read(); });
  BOOST_CHECK(f.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);

  // FIXME: This should not be necessary. Bug in ApplicationCore's shutdown procedure!?
  deviceBackend->throwExceptionRead = false;
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_3 \ref exceptionHandling_b_2_2_4_3 "B.2.2.4.3"
 * 
 * "After the device is fully recovered (cf. 3.1), the current value is (synchronously) read from the device. This is
 * the first value received by the accessor after an exception."
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_3, Fixture) {
  std::cout << "B_2_2_4_3 - value after recovery" << std::endl;

  // Normal behaviour
  write(exceptionDummyRegister, 66);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), {});
  pushVariable.read();

  // Change value while in exception state
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 77);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"));

  pushVariable.read();
  BOOST_CHECK_EQUAL(pushVariable, 66);

  // Recover from exception state
  deviceBackend->throwExceptionRead = false;

  // Now the value needs to be read
  CHECK_TIMEOUT(pushVariable.readNonBlocking() == true, 10000);
  BOOST_CHECK_EQUAL(pushVariable, 77);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_5 \ref exceptionHandling_b_2_2_5 "B.2.2.5"
 * 
 * "The VersionNumbers returned in case of an exception are the same for the same exception, even across variables and
 * modules. It will be generated in the moment the exception is reported."
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_5, Fixture) {
  std::cout << "B_2_2_5 - version numbers across PVs" << std::endl;

  // Go to exception state, report it explicitly
  ctk::VersionNumber someVersionBeforeReporting = {};
  deviceBackend->throwExceptionOpen = true; // required to make sure device stays down
  application.device.reportException("explicit report by test");
  deviceBackend->setException(); // FIXME: should this be called by reportException()??
  ctk::VersionNumber someVersionAfterReporting = {};

  //  Check push variable
  pushVariable.read();
  auto exceptionVersion = pushVariable.getVersionNumber();
  BOOST_CHECK(exceptionVersion > someVersionBeforeReporting);
  BOOST_CHECK(exceptionVersion < someVersionAfterReporting);

  // Check poll variable
  pollVariable.read();
  BOOST_CHECK(pollVariable.getVersionNumber() == exceptionVersion);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_6 \ref exceptionHandling_b_2_2_6 "B.2.2.6"
 * 
 * "The data buffer is not updated. This guarantees that the data buffer stays on the last known value if the user code
 * has not modified it since the last read."
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_6, Fixture) {
  std::cout << "B_2_2_6 - data buffer not updated" << std::endl;

  // Write both variables once (without error state)
  write(exceptionDummyRegister, 66);
  deviceBackend->triggerPush(ctk::RegisterPath("REG1/PUSH_READ"), {});
  pushVariable.read();
  BOOST_CHECK_EQUAL(pushVariable, 66);

  write(exceptionDummyRegister, 67);
  pollVariable.read();
  BOOST_CHECK_EQUAL(pollVariable, 67);

  // Go to exception state, report it explicitly
  write(exceptionDummyRegister, 68);
  ctk::VersionNumber someVersionBeforeReporting = {};
  deviceBackend->throwExceptionOpen = true; // required to make sure device stays down
  application.device.reportException("explicit report by test");
  deviceBackend->setException(); // FIXME: should this be called by reportException()??
  ctk::VersionNumber someVersionAfterReporting = {};

  //  Check push variable
  pushVariable = 42;
  pushVariable.read();
  BOOST_CHECK_EQUAL(pushVariable, 42);

  // Check poll variable
  pollVariable = 43;
  pollVariable.read();
  BOOST_CHECK_EQUAL(pollVariable, 43);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_3_1 \ref exceptionHandling_b_2_3_1 "B.2.3.1"
 * 
 * "In case of a fault state (new or persisting), the actual write operation will take place asynchronously when the
 * device is recovering."
 */
BOOST_FIXTURE_TEST_CASE(B_2_3_1, Fixture) {
  std::cout << "B_2_3_1 - delay write" << std::endl;

  // trigger runtime error
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // write when device is faulty
  outputVariable = 100;
  outputVariable.write();
  usleep(100000); // allow data to propagate if test would fail
  BOOST_CHECK_NE(read<int>(exceptionDummyRegister), 100);

  // recover
  deviceBackend->throwExceptionRead = false;
  pollVariable.read();

  // delayed value should arrive at device
  CHECK_EQUAL_TIMEOUT(read<int>(exceptionDummyRegister), 100, 10000);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_3_3 \ref exceptionHandling_b_2_3_3 "B.2.3.3"
 * 
 * "The return value of write() indicates whether data was lost in the transfer. If the write has to be delayed due to
 * an exception, the return value will be true (= data lost) if a previously delayed and not-yet written value is
 * discarded in the process, false (= no data lost) otherwise."
*/
BOOST_FIXTURE_TEST_CASE(B_2_3_3, Fixture) {
  std::cout << "B_2_3_3 - return value of write" << std::endl;
  // trigger runtime error
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // multiple writes on faulty device.
  outputVariable = 100;
  auto testval = outputVariable.write();
  BOOST_CHECK_EQUAL(testval, false); // data not lost

  outputVariable = 101;
  BOOST_CHECK_EQUAL(outputVariable.write(), true); // data lost
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/
/* Non-systematic tests following (to be reviewed and likely removed) */
/**********************************************************************************************************************/
/**********************************************************************************************************************/

constexpr char ExceptionDummyCDD1[] = "(ExceptionDummy:1?map=test3.map)";
constexpr char ExceptionDummyCDD2[] = "(ExceptionDummy:2?map=test3.map)";
constexpr char ExceptionDummyCDD3[] = "(ExceptionDummy:3?map=test4.map)";

/* dummy application */

struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  void defineConnections() {} // the setup is done in the tests

  ctk::DeviceModule dev1{this, ExceptionDummyCDD1};
  ctk::DeviceModule dev2{this, ExceptionDummyCDD2};
  ctk::ControlSystemModule cs;
};

struct OutputModule : public ctk::ApplicationModule {
  OutputModule(EntityOwner* owner, const std::string& name, const std::string& description,
      ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
      const std::unordered_set<std::string>& tags = {})
  : ApplicationModule(owner, name, description, hierarchyModifier, tags), mainLoopStarted(2) {}

  ctk::ScalarPushInput<int32_t> trigger{this, "trigger", "", "I wait for this to start."};
  ctk::ScalarOutput<int32_t> actuator{this, "actuator", "", "This is where I write to."};

  void mainLoop() override {
    mainLoopStarted.wait();

    trigger.read();
    actuator = int32_t(trigger);
    actuator.write();
  }

  // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
  // mainLoop(). This is required to make sure the initial value propagation is done.
  // execute this right after the Application::run():
  //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
  boost::barrier mainLoopStarted;
};

struct InputModule : public ctk::ApplicationModule {
  InputModule(EntityOwner* owner, const std::string& name, const std::string& description,
      ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
      const std::unordered_set<std::string>& tags = {})
  : ApplicationModule(owner, name, description, hierarchyModifier, tags), mainLoopStarted(2) {}

  ctk::ScalarPushInput<int32_t> trigger{this, "trigger", "", "I wait for this to start."};
  ctk::ScalarPollInput<int32_t> readback{this, "readback", "", "Just going to read something."};

  void mainLoop() override {
    mainLoopStarted.wait();

    trigger.read();
    readback.read();
    // I am not doing anything with the read values, but still a useful test (we do not get here anyway)
  }

  // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
  // mainLoop(). This is required to make sure the initial value propagation is done.
  // execute this right after the Application::run():
  //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
  boost::barrier mainLoopStarted;
};

struct RealisticModule : public ctk::ApplicationModule {
  RealisticModule(EntityOwner* owner, const std::string& name, const std::string& description,
      ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
      const std::unordered_set<std::string>& tags = {})
  : ApplicationModule(owner, name, description, hierarchyModifier, tags), mainLoopStarted(2) {}

  ctk::ScalarPushInput<int32_t> reg1{this, "REG1", "", "misused as input"};
  ctk::ScalarPollInput<int32_t> reg2{this, "REG2", "", "also no input..."};
  ctk::ScalarOutput<int32_t> reg3{this, "REG3", "", "my output"};

  void mainLoop() override {
    mainLoopStarted.wait();

    reg1.read();
    reg2.readLatest();

    reg3 = reg1 * reg2;
    reg3.write();
  }

  // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
  // mainLoop(). This is required to make sure the initial value propagation is done.
  // execute this right after the Application::run():
  //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
  boost::barrier mainLoopStarted;
};

// A more compicated scenario with module that have blocking reads and writes, fans that connect to the device and the CS, and direct connection device/CS only without fans.
struct TestApplication2 : public ctk::Application {
  TestApplication2() : Application("testSuite") {}
  ~TestApplication2() { shutdown(); }

  void defineConnections() {
    // let's do some manual cabling here....
    // A module that is only writin to a device such that no fan is involved
    cs("triggerActuator") >> outputModule("trigger");
    outputModule("actuator") >> dev1["MyModule"]("actuator");

    cs("triggerReadback") >> inputModule("trigger");
    dev1["MyModule"]("readBack") >> inputModule("readback");

    dev2.connectTo(cs["Device2"], cs("trigger2", typeid(int), 1));

    // the most realistic part: everything cabled everywhere with fans
    // first cable the module to the device. This determines the direction of the variables
    // FIXME: This does not work, don't know why.
    //dev3["MODULE"]("REG1")[ cs("triggerRealistic",typeid(int), 1) ] >> realisticModule("REG1");

    // This is not what I wanted. I wanted a triggered network for Reg1 and Reg2
    realisticModule("REG3") >> dev3["MODULE"]("REG3"); // for the direction
    dev3.connectTo(cs["Device3"], cs("triggerRealistic", typeid(int), 1));
    realisticModule.connectTo(cs["Device3"]["MODULE"]);
  }

  OutputModule outputModule{this, "outputModule", "The output module"};
  InputModule inputModule{this, "inputModule", "The input module"};

  RealisticModule realisticModule{this, "realisticModule", "The most realistic module"};

  ctk::DeviceModule dev1{this, ExceptionDummyCDD1};
  ctk::DeviceModule dev2{this, ExceptionDummyCDD2};
  ctk::DeviceModule dev3{this, ExceptionDummyCDD3};
  ctk::ControlSystemModule cs;
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExceptionHandlingRead) {
  std::cout << "testExceptionHandlingRead" << std::endl;
  TestApplication app;
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend2 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD2));

  ctk::Device dev1(ExceptionDummyCDD1);
  ctk::Device dev2(ExceptionDummyCDD2);

  // Connect the whole devices into the control system, and use the control system variable /trigger as trigger for
  // both devices. The variable becomes a control system to application variable and writing to it through the test
  // facility is generating the triggers.
  app.dev1.connectTo(app.cs["Device1"], app.cs("trigger", typeid(int), 1));
  app.dev2.connectTo(app.cs["Device2"], app.cs("trigger"));

  // Do not enable testable mode. The testable mode would block in the wrong place, as the trigger for reading variables
  // of a device in the error state is not being processed until the error state is cleared. We want to test that the
  // second device still works while the first device is in error state, which would be impossible with testable mode
  // enabled. As a consequence, our test logic has to work with timeouts (CHECK_TIMEOUT) etc. instead of the
  // deterministic stepApplication().
  ctk::TestFacility test(false);
  test.runApplication();

  auto message1 = test.getScalar<std::string>(std::string("/Devices/") + ExceptionDummyCDD1 + "/message");
  auto status1 = test.getScalar<int>(std::string("/Devices/") + ExceptionDummyCDD1 + "/status");
  auto readback1 = test.getScalar<int>("/Device1/MyModule/readBack");
  auto message2 = test.getScalar<std::string>(std::string("/Devices/") + ExceptionDummyCDD2 + "/message");
  auto status2 = test.getScalar<int>(std::string("/Devices/") + ExceptionDummyCDD2 + "/status");
  auto readback2 = test.getScalar<int>("/Device2/MyModule/readBack");

  auto trigger = test.getScalar<int>("trigger");

  // we do not use testable mode, so we need to read the initial values at CS ourself where present
  readback1.read();
  readback2.read();

  dev1.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 42);
  dev2.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 52);
  int readback1_expected = 42;

  // initially, devices are not opened but errors should be cleared once they are opened
  trigger.write();

  do {
    message1.readLatest();
    status1.readLatest();
  } while(status1 != 0 || std::string(message1) != "");
  BOOST_CHECK(!message1.readLatest());
  BOOST_CHECK(!status1.readLatest());

  do {
    message2.readLatest();
    status2.readLatest();
  } while(status2 != 0 || std::string(message2) != "");
  BOOST_CHECK(!message2.readLatest());
  BOOST_CHECK(!status2.readLatest());

  CHECK_TIMEOUT(readback1.readLatest(), 10000);
  CHECK_TIMEOUT(readback2.readLatest(), 10000);
  BOOST_CHECK_EQUAL(readback1, readback1_expected);
  BOOST_CHECK_EQUAL(readback2, 52);

  // repeat test a couple of times to make sure it works not only once
  for(int i = 0; i < 3; ++i) {
    // enable exception throwing in test device 1
    dev1.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 10 + i);
    dev2.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 20 + i);
    dummyBackend1->throwExceptionRead = true;
    trigger.write();
    CHECK_TIMEOUT(message1.readLatest(), 10000);
    CHECK_TIMEOUT(status1.readLatest(), 10000);
    BOOST_CHECK(static_cast<std::string>(message1) != "");
    BOOST_CHECK_EQUAL(status1, 1);
    CHECK_TIMEOUT(readback1.readNonBlocking(), 10000);                        // we have been signalized new data
    BOOST_CHECK(readback1.dataValidity() == ChimeraTK::DataValidity::faulty); // But the fault flag should be set
    // the second device must still be functional
    BOOST_CHECK(!message2.readNonBlocking());
    BOOST_CHECK(!status2.readNonBlocking());
    CHECK_TIMEOUT(readback2.readNonBlocking(), 10000); // device 2 still works
    BOOST_CHECK_EQUAL(readback2, 20 + i);

    // even with device 1 failing the trigger produces "new" data: The time stamp changes, but the data content does not, and it is flagged as invalid
    // device 2 is working normaly
    dev2.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 120 + i);
    trigger.write();
    readback1.read();
    BOOST_CHECK_EQUAL(readback1, readback1_expected);                         // The value has not changed
    BOOST_CHECK(readback1.dataValidity() == ChimeraTK::DataValidity::faulty); // But the fault flag should still be set
    CHECK_TIMEOUT(readback2.readNonBlocking(), 10000);                        // device 2 still works
    BOOST_CHECK_EQUAL(readback2, 120 + i);

    // Now "cure" the device problem
    dummyBackend1->throwExceptionRead = false;
    // we have to wait until the device has recovered. Otherwise the writing will throw.
    CHECK_TIMEOUT(status1.readLatest(), 10000);
    BOOST_CHECK_EQUAL(status1, 0);

    dev1.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 30 + i);
    dev2.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 40 + i);
    trigger.write();
    message1.read();
    readback1.read();
    BOOST_CHECK_EQUAL(static_cast<std::string>(message1), "");
    BOOST_CHECK_EQUAL(readback1,
        30 + i); // the "20+i" is never seen because there was a new vaulue before the next trigger after the recovery
    readback1_expected = 30 + i; // remember the last good value for the next iteration
    BOOST_CHECK(readback1.dataValidity() == ChimeraTK::DataValidity::ok); // The fault flag should have been cleared
    // device2
    BOOST_CHECK(!message2.readNonBlocking());
    BOOST_CHECK(!status2.readNonBlocking());
    CHECK_TIMEOUT(readback2.readNonBlocking(), 10000); // device 2 still works
    BOOST_CHECK_EQUAL(readback2, 40 + i);
  }
  // FIXME: This test only works for poll-type variables. We also have to test with push type.
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExceptionHandlingWrite) {
  std::cout << "testExceptionHandlingWrite" << std::endl;
  TestApplication app;
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend2 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD2));

  ctk::Device dev1(ExceptionDummyCDD1);
  ctk::Device dev2(ExceptionDummyCDD2);

  // Connect the whole devices into the control system, and use the control system variable /trigger as trigger for
  // both devices. The variable becomes a control system to application variable and writing to it through the test
  // facility is generating the triggers.
  app.dev1.connectTo(app.cs["Device1"], app.cs("trigger", typeid(int), 1));
  app.dev2.connectTo(app.cs["Device2"], app.cs("trigger"));

  // Do not enable testable mode. The testable mode would block in the wrong place, as the trigger for reading variables
  // of a device in the error state is not being processed until the error state is cleared. We want to test that the
  // second device still works while the first device is in error state, which would be impossible with testable mode
  // enabled. As a consequence, our test logic has to work with timeouts (CHECK_TIMEOUT) etc. instead of the
  // deterministic stepApplication().
  ctk::TestFacility test(false);
  test.runApplication();

  auto message1 = test.getScalar<std::string>(std::string("/Devices/") + ExceptionDummyCDD1 + "/message");
  auto status1 = test.getScalar<int>(std::string("/Devices/") + ExceptionDummyCDD1 + "/status");
  auto actuator1 = test.getScalar<int>("/Device1/MyModule/actuator");
  auto message2 = test.getScalar<std::string>(std::string("/Devices/") + ExceptionDummyCDD2 + "/message");
  auto status2 = test.getScalar<int>(std::string("/Devices/") + ExceptionDummyCDD2 + "/status");
  auto actuator2 = test.getScalar<int>("/Device2/MyModule/actuator");

  auto trigger = test.getScalar<int>("trigger");

  // initially, devices are not opened but errors should be cleared once they are opened
  do {
    message1.readLatest();
    status1.readLatest();
  } while(status1 != 0 || std::string(message1) != "");
  BOOST_CHECK(!message1.readLatest());
  BOOST_CHECK(!status1.readLatest());

  do {
    message2.readLatest();
    status2.readLatest();
  } while(status2 != 0 || std::string(message2) != "");
  BOOST_CHECK(!message2.readLatest());
  BOOST_CHECK(!status2.readLatest());

  actuator1 = 29;
  actuator1.write();
  actuator2 = 39;
  actuator2.write();
  BOOST_CHECK(!message1.readLatest());
  BOOST_CHECK(!status1.readLatest());
  CHECK_TIMEOUT(dev1.read<int>("MyModule/actuator") == 29, 10000);
  CHECK_TIMEOUT(dev2.read<int>("MyModule/actuator") == 39, 10000);
  BOOST_CHECK(static_cast<std::string>(message1) == "");
  BOOST_CHECK(status1 == 0);

  // repeat test a couple of times to make sure it works not only once
  for(int i = 0; i < 3; ++i) {
    // enable exception throwing in test device 1
    dummyBackend1->throwExceptionWrite = true;
    actuator1 = 30 + i;
    actuator1.write();
    actuator2 = 40 + i;
    actuator2.write();
    CHECK_TIMEOUT(message1.readLatest(), 10000);
    CHECK_TIMEOUT(status1.readLatest(), 10000);
    BOOST_CHECK(static_cast<std::string>(message1) != "");
    BOOST_CHECK_EQUAL(status1, 1);
    usleep(10000); // 10ms wait time so potential wrong values could have propagated
    // while the device is broken none of its accessors work. We have to use the dummy backend directly to look into its data buffer.
    auto actuatorDummyRaw = dummyBackend1->getRawAccessor("MyModule", "actuator");
    {
      auto bufferLock = actuatorDummyRaw.getBufferLock();
      BOOST_CHECK(actuatorDummyRaw == int(30 + i - 1)); // write not done for broken device
    }
    // the second device must still be functional
    BOOST_CHECK(!message2.readNonBlocking());
    BOOST_CHECK(!status2.readNonBlocking());
    CHECK_TIMEOUT(dev2.read<int>("MyModule/actuator") == int(40 + i), 10000); // device 2 still works

    // even with device 1 failing the second one must process the data, so send a new data before fixing dev1
    actuator2 = 120 + i;
    actuator2.write();
    CHECK_TIMEOUT(dev2.read<int>("MyModule/actuator") == int(120 + i), 10000); // device 2 still works
    {
      auto bufferLock = actuatorDummyRaw.getBufferLock();
      BOOST_CHECK(actuatorDummyRaw == int(30 + i - 1)); // device 1 is still broken and has not seen the new value yet
    }

    // Now "cure" the device problem
    dummyBackend1->throwExceptionWrite = false;
    CHECK_TIMEOUT(message1.readLatest(), 10000);
    CHECK_TIMEOUT(status1.readLatest(), 10000);
    CHECK_TIMEOUT(dev1.read<int>("MyModule/actuator") == int(30 + i), 10000); // write is now complete
    BOOST_CHECK_EQUAL(static_cast<std::string>(message1), "");
    BOOST_CHECK_EQUAL(status1, 0);
  }
}
/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExceptionHandlingOpen) {
  std::cout << "testExceptionHandlingOpen" << std::endl;
  TestApplication app;
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend2 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD2));

  ctk::Device dev1(ExceptionDummyCDD1);
  ctk::Device dev2(ExceptionDummyCDD2);
  dev1.open();
  dev2.open();
  dev1.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 100);
  dev2.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 110);
  dev1.close();
  dev2.close();

  // Connect the whole devices into the control system, and use the control system variable /trigger as trigger for
  // both devices. The variable becomes a control system to application variable and writing to it through the test
  // facility is generating the triggers.
  app.dev1.connectTo(app.cs["Device1"], app.cs("trigger", typeid(int), 1));
  app.dev2.connectTo(app.cs["Device2"], app.cs("trigger"));

  // Do not enable testable mode. The testable mode would block in the wrong place, as the trigger for reading variables
  // of a device in the error state is not being processed until the error state is cleared. We want to test that the
  // second device still works while the first device is in error state, which would be impossible with testable mode
  // enabled. As a consequence, our test logic has to work with timeouts (CHECK_TIMEOUT) etc. instead of the
  // deterministic stepApplication().
  ctk::TestFacility test(false);
  dummyBackend1->throwExceptionOpen = true;
  app.run(); // don't use TestFacility::runApplication() here as it blocks until all devices are open...

  auto message1 = test.getScalar<std::string>(std::string("/Devices/") + ExceptionDummyCDD1 + "/message");
  auto status1 = test.getScalar<int>(std::string("/Devices/") + ExceptionDummyCDD1 + "/status");
  auto readback1 = test.getScalar<int>("/Device1/MyModule/readBack");
  auto message2 = test.getScalar<std::string>(std::string("/Devices/") + ExceptionDummyCDD2 + "/message");
  auto status2 = test.getScalar<int>(std::string("/Devices/") + ExceptionDummyCDD2 + "/status");
  auto readback2 = test.getScalar<int>("/Device2/MyModule/readBack");

  auto trigger = test.getScalar<int>("trigger");

  trigger.write();
  //device 1 is in Error state
  CHECK_TIMEOUT(message1.readLatest(), 10000);
  CHECK_TIMEOUT(status1.readLatest(), 10000);
  BOOST_CHECK_EQUAL(status1, 1);
  BOOST_CHECK(!readback1.readNonBlocking()); // error state at the beginning is not yet propagated

  // Device 2 might/will also come up in error state until the device is opened (which happends asynchronously in a
  // separate thread).
  /// So we have to read until we get a DataValidity::ok
  CHECK_TIMEOUT((readback2.readNonBlocking(), readback2.dataValidity() == ctk::DataValidity::ok), 10000);
  BOOST_CHECK_EQUAL(readback2, 110);

  // even with device 1 failing the second one must process the data, so send a new trigger
  // before fixing dev1
  dev2.write<int>("MyModule/readBack.DUMMY_WRITEABLE", 120);
  trigger.write();
  CHECK_TIMEOUT(readback2.readNonBlocking(), 10000); // device 2 still works
  BOOST_CHECK_EQUAL(readback2, 120);
  //Device is not in error state.
  CHECK_TIMEOUT(!message2.readLatest(), 10000);
  CHECK_TIMEOUT(!status2.readLatest(), 10000);

  //fix device 1
  dummyBackend1->throwExceptionOpen = false;
  //device 1 is fixed
  CHECK_TIMEOUT(message1.readLatest(), 10000);
  CHECK_TIMEOUT(status1.readLatest(), 10000);
  BOOST_CHECK_EQUAL(status1, 0);
  CHECK_TIMEOUT(readback1.readNonBlocking(), 10000);
  BOOST_CHECK_EQUAL(readback1, 100);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testConstants) {
  std::cout << "testConstants" << std::endl;
  // Constants are registered to the device to be written when opening/recovering
  // Attention: This test does not test that errors when writing to constants are displayed correctly. It only checks that witing when opeing and recovering works.
  TestApplication app;
  ctk::VariableNetworkNode::makeConstant<int32_t>(true, 18) >> app.dev1("/MyModule/actuator");
  app.cs("PleaseWriteToMe", typeid(int), 1) >> app.dev1("/Integers/signed32", typeid(int), 1);

  ctk::TestFacility test;
  test.runApplication();

  ChimeraTK::Device dev;
  dev.open(ExceptionDummyCDD1);

  // after opening a device the runApplication() might return, but the initialisation might not have happened in the other thread yet. So check with timeout.
  CHECK_TIMEOUT(dev.read<int32_t>("/MyModule/actuator") == 18, 10000);

  // So far this is also tested by testDeviceAccessors. Now cause errors.
  // Take back the value of the constant which was written to the device before making the device fail for further writes.
  dev.write<int32_t>("/MyModule/actuator", 0);
  auto dummyBackend = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ctk::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));
  dummyBackend->throwExceptionWrite = true;

  auto pleaseWriteToMe = test.getScalar<int32_t>("/PleaseWriteToMe");
  pleaseWriteToMe = 42;
  pleaseWriteToMe.write();
  test.stepApplication(false);

  // Check that the error has been seen
  auto deviceStatus = test.getScalar<int32_t>(std::string("/Devices/") + ExceptionDummyCDD1 + "/status");
  deviceStatus.readLatest();
  BOOST_CHECK(deviceStatus == 1);

  // now cure the error
  dummyBackend->throwExceptionWrite = false;

  // Write something so we can call stepApplication to wake up the app.
  pleaseWriteToMe = 43;
  pleaseWriteToMe.write();
  test.stepApplication();

  CHECK_TIMEOUT(dev.read<int32_t>("/MyModule/actuator") == 18, 10000);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testShutdown) {
  std::cout << "testShutdown" << std::endl;
  static const uint32_t DEFAULT = 55;

  auto dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ctk::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));
  auto dummyBackend2 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ctk::BackendFactory::getInstance().createBackend(ExceptionDummyCDD2));
  auto dummyBackend3 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ctk::BackendFactory::getInstance().createBackend(ExceptionDummyCDD3));

  // Test that the application does shut down with a broken device and blocking accessors
  TestApplication2 app;
  ctk::TestFacility test(false); // test facility without testable mode

  ctk::Device dev2(ExceptionDummyCDD2);
  ctk::Device dev3(ExceptionDummyCDD3);

  // Non zero defaults set here to avoid race conditions documented in
  // https://github.com/ChimeraTK/ApplicationCore/issues/103
  test.setScalarDefault("/Device2/MyModule/actuator", static_cast<int32_t>(DEFAULT));
  test.setScalarDefault("/Device2/Integers/signed32", static_cast<int32_t>(DEFAULT));
  test.setScalarDefault("/Device2/Integers/unsigned32", static_cast<uint32_t>(DEFAULT));
  test.setScalarDefault("/Device2/Integers/signed16", static_cast<int16_t>(DEFAULT));
  test.setScalarDefault("/Device2/Integers/unsigned16", static_cast<uint16_t>(DEFAULT));
  test.setScalarDefault("/Device2/Integers/signed8", static_cast<int8_t>(DEFAULT));
  test.setScalarDefault("/Device2/Integers/unsigned8", static_cast<uint8_t>(DEFAULT));
  test.setScalarDefault("/Device2/FixedPoint/value", static_cast<float>(DEFAULT));
  test.setScalarDefault("/Device2/Deep/Hierarchies/Need/Tests/As/well", static_cast<int32_t>(DEFAULT));
  test.setScalarDefault("/Device2/Deep/Hierarchies/Need/Another/test", static_cast<int32_t>(DEFAULT));
  test.setScalarDefault("/Device3/MODULE/REG4", static_cast<int32_t>(DEFAULT));

  test.runApplication();
  app.inputModule.mainLoopStarted.wait();
  app.outputModule.mainLoopStarted.wait();
  app.realisticModule.mainLoopStarted.wait();

  // verify defaults have been written to the device
  CHECK_TIMEOUT(dev2.read<int32_t>("MyModule/actuator") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<int32_t>("Integers/signed32") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<uint32_t>("Integers/unsigned32") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<int16_t>("Integers/signed16") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<uint16_t>("Integers/unsigned16") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<int8_t>("Integers/signed8") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<uint8_t>("Integers/unsigned8") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<int32_t>("Deep/Hierarchies/Need/Tests/As/well") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev2.read<int32_t>("Deep/Hierarchies/Need/Another/test") == DEFAULT, 10000);
  CHECK_TIMEOUT(dev3.read<int32_t>("MODULE/REG4") == DEFAULT, 10000);

  // Wait for the devices to come up.
  CHECK_EQUAL_TIMEOUT(
      test.readScalar<int32_t>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD1 / "status"), 0, 10000);
  CHECK_EQUAL_TIMEOUT(
      test.readScalar<int32_t>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD2 / "status"), 0, 10000);
  CHECK_EQUAL_TIMEOUT(
      test.readScalar<int32_t>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD3 / "status"), 0, 10000);

  // make all devices fail, and wait until they report the error state, one after another
  dummyBackend2->throwExceptionWrite = true;
  dummyBackend2->throwExceptionRead = true;

  // two blocking accessors on dev3: one for reading, one for writing
  auto trigger2 = test.getScalar<int32_t>("/trigger2");
  trigger2.write(); // triggers the read of readBack

  // wait for the error to be reported in the control system
  CHECK_EQUAL_TIMEOUT(
      test.readScalar<int32_t>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD2 / "status"), 1, 10000);
  BOOST_CHECK(test.readScalar<std::string>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD2 / "message") != "");

  auto theInt = test.getScalar<int32_t>("/Device2/Integers/signed32");
  theInt.write();
  BOOST_CHECK(test.readScalar<std::string>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD2 / "message") != "");
  // the read is the first error we see. The second one is not reported any more for this device.

  // device 2 successfully broken!

  // block the output accessor of "outputModule
  dummyBackend1->throwExceptionWrite = true;
  dummyBackend1->throwExceptionRead = true;

  auto triggerActuator = test.getScalar<int32_t>("/triggerActuator");
  triggerActuator.write();

  // wait for the error to be reported in the control system
  CHECK_EQUAL_TIMEOUT(
      test.readScalar<int32_t>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD1 / "status"), 1, 10000);
  // the write message does not have a \n, it is not going though a feeding fanout
  BOOST_CHECK(test.readScalar<std::string>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD1 / "message") != "");

  auto triggerReadback = test.getScalar<int32_t>("/triggerReadback");
  triggerReadback.write();

  // device 1 successfully broken!

  dummyBackend3->throwExceptionWrite =
      false; // do not set to true, otherwise there is a race condition whether the read or the write in RealisticModule::mainLoop() triggers the exception
  dummyBackend3->throwExceptionRead = true;

  auto triggerRealistic = test.getScalar<int32_t>("/triggerRealistic");
  triggerRealistic.write();

  CHECK_EQUAL_TIMEOUT(
      test.readScalar<int32_t>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD3 / "status"), 1, 10000);
  BOOST_CHECK(test.readScalar<std::string>(ctk::RegisterPath("/Devices") / ExceptionDummyCDD3 / "message") != "");

  auto reg4 = test.getScalar<int32_t>("/Device3/MODULE/REG4");
  reg4.write();

  // device 3 successfully broken!

  // I now blocked everything that comes to my mind.
  // And now the real test: does the test end or does it block when shuttig down?
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
