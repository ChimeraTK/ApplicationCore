// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <future>

#define BOOST_TEST_MODULE testPropagateDataFaultFlag

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "VariableGroup.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/DummyRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/NDRegisterAccessor.h>

#include <boost/mpl/list.hpp>

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/* dummy application */

/*********************************************************************************************************************/

struct TestModule1 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> i1{this, "i1", "", ""};
  ctk::ArrayPushInput<int> i2{this, "i2", "", 2, ""};
  ctk::ScalarPushInputWB<int> i3{this, "i3", "", ""};
  ctk::ScalarOutput<int> o1{this, "o1", "", ""};
  ctk::ArrayOutput<int> o2{this, "o2", "", 2, ""};
  void mainLoop() override {
    auto group = readAnyGroup();
    while(true) {
      if(i3 > 10) {
        i3 = 10;
        i3.write();
      }
      o1 = int(i1);
      o2[0] = i2[0];
      o2[1] = i2[1];
      o1.write();
      o2.write();
      group.readAny();
    }
  }
};

/*********************************************************************************************************************/

struct TestModule2 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> i1{this, "i1", "", ""};
  ctk::ArrayPushInput<int> i2{this, "i2", "", 2, ""};
  ctk::ScalarPushInputWB<int> i3{this, "i3", "", ""};
  ctk::ScalarPushInput<int> o1{this, "o1", "", ""};
  ctk::ArrayPushInput<int> o2{this, "o2", "", 2, ""};
  void mainLoop() override {
    auto group = readAnyGroup();
    while(true) {
      group.readAny();
    }
  }
};

/*********************************************************************************************************************/

struct TestApplication1 : ctk::Application {
  TestApplication1() : Application("testSuite") {}
  ~TestApplication1() override { shutdown(); }

  TestModule1 t1{this, "t1", ""};
};

/*********************************************************************************************************************/

struct TestApplication2 : ctk::Application {
  TestApplication2() : Application("testSuite") {}
  ~TestApplication2() override { shutdown(); }

  TestModule1 a{this, "A", ""};
  TestModule2 b{this, "A", ""};
};

/*********************************************************************************************************************/
/*********************************************************************************************************************/

