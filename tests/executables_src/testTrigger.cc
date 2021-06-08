/*
 * testTrigger.cc
 *
 *  Created on: Jun 22, 2016
 *      Author: Martin Hierholzer
 */

#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testTrigger

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/DeviceAccessVersion.h>
#include <ChimeraTK/DummyBackend.h>

#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>
#include <ChimeraTK/ControlSystemAdapter/DevicePVManager.h>
#include <ChimeraTK/ControlSystemAdapter/PVManager.h>

#include "Application.h"
#include "ApplicationModule.h"
#include "ControlSystemModule.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include "check_timeout.h"

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

constexpr char dummySdm[] = "sdm://./TestTransferGroupDummy=test.map";

// list of user types the accessors are tested with
typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double> test_types;

/**********************************************************************************************************************/

class TestTransferGroupDummy : public ChimeraTK::DummyBackend {
 public:
  TestTransferGroupDummy(std::string mapFileName) : DummyBackend(mapFileName) {}

  static boost::shared_ptr<DeviceBackend> createInstance(
      std::string, std::string, std::list<std::string> parameters, std::string) {
    return boost::shared_ptr<DeviceBackend>(new TestTransferGroupDummy(parameters.front()));
  }

  void read(uint64_t bar, uint64_t address, int32_t* data, size_t sizeInBytes) override {
    last_bar = bar;
    last_address = address;
    last_sizeInBytes = sizeInBytes;
    numberOfTransfers++;
    DummyBackend::read(bar, address, data, sizeInBytes);
  }

  std::atomic<size_t> numberOfTransfers{0};
  std::atomic<uint64_t> last_bar;
  std::atomic<uint64_t> last_address;
  std::atomic<size_t> last_sizeInBytes;
};

/*********************************************************************************************************************/
/* the ApplicationModule for the test is a template of the user type */

template<typename T>
struct TestModule : public ctk::ApplicationModule {
  TestModule(EntityOwner* owner, const std::string& name, const std::string& description,
      ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
      const std::unordered_set<std::string>& tags = {})
  : ApplicationModule(owner, name, description, hierarchyModifier, tags), mainLoopStarted(2) {}

  ctk::ScalarPushInput<T> consumingPush{this, "consumingPush", "MV/m", "Description"};
  ctk::ScalarPushInput<T> consumingPush2{this, "consumingPush2", "MV/m", "Description"};
  ctk::ScalarPushInput<T> consumingPush3{this, "consumingPush3", "MV/m", "Description"};

  ctk::ScalarPollInput<T> consumingPoll{this, "consumingPoll", "MV/m", "Description"};
  ctk::ScalarPollInput<T> consumingPoll2{this, "consumingPoll2", "MV/m", "Description"};
  ctk::ScalarPollInput<T> consumingPoll3{this, "consumingPoll3", "MV/m", "Description"};

  ctk::ScalarOutput<T> theTrigger{this, "theTrigger", "MV/m", "Description"};
  ctk::ScalarOutput<T> feedingToDevice{this, "feedingToDevice", "MV/m", "Description"};

  // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
  // mainLoop(). This is required since the mainLoopWrapper accesses the module variables before the start of the
  // mainLoop.
  // execute this right after the Application::run():
  //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
  boost::barrier mainLoopStarted;

  void prepare() override {
    incrementDataFaultCounter(); // force data to be flagged as faulty
    feedingToDevice = 13;        // the initial value
    writeAll();
    decrementDataFaultCounter(); // data validity depends on inputs
  }

  void mainLoop() override { mainLoopStarted.wait(); }
};

/*********************************************************************************************************************/
/* dummy application */

template<typename T>
struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {
    ChimeraTK::BackendFactory::getInstance().registerBackendType(
        "TestTransferGroupDummy", "", &TestTransferGroupDummy::createInstance, CHIMERATK_DEVICEACCESS_VERSION);
  }
  ~TestApplication() { shutdown(); }

  using Application::makeConnections; // we call makeConnections() manually in
                                      // the tests to catch exceptions etc.
  void defineConnections() {}         // the setup is done in the tests

  TestModule<T> testModule{this, "testModule", "The test module"};
  ctk::DeviceModule dev{this, "Dummy0"};
  ctk::DeviceModule dev2{this, dummySdm};
  ctk::ControlSystemModule cs;
};

