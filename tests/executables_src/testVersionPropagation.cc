// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testVersionpropagation

#include "ApplicationModule.h"
#include "check_timeout.h"
#include "fixtures.h"

#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/VersionNumber.h>

#include <boost/test/included/unit_test.hpp>

#include <future>

namespace Tests::testVersionpropagation {

  namespace ctk = ChimeraTK;
  using Fixture = FixtureWithPollAndPushInput<false>;

  BOOST_FIXTURE_TEST_SUITE(VersionPropagationPart1, Fixture)

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testPolledRead) {
    std::cout << "versionPropagation_testPolledRead" << std::endl;
    auto moduleVersion = application.group1.pollModule.getCurrentVersionNumber();
    [[maybe_unused]] auto pollVariableVersion = pollVariable2.getVersionNumber();

    application.group1.outputModule.setCurrentVersionNumber({});
    outputVariable2.write();
    pollVariable2.read();

    assert(pollVariable2.getVersionNumber() > pollVariableVersion);
    BOOST_CHECK(moduleVersion == application.group1.pollModule.getCurrentVersionNumber());
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testPolledReadNonBlocking) {
    std::cout << "versionPropagation_testPolledReadNonBlocking" << std::endl;
    auto moduleVersion = application.group1.pollModule.getCurrentVersionNumber();
    [[maybe_unused]] auto pollVariableVersion = pollVariable2.getVersionNumber();

    application.group1.outputModule.setCurrentVersionNumber({});
    outputVariable2.write();
    pollVariable2.readNonBlocking();

    assert(pollVariable2.getVersionNumber() > pollVariableVersion);
    BOOST_CHECK(moduleVersion == application.group1.pollModule.getCurrentVersionNumber());
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testPolledReadLatest) {
    std::cout << "versionPropagation_testPolledReadLatest" << std::endl;
    auto moduleVersion = application.group1.pollModule.getCurrentVersionNumber();
    [[maybe_unused]] auto pollVariableVersion = pollVariable2.getVersionNumber();

    application.group1.outputModule.setCurrentVersionNumber({});
    outputVariable2.write();
    pollVariable2.readLatest();

    assert(pollVariable2.getVersionNumber() > pollVariableVersion);
    BOOST_CHECK(moduleVersion == application.group1.pollModule.getCurrentVersionNumber());
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testPushTypeRead) {
    std::cout << "versionPropagation_testPushTypeRead" << std::endl;
    // Make sure we pop out any stray values in the pushInput before test start:
    CHECK_TIMEOUT(pushVariable.readLatest() == false, 10000);

    [[maybe_unused]] ctk::VersionNumber nextVersionNumber = {};
    interrupt.write();
    pushVariable.read();
    assert(pushVariable.getVersionNumber() > nextVersionNumber);
    BOOST_CHECK(application.group1.pushModule.getCurrentVersionNumber() == pushVariable.getVersionNumber());
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testPushTypeReadNonBlocking) {
    std::cout << "versionPropagation_testPushTypeReadNonBlocking" << std::endl;
    CHECK_TIMEOUT(pushVariable.readLatest() == false, 10000);

    auto pushInputVersionNumber = pushVariable.getVersionNumber();

    // no version change on readNonBlocking false
    BOOST_CHECK_EQUAL(pushVariable.readNonBlocking(), false);
    BOOST_CHECK(pushInputVersionNumber == pushVariable.getVersionNumber());

    ctk::VersionNumber nextVersionNumber = {};
    auto moduleVersion = application.group1.pushModule.getCurrentVersionNumber();

    interrupt.write();
    CHECK_TIMEOUT(pushVariable.readNonBlocking() == true, 10000);
    BOOST_CHECK(pushVariable.getVersionNumber() > nextVersionNumber);

    // readNonBlocking will not propagete the version to the module
    BOOST_CHECK(application.group1.pushModule.getCurrentVersionNumber() == moduleVersion);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testPushTypeReadLatest) {
    std::cout << "versionPropagation_testPushTypeReadLatest" << std::endl;
    // Make sure we pop out any stray values in the pushInput before test start:
    CHECK_TIMEOUT(pushVariable.readLatest() == false, 10000);

    auto pushInputVersionNumber = pushVariable.getVersionNumber();

    // no version change on readNonBlocking false
    BOOST_CHECK_EQUAL(pushVariable.readLatest(), false);
    BOOST_CHECK(pushInputVersionNumber == pushVariable.getVersionNumber());

    ctk::VersionNumber nextVersionNumber = {};
    auto moduleVersion = application.group1.pushModule.getCurrentVersionNumber();

    interrupt.write();
    CHECK_TIMEOUT(pushVariable.readLatest() == true, 10000);
    BOOST_CHECK(pushVariable.getVersionNumber() > nextVersionNumber);

    // readLatest will not propagete the version to the module
    BOOST_CHECK(application.group1.pushModule.getCurrentVersionNumber() == moduleVersion);
  }

  BOOST_AUTO_TEST_SUITE_END()

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  // The EmptyFixture is required, since we cannot use an ordinary BOOST_AUTO_TEST_SUITE after a
  // BOOST_FIXTURE_TEST_SUITE when compiling with -std=c++20 or later.
  class EmptyFixture {};
  BOOST_FIXTURE_TEST_SUITE(VersionPropagationPart2, EmptyFixture)

  struct ThePushModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;

    ChimeraTK::ScalarPushInput<int> pushInput{this, "/theVariable", "", ""};

    std::promise<void> p;
    void mainLoop() override { p.set_value(); }
  };

  struct TheOutputModule : ChimeraTK::ApplicationModule {
    using ChimeraTK::ApplicationModule::ApplicationModule;

    ChimeraTK::ScalarOutput<int> output{this, "/theVariable", "", ""};

    void prepare() override { output.write(); }

    std::promise<void> p;
    void mainLoop() override { p.set_value(); }
  };

  struct TheTestApplication : ChimeraTK::Application {
    using ChimeraTK::Application::Application;
    explicit TheTestApplication(const std::string& name, const std::unordered_set<std::string>& pmTags = {})
    : ChimeraTK::Application(name), pm{this, "pm", "", pmTags} {}

    ~TheTestApplication() override { shutdown(); }

    ThePushModule pm;
    TheOutputModule om{this, "om", ""};
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testSetAndWrite) {
    std::cout << "versionPropagation_testSetAndWrite" << std::endl;
    TheTestApplication app("app");
    ChimeraTK::TestFacility test(app, false);
    test.runApplication();
    app.pm.p.get_future().wait();
    app.om.p.get_future().wait();

    ChimeraTK::VersionNumber theVersion;
    app.om.setCurrentVersionNumber(theVersion);
    app.om.output.setAndWrite(42);

    app.pm.pushInput.read();

    BOOST_CHECK(app.pm.getCurrentVersionNumber() == theVersion);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testWriteIfDifferent) {
    std::cout << "versionPropagation_testWriteIfDifferent" << std::endl;
    TheTestApplication app("app");
    ChimeraTK::TestFacility test(app, false);
    test.runApplication();
    app.pm.p.get_future().wait();
    app.om.p.get_future().wait();

    ChimeraTK::VersionNumber theVersion;
    app.om.setCurrentVersionNumber(theVersion);
    app.om.output.writeIfDifferent(42);

    app.pm.pushInput.read();

    BOOST_CHECK(app.pm.getCurrentVersionNumber() == theVersion);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(versionPropagation_testDisabledVersionProp) {
    std::cout << "versionPropagation_testDisabledVersionProp" << std::endl;
    TheTestApplication app("app", {ChimeraTK::independentVersionTag});
    ChimeraTK::TestFacility test(app, false);
    test.runApplication();
    app.pm.p.get_future().wait();
    app.om.p.get_future().wait();

    // test that special tag disables propagation of VersionNumber to application module
    ctk::VersionNumber vnInputBeforeWrite = app.pm.pushInput.getVersionNumber();
    ctk::VersionNumber vnModuleBeforeWrite = app.pm.getCurrentVersionNumber();
    app.om.setCurrentVersionNumber({});
    app.om.output.write();
    app.pm.pushInput.read();
    BOOST_CHECK(app.pm.pushInput.getVersionNumber() > vnInputBeforeWrite);
    BOOST_CHECK(app.pm.getCurrentVersionNumber() == vnModuleBeforeWrite);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_SUITE_END()

  /********************************************************************************************************************/

} // namespace Tests::testVersionpropagation