// first test without FanOuts of any kind
BOOST_AUTO_TEST_CASE(testDirectConnections) {
  std::cout << "testDirectConnections" << std::endl;
  TestApplication1 app;
  app.debugMakeConnections();
  ctk::TestFacility test(app);

  auto i1 = test.getScalar<int>("/t1/i1");
  auto i2 = test.getArray<int>("/t1/i2");
  auto i3 = test.getScalar<int>("/t1/i3");
  auto o1 = test.getScalar<int>("/t1/o1");
  auto o2 = test.getArray<int>("/t1/o2");

  test.runApplication();

  // test if fault flag propagates to all outputs
  i1 = 1;
  i1.setDataValidity(ctk::DataValidity::faulty);
  i1.write();
  test.stepApplication();
  o1.read();
  o2.read();
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(o1), 1);
  BOOST_CHECK_EQUAL(o2[0], 0);
  BOOST_CHECK_EQUAL(o2[1], 0);

  // write another value but keep fault flag
  i1 = 42;
  BOOST_CHECK(i1.dataValidity() == ctk::DataValidity::faulty);
  i1.write();
  test.stepApplication();
  o1.read();
  o2.read();
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(o1), 42);
  BOOST_CHECK_EQUAL(o2[0], 0);
  BOOST_CHECK_EQUAL(o2[1], 0);

  // a write on the ok variable should not clear the flag
  i2[0] = 10;
  i2[1] = 11;
  BOOST_CHECK(i2.dataValidity() == ctk::DataValidity::ok);
  i2.write();
  test.stepApplication();
  o1.read();
  o2.read();
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(o1), 42);
  BOOST_CHECK_EQUAL(o2[0], 10);
  BOOST_CHECK_EQUAL(o2[1], 11);

  // the return channel should also receive the flag
  BOOST_CHECK(i3.readNonBlocking() == false);
  BOOST_CHECK(i3.dataValidity() == ctk::DataValidity::ok);
  i3 = 20;
  i3.write();
  test.stepApplication();
  o1.read();
  o2.read();
  i3.read();
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(i3.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(o1), 42);
  BOOST_CHECK_EQUAL(o2[0], 10);
  BOOST_CHECK_EQUAL(o2[1], 11);
  BOOST_CHECK_EQUAL(int(i3), 10);

  // clear the flag on i1, i3 will keep it for now (we have received it there and not yet sent it out!)
  i1 = 3;
  i1.setDataValidity(ctk::DataValidity::ok);
  i1.write();
  test.stepApplication();
  o1.read();
  o2.read();
  BOOST_CHECK(i3.readNonBlocking() == false);
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(int(o1), 3);
  BOOST_CHECK_EQUAL(o2[0], 10);
  BOOST_CHECK_EQUAL(o2[1], 11);
  BOOST_CHECK(i3.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(i3), 10);

  // send two data fault flags. both need to be cleared before the outputs go back to ok
  i1 = 120;
  i1.setDataValidity(ctk::DataValidity::faulty);
  i1.write();
  i3 = 121;
  i3.write();
  BOOST_CHECK(i3.dataValidity() == ctk::DataValidity::faulty);
  test.stepApplication();
  o1.readLatest();
  o2.readLatest();
  i3.read();
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(o1), 120);
  BOOST_CHECK_EQUAL(o2[0], 10);
  BOOST_CHECK_EQUAL(o2[1], 11);
  BOOST_CHECK(i3.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(i3), 10);

  // clear first flag
  i1 = 122;
  i1.setDataValidity(ctk::DataValidity::ok);
  i1.write();
  test.stepApplication();
  o1.read();
  o2.read();
  BOOST_CHECK(i3.readNonBlocking() == false);
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(o1), 122);
  BOOST_CHECK_EQUAL(o2[0], 10);
  BOOST_CHECK_EQUAL(o2[1], 11);
  BOOST_CHECK(i3.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(i3), 10);

  // clear second flag
  i3 = 123;
  i3.setDataValidity(ctk::DataValidity::ok);
  i3.write();
  test.stepApplication();
  o1.read();
  o2.read();
  i3.read();
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(o2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(int(o1), 122);
  BOOST_CHECK_EQUAL(o2[0], 10);
  BOOST_CHECK_EQUAL(o2[1], 11);
  BOOST_CHECK(i3.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(int(i3), 10);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testWithFanOut) {
  std::cout << "testWithFanOut" << std::endl;
  TestApplication2 app;
  ctk::TestFacility test(app);

  auto Ai1 = test.getScalar<int>("A/i1");
  auto Ai2 = test.getArray<int>("A/i2");
  auto Ai3 = test.getScalar<int>("A/i3");
  auto Ao1 = test.getScalar<int>("A/o1");
  auto Ao2 = test.getArray<int>("A/o2");

  test.runApplication();

  // app.dumpConnections();

  // test if fault flag propagates to all outputs
  Ai1 = 1;
  Ai1.setDataValidity(ctk::DataValidity::faulty);
  Ai1.write();
  test.stepApplication();
  Ao1.read();
  Ao2.read();
  BOOST_CHECK(Ao1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(Ao2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(Ao1), 1);
  BOOST_CHECK_EQUAL(Ao2[0], 0);
  BOOST_CHECK_EQUAL(Ao2[1], 0);
  BOOST_CHECK(app.b.o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(app.b.o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(app.b.o1), 1);
  BOOST_CHECK_EQUAL(app.b.o2[0], 0);
  BOOST_CHECK_EQUAL(app.b.o2[1], 0);
  BOOST_CHECK(app.b.i1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(app.b.i1), 1);

  // send fault flag on a second variable
  Ai2[0] = 2;
  Ai2[1] = 3;
  Ai2.setDataValidity(ctk::DataValidity::faulty);
  Ai2.write();
  test.stepApplication();
  Ao1.read();
  Ao2.read();
  BOOST_CHECK(Ao1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(Ao2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(Ao1), 1);
  BOOST_CHECK_EQUAL(Ao2[0], 2);
  BOOST_CHECK_EQUAL(Ao2[1], 3);
  BOOST_CHECK(app.b.o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(app.b.o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(app.b.o1), 1);
  BOOST_CHECK_EQUAL(app.b.o2[0], 2);
  BOOST_CHECK_EQUAL(app.b.o2[1], 3);
  BOOST_CHECK(app.b.i2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(app.b.i2[0], 2);
  BOOST_CHECK_EQUAL(app.b.i2[1], 3);

  // clear fault flag on a second variable
  Ai2[0] = 4;
  Ai2[1] = 5;
  Ai2.setDataValidity(ctk::DataValidity::ok);
  Ai2.write();
  test.stepApplication();
  Ao1.read();
  Ao2.read();
  BOOST_CHECK(Ao1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(Ao2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(Ao1), 1);
  BOOST_CHECK_EQUAL(Ao2[0], 4);
  BOOST_CHECK_EQUAL(Ao2[1], 5);
  BOOST_CHECK(app.b.o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(app.b.o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(int(app.b.o1), 1);
  BOOST_CHECK_EQUAL(app.b.o2[0], 4);
  BOOST_CHECK_EQUAL(app.b.o2[1], 5);
  BOOST_CHECK(app.b.i2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(app.b.i2[0], 4);
  BOOST_CHECK_EQUAL(app.b.i2[1], 5);

  // clear fault flag on a first variable
  Ai1 = 6;
  Ai1.setDataValidity(ctk::DataValidity::ok);
  Ai1.write();
  test.stepApplication();
  Ao1.read();
  Ao2.read();
  BOOST_CHECK(Ao1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(Ao2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(int(Ao1), 6);
  BOOST_CHECK_EQUAL(Ao2[0], 4);
  BOOST_CHECK_EQUAL(Ao2[1], 5);
  BOOST_CHECK(app.b.o1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(app.b.o2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(int(app.b.o1), 6);
  BOOST_CHECK_EQUAL(app.b.o2[0], 4);
  BOOST_CHECK_EQUAL(app.b.o2[1], 5);
  BOOST_CHECK(app.b.i1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(int(app.b.i1), 6);
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*
 * Tests below verify data fault flag propagation on:
 * - Threaded FanOut
 * - Consuming FanOut
 * - Triggers
 */

struct Module1 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> fromThreadedFanout{this, "o1", "", "", {"DEVICE1", "CS"}};

  // As a workaround the device side connection is done manually for
  // acheiving this consumingFanout; see:
  // TestApplication3::defineConnections
  ctk::ScalarPollInput<int> fromConsumingFanout{this, "i1", "", "", {"CS"}};

  ctk::ScalarPollInput<int> fromDevice{this, "i2", "", "", {"DEVICE2"}};
  ctk::ScalarOutput<int> result{this, "Module1_result", "", "", {"CS"}};

  void mainLoop() override {
    while(true) {
      result = fromConsumingFanout + fromThreadedFanout + fromDevice;
      writeAll();
      readAll(); // read last, so initial values are written in the first round
    }
  }
};

struct Module2 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  struct : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPushInput<int> result{this, "Module1_result", "", "", {"CS"}};
  } m1VarsFromCS{this, "../m1", ""}; // "m1" being in there
                                     // not good for a general case
  ctk::ScalarOutput<int> result{this, "Module2_result", "", "", {"CS"}};

  void mainLoop() override {
    while(true) {
      result = static_cast<int>(m1VarsFromCS.result);
      writeAll();
      readAll(); // read last, so initial values are written in the first round
    }
  }
};

// struct TestApplication3 : ctk::ApplicationModule {
struct TestApplication3 : ctk::Application {
  /*
   *   CS +-----> threaded fanout +------------------+
   *                  +                              v
   *                  +---------+                   +Device1+
   *                            |                   |       |
   *              Feeding       v                   |       |
   *   CS   <----- fanout --+ Module1 <-----+       v       |
   *                 |          ^           +Consuming      |
   *                 |          +--------+    fanout        |
   *                 +------+            +      +           |
   *                        v         Device2   |           |
   *   CS   <-----------+ Module2               |           |
   *                                            |           |
   *   CS   <-----------------------------------+           |
   *                                                        |
   *                                                        |
   *   CS   <-----------+ Trigger fanout <------------------+
   *                           ^
   *                           |
   *                           +
   *                           CS
   */

  constexpr static char const* ExceptionDummyCDD1 = "(ExceptionDummy:1?map=testDataValidity1.map)";
  constexpr static char const* ExceptionDummyCDD2 = "(ExceptionDummy:1?map=testDataValidity2.map)";
  TestApplication3() : Application("testDataFlagPropagation") {}
  ~TestApplication3() override { shutdown(); }

  Module1 m1{this, "m1", ""};
  Module2 m2{this, "m2", ""};

  ctk::DeviceModule device1{this, ExceptionDummyCDD1, "/trigger"};
  ctk::DeviceModule device2{this, ExceptionDummyCDD2, "/trigger"};

  /*  void defineConnections() override {
      device1["m1"]("i1") >> m1("i1");
      findTag("CS").connectTo(cs);
      findTag("DEVICE1").connectTo(device1);
      findTag("DEVICE2").connectTo(device2);
      device1["m1"]("i3")[cs("trigger", typeid(int), 1)] >> cs("i3", typeid(int), 1);
    }*/
};

/*********************************************************************************************************************/
/*********************************************************************************************************************/

struct FixtureTestFacility {
  FixtureTestFacility()
  : device1DummyBackend(boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(TestApplication3::ExceptionDummyCDD1))),
    device2DummyBackend(boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(TestApplication3::ExceptionDummyCDD2))) {
    device1DummyBackend->open();
    device2DummyBackend->open();
    test.runApplication();
  }

  ~FixtureTestFacility() {
    device1DummyBackend->throwExceptionRead = false;
    device2DummyBackend->throwExceptionWrite = false;
  }

  boost::shared_ptr<ctk::ExceptionDummy> device1DummyBackend;
  boost::shared_ptr<ctk::ExceptionDummy> device2DummyBackend;
  TestApplication3 app;
  ctk::TestFacility test{app};
};

BOOST_FIXTURE_TEST_SUITE(data_validity_propagation, FixtureTestFacility)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testThreadedFanout) {
  std::cout << "testThreadedFanout" << std::endl;
  auto threadedFanoutInput = test.getScalar<int>("m1/o1");
  auto m1_result = test.getScalar<int>("m1/Module1_result");
  auto m2_result = test.getScalar<int>("m2/Module2_result");

  threadedFanoutInput = 20;
  threadedFanoutInput.write();
  // write to register: m1.i1 linked with the consumingFanout.
  auto consumingFanoutSource =
      ctk::Device(TestApplication3::ExceptionDummyCDD1).getScalarRegisterAccessor<int>("/m1/i1/DUMMY_WRITEABLE");
  consumingFanoutSource = 10;
  consumingFanoutSource.write();

  auto pollRegister =
      ctk::Device(TestApplication3::ExceptionDummyCDD2).getScalarRegisterAccessor<int>("/m1/i2/DUMMY_WRITEABLE");
  pollRegister = 5;
  pollRegister.write();

  test.stepApplication();

  m1_result.read();
  m2_result.read();
  BOOST_CHECK_EQUAL(m1_result, 35);
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::ok);

  BOOST_CHECK_EQUAL(m2_result, 35);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::ok);

  threadedFanoutInput = 10;
  threadedFanoutInput.setDataValidity(ctk::DataValidity::faulty);
  threadedFanoutInput.write();
  test.stepApplication();

  m1_result.read();
  m2_result.read();
  BOOST_CHECK_EQUAL(m1_result, 25);
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK_EQUAL(m2_result, 25);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::faulty);

  threadedFanoutInput = 40;
  threadedFanoutInput.setDataValidity(ctk::DataValidity::ok);
  threadedFanoutInput.write();
  test.stepApplication();

  m1_result.read();
  m2_result.read();
  BOOST_CHECK_EQUAL(m1_result, 55);
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK_EQUAL(m2_result, 55);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::ok);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testInvalidTrigger) {
  std::cout << "testInvalidTrigger" << std::endl;

  auto deviceRegister =
      ctk::Device(TestApplication3::ExceptionDummyCDD1).getScalarRegisterAccessor<int>("/m1/i3/DUMMY_WRITEABLE");
  deviceRegister = 20;
  deviceRegister.write();

  auto trigger = test.getVoid("trigger");
  auto result = test.getScalar<int>("/m1/i3"); // Cs hook into reg: m1.i3

  //----------------------------------------------------------------//
  // trigger works as expected
  trigger.write();

  test.stepApplication();

  result.read();
  BOOST_CHECK_EQUAL(result, 20);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);

  //----------------------------------------------------------------//
  // faulty trigger
  deviceRegister = 30;
  deviceRegister.write();
  trigger.setDataValidity(ctk::DataValidity::faulty);
  trigger.write();

  test.stepApplication();

  result.read();
  BOOST_CHECK_EQUAL(result, 30);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  //----------------------------------------------------------------//
  // recovery
  deviceRegister = 50;
  deviceRegister.write();

  trigger.setDataValidity(ctk::DataValidity::ok);
  trigger.write();

  test.stepApplication();

  result.read();
  BOOST_CHECK_EQUAL(result, 50);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);
}

BOOST_AUTO_TEST_SUITE_END()

/*********************************************************************************************************************/
/*********************************************************************************************************************/

struct FixtureNoTestableMode {
  FixtureNoTestableMode()
  : device1DummyBackend(boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(TestApplication3::ExceptionDummyCDD1))),
    device2DummyBackend(boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(TestApplication3::ExceptionDummyCDD2))) {
    device1Status.replace(test.getScalar<int32_t>(ctk::RegisterPath("/Devices") /
        ctk::Utilities::stripName(TestApplication3::ExceptionDummyCDD1, false) / "status"));
    device2Status.replace(test.getScalar<int32_t>(ctk::RegisterPath("/Devices") /
        ctk::Utilities::stripName(TestApplication3::ExceptionDummyCDD2, false) / "status"));

    device1DummyBackend->open();
    device2DummyBackend->open();
  }

  void waitForDevices() {
    // the block below is a work around for a race condition; make sure
    // all values are propagated to the device registers before starting
    // the test.
    static const int DEFAULT = 1234567;
    test.setScalarDefault("m1/o1", DEFAULT);

    test.runApplication();
    CHECK_EQUAL_TIMEOUT((device1Status.readLatest(), device1Status), 0, 100000);
    CHECK_EQUAL_TIMEOUT((device2Status.readLatest(), device2Status), 0, 100000);

    // Making sure the default is written to the device before proceeding.
    auto m1o1 = device1DummyBackend->getRegisterAccessor<int>("m1/o1", 1, 0, {});
    CHECK_EQUAL_TIMEOUT((m1o1->read(), m1o1->accessData(0)), DEFAULT, 10000);
  }

  ~FixtureNoTestableMode() {
    device1DummyBackend->throwExceptionRead = false;
    device2DummyBackend->throwExceptionWrite = false;
  }

  boost::shared_ptr<ctk::ExceptionDummy> device1DummyBackend;
  boost::shared_ptr<ctk::ExceptionDummy> device2DummyBackend;
  TestApplication3 app;
  ctk::TestFacility test{app, false};
  ChimeraTK::ScalarRegisterAccessor<int> device1Status;
  ChimeraTK::ScalarRegisterAccessor<int> device2Status;
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE(data_validity_propagation_noTestFacility)

BOOST_FIXTURE_TEST_CASE(testDeviceReadFailure, FixtureNoTestableMode) {
  std::cout << "testDeviceReadFailure" << std::endl;
  waitForDevices();

  auto consumingFanoutSource =
      ctk::Device(TestApplication3::ExceptionDummyCDD1).getScalarRegisterAccessor<int>("/m1/i1/DUMMY_WRITEABLE");
  auto pollRegister =
      ctk::Device(TestApplication3::ExceptionDummyCDD2).getScalarRegisterAccessor<int>("/m1/i2/DUMMY_WRITEABLE");

  auto threadedFanoutInput = test.getScalar<int>("m1/o1");
  auto result = test.getScalar<int>("m1/Module1_result");

  threadedFanoutInput = 10000;
  consumingFanoutSource = 1000;
  consumingFanoutSource.write();
  pollRegister = 1;
  pollRegister.write();

  // -------------------------------------------------------------//
  // without errors
  threadedFanoutInput.write();

  CHECK_TIMEOUT((result.readLatest(), result == 11001), 10000);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);

  // -------------------------------------------------------------//
  // device module exception
  threadedFanoutInput = 20000;
  pollRegister = 0;
  pollRegister.write();

  device2DummyBackend->throwExceptionRead = true;

  threadedFanoutInput.write();
  // The new value from the fanout input should have been propagated,
  // the new value of the poll input is not seen, because it gets skipped
  result.read();
  BOOST_CHECK_EQUAL(result, 21001);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  // make sure the device module has seen the exception
  CHECK_EQUAL_TIMEOUT((device2Status.readLatest(), device2Status), 1, 100000);

  // -------------------------------------------------------------//

  threadedFanoutInput = 30000;
  threadedFanoutInput.write();
  // Further reads to the poll input are skipped
  result.read();
  BOOST_CHECK_EQUAL(result, 31001);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  // -------------------------------------------------------------//

  // recovery from device module exception
  device2DummyBackend->throwExceptionRead = false;
  CHECK_EQUAL_TIMEOUT((device2Status.readLatest(), device2Status), 0, 100000);

  threadedFanoutInput = 40000;
  threadedFanoutInput.write();
  result.read();
  // Now we expect also the last value written to the pollRegister being
  // propagated and the DataValidity should be ok again.
  BOOST_CHECK_EQUAL(result, 41000);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);
}

/*********************************************************************************************************************/

BOOST_FIXTURE_TEST_CASE(testReadDeviceWithTrigger, FixtureNoTestableMode) {
  std::cout << "testReadDeviceWithTrigger" << std::endl;
  waitForDevices();

  auto trigger = test.getVoid("trigger");
  auto fromDevice = test.getScalar<int>("/m1/i3"); // cs side display: m1.i3
  //----------------------------------------------------------------//
  fromDevice.read(); // there is an initial value
  BOOST_CHECK_EQUAL(fromDevice, 0);

  auto deviceRegister =
      ctk::Device(TestApplication3::ExceptionDummyCDD1).getScalarRegisterAccessor<int>("/m1/i3/DUMMY_WRITEABLE");
  deviceRegister = 30;
  deviceRegister.write();

  // trigger works as expected
  trigger.write();

  fromDevice.read();
  BOOST_CHECK_EQUAL(fromDevice, 30);
  BOOST_CHECK(fromDevice.dataValidity() == ctk::DataValidity::ok);

  //----------------------------------------------------------------//
  // Device module exception
  deviceRegister = 10;
  deviceRegister.write();

  device1DummyBackend->throwExceptionRead = true;

  trigger.write();

  fromDevice.read();
  BOOST_CHECK_EQUAL(fromDevice, 30);
  BOOST_CHECK(fromDevice.dataValidity() == ctk::DataValidity::faulty);
  //----------------------------------------------------------------//
  // Recovery
  device1DummyBackend->throwExceptionRead = false;

  // Wait until the device has recovered. Otherwise the read might be skipped and we still read the previous value with
  // the faulty flag.
  while((void)device1Status.read(), device1Status == 1) {
    usleep(1000);
  }

  trigger.write();

  fromDevice.read();
  BOOST_CHECK_EQUAL(fromDevice, 10);
  BOOST_CHECK(fromDevice.dataValidity() == ctk::DataValidity::ok);
}

/*********************************************************************************************************************/

BOOST_FIXTURE_TEST_CASE(testConsumingFanout, FixtureNoTestableMode) {
  std::cout << "testConsumingFanout" << std::endl;
  waitForDevices();

  auto threadedFanoutInput = test.getScalar<int>("m1/o1");
  auto fromConsumingFanout = test.getScalar<int>("m1/i1"); // consumingfanout variable on cs side
  auto result = test.getScalar<int>("m1/Module1_result");
  fromConsumingFanout.read(); // initial value, don't care for this test
  result.read();              // initial value, don't care for this test

  auto pollRegisterSource =
      ctk::Device(TestApplication3::ExceptionDummyCDD2).getScalarRegisterAccessor<int>("/m1/i2.DUMMY_WRITEABLE");
  pollRegisterSource = 100;
  pollRegisterSource.write();

  threadedFanoutInput = 10;

  auto consumingFanoutSource =
      ctk::Device(TestApplication3::ExceptionDummyCDD1).getScalarRegisterAccessor<int>("/m1/i1.DUMMY_WRITEABLE");
  consumingFanoutSource = 1;
  consumingFanoutSource.write();

  //----------------------------------------------------------//
  // no device module exception
  threadedFanoutInput.write();

  result.read();
  BOOST_CHECK_EQUAL(result, 111);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);

  fromConsumingFanout.read();
  BOOST_CHECK_EQUAL(fromConsumingFanout, 1);
  BOOST_CHECK(fromConsumingFanout.dataValidity() == ctk::DataValidity::ok);

  // --------------------------------------------------------//
  // device exception on consuming fanout source read
  consumingFanoutSource = 0;
  consumingFanoutSource.write();

  device1DummyBackend->throwExceptionRead = true;
  threadedFanoutInput = 20;
  threadedFanoutInput.write();

  CHECK_TIMEOUT(result.readLatest(), 10000);
  BOOST_CHECK_EQUAL(result, 121);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  CHECK_TIMEOUT(fromConsumingFanout.readLatest(), 10000);
  BOOST_CHECK_EQUAL(fromConsumingFanout, 1);
  BOOST_CHECK(fromConsumingFanout.dataValidity() == ctk::DataValidity::faulty);

  // --------------------------------------------------------//
  // Recovery
  device1DummyBackend->throwExceptionRead = false;

  // Wait until the device has recovered. Otherwise the read might be skipped and we still read the previous value with
  // the faulty flag.
  while((void)device1Status.read(), device1Status == 1) {
    usleep(1000);
  }

  threadedFanoutInput = 30;
  threadedFanoutInput.write();

  CHECK_TIMEOUT(result.readLatest(), 10000);
  BOOST_CHECK_EQUAL(result, 130);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);

  CHECK_TIMEOUT(fromConsumingFanout.readLatest(), 10000);
  BOOST_CHECK_EQUAL(fromConsumingFanout, 0);
  BOOST_CHECK(fromConsumingFanout.dataValidity() == ctk::DataValidity::ok);
}

