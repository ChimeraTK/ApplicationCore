// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

// Define a name for the test module.
#define BOOST_TEST_MODULE testTemplateServer
// Only after defining the name include the unit test header.
#include "Server.h"

#include <ChimeraTK/ApplicationCore/TestFacility.h>

#include <boost/test/included/unit_test.hpp>

// Declare the server instance
static Server theServer;

static ChimeraTK::TestFacility testFacility{theServer};

struct TestFixture {
  TestFixture() { testFacility.runApplication(); }
};
static TestFixture fixture;

using namespace boost::unit_test_framework;

/// TestSuite for the server, adapt name
BOOST_AUTO_TEST_SUITE(TemplateServerTestSuite)

/**********************************************************************************************************************/

/// A template test case
BOOST_AUTO_TEST_CASE(testTemplate) {
  std::cout << "testTemplate" << std::endl;
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
