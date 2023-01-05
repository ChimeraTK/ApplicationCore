// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testTestFacilities

#include "Application.h"
#include "ApplicationModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestableMode.h"
#include "TestFacility.h"
#include "VariableGroup.h"

#include <ChimeraTK/Device.h>

#include <boost/mpl/list.hpp>
//#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
//#undef BOOST_NO_EXCEPTIONS
#include <boost/thread/barrier.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

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

constexpr char dummySdm[] = "(dummy?map=test_readonly.map)";

/*********************************************************************************************************************/
/* the BlockingReadTestModule blockingly reads its input in the main loop and writes the result to its output */

struct BlockingReadTestModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<int32_t> someInput{this, "/value", "cm", "This is just some input for testing"};
  ctk::ScalarOutput<int32_t> someOutput{this, "someOutput", "cm", "Description"};

  void mainLoop() override {
    while(true) {
      int32_t val = someInput;
      someOutput = val;
      usleep(10000); // wait some extra time to make sure we are really blocking
                     // the test procedure thread
      someOutput.write();
      someInput.read(); // read at the end to propagate the initial value
    }
  }
};

/*********************************************************************************************************************/
/* the ReadAnyTestModule calls readAny on a bunch of inputs and outputs some information on the received data */

struct ReadAnyTestModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  struct Inputs : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPushInput<int32_t> v1{this, "v1", "cm", "Input 1 for testing"};
    ctk::ScalarPushInput<int32_t> v2{this, "/REG2", "cm", "Input 2 for testing"};
    ctk::ScalarPushInput<int32_t> v3{this, "v3", "cm", "Input 3 for testing"};
    ctk::ScalarPushInput<int32_t> v4{this, "v4", "cm", "Input 4 for testing"};
  };
  Inputs inputs{this, "inputs", "A group of inputs"};
  ctk::ScalarOutput<int32_t> value{this, "/value", "cm", "The last value received from any of the inputs"};
  ctk::ScalarOutput<uint32_t> index{
      this, "index", "", "The index (1..4) of the input where the last value was received"};

  void prepare() override {
    incrementDataFaultCounter(); // force all outputs  to invalid
    writeAll();
    decrementDataFaultCounter(); // validity according to input validity
  }

  void mainLoop() override {
    auto group = inputs.readAnyGroup();
    while(true) {
      auto justRead = group.readAny();
      if(inputs.v1.getId() == justRead) {
        index = 1;
        value = int32_t(inputs.v1);
      }
      else if(inputs.v2.getId() == justRead) {
        index = 2;
        value = int32_t(inputs.v2);
      }
      else if(inputs.v3.getId() == justRead) {
        index = 3;
        value = int32_t(inputs.v3);
      }
      else if(inputs.v4.getId() == justRead) {
        index = 4;
        value = int32_t(inputs.v4);
      }
      else {
        index = 0;
        value = 0;
      }
      usleep(10000); // wait some extra time to make sure we are really blocking
                     // the test procedure thread
      index.write();
      value.write();
    }
  }
};

/*********************************************************************************************************************/
/* the PollingReadModule is designed to test poll-type transfers (even mixed with push-type) */

struct PollingReadModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<int32_t> push{this, "push", "cm", "A push-type input"};
  ctk::ScalarPushInput<int32_t> push2{this, "push2", "cm", "A second push-type input"};
  ctk::ScalarPollInput<int32_t> poll{this, "poll", "cm", "A poll-type input"};

  ctk::ScalarOutput<int32_t> valuePush{this, "valuePush", "cm", "The last value received for 'push'"};
  ctk::ScalarOutput<int32_t> valuePoll{this, "valuePoll", "cm", "The last value received for 'poll'"};
  ctk::ScalarOutput<int32_t> state{this, "state", "", "State of the test mainLoop"};

  void prepare() override {
    incrementDataFaultCounter(); // foce all outputs  to invalid
    writeAll();
    decrementDataFaultCounter(); // validity according to input validity
  }

  void mainLoop() override {
    while(true) {
      push.read();
      poll.read();
      valuePush = int32_t(push);
      valuePoll = int32_t(poll);
      valuePoll.write();
      valuePush.write();
      state = 1;
      state.write();

      push2.read();
      push.readNonBlocking();
      poll.read();
      valuePush = int32_t(push);
      valuePoll = int32_t(poll);
      valuePoll.write();
      valuePush.write();
      state = 2;
      state.write();

      push2.read();
      push.readLatest();
      poll.read();
      valuePush = int32_t(push);
      valuePoll = int32_t(poll);
      valuePoll.write();
      valuePush.write();
      state = 3;
      state.write();
    }
  }
};

