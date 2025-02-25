// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <ChimeraTK/Device.h>

#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/thread/exceptions.hpp>

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
#include <ChimeraTK/LogicalNameMappingBackend.h>
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

// Test backend which allows to block write operations.
struct WriteBlockingDummy : public ChimeraTK::ExceptionDummy {
  using ExceptionDummy::ExceptionDummy;

  std::atomic<bool> blockWriteOnce{false};         // Only use the following barrier if true.
  std::barrier<> blockWriteArrivedBarrier{2};      // Tell the test thread that we are there
  std::barrier<> blockWriteContinueBarrier{2};     // Wait for the test to tell us to continue.
  std::atomic<bool> throwThreadInterrupted{false}; // Throw a boost::thread_interrupted exception.

  void write(uint64_t bar, uint64_t address, int32_t const* data, size_t sizeInBytes) override {
    if(blockWriteOnce.exchange(false)) {           // atomically set to false and check the old value
      (void)blockWriteArrivedBarrier.arrive();     // Notify the test.
      blockWriteContinueBarrier.arrive_and_wait(); // Wait for the test to tell us to continue.
      if(throwThreadInterrupted) {
        throw boost::thread_interrupted(); // NOLINT hicpp-exception-baseclass
      }
    }

    ExceptionDummy::write(bar, address, data, sizeInBytes);
  }

  // NOLINTNEXTLINE performance-unnecessary-value-param (signature required like this by BackendFactory)
  static boost::shared_ptr<DeviceBackend> creatorFunction(std::string, std::map<std::string, std::string> parameters) {
    return boost::make_shared<WriteBlockingDummy>(parameters["map"]);
  }

  struct Registerer {
    Registerer() {
      ctk::BackendFactory::getInstance().registerBackendType("WriteBlockingDummy", WriteBlockingDummy::creatorFunction);
    }
  };
};

static WriteBlockingDummy::Registerer writeBlockingBackendRegisterer;

// Test backend which counts the number of open() calls and allows to block write operations.
struct OpenCountingLmapBackend : public ChimeraTK::LogicalNameMappingBackend {
  using LogicalNameMappingBackend::LogicalNameMappingBackend;

  std::atomic<size_t> openCounter{0};

  std::barrier<> aboutToThrowArrivedBarrier{2};    // Tell the test thread that we are there
  std::barrier<> aboutToThrowContinueBarrier{2};   // Wait for the test to tell us to continue.
  std::atomic<bool> throwThreadInterrupted{false}; // Throw a boost::thread_interrupted exception.
  static std::atomic<size_t> globalOpenCounter;    // Count the total number of open calls in throwing context.

  std::atomic<bool> blockOpen{false};
  std::barrier<> blockOpenArrivedBarrier{2};
  std::barrier<> blockOpenContinueBarrier{2};

  void open() override {
    if(throwThreadInterrupted) {
      // Only block for the testing sequence when the other two devices have successfully opened (and are waiting at the
      // barrier). This does not run in parallel due to the recovery groups "open mutex". If we block here, none of the
      // other backends will get through because we are holding the lock.
      if(globalOpenCounter == 2) {
        (void)aboutToThrowArrivedBarrier.arrive();
        aboutToThrowContinueBarrier.arrive_and_wait();
        throw boost::thread_interrupted(); // NOLINT hicpp-exception-baseclass
      }
      // just retry later
      throw ctk::runtime_error("Not ready to block yet");
    }
    // Handshake the barrier exactly once when requested. The first backend to arrive here will atomically check it and
    // turn it off
    if(blockOpen.exchange(false)) {
      (void)blockOpenArrivedBarrier.arrive();
      blockOpenContinueBarrier.arrive_and_wait();
    }

    ++openCounter;
    ++globalOpenCounter;
    ChimeraTK::LogicalNameMappingBackend::open();
  }

  // NOLINTNEXTLINE performance-unnecessary-value-param (signature required like this by BackendFactory)
  static boost::shared_ptr<DeviceBackend> creatorFunction(std::string, std::map<std::string, std::string> parameters) {
    auto ptr = boost::make_shared<OpenCountingLmapBackend>(parameters["map"]);
    parameters.erase(parameters.find("map"));
    ptr->_parameters = parameters;
    return boost::static_pointer_cast<DeviceBackend>(ptr);
  }

