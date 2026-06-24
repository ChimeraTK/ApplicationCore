// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ScalarAccessor.h"

#include <ChimeraTK/ExceptionDummyBackend.h>
#include <ChimeraTK/SharedDummyBackend.h>

#define BOOST_TEST_MODULE noInitialValueTest

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>

#include <boost/test/included/unit_test.hpp>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
namespace ctk = ChimeraTK;

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(TestNoInitialValueTagChecks) {
  std::cout << "TestNoInitialValueTagChecks" << std::endl;

  struct TestModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;
    ctk::ScalarPushInputNoInitialValue<int32_t> pushNoIv{this, "pushNoIv", "", ""};
    ctk::ScalarPollInputNoInitialValue<int32_t> pollNoIv{this, "pollNoIv", "", ""};
    ctk::ScalarPushInput<int32_t> normalPush{this, "normalPush", "", ""};
    ctk::ScalarPollInput<int32_t> normalPoll{this, "normalPoll", "", ""};
    void mainLoop() override {}
  };

  struct TagCheckApp : ctk::Application {
    TagCheckApp() : ctk::Application("tagCheckApp") {}
    ~TagCheckApp() override { shutdown(); }
    TestModule mod{this, "Module", ""};
  } app;

  auto accessorList = app.mod.getAccessorListRecursive();

  for(auto& var : accessorList) {
    if(var.getName() == "pushNoIv" || var.getName() == "pollNoIv") {
      BOOST_TEST(var.getTags().contains(ctk::noInitialValueReadTag));
    }
    if(var.getName() == "normalPush" || var.getName() == "normalPoll") {
      BOOST_TEST(!var.getTags().contains(ctk::noInitialValueReadTag));
    }
  }
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(TestNoInitialValueModuleEntersMainLoop) {
  std::cout << "TestNoInitialValueModuleEntersMainLoop" << std::endl;

  struct TestModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPushInputNoInitialValue<int32_t> pushNoIv{this, "pushNoIv", "", ""};
    ctk::ScalarPollInputNoInitialValue<int32_t> pollNoIv{this, "pollNoIv", "", ""};
    ctk::ScalarPushInput<int32_t> normalPush{this, "normalPush", "", ""};
    ctk::ScalarPollInput<int32_t> normalPoll{this, "normalPoll", "", ""};

    std::atomic<bool> mainLoopEntered{false};

    void mainLoop() override {
      mainLoopEntered = true;
      mainLoopEntered.notify_one();
    }
  };

  struct NoInitApp : ctk::Application {
    NoInitApp() : ctk::Application("noInitApp") {}
    ~NoInitApp() override { shutdown(); }
    TestModule mod{this, "Module", ""};
  } app;

  ctk::TestFacility test(app, false);

  test.setScalarDefault<int32_t>("/Module/pushNoIv", 42);
  test.setScalarDefault<int32_t>("/Module/pollNoIv", 120);
  test.setScalarDefault<int32_t>("/Module/normalPush", 666);
  test.setScalarDefault<int32_t>("/Module/normalPoll", 777);

  test.runApplication();

  CHECK_EQUAL_TIMEOUT(app.mod.mainLoopEntered, true, 2000);

  BOOST_TEST(app.mod.pushNoIv.getVersionNumber() == ctk::VersionNumber{nullptr});
  BOOST_TEST(app.mod.pollNoIv.getVersionNumber() == ctk::VersionNumber{nullptr});
  BOOST_TEST(app.mod.normalPush.getVersionNumber() != ctk::VersionNumber{nullptr});
  BOOST_TEST(app.mod.normalPoll.getVersionNumber() != ctk::VersionNumber{nullptr});

  BOOST_TEST(app.mod.pushNoIv == 0);
  BOOST_TEST(app.mod.pollNoIv == 0);
  BOOST_TEST(app.mod.normalPush == 666);
  BOOST_TEST(app.mod.normalPoll == 777);

  app.mod.pushNoIv.read();
  app.mod.pollNoIv.read();

  BOOST_TEST(app.mod.pushNoIv == 42);
  BOOST_TEST(app.mod.pollNoIv == 120);

  app.shutdown();
}

/**********************************************************************************************************************/

static constexpr const char* testMap = "testNoInitVal.map";
static constexpr const char* testDmap = "testNoInitVal.dmap";
static constexpr const char* backendCdd = "(ExceptionDummy:1?map=testNoInitVal.map)";

static void setupDeviceMap() {
  std::ofstream dmapFile(testDmap);
  dmapFile << "testDevice " << backendCdd << std::endl;

  std::ofstream mapFile(testMap);
  mapFile << "Module.noInit      0x00000001    0x00000000    0x00000004  1 32 0 1 RO" << std::endl;
  mapFile << "Module.noReco      0x00000001    0x00000004    0x00000004  1 32 0 1 RW" << std::endl;
}