/*********************************************************************************************************************/
/* the PollingThroughFanoutsModule is designed to test poll-type transfers in combination with FanOuts */

struct PollingThroughFanoutsModule : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> push1{this, "push1", "", ""};
  ctk::ScalarPollInput<int> poll1{this, "poll1", "", ""};
  ctk::ScalarPollInput<int> poll2{this, "poll2", "", ""};
  ctk::ScalarOutput<int> out1{this, "out1", "", ""};
  ctk::ScalarOutput<int> out2{this, "out2", "", ""};

  std::mutex m_forChecking;
  bool hasRead{false};

  void prepare() override {
    writeAll();
  }

  void run() override {
    ApplicationModule::run();
  }

  void mainLoop() override {
    while(true) {
      push1.read();

      std::unique_lock<std::mutex> lock(m_forChecking);
      hasRead = true;
      poll1.read();
      poll2.read();
      usleep(1000); // give try_lock() in tests a chance to fail if testable mode lock would not work
    }
  }
};

/*********************************************************************************************************************/
/* test that no TestableModeAccessorDecorator is used if the testable mode is not enabled */

struct TestNoDecoratorApplication : public ctk::Application {
  TestNoDecoratorApplication() : Application("testApplication") {}
  ~TestNoDecoratorApplication() override { shutdown(); }

  BlockingReadTestModule blockingReadTestModule{this, "blockingReadTestModule", "Module for testing blocking read"};
  ReadAnyTestModule readAnyTestModule{this, "readAnyTestModule", "Module for testing readAny()"};
};

BOOST_AUTO_TEST_CASE(testNoDecorator) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testNoDecorator" << std::endl;

  TestNoDecoratorApplication app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  app.initialise();
  app.run();

  // check if we got the decorator for the input
  auto hlinput = app.blockingReadTestModule.someInput.getHighLevelImplElement();
  BOOST_CHECK(boost::dynamic_pointer_cast<ctk::detail::TestableMode::AccessorDecorator<int32_t>>(hlinput) == nullptr);

  // check that we did not get the decorator for the output
  auto hloutput = app.blockingReadTestModule.someOutput.getHighLevelImplElement();
  BOOST_CHECK(boost::dynamic_pointer_cast<ctk::detail::TestableMode::AccessorDecorator<int32_t>>(hloutput) == nullptr);
}

/*********************************************************************************************************************/
/* test blocking read in test mode */

struct TestBlockingReadApplication : public ctk::Application {
  TestBlockingReadApplication() : Application("testApplication") {}
  ~TestBlockingReadApplication() override { shutdown(); }

  BlockingReadTestModule blockingReadTestModule{this, "blockingReadTestModule", "Module for testing blocking read"};
};

BOOST_AUTO_TEST_CASE(testBlockingRead) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testBlockingRead" << std::endl;

  TestBlockingReadApplication app;

  ctk::TestFacility test{app};
  auto pvInput = test.getScalar<int32_t>("/value");
  auto pvOutput = test.getScalar<int32_t>("/blockingReadTestModule/someOutput");
  test.runApplication();

  // test blocking read when taking control in the test thread (note: the
  // blocking read is executed in the app module!)
  for(int i = 0; i < 5; ++i) {
    pvInput = 120 + i;
    pvInput.write();
    usleep(10000);
    BOOST_CHECK(pvOutput.readNonBlocking() == false);
    test.stepApplication();
    CHECK_TIMEOUT(pvOutput.readNonBlocking() == true, 10000);
    int val = pvOutput;
    BOOST_CHECK(val == 120 + i);
  }
}

/*********************************************************************************************************************/
/* test testReadAny in test mode */

