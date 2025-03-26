// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testScriptedInitialisationHandler
#include "Application.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "ScriptedInitialisationHandler.h"
#include "TestFacility.h"

#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>

namespace Tests::testScriptedInitialisationHandler {

  using namespace ChimeraTK;

  /********************************************************************************************************************/

  struct TestApp : public Application {
    using Application::Application;
    ~TestApp() override { shutdown(); }

    SetDMapFilePath dmap{"test.dmap"};

    DeviceModule dev1{this, "Dummy0",
        "/MyModule/actuator"}; // pick one of the writable variables to AC knows that data type for the trigger

    // default name for the output variable (initScriptOutput)
    ScriptedInitHandler initHandler1{this, "InitHander1", "description", "./deviceInitScript1.bash", dev1};
    // change the name of the output variable in case a second script is needed. Shorten the error grace time to 1 second
    ScriptedInitHandler initHandler2{
        this, "InitHander2", "description", "./deviceInitScript2.bash", dev1, "secondInitScriptOutput", 1};
  };

  /*********************************************************************************************************************/

  struct Fixture {
    TestApp testApp{"ScriptedInitApp"};
    TestFacility testFacility{testApp, false};
    Fixture() {
      // always clear the temporary files before each test
      clearTemporaryFiles();
    }
    ~Fixture() {
      // always clear the temporary files after each test
      clearTemporaryFiles();
    }
    static void clearTemporaryFiles() {
      (void)std::filesystem::remove("device1Init.complete");
      (void)std::filesystem::remove("blockDevice1Init");
      (void)std::filesystem::remove("produceDevice2InitError");
    }
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  // Test that the script is actually executed
  BOOST_FIXTURE_TEST_CASE(TestExecution, Fixture) {
    testFacility.runApplication();

    CHECK_TIMEOUT(testFacility.readScalar<int32_t>("/Devices/Dummy0/status") == 0, 10000);
    // As soon as the device reports ready, the init script effect must be visible. This is just a smoke test that the
    // ScriptedInitialisationHandler actually executes the specified script. It does not check the initialisation
    // handler spec that the call is happening at the right time in the recovery process, which is tested separately.
    BOOST_CHECK(std::filesystem::exists("device1Init.complete"));
  }

  /********************************************************************************************************************/
  // Test that the message is published to the control system. Lines appear in the correct order.
  BOOST_FIXTURE_TEST_CASE(TestMessage, Fixture) {
    testFacility.runApplication();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/initScriptOutput");

    // Only test texts snippets and their order. There might be other stray output in the stuff collected from the
    // command line, so don't test for exact string equality.
    constexpr auto firstSnippet = "starting device1 init";
    constexpr auto secondSnippet = "device1 init complete";

    // Wait for the second message snippet.
    size_t secondSnippetPos;
    CHECK_TIMEOUT((initMessage.readLatest(),
                      (secondSnippetPos = std::string(initMessage).find(secondSnippet)) != std::string::npos),
        10000);
    // The first snippet is also there, and before the second snippet.
    BOOST_TEST(std::string(initMessage).find(firstSnippet) < secondSnippetPos);
    std::cout << std::string(initMessage) << std::endl;

    BOOST_CHECK(std::filesystem::exists("device1Init.complete"));
  }

  /********************************************************************************************************************/
  // A success message is appended after the script output.
  BOOST_FIXTURE_TEST_CASE(TestSuccess, Fixture) {
    testFacility.runApplication();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/initScriptOutput");

    constexpr auto scriptOutputSnippet = "device1 init complete";
    constexpr auto successSnippet = "SUCCESS"; // We intentionally do not test the exact line but only a snipped to be
                                               // less sensitive to message refactoring.

    size_t successSnippetPos;
    CHECK_TIMEOUT((initMessage.readLatest(),
                      (successSnippetPos = std::string(initMessage).find(successSnippet)) != std::string::npos),
        10000);
    // Tests that the script snippet is there before the success message.
    BOOST_TEST(std::string(initMessage).find(scriptOutputSnippet) < successSnippetPos);
  }