/*********************************************************************************************************************/
/* test trigger by app variable when connecting a polled device register to an
 * app variable */

BOOST_AUTO_TEST_CASE_TEMPLATE(testTriggerDevToApp, T, test_types) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testTriggerDevToApp<" << typeid(T).name() << ">" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;
  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  app.testModule.feedingToDevice >> app.dev["MyModule"]("actuator");

  app.dev["MyModule"]("readBack")[app.testModule.theTrigger] >> app.testModule.consumingPush;
  app.initialise();

  app.run();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  // check for initial value. Should be there when entering the main loop.
  BOOST_CHECK_EQUAL(static_cast<T>(app.testModule.consumingPush), 13);

  // single theaded test
  app.testModule.feedingToDevice = 42;
  app.testModule.feedingToDevice.write();
  BOOST_CHECK(app.testModule.consumingPush.readNonBlocking() == false);
  BOOST_CHECK_EQUAL(static_cast<T>(app.testModule.consumingPush), 13);
  app.testModule.theTrigger.write();
  app.testModule.consumingPush.read();
  BOOST_CHECK(app.testModule.consumingPush == 42);

  // launch read() on the consumer asynchronously and make sure it does not yet
  // receive anything
  auto futRead = std::async(std::launch::async, [&app] { app.testModule.consumingPush.read(); });
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);

  BOOST_CHECK(app.testModule.consumingPush == 42);

  // write to the feeder
  app.testModule.feedingToDevice = 120;
  app.testModule.feedingToDevice.write();
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
  BOOST_CHECK(app.testModule.consumingPush == 42);

  // send trigger
  app.testModule.theTrigger.write();

  // check that the consumer now receives the just written value
  BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
  BOOST_CHECK(app.testModule.consumingPush == 120);
}

/*********************************************************************************************************************/
/* test trigger by app variable when connecting a polled device register to
 * control system variable */

BOOST_AUTO_TEST_CASE_TEMPLATE(testTriggerDevToCS, T, test_types) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testTriggerDevToCS<" << typeid(T).name() << ">" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  app.dev("/MyModule/readBack", typeid(T), 1)[app.testModule.theTrigger] >> app.cs("myCSVar");

  ctk::Device dev("Dummy0");
  dev.open();
  dev.write("MyModule/actuator", 1); // write initial value

  app.initialise();
  app.run();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  auto myCSVar = pvManagers.first->getProcessArray<T>("/myCSVar");

  // single theaded test only, since the receiving process scalar does not support blocking
  myCSVar->read(); // read initial value
  BOOST_CHECK_EQUAL(myCSVar->accessData(0), 1);
  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  dev.write("MyModule/actuator", 42);
  usleep(10000);
  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  app.testModule.setCurrentVersionNumber({});
  app.testModule.theTrigger.write();
  myCSVar->read();
  BOOST_CHECK_EQUAL(myCSVar->accessData(0), 42);

  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  dev.write("MyModule/actuator", 120);
  usleep(10000);
  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  app.testModule.theTrigger.write();
  myCSVar->read();
  BOOST_CHECK_EQUAL(myCSVar->accessData(0), 120);

  BOOST_CHECK(myCSVar->readNonBlocking() == false);
}

/*********************************************************************************************************************/
/* test trigger by app variable when connecting a polled device register to
 * control system variable */