struct TestReadAnyApplication : public ctk::Application {
  TestReadAnyApplication() : Application("testApplication") {}
  ~TestReadAnyApplication() override { shutdown(); }

  ReadAnyTestModule readAnyTestModule{this, "readAnyTestModule", "Module for testing readAny()"};
};

BOOST_AUTO_TEST_CASE(testReadAny) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testReadAny" << std::endl;

  TestReadAnyApplication app;

  ctk::TestFacility test{app};
  auto value = test.getScalar<int32_t>("/value");
  auto index = test.getScalar<uint32_t>("/readAnyTestModule/index");
  auto v1 = test.getScalar<int32_t>("/readAnyTestModule/inputs/v1");
  auto v2 = test.getScalar<int32_t>("/REG2"); // just named irregularly, no device present!
  auto v3 = test.getScalar<int32_t>("/readAnyTestModule/inputs/v3");
  auto v4 = test.getScalar<int32_t>("/readAnyTestModule/inputs/v4");
  test.runApplication();
  // check that we don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // send something to v4
  v4 = 66;
  v4.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK_EQUAL(value, 66);
  BOOST_CHECK_EQUAL(index, 4);

  // send something to v1
  v1 = 33;
  v1.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK_EQUAL(value, 33);
  BOOST_CHECK_EQUAL(index, 1);

  // send something to v1 again
  v1 = 34;
  v1.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();

  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK_EQUAL(value, 34);
  BOOST_CHECK_EQUAL(index, 1);

  // send something to v3
  v3 = 40;
  v3.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK_EQUAL(value, 40);
  BOOST_CHECK_EQUAL(index, 3);

  // send something to v2
  v2 = 50;
  v2.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK_EQUAL(value, 50);
  BOOST_CHECK_EQUAL(index, 2);

  // check that stepApplication() throws an exception if no input data is
  // available
  try {
    test.stepApplication();
    BOOST_ERROR("IllegalParameter exception expected.");
  }
  catch(ChimeraTK::logic_error&) {
  }

  // check that we still don't receive anything anymore
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // send something to v1 a 3rd time
  v1 = 35;
  v1.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK_EQUAL(value, 35);
  BOOST_CHECK_EQUAL(index, 1);
}

/*********************************************************************************************************************/
/* test the interplay of multiple chained modules and their threads in test mode
 */

struct TestChaniedModulesApplication : public ctk::Application {
  TestChaniedModulesApplication() : Application("testApplication") {}
  ~TestChaniedModulesApplication() override { shutdown(); }

  BlockingReadTestModule blockingReadTestModule{this, "blockingReadTestModule", "Module for testing blocking read"};
  ReadAnyTestModule readAnyTestModule{this, "readAnyTestModule", "Module for testing readAny()"};
};

BOOST_AUTO_TEST_CASE(testChainedModules) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testChainedModules" << std::endl;

  TestChaniedModulesApplication app;

  ctk::TestFacility test{app};
  auto value = test.getScalar<int32_t>("/blockingReadTestModule/someOutput");
  auto index = test.getScalar<uint32_t>("/readAnyTestModule/index");
  auto v1 = test.getScalar<int32_t>("/readAnyTestModule/inputs/v1");
  auto v2 = test.getScalar<int32_t>("/REG2"); // just named irregularly, no device present!
  auto v3 = test.getScalar<int32_t>("/readAnyTestModule/inputs/v3");
  auto v4 = test.getScalar<int32_t>("/readAnyTestModule/inputs/v4");
  test.runApplication();

  // check that we don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // send something to v2
  v2 = 11;
  v2.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK(value == 11);
  BOOST_CHECK(index == 2);

  // send something to v3
  v3 = 12;
  v3.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK(value == 12);
  BOOST_CHECK(index == 3);

  // send something to v3 again
  v3 = 13;
  v3.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(value.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK(value == 13);
  BOOST_CHECK(index == 3);

  // check that stepApplication() throws an exception if no input data is
  // available
  try {
    test.stepApplication();
    BOOST_ERROR("IllegalParameter exception expected.");
  }
  catch(ChimeraTK::logic_error&) {
  }

  // check that we still don't receive anything anymore
  usleep(10000);
  BOOST_CHECK(value.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);
}

/*********************************************************************************************************************/
/* test combination with trigger */

