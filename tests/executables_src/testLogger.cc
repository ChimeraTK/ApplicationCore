// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cstdlib>

#define BOOST_TEST_MODULE testLogger

#include "Logger.h"

#include <boost/test/included/unit_test.hpp>

namespace ctk = ChimeraTK;

/**********************************************************************************************************************/

// Attention! This test must be run first, otherwise setting the environment variable will not have any effect!
BOOST_AUTO_TEST_CASE(TestEnvVarSetsMinSeverity) {
  std::cout << "testEnvVarSetsMinSeverity" << std::endl;

  // Set environment variable before creating logger instance
  setenv("CHIMERATK_LOG_LEVEL", "warning", 1);

  auto& logger = ctk::Logger::getInstance();

  // With CHIMERATK_LOG_LEVEL=warning, the default minimum severity should be warning,
  // so streams for lower log levels should be bad
  auto traceStream = logger.getStream(ctk::Logger::Severity::trace, "test");
  auto debugStream = logger.getStream(ctk::Logger::Severity::debug, "test");
  auto infoStream = logger.getStream(ctk::Logger::Severity::info, "test");
  auto warningStream = logger.getStream(ctk::Logger::Severity::warning, "test");
  auto errorStream = logger.getStream(ctk::Logger::Severity::error, "test");

  BOOST_TEST(!traceStream.good());
  BOOST_TEST(!debugStream.good());
  BOOST_TEST(!infoStream.good());
  BOOST_TEST(warningStream.good());
  BOOST_TEST(errorStream.good());
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(TestMinSeveritySetPublicly) {
  std::cout << "testMinSeveritySetPublicly" << std::endl;

  auto& logger = ctk::Logger::getInstance();

  // Set minimum severity to trace, so all streams should be good
  logger.setMinSeverity(ctk::Logger::Severity::trace);

  auto traceStream = logger.getStream(ctk::Logger::Severity::trace, "test");
  auto debugStream = logger.getStream(ctk::Logger::Severity::debug, "test");
  auto infoStream = logger.getStream(ctk::Logger::Severity::info, "test");

  BOOST_TEST(traceStream.good());
  BOOST_TEST(debugStream.good());
  BOOST_TEST(infoStream.good());

  // Set minimum severity to error, so only error stream should be good
  logger.setMinSeverity(ctk::Logger::Severity::error);

  auto errorStream = logger.getStream(ctk::Logger::Severity::error, "test");
  auto warningStream = logger.getStream(ctk::Logger::Severity::warning, "test");

  BOOST_TEST(errorStream.good());
  BOOST_TEST(!warningStream.good());

  // Reset to env-default for other tests
  logger.setMinSeverity(ctk::Logger::Severity::debug);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(TestStreamOutput) {
  std::cout << "testStreamOutput" << std::endl;

  auto& logger = ctk::Logger::getInstance();

  // Ensure we can capture output from info-level logs
  logger.setMinSeverity(ctk::Logger::Severity::info);

  // Send a log message. We cannot easily capture the async output here,
  // but we can verify the stream is in a good state, meaning the message
  // will be queued for output.
  {
    auto stream = logger.getStream(ctk::Logger::Severity::info, "testContext");
    BOOST_TEST(stream.good());
    stream << "test message" << std::endl;
  }

  // Give the logging thread time to process
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Verify that a debug message is filtered out at sender side when
  // minimum severity is set to info
  {
    auto stream = logger.getStream(ctk::Logger::Severity::debug, "testContext");
    BOOST_TEST(!stream.good());
  }

  logger.setMinSeverity(ctk::Logger::Severity::debug);
}

/**********************************************************************************************************************/
