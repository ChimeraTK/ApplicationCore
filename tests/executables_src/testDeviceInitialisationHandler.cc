// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#define BOOST_TEST_MODULE testDeviceInitialisationHandler

#include "Application.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "TestFacility.h"
#include "Utilities.h"

#include <ChimeraTK/Device.h>
#include <ChimeraTK/ExceptionDummyBackend.h>

#include <boost/test/included/unit_test.hpp>

#include <cstdlib>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

namespace Tests::testDeviceInitialisationHandler {

  static const std::string deviceCDD{"(ExceptionDummy?map=test.map)"};
  static const std::string exceptionMessage{"DEBUG: runtime error intentionally cased in device initialisation"};

  static std::atomic<bool> throwInInitialisation{false};
  static std::atomic<int32_t> var1{0};
  static std::atomic<int32_t> var2{0};
  static std::atomic<int32_t> var3{0};

  void initialiseReg1(ChimeraTK::Device&) {
    var1 = 42;
    if(throwInInitialisation) {
      throw ctk::runtime_error(exceptionMessage);
    }
  }

  void initialiseReg2(ChimeraTK::Device&) {
    // the initialisation of reg 2 must happen after the initialisation of reg1
    var2 = var1 + 5;
  }

  void initialiseReg3(ChimeraTK::Device&) {
    // the initialisation of reg 3 must happen after the initialisation of reg2
    var3 = var2 + 5;
  }

  /* dummy application */
  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {}
    ~TestApplication() override { shutdown(); }

    ctk::DeviceModule dev{this, deviceCDD, "", &initialiseReg1};
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(TestBasicInitialisation) {
    std::cout << "testBasicInitialisation" << std::endl;
    TestApplication app;

    var1 = 0;
    var2 = 0;
    var3 = 0;

    ctk::TestFacility test{app};
    test.runApplication();
    ctk::Device dummy;
    dummy.open(deviceCDD);

    // ********************************************************
    // REQUIRED TEST 1: After opening the device is initialised
    // ********************************************************
    BOOST_CHECK_EQUAL(var1, 42);

    var1 = 0;

    // check that accessing an exception triggers a reconnection with re-initialisation
    auto dummyBackend =
        boost::dynamic_pointer_cast<ctk::ExceptionDummy>(ctk::BackendFactory::getInstance().createBackend(deviceCDD));
    dummyBackend->throwExceptionWrite = true;

    auto reg2_cs = test.getScalar<int32_t>("/REG2");
    reg2_cs = 19;
    reg2_cs.write();
    test.stepApplication(false);

    BOOST_CHECK_EQUAL(var2, 0);
    BOOST_CHECK_EQUAL(var1, 0);
    dummyBackend->throwExceptionWrite = false; // now the device should work again and be re-initialised

    reg2_cs = 20;
    reg2_cs.write();
    test.stepApplication();

    BOOST_CHECK_EQUAL(dummy.read<int32_t>("/REG2"), 20);

    // ****************************************************************
    // REQUIRED TEST 2: After an exception the device is re-initialised
    // ****************************************************************
    BOOST_CHECK_EQUAL(var1, 42);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(TestMultipleInitialisationHandlers) {
    std::cout << "testMultipleInitialisationHandlers" << std::endl;
    TestApplication app;

    var1 = 0;
    var2 = 0;
    var3 = 0;

    app.dev.addInitialisationHandler(&initialiseReg2);
    app.dev.addInitialisationHandler(&initialiseReg3);
    ctk::TestFacility test{app};
    test.runApplication();

    auto deviceStatus = test.getScalar<int32_t>(
        ctk::RegisterPath("/Devices") / ctk::Utilities::escapeName(deviceCDD, false) / "status");

    // *********************************************************
    // REQUIRED TEST 4: Handlers are executed in the right order
    // *********************************************************
    BOOST_CHECK_EQUAL(var1, 42);
    BOOST_CHECK_EQUAL(var2, 47); // the initialiser used reg1+5, so order matters
    BOOST_CHECK_EQUAL(var3, 52); // the initialiser used reg2+5, so order matters

    // check that after an exception the re-initialisation is OK
    var1 = 0;
    var2 = 0;
    var3 = 0;

    // cause an exception
    auto dummyBackend =
        boost::dynamic_pointer_cast<ctk::ExceptionDummy>(ctk::BackendFactory::getInstance().createBackend(deviceCDD));
    dummyBackend->throwExceptionWrite = true;

    auto reg4_cs = test.getScalar<int32_t>("/REG4");
    reg4_cs = 19;
    reg4_cs.write();
    test.stepApplication(false);

    // recover
    dummyBackend->throwExceptionWrite = false;

    reg4_cs = 20;
    reg4_cs.write();
    test.stepApplication();

    BOOST_CHECK_EQUAL(var1, 42);
    BOOST_CHECK_EQUAL(var2, 47); // the initialiser used reg1+5, so order matters
    BOOST_CHECK_EQUAL(var3, 52); // the initialiser used reg2+5, so order matters
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(TestInitialisationException) {
    std::cout << "testInitialisationException" << std::endl;

    var1 = 0;
    var2 = 0;
    var3 = 0;

    throwInInitialisation = true;
    TestApplication app;

    app.dev.addInitialisationHandler(&initialiseReg2);
    app.dev.addInitialisationHandler(&initialiseReg3);
    // app.dev.connectTo(app.cs);
    ctk::TestFacility test(app, false); // test facility without testable mode

    auto status = test.getScalar<int32_t>(
        ctk::RegisterPath("/Devices") / ctk::Utilities::escapeName(std::string(deviceCDD), false) / "status");
    auto status_message = test.getScalar<std::string>(
        ctk::RegisterPath("/Devices") / ctk::Utilities::escapeName(std::string(deviceCDD), false) / "status_message");

    ctk::Device dummy;
    dummy.open(deviceCDD);

    // We cannot use runApplication because the DeviceModule leaves the testable mode without variables in the queue,
    // but has not finished error handling yet. In this special case we cannot make the programme continue, because
    // stepApplication only works if the queues are not empty. We have to work with timeouts here (until someone comes
    // up with a better idea)
    app.run();
    // app.dumpConnections();

    // initial values of device status
    BOOST_TEST(status.readAndGet() == 1);
    auto initialMessage = status_message.readAndGet();

    // wait for exception, message shall be changed
    status_message.read();
    BOOST_TEST(std::string(status_message) != initialMessage);

    // Check that the execution of init handlers was stopped after the exception:
    // initialiseReg2 and initialiseReg3 were not executed. As we already checked with timeout that the
    // initialisation error has been reported, we know that the data was written to the device and don't need the
    // timeout here.

    usleep(10000); // allow bugs to manifest (e.g. if manager would continue with other handlers after exception)

    BOOST_CHECK_EQUAL(var1, 42);
    BOOST_CHECK_EQUAL(var2, 0);
    BOOST_CHECK_EQUAL(var3, 0);

    // recover the error
    throwInInitialisation = false;

    // wait until the device is reported to be OK again (check with timeout),
    // then check the initialisation (again, no extra timeout needed because of the logic:
    // success is only reported after successful init).
    CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>(
                            ctk::RegisterPath("/Devices") / ctk::Utilities::escapeName(deviceCDD, false) / "status"),
        0, 30000);
    // We use the macro here for convenience, it's a test, speed should not matter
    // NOLINTNEXTLINE(readability-container-size-empty)
    CHECK_EQUAL_TIMEOUT(test.readScalar<std::string>(ctk::RegisterPath("/Devices") /
                            ctk::Utilities::escapeName(deviceCDD, false) / "status_message"),
        "", 30000);

    // initialisation should be correct now
    BOOST_CHECK_EQUAL(var1, 42);
    BOOST_CHECK_EQUAL(var2, 47);
    BOOST_CHECK_EQUAL(var3, 52);

    // now check that the initialisation error is also reported when recovering
    // Prepare registers to be initialised
    var1 = 12;
    var2 = 13;
    var3 = 14;

    // Make initialisation fail when executed, and then cause an error condition
    throwInInitialisation = true;
    auto dummyBackend =
        boost::dynamic_pointer_cast<ctk::ExceptionDummy>(ctk::BackendFactory::getInstance().createBackend(deviceCDD));
    dummyBackend->throwExceptionWrite = true;

    auto reg4_cs = test.getScalar<int32_t>("/REG4");
    reg4_cs = 20;
    reg4_cs.write();

    CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>(
                            ctk::RegisterPath("/Devices") / ctk::Utilities::escapeName(deviceCDD, false) / "status"),
        1, 30000);
    // First we see the message from the failing write
    CHECK_TIMEOUT(!test
                      .readScalar<std::string>(ctk::RegisterPath("/Devices") /
                          ctk::Utilities::escapeName(deviceCDD, false) / "status_message")
                      .empty(),
        30000);
    dummyBackend->throwExceptionWrite = false;
    // Afterwards we see a message from the failing initialisation (which we can now distinguish from the original write
    // exception because write does not throw any more)
    CHECK_EQUAL_TIMEOUT(test.readScalar<std::string>(ctk::RegisterPath("/Devices") /
                            ctk::Utilities::escapeName(deviceCDD, false) / "status_message"),
        exceptionMessage, 30000);

    // Now fix the initialisation error and check that the device comes up.
    throwInInitialisation = false;
    // Wait until the device is OK again
    CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>(
                            ctk::RegisterPath("/Devices") / ctk::Utilities::escapeName(deviceCDD, false) / "status"),
        0, 30000);

