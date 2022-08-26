/*
 * testDeviceAccessors.cc
 *
 *  Created on: Jun 22, 2016
 *      Author: Martin Hierholzer
 */

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

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

// list of user types the accessors are tested with
typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double> test_types;

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
/* the ApplicationModule for the test is a template of the user type */

template<typename T>
struct TestModule : public ctk::ApplicationModule {
  TestModule(EntityOwner* owner, const std::string& name, const std::string& description,
      ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
      const std::unordered_set<std::string>& tags = {})
  : ApplicationModule(owner, name, description, hierarchyModifier, tags) {}

  ctk::ScalarPollInput<T> consumingPoll{this, "consumingPoll", "MV/m", "Description"};

  ctk::ScalarPushInput<T> consumingPush{this, "consumingPush", "MV/m", "Description"};
  ctk::ScalarPushInput<T> consumingPush2{this, "consumingPush2", "MV/m", "Description"};

  ctk::ScalarOutput<T> feedingToDevice{this, "feedingToDevice", "MV/m", "Description"};

  void prepare() override {
    incrementDataFaultCounter(); // force data to be flagged as faulty
    writeAll();
    decrementDataFaultCounter(); // data validity depends on inputs
  }

  void mainLoop() override {}
};

/*********************************************************************************************************************/
/* dummy application */

template<typename T>
struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  using Application::deviceMap;       // expose the device map for the tests
  using Application::makeConnections; // we call makeConnections() manually in
                                      // the tests to catch exceptions etc.
  using Application::networkList;     // expose network list to check merging
                                      // networks
  void defineConnections() {}         // the setup is done in the tests

  TestModule<T> testModule{this, "testModule", "The test module"};
  ctk::DeviceModule dev{this, "Dummy0"};

  // note: direct device-to-controlsystem connections are tested in testControlSystemAccessors!
};

/*********************************************************************************************************************/
/* test feeding a scalar to a device */