/*********************************************************************************************************************/

BOOST_FIXTURE_TEST_CASE(testDataFlowOnDeviceException, FixtureNoTestableMode) {
  std::cout << "testDataFlowOnDeviceException" << std::endl;

  auto threadedFanoutInput = test.getScalar<int>("m1/o1");
  auto m1_result = test.getScalar<int>("m1/Module1_result");
  auto m2_result = test.getScalar<int>("m2/Module2_result");

  auto consumingFanoutSource = ctk::ScalarRegisterAccessor<int>(
      device1DummyBackend->getRegisterAccessor<int>("/m1/i1.DUMMY_WRITEABLE", 0, 0, {}));
  consumingFanoutSource = 1000;
  consumingFanoutSource.write();

  auto pollRegister = ctk::ScalarRegisterAccessor<int>(
      device2DummyBackend->getRegisterAccessor<int>("/m1/i2.DUMMY_WRITEABLE", 0, 0, {}));
  pollRegister = 100;
  pollRegister.write();

  waitForDevices();

  // get rid of initial values
  m1_result.read();
  m2_result.read();

  threadedFanoutInput = 1;

  // ------------------------------------------------------------------//
  // without exception
  threadedFanoutInput.write();

  CHECK_TIMEOUT(m1_result.readNonBlocking(), 10000);
  BOOST_CHECK_EQUAL(m1_result, 1101);
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::ok);

  CHECK_TIMEOUT(m2_result.readNonBlocking(), 10000);
  BOOST_CHECK_EQUAL(m2_result, 1101);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::ok);

  // ------------------------------------------------------------------//
  // faulty threadedFanout input
  threadedFanoutInput.setDataValidity(ctk::DataValidity::faulty);
  threadedFanoutInput.write();

  CHECK_TIMEOUT(m1_result.readNonBlocking(), 10000);
  BOOST_CHECK_EQUAL(m1_result, 1101);
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::faulty);

  CHECK_TIMEOUT(m2_result.readLatest(), 10000);
  BOOST_CHECK_EQUAL(m2_result, 1101);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::faulty);

  auto deviceStatus = test.getScalar<int32_t>(ctk::RegisterPath("/Devices") /
      ChimeraTK::Utilities::stripName(TestApplication3::ExceptionDummyCDD2, false) / "status");
  // the device is still OK
  CHECK_EQUAL_TIMEOUT((deviceStatus.readLatest(), deviceStatus), 0, 10000);

  // ---------------------------------------------------------------------//
  // device module exception
  device2DummyBackend->throwExceptionRead = true;
  pollRegister = 200;
  pollRegister.write();
  threadedFanoutInput = 0;
  threadedFanoutInput.write();

  // Now the device has to go into the error state
  CHECK_EQUAL_TIMEOUT((deviceStatus.readLatest(), deviceStatus), 1, 10000);

  // The new value of the threadedFanoutInput should be propagated, the
  // pollRegister is skipped, see testDataValidPropagationOnException.
  m1_result.read();
  BOOST_CHECK_EQUAL(m1_result, 1100);
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::faulty);
  m2_result.read();
  // Same for m2
  BOOST_CHECK_EQUAL(m2_result, 1100);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::faulty);

  // ---------------------------------------------------------------------//
  // device exception recovery
  device2DummyBackend->throwExceptionRead = false;

  // device error recovers. There must be exactly one new status values with the right value.
  deviceStatus.read();
  BOOST_CHECK(deviceStatus == 0);
  // nothing else in the queue
  BOOST_CHECK(deviceStatus.readNonBlocking() == false);

  // ---------------------------------------------------------------------//
  // Now both, threadedFanoutInput and pollRegister should propagate
  pollRegister = 300;
  pollRegister.write();
  threadedFanoutInput = 2;
  threadedFanoutInput.write();

  m1_result.read();
  BOOST_CHECK_EQUAL(m1_result, 1302);
  // Data validity still faulty because the input from the fan is invalid
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::faulty);
  // again, nothing else in the queue
  BOOST_CHECK(m1_result.readNonBlocking() == false);

  // same for m2
  m2_result.read();
  BOOST_CHECK_EQUAL(m2_result, 1302);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(m2_result.readNonBlocking() == false);

  // ---------------------------------------------------------------------//
  // recovery: fanout input
  threadedFanoutInput = 3;
  threadedFanoutInput.setDataValidity(ctk::DataValidity::ok);
  threadedFanoutInput.write();

  m1_result.read();
  BOOST_CHECK_EQUAL(m1_result, 1303);
  BOOST_CHECK(m1_result.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(m1_result.readNonBlocking() == false);

  m2_result.read();
  BOOST_CHECK_EQUAL(m2_result, 1303);
  BOOST_CHECK(m2_result.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(m1_result.readNonBlocking() == false);
}

BOOST_AUTO_TEST_SUITE_END()

/*********************************************************************************************************************/
/*********************************************************************************************************************/

// Module and Application for test case "testDataValidPropagationOnException"
struct Module3 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> pushTypeInputFromCS{this, "o1", "", "", {"CS"}};

  ctk::ScalarPollInput<int> pollInputFromDevice{this, "/m1/i2", "", "", {"DEVICE2"}};
  ctk::ScalarOutput<int> result{this, "Module3_result", "", "", {"CS"}};

  void mainLoop() override {
    while(true) {
      result = pushTypeInputFromCS + pollInputFromDevice;
      result.write();
      readAll(); // read last, so initial values are written in the first round
    }
  }
};

