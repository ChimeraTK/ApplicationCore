// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <ChimeraTK/Exception.h>
#define BOOST_TEST_MODULE testRecoveryGroups

#include "Application.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "ModuleGroup.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/cppext/finally.hpp>
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

// A test application with 7 devices in 3 recovery groups.
// It also contains intialisation handlers to test the barriers which ensure
// that each recovery step completes for all devices before continuing with the next step.
struct TestApp1 : ctk::Application {
  explicit TestApp1(std::string const& name = "threeRecoveryGroups") : Application(name) {
    // FIXME: Split the app into individual use cases. It has become to cluttered and complex.
    singleDev2.dev.addInitialisationHandler([&](ctk::Device&) { init1("Raw2"); });
  }
  ~TestApp1() override { shutdown(); }

  ctk::SetDMapFilePath path{"recoveryGroups.dmap"};

  struct DeviceModuleWithPath : public ctk::ModuleGroup {
    DeviceModuleWithPath(
        ModuleGroup* owner, std::string const& cdd_, std::function<void(ChimeraTK::Device&)> initHandler_ = nullptr)
    : ModuleGroup(owner, cdd_, ""), cdd(cdd_), initHandler(std::move(initHandler_)) {}
    std::string cdd;
    std::function<void(ChimeraTK::Device&)> initHandler;
    ctk::DeviceModule dev{this, cdd, "/somepath/dummyTrigger", initHandler};
  };

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

  // The second init function writes a 2 without waiting.
  std::atomic<bool> failInit2{false};
  std::atomic<size_t> failCounter{0};
  void init2() {
    if(failInit2) {
      throw ctk::runtime_error("Intentional failure " + std::to_string(++TestApp1::failCounter) + " in init2()");
    }
    // will be overwritten, but keep because for the restructuring
    ctk::Device d{"Raw2"};
    d.open();
    d.write("/MyModule/actuator", 2);
  }

  // First recovery group: Two devices with one backend each, and a logical name map which uses them both.
  DeviceModuleWithPath singleDev1{this, "Use1", [&](ctk::Device&) { init1("Raw1"); }};
  DeviceModuleWithPath singleDev2{this, "Use2", [&](ctk::Device&) { init2(); }};
  DeviceModuleWithPath mappedDev12{this, "Use12"};

  // Second recovery group with use3 and use4
  DeviceModuleWithPath singleDev3{this, "Use3"};
  DeviceModuleWithPath singleDev4{this, "Use4"};
  DeviceModuleWithPath mappedDev34{this, "Use34"};

  // Use5 is in its own recovery "group"
  DeviceModuleWithPath singleDev5{this, "Use5"};
};

/**********************************************************************************************************************/

template<class APP>
struct Fixture {
  APP testApp;
  ChimeraTK::TestFacility testFacility{testApp, /* enableTestableMode */ false};
  ctk::VoidRegisterAccessor trigger{testFacility.getVoid("/somepath/dummyTrigger")};

  ctk::Device raw1{"Raw1"};
  ctk::Device raw2{"Raw2"};
  ctk::Device raw3{"Raw3"};
  ctk::Device raw4{"Raw4"};
  ctk::Device raw5{"Raw5"};

  Fixture() { testFacility.runApplication(); };
};

/**********************************************************************************************************************/