  struct Registerer {
    Registerer() {
      ctk::BackendFactory::getInstance().registerBackendType(
          "OpenCountingLmapBackend", OpenCountingLmapBackend::creatorFunction);
    }
  };
};

std::atomic<size_t> OpenCountingLmapBackend::globalOpenCounter{0};
static OpenCountingLmapBackend::Registerer testLmapBackendRegisterer;

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
 * \anchor testExceptionHandling_a_5 \ref exceptionHandling_a_5 "A.5"
 * DeviceManagers with at least one common involved backend ID (see DeviceBackend::getInvolvedBackendIDs()) form a
 * recovery group. They collectively see exceptions and are recovered together.
 *
 * \anchor testExceptionHandling_a_5_1 \ref exceptionHandling_a_5_1 "A.5.1"
 *  Recovery groups which don't share any backend IDs behave independently.
 *
 * Note: the tests are done together because test A.5.1 requires exactly the same lines of code as the A.5 test
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

  // The actual test A.5: Check that Use1, Use2 and Use12 are in the same recovery group and thus have seen the error.
  // Requirement for A.5.1
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }

  // Test A.5.1: Use3 is in a different recovery and still OK
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
 * \anchor testExceptionHandling_a_5_2 \ref exceptionHandling_a_5_2 "A.5.2"
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
 * \anchor testExceptionHandling_b_3_1 \ref exceptionHandling_b_3_1 "B.3.1"
 * DeviceManagers wait until all involved DeviceManagers have seen the error condition
 * before trying to re-open ("barrier POST-DETECT").
 */
BOOST_FIXTURE_TEST_CASE(TestDetectBarrier, Fixture<BasicTestApp>) {
  // Unfortunately we can only do a race condition test, and check that the race condition does not occur. So the test
  // is insensitive for most of the time, but we still have a chance to see the error condition from time to time if it
  // is there.
  // Through the public API we cannot inject waiting code which block before the barrier, and we test that nothing has
  // run behind the barrier yet (would be sensitive most of the time).
  // So we take the first point after the barrier where we can block the code execution, and check that everything
  // before the barrier has happened.

  // Pre-condition: wait until all devices are ok
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
  auto testLmap1 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.singleDev1.dev.getDeviceManager().getDevice().getBackend());
  testLmap1->blockOpen = true;

  testApp.singleDev1.dev.reportException("reported from TestDetectBarrier");

  testLmap1->blockOpenArrivedBarrier.arrive_and_wait();
  // We now know that the recovery is blocked, so the following test does not produce
  // false positives because the recovery has already run through and cleared the error condition before we see it.

  // The actual test:
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    // Test without timeout! The devices must NOW be in error state.
    BOOST_TEST(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1);
  }

  // Finish the recovery.
  (void)testLmap1->blockOpenContinueBarrier.arrive();
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_2_1 \ref exceptionHandling_b_3_2_1 "B.3.2.1"
 * DeviceManagers wait until all involved DeviceManagers successfully complete the open step before starting the
 * initialisation handler
 */
