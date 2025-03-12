// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testRecoveryGroups

#include "Application.h"
#include "check_timeout.h"
#include "DeviceManager.h"
#include "DeviceModule.h"
#include "ModuleGroup.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/cppext/finally.hpp>
#include <ChimeraTK/Exception.h>
#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/ScalarRegisterAccessor.h>
#include <ChimeraTK/VoidRegisterAccessor.h>

namespace ctk = ChimeraTK;

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/test/included/unit_test.hpp>

#include <barrier>
#include <cstdint>
#include <string>

// Helper class to have all variable names from a device prepended by the cdd/alias name
// e.g. /Integers/unsigned32 from Use1 ends up in /Use1/Integers/unsigned32
struct DeviceModuleWithPath : public ctk::ModuleGroup {
  DeviceModuleWithPath(ModuleGroup* owner, std::string const& cdd)
  : ModuleGroup(owner, cdd, ""), dev(this, cdd, "/somepath/dummyTrigger") {}
  ctk::DeviceModule dev;
};

// A test application with 4 devices in 2 recovery groups.
// It is used in most tests, and extended with initialisation handlers where needed.
struct BasicTestApp : ctk::Application {
  explicit BasicTestApp(const std::string& name = "BasicTestApp") : Application(name) {}
  ~BasicTestApp() override { shutdown(); }

  ctk::SetDMapFilePath path{"recoveryGroups.dmap"};

  // Recovery group: Two devices with one backend each, and a device which uses both of them
  DeviceModuleWithPath singleDev1{this, "Use1"};
  DeviceModuleWithPath singleDev2{this, "Use2"};
  DeviceModuleWithPath mappedDev12{this, "Use12"};

  // Use3 is in its own recovery "group"
  DeviceModuleWithPath singleDev3{this, "Use3"};
};

/**********************************************************************************************************************/

template<class APP>
struct Fixture {
  APP testApp;
  ChimeraTK::TestFacility testFacility{testApp, /* enableTestableMode */ false};
  ctk::VoidRegisterAccessor trigger{testFacility.getVoid("/somepath/dummyTrigger")};

  ctk::Device raw1{"Raw1"};
  ctk::Device raw2{"Raw2"};

  Fixture() { testFacility.runApplication(); };
};

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_a_5_1 \ref exceptionHandling_b_5_1 "A.5.1"
 * DeviceManagers with at least one common involved backend ID (see DeviceBackend::getInvolvedBackendIDs()) form a
 * recovery group. They collectively see exceptions and are recovered together.
 *
 * \anchor testExceptionHandling_a_5_2 \ref exceptionHandling_b_5_2 "A.5.2"
 *  Recovery groups which don't share any backend IDs behave independently.
 *
 * Note: the tests are done together because test A.5.2 requires exactly the same lines of code as the A.5.1 test
 */