struct TestWithTriggerApplication : public ctk::Application {
  TestWithTriggerApplication() : Application("testApplication") {}
  ~TestWithTriggerApplication() override { shutdown(); }

  ctk::DeviceModule dev{this, dummySdm, "/trigger"};
  BlockingReadTestModule blockingReadTestModule{this, "blockingReadTestModule", "Module for testing blocking read"};
  ReadAnyTestModule readAnyTestModule{this, "readAnyTestModule", "Module for testing readAny()"};
};

BOOST_AUTO_TEST_CASE(testWithTrigger) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testWithTrigger" << std::endl;

  TestWithTriggerApplication app;

  ctk::TestFacility test{app};
  ctk::Device dev;
  dev.open(dummySdm);
  auto valueFromBlocking = test.getScalar<int32_t>("/blockingReadTestModule/someOutput");
  auto index = test.getScalar<uint32_t>("/readAnyTestModule/index");
  auto trigger = test.getVoid("/trigger");
  auto v2 = dev.getScalarRegisterAccessor<int32_t>("/REG2.DUMMY_WRITEABLE");
  test.runApplication();

  // check that we don't receive anything yet
  usleep(10000);
  BOOST_CHECK(valueFromBlocking.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // send something to v2 and send the trigger
  v2 = 11;
  v2.write();
  trigger.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(valueFromBlocking.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(valueFromBlocking.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK(valueFromBlocking == 11);
  BOOST_CHECK(index == 2);

  // again send something to v2 and send the trigger
  v2 = 22;
  v2.write();
  trigger.write();

  // check that we still don't receive anything yet
  usleep(10000);
  BOOST_CHECK(valueFromBlocking.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);

  // run the application and check that we got the expected result
  test.stepApplication();
  BOOST_CHECK(valueFromBlocking.readNonBlocking() == true);
  BOOST_CHECK(index.readNonBlocking() == true);
  BOOST_CHECK(valueFromBlocking == 22);
  BOOST_CHECK(index == 2);

  // check that stepApplication() throws an exception if no input data is
  // available
  try {
    test.stepApplication();
    BOOST_ERROR("IllegalParameter exception expected.");
  }
  catch(ChimeraTK::logic_error&) {
  }

  // check that we still don't receive anything anymore
  usleep(10000);
  BOOST_CHECK(valueFromBlocking.readNonBlocking() == false);
  //  BOOST_CHECK(valueFromAsync.readNonBlocking() == false);
  BOOST_CHECK(index.readNonBlocking() == false);
}

/*********************************************************************************************************************/
/* test convenience read functions */

struct TestConvenienceReadApplication : public ctk::Application {
  TestConvenienceReadApplication() : Application("testApplication") {}
  ~TestConvenienceReadApplication() override { shutdown(); }

  BlockingReadTestModule blockingReadTestModule{this, "blockingReadTestModule", "Module for testing blocking read"};
};

BOOST_AUTO_TEST_CASE(testConvenienceRead) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testConvenienceRead" << std::endl;

  TestConvenienceReadApplication app;

  ctk::TestFacility test{app};
  test.runApplication();

  // test blocking read when taking control in the test thread (note: the blocking read is executed in the app module!)
  for(int i = 0; i < 5; ++i) {
    test.writeScalar<int32_t>("/value", 120 + i);
    test.stepApplication();
    CHECK_TIMEOUT(test.readScalar<int32_t>("/blockingReadTestModule/someOutput") == 120 + i, 10000);
  }

  // same with array function (still a scalar variable behind, but this does not matter)
  for(int i = 0; i < 5; ++i) {
    std::vector<int32_t> myValue{120 + i};
    test.writeArray<int32_t>("/value", myValue);
    test.stepApplication();
    CHECK_TIMEOUT(
        test.readArray<int32_t>("/blockingReadTestModule/someOutput") == std::vector<int32_t>{120 + i}, 10000);
  }
}

/*********************************************************************************************************************/
/* test poll-type transfers mixed with push-type */

struct TestPollingApplication : public ctk::Application {
  TestPollingApplication() : Application("testApplication") {}
  ~TestPollingApplication() override { shutdown(); }

  PollingReadModule pollingReadModule{this, "pollingReadModule", "Module for testing poll-type transfers"};
};

BOOST_AUTO_TEST_CASE(testPolling) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testPolling" << std::endl;

  TestPollingApplication app; // app.pollingReadModule.connectTo(app.cs);

  ctk::TestFacility test{app};
  test.runApplication();

  auto pv_push = test.getScalar<int32_t>("/pollingReadModule/push");
  auto pv_push2 = test.getScalar<int32_t>("/pollingReadModule/push2");
  auto pv_poll = test.getScalar<int32_t>("/pollingReadModule/poll");
  auto pv_valuePush = test.getScalar<int32_t>("/pollingReadModule/valuePush");
  auto pv_valuePoll = test.getScalar<int32_t>("/pollingReadModule/valuePoll");
  auto pv_state = test.getScalar<int>("/pollingReadModule/state");

  // write values to 'push' and 'poll' and check result
  pv_push = 120;
  pv_push.write();
  pv_poll = 42;
  pv_poll.write();
  test.stepApplication();
  pv_valuePoll.read();
  pv_valuePush.read();
  pv_state.read();
  BOOST_CHECK_EQUAL((int32_t)pv_valuePoll, 42);
  BOOST_CHECK_EQUAL((int32_t)pv_valuePush, 120);
  BOOST_CHECK_EQUAL((int32_t)pv_state, 1);

  // this time the application gets triggered by push2, push is read
  // non-blockingly (single value only)
  pv_push = 22;
  pv_push.write();
  pv_poll = 44;
  pv_poll.write();
  pv_poll = 45;
  pv_poll.write();
  pv_push2.write();
  test.stepApplication();
  pv_valuePoll.read();
  pv_valuePush.read();
  pv_state.read();
  BOOST_CHECK_EQUAL((int32_t)pv_valuePoll, 45);
  BOOST_CHECK_EQUAL((int32_t)pv_valuePush, 22);
  BOOST_CHECK_EQUAL((int32_t)pv_state, 2);

  // this time the application gets triggered by push2, push is read with
  // readLatest()
  pv_push = 24;
  pv_push.write();
  pv_poll = 46;
  pv_poll.write();
  pv_push2.write();
  test.stepApplication();
  pv_valuePoll.read();
  pv_valuePush.read();
  pv_state.read();
  BOOST_CHECK_EQUAL((int32_t)pv_valuePoll, 46);
  BOOST_CHECK_EQUAL((int32_t)pv_valuePush, 24);
  BOOST_CHECK_EQUAL((int32_t)pv_state, 3);

  // provoke internal queue overflow in poll-type variable (should not make any difference)
  pv_push = 25;
  pv_push.write();
  for(int i = 0; i < 10; ++i) {
    pv_poll = 50 + i;
  }
  pv_poll.write();
  pv_push2.write();
  test.stepApplication();
  pv_valuePoll.read();
  pv_valuePush.read();
  pv_state.read();
  BOOST_CHECK_EQUAL((int32_t)pv_valuePoll, 59);
  BOOST_CHECK_EQUAL((int32_t)pv_valuePush, 25);
  BOOST_CHECK_EQUAL((int32_t)pv_state, 1);
}

