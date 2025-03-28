// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testConfigReader

#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ConfigReader.h"
#include "DeviceModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "VariableGroup.h"

namespace ctk = ChimeraTK;

constexpr std::string_view cdd{"(dummy?map=configReaderDevice.map)"};

namespace Tests::testConfigReader {

  /********************************************************************************************************************/
  /* Module to receive the config values */

  struct TestModule : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ConfigReader* theConfigReader; // just to compare if the correct instance is returned
    bool appConfigHasThrown{false};

    ctk::ScalarPushInput<int8_t> var8{this, "var8", "MV/m", "Desc"};
    ctk::ScalarPushInput<uint8_t> var8u{this, "var8u", "MV/m", "Desc"};
    ctk::ScalarPushInput<int16_t> var16{this, "var16", "MV/m", "Desc"};
    ctk::ScalarPushInput<uint16_t> var16u{this, "var16u", "MV/m", "Desc"};
    ctk::ScalarPushInput<int32_t> var32{this, "var32", "MV/m", "Desc"};
    ctk::ScalarPushInput<uint32_t> var32u{this, "var32u", "MV/m", "Desc"};
    ctk::ScalarPushInput<int64_t> var64{this, "var64", "MV/m", "Desc"};
    ctk::ScalarPushInput<uint64_t> var64u{this, "var64u", "MV/m", "Desc"};
    ctk::ScalarPushInput<float> varFloat{this, "varFloat", "MV/m", "Desc"};
    ctk::ScalarPushInput<double> varDouble{this, "varDouble", "MV/m", "Desc"};
    ctk::ScalarPushInput<std::string> varString{this, "varString", "MV/m", "Desc"};
    ctk::ScalarPushInput<int32_t> varAnotherInt{this, "varAnotherInt", "MV/m", "Desc"};
    ctk::ArrayPushInput<int32_t> intArray{this, "intArray", "MV/m", 10, "Desc"};
    ctk::ArrayPushInput<std::string> stringArray{this, "stringArray", "", 8, "Desc"};

    struct Module1 : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<int16_t> var16{this, "var16", "MV/m", "Desc"};
      ctk::ScalarPushInput<uint16_t> var16u{this, "var16u", "MV/m", "Desc"};
      ctk::ScalarPushInput<int32_t> var32{this, "var32", "MV/m", "Desc"};
      ctk::ScalarPushInput<uint32_t> var32u{this, "var32u", "MV/m", "Desc"};
      ctk::ScalarPushInput<std::string> varString{this, "varString", "MV/m", "Desc"};

      struct SubModule : ctk::VariableGroup {
        using ctk::VariableGroup::VariableGroup;
        ctk::ScalarPushInput<uint32_t> var32u{this, "var32u", "MV/m", "Desc"};
        ctk::ArrayPushInput<int32_t> intArray{this, "intArray", "MV/m", 10, "Desc"};
        ctk::ArrayPushInput<std::string> stringArray{this, "stringArray", "", 8, "Desc"};

