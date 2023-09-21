// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <future>

#define BOOST_TEST_MODULE testStatusMonitor

#include "Application.h"
#include "ScalarAccessor.h"
#include "StatusMonitor.h"
#include "TestFacility.h"

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/*********************************************************************************************************************/
/* Test dummy application for the Monitors ************************************************************************/
/*********************************************************************************************************************/

template<typename T>
struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() override { shutdown(); }

  T monitor{this, "/input/path", "/output/path", "/parameters", "Now this is a nice monitor...",
      ctk::TAGS{"MON_OUTPUT"}, ctk::TAGS{"MON_PARAMS"}};
};

/*********************************************************************************************************************/
/* Test generic functionality of the Monitors ************************************************************************/
/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMaxMonitor) {
  std::cout << "testMaxMonitor" << std::endl;
  TestApplication<ctk::MaxMonitor<double_t>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test(app);
  test.runApplication();
  // app.cs.dump();

  auto warning = test.getScalar<double_t>(std::string("/parameters/upperWarningThreshold"));
  warning = 50.0;
  warning.write();
  test.stepApplication();

  auto fault = test.getScalar<double_t>(std::string("/parameters/upperFaultThreshold"));
  fault = 60.0;
  fault.write();
  test.stepApplication();

  auto watch = test.getScalar<double_t>(std::string("/input/path"));
  watch = 40.0;
  watch.write();
  test.stepApplication();

  auto status = test.getScalar<int32_t>(std::string("/output/path"));
  status.readLatest();

  // should be in OK state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  //   //just below the warning level
  watch = 49.99;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // slightly above at the upper warning threshold (exact is not good due to rounding errors in floats/doubles)
  watch = 50.01;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // drop in a disable test.
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just below the fault threshold,. still warning
  watch = 59.99;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // slightly above at the upper fault threshold (exact is not good due to rounding errors in floats/doubles)
  watch = 60.01;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // increase well above the upper fault level
  watch = 65;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // now check that changing the status is updated correctly if we change the limits

  // increase fault value greater than watch
  fault = 68;
  fault.write();
  test.stepApplication();
  status.readLatest();
  // should be in WARNING state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // increase warning value greater than watch
  warning = 66;
  warning.write();
  test.stepApplication();
  status.readLatest();
  // should be in OK state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the upper fault limit below the upper warning limit and below the current temperature. The warning is not
  // active, but the fault. Although this is not a reasonable configuration the fault limit must superseed the warning
  // and the status has to be fault.
  fault = 60;
  fault.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // check that the tags are applied correctly
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("upperFaultThreshold") == app.monitor.faultThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("upperWarningThreshold") == app.monitor.warningThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("disable") == app.monitor.disable);
  // BOOST_CHECK(app.findTag("MON_OUTPUT")["output"]("path") == app.monitor.status);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMinMonitor) {
  std::cout << "testMinMonitor" << std::endl;

  TestApplication<ctk::MinMonitor<uint>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test(app);
  test.runApplication();
  // app.dumpConnections();

  auto warning = test.getScalar<uint>(std::string("/parameters/lowerWarningThreshold"));
  warning = 40;
  warning.write();
  test.stepApplication();

  auto fault = test.getScalar<uint>(std::string("/parameters/lowerFaultThreshold"));
  fault = 30;
  fault.write();
  test.stepApplication();

  auto watch = test.getScalar<uint>(std::string("/input/path"));
  watch = 45;
  watch.write();
  test.stepApplication();

  auto status = test.getScalar<int32_t>(std::string("/output/path"));
  status.readLatest();

  // should be in OK state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // just abow the lower warning limit
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // exactly at the lower warning limit
  watch = 40;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // drop in a disable test.
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just above the lower fault limit
  watch = 31;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // exactly at the lower fault limit (only well defined for int)
  watch = 30;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // way bellow the lower fault limit
  watch = 12;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // move the temperature back to the good range and check that the status updates correctly when changing the limits
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // change upper warning limit
  warning = 42;
  warning.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // rise the temperature above the lower warning limit
  watch = 43;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the lower fault limit above the lower warning limit. The warning is not active, but the fault.
  // Although this is not a reasonable configuration the fault limit must superseed the warning and the status has to be fault.
  fault = 44;
  fault.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // check that the tags are applied correctly
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("lowerFaultThreshold") == app.monitor.faultThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("lowerWarningThreshold") == app.monitor.warningThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("disable") == app.monitor.disable);
  // BOOST_CHECK(app.findTag("MON_OUTPUT")["output"]("path") == app.monitor.status);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testRangeMonitor) {
  std::cout << "testRangeMonitor" << std::endl;
  TestApplication<ctk::RangeMonitor<int>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test(app);
  test.runApplication();
  // app.dumpConnections();

  auto warningUpperLimit = test.getScalar<int>(std::string("/parameters/upperWarningThreshold"));
  warningUpperLimit = 50;
  warningUpperLimit.write();
  test.stepApplication();

  auto warningLowerLimit = test.getScalar<int>(std::string("/parameters/lowerWarningThreshold"));
  warningLowerLimit = 40;
  warningLowerLimit.write();
  test.stepApplication();

  auto faultUpperLimit = test.getScalar<int>(std::string("/parameters/upperFaultThreshold"));
  faultUpperLimit = 60;
  faultUpperLimit.write();
  test.stepApplication();

  auto faultLowerLimit = test.getScalar<int>(std::string("/parameters/lowerFaultThreshold"));
  faultLowerLimit = 30;
  faultLowerLimit.write();
  test.stepApplication();

  // start with a good value
  auto watch = test.getScalar<int>(std::string("/input/path"));
  watch = 45;
  watch.write();
  test.stepApplication();

  auto status = test.getScalar<int32_t>(std::string("/output/path"));
  status.readLatest();

  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // just below the warning level
  watch = 49;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // exactly at the upper warning threshold (only well defined for int)
  watch = 50;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // drop in a disable test.
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just below the fault threshold,. still warning
  watch = 59;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // exactly at the upper warning threshold (only well defined for int)
  watch = 60;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // increase well above the upper fault level
  watch = 65;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // back to ok, just abow the lower warning limit
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // exactly at the lower warning limit
  watch = 40;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just above the lower fault limit
  watch = 31;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // exactly at the lower fault limit (only well defined for int)
  watch = 30;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // way bellow the lower fault limit
  watch = 12;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // Put the value back to the good range, then check that chaning the threshold also updated the status
  watch = 49;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // change upper warning limit
  warningUpperLimit = 48;
  warningUpperLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // lower the temperature below the upper warning limit
  watch = 47;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the upper fault limit below the upper warning limit. The warning is not active, but the fault.
  // Although this is not a reasonable configuration the fault limit must superseed the warning and the status has to be fault.
  faultUpperLimit = 46;
  faultUpperLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // move the temperature back to the good range and repeat for the lower limits
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // change upper warning limit
  warningLowerLimit = 42;
  warningLowerLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // rise the temperature above the lower warning limit
  watch = 43;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the lower fault limit above the lower warning limit. The warning is not active, but the fault.
  // Although this is not a reasonable configuration the fault limit must superseed the warning and the status has to be fault.
  faultLowerLimit = 44;
  faultLowerLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // check that the tags are applied correctly
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("lowerFaultThreshold") == app.monitor.faultLowerThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("lowerWarningThreshold") == app.monitor.warningLowerThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("upperFaultThreshold") == app.monitor.faultUpperThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("upperWarningThreshold") == app.monitor.warningUpperThreshold);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("disable") == app.monitor.disable);
  // BOOST_CHECK(app.findTag("MON_OUTPUT")["output"]("path") == app.monitor.status);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExactMonitor) {
  std::cout << "testExactMonitor" << std::endl;
  TestApplication<ctk::ExactMonitor<int64_t>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test(app);
  test.runApplication();
  // app.dumpConnections();

  auto requiredValue = test.getScalar<int64_t>(std::string("/parameters/requiredValue"));
  requiredValue = 409;
  requiredValue.write();
  test.stepApplication();

  auto watch = test.getScalar<int64_t>(std::string("/input/path"));
  watch = 409;
  watch.write();
  test.stepApplication();

  auto status = test.getScalar<int32_t>(std::string("/output/path"));
  status.readLatest();

  // should be in OK state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // set watch value different than required value
  watch = 414;
  watch.write();
  test.stepApplication();
  status.readLatest();
  // should be in FAULT state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = true;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = false;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  watch = 409;
  watch.write();
  test.stepApplication();
  status.readLatest();
  // should be in OK state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // set requiredValue value different than watch value
  requiredValue = 413;
  requiredValue.write();
  test.stepApplication();
  status.readLatest();
  // should be in WARNING state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // set requiredValue value equals to watch value
  requiredValue = 409;
  requiredValue.write();
  test.stepApplication();
  status.readLatest();
  // should be in WARNING state.
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // check that the tags are applied correctly
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("requiredValue") == app.monitor.requiredValue);
  // BOOST_CHECK(app.findTag("MON_PARAMS")["parameters"]("disable") == app.monitor.disable);
  // BOOST_CHECK(app.findTag("MON_OUTPUT")["output"]("path") == app.monitor.status);
}

