// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testExceptionHandling

#include "check_timeout.h"
#include "DeviceModule.h"
#include "fixtures.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/DummyRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/NDRegisterAccessor.h>

#include <boost/mpl/list.hpp>

#include <chrono>
#include <cstring>
#include <future>

// this #include must come last
#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;
using Fixture = fixture_with_poll_and_push_input<false>;
using Fixture_initHandlers = fixture_with_poll_and_push_input<false, true>;
using Fixture_secondDeviceBroken = fixture_with_poll_and_push_input<false, false, true>;

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
  // message -> /Devices/(ExceptionDummy:1?map=test.map)/status_message

  BOOST_CHECK_EQUAL(status, 0);
  BOOST_CHECK_EQUAL(static_cast<std::string>(message), "");

  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  application.group1.pollModule.pollInput.read(); // causes device exception

  CHECK_TIMEOUT(status.readNonBlocking() == true, 10000);
  CHECK_TIMEOUT(message.readNonBlocking() == true, 10000);
  BOOST_CHECK_EQUAL(status, 1);
  BOOST_CHECK(!static_cast<std::string>(message).empty());

  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionOpen = false;

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
 *
 * TODO: Set previous fault flag through Backend, and test inside TriggerFanOut (the latter needs the first)
 */

BOOST_FIXTURE_TEST_CASE(B_2_2_2_poll, Fixture) {
  std::cout << "B_2_2_2_poll - exception with previous DataValidity::faulty" << std::endl;

  // initialize to known value in deviceBackend register
  write(exceptionDummyRegister, 134);
  pollVariable.read();
  BOOST_REQUIRE_EQUAL(pollVariable, 134);
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
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 10);

  // This read should be skipped but obtain a new version number
  pollVariable.read();
  auto versionNumberOnRuntimeError = pollVariable.getVersionNumber();
  BOOST_CHECK_EQUAL(pollVariable, 134);
  BOOST_CHECK(pollVariable.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(versionNumberOnRuntimeError > versionNumberBeforeRuntimeError);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_2_push \ref exceptionHandling_b_2_2_2 "B.2.2.2"
 *
 * "The DataValidity::faulty flag resulting from the fault state is propagated once, even if the variable had the a
 * DataValidity::faulty flag already set previously for another reason."
 *
 * TODO: Set previous fault flag through Backend, and test inside ThreadedFanOut and TriggerFanOut (as trigger).
 */

BOOST_FIXTURE_TEST_CASE(B_2_2_2_push, Fixture) {
  std::cout << "B_2_2_2_push - exception with previous DataValidity::faulty" << std::endl;

  // verify normal operation
  // initialize to known value in deviceBackend register
  write(exceptionDummyRegister, 101);
  ctk::VersionNumber versionNumberBeforeRuntimeError = {};
  deviceBackend->triggerInterrupt(1, 0);
  pushVariable.read();

  // Modify the validity flag of the application buffer (see note above in poll-type test)
  pushVariable.setDataValidity(ctk::DataValidity::faulty);
  for(auto& e : pushVariable.getHardwareAccessingElements()) {
    e->setDataValidity(ctk::DataValidity::faulty);
  }

  // modify value in register after breaking the device
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 11);
  deviceBackend->triggerInterrupt(1, 0);

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
 *
 * Test directly inside ApplicationModule
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_3, Fixture) {
  std::cout << "B_2_2_3 - skip poll type reads" << std::endl;

  // initialize to known value in deviceBackend register
  write(exceptionDummyRegister, 100);
  pollVariable.read();
  auto versionNumberBeforeRuntimeError = pollVariable.getVersionNumber();

  // modify value in register after breaking the device
  deviceBackend->throwExceptionOpen = true;
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
 * \anchor testExceptionHandling_b_2_2_3_TrFO \ref exceptionHandling_b_2_2_3 "B.2.2.3"
 *
 * "Read operations without AccessMode::wait_for_new_data are skipped until the device is fully recovered again (cf.
 * 3.1). The first skipped read operation will have a new VersionNumber."
 *
 * Test inside a TriggerFanOut. This is mainly necessary to make sure the ExceptionHandlingDecorator is used for
 * variables inside the TriggerFanOut.
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_3_TriggerFanOut, Fixture) {
  std::cout << "B_2_2_3_TriggerFanOut - skip poll type reads (in TriggerFanOut)" << std::endl;

  // initialize to known value in deviceBackend register
  triggeredInput.readLatest(); // empty queue (initial value)

  write(exceptionDummy2Register, 666);
  deviceBackend3->triggerInterrupt(1, 0);
  triggeredInput.read();
  BOOST_REQUIRE_EQUAL(triggeredInput, 666);

  // breaking the device and modify value
  deviceBackend2->throwExceptionOpen = true;
  deviceBackend2->throwExceptionRead = true;
  write(exceptionDummy2Register, 667);

  // Trigger readout of poll-type inside TriggerFanOut (should be skipped - VersionNumber is invisible in this context)
  deviceBackend3->triggerInterrupt(1, 0);
  triggeredInput.read();
  BOOST_CHECK_EQUAL(triggeredInput, 666);
  BOOST_CHECK(triggeredInput.dataValidity() == ctk::DataValidity::faulty);

  // A second read should be skipped, too
  deviceBackend3->triggerInterrupt(1, 0);
  triggeredInput.read();
  BOOST_CHECK_EQUAL(triggeredInput, 666);
  BOOST_CHECK(triggeredInput.dataValidity() == ctk::DataValidity::faulty);
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

  application.getModel().writeGraphViz("FixtureDummyApp.dot");

  pushVariable.readLatest();

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 456);
  deviceBackend->triggerInterrupt(1, 0);

  // as soon as the fault state has arrived, the operation is skipped
  pushVariable.read();
  BOOST_CHECK_NE(pushVariable, 456); // value did not come through
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

  pushVariable.readLatest();

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 123);
  deviceBackend->triggerInterrupt(1, 0);

  // as soon as the fault state has arrived, the operation is skipped
  CHECK_TIMEOUT(pushVariable.readNonBlocking() == true, 10000);
  BOOST_CHECK_NE(pushVariable, 123); // value did not come through
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

  pushVariable.readLatest();

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 234);
  deviceBackend->triggerInterrupt(1, 0);

  // as soon as the fault state has arrived, the operation is skipped
  CHECK_TIMEOUT(pushVariable.readLatest() == true, 10000);
  BOOST_CHECK_NE(pushVariable, 234); // value did not come through
  BOOST_CHECK(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  auto versionNumberOnRuntimeError = pushVariable.getVersionNumber();
  BOOST_CHECK(versionNumberOnRuntimeError > version);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_ThFO \ref exceptionHandling_b_2_2_4 "B.2.2.4"
 *
 * "Read operations with AccessMode::wait_for_new_data will be skipped once for each accessor to propagate the
 * DataValidity::faulty flag (which counts as new data, i.e. readNonBlocking()/readLatest() will return true
 * (= hasNewData), and a new VersionNumber is obtained)."
 *
 * This test is for read() inside a ThreadedFanOut. (The ThreadedFanOut never calles the other read functions.)
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_ThFO, Fixture) {
  std::cout << "B_2_2_4_ThFO - first skip read in ThreadedFanOut" << std::endl;

  // remove initial value from control system
  pushVariable3copy.readLatest();
  pushVariable3.readLatest();

  // go to exception state
  ctk::VersionNumber version = {};
  deviceBackend2->throwExceptionOpen = true;
  deviceBackend2->throwExceptionRead = true;
  write(exceptionDummy2Register, 345);
  deviceBackend2->triggerInterrupt(1, 0);

  // as soon as the fault state has arrived, the operation is skipped
  pushVariable3.read();
  BOOST_CHECK_NE(pushVariable3, 345); // value did not come through
  BOOST_CHECK(pushVariable3.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(pushVariable3.getVersionNumber() > version);

  // same state is visible at control system's copy
  pushVariable3copy.read();
  BOOST_CHECK_NE(pushVariable3copy, 345); // value did not come through
  BOOST_CHECK(pushVariable3copy.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(pushVariable3copy.getVersionNumber() > version);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_2_4_TrFO \ref exceptionHandling_b_2_2_4 "B.2.2.4"
 *
 * "Read operations with AccessMode::wait_for_new_data will be skipped once for each accessor to propagate the
 * DataValidity::faulty flag (which counts as new data, i.e. readNonBlocking()/readLatest() will return true
 * (= hasNewData), and a new VersionNumber is obtained)."
 *
 * This test is for read() inside a TriggerFanOut on the trigger variable.
 */
BOOST_FIXTURE_TEST_CASE(B_2_2_4_TrFO, Fixture) {
  std::cout << "B_2_2_4_TrFO - first skip read in TriggerFanOut on the trigger variable" << std::endl;

  triggeredInput.readLatest();

  // initialize to known value in deviceBackend register
  write(exceptionDummy2Register, 668);
  ctk::VersionNumber versionBeforeException = {};
  deviceBackend3->triggerInterrupt(1, 0);
  triggeredInput.read();

  // breaking the device and modify value
  deviceBackend3->throwExceptionOpen = true;
  deviceBackend3->throwExceptionRead = true;
  write(exceptionDummy2Register, 669);
  pollVariable3.read(); // make sure framework sees exception

  // as soon as the fault state has arrived, the operation is skipped (inside the TriggerFanOut), so we get the
  // updated value (remember: the updated value comes from another device which is not broken)
  triggeredInput.read();
  BOOST_CHECK_EQUAL(triggeredInput, 669);
  BOOST_CHECK(triggeredInput.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(triggeredInput.getVersionNumber() > versionBeforeException);
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

  pushVariable.readLatest();

  // go to exception state
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerInterrupt(1, 0);

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

  pushVariable.readLatest();

  // go to exception state
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerInterrupt(1, 0);

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

  pushVariable.readLatest();

  // go to exception state
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 100);
  deviceBackend->triggerInterrupt(1, 0);

  // perform first skipped operation
  pushVariable.read();

  // subsequent read operations should be frozen
  deviceBackend->triggerInterrupt(1, 0);
  auto f = std::async(std::launch::async, [&]() { pushVariable.read(); });
  BOOST_CHECK(f.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);

  // FIXME: This should not be necessary. Bug in ApplicationCore's shutdown procedure!?
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionOpen = false;
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

  pushVariable.readLatest();

  // Normal behaviour
  write(exceptionDummyRegister, 66);
  deviceBackend->triggerInterrupt(1, 0);
  pushVariable.read();

  // Change value while in exception state
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  write(exceptionDummyRegister, 77);
  deviceBackend->triggerInterrupt(1, 0);

  pushVariable.read();
  BOOST_CHECK_EQUAL(pushVariable, 66);

  // Recover from exception state
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionOpen = false;

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

  pushVariable.readLatest();

  // Go to exception state, report it explicitly
  ctk::VersionNumber someVersionBeforeReporting = {};
  deviceBackend->throwExceptionOpen = true; // required to make sure device stays down
  application.group1.device.reportException("explicit report by test");
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

  pushVariable.readLatest();

  // Write both variables once (without error state)
  write(exceptionDummyRegister, 66);
  deviceBackend->triggerInterrupt(1, 0);
  pushVariable.read();
  BOOST_CHECK_EQUAL(pushVariable, 66);

  write(exceptionDummyRegister, 67);
  pollVariable.read();
  BOOST_CHECK_EQUAL(pollVariable, 67);

  // Go to exception state, report it explicitly
  write(exceptionDummyRegister, 68);
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // Check push variable
  pushVariable = 42;
  BOOST_REQUIRE(pushVariable.dataValidity() == ctk::DataValidity::ok);
  pushVariable.read();
  BOOST_REQUIRE(pushVariable.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(pushVariable, 42);

  // Check poll variable
  pollVariable = 43;
  pollVariable.read();
  BOOST_CHECK_EQUAL(pollVariable, 43);
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
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // multiple writes on faulty device.
  outputVariable2 = 100;
  auto testval = outputVariable2.write();
  BOOST_CHECK_EQUAL(testval, false); // data not lost

  outputVariable2 = 101;
  BOOST_CHECK_EQUAL(outputVariable2.write(), true); // data lost
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_3_5 \ref exceptionHandling_b_2_3_5 "B.2.3.5"
 *
 * "It is guaranteed that the write takes place before the device is considered fully recovered again and other
 * transfers are allowed (cf. 3.1)."
 */
BOOST_FIXTURE_TEST_CASE(B_2_3_5, Fixture) {
  std::cout << "B_2_3_5 - write before deviceBecameFunctional" << std::endl;

  // trigger runtime error
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // write on faulty device.
  outputVariable2 = 987;
  outputVariable2.write();

  // recover device
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionOpen = false;
  deviceBecameFunctional.read();

  // check result (must be immediately present, so don't use CHECK_EQUAL_TIMEOUT!)
  BOOST_CHECK_EQUAL(exceptionDummyRegister2[0], 987);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_2_5 \ref exceptionHandling_b_2_5 "B.2.5"
 *
 * "TransferElement::isReadable(), TransferElement::isWriteable() and TransferElement::isReadonly() return with values
 * as if reading and writing would be allowed."
 */
BOOST_FIXTURE_TEST_CASE(B_2_5, Fixture) {
  std::cout << "B_2_5 - isReadable/isWriteable/isReadOnly" << std::endl;

  // trigger runtime error
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // Note: only test what is not anyway clear by the abstractor type. The others need to be implemented by the
  // abstractor directly.
  BOOST_CHECK(pollVariable.isReadable());

  BOOST_CHECK(pushVariable.isReadable());

  BOOST_CHECK(outputVariable2.isWriteable());
  BOOST_CHECK(!outputVariable2.isReadOnly());
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_3_1_1 \ref exceptionHandling_b_3_1_1 "B.3.1.1"
 *
 * [The recovery procedure involves] "the execution of so-called initialisation handlers (see 3.2)."
 *
 * \anchor testExceptionHandling_b_3_2 \ref exceptionHandling_b_3_2 "B.3.2"
 *
 * "Any number of initialisation handlers can be added to the DeviceModule in the user code. Initialisation handlers are
 * callback functions which will be executed when a device is opened for the first time and after a device recovers from
 * an exception, before any application-initiated transfers are executed (including delayed write transfers).
 * See DeviceModule::addInitialisationHandler()."
 */
BOOST_FIXTURE_TEST_CASE(B_3_1_1, Fixture_initHandlers) {
  std::cout << "B_3_1_1 - initialisation handlers" << std::endl;

  // device opened for first time
  BOOST_CHECK(initHandler1Called);
  BOOST_CHECK(initHandler2Called);
  initHandler1Called = false;
  initHandler2Called = false;

  // trigger runtime error
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // init handlers should not yet be called
  usleep(10000);
  BOOST_CHECK(!initHandler1Called);
  BOOST_CHECK(!initHandler2Called);

  // trigger recovery, but let first init handler throw
  initHandler1Throws = true;
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionOpen = false;

  // init handler 1 must be called eventually, but not init handler 2
  CHECK_TIMEOUT(initHandler1Called, 10000);
  usleep(10000);
  BOOST_CHECK(!initHandler2Called);

  // let the first init handler complete, but not the second one
  initHandler2Throws = true;
  initHandler1Called = false;
  initHandler1Throws = false;
  CHECK_TIMEOUT(initHandler1Called, 10000);
  CHECK_TIMEOUT(initHandler2Called, 10000);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_3_1_2 \ref exceptionHandling_b_3_1_2 "B.3.1.2"
 *
 * [After calling the initialisation handlers are called, the recovery procedure involves] "restoring all registers that
 * have been written since the start of the application with their latest values. The register values are restored in
 * the same order they were written. Registers of the type ChimeraTK::Void are not written."
 */
BOOST_FIXTURE_TEST_CASE(B_3_1_2, Fixture_initHandlers) {
  std::cout << "B_3_1_2 - delayed writes" << std::endl;

  // trigger runtime error
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();
  CHECK_TIMEOUT((status.readNonBlocking(), status == 1), 10000); // no test intended, just wait until error is reported

  // get current write count for each register (as a reference)
  auto wcReg2 = deviceBackend->getWriteCount("REG2");
  auto wcReg3 = deviceBackend->getWriteCount("REG3");
  auto wcRegV = deviceBackend->getWriteCount("REGV");

  // multiple writes to different registers on faulty device
  outputVariable2 = 801;
  outputVariable2.write();
  outputVariable3 = 802;
  outputVariable3.write();
  outputVariable2 = 803; // write a second time, overwriting the first value
  outputVariable2.write();
  outputVariableV.write(); // write the Void-typed register

  // check that values are not yet written to the device
  usleep(10000);
  BOOST_CHECK_NE(exceptionDummyRegister2[0], 803);
  BOOST_CHECK_NE(exceptionDummyRegister3[0], 802);

  // recover device for reading/opening but not yet for writing
  initHandler1Called = false;
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionWrite = true;
  deviceBackend->throwExceptionOpen = false;

  // wait until the write exception has been thrown
  deviceBackend->thereHaveBeenExceptions = false;
  CHECK_TIMEOUT(deviceBackend->thereHaveBeenExceptions, 10000);
  BOOST_CHECK_NE(exceptionDummyRegister2[0], 803);
  BOOST_CHECK_NE(exceptionDummyRegister3[0], 802);

  // check that write attempt has happened after initialisation handlers are called
  BOOST_CHECK(initHandler1Called);

  // now let write operations complete
  deviceBackend->throwExceptionWrite = false;

  // check that values finally are written to the device
  CHECK_EQUAL_TIMEOUT(exceptionDummyRegister2[0], 803, 10000);
  CHECK_EQUAL_TIMEOUT(exceptionDummyRegister3[0], 802, 10000);

  // check order of writes
  auto woReg2 = deviceBackend->getWriteOrder("REG2");
  auto woReg3 = deviceBackend->getWriteOrder("REG3");
  BOOST_CHECK_GT(woReg2, woReg3);

  // check each register is written only once ("only the latest written value [...] prevails"), except the Void register
  BOOST_CHECK_EQUAL(deviceBackend->getWriteCount("REG2") - wcReg2, 1);
  BOOST_CHECK_EQUAL(deviceBackend->getWriteCount("REG3") - wcReg3, 1);

  // The Void-typed register must have not been written.
  BOOST_CHECK_EQUAL(deviceBackend->getWriteCount("REGV"), wcRegV);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_3_1_3 \ref exceptionHandling_b_3_1_3 "B.3.1.3"
 *
 * [During recovery,] "the asynchronous read transfers of the device are (re-)activated by calling
 * Device::activateAsyncReads()" [after the delayed writes are executed.]
 */
BOOST_FIXTURE_TEST_CASE(B_3_1_3, Fixture_initHandlers) {
  std::cout << "B_3_1_3 - reactivate async reads" << std::endl;

  // Test async read after first open
  BOOST_CHECK(deviceBackend->asyncReadActivated());

  // Cause runtime error
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // Write to register to latest test order of recovery procedure
  outputVariable2.write();

  // Just to make sure the test is sensitive
  assert(deviceBackend->asyncReadActivated() == false);

  auto reg2WriteCountBeforeRecovery = deviceBackend->getWriteCount("REG2");

  // Recover from exception state
  initHandler1Called = false;
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionOpen = false;

  // Test async read after recovery
  CHECK_TIMEOUT(deviceBackend->asyncReadActivated(), 10000);
  BOOST_CHECK_EQUAL(deviceBackend->getWriteCount("REG2"), reg2WriteCountBeforeRecovery + 1);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_3_1_4 \ref exceptionHandling_b_3_1_4 "B.3.1.4"
 *
 * [As last part of the recovery,] "Devices/<alias>/deviceBecameFunctional is written to inform any module subscribing
 * to this variable about the finished recovery."
 */
BOOST_FIXTURE_TEST_CASE(B_3_1_4, Fixture_initHandlers) {
  std::cout << "B_3_1_4 - deviceBecameFunctional" << std::endl;

  // (Note: deviceBecameFunctional is read inside the fixture for the first time!)
  BOOST_CHECK(deviceBackend->asyncReadActivated());
  BOOST_CHECK(initHandler1Called);

  // Cause runtime error
  deviceBackend->throwExceptionOpen = true;
  deviceBackend->throwExceptionRead = true;
  pollVariable.read();

  // Make sure deviceBecameFunctional is not written at the wrong time
  usleep(10000);
  BOOST_CHECK(deviceBecameFunctional.readNonBlocking() == false);

  // Recover from exception state
  deviceBackend->throwExceptionRead = false;
  deviceBackend->throwExceptionOpen = false;

  // Check that deviceBecameFunctional is written after recovery
  CHECK_TIMEOUT(deviceBecameFunctional.readNonBlocking() == true, 10000);

  // Make sure deviceBecameFunctional is not written another time
  usleep(10000);
  BOOST_CHECK(deviceBecameFunctional.readNonBlocking() == false);
}

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_4_1 \ref exceptionHandling_b_4_1 "B.4.1"
 *
 * "Even if some devices are initially in a persisting error state, the part of the application which does not interact
 * with the faulty devices starts and works normally."
 */
BOOST_FIXTURE_TEST_CASE(B_4_1, Fixture_secondDeviceBroken) {
  std::cout << "B_4_1 - broken devices don't affect unrelated modules" << std::endl;

  pushVariable.readLatest();

  // verify the 3 ApplicationModules work
  write(exceptionDummyRegister, 101);
  deviceBackend->triggerInterrupt(1, 0);
  pushVariable.read();
  BOOST_CHECK_EQUAL(pushVariable, 101);

  write(exceptionDummyRegister, 102);
  pollVariable.read();
  BOOST_CHECK_EQUAL(pollVariable, 102);

  outputVariable2 = 103;
  outputVariable2.write();
  BOOST_CHECK_EQUAL(int(exceptionDummyRegister2), 103);

  // make sure test is effective (device2 is still in error condition)
  status2.readLatest();
  assert(status2 == 1);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
