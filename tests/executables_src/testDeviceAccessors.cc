// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <future>

#define BOOST_TEST_MODULE testDeviceAccessors

#include "Application.h"
#include "ApplicationModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/NDRegisterAccessor.h>

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

namespace Tests::testDeviceAccessors {

  /*********************************************************************************************************************/

#define CHECK_TIMEOUT(condition, maxMilliseconds)                                                                      \
  {                                                                                                                    \
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();                                       \
    while(!(condition)) {                                                                                              \
      bool timeout_reached = (std::chrono::steady_clock::now() - t0) > std::chrono::milliseconds(maxMilliseconds);     \
      BOOST_CHECK(!timeout_reached);                                                                                   \
      if(timeout_reached) break;                                                                                       \
      usleep(1000);                                                                                                    \
    }                                                                                                                  \
  }

  /*********************************************************************************************************************/

  struct TestModule : public ctk::ApplicationModule {
    TestModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, tags) {}

    ctk::ScalarPollInput<int> consumingPoll{this, "consumingPoll", "MV/m", "Description"};

    ctk::ScalarPushInput<int> consumingPush{this, "consumingPush", "MV/m", "Description"};
    ctk::ScalarPushInput<int> consumingPush2{this, "consumingPush2", "MV/m", "Description"};

    ctk::ScalarOutput<int> feedingToDevice{this, "feedingToDevice", "MV/m", "Description"};

    void prepare() override {
      incrementDataFaultCounter(); // force data to be flagged as faulty
      writeAll();
      decrementDataFaultCounter(); // data validity depends on inputs
    }