/*********************************************************************************************************************/
/* test poll-type transfers in combination with various FanOuts */

struct TestPollingThroughFanOutsApplication : public ctk::Application {
  TestPollingThroughFanOutsApplication() : Application("AnotherTestApplication") {}
  ~TestPollingThroughFanOutsApplication() override { shutdown(); }

  ctk::DeviceModule dev{this, dummySdm, "/fakeTriggerToSatisfyUnusedRegister"};
  PollingThroughFanoutsModule m1{this, "m1", ""};
  PollingThroughFanoutsModule m2{this, "m2", ""};
};

BOOST_AUTO_TEST_CASE(testPollingThroughFanOuts) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testPollingThroughFanOuts" << std::endl;

  // Case 1: FeedingFanOut
  // ---------------------
  {
    TestPollingThroughFanOutsApplication app;
    app.debugMakeConnections();
    // app.getTestableMode().setEnableDebug();

    app.m2.poll1 = {&app.m2, "/m1/out1", "", ""};
    app.m2.poll2 = {&app.m2, "/m1/out1", "", ""};
    app.m2.push1 = {&app.m2, "/m1/out2", "", ""};

    std::unique_lock<std::mutex> lk1(app.m1.m_forChecking, std::defer_lock);
    std::unique_lock<std::mutex> lk2(app.m2.m_forChecking, std::defer_lock);

    ctk::TestFacility test{app};

    test.runApplication();

    // test single value
    BOOST_REQUIRE(lk1.try_lock());
    app.m1.out1 = 123;
    app.m1.out1.write();
    app.m1.out2.write();
    lk1.unlock();

    test.stepApplication();

    BOOST_REQUIRE(lk2.try_lock());
    BOOST_CHECK_EQUAL(app.m2.poll1, 123);
    BOOST_CHECK_EQUAL(app.m2.poll2, 123);
    lk2.unlock();

    // test queue overrun
    BOOST_REQUIRE(lk1.try_lock());
    for(int i = 0; i < 10; ++i) {
      app.m1.out1 = 191 + i;
      app.m1.out1.write();
      app.m1.out2.write();
    }
    lk1.unlock();

    test.stepApplication();

    BOOST_REQUIRE(lk2.try_lock());
    BOOST_CHECK_EQUAL(app.m2.poll1, 200);
    BOOST_CHECK_EQUAL(app.m2.poll2, 200);
    lk2.unlock();
  }

  // Case 2: ConsumingFanOut
  // -----------------------
  {
    TestPollingThroughFanOutsApplication app;
    app.m1.poll1 = {&app.m1, "/REG1", "", ""};
    app.m2.push1 = {&app.m2, "/REG1", "", ""};
    // app.dev("REG1") >> app.m1.poll1 >> app.m2.push1;

    std::unique_lock<std::mutex> lk1(app.m1.m_forChecking, std::defer_lock);
    std::unique_lock<std::mutex> lk2(app.m2.m_forChecking, std::defer_lock);

    ctk::Device dev(dummySdm);
    auto reg1 = dev.getScalarRegisterAccessor<int>("REG1.DUMMY_WRITEABLE");

    ctk::TestFacility test{app};
    test.runApplication();

    reg1 = 42;
    reg1.write();

    BOOST_REQUIRE(lk1.try_lock());
    app.m1.poll1.read();
    BOOST_CHECK_EQUAL(app.m1.poll1, 42);
    lk1.unlock();
    BOOST_REQUIRE(lk2.try_lock());
    BOOST_CHECK_NE(app.m2.push1, 42);
    lk2.unlock();

    test.stepApplication();

    BOOST_REQUIRE(lk2.try_lock());
    BOOST_CHECK_EQUAL(app.m2.push1, 42);
    lk2.unlock();
  }

  // Case 3: ThreadedFanOut
  // ----------------------
  {
    std::cout << "=== Case 3" << std::endl;
    TestPollingThroughFanOutsApplication app;
    std::cout << " HIER 1" << std::endl;
    app.m1.poll2 = {&app.m1, "poll1", "", ""};
    std::cout << " HIER 2" << std::endl;
    app.m1.push1 = {&app.m1, "/m2/out2", "", ""};
    std::cout << " HIER 3" << std::endl;

    std::unique_lock<std::mutex> lk1(app.m1.m_forChecking, std::defer_lock);
    std::unique_lock<std::mutex> lk2(app.m2.m_forChecking, std::defer_lock);

    ctk::TestFacility test{app};

    auto var = test.getScalar<int>("/m1/poll1");
    app.getModel().writeGraphViz("testPollingThroughFanOuts.dot");
    test.runApplication();

    // test with single value
    var = 666;
    var.write();
    BOOST_REQUIRE(lk2.try_lock());
    app.m2.out2.write();
    lk2.unlock();

    test.stepApplication();

    BOOST_REQUIRE(lk1.try_lock());
    app.m1.poll1.read();
    BOOST_CHECK_EQUAL(app.m1.poll1, 666);
    app.m1.poll2.read();
    BOOST_CHECK_EQUAL(app.m1.poll2, 666);
    lk1.unlock();

    // test with queue overrun
    for(int i = 0; i < 10; ++i) {
      var = 691 + i;
      var.write();
    }
    BOOST_REQUIRE(lk2.try_lock());
    app.m2.out2.write();
    lk2.unlock();

    test.stepApplication();

    BOOST_REQUIRE(lk1.try_lock());
    app.m1.poll1.read();
    BOOST_CHECK_EQUAL(app.m1.poll1, 700);
    app.m1.poll2.read();
    BOOST_CHECK_EQUAL(app.m1.poll2, 700);
    lk1.unlock();
  }
}

