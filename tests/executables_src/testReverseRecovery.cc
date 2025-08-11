// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ScalarAccessor.h"

#include <ChimeraTK/DeviceBackend.h>
#include <ChimeraTK/ReadAnyGroup.h>
#include <ChimeraTK/SharedDummyBackend.h>
#include <ChimeraTK/TransferElement.h>
#include <ChimeraTK/VoidRegisterAccessor.h>

#include <boost/smart_ptr/shared_ptr.hpp>

#define BOOST_TEST_MODULE reverseRecoveryTest

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "Logger.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>

#include <boost/test/included/unit_test.hpp>

#include <cstdint>

namespace ctk = ChimeraTK;

struct ExternalMainLoopModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  std::function<void()> doMainLoop;

  void mainLoop() final { doMainLoop(); }
};
struct TestApplication : ctk::Application {
  TestApplication() : ctk::Application("tagTestApplication") { debugMakeConnections(); }
  ~TestApplication() override { shutdown(); }

  ExternalMainLoopModule mod{this, "Module", ""};
};

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDirectThreadedFanOutWithReturn) {
  std::cout << "testDirectThreadedFanOutWithReturn" << std::endl;

  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  TestApplication app;
  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/readWrite", 4);
  dev.write<int32_t>("/writeOnlyRB.DUMMY_WRITEABLE", 8);
  dev.write<int32_t>("/secondReadWrite", 16);

  // Set initial values for the variables
  test.setScalarDefault<int32_t>("/taggedReadWrite", 12);
  test.setScalarDefault<int32_t>("/taggedWriteOnly", 24);
  test.setScalarDefault<int32_t>("/untagged", 36);

  test.runApplication();

  // Wait for the device to become ready
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  up.wait(false);

  auto taggedReadWriteCs = test.getScalar<int32_t>("/taggedReadWrite");
  auto taggedWriteOnlyCs = test.getScalar<int32_t>("/taggedWriteOnly");
  auto untagged = test.getScalar<int32_t>("/untagged");

  // Check that the values are still on the values we have written explicitly
  // into the device, and not the initial values we configured above
  BOOST_TEST(dev.read<int32_t>("/readWrite") == 4);
  BOOST_TEST(dev.read<int32_t>("/writeOnlyRB") == 8);

  // Check that instead those values have been propagated to the CS (where applicable)
  CHECK_EQUAL_TIMEOUT((taggedReadWriteCs.readLatest(), int(taggedReadWriteCs)), 4, 2000);

  // The untagged register should have received the initial value from the CS
  BOOST_TEST(dev.read<int32_t>("/secondReadWrite") == 36);

  // Just do normal operations
  taggedReadWriteCs.setAndWrite(48);
  taggedWriteOnlyCs.setAndWrite(96);
  untagged.setAndWrite(128);

  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 48, 2000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/writeOnlyRB"), 96, 2000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/secondReadWrite"), 128, 2000);

  dev.write<int32_t>("/readWrite", 3);
  dev.write<int32_t>("/writeOnlyRB.DUMMY_WRITEABLE", 7);
  dev.write<int32_t>("/secondReadWrite", 15);
  devModule.reportException("Trigger device recovery");

  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  // The two tagged registers should keep their values, the untagged register should receive the value written before
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 3, 1000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/writeOnlyRB"), 7, 1000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/secondReadWrite"), 128, 1000);

  // The read-write register should have propagated its value to the CS
  CHECK_EQUAL_TIMEOUT((taggedReadWriteCs.readLatest(), int32_t(taggedReadWriteCs)), 3, 2000);

  app.shutdown();
}

/**********************************************************************************************************************/

// Create a ThreadedFanOutWithReturn and check that we can use the
// just the recovery value as an input
BOOST_AUTO_TEST_CASE(testThreadedFanOutWithReturnOnlyRecoverValue) {
  std::cout << "testThreadedFanOutWithReturnOnlyRecoverValue" << std::endl;
  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/readWrite", 4);

  TestApplication app;
  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  ctk::ScalarPushInput<int32_t> deviceInput{&app.mod, "/taggedReadWrite", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);

  // Set initial values for the variables
  test.setScalarDefault<int32_t>("/taggedReadWrite", 12);

  test.runApplication();
  up.wait(false);

  // Check that the device did not receive the initial value in this setup
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 4, 1000);

  // Check that the input is having the value from the device
  CHECK_EQUAL_TIMEOUT(deviceInput, 4, 1000);

  dev.write<int32_t>("/readWrite", 8);
  devModule.reportException("Trigger device recovery");

  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  deviceInput.read();
  CHECK_EQUAL_TIMEOUT(int32_t(deviceInput), 8, 1000);
  app.shutdown();
}

/**********************************************************************************************************************/