    void mainLoop() override {}
  };

  /*********************************************************************************************************************/
  /* dummy application */

  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {}
    ~TestApplication() override { shutdown(); }

    TestModule testModule{this, "testModule", "The test module"};
    ctk::DeviceModule dev{this, "Dummy0", "/dummyTriggerForUnusedVariables"};
    ctk::DeviceModule dev2;

    // note: direct device-to-controlsystem connections are tested in testControlSystemAccessors!
  };

  /*********************************************************************************************************************/
  /* test feeding a scalar to a device */

  BOOST_AUTO_TEST_CASE(testFeedToDevice) {
    std::cout << "testFeedToDevice" << std::endl;

    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

    TestApplication app;

    app.testModule.feedingToDevice = ctk::ScalarOutput<int>{&app.testModule, "/MyModule/actuator", "MV/m", ""};

    ctk::TestFacility test{app};
    test.runApplication();
    ChimeraTK::Device dev;
    dev.open("Dummy0");
    auto regacc = dev.getScalarRegisterAccessor<int>("/MyModule/actuator");

    regacc = 0;
    app.testModule.feedingToDevice = 42;
    app.testModule.feedingToDevice.write();
    regacc.read();
    BOOST_CHECK(regacc == 42);
    app.testModule.feedingToDevice = 120;
    regacc.read();
    BOOST_CHECK(regacc == 42);
    app.testModule.feedingToDevice.write();
    regacc.read();
    BOOST_CHECK(regacc == 120);
  }

  /*********************************************************************************************************************/
  /* test feeding a scalar to two different device registers */

  BOOST_AUTO_TEST_CASE(testFeedToDeviceFanOut) {
    std::cout << "testFeedToDeviceFanOut" << std::endl;

    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

    TestApplication app;

    app.testModule.feedingToDevice = {&app.testModule, "/MyModule/actuator", "MV/m", ""};
    app.dev2 = {&app, "Dummy0wo"};

    app.getModel().writeGraphViz("testFeedToDeviceFanOut.dot");

    ctk::TestFacility test{app};
    test.runApplication();

    ChimeraTK::Device dev("Dummy0");
    ChimeraTK::Device dev2("Dummy0wo");

    auto regac = dev.getScalarRegisterAccessor<int>("/MyModule/actuator");
    auto regrb = dev2.getScalarRegisterAccessor<int>("/MyModule/actuator");

    regac = 0;
    regrb = 0;
    app.testModule.feedingToDevice = 42;
    app.testModule.feedingToDevice.write();
    regac.read();
    BOOST_CHECK(regac == 42);
    regrb.read();
    BOOST_CHECK(regrb == 42);
    app.testModule.feedingToDevice = 120;
    regac.read();
    BOOST_CHECK(regac == 42);
    regrb.read();
    BOOST_CHECK(regrb == 42);
    app.testModule.feedingToDevice.write();
    regac.read();
    BOOST_CHECK(regac == 120);
    regrb.read();
    BOOST_CHECK(regrb == 120);
  }

  /*********************************************************************************************************************/
  /* test consuming a scalar from a device */

  BOOST_AUTO_TEST_CASE(testConsumeFromDevice) {
    std::cout << "testConsumeFromDevice" << std::endl;

    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

    TestApplication app;

    app.testModule.consumingPoll = {&app.testModule, "/MyModule/readBack", "MV/m", ""};

    ctk::TestFacility test{app};

    // Set the default value through the CS. The actuator and readBack map to the same register in the map file
    // Not setting a default will overwrite whatever is put into the device before the TestFacility::runApplication()
    // So we feed the default for the register through the IV mechanism of TestFacility.
    test.setScalarDefault<int>("/MyModule/actuator", 1);
    test.runApplication();

    ChimeraTK::Device dev;
    dev.open("Dummy0");
    auto regacc = dev.getScalarRegisterAccessor<int>("/MyModule/readBack.DUMMY_WRITEABLE");

    BOOST_REQUIRE(app.testModule.hasReachedTestableMode());

    BOOST_CHECK(app.testModule.consumingPoll == 1);
    regacc = 42;
    regacc.write();
    BOOST_CHECK(app.testModule.consumingPoll == 1);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    regacc = 120;
    regacc.write();
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPoll == 120);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPoll == 120);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPoll == 120);
  }

  /*********************************************************************************************************************/
  /* test consuming a scalar from a device with a ConsumingFanOut (i.e. one
   * poll-type consumer and several push-type consumers). */

  BOOST_AUTO_TEST_CASE(testConsumingFanOut) {
    std::cout << "testConsumingFanOut" << std::endl;

    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

    TestApplication app;

    app.testModule.consumingPoll = {&app.testModule, "/MyModule/readBack", "MV/m", ""};
    app.testModule.consumingPush = {&app.testModule, "/MyModule/readBack", "MV/m", ""};
    app.testModule.consumingPush2 = {&app.testModule, "/MyModule/readBack", "MV/m", ""};

    ctk::TestFacility test{app};

    // Set the default value through the CS. The actuator and readBack map to the same register in the map file
    // Not setting a default will overwrite whatever is put into the device before the TestFacility::runApplication()
    // So we feed the default for the register through the IV mechanism of TestFacility.
    test.setScalarDefault<int>("/MyModule/actuator", 1);
    ChimeraTK::Device dev;
    dev.open("Dummy0");
    auto regacc = dev.getScalarRegisterAccessor<int>("/MyModule/readBack.DUMMY_WRITEABLE");
    test.runApplication();

    // single threaded test only, since read() does not block in this case
    BOOST_CHECK(app.testModule.consumingPoll == 1);
    BOOST_CHECK(app.testModule.consumingPush2 == 1);
    regacc = 42;
    regacc.write();

    BOOST_CHECK(app.testModule.consumingPoll == 1);
    BOOST_CHECK(app.testModule.consumingPush2 == 1);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush == 1);
    BOOST_CHECK(app.testModule.consumingPush2 == 1);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    BOOST_CHECK(app.testModule.consumingPush == 42);
    BOOST_CHECK(app.testModule.consumingPush2 == 42);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    BOOST_CHECK(app.testModule.consumingPush == 42);
    BOOST_CHECK(app.testModule.consumingPush2 == 42);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    BOOST_CHECK(app.testModule.consumingPush == 42);
    BOOST_CHECK(app.testModule.consumingPush2 == 42);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
    regacc = 120;
    regacc.write();
    BOOST_CHECK(app.testModule.consumingPoll == 42);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush == 42);
    BOOST_CHECK(app.testModule.consumingPush2 == 42);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPoll == 120);
    BOOST_CHECK(app.testModule.consumingPush == 120);
    BOOST_CHECK(app.testModule.consumingPush2 == 120);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPoll == 120);
    BOOST_CHECK(app.testModule.consumingPush == 120);
    BOOST_CHECK(app.testModule.consumingPush2 == 120);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
    app.testModule.consumingPoll.read();
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == true);
    BOOST_CHECK(app.testModule.consumingPoll == 120);
    BOOST_CHECK(app.testModule.consumingPush == 120);
    BOOST_CHECK(app.testModule.consumingPush2 == 120);
    BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
    BOOST_CHECK(app.testModule.consumingPush2.readNonBlocking() == false);
  }

  /*********************************************************************************************************************/
  /* Application for tests of DeviceModule */

  struct TestModule2 : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarOutput<int> actuator{this, "actuator", "MV/m", "Description"};
    ctk::ScalarPollInput<int> readback{this, "readBack", "MV/m", "Description"};

    void mainLoop() override {}
  };

  struct Deeper : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct Hierarchies : ctk::VariableGroup {
      using ctk::VariableGroup ::VariableGroup;
      struct Need : ctk::VariableGroup {
        using ctk::VariableGroup ::VariableGroup;
        ctk::ScalarPollInput<int> tests{this, "tests", "MV/m", "Description"};
      } need{this, "need", ""};
      ctk::ScalarOutput<int> also{this, "also", "MV/m", "Description", {"ALSO"}};
    } hierarchies{this, "hierarchies", ""};

    void mainLoop() override {}
  };

  struct Deeper2 : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;

    ctk::DeviceModule dev{this, "Dummy1", "/Deeper/hierarchies/trigger", nullptr, "/MyModule"};

    struct Hierarchies : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      struct Need : ctk::VariableGroup {
        using ctk::VariableGroup ::VariableGroup;
        ctk::ScalarPollInput<int> tests{this, "tests", "MV/m", "Description"};
      } need{this, "need", ""};
      ctk::ScalarOutput<int> also{this, "also", "MV/m", "Description", {"ALSO"}};
      ctk::ScalarOutput<int> trigger{this, "trigger", "MV/m", "Description", {"ALSO"}};

      void mainLoop() override {}
    } hierarchies{this, "hierarchies", ""};
  };

  struct TestApplication3 : public ctk::Application {
    TestApplication3() : Application("testSuite") {}
    ~TestApplication3() override { shutdown(); }

    TestModule2 testModule{this, "MyModule", "The test module"};
    Deeper2 deeper{this, "Deeper", ""};

    std::atomic<size_t> initHandlerCallCount{0};
    ctk::DeviceModule dev{this, "Dummy0", "", [this](ChimeraTK::Device&) { initHandlerCallCount++; }};
  };

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDeviceModuleExceptions) {
    std::cout << "testDeviceModuleExceptions" << std::endl;

    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");
    TestApplication3 app;

    // non-absolute trigger path
    BOOST_CHECK_THROW(
        ctk::DeviceModule(&app.deeper, "Dummy0", "unqualifiedName", nullptr, "/MyModule"), ctk::logic_error);
    BOOST_CHECK_THROW(
        ctk::DeviceModule(&app.deeper, "Dummy0", "relative/name", nullptr, "/MyModule"), ctk::logic_error);
    BOOST_CHECK_THROW(
        ctk::DeviceModule(&app.deeper, "Dummy0", "./also/relative", nullptr, "/MyModule"), ctk::logic_error);
    BOOST_CHECK_THROW(
        ctk::DeviceModule(&app.deeper, "Dummy0", "../another/relative/name", nullptr, "/MyModule"), ctk::logic_error);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDeviceModule) {
    std::cout << "testDeviceModule" << std::endl;

    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");
    TestApplication3 app;

    ctk::TestFacility test{app};
    test.runApplication();
    BOOST_CHECK_EQUAL(app.initHandlerCallCount, 1);

    ctk::Device dev;
    dev.open("Dummy0");
    auto actuator = dev.getScalarRegisterAccessor<int>("MyModule/actuator");
    auto& readback = actuator; // same address in map file
    auto tests = dev.getScalarRegisterAccessor<int>("Deeper/hierarchies/need/tests/DUMMY_WRITEABLE");
    auto also = dev.getScalarRegisterAccessor<int>("Deeper/hierarchies/also");

    app.testModule.actuator = 42;
    app.testModule.actuator.write();
    actuator.read();
    BOOST_CHECK_EQUAL(actuator, 42);

    app.testModule.actuator = 12;
    app.testModule.actuator.write();
    actuator.read();
    BOOST_CHECK_EQUAL(actuator, 12);

    readback = 120;
    readback.write();
    app.testModule.readback.read();
    BOOST_CHECK_EQUAL(app.testModule.readback, 120);

    readback = 66;
    readback.write();
    app.testModule.readback.read();
    BOOST_CHECK_EQUAL(app.testModule.readback, 66);

    tests = 120;
    tests.write();
    app.deeper.hierarchies.need.tests.read();
    BOOST_CHECK_EQUAL(app.deeper.hierarchies.need.tests, 120);

    tests = 66;
    tests.write();
    app.deeper.hierarchies.need.tests.read();
    BOOST_CHECK_EQUAL(app.deeper.hierarchies.need.tests, 66);

    app.deeper.hierarchies.also = 42;
    app.deeper.hierarchies.also.write();
    also.read();
    BOOST_CHECK_EQUAL(also, 42);

    app.deeper.hierarchies.also = 12;
    app.deeper.hierarchies.also.write();
    also.read();
    BOOST_CHECK_EQUAL(also, 12);

    // test the second DeviceModule with the trigger
    ctk::Device dev2;
    dev2.open("Dummy1");
    auto readback2 = dev2.getScalarRegisterAccessor<int>("/MyModule/readBack/DUMMY_WRITEABLE");
    readback2 = 543;
    readback2.write();

    app.deeper.hierarchies.trigger.write();
    test.stepApplication();
    BOOST_CHECK_EQUAL(test.readScalar<int>("/Deeper/readBack"), 543);

    // make sure init handler is not called somehow a second time
    BOOST_CHECK_EQUAL(app.initHandlerCallCount, 1);
  }

  /*********************************************************************************************************************/
  /* Application for tests of DeviceModule move constructor/assignment */

  struct TestApplication4 : public ctk::Application {
    TestApplication4() : Application("testSuite") {}
    ~TestApplication4() override { shutdown(); }

    ctk::DeviceModule dev{this, "Dummy1"};
    ctk::DeviceModule dev2{this, "Dummy0"};

    std::vector<ctk::DeviceModule> cdevs;

    TestModule2 m{this, "MyModule", ""};
    Deeper deeper{this, "Deeper", ""};
  };

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDeviceModuleMove) {
    std::cout << "testDeviceModuleMove" << std::endl;
    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

    TestApplication4 app;
    app.dev = std::move(app.dev2);           // test move-assign
    app.cdevs.push_back(std::move(app.dev)); // test move-construct

    app.getModel().writeGraphViz("testDeviceModuleMove.dot");

    ctk::TestFacility test{app};

    test.runApplication();
    ctk::Device dummy0("Dummy0");
    auto readBack = dummy0.getScalarRegisterAccessor<int>("MyModule/readBack/DUMMY_WRITEABLE");
    readBack = 432;
    readBack.write();
    app.m.readback.read();
    BOOST_CHECK_EQUAL(app.m.readback, 432);
  }
  /*********************************************************************************************************************/

} // namespace Tests::testDeviceAccessors