BOOST_AUTO_TEST_CASE_TEMPLATE(testFeedToDevice, T, test_types) {
  std::cout << "testFeedToDevice" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  app.testModule.feedingToDevice >> app.dev["MyModule"]("actuator");

  ctk::TestFacility test;
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

BOOST_AUTO_TEST_CASE_TEMPLATE(testFeedToDeviceFanOut, T, test_types) {
  std::cout << "testFeedToDeviceFanOut" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  app.testModule.feedingToDevice >> app.dev["MyModule"]("actuator") >> app.dev["Deeper"]["hierarchies"]("also");
  ctk::TestFacility test;
  test.runApplication();
  ChimeraTK::Device dev;
  dev.open("Dummy0");

  auto regac = dev.getScalarRegisterAccessor<int>("/MyModule/actuator");
  auto regrb = dev.getScalarRegisterAccessor<int>("/Deeper/hierarchies/also");

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

BOOST_AUTO_TEST_CASE_TEMPLATE(testConsumeFromDevice, T, test_types) {
  std::cout << "testConsumeFromDevice" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  // We intentionally use an r/w register here to use it as an input only. Just to test the case
  // (might only be written in initialication and only read in the server itself)
  app.dev("/MyModule/actuator") >> app.testModule.consumingPoll;
  ctk::TestFacility test;
  ChimeraTK::Device dev;
  dev.open("Dummy0");
  auto regacc = dev.getScalarRegisterAccessor<int>("/MyModule/actuator");

  regacc = 1; // write initial value which should be present in accessor after app start
  regacc.write();

  test.runApplication();

  // single theaded test only, since read() does not block in this case
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

BOOST_AUTO_TEST_CASE_TEMPLATE(testConsumingFanOut, T, test_types) {
  std::cout << "testConsumingFanOut" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  app.dev("/MyModule/actuator") >> app.testModule.consumingPoll >> app.testModule.consumingPush >>
      app.testModule.consumingPush2;
  ctk::TestFacility test;
  ChimeraTK::Device dev;
  dev.open("Dummy0");
  auto regacc = dev.getScalarRegisterAccessor<int>("/MyModule/actuator");

  regacc = 1; // write initial value which should be present in accessor after app start
  regacc.write();

  test.runApplication();

  // single theaded test only, since read() does not block in this case
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
/* test merged networks (optimisation done in
 * Application::optimiseConnections()) */

BOOST_AUTO_TEST_CASE_TEMPLATE(testMergedNetworks, T, test_types) {
  std::cout << "testMergedNetworks" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  // we abuse "feedingToDevice" as trigger here...
  app.dev("/MyModule/actuator")[app.testModule.feedingToDevice] >> app.testModule.consumingPush;
  app.dev("/MyModule/actuator")[app.testModule.feedingToDevice] >> app.testModule.consumingPush2;

  // check that we have two separate networks for both connections
  size_t nDeviceFeeders = 0;
  for(auto& net : app.networkList) {
    if(net.getFeedingNode().getType() == ctk::NodeType::Device) nDeviceFeeders++;
  }
  BOOST_CHECK_EQUAL(nDeviceFeeders, 2);

  // the optimisation to test takes place here
  ctk::TestFacility test;

  // check we are left with just one network fed by the device
  nDeviceFeeders = 0;
  for(auto& net : app.networkList) {
    if(net.getFeedingNode().getType() == ctk::NodeType::Device) nDeviceFeeders++;
  }
  BOOST_CHECK_EQUAL(nDeviceFeeders, 1);

  ChimeraTK::Device dev;
  dev.open("Dummy0");
  auto regacc = dev.getScalarRegisterAccessor<int>("/MyModule/actuator");
  regacc = 1;
  regacc.write();

  // run the application to see if everything still behaves as expected
  test.runApplication();

  // single theaded test only, since read() does not block in this case
  regacc = 42;
  regacc.write();
  BOOST_CHECK(app.testModule.consumingPush == 1);
  BOOST_CHECK(app.testModule.consumingPush2 == 1);
  app.testModule.feedingToDevice.write();
  app.testModule.consumingPush.read();
  app.testModule.consumingPush2.read();
  BOOST_CHECK(app.testModule.consumingPush == 42);
  BOOST_CHECK(app.testModule.consumingPush2 == 42);
  regacc = 120;
  regacc.write();
  BOOST_CHECK(app.testModule.consumingPush == 42);
  BOOST_CHECK(app.testModule.consumingPush2 == 42);
  app.testModule.feedingToDevice.write();
  app.testModule.consumingPush.read();
  app.testModule.consumingPush2.read();
  BOOST_CHECK(app.testModule.consumingPush == 120);
  BOOST_CHECK(app.testModule.consumingPush2 == 120);
}

/*********************************************************************************************************************/
/* test feeding a constant to a device register. */

BOOST_AUTO_TEST_CASE_TEMPLATE(testConstantToDevice, T, test_types) {
  std::cout << "testConstantToDevice" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  ctk::VariableNetworkNode::makeConstant<T>(true, 18) >> app.dev("/MyModule/actuator");
  ctk::TestFacility test;
  test.runApplication();

  ChimeraTK::Device dev;
  dev.open("Dummy0");

  CHECK_TIMEOUT(dev.read<T>("/MyModule/actuator") == 18, 10000);
}

/*********************************************************************************************************************/
/* test feeding a constant to a device register with a fan out. */

BOOST_AUTO_TEST_CASE_TEMPLATE(testConstantToDeviceFanOut, T, test_types) {
  std::cout << "testConstantToDeviceFanOut" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  ctk::VariableNetworkNode::makeConstant<T>(true, 20) >> app.dev("/MyModule/actuator") >>
      app.dev("/Deeper/hierarchies/also");
  ctk::TestFacility test;
  test.runApplication();

  ChimeraTK::Device dev;
  dev.open("Dummy0");

  CHECK_TIMEOUT(dev.read<T>("/MyModule/actuator") == 20, 10000);
  CHECK_TIMEOUT(dev.read<T>("/Deeper/hierarchies/also") == 20, 10000);
}

/*********************************************************************************************************************/
/* test subscript operator of DeviceModule */

BOOST_AUTO_TEST_CASE_TEMPLATE(testDeviceModuleSubscriptOp, T, test_types) {
  std::cout << "testDeviceModuleSubscriptOp" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  app.testModule.feedingToDevice >> app.dev["MyModule"]("actuator");
  ctk::TestFacility test;
  ChimeraTK::Device dev;
  dev.open("Dummy0");
  auto regacc = dev.getScalarRegisterAccessor<int>("/MyModule/actuator");
  test.runApplication();

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
/* test DeviceModule::virtualise()  (trivial implementation) */

BOOST_AUTO_TEST_CASE(testDeviceModuleVirtuallise) {
  std::cout << "testDeviceModuleVirtuallise" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<int> app;

  app.testModule.feedingToDevice >> app.dev.virtualise()["MyModule"]("actuator");

  ctk::TestFacility test;

  BOOST_CHECK(&(app.dev.virtualise()) == &(app.dev));
}

/*********************************************************************************************************************/
/* Application for tests of connectTo() */

template<typename T>
struct TestModule2 : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarOutput<T> actuator{this, "actuator", "MV/m", "Description"};
  ctk::ScalarPollInput<T> readback{this, "readBack", "MV/m", "Description"};

  void mainLoop() {}
};

template<typename T>
struct Deeper : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  struct Hierarchies : ctk::VariableGroup {
    using ctk::VariableGroup ::VariableGroup;
    struct Need : ctk::VariableGroup {
      using ctk::VariableGroup ::VariableGroup;
      ctk::ScalarPollInput<T> tests{this, "tests", "MV/m", "Description"};
    } need{this, "need", ""};
    ctk::ScalarOutput<T> also{this, "also", "MV/m", "Description", {"ALSO"}};
  } hierarchies{this, "hierarchies", ""};

  void mainLoop() {}
};

template<typename T>
struct TestApplication2 : public ctk::Application {
  TestApplication2() : Application("testSuite") {}
  ~TestApplication2() { shutdown(); }

  using Application::deviceMap;       // expose the device map for the tests
  using Application::makeConnections; // we call makeConnections() manually in
                                      // the tests to catch exceptions etc.
  using Application::networkList;     // expose network list to check merging
                                      // networks
  void defineConnections() {}         // the setup is done in the tests

  TestModule2<T> testModule{this, "MyModule", "The test module"};
  Deeper<T> deeper{this, "Deeper", ""};
  ctk::DeviceModule dev{this, "Dummy0"};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE_TEMPLATE(testConnectTo, T, test_types) {
  std::cout << "testConnectTo" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication2<int> app;
  app.testModule.connectTo(app.dev["MyModule"]);
  app.deeper.hierarchies.need.connectTo(app.dev["Deeper"]["hierarchies"]["need"]);
  app.deeper.hierarchies.findTag("ALSO").connectTo(app.dev["Deeper"]["hierarchies"]);

  ctk::TestFacility test;
  test.runApplication();

  ctk::Device dev;
  dev.open("Dummy0");
  auto actuator = dev.getScalarRegisterAccessor<T>("MyModule/actuator");
  auto& readback = actuator; // same address in map file
  auto tests = dev.getScalarRegisterAccessor<T>("Deeper/hierarchies/need/tests/DUMMY_WRITEABLE");
  auto also = dev.getScalarRegisterAccessor<T>("Deeper/hierarchies/also");

  app.testModule.actuator = 42;
  app.testModule.actuator.write();
  actuator.read();
  BOOST_CHECK_EQUAL(T(actuator), 42);

  app.testModule.actuator = 12;
  app.testModule.actuator.write();
  actuator.read();
  BOOST_CHECK_EQUAL(T(actuator), 12);

  readback = 120;
  readback.write();
  app.testModule.readback.read();
  BOOST_CHECK_EQUAL(T(app.testModule.readback), 120);

  readback = 66;
  readback.write();
  app.testModule.readback.read();
  BOOST_CHECK_EQUAL(T(app.testModule.readback), 66);

  tests = 120;
  tests.write();
  app.deeper.hierarchies.need.tests.read();
  BOOST_CHECK_EQUAL(T(app.deeper.hierarchies.need.tests), 120);

  tests = 66;
  tests.write();
  app.deeper.hierarchies.need.tests.read();
  BOOST_CHECK_EQUAL(T(app.deeper.hierarchies.need.tests), 66);

  app.deeper.hierarchies.also = 42;
  app.deeper.hierarchies.also.write();
  also.read();
  BOOST_CHECK_EQUAL(T(also), 42);

  app.deeper.hierarchies.also = 12;
  app.deeper.hierarchies.also.write();
  also.read();
  BOOST_CHECK_EQUAL(T(also), 12);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE_TEMPLATE(testConnectTo2, T, test_types) {
  std::cout << "testConnectTo2" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication2<int> app;
  app.findTag(".*").connectTo(app.dev);

  ctk::TestFacility test;
  test.runApplication();

  ctk::Device dev;
  dev.open("Dummy0");
  auto actuator = dev.getScalarRegisterAccessor<T>("MyModule/actuator");
  auto& readback = actuator; // same address in map file
  auto tests = dev.getScalarRegisterAccessor<T>("Deeper/hierarchies/need/tests/DUMMY_WRITEABLE");
  auto also = dev.getScalarRegisterAccessor<T>("Deeper/hierarchies/also");

  app.testModule.actuator = 42;
  app.testModule.actuator.write();
  actuator.read();
  BOOST_CHECK_EQUAL(T(actuator), 42);

  app.testModule.actuator = 12;
  app.testModule.actuator.write();
  actuator.read();
  BOOST_CHECK_EQUAL(T(actuator), 12);

  readback = 120;
  readback.write();
  app.testModule.readback.read();
  BOOST_CHECK_EQUAL(T(app.testModule.readback), 120);

  readback = 66;
  readback.write();
  app.testModule.readback.read();
  BOOST_CHECK_EQUAL(T(app.testModule.readback), 66);

  tests = 120;
  tests.write();
  app.deeper.hierarchies.need.tests.read();
  BOOST_CHECK_EQUAL(T(app.deeper.hierarchies.need.tests), 120);

  tests = 66;
  tests.write();
  app.deeper.hierarchies.need.tests.read();
  BOOST_CHECK_EQUAL(T(app.deeper.hierarchies.need.tests), 66);

  app.deeper.hierarchies.also = 42;
  app.deeper.hierarchies.also.write();
  also.read();
  BOOST_CHECK_EQUAL(T(also), 42);

  app.deeper.hierarchies.also = 12;
  app.deeper.hierarchies.also.write();
  also.read();
  BOOST_CHECK_EQUAL(T(also), 12);
}

/*********************************************************************************************************************/
/* Application for tests of ConnectingDeviceModule */

template<typename T>
struct Deeper2 : ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;

  ctk::ConnectingDeviceModule dev{this, "Dummy1", "/Deeper/hierarchies/trigger", nullptr, "/MyModule"};

  struct Hierarchies : ctk::ApplicationModule {
    using ctk::ApplicationModule ::ApplicationModule;

    struct Need : ctk::VariableGroup {
      using ctk::VariableGroup ::VariableGroup;
      ctk::ScalarPollInput<T> tests{this, "tests", "MV/m", "Description"};
    } need{this, "need", ""};
    ctk::ScalarOutput<T> also{this, "also", "MV/m", "Description", {"ALSO"}};
    ctk::ScalarOutput<T> trigger{this, "trigger", "MV/m", "Description", {"ALSO"}};

    void mainLoop() override {}
  } hierarchies{this, "hierarchies", ""};
};

template<typename T>
struct TestApplication3 : public ctk::Application {
  TestApplication3() : Application("testSuite") {}
  ~TestApplication3() override { shutdown(); }

  TestModule2<T> testModule{this, "MyModule", "The test module"};
  Deeper2<T> deeper{this, "Deeper", ""};

  std::atomic<size_t> initHandlerCallCount{0};
  ctk::ConnectingDeviceModule dev{this, "Dummy0", "", [this](ctk::DeviceModule*) { initHandlerCallCount++; }};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testConnectingDeviceModuleExceptions) {
  std::cout << "testConnectingDeviceModuleExceptions" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");
  TestApplication3<int> app;

  // wrong owner
  BOOST_CHECK_THROW(
      ctk::ConnectingDeviceModule(&app.deeper.hierarchies, "Dummy0", "", nullptr, "/MyModule"), ctk::logic_error);
  BOOST_CHECK_THROW(
      ctk::ConnectingDeviceModule(&app.deeper.hierarchies.need, "Dummy0", "", nullptr, "/MyModule"), ctk::logic_error);

  // non-absolute trigger path
  BOOST_CHECK_THROW(
      ctk::ConnectingDeviceModule(&app.deeper.hierarchies, "Dummy0", "unqualifiedName", nullptr, "/MyModule"),
      ctk::logic_error);
  BOOST_CHECK_THROW(
      ctk::ConnectingDeviceModule(&app.deeper.hierarchies, "Dummy0", "relative/name", nullptr, "/MyModule"),
      ctk::logic_error);
  BOOST_CHECK_THROW(
      ctk::ConnectingDeviceModule(&app.deeper.hierarchies, "Dummy0", "./also/relative", nullptr, "/MyModule"),
      ctk::logic_error);
  BOOST_CHECK_THROW(
      ctk::ConnectingDeviceModule(&app.deeper.hierarchies, "Dummy0", "../another/relative/name", nullptr, "/MyModule"),
      ctk::logic_error);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testConnectingDeviceModule) {
  std::cout << "testConnectingDeviceModule" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");
  TestApplication3<int> app;

  ctk::TestFacility test;
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

  // test the second ConnectingDeviceModule with the trigger
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

  std::vector<ctk::DeviceModule> devs;
  std::vector<ctk::ConnectingDeviceModule> cdevs;

  TestModule2<int> m{this, "MyModule", ""};
  Deeper<int> deeper{this, "Deeper", ""};

  void defineConnections() override {}
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDeviceModuleMove) {
  std::cout << "testDeviceModuleMove" << std::endl;

  // ordinary DeviceModule
  {
    TestApplication4 app;
    ctk::DeviceModule dev{&app, "Dummy1"};
    ctk::DeviceModule dev2{&app, "Dummy0"};
    dev = std::move(dev2);              // test move-assign
    app.devs.push_back(std::move(dev)); // test move-construct
    app.devs.back()["MyModule"].connectTo(app.m);
    ctk::TestFacility test;
    test.runApplication();
    ctk::Device dummy0("Dummy0");
    auto readBack = dummy0.getScalarRegisterAccessor<int>("MyModule/readBack/DUMMY_WRITEABLE");
    readBack = 432;
    readBack.write();
    app.m.readback.read();
    BOOST_CHECK_EQUAL(app.m.readback, 432);
  }

  // ConnectingDeviceModule
  {
    TestApplication4 app;
    ctk::ControlSystemModule cs;
    app.findTag(".*").connectTo(cs);
    ctk::ConnectingDeviceModule dev{&app, "Dummy1"};
    ctk::ConnectingDeviceModule dev2{&app, "Dummy0"};
    dev = std::move(dev2);               // test move-assign
    app.cdevs.push_back(std::move(dev)); // test move-construct
    ctk::TestFacility test;
    app.dumpConnections();
    test.runApplication();
    ctk::Device dummy0("Dummy0");
    auto readBack = dummy0.getScalarRegisterAccessor<int>("MyModule/readBack/DUMMY_WRITEABLE");
    readBack = 432;
    readBack.write();
    app.m.readback.read();
    BOOST_CHECK_EQUAL(app.m.readback, 432);
  }
}
/*********************************************************************************************************************/
