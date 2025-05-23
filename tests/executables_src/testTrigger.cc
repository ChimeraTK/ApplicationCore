// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VoidAccessor.h"

#define BOOST_TEST_MODULE testTrigger

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>
#include <ChimeraTK/ControlSystemAdapter/DevicePVManager.h>
#include <ChimeraTK/ControlSystemAdapter/PVManager.h>
#include <ChimeraTK/Device.h>
#include <ChimeraTK/DeviceAccessVersion.h>
#include <ChimeraTK/DummyBackend.h>

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

#include <latch>

namespace Tests::testTrigger {

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  // Application that has one polling consumer for a polling provider
  // It should work without any trigger
  struct TestApp1 : ctk::Application {
    TestApp1() : ctk::Application("testApp1") {}
    ~TestApp1() override { shutdown(); }

    struct : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ScalarPollInput<int> readBack{this, "/MyModule/readBack", "unit", "description"};

      // This is just here so that we do not need a trigger - otherwise it would be connected to a pushing CS consumer
      // automatically which would require a trigger
      ctk::ScalarPollInput<int> tests{this, "/Deeper/hierarchies/need/tests", "unit", "description"};

      ctk::VoidInput finger{this, "/finger", ""};

      void mainLoop() override {
        while(true) {
          readAll();
        }
      }
    } someModule{this, ".", ""};

    ctk::SetDMapFilePath path{"test.dmap"};
    ctk::DeviceModule dev;
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(TestDev2AppWithPollTrigger) {
    // TestApp1 should work without specifying any trigger
    {
      TestApp1 app;
      app.dev = {&app, "Dummy0"};
      ChimeraTK::TestFacility tf{app};
      auto finger = tf.getVoid("/finger");
      auto rb = tf.getScalar<int>("/MyModule/readBack");

      tf.runApplication();

      ctk::Device dev("Dummy0");
      dev.open();
      dev.write("MyModule/actuator", 1);

      BOOST_TEST(rb.readNonBlocking() == false);
      finger.write();
      tf.stepApplication();
      BOOST_TEST(rb.readNonBlocking() == true);
      BOOST_TEST(rb == 1);

      dev.write("MyModule/actuator", 10);
      finger.write();
      tf.stepApplication();
      BOOST_TEST(rb.readNonBlocking() == true);
      BOOST_TEST(rb == 10);
    }

    // TestApp1 should also work with any trigger, but the trigger should be ignored
    {
      TestApp1 app;
      app.dev = {&app, "Dummy0", "/cs/tick"};
      ChimeraTK::TestFacility tf{app};
      auto tick = tf.getVoid("/cs/tick");
      auto finger = tf.getVoid("/finger");
      auto rb = tf.getScalar<int>("/MyModule/readBack");

      tf.runApplication();

      ctk::Device dev("Dummy0");
      dev.open();
      dev.write("MyModule/actuator", 2);

      BOOST_TEST(rb.readNonBlocking() == false);
      finger.write();
      tf.stepApplication();
      BOOST_TEST(rb.readNonBlocking() == true);
      BOOST_TEST(rb == 2);

      // Trigger device trigger - values should not change
      tick.write();
      tf.stepApplication();
      BOOST_TEST(rb.readNonBlocking() == false);
      BOOST_TEST(rb == 2);

      dev.write("MyModule/actuator", 20);

      // Trigger read-out of poll variables in main loop
      finger.write();
      tf.stepApplication();
      BOOST_TEST(rb.readNonBlocking() == true);
      BOOST_TEST(rb == 20);

      // Trigger device trigger - values should not change
      tick.write();
      tf.stepApplication();
      BOOST_TEST(rb.readNonBlocking() == false);
      BOOST_TEST(rb == 20);
    }
  }

  /********************************************************************************************************************/

  struct TestApp2 : ctk::Application {
    TestApp2() : ctk::Application("testApp2") {}
    ~TestApp2() override { shutdown(); }

    struct : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ScalarPushInput<int> readBack{this, "/MyModule/readBack", "unit", "description"};

      void mainLoop() override {
        while(true) {
          readAll();
        }
      }
    } someModule{this, ".", ""};

