// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <future>

#define BOOST_TEST_MODULE testControlSystemAccessors

#include "Application.h"
#include "ApplicationModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>
#include <ChimeraTK/ControlSystemAdapter/DevicePVManager.h>
#include <ChimeraTK/ControlSystemAdapter/PVManager.h>
#include <ChimeraTK/Device.h>

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

  ctk::DeviceModule dev{this, "Dummy0"};
};

/*********************************************************************************************************************/
/* test feeding a scalar to the control system adapter */

BOOST_AUTO_TEST_CASE_TEMPLATE(testFeedToCS, T, test_types) {
  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // app.testModule.feeder >> app.cs("myFeeder");
  app.initialise();
  app.run();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  auto myFeeder = pvManagers.first->getProcessArray<T>("/myFeeder");
  BOOST_CHECK(myFeeder->getName() == "/myFeeder");
  BOOST_CHECK(myFeeder->getUnit() == "MV/m");
  BOOST_CHECK(myFeeder->getDescription() == "The test module - Some fancy explanation about this variable");

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

  auto myConsumer = pvManagers.first->getProcessArray<T>("/myConsumer");
  BOOST_CHECK(myConsumer->getName() == "/myConsumer");
  BOOST_CHECK(myConsumer->getUnit() == "");
  BOOST_CHECK(myConsumer->getDescription() == "The test module - No comment.");

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
/* test multiple publications of the same variable */

BOOST_AUTO_TEST_CASE_TEMPLATE(testMultiplePublications, T, test_types) {
  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // app.testModule.feeder >> app.cs("myFeeder0");
  // app.testModule.feeder >> app.cs("myFeeder1");
  // app.testModule.feeder >> app.cs("myFeeder2");
  // app.testModule.feeder >> app.cs("myFeeder3");
  app.initialise();
  app.run();                             // make the connections and start the FanOut threads
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  auto myFeeder0 = pvManagers.first->getProcessArray<T>("/myFeeder0");
  auto myFeeder1 = pvManagers.first->getProcessArray<T>("/myFeeder1");
  auto myFeeder2 = pvManagers.first->getProcessArray<T>("/myFeeder2");
  auto myFeeder3 = pvManagers.first->getProcessArray<T>("/myFeeder3");

  BOOST_CHECK(myFeeder0->getName() == "/myFeeder0");
  BOOST_CHECK(myFeeder0->getUnit() == "MV/m");
  BOOST_CHECK(myFeeder0->getDescription() == "The test module - Some fancy explanation about this variable");

  BOOST_CHECK(myFeeder1->getName() == "/myFeeder1");
  BOOST_CHECK(myFeeder1->getUnit() == "MV/m");
  BOOST_CHECK(myFeeder1->getDescription() == "The test module - Some fancy explanation about this variable");

  BOOST_CHECK(myFeeder2->getName() == "/myFeeder2");
  BOOST_CHECK(myFeeder2->getUnit() == "MV/m");
  BOOST_CHECK(myFeeder2->getDescription() == "The test module - Some fancy explanation about this variable");

  BOOST_CHECK(myFeeder3->getName() == "/myFeeder3");
  BOOST_CHECK(myFeeder3->getUnit() == "MV/m");
  BOOST_CHECK(myFeeder3->getDescription() == "The test module - Some fancy explanation about this variable");

  app.testModule.feeder = 42;
  BOOST_CHECK(myFeeder0->readNonBlocking() == false);
  BOOST_CHECK(myFeeder1->readNonBlocking() == false);
  BOOST_CHECK(myFeeder2->readNonBlocking() == false);
  BOOST_CHECK(myFeeder3->readNonBlocking() == false);
  app.testModule.feeder.write();
  usleep(200000);
  BOOST_CHECK(myFeeder0->readNonBlocking() == true);
  BOOST_CHECK(myFeeder1->readNonBlocking() == true);
  BOOST_CHECK(myFeeder2->readNonBlocking() == true);
  BOOST_CHECK(myFeeder3->readNonBlocking() == true);
  BOOST_CHECK(myFeeder0->accessData(0) == 42);
  BOOST_CHECK(myFeeder1->accessData(0) == 42);
  BOOST_CHECK(myFeeder2->accessData(0) == 42);
  BOOST_CHECK(myFeeder3->accessData(0) == 42);
  BOOST_CHECK(myFeeder0->readNonBlocking() == false);
  BOOST_CHECK(myFeeder1->readNonBlocking() == false);
  BOOST_CHECK(myFeeder2->readNonBlocking() == false);
  BOOST_CHECK(myFeeder3->readNonBlocking() == false);

  app.testModule.feeder = 120;
  BOOST_CHECK(myFeeder0->readNonBlocking() == false);
  BOOST_CHECK(myFeeder1->readNonBlocking() == false);
  BOOST_CHECK(myFeeder2->readNonBlocking() == false);
  BOOST_CHECK(myFeeder3->readNonBlocking() == false);
  app.testModule.feeder.write();
  usleep(200000);
  BOOST_CHECK(myFeeder0->readNonBlocking() == true);
  BOOST_CHECK(myFeeder1->readNonBlocking() == true);
  BOOST_CHECK(myFeeder2->readNonBlocking() == true);
  BOOST_CHECK(myFeeder3->readNonBlocking() == true);
  BOOST_CHECK(myFeeder0->accessData(0) == 120);
  BOOST_CHECK(myFeeder1->accessData(0) == 120);
  BOOST_CHECK(myFeeder2->accessData(0) == 120);
  BOOST_CHECK(myFeeder3->accessData(0) == 120);
  BOOST_CHECK(myFeeder0->readNonBlocking() == false);
  BOOST_CHECK(myFeeder1->readNonBlocking() == false);
  BOOST_CHECK(myFeeder2->readNonBlocking() == false);
  BOOST_CHECK(myFeeder3->readNonBlocking() == false);

  // resend same number
  BOOST_CHECK(myFeeder0->readNonBlocking() == false);
  BOOST_CHECK(myFeeder1->readNonBlocking() == false);
  BOOST_CHECK(myFeeder2->readNonBlocking() == false);
  BOOST_CHECK(myFeeder3->readNonBlocking() == false);
  app.testModule.feeder.write();
  usleep(200000);
  BOOST_CHECK(myFeeder0->readNonBlocking() == true);
  BOOST_CHECK(myFeeder1->readNonBlocking() == true);
  BOOST_CHECK(myFeeder2->readNonBlocking() == true);
  BOOST_CHECK(myFeeder3->readNonBlocking() == true);
  BOOST_CHECK(myFeeder0->accessData(0) == 120);
  BOOST_CHECK(myFeeder1->accessData(0) == 120);
  BOOST_CHECK(myFeeder2->accessData(0) == 120);
  BOOST_CHECK(myFeeder3->accessData(0) == 120);
  BOOST_CHECK(myFeeder0->readNonBlocking() == false);
  BOOST_CHECK(myFeeder1->readNonBlocking() == false);
  BOOST_CHECK(myFeeder2->readNonBlocking() == false);
  BOOST_CHECK(myFeeder3->readNonBlocking() == false);
}

/*********************************************************************************************************************/
/* test multiple re-publications of a variable fed from the control system */

BOOST_AUTO_TEST_CASE_TEMPLATE(testMultipleRePublications, T, test_types) {
  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // app.cs("myConsumer") >> app.testModule.consumer;
  // app.testModule.consumer >> app.cs("myConsumer_copy1");
  // app.testModule.consumer >> app.cs("myConsumer_copy2");
  // app.testModule.consumer >> app.cs("myConsumer_copy3");

  app.initialise();

  auto myConsumer = pvManagers.first->getProcessArray<T>("/myConsumer");
  auto myConsumer_copy1 = pvManagers.first->getProcessArray<T>("/myConsumer_copy1");
  auto myConsumer_copy2 = pvManagers.first->getProcessArray<T>("/myConsumer_copy2");
  auto myConsumer_copy3 = pvManagers.first->getProcessArray<T>("/myConsumer_copy3");

  myConsumer->accessData(0) = 66; // set inital value
  myConsumer->write();

  app.run();                             // make the connections, start the FanOut threads and propagate initial value
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  BOOST_CHECK(app.testModule.consumer == 66);
  myConsumer_copy1->read();
  myConsumer_copy2->read();
  myConsumer_copy3->read();
  BOOST_CHECK(myConsumer_copy1->accessData(0) == 66);
  BOOST_CHECK(myConsumer_copy2->accessData(0) == 66);
  BOOST_CHECK(myConsumer_copy3->accessData(0) == 66);
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == false);

  BOOST_CHECK(myConsumer->getName() == "/myConsumer");
  BOOST_CHECK(myConsumer->getUnit() == "");
  BOOST_CHECK(myConsumer->getDescription() == "The test module - No comment.");

  BOOST_CHECK(myConsumer_copy1->getName() == "/myConsumer_copy1");
  BOOST_CHECK(myConsumer_copy1->getUnit() == "");
  BOOST_CHECK(myConsumer_copy1->getDescription() == "The test module - No comment.");

  BOOST_CHECK(myConsumer_copy2->getName() == "/myConsumer_copy2");
  BOOST_CHECK(myConsumer_copy2->getUnit() == "");
  BOOST_CHECK(myConsumer_copy2->getDescription() == "The test module - No comment.");

  BOOST_CHECK(myConsumer_copy3->getName() == "/myConsumer_copy3");
  BOOST_CHECK(myConsumer_copy3->getUnit() == "");
  BOOST_CHECK(myConsumer_copy3->getDescription() == "The test module - No comment.");

  myConsumer->accessData(0) = 42;
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == false);
  myConsumer->write();
  usleep(200000);
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy1->accessData(0) == 42);
  BOOST_CHECK(myConsumer_copy2->accessData(0) == 42);
  BOOST_CHECK(myConsumer_copy3->accessData(0) == 42);
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == false);
  app.testModule.consumer.read();
  BOOST_CHECK(app.testModule.consumer == 42);

  myConsumer->accessData(0) = 120;
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == false);
  myConsumer->write();
  usleep(200000);
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy1->accessData(0) == 120);
  BOOST_CHECK(myConsumer_copy2->accessData(0) == 120);
  BOOST_CHECK(myConsumer_copy3->accessData(0) == 120);
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == false);
  app.testModule.consumer.read();
  BOOST_CHECK(app.testModule.consumer == 120);

  // resend same number
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == false);
  myConsumer->write();
  usleep(200000);
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == true);
  BOOST_CHECK(myConsumer_copy1->accessData(0) == 120);
  BOOST_CHECK(myConsumer_copy2->accessData(0) == 120);
  BOOST_CHECK(myConsumer_copy3->accessData(0) == 120);
  BOOST_CHECK(myConsumer_copy1->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy2->readNonBlocking() == false);
  BOOST_CHECK(myConsumer_copy3->readNonBlocking() == false);
  app.testModule.consumer.read();
  BOOST_CHECK(app.testModule.consumer == 120);
}

/*********************************************************************************************************************/
/* test direct control system to control system connections */

BOOST_AUTO_TEST_CASE_TEMPLATE(testDirectCStoCS, T, test_types) {
  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // app.cs("mySender", typeid(T), 1) >> app.cs("myReceiver");
  app.initialise();
  app.run();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  auto mySender = pvManagers.first->getProcessArray<T>("/mySender");
  BOOST_CHECK(mySender->getName() == "/mySender");
  auto myReceiver = pvManagers.first->getProcessArray<T>("/myReceiver");
  BOOST_CHECK(myReceiver->getName() == "/myReceiver");

  mySender->accessData(0) = 22;
  mySender->write();
  myReceiver->read();
  BOOST_CHECK_EQUAL(myReceiver->accessData(0), 22);

  mySender->accessData(0) = 23;
  mySender->write();
  myReceiver->read();
  BOOST_CHECK_EQUAL(myReceiver->accessData(0), 23);

  mySender->accessData(0) = 24;
  mySender->write();
  myReceiver->read();
  BOOST_CHECK_EQUAL(myReceiver->accessData(0), 24);
}
