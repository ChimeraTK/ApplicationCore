// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testFanoutConnections

#include "Application.h"
#include "ApplicationModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/ExceptionDummyBackend.h>

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

namespace Tests::testFanoutConnections {

  namespace ctk = ChimeraTK;

  struct TestModule1 : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;
    ctk::ScalarPushInput<int> moduleTrigger{this, "moduleTrigger", "", ""};

    ctk::ScalarPollInput<int> i3{this, "i3", "", ""};

    ctk::ScalarOutput<int> moduleOutput{this, "moduleOutput", "", ""};

    void mainLoop() override {
      while(true) {
        moduleTrigger.read();

        i3.readLatest();

        moduleOutput = int(i3);

        writeAll();
      }
    }
  };

  // FIXME: This test is probably already covered by one of the other test cases
  // What it previously tested, a different connection order of device and application to CS, is
  // no longer possible.
  //
  // the connection code has to create a consuming fan out because m1.i3 is a poll type consumer,
  // and a trigger fan out because m1.i1 only has one push type consumer in the CS
  struct TestApplication1 : ctk::Application {
    TestApplication1() : Application("testApp") {}
    ~TestApplication1() override { shutdown(); }

    constexpr static char const* dummyCDD1 = "(dummy?map=testDataValidity1.map)";

    TestModule1 m1{this, "m1", ""};
    ctk::DeviceModule device{this, dummyCDD1, "/deviceTrigger"};
  };

  BOOST_AUTO_TEST_CASE(testConnectConsumingFanout) {
    TestApplication1 theApp;
    ctk::TestFacility testFacility{theApp};
    ChimeraTK::Device dummy(TestApplication1::dummyCDD1);

    // write initial values to the dummy before starting the application
    dummy.open();
    dummy.write("m1/i1/DUMMY_WRITEABLE", 12);
    dummy.write("m1/i3/DUMMY_WRITEABLE", 32);

    testFacility.runApplication();

    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/i1"), 12);
    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/i3"), 32);

    // check that the trigger only affects i1
    dummy.write("m1/i1/DUMMY_WRITEABLE", 13);
    dummy.write("m1/i3/DUMMY_WRITEABLE", 33);

    testFacility.getVoid("deviceTrigger").write();
    testFacility.stepApplication();

    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/i1"), 13);
    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/i3"), 32);

    // check that the module trigger updates i3
    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/moduleOutput"), 0);

    dummy.write("m1/i1/DUMMY_WRITEABLE", 14);
    dummy.write("m1/i3/DUMMY_WRITEABLE", 34);

    testFacility.writeScalar<int>("m1/moduleTrigger", 1);
    testFacility.stepApplication();

    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/i1"), 13);
    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/i3"), 34);
    BOOST_CHECK_EQUAL(testFacility.readScalar<int>("m1/moduleOutput"), 34);
  }

} // namespace Tests::testFanoutConnections