/*********************************************************************************************************************/
/* test device variables */

struct TestDeviceApplication : public ctk::Application {
  TestDeviceApplication() : Application("testApplication") {}
  ~TestDeviceApplication() override { shutdown(); }

  ctk::DeviceModule dev{this, dummySdm, "/fakeTriggerToSatisfyUnusedRegister"};
  PollingReadModule pollingReadModule{this, "pollingReadModule", "Module for testing poll-type transfers"};
};

BOOST_AUTO_TEST_CASE(testDevice) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testDevice" << std::endl;

  TestDeviceApplication app;
  app.pollingReadModule.poll = {&app.pollingReadModule, "/REG1", "cm", "A poll-type input"};

  ctk::TestFacility test(app);
  auto push = test.getScalar<int32_t>("/pollingReadModule/push");
  auto push2 = test.getScalar<int32_t>("/pollingReadModule/push2");
  auto valuePoll = test.getScalar<int32_t>("/pollingReadModule/valuePoll");

  ctk::Device dev(dummySdm);
  auto r1 = dev.getScalarRegisterAccessor<int32_t>("/REG1.DUMMY_WRITEABLE");

  test.runApplication();

  // this is state 1 in PollingReadModule -> read()
  r1 = 42;
  r1.write();
  push.write();
  test.stepApplication();
  valuePoll.read();
  BOOST_CHECK_EQUAL(valuePoll, 42);

  // this is state 2 in PollingReadModule -> readNonBlocking()
  r1 = 43;
  r1.write();
  push2.write();
  test.stepApplication();
  valuePoll.read();
  BOOST_CHECK_EQUAL(valuePoll, 43);

  // this is state 2 in PollingReadModule -> readLatest()
  r1 = 44;
  r1.write();
  push2.write();
  test.stepApplication();
  valuePoll.read();
  BOOST_CHECK_EQUAL(valuePoll, 44);
}