  /********************************************************************************************************************/
  // The script output does not only occur at the end of the script. Lines printed by the script are already shown
  // while the script is still running.
  BOOST_FIXTURE_TEST_CASE(TestPartialOutput, Fixture) {
    // Don't let the script finish, so we can do a check without race condition and know the test is sensitive.
    std::ofstream blockFile;
    blockFile.open("blockDevice1Init", std::ios::out);

    testFacility.runApplication();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/initScriptOutput");

    constexpr auto firstSnippet = "starting device1 init";
    constexpr auto secondSnippet = "device1 init complete";

    // The first snippet is in the message, the second not yet. The script has not reached the complete step yet.
    CHECK_TIMEOUT((initMessage.readLatest(), std::string(initMessage).find(firstSnippet) != std::string::npos), 10000);
    BOOST_TEST(std::string(initMessage).find(secondSnippet) == std::string::npos);
    BOOST_CHECK(!std::filesystem::exists("device1Init.complete"));

    // let the script finish
    (void)std::filesystem::remove("blockDevice1Init");

    // Wait for the second message snippet.
    size_t secondSnippetPos;
    CHECK_TIMEOUT((initMessage.readLatest(),
                      (secondSnippetPos = std::string(initMessage).find(secondSnippet)) != std::string::npos),
        10000);
    // The first snippet is still in there, and before the second snippet.
    BOOST_TEST(std::string(initMessage).find(firstSnippet) < secondSnippetPos);

    BOOST_CHECK(std::filesystem::exists("device1Init.complete"));
  }

  /********************************************************************************************************************/
  // Two differennt scripts on the same device have different outputs.
  BOOST_FIXTURE_TEST_CASE(TestTwoScripts, Fixture) {
    testFacility.runApplication();

    constexpr auto script1Snippet = "device1 init complete";
    constexpr auto script2Snippet = "just a second script";
    constexpr auto successSnippet = "SUCCESS";

    // wait for the success snippet in message1. It must contain the first, but not the second snippet.
    auto initMessage1 = testFacility.getScalar<std::string>("/Devices/Dummy0/initScriptOutput");
    CHECK_TIMEOUT(
        (initMessage1.readLatest(), std::string(initMessage1).find(successSnippet) != std::string::npos), 10000);
    BOOST_TEST(std::string(initMessage1).find(script1Snippet) != std::string::npos);
    BOOST_TEST(std::string(initMessage1).find(script2Snippet) == std::string::npos);

    // same for snippets in message2.
    auto initMessage2 = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");
    CHECK_TIMEOUT(
        (initMessage2.readLatest(), std::string(initMessage2).find(successSnippet) != std::string::npos), 10000);
    BOOST_TEST(std::string(initMessage2).find(script1Snippet) == std::string::npos);
    BOOST_TEST(std::string(initMessage2).find(script2Snippet) != std::string::npos);
  }

  /********************************************************************************************************************/
  // The error message is renewed with each run of the script, so changing error messages can be seen.
  BOOST_FIXTURE_TEST_CASE(TestErrorUpdate, Fixture) {
    std::ofstream produceErrorFile; // If the file exists, the script produces an error
    produceErrorFile.open("produceDevice2InitError", std::ios::out);
    produceErrorFile << 0 << std::flush;

    testFacility.runApplication();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");

    // let the script run three times, check that always the output of the last run is visible in the control system
    for(int i = 0; i < 3; ++i) {
      produceErrorFile.seekp(0);
      produceErrorFile << i << std::flush;
      std::string expectedSnippet = "Simulating error in second script: " + std::to_string(i);
      CHECK_TIMEOUT(
          (initMessage.readLatest(), std::string(initMessage).find(expectedSnippet) != std::string::npos), 20000);
      // renewed means the old message is replaced
      if(i == 1) {
        BOOST_TEST(std::string(initMessage).find("script: 0") == std::string::npos);
      }
      if(i == 2) {
        BOOST_TEST(std::string(initMessage).find("script: 1") == std::string::npos);
      }
    }

    // recovery
    (void)std::filesystem::remove("produceDevice2InitError");
    CHECK_TIMEOUT(testFacility.readScalar<int32_t>("/Devices/Dummy0/status") == 0, 10000);
  }