// Test that App1 actually has three different recovery groups: [Use1, Use2, Use12], [Use3, Use4, Use34] and [Use5]
BOOST_FIXTURE_TEST_CASE(Test3RecoveryGroups, Fixture<TestApp1>) {
  // wait until all devices are ok
  for(auto const* dev : {"Use1", "Use2", "Use3", "Use4", "Use5", "Use12", "Use34"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // turn backend 3 into exception state
  auto dummy3 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw3.getBackend());
  dummy3->throwExceptionOpen = true;
  dummy3->throwExceptionRead = true;

  trigger.write();
  // check that Use3, Use4 and Use34 are in error state, while the other devices are still working
  for(auto const* dev : {"Use3", "Use4", "Use34"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }

  testFacility.writeScalar("Use1/Integers/signed32", 42);
  CHECK_TIMEOUT(raw1.read<int32_t>("Integers/signed32") == 42, 10000);
  CHECK_TIMEOUT(testFacility.readScalar<int>("Devices/Use1/status") == 0, 10000);

  testFacility.writeScalar("/Use2/Integers/signed32", 43);
  CHECK_TIMEOUT(raw2.read<int32_t>("Integers/signed32") == 43, 10000);
  CHECK_TIMEOUT(testFacility.readScalar<int>("Devices/Use2/status") == 0, 10000);

  testFacility.writeScalar<int16_t>("/Use12/Integers/signed16", 44);
  CHECK_TIMEOUT(raw1.read<int16_t>("Integers/signed16") == 44, 10000);
  CHECK_TIMEOUT(testFacility.readScalar<int>("Devices/Use12/status") == 0, 10000);

  testFacility.writeScalar("/Use5/Integers/signed32", 45);
  CHECK_TIMEOUT(raw5.read<int32_t>("Integers/signed32") == 45, 10000);
  CHECK_TIMEOUT(testFacility.readScalar<int>("Devices/Use5/status") == 0, 10000);

  // also set backend 1 into error state. Now only Use5 is working.
  auto dummy1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw1.getBackend());
  dummy1->throwExceptionOpen = true;
  dummy1->throwExceptionRead = true;

  trigger.write();
  // All devices but Use5 are in error state
  for(auto const* dev : {"Use1", "Use2", "Use12", "Use3", "Use4", "Use34"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }
  // Use5 is still working
  testFacility.writeScalar("/Use5/Integers/signed32", 46);
  CHECK_TIMEOUT(raw5.read<int32_t>("Integers/signed32") == 46, 10000);
  CHECK_TIMEOUT(testFacility.readScalar<int>("Devices/Use5/status") == 0, 10000);

  // Remove error condition on raw3
  dummy3->throwExceptionOpen = false;
  dummy3->throwExceptionRead = false;

  // Wait for Use3, Use4 and use34 to recover, while Use1, Use2 and Use12 are still in error state
  for(auto const* dev : {"Use3", "Use4", "Use34", "Use5"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
  for(auto const* dev : {"Use1", "Use2", "Use12"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }

  // Remove error condition on raw1 and recover everything
  dummy1->throwExceptionOpen = false;
  dummy1->throwExceptionRead = false;

  for(auto const* dev : {"Use1", "Use2", "Use12", "Use3", "Use4", "Use34", "Use5"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

struct TestApp2 : public TestApp1 {
  TestApp2() : TestApp1("twoRecoveryGroups") {}
  ~TestApp2() override { shutdown(); }

  // Add another device which "connects" the two recovery groups in App1.
  // Now there is no device which has both the backends raw1 and raw4, but they are in the same
  // recovery group.
  DeviceModuleWithPath mappedDev23{this, "Use23"};
};

/**********************************************************************************************************************/

BOOST_FIXTURE_TEST_CASE(Test2RecoveryGroups, Fixture<TestApp2>) {
  // pre-condition: all devices OK
  for(auto const* dev : {"Use1", "Use2", "Use12", "Use3", "Use4", "Use34", "Use5", "Use23"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // Put dummy1 into an error state. Now the whole large recovery group (except for Use5) go into error state.
  // In particular, also Use4, which is in no Device together with Use1.
  auto dummy1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw1.getBackend());
  dummy1->throwExceptionOpen = true;
  dummy1->throwExceptionRead = true;

  trigger.write();
  // All devices but Use5 are in error state
  for(auto const* dev : {"Use1", "Use2", "Use12", "Use3", "Use4", "Use34", "Use23"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }
  CHECK_TIMEOUT(testFacility.readScalar<int>("Devices/Use5/status") == 0, 10000);

  // Recovery
  dummy1->throwExceptionOpen = false;
  dummy1->throwExceptionRead = false;

  trigger.write();

  for(auto const* dev : {"Use1", "Use2", "Use12", "Use3", "Use4", "Use34", "Use5", "Use23"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

BOOST_FIXTURE_TEST_CASE(TestRecoverySteps, Fixture<TestApp1>) {
  // pre-condition: all (relevant) devices OK
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // Set values for some variables. They are restored during the recovery process.
  testFacility.writeScalar<uint32_t>("/Use1/Integers/unsigned32", 17);
  testFacility.writeScalar<uint32_t>("/Use2/Integers/unsigned32", 18);

  // Wait until they arrived, the overwrite them and the values set in the init script.
  CHECK_TIMEOUT(raw1.read<uint32_t>("/Integers/unsigned32") == 17, 10000);
  CHECK_TIMEOUT(raw2.read<uint32_t>("/Integers/unsigned32") == 18, 10000);
  raw1.write<uint32_t>("/Integers/unsigned32", 13);
  raw2.write<uint32_t>("/Integers/unsigned32", 14);
  raw1.write<int32_t>("/MyModule/actuator", 15);
  raw2.write<int32_t>("/MyModule/actuator", 16);

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

  // Stage 1: Although device 2 is ok, the init handler does not run because it is waiting at the barrier before the
  // init handlers (all backends must be successfully opened)
  CHECK_TIMEOUT(raw2.isFunctional(), 10000);
  usleep(100000); // Wait 100 ms for the init handler. It should not happen, so don't wait too long...
  BOOST_TEST(raw2.read<int32_t>("MyModule/actuator") == 16);

  // Stage 2: Resolve the error. Check that init handler 2 goes through, but no inital values are written yet.
  dummy1->throwExceptionOpen = false;
  dummy1->throwExceptionRead = false;
  // wait until Use1 is in the init handler
  testApp.arrivedInInitHandler.arrive_and_wait();
  assert(testApp.initCounter == 2);
  // We know one of the backends is closed when entering the init handler, so we have to re-open it.
  // As we don't know which one, we just open both.
  raw1.open();
  raw2.open();

  CHECK_TIMEOUT(
      (raw1.read<int32_t>("MyModule/actuator") == 1) || (raw2.read<int32_t>("MyModule/actuator") == 1), 10000);
  usleep(100000); // Wait 110 ms for the recovery values. It should not happen, so don't wait too long...
  BOOST_TEST(raw1.read<int32_t>("Integers/unsigned32") == 13); // recovery values not written
  BOOST_TEST(raw2.read<int32_t>("Integers/unsigned32") == 14); // recovery values not written

  // Stage 3: Release the blocking init handler and check that the initial values are restored.
  testApp.blockInit = false;

  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
  BOOST_TEST(raw2.read<int32_t>("MyModule/actuator") == 1);
  BOOST_TEST(raw1.read<int32_t>("MyModule/actuator") == 1);
  BOOST_TEST(raw1.read<int32_t>("Integers/unsigned32") == 17);
  BOOST_TEST(raw2.read<int32_t>("Integers/unsigned32") == 18);
}

/**********************************************************************************************************************/

BOOST_FIXTURE_TEST_CASE(TestInitFailure, Fixture<TestApp1>) {
  // This test is checking that the error condition of a failure in the init handler does
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

  // Wait until the failure counter is larger than 1 , i.e. the DeviceManager has actually failed at the place we want,
  // and the recovery process has restarted after it at least once (by checking that it failed twice).
  CHECK_TIMEOUT(testApp.failCounter > 1, 10000);

  // Resolve the error condition and wait until everything has recovered.
  testApp.failInit2 = false;
  for(auto const* dev : {"Use1", "Use2"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/

BOOST_FIXTURE_TEST_CASE(TestRecoveryWriteFailure, Fixture<TestApp1>) {
  // This test is checking that the error condition of a failure when writing the recovery accessors do
  // not confuse the barrier order and lock up the manager.

  // pre-condition: all (relevant) devices OK
  for(auto const* dev : {"Use3", "Use4"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }

  // Write something to Use3 so we can check when its recovery accessor writing is through.
  // We take Use3 this time because it does not have an init handler, so the DeviceManager
  // does not inadvertently close the device while we want to read it in the test.
  // In addition, Use34 only has read-only registers, so it does not cause errors in recovery,
  // which would set the device 3 back into error state.
  testFacility.writeScalar<uint32_t>("/Use3/Integers/unsigned32", 18);
  CHECK_TIMEOUT(raw3.read<uint32_t>("Integers/unsigned32") == 18, 10000);

  // create an error condition which also thrown when writing (the recovery accessors)
  auto dummy4 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(raw4.getBackend());
  dummy4->throwExceptionOpen = true;
  dummy4->throwExceptionRead = true;
  dummy4->throwExceptionWrite = true;
  trigger.write();

  // Wait until the error has been detected, then clean the recovery value on Use3 and
  // resolve the open/read errors, but keep the write error.
  for(auto const* dev : {"Use3", "Use4"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 1, 10000);
  }
  // Get the status accessors to check that Use3 does not report ready, although it
  // got through with its recovery.
  auto use3Status = testFacility.getScalar<int32_t>("Devices/Use3/status");
  use3Status.readLatest(); // just empty the queue
  auto use3BecameFunctional = testFacility.getVoid("Devices/Use3/deviceBecameFunctional");
  use3BecameFunctional.readLatest(); // just empty the queue

  // now clear the opening error.
  dummy4->throwExceptionOpen = false;
  dummy4->throwExceptionRead = false;

  // Wait until we can read/write from raw3 without error.
  CHECK_TIMEOUT(raw3.isFunctional(), 10000);
  raw3.write("Integers/unsigned32", 0);
  // Wait until we have seen the recovery section of Use3 run through, then clear it and wait again.
  // Like this we make sure that the error condition in Use4 has been hit at least once.
  CHECK_TIMEOUT(raw3.read<uint32_t>("Integers/unsigned32") == 18, 10000);

  raw3.write("Integers/unsigned32", 0);
  CHECK_TIMEOUT(raw3.read<uint32_t>("Integers/unsigned32") == 18, 10000);

  // No changing status has been reported in the mean time
  BOOST_TEST(!use3Status.readNonBlocking());
  BOOST_TEST(!use3BecameFunctional.readNonBlocking());

  // Finally, resolve the error condition and wait until everything recovers.
  dummy4->throwExceptionWrite = false;
  for(auto const* dev : {"Use3", "Use4"}) {
    CHECK_TIMEOUT(testFacility.readScalar<int>(std::string("Devices/") + dev + "/status") == 0, 10000);
  }
}

/**********************************************************************************************************************/
