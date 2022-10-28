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

namespace ctk = ChimeraTK;
using Fixture = fixture_with_poll_and_push_input<false>;

BOOST_FIXTURE_TEST_SUITE(versionPropagation, Fixture)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(versionPropagation_testPolledRead) {
  std::cout << "versionPropagation_testPolledRead" << std::endl;
  auto moduleVersion = application.group1.pollModule.getCurrentVersionNumber();
  auto pollVariableVersion = pollVariable2.getVersionNumber();

  application.group1.outputModule.setCurrentVersionNumber({});
  outputVariable2.write();
  pollVariable2.read();

  assert(pollVariable2.getVersionNumber() > pollVariableVersion);
  BOOST_CHECK(moduleVersion == application.group1.pollModule.getCurrentVersionNumber());
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(versionPropagation_testPolledReadNonBlocking) {
  std::cout << "versionPropagation_testPolledReadNonBlocking" << std::endl;
  auto moduleVersion = application.group1.pollModule.getCurrentVersionNumber();
  auto pollVariableVersion = pollVariable2.getVersionNumber();

  application.group1.outputModule.setCurrentVersionNumber({});
  outputVariable2.write();
  pollVariable2.readNonBlocking();

  assert(pollVariable2.getVersionNumber() > pollVariableVersion);
  BOOST_CHECK(moduleVersion == application.group1.pollModule.getCurrentVersionNumber());
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(versionPropagation_testPolledReadLatest) {
  std::cout << "versionPropagation_testPolledReadLatest" << std::endl;
  auto moduleVersion = application.group1.pollModule.getCurrentVersionNumber();
  auto pollVariableVersion = pollVariable2.getVersionNumber();

  application.group1.outputModule.setCurrentVersionNumber({});
  outputVariable2.write();
  pollVariable2.readLatest();

  assert(pollVariable2.getVersionNumber() > pollVariableVersion);
  BOOST_CHECK(moduleVersion == application.group1.pollModule.getCurrentVersionNumber());
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(versionPropagation_testPushTypeRead) {
  std::cout << "versionPropagation_testPushTypeRead" << std::endl;
  // Make sure we pop out any stray values in the pushInput before test start:
  CHECK_TIMEOUT(pushVariable.readLatest() == false, 10000);

  ctk::VersionNumber nextVersionNumber = {};
  interrupt.write();
  pushVariable.read();
  assert(pushVariable.getVersionNumber() > nextVersionNumber);
  BOOST_CHECK(application.group1.pushModule.getCurrentVersionNumber() == pushVariable.getVersionNumber());
}

/*********************************************************************************************************************/

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

/*********************************************************************************************************************/

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

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()

/*********************************************************************************************************************/