BOOST_FIXTURE_TEST_CASE(TestRecoveryGroups, Fixture<BasicTestApp>) {
  // Pre-condition: wait until all devices are ok
  // Necessary because we are not using the testable mode
  for(auto const* dev : {"Use1", "Use2", "Use3", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // Test preparation: turn backend 1 into exception state
  auto dummy1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw1.getBackend());
  dummy1->throwExceptionOpen = true;
  dummy1->throwExceptionRead = true;

  trigger.write();

  // The actual test A.5.1: Check that Use1, Use2 and Use12 are in the same recovery group and thus have seen the error.
  // Requirement for A.5.2
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }

  // Test A.5.2: Use3 is in a different recovery and still OK
  CHECK_TIMEOUT(testFacility.readScalar<int>("Devices/Use3/status") == 0, 10000);

  // Remove error condition on raw1 and recover everything
  dummy1->throwExceptionOpen = false;
  dummy1->throwExceptionRead = false;

  for(auto const* dev : {"Use1", "Use2", "Use12", "Use3"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_a_5_3 \ref exceptionHandling_b_5_1 "A.5.3"
 * Two DeviceManagers which are not sharing any involved backend IDs will end up in the same recovery group if there is
 * one other DeviceManager sharing an involved backend ID with each of them.
 */
BOOST_FIXTURE_TEST_CASE(TestRecoveryGroupMerging, Fixture<BasicTestApp>) {
  // Just check that Use1 and Use2 are do not share any backend IDs. That they are in the same recovery group is already
  // tested in testExceptionHandling_a_5_1
  auto& dm1 = testApp.singleDev1.dev.getDeviceManager();
  auto& dm2 = testApp.singleDev2.dev.getDeviceManager();

  auto ids1 = dm1.getDevice().getInvolvedBackendIDs();
  auto ids2 = dm2.getDevice().getInvolvedBackendIDs();
  for(auto id : ids1) {
    BOOST_CHECK(!ids2.contains(id));
  }
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_1_0 \ref exceptionHandling_b_3_1_0 "B.3.1.0"
 * DeviceManagers wait until all involved DeviceManagers successfully complete the open step before starting the
 * initialisation handler
 */
BOOST_FIXTURE_TEST_CASE(TestRecoveryStepOpen, Fixture<BasicTestApp>) {
  // pre-condition: all (relevant) devices OK
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
  // set different value for the register written by the init handler, so we can see if the hander ran.
  raw2.write<int32_t>("/MyModule/actuator", 16);

  // Test preparation: Put backend 1 into an error state with read error.
  auto dummy1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw1.getBackend());
  dummy1->throwExceptionOpen = true;
  dummy1->throwExceptionRead = true;

  trigger.write();
  // wait until the errors have been seen.
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }

  // Wait for the device 2 backend to become ok, so we know that the according DeviceManager has run the OPEN stage.
  CHECK_TIMEOUT(raw2.isFunctional(), 10000);
  usleep(100000); // Wait 100 ms for the init handler. It should not happen, so don't wait too long...

  // The actual test: The init script of Use2 has not run.
  BOOST_TEST(raw2.read<int32_t>("MyModule/actuator") == 16);

  // Cleanup: Resolve the error and see that everything recovers.
  dummy1->throwExceptionOpen = false;
  dummy1->throwExceptionRead = false;

  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

struct BlockInitTestApp : BasicTestApp {
  explicit BlockInitTestApp() : BasicTestApp("TestStepApp") {
    singleDev1.dev.addInitialisationHandler([&](ctk::Device&) { init1("Raw1"); });
    singleDev2.dev.addInitialisationHandler([&](ctk::Device&) { init1("Raw2"); });
  }
  ~BlockInitTestApp() override { shutdown(); }

  // The execution of the first init functions can be blocked.
  // The first init handler will run through, the second one will block.
  std::atomic<bool> blockInit{false};
  std::atomic<size_t> initCounter{0};
  std::barrier<> arrivedInInitHandler{2};
  void init1(const std::string& device) {
    // cheap implementation with busy waiting
    if(blockInit) {
      if(++initCounter == 2) {
        (void)arrivedInInitHandler.arrive();

        while(blockInit) {
          usleep(100);
        }
      }
    }
    ctk::Device d{device};
    d.open();
    d.write("/MyModule/actuator", 1);
  }
};

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_3_1_1_2 \ref exceptionHandling_b_3_1_1_2 "B.3.1.1.2"
 * DeviceManagers wait until all involved DeviceManagers complete the init handler step before restoring
 * register values.
 */
BOOST_FIXTURE_TEST_CASE(TestRecoveryStepInitHandlers, Fixture<BlockInitTestApp>) {
  // pre-condition: all (relevant) devices OK
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // While everything is functional, set values for some variables. They are restored during the recovery process.
  testFacility.writeScalar<uint32_t>("/Use1/Integers/unsigned32", 17);
  testFacility.writeScalar<uint32_t>("/Use2/Integers/unsigned32", 18);

  // Wait until they arrived, the overwrite them and the values set in the init script.
  CHECK_TIMEOUT(raw1.read<uint32_t>("/Integers/unsigned32") == 17, 10000);
  CHECK_TIMEOUT(raw2.read<uint32_t>("/Integers/unsigned32") == 18, 10000);
  raw1.write<uint32_t>("/Integers/unsigned32", 13);
  raw2.write<uint32_t>("/Integers/unsigned32", 14);

  // Block the init handler, set an error condition on 1 and trigger a read.
  testApp.blockInit = true;
  // in case something goes wrong in the test: make sure the process terminates
  auto _ = cppext::finally([&]() { testApp.blockInit = false; });

  auto dummy1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw1.getBackend());
  dummy1->throwExceptionOpen = true;
  dummy1->throwExceptionRead = true;

  trigger.write();
  // wait until the errors have been seen.
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }

  // Stage 2: Resolve the error.
  dummy1->throwExceptionOpen = false;
  dummy1->throwExceptionRead = false;

  // wait until one init handler has run, and the other is blocking
  testApp.arrivedInInitHandler.arrive_and_wait();
  assert(testApp.initCounter == 2);
  // We know one of the backends is closed when entering the init handler, so we have to re-open it.
  // As we don't know which one, we just open both.
  raw1.open();
  raw2.open();

  // The actual test: none of the recovery values has been written
  BOOST_TEST(raw1.read<int32_t>("Integers/unsigned32") == 13);
  BOOST_TEST(raw2.read<int32_t>("Integers/unsigned32") == 14);

  // Stage 3: Release the blocking init handler and check that the device recovers.
  testApp.blockInit = false;

  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/
struct InitFailureApp : BasicTestApp {
  explicit InitFailureApp() : BasicTestApp("InitFailureApp") {
    singleDev2.dev.addInitialisationHandler([&](ctk::Device&) { init2(); });
  }
  ~InitFailureApp() override { shutdown(); }

  // InitFunction to raise an error
  std::atomic<bool> failInit2{false};
  std::atomic<size_t> failCounter{0};
  void init2() {
    if(failInit2) {
      throw ctk::runtime_error("Intentional failure " + std::to_string(++failCounter) + " in init2()");
    }
  }
};

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_3_1_1_3 \ref exceptionHandling_b_3_1_1_3 "B.3.1.1.3"
 * If any DeviceManager sees an exception in one of its initialisation handlers, *all* DeviceManagers in the recovery
 * group restart the recovery procedure after the POST-INIT-HANDLER barrier.
 */
BOOST_FIXTURE_TEST_CASE(TestInitFailure, Fixture<InitFailureApp>) {
  // This test is also checking that the error condition of a failure in the init handler does
  // not confuse the barrier order and lock up the manager.

  // pre-condition: all (relevant) devices OK
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // Set the init script to fail and trigger an error condition.
  testApp.failInit2 = true;

  auto dummy1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw1.getBackend());
  dummy1->throwExceptionOpen = true;
  dummy1->throwExceptionRead = true;
  trigger.write();
  // Wait until the error has been detected, then resolve it.
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }

  dummy1->throwExceptionOpen = false;
  dummy1->throwExceptionRead = false;

  // Wait until the failure counter is larger than 1 , i.e. the DeviceManager has actually failed at the place we
  // want, and the recovery process has restarted after it at least once (by checking that it failed twice).
  // FIXME: This does not check that
  // - all device managers retry
  // - the recovery restarts with open (could only be the init step)
  // - the recovery only happens after the init barrier
  BOOST_CHECK(false);
  CHECK_TIMEOUT(testApp.failCounter > 1, 10000);

  // Resolve the error condition and wait until everything has recovered.
  testApp.failInit2 = false;
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

struct RecoveryFailureTestApp : ctk::Application {
  explicit RecoveryFailureTestApp() : Application("RecoveryFailureTestApp") {}
  ~RecoveryFailureTestApp() override { shutdown(); }

  ctk::SetDMapFilePath path{"recoveryGroups.dmap"};

  // recovery group with Use1 and Use2
  DeviceModuleWithPath singleDev1{this, "Use1"};
  DeviceModuleWithPath singleDev2{this, "Use2"};
  DeviceModuleWithPath mappedDev12{this, "Use12ReadOnly"};
};

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_1_2_1 \ref exceptionHandling_b_3_1_2_1 "B.3.1.2.1"
 * DeviceManagers wait until all involved DeviceManagers complete the register value restoring before activating the
 * synchronous read.
 */

BOOST_FIXTURE_TEST_CASE(TestRecoveryWriteBarrier, Fixture<RecoveryFailureTestApp>) {
  // FIXME This test is mixing B.3.1.2.1 and B.3.1.2.2, and does none of them cleanly.
  // This test is checking that the error condition of a failure when writing the recovery accessors do
  // not confuse the barrier order and lock up the manager.
  BOOST_CHECK(false);
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_1_2_2 \ref exceptionHandling_b_3_1_2_2 "B.3.1.2.2"
 * If any DeviceManager sees an exception while restoring register values, *all* DeviceManagers in the
 * recovery group restart the recovery procedure after the POST-WRITE-RECOVERY barrier.
 */

BOOST_FIXTURE_TEST_CASE(TestRecoveryWriteFailure, Fixture<RecoveryFailureTestApp>) {
  // FIXME This test is mixing B.3.1.2.1 and B.3.1.2.2, and does none of them cleanly.
  // This test is checking that the error condition of a failure when writing the recovery accessors do
  // not confuse the barrier order and lock up the manager.
  BOOST_CHECK(false);

  // pre-condition: all (relevant) devices OK
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // Write something to Use1 so we can check when its recovery accessor writing is through.
  // We take Use1 this time because it does not have an init handler, so the DeviceManager
  // does not inadvertently close the device while we want to read it in the test.
  // In addition, Use12ReadOnly has only read-only registers, so it does not cause errors in recovery,
  // which would set the device 1 back into error state.
  testFacility.writeScalar<uint32_t>("/Use1/Integers/unsigned32", 18);
  CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);

  // create an error condition which also thrown when writing (the recovery accessors)
  auto dummy4 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw2.getBackend());
  dummy4->throwExceptionOpen = true;
  dummy4->throwExceptionRead = true;
  dummy4->throwExceptionWrite = true;
  trigger.write();

  // Wait until the error has been detected, then clean the recovery value on Use1 and
  // resolve the open/read errors, but keep the write error.
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }
  // Get the status accessors to check that Use1 does not report ready, although it
  // got through with its recovery.
  auto Use1Status = testFacility.getScalar<int32_t>("Devices/Use1/status");
  Use1Status.readLatest(); // just empty the queue
  auto Use1BecameFunctional = testFacility.getVoid("Devices/Use1/deviceBecameFunctional");
  Use1BecameFunctional.readLatest(); // just empty the queue

  // now clear the opening error.
  dummy4->throwExceptionOpen = false;
  dummy4->throwExceptionRead = false;

  // Wait until we can read/write from raw1 without error.
  CHECK_TIMEOUT(raw1.isFunctional(), 10000);
  raw1.write("Integers/unsigned32", 0);
  // Wait until we have seen the recovery section of Use1 run through, then clear it and wait again.
  // Like this we make sure that the error condition in Use2 has been hit at least once.
  CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);

  raw1.write("Integers/unsigned32", 0);
  CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);

  // No changing status has been reported in the mean time
  BOOST_TEST(!Use1Status.readNonBlocking());
  BOOST_TEST(!Use1BecameFunctional.readNonBlocking());

  // Finally, resolve the error condition and wait until everything recovers.
  dummy4->throwExceptionWrite = false;
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