/*********************************************************************************************************************/
/* test initial values (from control system variables) */

struct TestInitialApplication : public ctk::Application {
  TestInitialApplication() : Application("AnotherTestApplication") {}
  ~TestInitialApplication() override { shutdown(); }

  PollingThroughFanoutsModule m1{this, "m1", ""};
  PollingThroughFanoutsModule m2{this, "m2", ""};
};

BOOST_AUTO_TEST_CASE(testInitialValues) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testInitialValues" << std::endl;

  TestInitialApplication app;
  // app.findTag(".*").connectTo(app.cs);

  std::unique_lock<std::mutex> lk1(app.m1.m_forChecking, std::defer_lock);
  std::unique_lock<std::mutex> lk2(app.m2.m_forChecking, std::defer_lock);

  ctk::TestFacility test{app};

  test.setScalarDefault<int>("/m1/push1", 42);
  test.setScalarDefault<int>("/m1/poll1", 43);
  test.setScalarDefault<int>("/m2/poll2", 44);

  test.runApplication();

  BOOST_REQUIRE(lk1.try_lock());
  BOOST_CHECK(!app.m1.hasRead);
  BOOST_CHECK_EQUAL(app.m1.push1, 42);
  BOOST_CHECK_EQUAL(app.m1.poll1, 43);
  BOOST_CHECK_EQUAL(app.m1.poll2, 0);
  lk1.unlock();
  BOOST_REQUIRE(lk2.try_lock());
  BOOST_CHECK(!app.m2.hasRead);
  BOOST_CHECK_EQUAL(app.m2.push1, 0);
  BOOST_CHECK_EQUAL(app.m2.poll1, 0);
  BOOST_CHECK_EQUAL(app.m2.poll2, 44);
  lk2.unlock();
}

/*********************************************************************************************************************/