    ctk::SetDMapFilePath path{"test.dmap"};
    ctk::DeviceModule dev;
  };

  /********************************************************************************************************************/

  // Device that requires trigger, the trigger is 1:1 put into the CS
  BOOST_AUTO_TEST_CASE(TestDev2AppWithCsDirectTrigger) {
    // TestApp2 should not work without specifying any trigger
    {
      TestApp2 app;
      app.dev = {&app, "Dummy0"};
      BOOST_CHECK_THROW(
          {
            app.initialise();
            app.run();
          },
          ChimeraTK::logic_error);
    }

    // TestApp2 also works with a trigger. If the trigger is triggered, no data transfer should happen
    {
      TestApp2 app;
      app.dev = {&app, "Dummy0", "/cs/trigger"};

      ChimeraTK::TestFacility tf{app, true};
      auto tick = tf.getVoid("/cs/trigger");
      auto rb = tf.getScalar<int>("/MyModule/readBack");

      tf.runApplication();

      ctk::Device dev("Dummy0");
      dev.open();
      dev.write("MyModule/actuator", 1);

      tick.write();
      tf.stepApplication();

      BOOST_TEST(rb.readNonBlocking() == true);
      BOOST_TEST(rb == 1);

      dev.write("MyModule/actuator", 12);
      BOOST_TEST(rb.readNonBlocking() == false);
      BOOST_TEST(rb == 1);

      tick.write();
      tf.stepApplication();
      BOOST_TEST(rb.readNonBlocking() == true);
      BOOST_TEST(rb == 12);
    }
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  struct TestApp3 : ctk::Application {
    TestApp3() : ctk::Application("testApp3") {}
    ~TestApp3() override { shutdown(); }

    struct : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::VoidInput tick{this, "/cs/trigger", "description"};
      ctk::ScalarOutput<int> tock{this, "/tock", "", ""};

      void mainLoop() override {
        tock = 0;
        while(true) {
          tock.write();
          tock++;
          readAll();
        }
      }
    } tock{this, ".", ""};

    struct : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ScalarPushInput<int> readBack{this, "/MyModule/readBack", "unit", "description"};
      ctk::ScalarPollInput<int> tests{this, "/Deeper/hierarchies/need/tests", "unit", "description"};

      void mainLoop() override {
        while(true) {
          readAll();
        }
      }
    } someModule{this, ".", ""};

    ctk::SetDMapFilePath path{"test.dmap"};
    ctk::DeviceModule dev{this, "Dummy0", "/cs/trigger"};
  };

  /********************************************************************************************************************/

  // Device that requires trigger, the trigger is distributed in the Application as well
  BOOST_AUTO_TEST_CASE(TestDev2AppWithCsDistributedTrigger) {
    TestApp3 app;

    ChimeraTK::TestFacility tf{app, true};
    auto tick = tf.getVoid("/cs/trigger");
    auto tock = tf.getScalar<int>("/tock");
    auto rb = tf.getScalar<int>("/MyModule/readBack");

    tf.runApplication();

    ctk::Device dev("Dummy0");
    dev.open();
    dev.write("MyModule/actuator", 1);

    tick.write();
    tf.stepApplication();

    BOOST_TEST(tock.readNonBlocking() == true);
    BOOST_TEST(tock == 1);
    BOOST_TEST(rb.readNonBlocking() == true);
    BOOST_TEST(rb == 1);

    dev.write("MyModule/actuator", 12);
    BOOST_TEST(tock.readNonBlocking() == false);
    BOOST_TEST(tock == 1);
    BOOST_TEST(rb.readNonBlocking() == false);
    BOOST_TEST(rb == 1);

    tick.write();
    tf.stepApplication();
    BOOST_TEST(tock.readNonBlocking() == true);
    BOOST_TEST(tock == 2);
    BOOST_TEST(rb.readNonBlocking() == true);
    BOOST_TEST(rb == 12);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  struct TestApp4 : ctk::Application {
    TestApp4() : ctk::Application("testApp4") {}
    ~TestApp4() override { shutdown(); }

    struct : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ScalarPushInput<float> singed32{this, "/Device/signed32", "unit", "description"};

      void mainLoop() override {
        while(true) {
          readAll();
        }
      }
    } someOtherModule{this, ".", ""};

    struct : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ScalarPushInput<int> readBack{this, "/MyModule/readBack", "unit", "description"};
      ctk::ScalarPollInput<int> tests{this, "/Deeper/hierarchies/need/tests", "unit", "description"};

      void mainLoop() override {
        while(true) {
          readAll();
        }
      }
    } someModule{this, ".", ""};

    ctk::SetDMapFilePath path{"test.dmap"};
    ctk::DeviceModule dev{this, "Dummy0", "/cs/trigger"};
    ctk::DeviceModule dev2{this, "Dummy1Mapped", "/cs/trigger"};
  };

  /********************************************************************************************************************/

  // Two devices using the same trigger
  BOOST_AUTO_TEST_CASE(TestDev2App1Trigger2Devices) {
    TestApp4 app;

    ChimeraTK::TestFacility tf{app, true};
    auto tick = tf.getVoid("/cs/trigger");
    auto f = tf.getScalar<float>("/Device/signed32");
    auto rb = tf.getScalar<int>("/MyModule/readBack");

    ctk::Device dev("Dummy0");
    dev.open();

    ctk::Device dev2("Dummy1");
    dev2.open();
    dev2.write("FixedPoint/value", 12.4);

    tf.runApplication();

    dev.write("MyModule/actuator", 1);

    BOOST_TEST(f.readNonBlocking() == false);
    BOOST_TEST(rb.readNonBlocking() == false);

    tick.write();
    tf.stepApplication();
    BOOST_TEST(f.readNonBlocking() == true);
    BOOST_TEST(rb.readNonBlocking() == true);

    BOOST_TEST((f - 12.4) < 0.01);
    BOOST_TEST(rb == 1);

    dev.write("MyModule/actuator", 2);
    dev2.write("FixedPoint/value", 24.8);

    BOOST_TEST(f.readNonBlocking() == false);
    BOOST_TEST(rb.readNonBlocking() == false);

    tick.write();
    tf.stepApplication();
    BOOST_TEST(f.readNonBlocking() == true);
    BOOST_TEST(rb.readNonBlocking() == true);

    BOOST_TEST((f - 24.8) < 0.001);
    BOOST_TEST(rb == 2);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  struct TestApp5 : ctk::Application {
    TestApp5() : ctk::Application("testApp5") {}
    ~TestApp5() override { shutdown(); }

    struct : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::VoidInput finger{this, "/finger", ""};
      ctk::VoidOutput trigger{this, "/trigger", ""};

      void mainLoop() override {
        while(true) {
          readAll();
          trigger.write();
        }
      }
    } someModule{this, ".", ""};

    ctk::SetDMapFilePath path{"test.dmap"};
    ctk::DeviceModule dev;
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(TestDev2CSCsTrigger) {
    TestApp5 app;
    app.dev = {&app, "Dummy0", "/cs/trigger"};

    ChimeraTK::TestFacility tf{app, true};
    auto tick = tf.getVoid("/cs/trigger");
    auto rb = tf.getScalar<int>("/MyModule/readBack");

    tf.runApplication();

    ctk::Device dev("Dummy0");
    dev.open();
    dev.write("MyModule/actuator", 1);

    tick.write();
    tf.stepApplication();

    BOOST_TEST(rb.readNonBlocking() == true);
    BOOST_TEST(rb == 1);

    dev.write("MyModule/actuator", 12);
    BOOST_TEST(rb.readNonBlocking() == false);
    BOOST_TEST(rb == 1);

    tick.write();
    tf.stepApplication();
    BOOST_TEST(rb.readNonBlocking() == true);
    BOOST_TEST(rb == 12);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(TestDev2CSAppTrigger) {
    TestApp5 app;
    app.dev = {&app, "Dummy0", "/trigger"};

    ChimeraTK::TestFacility tf{app, true};
    auto tick = tf.getVoid("/finger");
    auto rb = tf.getScalar<int>("/MyModule/readBack");

    tf.runApplication();

    ctk::Device dev("Dummy0");
    dev.open();
    dev.write("MyModule/actuator", 1);

    tick.write();
    tf.stepApplication();

    BOOST_TEST(rb.readNonBlocking() == true);
    BOOST_TEST(rb == 1);

    dev.write("MyModule/actuator", 12);
    BOOST_TEST(rb.readNonBlocking() == false);
    BOOST_TEST(rb == 1);

    tick.write();
    tf.stepApplication();
    BOOST_TEST(rb.readNonBlocking() == true);
    BOOST_TEST(rb == 12);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string dummySdm{"(TestTransferGroupDummy?map=test_readonly.map)"};

  // list of user types the accessors are tested with
  using test_types = boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double>;

  /********************************************************************************************************************/

  class TestTransferGroupDummy : public ChimeraTK::DummyBackend {
   public:
    explicit TestTransferGroupDummy(const std::string& mapFileName) : DummyBackend(mapFileName) {}

    // FIXME: This depends on the API from DeviceAccess. If this linter warning is fixed, code will not compile anymore
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    static boost::shared_ptr<DeviceBackend> createInstance(std::string, std::map<std::string, std::string> parameters) {
      return boost::shared_ptr<DeviceBackend>(new TestTransferGroupDummy(parameters["map"]));
    }

    void read(uint64_t bar, uint64_t address, int32_t* data, size_t sizeInBytes) override {
      lastBar = bar;
      lastAddress = address;
      lastSizeInBytes = sizeInBytes;
      numberOfTransfers++;
      DummyBackend::read(bar, address, data, sizeInBytes);
    }

    std::atomic<size_t> numberOfTransfers{0};
    std::atomic<uint64_t> lastBar;
    std::atomic<uint64_t> lastAddress;
    std::atomic<size_t> lastSizeInBytes;
  };

  /********************************************************************************************************************/
  /* the ApplicationModule for the test is a template of the user type */

  struct TestModule : public ctk::ApplicationModule {
    TestModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, tags), mainLoopStarted(2) {}

    ctk::ScalarPushInput<int> consumingPush{this, "/REG1", "MV/m", "Description"};
    ctk::ScalarPushInput<int> consumingPush2{this, "/REG2", "MV/m", "Description"};
    ctk::ScalarPushInput<int> consumingPush3{this, "/REG3", "MV/m", "Description"};

    ctk::ScalarOutput<int> theTrigger{this, "theTrigger", "MV/m", "Description"};

    // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
    // mainLoop(). This is required since the mainLoopWrapper accesses the module variables before the start of the
    // mainLoop.
    // execute this right after the Application::run():
    //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
    boost::barrier mainLoopStarted;

    void prepare() override {
      incrementDataFaultCounter(); // force data to be flagged as faulty
      writeAll();
      decrementDataFaultCounter(); // data validity depends on inputs
    }

    void mainLoop() override {
      std::cout << "Start of main loop" << std::endl;
      mainLoopStarted.wait();
      std::cout << "End of main loop" << std::endl;
    }
  };

  /********************************************************************************************************************/
  /* dummy application */

  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {
      ChimeraTK::BackendFactory::getInstance().registerBackendType(
          "TestTransferGroupDummy", &TestTransferGroupDummy::createInstance);
      dev2 = {this, dummySdm, "/testModule/theTrigger"};
    }
    ~TestApplication() override { shutdown(); }

    TestModule testModule{this, "testModule", "The test module"};
    ctk::DeviceModule dev2;
  };

  /********************************************************************************************************************/
  /* test that multiple variables triggered by the same source are put into the
   * same TransferGroup */

  BOOST_AUTO_TEST_CASE(TestTriggerTransferGroup) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> testTriggerTransferGroup" << std::endl;

    ChimeraTK::BackendFactory::getInstance().setDMapFilePath("test.dmap");

    TestApplication app;
    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);

    ChimeraTK::Device dev;
    dev.open(dummySdm);
    auto backend = boost::dynamic_pointer_cast<TestTransferGroupDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(dummySdm));
    BOOST_CHECK(backend != nullptr);

    app.initialise();
    app.run();
    app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

    // initialise values
    app.testModule.consumingPush = 0;
    app.testModule.consumingPush2 = 0;
    app.testModule.consumingPush3 = 0;
    dev.write("/REG1.DUMMY_WRITEABLE", 11);
    dev.write("/REG2.DUMMY_WRITEABLE", 22);
    dev.write("/REG3.DUMMY_WRITEABLE", 33);

    // from the initial value transfer
    CHECK_TIMEOUT(backend->numberOfTransfers == 1, 10000);

    // trigger the transfer
    app.testModule.theTrigger.write();
    CHECK_TIMEOUT(backend->numberOfTransfers == 2, 10000);
    BOOST_TEST(backend->lastBar == 0);
    BOOST_TEST(backend->lastAddress == 0);

    // We only explicitly connect the three registers in the app, but the connection code will also connect the other
    // registers into the CS, hence we need to check for the full size
    BOOST_TEST(backend->lastSizeInBytes == 32);

    // check result
    app.testModule.consumingPush.read();
    app.testModule.consumingPush2.read();
    app.testModule.consumingPush3.read();
    BOOST_CHECK_EQUAL(app.testModule.consumingPush, 11);
    BOOST_CHECK_EQUAL(app.testModule.consumingPush2, 22);
    BOOST_CHECK_EQUAL(app.testModule.consumingPush3, 33);

    // prepare a second transfer
    dev.write("/REG1.DUMMY_WRITEABLE", 12);
    dev.write("/REG2.DUMMY_WRITEABLE", 23);
    dev.write("/REG3.DUMMY_WRITEABLE", 34);

    // trigger the transfer
    app.testModule.theTrigger.write();
    CHECK_TIMEOUT(backend->numberOfTransfers == 3, 10000);
    BOOST_TEST(backend->lastBar == 0);
    BOOST_TEST(backend->lastAddress == 0);

    // We only explicitly connect the three registers in the app, but the connection code will also connect the other
    // registers into the CS, hence we need to check for the full size
    BOOST_TEST(backend->lastSizeInBytes == 32);

    // check result
    app.testModule.consumingPush.read();
    app.testModule.consumingPush2.read();
    app.testModule.consumingPush3.read();
    BOOST_CHECK_EQUAL(app.testModule.consumingPush, 12);
    BOOST_CHECK_EQUAL(app.testModule.consumingPush2, 23);
    BOOST_CHECK_EQUAL(app.testModule.consumingPush3, 34);

    dev.close();
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  /* dummy application */

  struct TestApplication2 : public ctk::Application {
    TestApplication2() : Application("testSuite") {}
    ~TestApplication2() override { shutdown(); }

    std::atomic<bool> block{false};
    std::latch confirmBlock{2};
    std::latch unblock{2};

    ctk::SetDMapFilePath dmap{"test.dmap"};
    ctk::DeviceModule dev{this, "Dummy0", "/testModule/theTrigger", [&](ctk::Device&) {
                            if(block) {
                              std::cout << "Init Handler blocked." << std::endl;
                              confirmBlock.arrive_and_wait();
                              unblock.arrive_and_wait();
                            }
                            std::cout << "Init Handler completed." << std::endl;
                          }};
  };

  /********************************************************************************************************************/
  /*
   * Test that the init handler closing the device while executing does not interfer with the TransferGroup reading
   * in the TriggerFanOut. See #14286 for background information.
   *
   * Just connecting a single variable to the control system with a trigger will already give us a TriggerFanOut which
   * always uses internally a TransferGroup, so no complicated setup should be required here.
   */

  BOOST_AUTO_TEST_CASE(TestTriggerTransferGroupInitHandler) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> TestTriggerTransferGroupInitHandler" << std::endl;

    TestApplication2 app;

    ctk::TestFacility test(app, false); // no testable mode, since we need to block the device recovery

    auto trigger = test.getVoid("/testModule/theTrigger");
    auto readBack = test.getScalar<int>("/MyModule/readBack");
    auto devStatus = test.getScalar<int>("/Devices/Dummy0/status");

    test.runApplication();

    // await device to be opened
    BOOST_REQUIRE(devStatus == 0);
    devStatus.read();
    BOOST_REQUIRE(devStatus == 1);

    // trigger once and read the polled data, to make sure everything is running. Otherwise we would not (reliably)
    // observe the failure below.
    trigger.write();
    readBack.read();

    // trigger init handler and keep it blocked. The device will remain closed until the init handler is unblocked.
    app.block = true;
    app.dev.reportException("Force error");
    app.confirmBlock.arrive_and_wait();

    devStatus.read();
    BOOST_REQUIRE(devStatus == 0);

    // trigger the TriggerFanOut which will attempt to read from the closed device. The ExceptionHandlingDecorator
    // should prevent this, so no logic_error should be thrown.
    trigger.write();
    // 0.5 seconds delay, give TriggerFanOut some time. In case of a bug, the logic_error will be thrown in the
    // TriggerFanOut thread.
    usleep(500000);

    // unblock to finish recovery
    app.block = false;
    app.unblock.arrive_and_wait();
    devStatus.read();
    BOOST_REQUIRE(devStatus == 1);
  }

  /********************************************************************************************************************/

} // namespace Tests::testTrigger