BOOST_AUTO_TEST_CASE_TEMPLATE(testTriggerByCS, T, test_types) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testTriggerByCS<" << typeid(T).name() << ">" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  app.dev("/MyModule/readBack", typeid(T), 1)[app.cs("theTrigger", typeid(T), 1)] >> app.cs("myCSVar");

  ctk::Device dev("Dummy0");
  dev.open();
  dev.write("MyModule/actuator", 1); // write initial value

  app.initialise();
  app.run();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  auto myCSVar = pvManagers.first->getProcessArray<T>("/myCSVar");
  auto theTrigger = pvManagers.first->getProcessArray<T>("/theTrigger");

  // Need to send the trigger once, since ApplicationCore expects all CS variables to be written once by the
  // ControlSystemAdapter. We do not use the TestFacility here, so we have to do it ourself.
  theTrigger->write();

  // single theaded test only, since the receiving process scalar does not support blocking
  myCSVar->read(); // read initial value
  BOOST_CHECK_EQUAL(myCSVar->accessData(0), 1);
  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  dev.write("MyModule/actuator", 42);
  usleep(10000);
  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  myCSVar->accessData(0) = 0;
  theTrigger->write();
  myCSVar->read();
  BOOST_CHECK_EQUAL(myCSVar->accessData(0), 42);

  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  dev.write("MyModule/actuator", 120);
  usleep(10000);
  BOOST_CHECK(myCSVar->readNonBlocking() == false);
  myCSVar->accessData(0) = 0;
  theTrigger->write();
  myCSVar->read();
  BOOST_CHECK_EQUAL(myCSVar->accessData(0), 120);

  BOOST_CHECK(myCSVar->readNonBlocking() == false);
}

/*********************************************************************************************************************/
/* test that multiple variables triggered by the same source are put into the
 * same TransferGroup */

BOOST_AUTO_TEST_CASE_TEMPLATE(testTriggerTransferGroup, T, test_types) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testTriggerTransferGroup<" << typeid(T).name() << ">" << std::endl;

  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

  TestApplication<T> app;
  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  ChimeraTK::Device dev;
  dev.open(dummySdm);
  auto backend = boost::dynamic_pointer_cast<TestTransferGroupDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(dummySdm));
  BOOST_CHECK(backend != NULL);

  app.dev2("/REG1")[app.testModule.theTrigger] >> app.testModule.consumingPush;
  app.dev2("/REG2")[app.testModule.theTrigger] >> app.testModule.consumingPush2;
  app.dev2("/REG3")[app.testModule.theTrigger] >> app.testModule.consumingPush3;
  app.initialise();
  app.run();
  app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

  // initialise values
  app.testModule.consumingPush = 0;
  app.testModule.consumingPush2 = 0;
  app.testModule.consumingPush3 = 0;
  dev.write("/REG1", 11);
  dev.write("/REG2", 22);
  dev.write("/REG3", 33);

  // from the inital value transfer
  CHECK_TIMEOUT(backend->numberOfTransfers == 1, 10000);

  // trigger the transfer
  app.testModule.theTrigger.write();
  CHECK_TIMEOUT(backend->numberOfTransfers == 2, 10000);
  BOOST_CHECK(backend->last_bar == 0);
  BOOST_CHECK(backend->last_address == 0);
  BOOST_CHECK(backend->last_sizeInBytes == 12);

  // check result
  app.testModule.consumingPush.read();
  app.testModule.consumingPush2.read();
  app.testModule.consumingPush3.read();
  BOOST_CHECK_EQUAL(app.testModule.consumingPush, 11);
  BOOST_CHECK_EQUAL(app.testModule.consumingPush2, 22);
  BOOST_CHECK_EQUAL(app.testModule.consumingPush3, 33);

  // prepare a second transfer
  dev.write("/REG1", 12);
  dev.write("/REG2", 23);
  dev.write("/REG3", 34);

  // trigger the transfer
  app.testModule.theTrigger.write();
  CHECK_TIMEOUT(backend->numberOfTransfers == 3, 10000);
  BOOST_CHECK(backend->last_bar == 0);
  BOOST_CHECK(backend->last_address == 0);
  BOOST_CHECK(backend->last_sizeInBytes == 12);

  // check result
  app.testModule.consumingPush.read();
  app.testModule.consumingPush2.read();
  app.testModule.consumingPush3.read();
  BOOST_CHECK_EQUAL(app.testModule.consumingPush, 12);
  BOOST_CHECK_EQUAL(app.testModule.consumingPush2, 23);
  BOOST_CHECK_EQUAL(app.testModule.consumingPush3, 34);

  dev.close();
}