/*********************************************************************************************************************/
/* Test initial value propagation for the Monitors *******************************************************************/
/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMaxMonitorInitialValuePropagation) {
  std::cout << "testMaxMonitorInitialValuePropagation" << std::endl;

  {
    TestApplication<ctk::MaxMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/upperFaultThreshold", 60.0);
    test.setScalarDefault<float_t>("/parameters/upperWarningThreshold", 50.0);
    test.setScalarDefault<float_t>("/input/path", 45.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));
  }
  {
    TestApplication<ctk::MaxMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/upperFaultThreshold", 60.0);
    test.setScalarDefault<float_t>("/parameters/upperWarningThreshold", 50.0);
    test.setScalarDefault<float_t>("/input/path", 55.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));
  }
  {
    TestApplication<ctk::MaxMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/upperFaultThreshold", 60.0);
    test.setScalarDefault<float_t>("/parameters/upperWarningThreshold", 50.0);
    test.setScalarDefault<float_t>("/input/path", 55.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", true);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));
  }
  {
    TestApplication<ctk::MaxMonitor<double_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<double_t>("/parameters/upperFaultThreshold", 60.0);
    test.setScalarDefault<double_t>("/parameters/upperWarningThreshold", 50.0);
    test.setScalarDefault<double_t>("/input/path", 65.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMinMonitorInitialValuePropagation) {
  std::cout << "testMinMonitorInitialValuePropagation" << std::endl;

  {
    TestApplication<ctk::MinMonitor<double_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<double_t>("/parameters/lowerFaultThreshold", 50.0);
    test.setScalarDefault<double_t>("/parameters/lowerWarningThreshold", 60.0);
    test.setScalarDefault<double_t>("/input/path", 65.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));
  }
  {
    TestApplication<ctk::MinMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/lowerFaultThreshold", 50.0);
    test.setScalarDefault<float_t>("/parameters/lowerWarningThreshold", 60.0);
    test.setScalarDefault<float_t>("/input/path", 55.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));
  }
  {
    TestApplication<ctk::MinMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/lowerFaultThreshold", 50.0);
    test.setScalarDefault<float_t>("/parameters/lowerWarningThreshold", 60.0);
    test.setScalarDefault<float_t>("/input/path", 55.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", true);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));
  }
  {
    TestApplication<ctk::MinMonitor<int32_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<int32_t>("/parameters/lowerFaultThreshold", 50);
    test.setScalarDefault<int32_t>("/parameters/lowerWarningThreshold", 60);
    test.setScalarDefault<int32_t>("/input/path", 45);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testRangeMonitorInitialValuePropagation) {
  std::cout << "testRangeMonitorInitialValuePropagation" << std::endl;
  // each {} defines new application and test facility
  {
    TestApplication<ctk::RangeMonitor<double_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<double_t>("/parameters/upperFaultThreshold", 80.0);
    test.setScalarDefault<double_t>("/parameters/upperWarningThreshold", 70.0);
    test.setScalarDefault<double_t>("/parameters/lowerWarningThreshold", 60.0);
    test.setScalarDefault<double_t>("/parameters/lowerFaultThreshold", 50.0);
    test.setScalarDefault<double_t>("/input/path", 65.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));
  }
  {
    TestApplication<ctk::RangeMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/upperFaultThreshold", 80.0);
    test.setScalarDefault<float_t>("/parameters/upperWarningThreshold", 70.0);
    test.setScalarDefault<float_t>("/parameters/lowerWarningThreshold", 60.0);
    test.setScalarDefault<float_t>("/parameters/lowerFaultThreshold", 50.0);
    test.setScalarDefault<float_t>("/input/path", 75.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));
  }
  {
    TestApplication<ctk::RangeMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/upperFaultThreshold", 80.0);
    test.setScalarDefault<float_t>("/parameters/upperWarningThreshold", 70.0);
    test.setScalarDefault<float_t>("/parameters/lowerWarningThreshold", 60.0);
    test.setScalarDefault<float_t>("/parameters/lowerFaultThreshold", 50.0);
    test.setScalarDefault<float_t>("/input/path", 55.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));
  }
  {
    TestApplication<ctk::RangeMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/upperFaultThreshold", 80.0);
    test.setScalarDefault<float_t>("/parameters/upperWarningThreshold", 70.0);
    test.setScalarDefault<float_t>("/parameters/lowerWarningThreshold", 60.0);
    test.setScalarDefault<float_t>("/parameters/lowerFaultThreshold", 50.0);
    test.setScalarDefault<float_t>("/input/path", 55.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", true);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));
  }
  {
    TestApplication<ctk::RangeMonitor<int32_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<int32_t>("/parameters/upperFaultThreshold", 80);
    test.setScalarDefault<int32_t>("/parameters/upperWarningThreshold", 70);
    test.setScalarDefault<int32_t>("/parameters/lowerWarningThreshold", 60);
    test.setScalarDefault<int32_t>("/parameters/lowerFaultThreshold", 50);
    test.setScalarDefault<int32_t>("/input/path", 85);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));
  }
  {
    TestApplication<ctk::RangeMonitor<int32_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<int32_t>("/parameters/upperFaultThreshold", 80);
    test.setScalarDefault<int32_t>("/parameters/upperWarningThreshold", 70);
    test.setScalarDefault<int32_t>("/parameters/lowerWarningThreshold", 60);
    test.setScalarDefault<int32_t>("/parameters/lowerFaultThreshold", 50);
    test.setScalarDefault<int32_t>("/input/path", 45);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExactMonitorInitialValuePropagation) {
  std::cout << "testExactMonitorInitialValuePropagation" << std::endl;

  {
    TestApplication<ctk::ExactMonitor<double_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<double_t>("/parameters/requiredValue", 60.0);
    test.setScalarDefault<double_t>("/input/path", 60.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));
  }
  {
    TestApplication<ctk::ExactMonitor<float_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<float_t>("/parameters/requiredValue", 60.0);
    test.setScalarDefault<float_t>("/input/path", 55.0);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", true);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));
  }
  {
    TestApplication<ctk::ExactMonitor<int32_t>> app;
    ctk::TestFacility test(app);
    test.setScalarDefault<int32_t>("/parameters/requiredValue", 60);
    test.setScalarDefault<int32_t>("/input/path", 45);
    test.setScalarDefault<ChimeraTK::Boolean>("/parameters/disable", false);

    test.runApplication();

    BOOST_TEST(test.readScalar<int32_t>("/output/path") == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));
  }
}