        struct SubSubModule : ctk::VariableGroup {
          using ctk::VariableGroup::VariableGroup;
          ctk::ScalarPushInput<int32_t> var32{this, "var32", "MV/m", "Desc"};
          ctk::ScalarPushInput<uint32_t> var32u{this, "var32u", "MV/m", "Desc"};
        } subsubmodule{this, "subsubmodule", ""};
      } submodule{this, "submodule", ""};

    } module1{this, "module1", ""};

    struct Module2 : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;

      struct AnotherSubModule : ctk::VariableGroup {
        using ctk::VariableGroup::VariableGroup;
        ctk::ScalarPushInput<double> var1{this, "var1", "m", "Desc"};
        ctk::ScalarPushInput<double> var2{this, "var2", "kg", "Desc"};
      } submodule1{this, "submodule1", ""}, submodule2{this, "submodule2", ""};
    } module2{this, "module2", ""};

    std::atomic<bool> done{false};

    void mainLoop() override {
      // values should be available right away
      BOOST_CHECK_EQUAL(int8_t(var8), -123);
      BOOST_CHECK_EQUAL(uint8_t(var8u), 34);
      BOOST_CHECK_EQUAL(int16_t(var16), -567);
      BOOST_CHECK_EQUAL(uint16_t(var16u), 678);
      BOOST_CHECK_EQUAL(int32_t(var32), -345678);
      BOOST_CHECK_EQUAL(uint32_t(var32u), 234567);
      BOOST_CHECK_EQUAL(int64_t(var64), -2345678901234567890);
      BOOST_CHECK_EQUAL(uint64_t(var64u), 12345678901234567890U);
      BOOST_CHECK_CLOSE(float(varFloat), 3.1415, 0.000001);
      BOOST_CHECK_CLOSE(double(varDouble), -2.8, 0.000001);
      BOOST_CHECK_EQUAL(std::string(varString), "My dear mister singing club!");

      BOOST_CHECK_EQUAL(intArray.getNElements(), 10);
      for(unsigned int i = 0; i < 10; ++i) {
        BOOST_CHECK_EQUAL(intArray[i], 10 - i);
      }

      BOOST_CHECK_EQUAL(stringArray.getNElements(), 8);
      for(unsigned int i = 0; i < 8; ++i) {
        BOOST_CHECK_EQUAL(stringArray[i], "Hallo" + std::to_string(i + 1));
      }

      BOOST_CHECK_EQUAL(int16_t(module1.var16), -567);
      BOOST_CHECK_EQUAL(uint16_t(module1.var16u), 678);
      BOOST_CHECK_EQUAL(int32_t(module1.var32), -345678);
      BOOST_CHECK_EQUAL(uint32_t(module1.var32u), 234567);
      BOOST_CHECK_EQUAL(uint32_t(module1.submodule.var32u), 234567);

      BOOST_CHECK_EQUAL(module1.submodule.intArray.getNElements(), 10);
      for(unsigned int i = 0; i < 10; ++i) {
        BOOST_CHECK_EQUAL(module1.submodule.intArray[i], 10 - i);
      }

      BOOST_CHECK_EQUAL(module1.submodule.stringArray.getNElements(), 8);
      for(unsigned int i = 0; i < 8; ++i) {
        BOOST_CHECK_EQUAL(module1.submodule.stringArray[i], "Hallo" + std::to_string(i + 1));
      }

      // no further update shall be received
      usleep(1000000); // 1 second
      BOOST_CHECK(!var8.readNonBlocking());
      BOOST_CHECK(!var8u.readNonBlocking());
      BOOST_CHECK(!var16.readNonBlocking());
      BOOST_CHECK(!var16u.readNonBlocking());
      BOOST_CHECK(!var32.readNonBlocking());
      BOOST_CHECK(!var32u.readNonBlocking());
      BOOST_CHECK(!var64.readNonBlocking());
      BOOST_CHECK(!var64u.readNonBlocking());
      BOOST_CHECK(!varFloat.readNonBlocking());
      BOOST_CHECK(!varDouble.readNonBlocking());
      BOOST_CHECK(!varString.readNonBlocking());
      BOOST_CHECK(!intArray.readNonBlocking());

      BOOST_CHECK(!module1.var16.readNonBlocking());
      BOOST_CHECK(!module1.var16u.readNonBlocking());
      BOOST_CHECK(!module1.var32.readNonBlocking());
      BOOST_CHECK(!module1.var32u.readNonBlocking());
      BOOST_CHECK(!module1.submodule.var32u.readNonBlocking());
      BOOST_CHECK(!module1.submodule.intArray.readNonBlocking());
      BOOST_CHECK(!module1.submodule.stringArray.readNonBlocking());

      // inform main thread that we are done
      done = true;
    }
  };

  /********************************************************************************************************************/
  /* dummy application */

  struct TestApplication : public ctk::Application {
    TestApplication(const std::string& name = "valid") : Application(name) {}
    ~TestApplication() override { shutdown(); }

    TestModule testModule{this, "/", "The test module"};
  };

  /********************************************************************************************************************/
  /* dummy application with two config readers (to check the exception in ApplicationModule::appConfig()) */

  struct TestApplicationTwoConfigs : public ctk::Application {
    TestApplicationTwoConfigs() : Application("TestApplicationTwoConfigs") {}
    ~TestApplicationTwoConfigs() override { shutdown(); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    ctk::ConfigReader config{this, "config", "validConfig.xml", {"MyTAG"}};
    ctk::ConfigReader config2{this, "config2", "validConfig.xml"};
#pragma GCC diagnostic pop
  };

  /********************************************************************************************************************/
  /* dummy application with default config readers, but no matching config file */

  struct TestApplicationNoConfigs : public ctk::Application {
    TestApplicationNoConfigs() : Application("TestApplicationNoConfigs") {}
    ~TestApplicationNoConfigs() override { shutdown(); }
  };

  /********************************************************************************************************************/
  /* dummy application with deprecated config that is invalid */

  struct TestApplicationInvalidConfig : public ctk::Application {
    TestApplicationInvalidConfig() : Application("TestApplicationInvalidConfig") {}
    ~TestApplicationInvalidConfig() override { shutdown(); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    ctk::ConfigReader config{this, ".", "InValidConfig.xml", {"MyTAG"}};
#pragma GCC diagnostic pop
  };

  /********************************************************************************************************************/
  /* dummy application which directly connects config reader variables to a device */

  struct TestApplicationWithDevice : public ctk::Application {
    TestApplicationWithDevice() : Application("valid") {}
    ~TestApplicationWithDevice() override { shutdown(); }

    ctk::DeviceModule device{this, cdd.data()};
  };

  /********************************************************************************************************************/
  /* test trigger by app variable when connecting a polled device register to an
   * app variable */

  BOOST_AUTO_TEST_CASE(testConfigReader) {
    std::cout << "==> testConfigReader" << std::endl;

    TestApplication app;
    auto& config = app.getConfigReader();

    BOOST_TEST(config.getOwner() != nullptr);

    // check if values are already accessible
    BOOST_CHECK_EQUAL(config.get<int8_t>("var8"), -123);
    BOOST_CHECK_EQUAL(config.get<uint8_t>("var8u"), 34);
    BOOST_CHECK_EQUAL(config.get<int16_t>("var16"), -567);
    BOOST_CHECK_EQUAL(config.get<uint16_t>("var16u"), 678);
    BOOST_CHECK_EQUAL(config.get<int32_t>("var32"), -345678);
    BOOST_CHECK_EQUAL(config.get<uint32_t>("var32u"), 234567);
    BOOST_CHECK_EQUAL(config.get<int64_t>("var64"), -2345678901234567890);
    BOOST_CHECK_EQUAL(config.get<uint64_t>("var64u"), 12345678901234567890U);
    BOOST_CHECK_CLOSE(config.get<float>("varFloat"), 3.1415, 0.000001);
    BOOST_CHECK_CLOSE(config.get<double>("varDouble"), -2.8, 0.000001);
    BOOST_CHECK_EQUAL(config.get<std::string>("varString"), "My dear mister singing club!");

    std::vector<int> arrayValue = config.get<std::vector<int>>("intArray");
    BOOST_CHECK_EQUAL(arrayValue.size(), 10);
    for(size_t i = 0; i < 10; ++i) {
      BOOST_CHECK_EQUAL(arrayValue[i], 10 - i);
    }

    std::vector<std::string> arrayValueString = config.get<std::vector<std::string>>("stringArray");
    BOOST_CHECK_EQUAL(arrayValueString.size(), 8);
    for(size_t i = 0; i < 8; ++i) {
      BOOST_CHECK_EQUAL(arrayValueString[i], "Hallo" + std::to_string(i + 1));
    }

    BOOST_CHECK_EQUAL(config.get<int16_t>("module1/var16"), -567);
    BOOST_CHECK_EQUAL(config.get<uint16_t>("module1/var16u"), 678);
    BOOST_CHECK_EQUAL(config.get<int32_t>("module1/var32"), -345678);
    BOOST_CHECK_EQUAL(config.get<uint32_t>("module1/var32u"), 234567);
    BOOST_CHECK_EQUAL(config.get<uint32_t>("module1/submodule/var32u"), 234567);
    BOOST_CHECK_EQUAL(config.get<uint32_t>("module1/submodule/subsubmodule/var32u"), 234568);

    arrayValue = config.get<std::vector<int>>("module1/submodule/intArray");
    BOOST_CHECK_EQUAL(arrayValue.size(), 10);
    for(size_t i = 0; i < 10; ++i) {
      BOOST_CHECK_EQUAL(arrayValue[i], 10 - i);
    }

    arrayValueString = config.get<std::vector<std::string>>("module1/submodule/stringArray");
    BOOST_CHECK_EQUAL(arrayValueString.size(), 8);
    for(size_t i = 0; i < 8; ++i) {
      BOOST_CHECK_EQUAL(arrayValueString[i], "Hallo" + std::to_string(i + 1));
    }

    // app.config.virtualise().dump();
    // app.config.connectTo(app.testModule);

    // Cheap way to get a PV manager
    ctk::TestFacility tf{app, false};
    tf.runApplication();

    // wait until tests in TestModule::mainLoop() are complete
    while(!app.testModule.done) {
      usleep(10000);
    }
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testExceptions) {
    std::cout << "==> testExceptions" << std::endl;
    {
      BOOST_CHECK_THROW(std::make_unique<TestApplicationTwoConfigs>(), ctk::logic_error);
    }
    {
      BOOST_CHECK_THROW(std::make_unique<TestApplicationInvalidConfig>(), ctk::logic_error);
    }
    {
      TestApplication app;
      auto& config = app.getConfigReader();
      // Test get with types mismatch
      BOOST_CHECK_THROW(config.get<uint16_t>("var32u"), ctk::logic_error);

      // Test getting nonexisting varibale
      BOOST_CHECK_THROW(config.get<int>("nonexistentVariable"), ctk::logic_error);

      // Same for arrays
      // Test get with types mismatch
      BOOST_CHECK_THROW(config.get<std::vector<float>>("module1/submodule/intArray"), ctk::logic_error);

      // Test getting nonexisting varibale
      BOOST_CHECK_THROW(config.get<std::vector<int>>("nonexistentVariable"), ctk::logic_error);
    }
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDirectWriteToDevice) {
    std::cout << "==> testDirectWriteToDevice" << std::endl;
    TestApplicationWithDevice app;
    ctk::TestFacility test(app);
    test.runApplication();

    BOOST_TEST(app.getConfigReader().getOwner() != nullptr);

    ctk::Device device(cdd.data());

    auto var32u = device.getScalarRegisterAccessor<uint32_t>("var32u");
    auto var16 = device.getScalarRegisterAccessor<int16_t>("var16");
    auto module1var16 = device.getScalarRegisterAccessor<int16_t>("module1/var16");
    auto intArray = device.getOneDRegisterAccessor<int32_t>("intArray");
    var32u.read();
    var16.read();
    module1var16.read();
    intArray.read();
    BOOST_CHECK_EQUAL(var32u, 234567);
    BOOST_CHECK_EQUAL(var16, -567);
    BOOST_CHECK_EQUAL(module1var16, -567);
    BOOST_CHECK_EQUAL(intArray.getNElements(), 10);
    for(unsigned int i = 0; i < 10; ++i) {
      BOOST_CHECK_EQUAL(intArray[i], 10 - i);
    }
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testGetModules) {
    TestApplication app;
    auto& config = app.getConfigReader();

    auto modules = config.getModules();
    BOOST_TEST(std::list<std::string>({"module1", "module2"}) == modules);

    auto modules2 = config.getModules("module1");
    BOOST_TEST(std::list<std::string>({"submodule"}) == modules2);

    BOOST_TEST(config.getModules("this/should/not/exist") == std::list<std::string>());
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOverrideTestFacility) {
    std::cout << "==> testOverrideTestFacility" << std::endl;
    {
      // Case 1: Config file exists
      ctk::TestFacility::setConfigScalar<int8_t>("var8", 12); // override existing scalar value
      ctk::TestFacility::setConfigScalar<std::string>("varString", "another overridden value");

      ctk::TestFacility::setConfigScalar<int8_t>("newVar8", -42); // add new scalar

      std::vector<int> ref({1, 2, 4, 8, 16, 32, 64, 128, 256, 512});
      ctk::TestFacility::setConfigArray<int>("module1/submodule/intArray", ref);

      TestApplication app;
      auto& config = app.getConfigReader();

      BOOST_TEST(config.get<int8_t>("var8") == 12);
      BOOST_TEST(config.get<uint8_t>("var8u") == 34); // not overridden
      BOOST_TEST(config.get<std::string>("varString") == "another overridden value");
      BOOST_TEST(config.get<int8_t>("newVar8") == -42);

      auto arrayValue = config.get<std::vector<int>>("module1/submodule/intArray");
      BOOST_TEST(arrayValue == ref);
    }
    {
      // Case 2: Config file does not exist exists
      ctk::TestFacility::setConfigScalar<int8_t>("var8", 12); // override existing scalar value
      ctk::TestFacility::setConfigScalar<std::string>("varString", "another overridden value");

      ctk::TestFacility::setConfigScalar<int8_t>("newVar8", -42); // add new scalar

      std::vector<int> ref({1, 2, 4, 8, 16, 32, 64, 128, 256, 512});
      ctk::TestFacility::setConfigArray<int>("module1/submodule/intArray", ref);

      TestApplication app("AppWithoutConfigFile");
      auto& config = app.getConfigReader();

      BOOST_TEST(config.get<int8_t>("var8") == 12);
      BOOST_CHECK_THROW(config.get<uint8_t>("var8u"), ChimeraTK::logic_error); // not overridden
      BOOST_TEST(config.get<std::string>("varString") == "another overridden value");
      BOOST_TEST(config.get<int8_t>("newVar8") == -42);

      auto arrayValue = config.get<std::vector<int>>("module1/submodule/intArray");
      BOOST_TEST(arrayValue == ref);
    }
  }

  /********************************************************************************************************************/

} // namespace Tests::testConfigReader