BOOST_FIXTURE_TEST_CASE(TestRecoveryStepOpen, Fixture<BasicTestApp>) {
  // pre-condition: all devices in recovery group are OK
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
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
 * \anchor testExceptionHandling_b_3_2_2_2 \ref exceptionHandling_b_3_2_2_2 "B.3.2.2.2"
 * DeviceManagers wait until all involved DeviceManagers complete the init handler step before restoring
 * register values.
 */
BOOST_FIXTURE_TEST_CASE(TestRecoveryStepInitHandlers, Fixture<BlockInitTestApp>) {
  // pre-condition: all devices in recovery group are OK
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
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
    singleDev1.dev.addInitialisationHandler([&](ctk::Device&) { init(); });
    singleDev2.dev.addInitialisationHandler([&](ctk::Device&) { init(); });
  }
  ~InitFailureApp() override { shutdown(); }

  // InitFunction to raise an error
  std::atomic<bool> blockInitOnce{false}; // block the execution of at the start if all init handlers
  std::barrier<> blockInitArrivedBarrier{2};
  std::barrier<> blockInitContinueBarrier{2};
  std::atomic<bool> failInit{false};
  std::atomic<size_t> initCounter{0};
  std::atomic<size_t> initSuccessCounter{0};
  std::barrier<> aboutToFail{2};     // notify the test where we are. It has to do some checks
  std::barrier<> proceedWithFail{2}; // wait for the test to complete its checks
  void init() {
    if(blockInitOnce) {
      blockInitOnce = false;
      (void)blockInitArrivedBarrier.arrive();     // notify the test
      blockInitContinueBarrier.arrive_and_wait(); // only continue when testing is done
    }
    if(failInit) {
      if(++initCounter == 2) {
        // This branch will be only hit once because the counter is higher afterwards.
        (void)aboutToFail.arrive();        // notify the test that it can do the preparation
        proceedWithFail.arrive_and_wait(); // wait for the test to complete the preparation
        throw ctk::runtime_error("Intentional failure in init()");
      }
    }
    ++initSuccessCounter;
  }
};

/**********************************************************************************************************************/
/**
 * \anchor testExceptionHandling_b_3_2_2_3 \ref exceptionHandling_b_3_2_2_3 "B.3.2.2.3"
 * If any DeviceManager sees an exception in one of its initialisation handlers, *all* DeviceManagers in the recovery
 * group restart the recovery procedure after the POST-INIT-HANDLER barrier.
 */
BOOST_FIXTURE_TEST_CASE(TestInitFailure, Fixture<InitFailureApp>) {
  // Side effect: This test is also checking that the error condition of a failure in the init handler does
  // not confuse the barrier order and lock up the manager (basically error handling smoke test).

  // This test contains three checks:
  //  1. *All* device managers restart the recovery.
  //  2. The recovery restarts with *open* (not only the init step is repeated).
  //  3. The recovery happens *after the POST-INIT-HANDLER barrier*.

  // pre-condition: all devices in recovery group are OK
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // preparation for check 3: Use1 is at the POST-INIT-HANDLER barrier, i.e. init handler is through, recovering write
  // values is not.
  testFacility.writeScalar<uint32_t>("/Use1/Integers/unsigned32", 17); // written by recovery write
  testFacility.writeScalar<uint32_t>("/Use2/Integers/unsigned32", 18); // written by recovery write
  testFacility.writeScalar<int16_t>("/Use12/Integers/signed16", 19);   // written by recovery write
  // Wait until the value arrived at the device, the overwrite
  CHECK_TIMEOUT(raw1.read<uint32_t>("/Integers/unsigned32") == 17, 10000);
  CHECK_TIMEOUT(raw2.read<uint32_t>("/Integers/unsigned32") == 18, 10000);
  CHECK_TIMEOUT(raw1.read<int16_t>("/Integers/signed16") == 19, 10000);
  raw1.write<uint32_t>("/Integers/unsigned32", 13);
  raw2.write<uint32_t>("/Integers/unsigned32", 14);
  raw1.write<int16_t>("/Integers/signed16", 15);

  // Set the init script to fail and trigger an error condition.
  testApp.failInit = true;
  testApp.initSuccessCounter = 0;
  testApp.singleDev1.dev.reportException("reported from TestInitFailure");

  testApp.aboutToFail.arrive_and_wait();

  // Check 3 part 1: One of the init handlers increased the initSuccessCounter, so we know it has
  // run and due to the sleeps, we can be pretty sure it has arrived at the POST-INIT-HANDLER barrier.
  BOOST_CHECK(testApp.initSuccessCounter == 1);

  // Preparation. At this point we know that
  // - One of the init handlers has run through.
  // - The other init handler is waiting in the init handler
  // - The successful init handler has a higher open count because the device is re-opened after the init handler
  // But we don't know which device is in which state.
  // So we store all open counters.
  auto testLmap1 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.singleDev1.dev.getDeviceManager().getDevice().getBackend());
  auto testLmap2 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.singleDev2.dev.getDeviceManager().getDevice().getBackend());
  auto testLmap12 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.mappedDev12.dev.getDeviceManager().getDevice().getBackend());
  size_t openCount1 = testLmap1->openCounter;
  size_t openCount2 = testLmap2->openCounter;
  size_t openCount12 = testLmap12->openCounter;

  // Also block the execution of newly starting init handlers so we know that at this point only
  // the open step has happened. This simplifies testing.
  testApp.blockInitOnce = true;

  // now make the second init handler throw
  (void)testApp.proceedWithFail.arrive();

  // Check 1 and 2: *All* DeviceManagers have *restarted* the  recovery procedure.
  // The restart of the recovery procedure is detected by looking at the open counter.
  testApp.blockInitArrivedBarrier.arrive_and_wait();
  BOOST_TEST(testLmap1->openCounter == openCount1 + 1);
  BOOST_TEST(testLmap2->openCounter == openCount2 + 1);
  BOOST_TEST(testLmap12->openCounter == openCount12 + 1);

  // Check 3: The recover actually restarted after the POST-INIT-HANDLER and no write recovery was done.
  // We know that one of the init handlers is blocking, so one of the devices is closed,
  // but no futher close will happen while the init handler is blocking.
  // Just reopen all devices.
  raw1.open();
  raw2.open();
  BOOST_TEST(raw1.read<uint32_t>("/Integers/unsigned32") == 13);
  BOOST_TEST(raw2.read<uint32_t>("/Integers/unsigned32") == 14);
  BOOST_TEST(raw1.read<int16_t>("/Integers/signed16") == 15);

  // Resolve the error condition and wait until everything has recovered.
  testApp.failInit = false;
  (void)testApp.blockInitContinueBarrier.arrive();
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

