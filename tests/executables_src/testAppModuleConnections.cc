// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>

#include <boost/mpl/list.hpp>

#include <future>

#define BOOST_NO_EXCEPTIONS
#define BOOST_TEST_MODULE testAppModuleConnections
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

// list of user types the accessors are tested with
typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double> test_types;

/*********************************************************************************************************************/
/* the ApplicationModule for the test is a template of the user type */

template<typename T>
struct TestModule : public ctk::ApplicationModule {
  TestModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags = {})
  : ApplicationModule(owner, name, description, tags), mainLoopStarted(2) {}

  ctk::ScalarOutput<T> feedingPush;
  ctk::ScalarPushInput<T> consumingPush;
  ctk::ScalarPushInput<T> consumingPush2;
  ctk::ScalarPushInput<T> consumingPush3;

  ctk::ScalarPollInput<T> consumingPoll;
  ctk::ArrayPushInput<T> consumingPushArray;

  ctk::ArrayOutput<T> feedingArray;
  ctk::ArrayOutput<T> feedingPseudoArray;

  // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
  // mainLoop(). This is required since the mainLoopWrapper accesses the module variables before the start of the
  // mainLoop.
  // execute this right after the Application::run():
  //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
  boost::barrier mainLoopStarted;

  void prepare() override {
    incrementDataFaultCounter(); // force all outputs  to invalid
    writeAll();                  // write initial values
    decrementDataFaultCounter(); // validity according to input validity
  }

  void mainLoop() override { mainLoopStarted.wait(); }
};

/*********************************************************************************************************************/
/* dummy application */

template<typename T>
struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() override { shutdown(); }

  void defineConnections() {} // the setup is done in the tests

  TestModule<T> testModule{this, "testModule", "The test module"};
};

/*********************************************************************************************************************/
/* test case for two scalar accessors in push mode */

BOOST_AUTO_TEST_CASE_TEMPLATE(testTwoScalarPushAccessors, T, test_types) {
  // FIXME: With the new scheme, there cannot be a 1:1 module connection any more, it will always be a network involving
  // the ControlSystem
  std::cout << "*** testTwoScalarPushAccessors<" << typeid(T).name() << ">" << std::endl;

  TestApplication<T> app;
  app.testModule.feedingPush = {&app.testModule, "testTwoScalarPushAccessors", "", ""};
  app.testModule.consumingPush = {&app.testModule, "testTwoScalarPushAccessors", "", ""};

  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  // single threaded test
  app.testModule.consumingPush = 0;
  app.testModule.feedingPush = 42;
  BOOST_CHECK(app.testModule.consumingPush == 0);
  app.testModule.feedingPush.write();
  BOOST_CHECK(app.testModule.consumingPush == 0);
  app.testModule.consumingPush.read();
  BOOST_CHECK(app.testModule.consumingPush == 42);

  // launch read() on the consumer asynchronously and make sure it does not yet
  // receive anything
  auto futRead = std::async(std::launch::async, [&app] { app.testModule.consumingPush.read(); });
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);

  BOOST_CHECK(app.testModule.consumingPush == 42);

  // write to the feeder
  app.testModule.feedingPush = 120;
  app.testModule.feedingPush.write();

  // check that the consumer now receives the just written value
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
  BOOST_CHECK(app.testModule.consumingPush == 120);
}

/*********************************************************************************************************************/
/* test case for four scalar accessors in push mode: one feeder and three
 * consumers */

