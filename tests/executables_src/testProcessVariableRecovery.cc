// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testProcessVariableRecovery

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/Device.h>
#include <ChimeraTK/ExceptionDummyBackend.h>

#include <boost/thread/barrier.hpp>

#include <cstdlib>
#include <regex>

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

namespace Tests::testProcessVariableRecovery {

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;

  static constexpr std::string_view deviceCDD{"(ExceptionDummy?map=test5.map)"};

  /* The test module is writing to the device. It is the "module under test".
   * This is the one whose variables are to be recovered. It is not the place the the
   * application first sees the exception.
   */
  struct TestModule : public ctk::ApplicationModule {
    TestModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, tags), mainLoopStarted(2) {}

    ctk::ScalarPushInput<int32_t> trigger{this, "trigger", "", "This is my trigger."};
    ctk::ScalarOutput<int32_t> scalarOutput{this, "TO_DEV_SCALAR1", "", "Here I write a scalar"};
    ctk::ArrayOutput<int32_t> arrayOutput{this, "TO_DEV_ARRAY1", "", 4, "Here I write an array"};

    // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
    // mainLoop(). This is required to make sure the initial value propagation is done.
    // execute this right after the Application::run():
    //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
    boost::barrier mainLoopStarted;

    void mainLoop() override {
      mainLoopStarted.wait();

      while(true) {
        scalarOutput = int32_t(trigger);
        scalarOutput.write();
        for(uint i = 0; i < 4; i++) {
          arrayOutput[i] = int32_t(trigger);
        }
        arrayOutput.write();
        trigger.read(); // read the blocking variable at the end so the initial values are propagated
      }
    }
  };

  /* dummy application */
  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {}
    ~TestApplication() override { shutdown(); }

    ctk::DeviceModule dev{this, deviceCDD.data(), "/deviceTrigger"};
    TestModule module{this, "TEST", "The test module"};
  };

  /*
   * Test application for the specific case of writing to a read-only accessor. Provides an input to an ApplicationModule
   * from a read-only accessor of the device. For the test, the accessor must not be routed through the control system,
   * the illegal write would be caught by the ControlSystemAdapter, not by the ExceptionHandlingDecorator under test here.
   */
  struct ReadOnlyTestApplication : public ctk::Application {
    ReadOnlyTestApplication() : Application("ReadOnlytestApp") {}
    ~ReadOnlyTestApplication() override { shutdown(); }

    ctk::DeviceModule dev{this, deviceCDD.data(), "/weNowNeedATriggerHere"};

    struct TestModule : public ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ScalarPushInput<int> start{
          this, "startTest", "", "This has to be written once, before writing to the device", {"CS"}};
      ctk::ScalarPollInput<int32_t> scalarROInput{
          this, "/TEST/FROM_DEV_SCALAR2", "", "Here I read from a scalar RO-register"};

      void mainLoop() override {
        // Just to have a blocking read, gives the test time to dumpConnections and explicitly trigger before terminating.
        start.read();

        scalarROInput = 42;
        try {
          scalarROInput.write();
          BOOST_CHECK_MESSAGE(
              false, "ReadOnlyTestApplication: Calling write() on input to read-only device register did not throw.");
        }
        catch(ChimeraTK::logic_error& e) {
          const std::string exMsg{e.what()};
          std::regex exceptionHandlingRegex{"^ChimeraTK::ExceptionhandlingDecorator:*"};
          std::smatch exMatch;

          std::cout << exMsg << std::endl;

          BOOST_CHECK(std::regex_search(exMsg, exMatch, exceptionHandlingRegex));
        }
      }

    } module{this, "READ_ONLY_TEST", "The test module"};
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testWriteToReadOnly) {
    std::cout << "testWriteToReadOnly" << std::endl;

    ReadOnlyTestApplication app;

    ctk::TestFacility test{app};
    app.optimiseUnmappedVariables({"/TEST/FROM_DEV_SCALAR2"});

    test.runApplication();

    // Should trigger the blocking read in ReadOnlyTestApplication's ApplicationModule. It then writes to a read-only
    // register of the device, which should throw. Check is done in the module's mainLoop. We can not check here, as the
    // exception gets thrown in the thread of the module.
    test.writeScalar("/READ_ONLY_TEST/startTest", 1);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testProcessVariableRecovery) {
    std::cout << "testProcessVariableRecovery" << std::endl;
    TestApplication app;

    ctk::TestFacility test{app, false};
    // Write initial values manually since we do not use the testable mode.
    // Otherwise the main loops never start.

    // initial value for the direct CS->DEV register
    test.writeScalar("/TEST/TO_DEV_SCALAR2", 42);
    std::vector<int32_t> array = {99, 99, 99, 99};
    test.writeArray("/TEST/TO_DEV_ARRAY2", array);

    // initial value for the trigger
    test.writeScalar("/TEST/trigger", 0);

    app.run();
    app.module.mainLoopStarted.wait();

    ctk::Device dummy;
    dummy.open(deviceCDD.data());

    // wait for the device to be opened successfully so the access to the dummy does not throw
    // (as they use the same backend it now throws if there has been an exception somewhere else)
    CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>(
                            std::string("/Devices/") + ctk::Utilities::escapeName(deviceCDD.data(), false) + "/status"),
        0, 10000);

    // Check that the initial values are there.
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_SCALAR2"), 42, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY2", 1, 0)[0], 99, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY2", 1, 1)[0], 99, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY2", 1, 2)[0], 99, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY2", 1, 3)[0], 99, 10000);

    // Update device register via application module.
    auto trigger = test.getScalar<int32_t>("/TEST/trigger");
    trigger = 100;
    trigger.write();
    // Check if the values are updated.
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_SCALAR1"), 100, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY1", 1, 0)[0], 100, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY1", 1, 1)[0], 100, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY1", 1, 2)[0], 100, 10000);
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/TEST/TO_DEV_ARRAY1", 1, 3)[0], 100, 10000);

    auto dummyBackend = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
        ctk::BackendFactory::getInstance().createBackend(deviceCDD.data()));

    // Set the device to throw.
    dummyBackend->throwExceptionOpen = true;

    // Set dummy registers to 0.
    dummy.write<int32_t>("/CONSTANT/VAR32", 0);
    dummy.write<int32_t>("/TEST/TO_DEV_SCALAR1", 0);
    dummy.write<int32_t>("/TEST/TO_DEV_SCALAR2", 0);
    array = {0, 0, 0, 0};
    dummy.write("/TEST/TO_DEV_ARRAY1", array);
    dummy.write("/TEST/TO_DEV_ARRAY2", array);

    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/CONSTANT/VAR32"), 0, 10000);
    dummyBackend->throwExceptionWrite = true;
    dummyBackend->throwExceptionRead = true;

    // Now we trigger the reading module. This should put the device into an error state
    auto trigger2 = test.getVoid("/deviceTrigger");
    trigger2.write();

    // Verify that the device is in error state.
    CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>(ctk::RegisterPath("/Devices") /
                            ctk::Utilities::escapeName(deviceCDD.data(), false) / "status"),
        1, 10000);

    // Set device back to normal.
    dummyBackend->throwExceptionWrite = false;
    dummyBackend->throwExceptionRead = false;
    dummyBackend->throwExceptionOpen = false;
    // Verify if the device is ready.
    CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>(ctk::RegisterPath("/Devices") /
                            ctk::Utilities::escapeName(deviceCDD.data(), false) / "status"),
        0, 10000);

    // Device should have the correct values now. Notice that we did not trigger the writer module!
    BOOST_CHECK_EQUAL(dummy.read<int32_t>("/TEST/TO_DEV_SCALAR2"), 42);
    BOOST_CHECK((dummy.read<int32_t>("/TEST/TO_DEV_ARRAY2", 0) == std::vector<int32_t>{99, 99, 99, 99}));

    BOOST_CHECK_EQUAL(dummy.read<int32_t>("/TEST/TO_DEV_SCALAR1"), 100);
    BOOST_CHECK((dummy.read<int32_t>("/TEST/TO_DEV_ARRAY1", 0) == std::vector<int32_t>{100, 100, 100, 100}));
  }

} // namespace Tests::testProcessVariableRecovery
