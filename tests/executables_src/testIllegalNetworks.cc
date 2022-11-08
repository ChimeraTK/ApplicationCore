// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestFacility.h"

#include <future>

#define BOOST_TEST_MODULE testIllegalNetworks

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/BackendFactory.h>

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

// list of user types the accessors are tested with
using TestTypes = boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double, ctk::Boolean>;

/*********************************************************************************************************************/
/* test case for two scalar accessors, feeder in poll mode and consumer in push
 * mode (without trigger) */

struct TestApplication1 : public ctk::Application {
  TestApplication1() : Application("testSuite") {}
  ~TestApplication1() override { shutdown(); }

  ctk::SetDMapFilePath dmap{"test.dmap"};

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ScalarPushInput<int> consumingPush{this, "/MyModule/readBack", "", ""};

    void mainLoop() override {}
  } testModule{(this), ".", ""}; // extra parentheses are for doxygen...

  ctk::DeviceModule dev{this, "Dummy0"};
};

BOOST_AUTO_TEST_CASE(testTwoScalarPollPushAccessors) {
  TestApplication1 app;
  app.debugMakeConnections();

  BOOST_CHECK_THROW(
      {
        app.initialise();
        app.run();
      },
      ctk::logic_error);
}

/*********************************************************************************************************************/
/* test case for two feeders */

template<typename T>
struct TestApplication3 : public ctk::Application {
  TestApplication3() : Application("testSuite") { debugMakeConnections(); }
  ~TestApplication3() override { shutdown(); }

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ScalarOutput<T> consumingPush{this, "/MyModule/readBack", "", ""};

    void mainLoop() override {}
  } testModule{this, ".", ""};

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ScalarOutput<T> consumingPush2{this, "/MyModule/readBack", "", ""};

    void mainLoop() override {}
  } testModule2{this, ".", ""};
};

BOOST_AUTO_TEST_CASE_TEMPLATE(testTwoFeeders, T, TestTypes) {
  TestApplication3<T> app;

  BOOST_CHECK_THROW(ctk::TestFacility tf(app, false), ctk::logic_error);
}

/*********************************************************************************************************************/
/* test case for too many polling consumers */

struct TestApplication4 : public ctk::Application {
  TestApplication4() : Application("testSuite") { debugMakeConnections(); }
  ~TestApplication4() override { shutdown(); }

  ctk::SetDMapFilePath dmap{"test.dmap"};

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ScalarPollInput<int> consumingPush{this, "/MyModule/readBack", "", ""};

    void mainLoop() override {}
  } testModule{this, ".", ""};

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ScalarPollInput<int> consumingPush2{this, "/MyModule/readBack", "", ""};

    void mainLoop() override {}
  } testModule2{this, ".", ""};

  ctk::DeviceModule dev{this, "Dummy0"};
};

BOOST_AUTO_TEST_CASE(testTooManyPollingConsumers) {
  TestApplication4 app;

  BOOST_CHECK_THROW(
      {
        app.initialise();
        app.run();
      },
      ctk::logic_error);
}

/*********************************************************************************************************************/
/* test case for different number of elements */

template<typename T>
struct TestApplication5 : public ctk::Application {
  TestApplication5() : Application("testSuite") { debugMakeConnections(); }
  ~TestApplication5() override { shutdown(); }

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ArrayOutput<T> feed{this, "/MyModule/readBack", "", 10, ""};

    void mainLoop() override {}
  } testModule{this, ".", ""};

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ArrayPollInput<T> consume{this, "/MyModule/readBack", "", 20, ""};

    void mainLoop() override {}
  } testModule2{this, ".", ""};
};

BOOST_AUTO_TEST_CASE_TEMPLATE(testDifferentNrElements, T, TestTypes) {
  TestApplication5<T> app;
  BOOST_CHECK_THROW(ctk::TestFacility tf(app, false), ctk::logic_error);
}

/*********************************************************************************************************************/
/* test case for zero-length elements that are not void */

template<typename T>
struct TestApplication6 : public ctk::Application {
  TestApplication6() : Application("testSuite") { debugMakeConnections(); }
  ~TestApplication6() override { shutdown(); }

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ArrayOutput<T> feed{this, "/MyModule/readBack", "", 0, ""};

    void mainLoop() override {}
  } testModule{this, ".", ""};

  struct : ctk::ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ctk::ArrayPollInput<T> consume{this, "/MyModule/readBack", "", 0, ""};

    void mainLoop() override {}
  } testModule2{this, ".", ""};
};