/*********************************************************************************************************************/
/* Test data validity propagation for the Monitors *******************************************************************/
/*********************************************************************************************************************/

/*
 * Data validity is checked in base class of all monitors (MonitorBase) in method MonitorBase::setStatus only
 * There is no need to test all types of monitors so we gonna use MaxMonitor
 * */
BOOST_AUTO_TEST_CASE(testMonitorDataValidityPropagation) {
  std::cout << "testMonitorDataValidityPropagation" << std::endl;

  TestApplication<ctk::MaxMonitor<double_t>> app;
  ctk::TestFacility test(app);

  test.runApplication();

  auto fault = test.getScalar<double_t>("/parameters/upperFaultThreshold");
  auto warning = test.getScalar<double_t>("/parameters/upperWarningThreshold");
  auto watch = test.getScalar<double_t>("/input/path");
  auto status = test.getScalar<int32_t>("/output/path");

  fault = 60.0;
  fault.write();
  warning = 50.0;
  warning.write();
  watch = 40.0;
  watch.write();
  test.stepApplication();
  status.readLatest();
  // status is OK and data validity also
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));
  BOOST_CHECK(status.dataValidity() == ctk::DataValidity::ok);

  watch.setDataValidity(ctk::DataValidity::faulty);
  watch.write();
  test.stepApplication();
  status.readLatest();
  // status is not changed as watch is the same, data validity is changed -> test condition: getDataValidity() !=
  // lastStatusValidity
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::OK));
  BOOST_CHECK(status.dataValidity() == ctk::DataValidity::faulty);

  watch = 55.0;
  watch.write();
  test.stepApplication();
  status.readLatest();
  // status is changed, data validity is not changed -> test condition: status.value != newStatus
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));
  BOOST_CHECK(status.dataValidity() == ctk::DataValidity::faulty);

  watch = 70.0;
  watch.setDataValidity(ctk::DataValidity::ok);
  watch.write();
  test.stepApplication();
  status.readLatest();
  // status is changed, data validity is changed -> test condition: status.value != newStatus || status.value != newStatus
  BOOST_CHECK(status == static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));
  BOOST_CHECK(status.dataValidity() == ctk::DataValidity::ok);

  watch = 75.0;
  watch.setDataValidity(ctk::DataValidity::ok);
  watch.write();
  test.stepApplication();
  // status is not changed, data validity is not changed -> test that there is no new value of status
  BOOST_CHECK(status.readLatest() == false);
}