// Force the connection maker to create a direct connection with constant
// feeder
BOOST_AUTO_TEST_CASE(testConstantFeederInversion) {
  std::cout << "testConstantFeederInversion" << std::endl;

  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/readWrite", 4);

  TestApplication app;
  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  ctk::ScalarPushInput<int32_t> deviceInput{&app.mod, "/taggedReadWrite", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);

  app.optimiseUnmappedVariables({"/taggedReadWrite"});
  test.runApplication();
  up.wait(false);

  CHECK_EQUAL_TIMEOUT(int32_t(deviceInput), 4, 1000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 4, 1000);

  devModule.reportException("Trigger device recovery");

  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  deviceInput.read();
  CHECK_EQUAL_TIMEOUT(int32_t(deviceInput), 4, 1000);
  app.shutdown();
}

/**********************************************************************************************************************/

// Have an application module that has an explicit accessor requesting reverse recovery
BOOST_AUTO_TEST_CASE(testFeedingFanOutWithExplicitAccessor) {
  std::cout << "testFeedingFanOutWithExplicitAccessor" << std::endl;

  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/readWrite", 4);

  TestApplication app;
  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  ctk::ScalarOutputReverseRecovery<int32_t> deviceInput{&app.mod, "/taggedReadWrite", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);

  test.runApplication();
  up.wait(false);

  CHECK_EQUAL_TIMEOUT((int32_t(deviceInput)), 4, 1000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 4, 1000);

  // Check that we can still write down to the device properly
  deviceInput.setAndWrite(44);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 44, 1000);

  // Manipulate the device so we can check that the value is propagated
  // from the device to the application, as expected, after the device recovers
  dev.write<int32_t>("/readWrite", 111);

  devModule.reportException("Trigger device recovery");

  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  deviceInput.read();
  CHECK_EQUAL_TIMEOUT(int32_t(deviceInput), 111, 1000);
  app.shutdown();
}

/**********************************************************************************************************************/

// Have an application module that has an explicit accessor requesting reverse recovery
BOOST_AUTO_TEST_CASE(testFanOutWithExplicitAccessor02) {
  std::cout << "testFanOutWithExplicitAccessor02" << std::endl;
  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/readWrite", 4);

  TestApplication app;
  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  ctk::ScalarOutputReverseRecovery<int32_t> deviceInput{&app.mod, "/taggedReadWrite", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);

  app.optimiseUnmappedVariables({"/taggedReadWrite"});
  test.runApplication();
  up.wait(false);

  CHECK_EQUAL_TIMEOUT((int32_t(deviceInput)), 4, 1000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 4, 1000);

  // Check that we can still write down to the device properly
  deviceInput.setAndWrite(44);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 44, 1000);

  // Manipulate the device so we can check that the value is propagated
  // from the device to the application, as expected, after the device recovers
  dev.write<int32_t>("/readWrite", 111);

  devModule.reportException("Trigger device recovery");

  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  deviceInput.read();
  CHECK_EQUAL_TIMEOUT(int32_t(deviceInput), 111, 1000);
  app.shutdown();
}

// Have an application module that has an explicit accessor requesting reverse recovery
BOOST_AUTO_TEST_CASE(testFanOutWithExplicitAccessor03) {
  std::cout << "testFanOutWithExplicitAccessor03" << std::endl;
  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/readWrite", 4);

  TestApplication app;
  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  ctk::ScalarPushInput<int32_t> deviceInput{&app.mod, "/taggedReadWrite", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);

  app.optimiseUnmappedVariables({"/taggedReadWrite"});
  test.runApplication();
  up.wait(false);

  CHECK_EQUAL_TIMEOUT((int32_t(deviceInput)), 4, 1000);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 4, 1000);

  // Check that we can still write down to the device properly
  deviceInput.setAndWrite(44);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/readWrite"), 44, 1000);

  // Manipulate the device so we can check that the value is propagated
  // from the device to the application, as expected, after the device recovers
  dev.write<int32_t>("/readWrite", 111);

  devModule.reportException("Trigger device recovery");

  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  deviceInput.read();
  CHECK_EQUAL_TIMEOUT(int32_t(deviceInput), 111, 1000);
  app.shutdown();
}

/**********************************************************************************************************************/

// Request that we do the reverse recovery from an untagged device register by using the ReverseRecovery accessor
BOOST_AUTO_TEST_CASE(testReverseRecoveryFromApp) {
  std::cout << "testReverseRecoveryFromApp" << std::endl;

  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/secondReadWrite", 815);

  TestApplication app;

  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  ctk::ScalarOutputReverseRecovery<int32_t> deviceInput{&app.mod, "/untagged", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);
  test.setScalarDefault("/untagged", 4711);

  test.runApplication();

  // Wait for the device to become ready
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  up.wait(false);
  auto untagged = test.getScalar<int32_t>("/untagged");

  BOOST_TEST(dev.read<int32_t>("/secondReadWrite") == 815);

  deviceInput.setAndWrite(128);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/secondReadWrite"), 128, 2000);

  dev.write<int32_t>("/secondReadWrite", 3);
  devModule.reportException("Trigger device recovery");

  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/secondReadWrite"), 3, 2000);

  app.shutdown();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testRecoveryFromAppDirect) {
  std::cout << "testReverseRecoveryFromAppDirect" << std::endl;

  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Device dev;
  dev.open("baseDevice");

  // Initialize the device with some values
  dev.write<int32_t>("/secondReadWrite", 815);

  TestApplication app;
  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  std::atomic<bool> up{false};

  ctk::ScalarOutputReverseRecovery<int32_t> deviceInput{&app.mod, "/untagged", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);
  test.setScalarDefault("/untagged", 4711);
  app.optimiseUnmappedVariables({"/untagged"});
  test.runApplication();

  // Wait for the device to become ready
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  up.wait(false);
  auto untagged = test.getScalar<int32_t>("/untagged");

  BOOST_TEST(dev.read<int32_t>("/secondReadWrite") == 815);

  deviceInput.setAndWrite(128);
  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/secondReadWrite"), 128, 2000);

  dev.write<int32_t>("/secondReadWrite", 3);
  devModule.reportException("Trigger device recovery");

  // Wait for ApplicationCore to recover
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 1, 1000);
  CHECK_EQUAL_TIMEOUT(test.readScalar<int32_t>("/Devices/taggedDevice/status"), 0, 1000);

  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/secondReadWrite"), 3, 2000);

  app.shutdown();
}