static void cleanupDeviceMap() {
  std::remove(testDmap);
  std::remove(testMap);
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(TestNoInitialValueOnlyEntersWithBrokenDevice) {
  std::cout << "TestNoInitialValueOnlyEntersWithBrokenDevice" << std::endl;

  setupDeviceMap();
  ctk::BackendFactory::getInstance().setDMapFilePath(testDmap);

  // Place backend into error state BEFORE the application starts
  auto backend =
      boost::dynamic_pointer_cast<ctk::ExceptionDummy>(ctk::BackendFactory::getInstance().createBackend("testDevice"));
  BOOST_REQUIRE(backend);
  backend->throwExceptionOpen = true;
  struct NoInitOnlyModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPollInputNoInitialValue<int32_t> noInit{this, "/Module/noInit", "", ""};

    // Reverse Recovery paired with noInitialValueRead effectively disables recovery entirely. Writes only go through
    // when the device is online during the write operation and are otherwise discarded.
    ctk::ScalarOutputReverseRecovery<int32_t> noReco{
        this, "/Module/noReco", "", "", {ChimeraTK::noInitialValueReadTag}};

    std::atomic<bool> mainLoopEntered{false};

    void mainLoop() override {
      // Now read the NoInitialValue input — this should not block even though the device is broken, because we skip
      // DeviceManager::waitForInitialValues() in ExceptionHandlingDecorator::doPreRead()
      noInit.read();

      noReco.write();

      mainLoopEntered = true;
      mainLoopEntered.notify_one();
    }
  };

  struct OnlyNoInitApp : ctk::Application {
    OnlyNoInitApp() : ctk::Application("onlyNoInitApp") {}
    ~OnlyNoInitApp() override { shutdown(); }
    ctk::DeviceModule devMod{this, "testDevice", "/fakeTrigger"};
    NoInitOnlyModule mod{this, "Module", ""};
  } app;

  ctk::TestFacility test(app, false);
  test.runApplication();

  // Module with only NoInitialValue inputs should enter mainLoop
  // despite broken device, because all its inputs skip the initial read
  // AND the ExceptionHandlingDecorator::doPreRead skips waitForInitialValues()
  CHECK_EQUAL_TIMEOUT(app.mod.mainLoopEntered, true, 5000);

  app.shutdown();
  cleanupDeviceMap();
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(TestWithOptimiseUnmappedVariables) {
  std::cout << "TestWithOptimiseUnmappedVariables" << std::endl;

  setupDeviceMap();
  ctk::BackendFactory::getInstance().setDMapFilePath(testDmap);

  // Place backend into error state BEFORE the application starts
  auto backend =
      boost::dynamic_pointer_cast<ctk::ExceptionDummy>(ctk::BackendFactory::getInstance().createBackend("testDevice"));
  BOOST_REQUIRE(backend);
  backend->throwExceptionOpen = true;
  struct NoInitOnlyModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPollInputNoInitialValue<int32_t> noInit{this, "/Module/noInit", "", ""};

    // Reverse Recovery paired with noInitialValueRead effectively disables recovery entirely. Writes only go through
    // when the device is online during the write operation and are otherwise discarded.
    ctk::ScalarOutputReverseRecovery<int32_t> noReco{
        this, "/Module/noReco", "", "", {ChimeraTK::noInitialValueReadTag}};

    std::atomic<bool> mainLoopEntered{false};

    void mainLoop() override {
      // Now read the NoInitialValue input — this should not block even though the device is broken and the CS
      // connection has been optimised out (direct Device→App connection), because we skip
      // DeviceManager::waitForInitialValues() in ExceptionHandlingDecorator::doPreRead()
      noInit.read();

      noReco.write();

      mainLoopEntered = true;
      mainLoopEntered.notify_one();
    }
  };

  struct OnlyNoInitApp : ctk::Application {
    OnlyNoInitApp() : ctk::Application("onlyNoInitOptApp") {}
    ~OnlyNoInitApp() override { shutdown(); }
    ctk::DeviceModule devMod{this, "testDevice", "/fakeTrigger"};
    NoInitOnlyModule mod{this, "Module", ""};
  } app;

  ctk::TestFacility test(app, false);

  // Optimise out the control system connection, making it a direct Device→App connection (1:1).
  // This exercises makeDirectConnectionForFeederWithImplementation instead of makeFanOutConnectionForFeederWithImplementation.
  app.optimiseUnmappedVariables({"/Module/noInit", "/Module/noReco"});

  test.runApplication();

  // Module with only NoInitialValue inputs should enter mainLoop
  // despite broken device, because all its inputs skip the initial read
  // AND the ExceptionHandlingDecorator::doPreRead skips waitForInitialValues()
  CHECK_EQUAL_TIMEOUT(app.mod.mainLoopEntered, true, 5000);

  app.shutdown();
  cleanupDeviceMap();
}
