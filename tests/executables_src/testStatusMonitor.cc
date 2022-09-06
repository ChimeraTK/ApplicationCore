// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <future>

#define BOOST_TEST_MODULE testExceptionHandling

#include "Application.h"
#include "ControlSystemModule.h"
#include "ScalarAccessor.h"
#include "StatusMonitor.h"
#include "TestFacility.h"

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/* dummy application - for new StatusMonitor interface */
template<typename T>
struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() override { shutdown(); }

  void defineConnections() override {
    findTag(".*").connectTo(cs); // publish everything to CS
    findTag("MON_PARAMS")
        .connectTo(cs["MonitorParameters"]); // cable the parameters in addition (checking that tags are set correctly)
    findTag("MON_OUTPUT")
        .connectTo(cs["MonitorOutput"]); // cable the parameters in addition (checking that tags are set correctly)
  }
  ctk::ControlSystemModule cs;
  T monitor{this, "/input/path", "/output/path", "/parameters", "Now this is a nice monitor...",
      ctk::TAGS{"MON_OUTPUT"}, ctk::TAGS{"MON_PARAMS"}};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMaxMonitor) {
  std::cout << "testMaxMonitor" << std::endl;
  TestApplication<ctk::MaxMonitor<double_t>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status.value).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test;
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
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  //   //just below the warning level
  watch = 49.99;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // slightly above at the upper warning threshold (exact is not good due to rounding errors in floats/doubles)
  watch = 50.01;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // drop in a disable test.
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just below the fault threshold,. still warning
  watch = 59.99;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // slightly above at the upper fault threshold (exact is not good due to rounding errors in floats/doubles)
  watch = 60.01;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // increase well above the upper fault level
  watch = 65;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // now check that changing the status is updated correctly if we change the limits

  // increase fault value greater than watch
  fault = 68;
  fault.write();
  test.stepApplication();
  status.readLatest();
  // should be in WARNING state.
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // increase warning value greater than watch
  warning = 66;
  warning.write();
  test.stepApplication();
  status.readLatest();
  // should be in OK state.
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the upper fault limit below the upper warning limit and below the current temperature. The warning is not
  // active, but the fault. Although this is not a reasonable configuration the fault limit must superseed the warning
  // and the status has to be fault.
  fault = 60;
  fault.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // check that the tags are applied correctly
  BOOST_CHECK_EQUAL(status, test.readScalar<int32_t>("/MonitorOutput/output/path"));
  BOOST_CHECK_EQUAL(fault, test.readScalar<double_t>("/MonitorParameters/parameters/upperFaultThreshold"));
  BOOST_CHECK_EQUAL(warning, test.readScalar<double_t>("/MonitorParameters/parameters/upperWarningThreshold"));
  disable = 1;
  disable.write();
  test.stepApplication();
  BOOST_CHECK_EQUAL(disable, test.readScalar<ChimeraTK::Boolean>("/MonitorParameters/parameters/disable"));
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMinMonitor) {
  std::cout << "testMinMonitor" << std::endl;

  TestApplication<ctk::MinMonitor<uint>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status.value).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test;
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
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // just abow the lower warning limit
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // exactly at the lower warning limit
  watch = 40;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // drop in a disable test.
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just above the lower fault limit
  watch = 31;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // exactly at the lower fault limit (only well defined for int)
  watch = 30;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // way bellow the lower fault limit
  watch = 12;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // move the temperature back to the good range and check that the status updates correctly when changing the limits
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // change upper warning limit
  warning = 42;
  warning.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // rise the temperature above the lower warning limit
  watch = 43;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the lower fault limit above the lower warning limit. The warning is not active, but the fault.
  // Although this is not a reasonable configuration the fault limit must superseed the warning and the status has to be fault.
  fault = 44;
  fault.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // check that the tags are applied correctly
  BOOST_CHECK_EQUAL(status, test.readScalar<int32_t>("/MonitorOutput/output/path"));
  BOOST_CHECK_EQUAL(fault, test.readScalar<uint>("/MonitorParameters/parameters/lowerFaultThreshold"));
  BOOST_CHECK_EQUAL(warning, test.readScalar<uint>("/MonitorParameters/parameters/lowerWarningThreshold"));
  disable = 1;
  disable.write();
  test.stepApplication();
  BOOST_CHECK_EQUAL(disable, test.readScalar<ChimeraTK::Boolean>("/MonitorParameters/parameters/disable"));
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testRangeMonitor) {
  std::cout << "testRangeMonitor" << std::endl;
  TestApplication<ctk::RangeMonitor<int>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status.value).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test;
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

  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // just below the warning level
  watch = 49;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // exactly at the upper warning threshold (only well defined for int)
  watch = 50;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // drop in a disable test.
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just below the fault threshold,. still warning
  watch = 59;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // exactly at the upper warning threshold (only well defined for int)
  watch = 60;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // increase well above the upper fault level
  watch = 65;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // back to ok, just abow the lower warning limit
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // exactly at the lower warning limit
  watch = 40;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // just above the lower fault limit
  watch = 31;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // exactly at the lower fault limit (only well defined for int)
  watch = 30;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // way bellow the lower fault limit
  watch = 12;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // Put the value back to the good range, then check that chaning the threshold also updated the status
  watch = 49;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // change upper warning limit
  warningUpperLimit = 48;
  warningUpperLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // lower the temperature below the upper warning limit
  watch = 47;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the upper fault limit below the upper warning limit. The warning is not active, but the fault.
  // Although this is not a reasonable configuration the fault limit must superseed the warning and the status has to be fault.
  faultUpperLimit = 46;
  faultUpperLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // move the temperature back to the good range and repeat for the lower limits
  watch = 41;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // change upper warning limit
  warningLowerLimit = 42;
  warningLowerLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::WARNING));

  // rise the temperature above the lower warning limit
  watch = 43;
  watch.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // Set the lower fault limit above the lower warning limit. The warning is not active, but the fault.
  // Although this is not a reasonable configuration the fault limit must superseed the warning and the status has to be fault.
  faultLowerLimit = 44;
  faultLowerLimit.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // check that the tags are applied correctly
  BOOST_CHECK_EQUAL(status, test.readScalar<int32_t>("/MonitorOutput/output/path"));
  BOOST_CHECK_EQUAL(faultLowerLimit, test.readScalar<int>("/MonitorParameters/parameters/lowerFaultThreshold"));
  BOOST_CHECK_EQUAL(warningLowerLimit, test.readScalar<int>("/MonitorParameters/parameters/lowerWarningThreshold"));
  BOOST_CHECK_EQUAL(faultUpperLimit, test.readScalar<int>("/MonitorParameters/parameters/upperFaultThreshold"));
  BOOST_CHECK_EQUAL(warningUpperLimit, test.readScalar<int>("/MonitorParameters/parameters/upperWarningThreshold"));
  disable = 1;
  disable.write();
  test.stepApplication();
  BOOST_CHECK_EQUAL(disable, test.readScalar<ChimeraTK::Boolean>("/MonitorParameters/parameters/disable"));
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExactMonitor) {
  std::cout << "testExactMonitor" << std::endl;
  TestApplication<ctk::ExactMonitor<int64_t>> app;

  // check that the reserved StatusOutput tag is present at the output, required for StatusAggregator integration
  auto tags = ctk::VariableNetworkNode(app.monitor.status.value).getTags();
  BOOST_CHECK(tags.find(ctk::StatusOutput::tagStatusOutput) != tags.end());

  ctk::TestFacility test;
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
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // drop in a disable test.
  auto disable = test.getScalar<ChimeraTK::Boolean>("/parameters/disable");
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // set watch value different than required value
  watch = 414;
  watch.write();
  test.stepApplication();
  status.readLatest();
  // should be in FAULT state.
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // drop in a disable test.
  disable = 1;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OFF));

  disable = 0;
  disable.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  watch = 409;
  watch.write();
  test.stepApplication();
  status.readLatest();
  // should be in OK state.
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // set requiredValue value different than watch value
  requiredValue = 413;
  requiredValue.write();
  test.stepApplication();
  status.readLatest();
  // should be in WARNING state.
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::FAULT));

  // set requiredValue value equals to watch value
  requiredValue = 409;
  requiredValue.write();
  test.stepApplication();
  status.readLatest();
  // should be in WARNING state.
  BOOST_CHECK_EQUAL(status, static_cast<int>(ChimeraTK::StatusOutput::Status::OK));

  // check that the tags are applied correctly
  BOOST_CHECK_EQUAL(status, test.readScalar<int32_t>("/MonitorOutput/output/path"));
  BOOST_CHECK_EQUAL(requiredValue, test.readScalar<int64_t>("/MonitorParameters/parameters/requiredValue"));
  disable = 1;
  disable.write();
  test.stepApplication();
  BOOST_CHECK_EQUAL(disable, test.readScalar<ChimeraTK::Boolean>("/MonitorParameters/parameters/disable"));
}

/*********************************************************************************************************************/
