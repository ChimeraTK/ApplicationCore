// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testPeriodicTrigger

#include "Application.h"
#include "PeriodicTrigger.h"
#include "TestFacility.h"

#include <boost/test/included/unit_test.hpp>

namespace Tests::testPeriodicTrigger {

  using namespace boost::unit_test_framework;
  using namespace ChimeraTK;

  // Just consume variables because the testfacility is too stupid to step without anything being written.
  struct TestModule : public ApplicationModule {
    using ApplicationModule::ApplicationModule;

    ScalarPushInput<int> in{this, "in", "", ""};
    void mainLoop() override { in.read(); }
  };

  struct TestApplication : Application {
    TestApplication() : Application("myTestApp") {}
    ~TestApplication() override { shutdown(); }

    PeriodicTrigger p{this, "SomeTimer", "", 1000, {}, "/Config/timerPeriod", "../tickTock"};
    TestModule m{this, "SomeModule", ""};
  };

  // This test is checking that the I/O variables are created as intended,
  // and that the functionality in testable mode is working. It does not
  // the real timing (and thus the only and main functionality of the PeriodicTrigger).
  BOOST_AUTO_TEST_CASE(testIterface) {
    BOOST_CHECK(true);
    TestApplication app;
    TestFacility test{app};
    test.runApplication();

    auto tick = test.getScalar<uint64_t>("/tickTock");
    tick.readLatest();
    BOOST_CHECK(tick.getVersionNumber() != VersionNumber{nullptr});
    BOOST_CHECK_EQUAL(static_cast<uint64_t>(tick), 0);

    // We can only check that the period variable exists and is writeable.
    // There is no effect in testable mode. Actually, we cannot even write to it
    // because it not read any more, and the test would faild with an unread queue.
    BOOST_CHECK_NO_THROW((void)test.getScalar<uint32_t>("/Config/timerPeriod"));

    auto oldVersion = tick.getVersionNumber();
    // The test facilty does not recognise that the PeriodicTrigger send something. It expects some input from the CS.
    app.p.sendTrigger();
    test.writeScalar<int>("/SomeModule/in", 42);

    test.stepApplication();
    tick.read();

    BOOST_CHECK(tick.getVersionNumber() > oldVersion);
    BOOST_CHECK_EQUAL(static_cast<uint64_t>(tick), 1);
  }

} // namespace Tests::testPeriodicTrigger
