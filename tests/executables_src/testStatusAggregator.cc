// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Application.h"
#include "ModuleGroup.h"
#include "StatusAggregator.h"
#include "StatusWithMessage.h"
#include "TestFacility.h"

#define BOOST_NO_EXCEPTIONS
#define BOOST_TEST_MODULE testStatusAggregator
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

namespace Tests::testStatusAggregator {

  using namespace boost::unit_test_framework;

  namespace ctk = ChimeraTK;

  /********************************************************************************************************************/

  struct StatusGenerator : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    StatusGenerator(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {},
        ctk::StatusOutput::Status initialStatus = ctk::StatusOutput::Status::OFF)
    : ApplicationModule(owner, name, description, tags), initialValue(initialStatus) {}

    ctk::StatusOutput status{this, getName(), ""};

    ctk::StatusOutput::Status initialValue;

    void prepare() override {
      status = initialValue;
      status.write();
    }
    void mainLoop() override {}
  };

  /********************************************************************************************************************/

  struct StatusWithMessageGenerator : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    StatusWithMessageGenerator(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {},
        ctk::StatusOutput::Status initialStatus = ctk::StatusOutput::Status::OFF)
    : ApplicationModule(owner, name, description, tags), initialValue(initialStatus) {}

    ctk::StatusWithMessage status{this, getName(), ""};

    ctk::StatusOutput::Status initialValue;

    void prepare() override {
      if(initialValue == ctk::StatusOutput::Status::OK) {
        status.writeOk();
      }
      else {
        status.write(initialValue, getDescription());
      }
    }
    void mainLoop() override {}
  };

  /********************************************************************************************************************/

  struct TestApplication : ctk::Application {
    TestApplication() : Application("testApp") {}
    ~TestApplication() override { shutdown(); }

    StatusGenerator s{this, "s", "Status"};

    struct OuterGroup : ctk::ModuleGroup {
      using ctk::ModuleGroup::ModuleGroup;

      StatusGenerator s1{this, "s1", "Status 1"};
      StatusGenerator s2{this, "s2", "Status 2"};

      struct InnerGroup : ctk::ModuleGroup {
        using ctk::ModuleGroup::ModuleGroup;
        StatusGenerator s{this, "s", "Status"};
        StatusGenerator deep{this, "deep", "Status"};
      };
      InnerGroup innerGroup1{this, "InnerGroup1", ""};
      InnerGroup innerGroup2{this, "InnerGroup2", ""};

    } outerGroup{this, "OuterGroup", ""};

    ctk::StatusAggregator aggregator{
        this, "Aggregated/status", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko};
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testSingleNoTags) {
    std::cout << "testSingleNoTags" << std::endl;

    TestApplication app;
    ctk::TestFacility test(app);

    auto status = test.getScalar<int>("/Aggregated/status");

    test.runApplication();

    // check that statuses on different levels are correctly aggregated
    auto check = [&](auto& var) {
      var = ctk::StatusOutput::Status::OK;
      var.write();
      test.stepApplication();
      BOOST_CHECK(status.readNonBlocking() == true);
      BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
      var = ctk::StatusOutput::Status::OFF;
      var.write();
      test.stepApplication();
      BOOST_CHECK(status.readNonBlocking() == true);
      BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));
    };
    check(app.s.status);
    check(app.outerGroup.s1.status);
    check(app.outerGroup.s2.status);
    check(app.outerGroup.innerGroup1.s.status);
    check(app.outerGroup.innerGroup1.deep.status);
    check(app.outerGroup.innerGroup2.s.status);
    check(app.outerGroup.innerGroup2.deep.status);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDataValidity) {
    std::cout << "testDataValidity" << std::endl;

    TestApplication app;
    ctk::TestFacility test(app);

    auto status = test.getScalar<int>("/Aggregated/status");

    test.runApplication();

    app.s.incrementDataFaultCounter();
    app.s.status = ctk::StatusOutput::Status::OK;
    app.s.status.write();
    test.stepApplication();
    BOOST_CHECK(status.readNonBlocking() == true);
    BOOST_CHECK(status.dataValidity() == ctk::DataValidity::ok);
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
    app.s.status = ctk::StatusOutput::Status::OFF;
    app.s.status.write();
    test.stepApplication();
    BOOST_CHECK(status.readNonBlocking() == true);
    BOOST_CHECK(status.dataValidity() == ctk::DataValidity::ok);
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));
  }

  /********************************************************************************************************************/

  struct TestPrioApplication : ctk::Application {
    explicit TestPrioApplication(ctk::StatusOutput::Status theInitialValue)
    : Application("testApp"), initialValue(theInitialValue) {}
    ~TestPrioApplication() override { shutdown(); }

    ctk::StatusOutput::Status initialValue;

    StatusGenerator s1{this, "sg1/internal", "Status 1", {}, initialValue};
    StatusGenerator s2{this, "sg2/external", "Status 2", {}, initialValue};

    ctk::StatusAggregator aggregator;
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testPriorities) {
    std::cout << "testPriorities" << std::endl;

    // Define repeated check for a given priority mode
    auto check = [&](ctk::StatusAggregator::PriorityMode mode, auto prio0, auto prio1, auto prio2, auto prio3,
                     bool warnMixed01 = false) {
      // create app with initial values set to lowest prio value
      TestPrioApplication app(prio0);

      app.aggregator = ctk::StatusAggregator{&app, "Aggregated/status", "aggregated status description", mode};

      ctk::TestFacility test(app);

      auto status = test.getScalar<int>("/Aggregated/status");

      test.runApplication();

      // check initial value
      status.readNonBlocking(); // do not check return value, as it will only be written when changed
      BOOST_CHECK_EQUAL(int(status), int(prio0));

      // Define repeated check to test all combinations of two given values with different priority.
      // This kind of a whitebox test. We know that
      // - the first aggregates variable has a different code path
      // - the code path depends on the VersionNumber, so the write order matters and we have to set the version number
      auto subcheck = [&](auto lower, auto higher, bool writeS2First, bool warnMixed = false) {
        StatusGenerator* first;
        StatusGenerator* second;
        if(writeS2First) {
          first = &app.s2;
          second = &app.s1;
        }
        else {
          first = &app.s1;
          second = &app.s2;
        }
        std::cout << int(lower) << " vs. " << int(higher) << std::endl;
        first->status = lower;
        first->setCurrentVersionNumber({});
        first->status.write();
        second->status = lower;
        second->setCurrentVersionNumber({});
        second->status.write();
        test.stepApplication();
        status.readLatest();
        BOOST_CHECK_EQUAL(int(status), int(lower));
        first->status = lower;
        first->setCurrentVersionNumber({});
        first->status.write();
        second->status = higher;
        second->setCurrentVersionNumber({});
        second->status.write();
        test.stepApplication();
        status.readLatest();
        if(!warnMixed) {
          BOOST_CHECK_EQUAL(int(status), int(higher));
        }
        else {
          BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::WARNING));
        }
        first->status = higher;
        first->setCurrentVersionNumber({});
        first->status.write();
        second->status = lower;
        second->setCurrentVersionNumber({});
        second->status.write();
        test.stepApplication();
        status.readLatest();
        if(!warnMixed) {
          BOOST_CHECK_EQUAL(int(status), int(higher));
        }
        else {
          BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::WARNING));
        }
        first->status = higher;
        first->setCurrentVersionNumber({});
        first->status.write();
        second->status = higher;
        second->setCurrentVersionNumber({});
        second->status.write();
        test.stepApplication();
        status.readLatest();
        BOOST_CHECK_EQUAL(int(status), int(higher));
      };

      // all prios against each other
      auto runAllSubchecks = [&](bool writeS2First) {
        subcheck(prio0, prio1, writeS2First, warnMixed01);
        subcheck(prio0, prio2, writeS2First);
        subcheck(prio0, prio3, writeS2First);
        subcheck(prio1, prio2, writeS2First);
        subcheck(prio1, prio3, writeS2First);
        subcheck(prio2, prio3, writeS2First);
      };
      runAllSubchecks(false);
      runAllSubchecks(true);
    };

    // check all priority modes
    std::cout << "PriorityMode::fwko" << std::endl;
    check(ctk::StatusAggregator::PriorityMode::fwko, ctk::StatusOutput::Status::OFF, ctk::StatusOutput::Status::OK,
        ctk::StatusOutput::Status::WARNING, ctk::StatusOutput::Status::FAULT);
    std::cout << "PriorityMode::fwok" << std::endl;
    check(ctk::StatusAggregator::PriorityMode::fwok, ctk::StatusOutput::Status::OK, ctk::StatusOutput::Status::OFF,
        ctk::StatusOutput::Status::WARNING, ctk::StatusOutput::Status::FAULT);
    std::cout << "PriorityMode::ofwk" << std::endl;
    check(ctk::StatusAggregator::PriorityMode::ofwk, ctk::StatusOutput::Status::OK, ctk::StatusOutput::Status::WARNING,
        ctk::StatusOutput::Status::FAULT, ctk::StatusOutput::Status::OFF);
    std::cout << "PriorityMode::fw_warn_mixed" << std::endl;
    check(ctk::StatusAggregator::PriorityMode::fw_warn_mixed, ctk::StatusOutput::Status::OFF,
        ctk::StatusOutput::Status::OK, ctk::StatusOutput::Status::WARNING, ctk::StatusOutput::Status::FAULT, true);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testCustomMixedWarnMessage) {
    std::cout << "testCustomMixedWarnMessage" << std::endl;

    const std::string customMessage1{"My custom warn mixed message"};

    TestPrioApplication app(ctk::StatusOutput::Status::OK);
    app.aggregator = ctk::StatusAggregator{&app, "Aggregated/status", "aggregated status description",
        ctk::StatusAggregator::PriorityMode::fw_warn_mixed, {}, {}, customMessage1};

    ctk::TestFacility test(app);
    test.runApplication();

    auto statusMessage = test.getScalar<std::string>("/Aggregated/status_message");

    // check test pre-condition: No message when OK.
    BOOST_TEST(std::string(statusMessage) == "");

    app.s1.setCurrentVersionNumber({});
    app.s1.status.setAndWrite(int(ctk::StatusOutput::Status::OFF));
    test.stepApplication();
    BOOST_TEST(statusMessage.readAndGet() == customMessage1);

    // Change custom message at run time
    const std::string customMessage2{"Another warn mixed message"};
    app.aggregator.setWarnMixedMessage(customMessage2);

    app.s1.setCurrentVersionNumber({});
    app.s1.status.setAndWrite(int(ctk::StatusOutput::Status::OFF));
    test.stepApplication();
    BOOST_TEST(statusMessage.readAndGet() == customMessage2);
  }

  /********************************************************************************************************************/

  struct TestApplication2Levels : ctk::Application {
    TestApplication2Levels() : Application("testApp") {}
    ~TestApplication2Levels() override { shutdown(); }

    StatusGenerator s{this, "s", "Status"};

    struct OuterGroup : ctk::ModuleGroup {
      using ctk::ModuleGroup::ModuleGroup;

      // Set one of the inputs for the extraAggregator to fault, which has no effect, since one other is OFF which is
      // prioritised. If the top-level aggregator would wrongly aggregate this input directly, it would go to FAULT.
      StatusGenerator s1{this, "s1", "Status 1", {}, ctk::StatusOutput::Status::FAULT};

      StatusGenerator s2{this, "s2", "Status 2"};

      ctk::StatusAggregator extraAggregator{
          this, "/Aggregated/extraStatus", "aggregated status description", ctk::StatusAggregator::PriorityMode::ofwk};

    } outerGroup{this, "OuterGroup", ""};

    ctk::StatusAggregator aggregator{
        this, "Aggregated/status", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko};
  };

  ///**********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testTwoLevels) {
    std::cout << "testTwoLevels" << std::endl << std::endl << std::endl;
    TestApplication2Levels app;

    ctk::TestFacility test(app);

    auto status = test.getScalar<int>("/Aggregated/status");
    auto extraStatus = test.getScalar<int>("/Aggregated/extraStatus");

    test.runApplication();

    // check the initial values
    extraStatus.readLatest();
    BOOST_CHECK_EQUAL(int(extraStatus), int(ctk::StatusOutput::Status::OFF));
    status.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));

    // change status which goes directly into the upper aggregator
    app.s.status = ctk::StatusOutput::Status::OK;
    app.s.status.write();
    test.stepApplication();
    status.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
    app.s.status = ctk::StatusOutput::Status::OFF;
    app.s.status.write();
    test.stepApplication();
    status.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));

    // change status which goes into the lower aggregator (then the FAULT of s1 will win)
    app.outerGroup.s2.status = ctk::StatusOutput::Status::OK;
    app.outerGroup.s2.status.write();
    test.stepApplication();
    status.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::FAULT));
    app.outerGroup.s2.status = ctk::StatusOutput::Status::OFF;
    app.outerGroup.s2.status.write();
    test.stepApplication();
    status.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));
  }

  /********************************************************************************************************************/

  struct TestApplicationTags : ctk::Application {
    TestApplicationTags() : Application("testApp") {}
    ~TestApplicationTags() override { shutdown(); }

    struct OuterGroup : ctk::ModuleGroup {
      using ctk::ModuleGroup::ModuleGroup;

      StatusGenerator sA{this, "sA", "Status 1", ctk::TAGS{"A"}, ctk::StatusOutput::Status::WARNING};
      StatusGenerator sAB{this, "sAB", "Status 2", {"A", "B"}, ctk::StatusOutput::Status::OFF};

      // First level of aggregation: Input and output tags are identical
      ctk::StatusAggregator aggregateA{
          this, "aggregateA", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko, {"A"}, {"A"}};
      ctk::StatusAggregator aggregateB{
          this, "aggregateB", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko, {"B"}, {"B"}};

    } group{this, "Group", ""};

    // Use other priority mode here to make sure only the aggregators are aggregated, not the generators
    ctk::StatusAggregator aggregateA{
        this, "aggregateA", "aggregated status description", ctk::StatusAggregator::PriorityMode::ofwk, {"A"}};
    ctk::StatusAggregator aggregateB{
        this, "aggregateB", "aggregated status description", ctk::StatusAggregator::PriorityMode::ofwk, {"B"}};
    ctk::StatusAggregator aggregateAll{
        this, "aggregateAll", "aggregated status description", ctk::StatusAggregator::PriorityMode::fw_warn_mixed};
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testTags) {
    std::cout << "testTags" << std::endl;

    TestApplicationTags app;
    ctk::TestFacility test(app);

    auto aggregateA = test.getScalar<int>("/aggregateA");
    auto aggregateB = test.getScalar<int>("/aggregateB");
    auto aggregateAll = test.getScalar<int>("/aggregateAll");
    auto Group_aggregateA = test.getScalar<int>("/Group/aggregateA");
    auto Group_aggregateB = test.getScalar<int>("/Group/aggregateB");

    test.runApplication();

    // check initial values
    aggregateA.readLatest();
    aggregateB.readLatest();
    aggregateAll.readLatest();
    Group_aggregateA.readLatest();
    Group_aggregateB.readLatest();
    BOOST_CHECK_EQUAL(int(aggregateA), int(ctk::StatusOutput::Status::WARNING));
    BOOST_CHECK_EQUAL(int(aggregateB), int(ctk::StatusOutput::Status::OFF));
    BOOST_CHECK_EQUAL(int(aggregateAll), int(ctk::StatusOutput::Status::WARNING));
    BOOST_CHECK_EQUAL(int(Group_aggregateA), int(ctk::StatusOutput::Status::WARNING));
    BOOST_CHECK_EQUAL(int(Group_aggregateB), int(ctk::StatusOutput::Status::OFF));

    app.group.sAB.status = ctk::StatusOutput::Status::FAULT;
    app.group.sAB.status.write();

    test.stepApplication();

    // change value tagged with 'A' and 'B', affecting all aggregators to highest priority value, so it is visible
    // everywhere
    aggregateA.readLatest();
    aggregateB.readLatest();
    aggregateAll.readLatest();
    Group_aggregateA.readLatest();
    Group_aggregateB.readLatest();
    BOOST_CHECK_EQUAL(int(aggregateA), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(int(aggregateB), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(int(aggregateAll), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(int(Group_aggregateA), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(int(Group_aggregateB), int(ctk::StatusOutput::Status::FAULT));
  }

  /********************************************************************************************************************/

  struct TestApplicationAggregatorTags : ctk::Application {
    TestApplicationAggregatorTags() : Application("testApp") {}
    ~TestApplicationAggregatorTags() override { shutdown(); }

    struct OuterGroup : ctk::ModuleGroup {
      using ctk::ModuleGroup::ModuleGroup;

      StatusGenerator sA{this, "sA", "Status A", ctk::TAGS{"A"}};
      StatusGenerator sB1{this, "sB1", "Status B1", ctk::TAGS{"B"}};
      StatusGenerator sB2{this, "sB2", "Status B2", ctk::TAGS{"B"}};
      StatusGenerator sC{this, "sC", "Status C", ctk::TAGS{"C"}};

      ctk::StatusAggregator aggregateA{
          this, "aggregatedA", "", ctk::StatusAggregator::PriorityMode::fwko, {"A"}, {"AGG_A"}};
      //  Same input and output tag. When aggregating tag "B" the status outputs themselves shoudl not be taken again.
      ctk::StatusAggregator aggregateB{
          this, "aggregatedB", "", ctk::StatusAggregator::PriorityMode::fwko, {"B"}, {"B"}};

      // Missing: Test of multiple aggregation tags. Currently only one tag is allowed due to the missing
      // design decision whether multiple tags should be a logical AND or OR (#13256).
    } group{this, "Group", ""};

    // Does not aggregate an aggregator
    ctk::StatusAggregator aggregateA{this, "aggA", "", ctk::StatusAggregator::PriorityMode::fwko, {"A"}};
    // Aggregates the A aggregator
    ctk::StatusAggregator aggAggA{this, "aggAggA", "", ctk::StatusAggregator::PriorityMode::fwko, {"AGG_A"}};
    // Aggregates the B aggregator
    ctk::StatusAggregator aggAggB{this, "aggAggB", "", ctk::StatusAggregator::PriorityMode::fwko, {"B"}};
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testAggregatorTags) {
    std::cout << "testAggregatorTags" << std::endl;

    TestApplicationAggregatorTags app;
    ctk::TestFacility test(app);

    auto checkForName = [](auto& accessors, std::string name) {
      return std::any_of(accessors.begin(), accessors.end(), [&name](auto acc) { return acc.getName() == name; });
    };

    auto accessorsAggA = app.aggregateA.getAccessorListRecursive();
    // One aggregated input ("sA") + plus 3 inputs/outputs from the aggregator itself
    // "aggregatedA" is not used because the tag "A" is its input. The output tag is "AGG_A"
    BOOST_TEST(accessorsAggA.size() == 4);
    BOOST_CHECK(checkForName(accessorsAggA, "sA"));
    BOOST_CHECK(!checkForName(accessorsAggA, "aggregatedA"));

    auto accessorsAggAggA = app.aggAggA.getAccessorListRecursive();
    // One aggregated input ("aggregatedA") + according status message + plus 3 inputs/outputs from the aggregator
    // itself "sA" is not used because the tag "A" is its input. The output tag is "AGG_A"
    BOOST_TEST(accessorsAggAggA.size() == 5);
    BOOST_CHECK(checkForName(accessorsAggAggA, "aggregatedA"));
    BOOST_CHECK(!checkForName(accessorsAggAggA, "sA"));

    auto accessorsAggAggB = app.aggAggB.getAccessorListRecursive();
    // One aggregated input ("aggregatedB") + according status message + plus 3 inputs/outputs from the aggregator
    // itself "sB1" and "sB2" are not used because their input is already aggregated.
    BOOST_TEST(accessorsAggAggB.size() == 5);
    BOOST_CHECK(checkForName(accessorsAggAggB, "aggregatedB"));
    BOOST_CHECK(!checkForName(accessorsAggAggB, "sB1"));
    BOOST_CHECK(!checkForName(accessorsAggAggB, "sB2"));
  }

  /********************************************************************************************************************/

  struct TestApplicationMessage : ctk::Application {
    TestApplicationMessage() : Application("testApp") {}
    ~TestApplicationMessage() override { shutdown(); }

    StatusGenerator s{this, "s", "Status", {}, ctk::StatusOutput::Status::OK};

    struct OuterGroup : ctk::ModuleGroup {
      using ctk::ModuleGroup::ModuleGroup;

      StatusGenerator s1{this, "s1", "Status 1", {}, ctk::StatusOutput::Status::OK};

      StatusWithMessageGenerator s2{this, "s2", "Status 2", {}, ctk::StatusOutput::Status::OK};

      ctk::StatusAggregator extraAggregator{
          this, "/Aggregated/extraStatus", "aggregated status description", ctk::StatusAggregator::PriorityMode::ofwk};

    } outerGroup{this, "OuterGroup", ""};

    ctk::StatusAggregator aggregator{
        this, "Aggregated/status", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko};
  };

  /********************************************************************************************************************/

  // test behavior for status+string:
  // test that status aggregator always has a message output and hands it over to next status aggregator
  BOOST_AUTO_TEST_CASE(testStatusMessage) {
    std::cout << "testStatusMessage" << std::endl;
    TestApplicationMessage app;

    ctk::TestFacility test(app);

    auto status = test.getScalar<int>("/Aggregated/status");
    auto statusMessage = test.getScalar<std::string>("/Aggregated/status_message");
    auto innerStatus = test.getScalar<int>("/Aggregated/extraStatus");
    auto innerStatusMessage = test.getScalar<std::string>("/Aggregated/extraStatus_message");

    test.runApplication();

    // check the initial values
    innerStatus.readLatest();
    BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::OK));
    innerStatusMessage.readLatest();
    BOOST_CHECK_EQUAL(std::string(innerStatusMessage), "");
    status.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
    statusMessage.readLatest();
    BOOST_CHECK_EQUAL(std::string(statusMessage), "");

    // check normal status (without message) going to fault
    app.outerGroup.s1.status = ctk::StatusOutput::Status::FAULT;
    app.outerGroup.s1.status.write();
    test.stepApplication();
    status.readLatest();
    statusMessage.readLatest();
    innerStatus.readLatest();
    innerStatusMessage.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::FAULT));
    std::string faultString1 = "/OuterGroup/s1/s1 switched to FAULT";
    BOOST_CHECK_EQUAL(std::string(statusMessage), faultString1);
    BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(std::string(innerStatusMessage), faultString1);

    // go back to OK
    app.outerGroup.s1.status = ctk::StatusOutput::Status::OK;
    app.outerGroup.s1.status.write();
    test.stepApplication();
    status.readLatest();
    statusMessage.readLatest();
    innerStatus.readLatest();
    innerStatusMessage.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
    BOOST_CHECK_EQUAL(std::string(statusMessage), "");
    BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::OK));
    BOOST_CHECK_EQUAL(std::string(innerStatusMessage), "");

    // check StatusWithMessage going to fault
    std::string faultString2 = "Status 2 at fault";
    app.outerGroup.s2.setCurrentVersionNumber({});
    app.outerGroup.s2.status.write(ctk::StatusOutput::Status::FAULT, faultString2);
    test.stepApplication();
    status.readLatest();
    statusMessage.readLatest();
    innerStatus.readLatest();
    innerStatusMessage.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(std::string(statusMessage), faultString2);
    BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(std::string(innerStatusMessage), faultString2);

    // set normal status to fault, too, to see the right message "wins" (first message should stay)
    app.outerGroup.s1.setCurrentVersionNumber({});
    app.outerGroup.s1.status = ctk::StatusOutput::Status::FAULT;
    app.outerGroup.s1.status.write();
    test.stepApplication();
    status.readLatest();
    statusMessage.readLatest();
    innerStatus.readLatest();
    innerStatusMessage.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(std::string(statusMessage), faultString2);
    BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(std::string(innerStatusMessage), faultString2);

    // go back to OK
    app.outerGroup.s1.setCurrentVersionNumber({});
    app.outerGroup.s1.status = ctk::StatusOutput::Status::OK;
    app.outerGroup.s1.status.write();
    app.outerGroup.s2.setCurrentVersionNumber({});
    app.outerGroup.s2.status.writeOk();
    test.stepApplication();
    status.readLatest();
    statusMessage.readLatest();
    innerStatus.readLatest();
    innerStatusMessage.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
    BOOST_CHECK_EQUAL(std::string(statusMessage), "");
    BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::OK));
    BOOST_CHECK_EQUAL(std::string(innerStatusMessage), "");

    // set both status to fault in alternate order (compared to before), again the first message should "win"
    app.outerGroup.s1.setCurrentVersionNumber({});
    app.outerGroup.s1.status = ctk::StatusOutput::Status::FAULT;
    app.outerGroup.s1.status.write();
    app.outerGroup.s2.setCurrentVersionNumber({});
    app.outerGroup.s2.status.write(ctk::StatusOutput::Status::FAULT, faultString2);
    test.stepApplication();
    status.readLatest();
    statusMessage.readLatest();
    innerStatus.readLatest();
    innerStatusMessage.readLatest();
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(std::string(statusMessage), faultString1);
    BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::FAULT));
    BOOST_CHECK_EQUAL(std::string(innerStatusMessage), faultString1);
  }

  /********************************************************************************************************************/

} // namespace Tests::testStatusAggregator
