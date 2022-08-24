#define BOOST_TEST_MODULE testDataValidityPropagation

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "ControlSystemModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/Device.h>
#include <ChimeraTK/DummyRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/NDRegisterAccessor.h>

#include <boost/mpl/list.hpp>

#include <chrono>
#include <cstring>

// this #include must come last
#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

// A module used for initial value tests:
// It has an output which is never written to.
struct TestModule0 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> i1{this, "i1", "", ""};
  ctk::ScalarOutput<int> oNothing{this, "oNothing", "", ""};
  void mainLoop() override {
    auto group = readAnyGroup();
    while(true) {
      group.readAny();
    }
  }
};

// module for most of hte data validity propagation tests
struct TestModule1 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> i1{this, "i1", "", ""};
  ctk::ScalarOutput<int> o1{this, "o1", "", ""};
  ctk::ScalarOutput<int> oconst{this, "oconst", "", ""};
  void mainLoop() override {
    oconst = 1;
    oconst.setDataValidity(outputValidity);
    oconst.write();
    // also provide initial value for o1, in case some module waits on it.
    // this is important for the testable mode
    o1 = -1;
    o1.setDataValidity(outputValidity);
    o1.write();
    auto group = readAnyGroup();
    while(true) {
      group.readAny();
      o1 = int(i1);
      o1.setDataValidity(outputValidity);
      o1.write();
    }
  }
  // used for overwriting the outputs validities.
  ctk::DataValidity outputValidity = ctk::DataValidity::ok;
  int incCalled = 0;
  int decCalled = 0;
  void incrementDataFaultCounter() override {
    incCalled++;
    ctk::ApplicationModule::incrementDataFaultCounter();
  }
  void decrementDataFaultCounter() override {
    decCalled++;
    ctk::ApplicationModule::decrementDataFaultCounter();
  }
};

struct TestModule2 : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPushInput<int> i1{this, "i1", "", ""};
  bool dataValidity1 = true, dataValidity2 = true;
  void mainLoop() override {
    auto group = readAnyGroup();
    while(true) {
      dataValidity1 = this->getDataValidity() == ctk::DataValidity::ok;
      this->incrementDataFaultCounter();
      dataValidity2 = this->getDataValidity() == ctk::DataValidity::ok;
      this->decrementDataFaultCounter();

      group.readAny();
    }
  }
};

struct TriggerModule : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarOutput<int> o1{this, "o1", "", ""};
  void mainLoop() override {
    // do nothing, we trigger o1 manually in the tests
    //    while(true) {
    //      // using boost sleep helps for the interruption point
    //      boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
    //      o1=1;
    //      o1.write();
    //    }
  }
};

template<typename ModuleT>
struct TestApplication1 : ctk::Application {
  TestApplication1() : Application("testSuite") {}
  ~TestApplication1() { shutdown(); }

  void defineConnections() { findTag(".*").connectTo(cs); }

  ModuleT mod{this, "m1", ""};
  ctk::ControlSystemModule cs;

  TriggerModule m2{this, "m2", ""};
  ctk::ConnectingDeviceModule dev{
      this, "(ExceptionDummy?map=testDataValidityPropagation.map)", "/m2/o1", &initialiseDev};
  std::atomic_bool deviceError = false;
  static void initialiseDev(ChimeraTK::DeviceModule*) {
    bool err = ((TestApplication1&)Application::getInstance()).deviceError;
    if(err) {
      throw ChimeraTK::runtime_error("device is not ready.");
    }
  }
};

// BOOST_AUTO_TEST_SUITE(dataValidityPropagation)

struct TestApplication3 : ctk::Application {
  constexpr static char const* ExceptionDummyCDD = "(ExceptionDummy?map=testDataValidityPropagation2.map)";
  TestApplication3() : Application("testPartiallyInvalidDevice") {}
  ~TestApplication3() { shutdown(); }

  TriggerModule m2{this, "m2", ""};

  ctk::ControlSystemModule cs;
  ctk::ConnectingDeviceModule device1{this, ExceptionDummyCDD, "/m2/o1"};

