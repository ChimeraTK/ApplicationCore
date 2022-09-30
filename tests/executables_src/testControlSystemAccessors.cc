// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <future>

#define BOOST_TEST_MODULE testControlSystemAccessors

#include "Application.h"
#include "ApplicationModule.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>
#include <ChimeraTK/ControlSystemAdapter/DevicePVManager.h>
#include <ChimeraTK/ControlSystemAdapter/PVManager.h>

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread/barrier.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

// list of user types the accessors are tested with
typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double> test_types;

#define CHECK_TIMEOUT(condition, maxMilliseconds)                                                                      \
  {                                                                                                                    \
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();                                       \
    while(!(condition)) {                                                                                              \
      bool timeout_reached = (std::chrono::steady_clock::now() - t0) > std::chrono::milliseconds(maxMilliseconds);     \
      BOOST_CHECK(!timeout_reached);                                                                                   \
      if(timeout_reached) break;                                                                                       \
      usleep(1000);                                                                                                    \
    }                                                                                                                  \
  }

/*********************************************************************************************************************/
/* the ApplicationModule for the test is a template of the user type */

template<typename T>
struct TestModule : public ctk::ApplicationModule {
  TestModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags = {})
  : ApplicationModule(owner, name, description, tags), mainLoopStarted(2) {}

  ctk::ScalarPushInput<T> consumer{this, "consumer", "", "No comment."};
  ctk::ScalarOutput<T> feeder{this, "feeder", "MV/m", "Some fancy explanation about this variable"};

  // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
  // mainLoop(). This is required since the mainLoopWrapper accesses the module variables before the start of the
  // mainLoop.
  // execute this right after the Application::run():
  //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
  boost::barrier mainLoopStarted;

  void mainLoop() { mainLoopStarted.wait(); }
};

/*********************************************************************************************************************/
/* dummy application */

template<typename T>
struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {
    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");
  }
  ~TestApplication() { shutdown(); }

  TestModule<T> testModule{this, "TestModule", "The test module"};

};

/*********************************************************************************************************************/
/* test feeding a scalar to the control system adapter */

BOOST_AUTO_TEST_CASE_TEMPLATE(testFeedToCS, T, test_types) {
  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // app.testModule.feeder >> app.cs("myFeeder");
  app.initialise();

  auto myFeeder = pvManagers.first->getProcessArray<T>("/TestModule/feeder");
  auto consumer = pvManagers.first->getProcessArray<T>("/TestModule/consumer");

  app.run();
  consumer->write();                     // send initial value so the application module can start up
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  BOOST_TEST(myFeeder->getName() == "/TestModule/feeder");
  BOOST_TEST(myFeeder->getUnit() == "MV/m");
  BOOST_TEST(myFeeder->getDescription() == "The test module - Some fancy explanation about this variable");

  app.testModule.feeder = 42;
  BOOST_CHECK_EQUAL(myFeeder->readNonBlocking(), false);
  app.testModule.feeder.write();
  BOOST_CHECK_EQUAL(myFeeder->readNonBlocking(), true);
  BOOST_CHECK_EQUAL(myFeeder->readNonBlocking(), false);
  BOOST_CHECK(myFeeder->accessData(0) == 42);

  app.testModule.feeder = 120;
  BOOST_CHECK_EQUAL(myFeeder->readNonBlocking(), false);
  app.testModule.feeder.write();
  BOOST_CHECK_EQUAL(myFeeder->readNonBlocking(), true);
  BOOST_CHECK_EQUAL(myFeeder->readNonBlocking(), false);
  BOOST_CHECK(myFeeder->accessData(0) == 120);
}

/*********************************************************************************************************************/
/* test consuming a scalar from the control system adapter */

BOOST_AUTO_TEST_CASE_TEMPLATE(testConsumeFromCS, T, test_types) {
  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // app.cs("myConsumer") >> app.testModule.consumer;
  app.initialise();

  auto myConsumer = pvManagers.first->getProcessArray<T>("/TestModule/consumer");
  BOOST_TEST(myConsumer->getName() == "/TestModule/consumer");
  BOOST_TEST(myConsumer->getUnit() == "");
  BOOST_TEST(myConsumer->getDescription() == "The test module - No comment.");

  myConsumer->accessData(0) = 123; // set inital value
  myConsumer->write();

  app.run();                             // should propagate initial value
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  BOOST_CHECK(app.testModule.consumer == 123); // check initial value

  myConsumer->accessData(0) = 42;
  myConsumer->write();
  app.testModule.consumer.read();
  BOOST_CHECK(app.testModule.consumer == 42);

  myConsumer->accessData(0) = 120;
  myConsumer->write();
  app.testModule.consumer.read();
  BOOST_CHECK(app.testModule.consumer == 120);
}

/*********************************************************************************************************************/