  /********************************************************************************************************************/
  // A failure message is appended if the script fails.
  BOOST_FIXTURE_TEST_CASE(TestFailureMessage, Fixture) {
    std::ofstream produceErrorFile; // If the file exists, the script produces an error
    produceErrorFile.open("produceDevice2InitError", std::ios::out);

    testFacility.runApplication();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");

    std::string scriptSnippet = "Simulating error in second script:";
    std::string failSnippet = "FAILED";

    size_t failSnippedPos;
    CHECK_TIMEOUT(
        (initMessage.readLatest(), (failSnippedPos = std::string(initMessage).find(failSnippet)) != std::string::npos),
        20000);
    BOOST_TEST(std::string(initMessage).find(scriptSnippet) < failSnippedPos);

    // recovery
    (void)std::filesystem::remove("produceDevice2InitError");
    CHECK_TIMEOUT(testFacility.readScalar<int32_t>("/Devices/Dummy0/status") == 0, 10000);
  }

  /********************************************************************************************************************/
  // A successful message replaces the previous failure message.
  BOOST_FIXTURE_TEST_CASE(TestSuccessAfterFailure, Fixture) {
    std::ofstream produceErrorFile; // If the file exists, the script produces an error
    produceErrorFile.open("produceDevice2InitError", std::ios::out);

    testFacility.runApplication();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");
    std::string failSnippet = "FAILED";
    std::string successSnippet = "SUCCESS";

    // wait for fail message to appear
    CHECK_TIMEOUT((initMessage.readLatest(), std::string(initMessage).find(failSnippet) != std::string::npos), 20000);

    // recovery
    (void)std::filesystem::remove("produceDevice2InitError");

    // Wait for success message to appear
    CHECK_TIMEOUT(
        (initMessage.readLatest(), std::string(initMessage).find(successSnippet) != std::string::npos), 20000);

    // The actual test: The failure message is not part of the message any more
    BOOST_TEST(std::string(initMessage).find(failSnippet) == std::string::npos);
  }

  /********************************************************************************************************************/
  // There is an error grace period which limits the retry rate.
  BOOST_FIXTURE_TEST_CASE(TestErrorGracePeriod, Fixture) {
    std::ofstream produceErrorFile; // If the file exists, the script produces an error
    produceErrorFile.open("produceDevice2InitError", std::ios::out);
    produceErrorFile << 1 << std::flush;

    testFacility.runApplication();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");

    auto startTime = std::chrono::steady_clock::now();
    // Let the script run three times, and wait for according error message to show. This is not the
    // actual test here, just a pre-conditon for measuring the time.
    for(int i = 0; i < 3; ++i) {
      produceErrorFile.seekp(0);
      produceErrorFile << i << std::flush;
      std::string expectedSnippet = "Simulating error in second script: " + std::to_string(i);
      CHECK_TIMEOUT(
          (initMessage.readLatest(), std::string(initMessage).find(expectedSnippet) != std::string::npos), 20000);
    }

    // recovery
    (void)std::filesystem::remove("produceDevice2InitError");
    CHECK_TIMEOUT(testFacility.readScalar<int32_t>("/Devices/Dummy0/status") == 0, 10000);

    // The actual test: At least three failure grace periods have passed.
    auto stopTime = std::chrono::steady_clock::now();
    BOOST_CHECK(std::chrono::duration_cast<std::chrono::seconds>(stopTime - startTime).count() >= 3);
  }
  /********************************************************************************************************************/

} // namespace Tests::testScriptedInitialisationHandler