struct TestApplication4 : ctk::Application {
  /*
   *
   */

  constexpr static char const* ExceptionDummyCDD2 = "(ExceptionDummy:1?map=testDataValidity2.map)";
  TestApplication4() : Application("testDataFlagPropagation") {}
  ~TestApplication4() override { shutdown(); }

  Module3 module{this, "module", ""};

  ctk::DeviceModule device2{this, ExceptionDummyCDD2};

  /*  void defineConnections() override {
      findTag("CS").connectTo(cs);
      findTag("DEVICE2").flatten().connectTo(device2["m1"]);
    }*/
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDataValidPropagationOnException) {
  std::cout << "testDataValidPropagationOnException" << std::endl;

  boost::shared_ptr<ctk::ExceptionDummy> device2DummyBackend(boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(TestApplication3::ExceptionDummyCDD2)));
  ctk::Device device2(TestApplication3::ExceptionDummyCDD2);

  TestApplication4 app;
  ctk::TestFacility test{app, false};

  auto pollRegister = device2.getScalarRegisterAccessor<int>("/m1/i2.DUMMY_WRITEABLE");
  device2.open();
  pollRegister = 1;
  pollRegister.write();
  device2.close();

  test.runApplication();

  auto pushInput = test.getScalar<int>("module/o1");
  auto result = test.getScalar<int>("module/Module3_result");

  auto deviceStatus = test.getScalar<int32_t>(ctk::RegisterPath("/Devices") /
      ctk::Utilities::stripName(TestApplication3::ExceptionDummyCDD2, false) / "status");

  pushInput = 10;
  pushInput.write();

  CHECK_TIMEOUT((result.readLatest(), result == 11), 10000);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);
  CHECK_EQUAL_TIMEOUT((deviceStatus.readLatest(), deviceStatus), 0, 10000);

  // Set data validity to faulty and trigger excetion in the same update
  pollRegister = 2;
  pollRegister.write();
  pushInput = 20;
  pushInput.setDataValidity(ctk::DataValidity::faulty);
  device2DummyBackend->throwExceptionRead = true;
  pushInput.write();

  CHECK_EQUAL_TIMEOUT((deviceStatus.readLatest(), deviceStatus), 1, 10000);
  result.read();
  BOOST_CHECK(result.readLatest() == false);
  // The new data from the pushInput and the DataValidity::faulty should have been propagated to the outout,
  // the pollRegister should be skipped (Exceptionhandling spec B.2.2.3), so we don't expect the latest assigned value of 2
  BOOST_CHECK_EQUAL(result, 21);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  // Writeing the pushInput should still trigger module execution and
  // update the result value. Result validity should still be faulty because
  // the device still has the exception
  pushInput = 30;
  pushInput.setDataValidity(ctk::DataValidity::ok);
  pushInput.write();
  result.read();
  BOOST_CHECK_EQUAL(result, 31);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  // let the device recover
  device2DummyBackend->throwExceptionRead = false;
  CHECK_EQUAL_TIMEOUT((deviceStatus.readLatest(), deviceStatus), 0, 10000);

  // Everything should be back to normal, also the value of the pollRegister
  // should be reflected in the output
  pushInput = 40;
  pollRegister = 3;
  pollRegister.write();
  pushInput.write();
  result.read();
  BOOST_CHECK_EQUAL(result, 43);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);
  // nothing more in the queue
  BOOST_CHECK(result.readLatest() == false);

  // Check if we get faulty output from the exception alone,
  // keep pushInput ok
  pollRegister = 4;
  pollRegister.write();
  pushInput = 50;
  device2DummyBackend->throwExceptionRead = true;

  pushInput.write();
  result.read();
  BOOST_CHECK(result.readLatest() == false);
  // The new data from the pushInput, the device exception should yield DataValidity::faulty at the outout,
  BOOST_CHECK_EQUAL(result, 53);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  // Device status should report fault. We need to wait for it here to make sure the DeviceModule has seen the fault.
  CHECK_EQUAL_TIMEOUT((deviceStatus.readLatest(), deviceStatus), 1, 10000);

  // Also set pushInputValidity to faulty
  pushInput = 60;
  pushInput.setDataValidity(ctk::DataValidity::faulty);
  pushInput.write();
  result.read();
  BOOST_CHECK_EQUAL(result, 63);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  // let the device recover
  device2DummyBackend->throwExceptionRead = false;
  CHECK_EQUAL_TIMEOUT((deviceStatus.readLatest(), deviceStatus), 0, 10000);

  // The new pollRegister value should now be reflected in the result,
  // but it's still faulty from the pushInput
  pushInput = 70;
  pollRegister = 5;
  pollRegister.write();
  pushInput.write();
  result.read();
  BOOST_CHECK_EQUAL(result, 75);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);

  // MAke pushInput ok, everything should be back to normal
  pushInput = 80;
  pushInput.setDataValidity(ctk::DataValidity::ok);
  pollRegister = 6;
  pollRegister.write();
  pushInput.write();
  result.read();
  BOOST_CHECK_EQUAL(result, 86);
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::ok);
  // nothing more in the queue
  BOOST_CHECK(result.readLatest() == false);
}