    // We use the macro here for convenience, it's a test, speed should not matter
    // NOLINTNEXTLINE(readability-container-size-empty)
    CHECK_EQUAL_TIMEOUT(test.readScalar<std::string>(ctk::RegisterPath("/Devices") /
                            ctk::Utilities::escapeName(deviceCDD, false) / "status_message"),
        "", 30000);
    // Finally check that the 20 arrives on the device
    CHECK_EQUAL_TIMEOUT(dummy.read<int32_t>("/REG4"), 20, 30000);
  }

  /********************************************************************************************************************/
  /**
   * \anchor testExceptionHandling_b_3_2_2_1 \ref exceptionHandling_b_3_2_2_1 "B.3.2.2.1"
   * The device is closed before the initialisation handler is called.
   */

  BOOST_AUTO_TEST_CASE(TestDeviceClosedInInitHandler) {
    // Check that the device has been closed when the init handler is called.
    std::cout << "TestDeviceClosedInInitHandler" << std::endl;

    TestApplication app;
    // Cache the opened state in the init handler in a variable. BOOST_CHECK is not threat safe and
    // cannot directly be used in the handler.
    bool isOpenedInInitHandler{
        true}; // We expect false, so we set the starting value to true to know the test is sensitive.
    app.dev.addInitialisationHandler([&](ctk::Device& d) { isOpenedInInitHandler = d.isOpened(); });

    ctk::TestFacility testFacility(app);
    testFacility.runApplication();
    // The testFacility in testable mode guarantees that the device has been opened at this point. So we know the init
    // handler with the test has been run at this point.
    BOOST_CHECK(!isOpenedInInitHandler);
  }

} // namespace Tests::testDeviceInitialisationHandler
