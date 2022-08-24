#define BOOST_TEST_MODULE testExceptionHandling
#include "Application.h"
#include "check_timeout.h"
#include "ControlSystemModule.h"
#include "DeviceModule.h"
#include "ScriptedInitialisationHandler.h"
#include "TestFacility.h"

#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>

using namespace ChimeraTK;

struct DMapSetter {
  DMapSetter() { setDMapFilePath("test.dmap"); }
};

struct TestApp : public Application {
  using Application::Application;
  ~TestApp() { shutdown(); }

  ConnectingDeviceModule dev1{this, "Dummy0",
      "/MyModule/actuator"}; // pick one of the writable variables to AC knows that data type for the trigger

  // default name for the output variable (initScriptOutput)
  ScriptedInitHandler initHandler1{
      this, "InitHander1", "description", "./deviceInitScript1.bash", dev1.getDeviceModule()};
  // change the name of the output variable in case a second script is needed. Shorten the error grace time to 1 second
  ScriptedInitHandler initHandler2{this, "InitHander2", "description", "./deviceInitScript2.bash",
      dev1.getDeviceModule(), "secondInitScriptOutput", 1};
};

struct Fixture {
  DMapSetter dmapSetter;
  TestApp testApp{"ScriptedInitApp"};
  TestFacility testFacility{false};
};

BOOST_FIXTURE_TEST_CASE(testSuccess, Fixture) {
  (void)std::filesystem::remove("device1Init.success");
  (void)std::filesystem::remove("continueDevice1Init");
  (void)std::filesystem::remove("produceDevice1InitError");
  testFacility.runApplication();
  // testApp.dumpConnections();
  auto initMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/initScriptOutput");

  initMessage.read();
  std::string referenceString; // currently emtpy
  BOOST_CHECK_EQUAL(static_cast<std::string>(initMessage), referenceString);

  initMessage.read();
  referenceString += "starting device1 init\n";
  BOOST_CHECK_EQUAL(static_cast<std::string>(initMessage), referenceString);

  // no more messages, script waiting for continue file
  BOOST_CHECK(initMessage.readLatest() == false);

  // let the script finish
  std::ofstream continueFile;
  continueFile.open("continueDevice1Init", std::ios::out);

  initMessage.read();
  referenceString += "device1 init successful\n";
  BOOST_CHECK_EQUAL(static_cast<std::string>(initMessage), referenceString);

  initMessage.read();
  referenceString += "Dummy0 initialisation SUCCESS!";
  BOOST_CHECK_EQUAL(static_cast<std::string>(initMessage), referenceString);

  BOOST_CHECK(std::filesystem::exists("device1Init.success"));

  auto secondInitMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");
  secondInitMessage.read(); // empty
  secondInitMessage.read(); // script message
  secondInitMessage.read(); // success message
  BOOST_CHECK_EQUAL(
      static_cast<std::string>(secondInitMessage), "just a second script\nDummy0 initialisation SUCCESS!");

  // cleanup
  (void)std::filesystem::remove("device1Init.success");
  (void)std::filesystem::remove("continueDevice1Init");
}

BOOST_FIXTURE_TEST_CASE(testError, Fixture) {
  std::ofstream produceErrorFile; // If the file exists, the script produces an error
  produceErrorFile.open("produceDevice2InitError", std::ios::out);

  // let script1 finish
  std::ofstream continueFile;
  continueFile.open("continueDevice1Init", std::ios::out);

  testFacility.runApplication();

  // testApp.dumpConnections();
  auto secondInitMessage = testFacility.getScalar<std::string>("/Devices/Dummy0/secondInitScriptOutput");

  // let the script run three times. Afterwards manually check that there is only one printout on the console
  auto startTime = std::chrono::steady_clock::now();
  for(int i = 0; i < 3; ++i) {
    secondInitMessage.read();
    std::string referenceString; // currently emtpy
    BOOST_CHECK_EQUAL(static_cast<std::string>(secondInitMessage), referenceString);

    secondInitMessage.read();
    referenceString += "Simulating error in second script\n";
    BOOST_CHECK_EQUAL(static_cast<std::string>(secondInitMessage), referenceString);

    secondInitMessage.read();
    referenceString += "!!! Dummy0 initialisation FAILED!";
    BOOST_CHECK_EQUAL(static_cast<std::string>(secondInitMessage), referenceString);
  }

  (void)std::filesystem::remove("produceDevice2InitError");

  // recovery
  secondInitMessage.read(); // empty
  secondInitMessage.read(); // script message
  secondInitMessage.read(); // success message
  BOOST_CHECK_EQUAL(
      static_cast<std::string>(secondInitMessage), "just a second script\nDummy0 initialisation SUCCESS!");
  // at least three failure grace periods
  auto stopTime = std::chrono::steady_clock::now();
  BOOST_CHECK(std::chrono::duration_cast<std::chrono::seconds>(stopTime - startTime).count() >= 3);

  (void)std::filesystem::remove("device1Init.success");
  (void)std::filesystem::remove("continueDevice1Init");
}