struct IncompleteRecoveryTestApp : ctk::Application {
  explicit IncompleteRecoveryTestApp() : Application("IncompleteRecoveryTestApp") {}
  ~IncompleteRecoveryTestApp() override { shutdown(); }

  ctk::SetDMapFilePath path{"recoveryGroups.dmap"};

  std::atomic<bool> throwInInit{false};
  std::atomic<size_t> initCounter{0};
  std::barrier<> aboutToThrow{2};
  void init() {
    // cheap implementation with busy waiting
    if(throwInInit) {
      if(++initCounter == 2) {
        // The other init handler has passed this point already. Wait a bit to be pretty sure it has reached
        // the INIT_HANDLER barrier.
        usleep(100000); // 100 ms

        // Tell the test thread that we are here, about to throw the exception
        (void)aboutToThrow.arrive();

        // Jump out of the DeviceManager main loop with a thread_interrupted exception, just like all other
        // breadpoints do
        throw boost::thread_interrupted(); // NOLINT hicpp-exception-baseclass
      }
    }
  }

  // recovery group with Use1 and Use2
  DeviceModuleWithPath singleDev1{this, "Use1"};
  DeviceModuleWithPath singleDev2{this, "Use2"};
  DeviceModuleWithPath mappedDev12{this, "Use12ReadOnly"};
};

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_1_2_2 \ref exceptionHandling_b_3_1_2_2 "B.3.1.2.2" The application terminates
 * cleanly, even if the recovery is waiting at one of the barriers mentioned in \ref b_3_1 "3.1"
 */
BOOST_AUTO_TEST_CASE(TestIncompleteRecovery) {
  { // open a new scope so we can test after the app goes out of scope
    IncompleteRecoveryTestApp testApp;
    ChimeraTK::TestFacility testFacility{testApp, /* enableTestableMode */ false};
    testFacility.runApplication();

    // pre-condition: all (relevant) devices OK
    for(auto const* dev : {"Use1", "Use2"}) {
      CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
    }

    testApp.throwInInit = true;
    testApp.singleDev1.dev.reportException("reported from test");

    // Wait until the init handler which will throw told us it has reached that point, so we don't end the application
    // scope before the test is sensitive.
    testApp.aboutToThrow.arrive_and_wait();

    // now end the scope of the application
  }
  // The actual test: We reached this point, the test did not block
  BOOST_CHECK(true);
}

/**********************************************************************************************************************/