BOOST_AUTO_TEST_CASE_TEMPLATE(testFourScalarPushAccessors, T, test_types) {
  std::cout << "*** testFourScalarPushAccessors<" << typeid(T).name() << ">" << std::endl;

  TestApplication<T> app;
  app.testModule.consumingPush = {&app.testModule, "testFourScalarPushAccessors", "", ""};
  app.testModule.consumingPush2 = {&app.testModule, "testFourScalarPushAccessors", "", ""};
  app.testModule.feedingPush = {&app.testModule, "testFourScalarPushAccessors", "", ""};
  app.testModule.consumingPush3 = {&app.testModule, "testFourScalarPushAccessors", "", ""};

  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  // single threaded test
  app.testModule.consumingPush = 0;
  app.testModule.consumingPush2 = 2;
  app.testModule.consumingPush3 = 3;
  app.testModule.feedingPush = 42;
  BOOST_CHECK(app.testModule.consumingPush == 0);
  BOOST_CHECK(app.testModule.consumingPush2 == 2);
  BOOST_CHECK(app.testModule.consumingPush3 == 3);
  app.testModule.feedingPush.write();
  BOOST_CHECK(app.testModule.consumingPush == 0);
  BOOST_CHECK(app.testModule.consumingPush2 == 2);
  BOOST_CHECK(app.testModule.consumingPush3 == 3);
  app.testModule.consumingPush.read();
  BOOST_CHECK(app.testModule.consumingPush == 42);
  BOOST_CHECK(app.testModule.consumingPush2 == 2);
  BOOST_CHECK(app.testModule.consumingPush3 == 3);
  app.testModule.consumingPush2.read();
  BOOST_CHECK(app.testModule.consumingPush == 42);
  BOOST_CHECK(app.testModule.consumingPush2 == 42);
  BOOST_CHECK(app.testModule.consumingPush3 == 3);
  app.testModule.consumingPush3.read();
  BOOST_CHECK(app.testModule.consumingPush == 42);
  BOOST_CHECK(app.testModule.consumingPush2 == 42);
  BOOST_CHECK(app.testModule.consumingPush3 == 42);

  // launch read() on the consumers asynchronously and make sure it does not yet
  // receive anything
  auto futRead = std::async(std::launch::async, [&app] { app.testModule.consumingPush.read(); });
  auto futRead2 = std::async(std::launch::async, [&app] { app.testModule.consumingPush2.read(); });
  auto futRead3 = std::async(std::launch::async, [&app] { app.testModule.consumingPush3.read(); });
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
  BOOST_CHECK(futRead2.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout);
  BOOST_CHECK(futRead3.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout);

  BOOST_CHECK(app.testModule.consumingPush == 42);
  BOOST_CHECK(app.testModule.consumingPush2 == 42);
  BOOST_CHECK(app.testModule.consumingPush3 == 42);

  // write to the feeder
  app.testModule.feedingPush = 120;
  app.testModule.feedingPush.write();

  // check that the consumers now receive the just written value
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
  BOOST_CHECK(futRead2.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
  BOOST_CHECK(futRead3.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
  BOOST_CHECK(app.testModule.consumingPush == 120);
  BOOST_CHECK(app.testModule.consumingPush2 == 120);
  BOOST_CHECK(app.testModule.consumingPush3 == 120);
}

/*********************************************************************************************************************/
/* test case for two scalar accessors, feeder in push mode and consumer in poll
 * mode */

BOOST_AUTO_TEST_CASE_TEMPLATE(testTwoScalarPushPollAccessors, T, test_types) {
  std::cout << "*** testTwoScalarPushPollAccessors<" << typeid(T).name() << ">" << std::endl;

  TestApplication<T> app;

  app.testModule.feedingPush = {&app.testModule, "testTwoScalarPushPollAccessors", "", ""};
  app.testModule.consumingPoll = {&app.testModule, "testTwoScalarPushPollAccessors", "", ""};

  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  // single threaded test only, since read() does not block in this case
  app.testModule.consumingPoll = 0;
  app.testModule.feedingPush = 42;
  BOOST_CHECK(app.testModule.consumingPoll == 0);
  app.testModule.feedingPush.write();
  BOOST_CHECK(app.testModule.consumingPoll == 0);
  app.testModule.consumingPoll.read();
  BOOST_CHECK(app.testModule.consumingPoll == 42);
  app.testModule.consumingPoll.read();
  BOOST_CHECK(app.testModule.consumingPoll == 42);
  app.testModule.consumingPoll.read();
  BOOST_CHECK(app.testModule.consumingPoll == 42);
  app.testModule.feedingPush = 120;
  BOOST_CHECK(app.testModule.consumingPoll == 42);
  app.testModule.feedingPush.write();
  BOOST_CHECK(app.testModule.consumingPoll == 42);
  app.testModule.consumingPoll.read();
  BOOST_CHECK(app.testModule.consumingPoll == 120);
  app.testModule.consumingPoll.read();
  BOOST_CHECK(app.testModule.consumingPoll == 120);
  app.testModule.consumingPoll.read();
  BOOST_CHECK(app.testModule.consumingPoll == 120);
}

/*********************************************************************************************************************/
/* test case for two array accessors in push mode */

BOOST_AUTO_TEST_CASE_TEMPLATE(testTwoArrayAccessors, T, test_types) {
  std::cout << "*** testTwoArrayAccessors<" << typeid(T).name() << ">" << std::endl;

  TestApplication<T> app;

  // app.testModule.feedingArray >> app.testModule.consumingPushArray;
  app.testModule.feedingArray = {&app.testModule, "testFourScalarPushAccessors", "", 10, ""};
  app.testModule.consumingPushArray = {&app.testModule, "testFourScalarPushAccessors", "", 10, ""};
  ctk::TestFacility tf{app, false};
  tf.runApplication();

  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  BOOST_CHECK(app.testModule.feedingArray.getNElements() == 10);
  BOOST_CHECK(app.testModule.consumingPushArray.getNElements() == 10);

  // single threaded test
  for(auto& val : app.testModule.consumingPushArray) val = 0;
  for(unsigned int i = 0; i < 10; ++i) app.testModule.feedingArray[i] = 99 + (T)i;
  for(auto& val : app.testModule.consumingPushArray) BOOST_CHECK(val == 0);
  app.testModule.feedingArray.write();
  for(auto& val : app.testModule.consumingPushArray) BOOST_CHECK(val == 0);
  app.testModule.consumingPushArray.read();
  for(unsigned int i = 0; i < 10; ++i) BOOST_CHECK(app.testModule.consumingPushArray[i] == 99 + (T)i);

  // launch read() on the consumer asynchronously and make sure it does not yet
  // receive anything
  auto futRead = std::async(std::launch::async, [&app] { app.testModule.consumingPushArray.read(); });
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);

  for(unsigned int i = 0; i < 10; ++i) BOOST_CHECK(app.testModule.consumingPushArray[i] == 99 + (T)i);

  // write to the feeder
  for(unsigned int i = 0; i < 10; ++i) app.testModule.feedingArray[i] = 42 - (T)i;
  app.testModule.feedingArray.write();

  // check that the consumer now receives the just written value
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
  for(unsigned int i = 0; i < 10; ++i) BOOST_CHECK(app.testModule.consumingPushArray[i] == 42 - (T)i);
}


/*********************************************************************************************************************/
/* test case for connecting array of length 1 with scalar */

BOOST_AUTO_TEST_CASE_TEMPLATE(testPseudoArray, T, test_types) {
  std::cout << "*** testPseudoArray<" << typeid(T).name() << ">" << std::endl;

  TestApplication<T> app;

  // app.testModule.feedingPseudoArray >> app.testModule.consumingPush;
  app.testModule.feedingPseudoArray = {&app.testModule, "testPseudoArray", "", 1, ""};
  app.testModule.consumingPush = {&app.testModule, "testPseudoArray", "", ""};

  // run the app
  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  // test data transfer
  app.testModule.feedingPseudoArray[0] = 33;
  app.testModule.feedingPseudoArray.write();
  app.testModule.consumingPush.read();
  BOOST_CHECK(app.testModule.consumingPush == 33);
}
