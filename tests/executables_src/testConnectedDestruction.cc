// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <ChimeraTK/BackendFactory.h>

#include <boost/process.hpp>

#define BOOST_TEST_MODULE testConnectedDestruction
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE(testDestroyConnectedApplicationModule) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}

BOOST_AUTO_TEST_CASE(testDestroyConnectedApplicationModuleWithArray) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}

BOOST_AUTO_TEST_CASE(testMoveConnectedApplicationModule) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}

BOOST_AUTO_TEST_CASE(testMoveConnectedApplicationModuleWithArray) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}

BOOST_AUTO_TEST_CASE(testDestroyConnectedVariableGroupDelete) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}

BOOST_AUTO_TEST_CASE(testDestroyConnectedVariableGroupArrayDelete) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}

BOOST_AUTO_TEST_CASE(testMoveConnectedVariableGroup) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}

BOOST_AUTO_TEST_CASE(testMoveConnectedVariableGroupArray) {
  auto result = boost::process::system(
      "testConnectedDestructionRunner", "-t", boost::unit_test::framework::current_test_case().p_name.value);
  BOOST_CHECK(result != 0);
}
