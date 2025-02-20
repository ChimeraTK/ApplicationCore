// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testPythonInitialisationHandler
#include "Application.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "PythonInitialisationHandler.h"
#include "TestFacility.h"

#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>

namespace Tests::testPythonInitialisationHandler {

  using namespace ChimeraTK;

  /********************************************************************************************************************/

  struct TestApp : public Application {
    using Application::Application;
    ~TestApp() override { shutdown(); }

    SetDMapFilePath dmap{"test.dmap"};

    DeviceModule dev1{this, "Dummy0",
        "/MyModule/actuator"}; // pick one of the writable variables to AC knows that data type for the trigger

    // default name for the output variable (initScriptOutput)
    PythonInitHandler initHandler1{this, "InitHander1", "description", "deviceInitScript1.py", dev1};
    // change the name of the output variable in case a second script is needed. Shorten the error grace time to 1 second
    PythonInitHandler initHandler2{
        this, "InitHander2", "description", "deviceInitScript2.py", dev1, "secondInitScriptOutput", 1};
  };

  /********************************************************************************************************************/

  struct Fixture {
    TestApp testApp{"PythonInitApp"};
    TestFacility testFacility{testApp, false};
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  BOOST_FIXTURE_TEST_CASE(testSuccess, Fixture) {
    (void)std::filesystem::remove("continuePythonDevice1Init");
    (void)std::filesystem::remove("producePythonDeviceInitError1");
    (void)std::filesystem::remove("producePythonDeviceInitError2");

    testFacility.runApplication();
    // testApp.dumpConnections();

    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/initScriptOutput");
    auto deviceStatus = testFacility.getScalar<int>("/Devices/Dummy0/status");

    initMessage.read();
    std::string referenceString; // currently emtpy
    BOOST_CHECK_EQUAL(static_cast<std::string>(initMessage), referenceString);

    initMessage.read();
    // coming from the script
    referenceString += "starting device1 init\n";
    referenceString += "device1 init successful\n";
    // coming from the handler
    referenceString += "Dummy0 initialisation SUCCESS!";
    BOOST_CHECK_EQUAL(static_cast<std::string>(initMessage), referenceString);

    auto secondInitMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");
    referenceString = "just a second script\nDummy0 initialisation SUCCESS!";
    CHECK_TIMEOUT((secondInitMessage.readLatest(), std::string(secondInitMessage) == referenceString), 20000);
    CHECK_TIMEOUT((deviceStatus.readLatest(), deviceStatus == 0), 500);
  }

  /********************************************************************************************************************/

  BOOST_FIXTURE_TEST_CASE(testException, Fixture) {
    std::ofstream produceErrorFile; // If the file exists, the script produces an error
    produceErrorFile.open("producePythonDeviceInitError1", std::ios::out);

    testFacility.runApplication();

    // testApp.dumpConnections();
    auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/initScriptOutput");
    auto deviceStatus = testFacility.getScalar<int>("/Devices/Dummy0/status");

    // let the script run three times, check that always the output of the last run is visible in the control system
    auto startTime = std::chrono::steady_clock::now();
    for(int i = 0; i < 3; ++i) {
      produceErrorFile.seekp(0);
      produceErrorFile << i << std::flush;
      std::string referenceString = "starting device1 init\n"
                                    "error initialising device: " +
          std::to_string(i) +
          "\n"
          "!!! Dummy0 initialisation FAILED!";
      CHECK_TIMEOUT((initMessage.readLatest(), std::string(initMessage) == referenceString), 20000);
      BOOST_TEST(std::string(initMessage) == referenceString);
      BOOST_CHECK((deviceStatus.readLatest(), deviceStatus == 1));
      std::cout << "device status " << deviceStatus << std::endl;
    }

    (void)std::filesystem::remove("producePythonDevice1InitError");

    // recovery
    std::string referenceString = "starting device1 init\ndevice1 init successful\nDummy0 initialisation SUCCESS!";
    CHECK_TIMEOUT((initMessage.readLatest(), std::string(initMessage)) == referenceString, 20000);
    // at least three failure grace periods
    auto stopTime = std::chrono::steady_clock::now();
    BOOST_CHECK(std::chrono::duration_cast<std::chrono::seconds>(stopTime - startTime).count() >= 3);
    CHECK_TIMEOUT((deviceStatus.readLatest(), deviceStatus == 0), 500);
  }

  /********************************************************************************************************************/

} // namespace Tests::testPythonInitialisationHandler