  // ctk::DeviceModule device1{this, ExceptionDummyCDD};
  //   void defineConnections() override {
  //     // use manual connection setup instead of ConnectingDeviceModule.
  //     // note, differently from automatic connection setup, registers contain . and not / as separator
  //     auto pollInput1 = device1("dev.i1", typeid(int), 1, ChimeraTK::UpdateMode::poll);
  //     auto pollInput2 = device1("dev.i2", typeid(int), 1, ChimeraTK::UpdateMode::poll);
  //     auto trigger = m2("o1");
  //     pollInput1[trigger] >> cs("dev.i1", typeid(int));
  //     pollInput2[trigger] >> cs("dev.i2", typeid(int));
  //   }
};
/**
 *  tests the ExceptionDummyPollDecorator of the ExceptionDummyBackend, which provides a way for
 *  forcing individual (poll-type) device outputs to DataValidity=faulty
 */
BOOST_AUTO_TEST_CASE(testDataValidity_exceptionDummy) {
  TestApplication3 app;
  ctk::TestFacility test;
  // app.dumpConnections();

  auto devI1 = test.getScalar<int>("/dev/i1");
  auto devI2 = test.getScalar<int>("/dev/i2");
  test.runApplication();
  // it seems that the exceptionDummy is created only when app starts.
  auto exceptionDummy =
      boost::dynamic_pointer_cast<ctk::ExceptionDummy>(app.device1.getDeviceModule().device.getBackend());
  assert(exceptionDummy);
  exceptionDummy->setValidity("/dev/i1", ctk::DataValidity::faulty);

  app.m2.o1 = 1;
  app.m2.o1.write();
  test.stepApplication();
  devI1.read();
  devI2.read();
  BOOST_CHECK(devI1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(devI2.dataValidity() == ctk::DataValidity::ok);
}

/*
 * \anchor testDataValidity_1_1 \ref dataValidity_1_1 "1.1"
 * In ApplicationCore each variable has a data validiy flag attached to it. DataValidity can be 'ok' or 'faulty'.
 *
 * Explicit test does not make sense since this is clear if this suite compiles,
 * i.e. expressions testmod1.i1.dataValidity() and testmod1.o1.dataValidity();
 */

/*
 * \anchor testDataValidity_1_2 \ref dataValidity_1_2 "1.2"
 * This flag is automatically propagated: If any of the inputs of an ApplicationModule is faulty,
 * the data validity of the module becomes faulty, which means all outputs of this module will automatically be
 * flagged as faulty.
 * Fan-outs might be special cases (see 2.4).
 *
 * See \ref testDataValidity_2_3_3
 */

/**
 * \anchor testDataValidity_1_3 \ref dataValidity_1_3 "1.3"
 * If a device is in error state, all variables which are read from it shall be marked as 'faulty'. This flag is then
 * propagated through all the modules (via 1.2) so it shows up in the control system.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_1_3) {
  // set up an application with a faulty device
  TestApplication1<TestModule1> app;
  app.deviceError = true;
  // testable mode cannot be used here, since it would wait on initial values (which are not provided)
  ctk::TestFacility test(false);

  auto i1 = test.getScalar<int>("/dev/i1");
  test.runApplication();

  // i1.read() would block here
  i1.readLatest();

  BOOST_CHECK(i1.dataValidity() == ctk::DataValidity::faulty);

  // if Application does not shutdown, this could be an applicationcore bug that requires a workaround for this test
  // redmine issue: #8550
}

/*
 * \anchor testDataValidity_1_4 \ref dataValidity_1_4 "1.4"
 * The user code has the possibility to query the data validity of the module
 *
 * No explicit test, if test suite compiles it's ok.
 */

/*
 * \anchor testDataValidity_1_5 \ref dataValidity_1_5 "1.5"
 * The user code has the possibility to set the data validity of the module to 'faulty'. However, the user code cannot
 * actively set the module to 'ok' if any of the module inputs are 'faulty'.
 *
 * No explicit test. The module should use increment/decrement mechanism to set invalid state,
 * which implies it cannot override faulty state.
 * BUT it is actually possible to override getDataValidity in the Module, so spec does not hold strictly!
 *
 */

/**
 * app with two chained modules, for \ref testDataValidity_1_6
 */
struct TestApplication16 : ctk::Application {
  TestApplication16() : Application("testSuite") {}
  ~TestApplication16() { shutdown(); }

  void defineConnections() {
    mod1("o1") >> mod2("i1");
    findTag(".*").connectTo(cs);
  }

  TestModule1 mod1{this, "m1", ""};
  TestModule1 mod2{this, "m2", ""};
  ctk::ControlSystemModule cs;
};

/**
 * \anchor testDataValidity_1_6 \ref dataValidity_1_6 "1.6"
 * The user code can flag individual outputs as bad. However, the user code cannot actively set an output to 'ok' if
 * the data validity of the module is 'faulty'.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_1_6) {
  TestApplication16 app;
  app.mod1.outputValidity = ctk::DataValidity::faulty;
  app.mod2.outputValidity = ctk::DataValidity::ok;
  ctk::TestFacility test;

  // app.dumpConnections();
  auto input = test.getScalar<int>("/m1/i1");
  auto result = test.getScalar<int>("/m2/o1");
  test.runApplication();
  input.write();
  test.stepApplication();
  input.write();
  test.stepApplication();

  // module 2 must be flagged bad because of the faulty input from module 1
  BOOST_CHECK(app.mod2.getDataValidity() == ctk::DataValidity::faulty);
  result.read();
  // output of module 2 cannot be valid, even if module tries to set it to valid
  BOOST_CHECK(result.dataValidity() == ctk::DataValidity::faulty);
}

/*
 * \anchor testDataValidity_1_7 \ref dataValidity_1_7 "1.7"
 *  The user code can get the data validity flag of individual inputs and take special actions.
 *
 *  No explicit test required.
 */

/**
 * \anchor testDataValidity_1_8 \ref dataValidity_1_8 "1.8"
 * The data validity of receiving variables is set to 'faulty' on construction. Like this, data is marked as faulty as
 * long as no sensible initial values have been propagated.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_1_8) {
  TestApplication1<TestModule0> app;
  // testable mode cannot be used here, since it would wait on initial values (which are not provided)
  ctk::TestFacility test(false);

  auto o0 = test.getScalar<int>("/m1/oNothing");
  test.runApplication();

  // o0.read() would block here
  BOOST_CHECK(o0.readNonBlocking() == false);

  BOOST_CHECK(o0.dataValidity() == ctk::DataValidity::faulty);
}

/**
 * \anchor testDataValidity_2_1_1 \ref dataValidity_2_1_1 "2.1.1"
 * Each input and each output of a module (or fan out) is decorated with a MetaDataPropagatingRegisterDecorator
 *  (except for the TriggerFanOut, see. \ref dataValidity_2_4 "2.4")
 */
BOOST_AUTO_TEST_CASE(testDataValidity_2_1_1) {
  TestApplication1<TestModule1> app;
  ctk::TestFacility test;
  auto i1 = test.getScalar<int>("/m1/i1");
  test.runApplication();
  assert(app.mod.getDataValidity() == ctk::DataValidity::ok);
  // we cannot check inputs via dynamic_cast to MetaDataPropagatingRegisterDecorator, since implementation detail is
  // hidden by TransferElementAbstractor instead, check what the decorator is supposed to do. check that the
  // MetaDataPropagatingRegisterDecorator counts data validity changes ( in doPostRead)
  i1 = 0;
  i1.write();
  test.stepApplication(); // triggers m1.i1.read()
  i1.setDataValidity(ctk::DataValidity::faulty);
  i1.write();
  test.stepApplication(); // triggers m1.i1.read()
  BOOST_CHECK(app.mod.incCalled == 1);
  BOOST_CHECK(app.mod.decCalled == 0);

  // check that the MetaDataPropagatingRegisterDecorator takes over faulty data validity from owning module ( in doPreWrite)
  BOOST_CHECK(app.mod.getDataValidity() == ctk::DataValidity::faulty);
}

/*
 * \anchor testDataValidity_2_1_2 \ref dataValidity_2_1_2 "2.1.2"
 * The decorator knows about the module it is connected to. It is called the 'owner'.
 *
 * There is no public function for getting the owner, but implicitly this was
 * tested in \ref testDataValidity_2_1_1.
 */

/**
 * \anchor testDataValidity_2_1_3 \ref dataValidity_2_1_3 "2.1.3"
 *  **read:** For each read operation it checks the incoming data validity and increases/decreases the data fault
 *  counter of the owner.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_2_1_3) {
  TestApplication1<TestModule1> app;
  ctk::TestFacility test;

  auto i1 = test.getScalar<int>("/m1/i1");

  test.runApplication();
  // mark input faulty
  i1 = 1;
  i1.setDataValidity(ctk::DataValidity::faulty);
  i1.write();
  // propagate value
  test.stepApplication();
  // check that fault counter was incremented
  BOOST_CHECK(app.mod.incCalled == 1);
  // mark input ok
  i1.setDataValidity(ctk::DataValidity::ok);
  i1.write();
  // propagate value
  test.stepApplication();
  // check that fault counter was decremented
  BOOST_CHECK(app.mod.decCalled == 1);
}

/*
 * \anchor testDataValidity_2_1_5 \ref dataValidity_2_1_5 "2.1.5"
 * **write:** When writing, the decorator is checking the validity of the owner and the individual flag of the output
 * set by the user. Only if both are 'ok' the output validity is 'ok', otherwise the outgoing data is send as 'faulty'.
 *
 * Test ist identical to \ref testDataValidity_1_6
 */

/**
 * \anchor testDataValidity_2_3_1 \ref dataValidity_2_3_1  "2.3.1"
 * Each ApplicationModule has one data fault counter variable which is increased/decreased by
 * EntityOwner::incrementDataFaultCounter() and  EntityOwner::decrementDataFaultCounter.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_2_3_1) {
  TestModule1 testmod1;
  BOOST_CHECK(testmod1.getDataValidity() == ctk::DataValidity::ok);
  testmod1.incrementDataFaultCounter();
  BOOST_CHECK(testmod1.getDataValidity() == ctk::DataValidity::faulty);
  testmod1.decrementDataFaultCounter();
  BOOST_CHECK(testmod1.getDataValidity() == ctk::DataValidity::ok);
}

/*
 * \anchor testDataValidity_2_3_2 \ref dataValidity_2_3_2 "2.3.2"
 * All inputs and outputs have a MetaDataPropagatingRegisterDecorator.
 *
 * Tested in \ref testDataValidity_2_1_1
 */

/**
 * \anchor testDataValidity_2_3_3 \ref dataValidity_2_3_3 "2.3.3"
 * The main loop of the module usually does not care about data validity. If any input is invalid, all outputs are
 * automatically invalid. The loop just runs through normally, even if an input has invalid data.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_2_3_3) {
  TestApplication1<TestModule1> app;
  ctk::TestFacility test;

  auto i1 = test.getScalar<int>("/m1/i1");
  auto o1 = test.getScalar<int>("/m1/o1");
  auto oconst = test.getScalar<int>("/m1/oconst");
  test.runApplication();

  i1 = 1;
  i1.setDataValidity(ctk::DataValidity::faulty);
  i1.write();

  test.stepApplication();

  // check that an output which is re-calculated becomes invalid
  // we need to get the destination of o1.write(), which is unlike o1.dataValidity() inside the module main loop
  o1.read();
  BOOST_CHECK(o1.dataValidity() == ctk::DataValidity::faulty);
  // check that an output which is not re-calculated stays valid
  BOOST_CHECK(oconst.readLatest() == false);
}

/**
 * \anchor testDataValidity_2_3_4 \ref dataValidity_2_3_4 "2.3.4"
 * Inside the ApplicationModule main loop the module's data fault counter is accessible. The user can increment and
 * decrement it, but has to be careful to do this in pairs. The more common use case will be to query the module's
 * data validity.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_2_3_4) {
  TestApplication1<TestModule2> app;
  ctk::TestFacility test;

  auto i1 = test.getScalar<int>("/m1/i1");
  test.runApplication();
  i1.write();
  test.stepApplication();
  // check that module variable v2 reports faulty
  BOOST_CHECK(app.mod.dataValidity2 == false);

  i1 = 1;
  i1.setDataValidity(ctk::DataValidity::faulty);
  i1.write();
  test.stepApplication();
  // check that module variable v1 now also reports faulty
  BOOST_CHECK(app.mod.dataValidity1 == false);
}

/**
 * \anchor testDataValidity_2_4_1 \ref dataValidity_2_4_1 "2.4.1"
 * Only the push-type trigger input of the TriggerFanOut is equiped with a MetaDataPropagatingRegisterDecorator.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_2_4_1) {
  TestApplication1<TestModule1> app;
  ctk::TestFacility test;
  // app.debugTestableMode();

  // the TriggerFanOut is realized via device module connected to control system,
  // using a trigger from TriggerModule
  auto result1 = test.getScalar<int>("dev/i3"); // RO

  test.runApplication();

  // check that setting trigger to invalid propagates to outputs of TriggerFanOut
  app.m2.o1.setDataValidity(ctk::DataValidity::faulty);
  app.m2.o1.write();

  test.stepApplication();
  result1.read();
  BOOST_CHECK(result1.dataValidity() == ctk::DataValidity::faulty);
}

/*
 * \anchor testDataValidity_2_4_2 \ref dataValidity_2_4_2 "2.4.2"
 * The poll-type data inputs do not have a MetaDataPropagatingRegisterDecorator.
 *
 * No functionality that needs testing
 */

/**
 * \anchor testDataValidity_2_4_3 \ref dataValidity_2_4_3 "2.4.3"
 * The individual poll-type inputs propagate the data validity flag only to the corresponding outputs.
 */
BOOST_AUTO_TEST_CASE(testDataValidity_2_4_3) {
  TestApplication1<TestModule1> app;
  ctk::TestFacility test;

  // the TriggerFanOut is realized via device module connected to control system,
  // using a trigger from TriggerModule
  auto result1 = test.getScalar<int>("/dev/i1");
  auto result2 = test.getScalar<int>("/dev/i3");

  test.runApplication();
  auto exceptionDummy = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(app.dev.getDeviceModule().device.getBackend());
  assert(exceptionDummy);
  exceptionDummy->setValidity("/dev/i1", ctk::DataValidity::faulty);

  app.m2.o1.write();
  test.stepApplication();
  result1.read();
  BOOST_CHECK(result1.dataValidity() == ctk::DataValidity::faulty);
  result2.read();
  BOOST_CHECK(result2.dataValidity() == ctk::DataValidity::ok);
}

/*
 * \anchor testDataValidity_2_4_4 \ref dataValidity_2_4_4 "2.4.4"
 * Although the trigger conceptually has data type 'void', it can also be `faulty`. An invalid trigger is processed,
 * but all read out data is flagged as `faulty`.
 *
 * Already tested in \ref testDataValidity_2_4_1
 */

/*
 * \anchor testDataValidity_2_5_1 \ref dataValidity_2_5_1 "2.5.1"
 * The MetaDataPropagatingRegisterDecorator is always placed *around* the ExceptionHandlingDecorator if both
 * decorators are used on a process variable. Like this a `faulty` flag raised by the ExceptionHandlingDecorator is
 * automatically picked up by the MetaDataPropagatingRegisterDecorator.
 *
 * Already tested in  \ref testDataValidity_1_3
 */
/*
 * \anchor testDataValidity_2_5_2 \ref dataValidity_2_5_2 "2.5.2"
 * The first failing read returns with the old data and the 'faulty' flag. Like this the flag is propagated to the
 * outputs. Only further reads might freeze until the device is available again.
 *
 *  Already tested in  \ref testDataValidity_1_3
 */

/*
 * \anchor testDataValidity_2_6_1 \ref dataValidity_2_6_1 "2.6.1"
 * For device variables, the requirement of setting receiving endpoints to 'faulty' on construction can not be
 * fulfilled. In DeviceAccess the accessors are bidirectional and provide no possibility to distinguish sender and
 * receiver. Instead, the validity is set right after construction in Application::createDeviceVariable() for receivers.
 *
 * Already tested in  \ref testDataValidity_1_3
 */

/*
 * \anchor testDataValidiy_3_1 \ref dataValidity_3_1 "3.1"
 * The decorators which manipulate the data fault counter are responsible for counting up and down in pairs, such that
 * the counter goes back to 0 if all data is ok, and never becomes negative.
 *
 * Not tested since it's an implementation detail.
 */

// BOOST_AUTO_TEST_SUITE_END()