/**********************************************************************************************************************/

// Special case: Reverse recovery, but without any device
BOOST_AUTO_TEST_CASE(testReverseRecoveryFromCS) {
  std::cout << "testReverseRecoveryFromCS" << std::endl;
  TestApplication app;

  std::atomic<bool> up{false};

  ctk::ScalarOutputReverseRecovery<int32_t> csOutput{&app.mod, "/taggedReadWrite", "", ""};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);

  test.setScalarDefault("/taggedReadWrite", 4711);

  test.runApplication();
  up.wait(false);

  CHECK_EQUAL_TIMEOUT(csOutput, 4711, 2000);

  app.shutdown();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testReverseRecoveryWithAdditionalInput) {
  std::cout << "testReverseRecoveryWithAdditionalInput" << std::endl;

  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  TestApplication app;
  ExternalMainLoopModule mod2{&app, "Module2", ""};

  // One module with an ouptut that has reverse recovery, and another module that takes this as
  // input
  ctk::ScalarOutputReverseRecovery<uint32_t> out{&app.mod, "/Out/a", "", ""};
  ctk::ScalarPushInput<uint32_t> in{&mod2, "/Out/a", "", ""};

  std::atomic<bool> up{false};
  std::atomic<bool> up2{false};

  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  mod2.doMainLoop = [&]() {
    up2 = true;
    up2.notify_one();
  };

  ctk::TestFacility test(app, false);
  test.setScalarDefault<uint32_t>("/Out/a", 32);

  test.runApplication();
  up.wait(false);
  up2.wait(false);

  CHECK_EQUAL_TIMEOUT(out, 32, 2000);
  CHECK_EQUAL_TIMEOUT(in, 32, 2000);

  app.shutdown();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testReverseRecoveryPromotingDeviceWoToFeeder) {
  std::cout << "testReverseRecoveryPromotingDeviceWoToFeeder" << std::endl;

  // This test just checks that we can connect this network successfully
  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  TestApplication app;

  std::atomic<bool> up{false};

  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};

  ctk::Device dev;
  dev.open("baseDevice");
  dev.write<int32_t>("/writeOnlyRB.DUMMY_WRITEABLE", 8);

  // Nothing to do
  app.mod.doMainLoop = [&]() {};

  ctk::TestFacility test(app, false);
  app.optimiseUnmappedVariables({"/taggedWriteOnly"});

  test.runApplication();

  std::this_thread::sleep_for(std::chrono::seconds(1));

  BOOST_TEST(dev.read<int32_t>("/writeOnlyRB") == 8);
  app.shutdown();
}

BOOST_AUTO_TEST_CASE(testReverseRecoveryNetworkWoOptimized) {
  std::cout << "testReverseRecoveryNetworkWoOptimized" << std::endl;

  // This test just checks that we can connect this network successfully
  ctk::BackendFactory::getInstance().setDMapFilePath("testTagged.dmap");

  ctk::Logger::getInstance().setMinSeverity(ctk::Logger::Severity::debug);

  TestApplication app;

  std::atomic<bool> up{false};

  ctk::DeviceModule devModule{&app, "taggedDevice", "/trigger"};
  ctk::ScalarPushInput<int32_t> modIn{&app.mod, "/taggedWriteOnly", "", ""};

  ctk::Device dev;
  dev.open("baseDevice");
  dev.write<int32_t>("/writeOnlyRB.DUMMY_WRITEABLE", 8);

  // Nothing to do
  app.mod.doMainLoop = [&]() {
    up = true;
    up.notify_one();
  };

  ctk::TestFacility test(app, false);
  test.setScalarDefault<int32_t>("/taggedWriteOnly", 12);
  app.optimiseUnmappedVariables({"/taggedWriteOnly"});

  test.runApplication();
  up.wait(false);

  // Gets value from constant feeder
  CHECK_EQUAL_TIMEOUT(int(modIn), 0, 2000);

  CHECK_EQUAL_TIMEOUT(dev.read<int32_t>("/writeOnlyRB"), 8, 2000);
  app.shutdown();
}