/*********************************************************************************************************************/

struct TestModule3 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarOutput<int> o1{this, "o1", "", ""};
  ctk::ArrayOutput<int> o2{this, "o2", "", 2, ""};
  void mainLoop() override {}
};

/*********************************************************************************************************************/

struct TestApplication5 : ctk::Application {
  TestApplication5() : Application("testSuite") {}
  ~TestApplication5() override { shutdown(); }

  TestModule3 a{this, "A", ""};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testWriteIfDifferent) {
  std::cout << "testWriteIfDifferent" << std::endl;

  TestApplication5 app;
  ctk::TestFacility test{app, false};

  auto o1 = test.getScalar<int>("/A/o1");
  auto o2 = test.getArray<int>("/A/o2");

  test.runApplication();

  // initialise in defined conditions
  app.a.o1 = 42;
  app.a.o1.write();
  BOOST_TEST(o1.readLatest());
  BOOST_TEST(o1.dataValidity() == ctk::DataValidity::ok);
  BOOST_TEST(int(o1) == 42);

  app.a.o2 = {48, 59};
  app.a.o2.write();
  BOOST_TEST(o2.readLatest());
  BOOST_TEST(o2.dataValidity() == ctk::DataValidity::ok);
  BOOST_TEST(std::vector<int>(o2) == std::vector<int>({48, 59}));

  // set module to faulty and write same value with writeIfDifferent again: faulty flag should be propagated
  app.a.incrementDataFaultCounter();
  app.a.o1.writeIfDifferent(42);
  BOOST_TEST(o1.readNonBlocking());
  BOOST_TEST(o1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_TEST(int(o1) == 42);

  // repeat with array
  app.a.o2.writeIfDifferent({48, 59});
  BOOST_TEST(o2.readNonBlocking());
  BOOST_TEST(o2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_TEST(std::vector<int>(o2) == std::vector<int>({48, 59}));

  // repeat with ok validity
  app.a.decrementDataFaultCounter();
  app.a.o1.writeIfDifferent(42);
  BOOST_TEST(o1.readNonBlocking());
  BOOST_TEST(o1.dataValidity() == ctk::DataValidity::ok);
  BOOST_TEST(int(o1) == 42);

  app.a.o2.writeIfDifferent({48, 59});
  BOOST_TEST(o2.readNonBlocking());
  BOOST_TEST(o2.dataValidity() == ctk::DataValidity::ok);
  BOOST_TEST(std::vector<int>(o2) == std::vector<int>({48, 59}));
}

/*********************************************************************************************************************/