// App used by TestRecoveryWriteBarrier and TestIncompleteWriteRecovery
struct WriteRecoveryTestApp : ctk::Application {
  explicit WriteRecoveryTestApp() : Application("RecoveryFailureTestApp") {}
  ~WriteRecoveryTestApp() override { shutdown(); }

  ctk::SetDMapFilePath path{"recoveryGroups.dmap"};

  // recovery group with Use1 and Use2
  DeviceModuleWithPath singleDev1{this, "Use1"};
  DeviceModuleWithPath singleDev2{this, "Use2"};
  // Use the combining xlmap file which does not use write registers.
  // The tests (TestRecoveryWriteBarrier, TestIncompleteWriteRecovery) require that there is only one register
  // written on backend 2.
  DeviceModuleWithPath mappedDev12{this, "Use12ReadOnly"};
};

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_2_3_1 \ref exceptionHandling_b_3_2_3_1 "B.3.2.3.1"
 * DeviceManagers wait until all involved DeviceManagers complete the register value restoring before clearing the
 * internal error flag.
 */

BOOST_FIXTURE_TEST_CASE(TestRecoveryWriteBarrier, Fixture<WriteRecoveryTestApp>) {
  // pre-condition: all devices OK
  for(auto const* dev : {"Use1", "Use2", "Use12ReadOnly"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
  // Write something to Use1 so we can check when its recovery accessor writing is through.
  testFacility.writeScalar<uint32_t>("/Use1/Integers/unsigned32", 18);
  CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);
  // Change the value on the device to detect when the recovery writing is through.
  raw1.write("Integers/unsigned32", 0);

  // Block Use2 and trigger a recovery
  auto dummy1 = boost::dynamic_pointer_cast<WriteBlockingDummy>(raw1.getBackend());
  auto dummy2 = boost::dynamic_pointer_cast<WriteBlockingDummy>(raw2.getBackend());
  dummy2->blockWriteOnce = true;
  testApp.singleDev2.dev.reportException("reported from TestRecoveryWriteBarrier");

  // Wait until Use2 is blocking
  dummy2->blockWriteArrivedBarrier.arrive_and_wait();

  // store the Use1 open counters
  auto lmapDummy1 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.singleDev1.dev.getDeviceManager().getDevice().getBackend());
  size_t openCount1 = lmapDummy1->openCounter;

  // The actual test:
  // Even though Use1 has completed the recovery write step, it has not reset the internal error flag yet,
  // so reporting an exception does not cause another recovery round (monitored by looking at the open counter
  // after the successful recovery.
  CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000); // recovery write step complete
  testApp.singleDev1.dev.reportException("This exception should be suppressed.");
  (void)dummy2->blockWriteContinueBarrier.arrive();
  for(auto const* dev : {"Use1", "Use2", "Use12ReadOnly"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
  BOOST_TEST(lmapDummy1->openCounter == openCount1);
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_2_3_2 \ref exceptionHandling_b_3_2_3_2 "B.3.2.3.2"
 * If any DeviceManager sees an exception while restoring register values, *all* DeviceManagers in the
 * recovery group restart the recovery procedure after the POST-WRITE-RECOVERY barrier.
 */

BOOST_FIXTURE_TEST_CASE(TestRecoveryWriteFailure, Fixture<WriteRecoveryTestApp>) {
  // Side effect: This test is checking that the error condition of a failure when writing the recovery accessors do
  // not confuse the barrier order and lock up the manager.

  // This test contains three checks:
  //  1. *All* device managers restart the recovery.
  //  2. The recovery restarts with *open* (not only the init step is repeated).
  //  3. The recovery happens *after the POST-WRITE-RECOVERY barrier*.

  // pre-condition: all devices OK
  for(auto const* dev : {"Use1", "Use2", "Use12ReadOnly"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // Write something to Use1 so we can check when its recovery accessor writing is through.
  testFacility.writeScalar<uint32_t>("/Use1/Integers/unsigned32", 18);
  CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);
  // Change the value on the device to detect that the writing is through.
  raw1.write("Integers/unsigned32", 0);

  // create an error condition which throws when writing (the recovery accessors)
  auto dummy2 = boost::dynamic_pointer_cast<WriteBlockingDummy>(raw2.getBackend());
  dummy2->throwExceptionWrite = true;
  dummy2->blockWriteOnce = true;
  testApp.singleDev2.dev.reportException("reported from TestRecoveryWriteFailure");

  // Wait until Use2 is blocking and Use1 has restored the write values.
  dummy2->blockWriteArrivedBarrier.arrive_and_wait();
  CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);
  // sleep a bit so we can be pretty sure that Use1 has arrived at the POST-WRITE-RECOVERY barrier
  usleep(100000); // 100 ms

  // Take a snapshot of the open counters for checks 1 and 2.
  auto testLmap1 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.singleDev1.dev.getDeviceManager().getDevice().getBackend());
  auto testLmap2 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.singleDev2.dev.getDeviceManager().getDevice().getBackend());
  auto testLmap12 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.mappedDev12.dev.getDeviceManager().getDevice().getBackend());
  size_t openCount1 = testLmap1->openCounter;
  size_t openCount2 = testLmap2->openCounter;
  size_t openCount12 = testLmap12->openCounter;

  // preparation for check 3: recovery started directly after the POST-WRITE-RECOVERY barrier
  // Get an asynchronous variable from Use1. It must not have seen any data after the exception
  auto pushedSigned32 = testFacility.getScalar<int32_t>("/Use1/Integers/pushedSigned32");
  pushedSigned32.readLatest(); // just empty the queue.
  // The last thing we should have seen is the exception, so data validity is faulty.
  BOOST_CHECK(pushedSigned32.dataValidity() == ctk::DataValidity::faulty);

  // Now let Use2 continue and throw the write exception. Already request to
  // stop at the next write.
  dummy2->blockWriteOnce = true;
  (void)dummy2->blockWriteContinueBarrier.arrive();
  // Now the recovery should see an error and continue from the beginning.
  // Wait again until Use2 blocks when writing.
  dummy2->blockWriteArrivedBarrier.arrive_and_wait();

  // Check 1 and 2: *All* DeviceManagers have *restarted* the  recovery procedure.
  // The restart of the recovery procedure is detected by looking at the open counter.
  BOOST_TEST(testLmap1->openCounter == openCount1 + 1);
  BOOST_TEST(testLmap2->openCounter == openCount2 + 1);
  BOOST_TEST(testLmap12->openCounter == openCount12 + 1);

  // Check 3: After seeing the exception in Use2, Use1 has not completed the
  // recovery after the POST-WRITE-RECOVERY barrier and hence async read is not turned on yet.
  // Wait a bit (100 ms) for data to arrive, but not too long as we don't expect anything.
  usleep(100000);
  BOOST_CHECK(!pushedSigned32.readNonBlocking());

  // Finally, resolve the error condition and wait until everything recovers.
  dummy2->throwExceptionWrite = false;
  (void)dummy2->blockWriteContinueBarrier.arrive();
  for(auto const* dev : {"Use1", "Use2", "Use12ReadOnly"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_2_4_1 \ref exceptionHandling_b_3_2_4_1 "B.3.2.4.1"
 * DeviceManagers wait until all involved DeviceManagers clear their internal error before flag activating the
 * asynchronous read in B.3.2.5 ("barrier POST-CLEAR-ERROR").
 */
BOOST_AUTO_TEST_CASE(TestClearErrorBarrier) {
  // This test is trying to provoke a rare race condition that existed (and should now be removed). If
  // one backend has already activated the async read and had not waited at the POST-CLEAR-ERROR barrier,
  // another thread might not have cleared the internal error condition yet and will discard the reported exception.
  // In this case it will never reach the POST-DETECT barrier all DeviceManagers in this recovery group are stuck,
  // because the others are waiting there.

  BasicTestApp testApp;
  ChimeraTK::TestFacility testFacility{testApp, /* enableTestableMode */ false};
  auto pushed1 = testFacility.getScalar<int>(std::string("/Use1/Integers/pushedSigned32"));
  testFacility.runApplication();

  // Maximise the chance for the race condition:
  // As soon as Use1 sends the initial value to the push type accessor, report an exception to Use12.
  // If it misses it, no recovery is triggered and the open count is 1.
  // If it got it, there was a recovery round and the open count is 2.
  pushed1.read();
  testApp.mappedDev12.dev.reportException("reported from TestClearErrorBarrier");

  auto testLmap1 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
      testApp.singleDev1.dev.getDeviceManager().getDevice().getBackend());
  CHECK_TIMEOUT(testLmap1->openCounter == 2, 10000);

  // Wait for recovery to complete.
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_4_open \ref exceptionHandling_b_3_4 "B.3.4" The application terminates
 * cleanly, even if the recovery is waiting at one of the barriers mentioned in \ref exceptionHandling_b_3_2 "3.2"
 *
 * Test at the POST-OPEN barrier.
 */
BOOST_AUTO_TEST_CASE(TestIncompleteRecoveryOpen) {
  { // open a new scope so we can test after the app goes out of scope
    BasicTestApp testApp;
    ChimeraTK::TestFacility testFacility{testApp, /* enableTestableMode */ false};
    testFacility.runApplication();

    // pre-condition: all devices in recovery group are OK
    for(auto const* dev : {"Use1", "Use2", "Use12"}) {
      CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
    }

    // Prepare throwing in thread_interrupted in open
    OpenCountingLmapBackend::globalOpenCounter = 0;
    auto testLmap2 = boost::dynamic_pointer_cast<OpenCountingLmapBackend>(
        testApp.singleDev2.dev.getDeviceManager().getDevice().getBackend());
    testLmap2->throwThreadInterrupted = true;
    testApp.singleDev1.dev.reportException("reported from TestIncompleteRecoveryOpen");

    // Wait until the dummy backend told us it is about to throw.
    testLmap2->aboutToThrowArrivedBarrier.arrive_and_wait();

    // wait until the other DeviceManager has opened its backend, then sleep a bit to be pretty sure
    // it has reached the barrier
    CHECK_TIMEOUT(testApp.singleDev1.dev.getDeviceManager().getDevice().isFunctional(), 10000);
    usleep(100000);

    // once we let dummy2 continue it will throw.
    (void)testLmap2->aboutToThrowContinueBarrier.arrive();
  }
  // The actual test: We reached this point, the test did not block
  BOOST_CHECK(true);
}

/**********************************************************************************************************************/

struct IncompleteRecoveryTestApp : ctk::Application {
  explicit IncompleteRecoveryTestApp() : Application("IncompleteRecoveryTestApp") {
    singleDev1.dev.addInitialisationHandler([&](ctk::Device&) { init(); });
    singleDev2.dev.addInitialisationHandler([&](ctk::Device&) { init(); });
  }
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
        aboutToThrow.arrive_and_wait();

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
 * \anchor testExceptionHandling_b_3_4_init \ref exceptionHandling_b_3_4 "B.3.4" The application terminates
 * cleanly, even if the recovery is waiting at one of the barriers mentioned in \ref exceptionHandling_b_3_2 "3.2"
 *
 * Test at the POST-INIT-HANDLER barrier.
 */
BOOST_AUTO_TEST_CASE(TestIncompleteRecoveryInit) {
  { // open a new scope so we can test after the app goes out of scope
    IncompleteRecoveryTestApp testApp;
    ChimeraTK::TestFacility testFacility{testApp, /* enableTestableMode */ false};
    testFacility.runApplication();

    // pre-condition: all devices in recovery group are OK
    for(auto const* dev : {"Use1", "Use2", "Use12ReadOnly"}) {
      CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
    }

    testApp.throwInInit = true;
    testApp.singleDev1.dev.reportException("reported from TestIncompleteRecoveryInit");

    // Wait until the init handler which will throw told us it has reached that point, so we don't end the application
    // scope before the test is sensitive.
    // The second init handler which is run does the blocking, and sleeps a bit before arriving here, so we are pretty
    // sure that the other init handler has reached the barrier.
    testApp.aboutToThrow.arrive_and_wait();
  }
  // The actual test: We reached this point, the test did not block
  BOOST_CHECK(true);
}

/**********************************************************************************************************************/

/**
 * \anchor testExceptionHandling_b_3_4_writeRecovery \ref exceptionHandling_b_3_4 "B.3.4" The application terminates
 * cleanly, even if the recovery is waiting at one of the barriers mentioned in \ref exceptionHandling_b_3_2 "3.2"
 *
 * Test at the POST-INIT-HANDLER barrier.
 */
BOOST_AUTO_TEST_CASE(TestIncompleteWriteRecovery) {
  { // open a new scope so we can test after the app goes out of scope

    // Use the WriteRecoveryTestApp with Use12ReadOnly because we again require that only Use2 is writing to the backend
    WriteRecoveryTestApp testApp;
    ChimeraTK::TestFacility testFacility{testApp, /* enableTestableMode */ false};
    testFacility.runApplication();

    // pre-condition: all devices in recovery group are OK
    for(auto const* dev : {"Use1", "Use2", "Use12ReadOnly"}) {
      CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
    }

    ctk::Device raw1{"Raw1"};
    raw1.open();

    // Write something to Use1 so we can check when its recovery accessor writing is through.
    testFacility.writeScalar<uint32_t>("/Use1/Integers/unsigned32", 18);
    CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);
    // Change the value on the device to detect that the writing is through.
    raw1.write("Integers/unsigned32", 0);

    // Prepare throwing in thread_interrupted in the read recovery
    ctk::Device raw2{"Raw2"};
    raw2.open();
    auto dummy2 = boost::dynamic_pointer_cast<WriteBlockingDummy>(raw2.getBackend());
    dummy2->blockWriteOnce = true;
    dummy2->throwThreadInterrupted = true;
    testApp.singleDev1.dev.reportException("reported from TestIncompleteWriteRecovery");

    // Wait until the dummy backend told us it is about to throw.
    dummy2->blockWriteArrivedBarrier.arrive_and_wait();

    // wait until the other DeviceManager has written its values, then sleep a bit to be pretty sure
    // it has reached the barrier
    CHECK_TIMEOUT(raw1.read<uint32_t>("Integers/unsigned32") == 18, 10000);
    usleep(100000);

    // once we let dummy2 continue it will throw.
    (void)dummy2->blockWriteContinueBarrier.arrive();
  }
  // The actual test: We reached this point, the test did not block
  BOOST_CHECK(true);
}

/**********************************************************************************************************************/
