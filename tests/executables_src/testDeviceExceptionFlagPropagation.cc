// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testDeviceExceptionFlagPropagation

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "PeriodicTrigger.h"
#include "TestFacility.h"
#include "VariableGroup.h"

#include <ChimeraTK/DummyRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS
using namespace boost::unit_test_framework;

namespace ctk = ChimeraTK;

constexpr std::string_view ExceptionDummyCDD1{"(ExceptionDummy:1?map=test3.map)"};

/*********************************************************************************************************************/

struct TestApplication : ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() override { shutdown(); }

  struct Name : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct Name2 : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarOutput<uint64_t> tick{this, "tick", "", ""};
    } name{(this), "name", ""}; // extra parentheses are for doxygen...

    void prepare() override { name.tick.write(); /* send initial value */ }
    void mainLoop() override {}
  } name{this, "name", ""};

  struct Module : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    mutable int readMode{0};

    struct Vars : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<uint64_t> tick{this, "/trigger/tick", "", ""};
      ctk::ScalarPollInput<int> read{this, "/MyModule/readBack", "", ""};
      ctk::ScalarOutput<int> set{this, "/MyModule/actuator", "", ""};
    } vars{this, ".", ""};

    std::atomic<ctk::DataValidity> readDataValidity{ctk::DataValidity::ok};

    void prepare() override {
      readDataValidity = vars.read.dataValidity();
      // The receiving end of all accessor implementations should be constructed with faulty (Initial value propagation
      // spec, D.1)
      BOOST_CHECK(readDataValidity == ctk::DataValidity::faulty);
      vars.set.write(); /* send initial value */
    }

    void mainLoop() override {
      readDataValidity = vars.read.dataValidity();
      while(true) {
        vars.tick.read();
        readDataValidity = vars.read.dataValidity();
        switch(readMode) {
          case 0:
            vars.read.readNonBlocking();
            break;
          case 1:
            vars.read.readLatest();
            break;
          case 2:
            vars.read.read();
            break;
          case 3:
            vars.set.write();
            break;
          case 4:
            vars.set.writeDestructively();
            break;
          default:
            break;
        }
      }
    }

  } module{this, "module", ""};

  ctk::DeviceModule dev{this, ExceptionDummyCDD1.data(), "/fakeTriggerToMakeUnusedPollRegsHappy"};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDirectConnectOpen) {
  for(int readMode = 0; readMode < 2; ++readMode) {
    std::cout << "testDirectConnectOpen (readMode = " << readMode << ")" << std::endl;

    TestApplication app;

    boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1.data()));

    ctk::TestFacility test(app, false);

    // Throw on device open and check if DataValidity::faulty gets propagated
    dummyBackend1->throwExceptionOpen = true;
    // set the read mode
    app.module.readMode = readMode;
    std::cout << "Read mode is: " << app.module.readMode << ". Run application.\n";

    // app.run();
    test.runApplication();

    CHECK_EQUAL_TIMEOUT(
        test.readScalar<int>("Devices/" + ctk::Utilities::escapeName(ExceptionDummyCDD1.data(), false) + "/status"), 1,
        10000);

    // Trigger and check
    test.writeScalar<uint64_t>("/trigger/tick", 1);
    usleep(10000);
    BOOST_CHECK(app.module.readDataValidity == ctk::DataValidity::faulty);

    // recover from error state
    dummyBackend1->throwExceptionOpen = false;
    CHECK_TIMEOUT(app.module.readDataValidity == ctk::DataValidity::ok, 10000);
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDirectConnectRead) {
  std::cout << "testDirectConnectRead" << std::endl;
  TestApplication app;
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1.data()));

  app.module.vars.tick = {&app.module.vars, "/trigger/tick", "", ""};

  app.getModel().writeGraphViz("testDirectConnectRead.dot");

  app.debugMakeConnections();

  ctk::TestFacility test(app);
  test.runApplication();

  // Advance through all read methods
  while(app.module.readMode < 3) {
    // Check
    test.writeScalar<uint64_t>("/trigger/tick", 1);
    test.stepApplication();
    BOOST_CHECK(app.module.vars.read.dataValidity() == ctk::DataValidity::ok);

    // Check
    std::cout << "Checking read mode " << app.module.readMode << "\n";
    dummyBackend1->throwExceptionRead = true;
    test.writeScalar<uint64_t>("/trigger/tick", 1);
    test.stepApplication(false);
    BOOST_CHECK(app.module.vars.read.dataValidity() == ctk::DataValidity::faulty);

    // Reset throwing and let the device recover
    dummyBackend1->throwExceptionRead = false;
    test.stepApplication(true);

    // advance to the next read
    app.module.readMode++;
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDirectConnectWrite) {
  std::cout << "testDirectConnectWrite" << std::endl;
  TestApplication app;
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1.data()));

  app.module.readMode = 3;

  ctk::TestFacility test(app);
  test.runApplication();

  // Advance through all non-blocking read methods
  while(app.module.readMode < 5) {
    // Check
    test.writeScalar<uint64_t>("/trigger/tick", 1);
    test.stepApplication();
    BOOST_CHECK(app.module.vars.set.dataValidity() == ctk::DataValidity::ok);

    // Check
    dummyBackend1->throwExceptionWrite = true;
    test.writeScalar<uint64_t>("/trigger/tick", 1);
    test.stepApplication(false);
    // write operations failing does not invalidate data
    BOOST_CHECK(app.module.vars.set.dataValidity() == ctk::DataValidity::ok);

    // advance to the next read
    dummyBackend1->throwExceptionWrite = false;
    app.module.readMode++;
  }
}

/*********************************************************************************************************************/
