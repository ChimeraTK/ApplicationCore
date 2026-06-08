// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testPython

#include "Application.h"
#include "ConfigReader.h"
#include "DeviceModule.h"
#include "TestFacility.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

// #define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
// #undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

namespace Tests::testPython {

  /********************************************************************************************************************/

  struct TestApp : public ctk::Application {
    explicit TestApp(const std::string& name) : ctk::Application(name) {}
    ~TestApp() override { shutdown(); }
  };

  /********************************************************************************************************************/
  /* Very simple test with single Python module and nothing else */

  BOOST_AUTO_TEST_CASE(testPythonModule) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testPythonModule" << std::endl;

    TestApp app("testPythonSimpleApp");
    ctk::TestFacility tf(app);

    auto var1 = tf.getScalar<float>("/Var1");
    auto var2 = tf.getScalar<int32_t>("/Var2");

    tf.runApplication();

    var2.setAndWrite(42);
    tf.stepApplication();
    BOOST_TEST(var1.readNonBlocking());
    BOOST_TEST(float(var1) == 42.5, boost::test_tools::tolerance(0.001));
  }

  /********************************************************************************************************************/
  /* Test initial values */

  BOOST_AUTO_TEST_CASE(testInitialValues) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testInitialValues" << std::endl;

    TestApp app("testPythonSimpleApp");
    ctk::TestFacility tf(app);

    auto var1 = tf.getScalar<float>("/Var1");
    auto var2 = tf.getScalar<int32_t>("/Var2");

    tf.setScalarDefault<int32_t>("/Var2", 10);

    tf.runApplication();

    BOOST_TEST(float(var1) == 0.5, boost::test_tools::tolerance(0.001));
  }

  /********************************************************************************************************************/
  /* Test arrays */

  BOOST_AUTO_TEST_CASE(testArrays) {
    std::vector<int32_t> ref(10);

    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testArrays" << std::endl;

    TestApp app("testPythonWithArray");
    ctk::TestFacility tf(app);

    auto ArrayIn1 = tf.getArray<int32_t>("/SomeName/ArrayIn1");
    auto ArrayIn2 = tf.getArray<int32_t>("/SomeName/ArrayIn2");
    auto ArrayInPoll = tf.getArray<int32_t>("/SomeName/ArrayInPOLL");
    auto ArrayInWB = tf.getArray<int32_t>("/SomeName/ArrayInWB");
    auto ArrayOut1 = tf.getArray<int32_t>("/SomeName/ArrayOut1");
    auto ArrayOut2 = tf.getArray<int32_t>("/SomeName/ArrayOut2");
    auto error = tf.getScalar<std::string>("/SomeName/TestError");
    auto error2 = tf.getScalar<std::string>("/Foo/TestError");

    // set initial value
    tf.setArrayDefault<int32_t>("/SomeName/ArrayIn1", {50, 5});

    tf.runApplication();

    ArrayInPoll = {42, 1};
    ArrayInPoll.write();
    ArrayInPoll = {43, 2};
    ArrayInPoll.write();

    ArrayInWB = {15};
    ArrayInWB.write();

    // check initial value
    for(int i = 0; i < 10; ++i) ref[i] = 50 + 5 + i;
    BOOST_TEST(std::vector<int32_t>(ArrayOut1) == ref, boost::test_tools::per_element());
    // test readAndGet()
    ArrayIn2 = {2, 3, 4, 5, 6};
    ArrayIn2.write();
    tf.stepApplication();
    BOOST_TEST(ArrayOut2.readNonBlocking());
    for(int i = 0; i < 10; ++i) ref[i] = 2 + 3 + 4 + 5 + 6 + i;
    BOOST_TEST(std::vector<int32_t>(ArrayOut2) == ref, boost::test_tools::per_element());

    BOOST_TEST(ArrayInWB.readNonBlocking());
    BOOST_TEST(std::vector<int32_t>(ArrayInWB) == std::vector<int32_t>({28}));

    // test read() -> get()
    ArrayIn1 = {100, 20};
    ArrayIn1.write();
    tf.stepApplication();
    BOOST_TEST(ArrayOut1.readNonBlocking());
    for(int i = 0; i < 10; ++i) ref[i] = 100 + 20 + i;
    BOOST_TEST(std::vector<int32_t>(ArrayOut1) == ref, boost::test_tools::per_element());

    // check result of the Python-side tests
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");
    BOOST_TEST(error2.readNonBlocking() == false);
    BOOST_TEST(std::string(error2) == "");
  }

  /********************************************************************************************************************/
  /* Test scalars */

  BOOST_AUTO_TEST_CASE(testScalars) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testScalars" << std::endl;

    TestApp app("testPythonScalarAccessors");
    ctk::TestFacility tf(app);

    auto result = tf.getScalar<std::string>("/Test/Result");

    tf.runApplication();

    result.readNonBlocking();
    BOOST_TEST(std::string(result) == "Scalar test did not produce any Python Errors");
  }

  /********************************************************************************************************************/
  /* Test appConfig group */

  BOOST_AUTO_TEST_CASE(testAppConfig) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testAppConfig" << std::endl;

    TestApp app("testPythonAppConfig");
    ctk::TestFacility tf(app);
    auto result = tf.getScalar<std::string>("/UserModule/testError");

    tf.runApplication();
    result.readLatest();
    BOOST_TEST(std::string(result) == "");
  }

  /********************************************************************************************************************/
  /* Test variable group */

  BOOST_AUTO_TEST_CASE(testVariableGroup) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testVariableGroup" << std::endl;

    TestApp app("testPythonVariableGroup");
    ctk::TestFacility tf(app);

    auto in1 = tf.getArray<int32_t>("/UserModule/VG/in1");
    auto out1 = tf.getScalar<int32_t>("/UserModule/VG/out1");
    auto out2 = tf.getArray<int32_t>("/UserModule/VG2/out2");
    auto out3 = tf.getArray<int32_t>("/UserModule/VG2/VG3/out3");
    auto result = tf.getScalar<std::string>("/UserModule/testError");

    tf.runApplication();

    out1.readLatest();
    BOOST_TEST(out1 == 1);

    in1 = {2, 3};
    in1.write();
    tf.stepApplication();
    out2.readLatest();
    BOOST_TEST(std::vector<int32_t>(out2) == std::vector<int32_t>(in1), boost::test_tools::per_element());
    out3.readLatest();
    BOOST_TEST(std::vector<int32_t>(out3) == std::vector<int32_t>(in1), boost::test_tools::per_element());
    result.readLatest();
    BOOST_TEST(std::string(result) == "");
  }

  /********************************************************************************************************************/
  /* Test module group */

  BOOST_AUTO_TEST_CASE(testModuleGroup) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testModuleGroup" << std::endl;

    TestApp app("testPythonVariableGroup");
    ctk::TestFacility tf(app);

    auto in1 = tf.getArray<int32_t>("/SomeGroup/UserModuleInGroup/VG/in1");
    auto out1 = tf.getScalar<int32_t>("/SomeGroup/UserModuleInGroup/VG/out1");
    auto out2 = tf.getArray<int32_t>("/SomeGroup/UserModuleInGroup/VG2/out2");
    auto out3 = tf.getArray<int32_t>("/SomeGroup/UserModuleInGroup/VG2/VG3/out3");
    auto result = tf.getScalar<std::string>("/SomeGroup/UserModuleInGroup/testError");

    tf.runApplication();

    out1.readLatest();
    BOOST_TEST(out1 == 1);

    in1 = {2, 3};
    in1.write();
    tf.stepApplication();
    out2.readLatest();
    BOOST_TEST(std::vector<int32_t>(out2) == std::vector<int32_t>(in1), boost::test_tools::per_element());
    out3.readLatest();
    BOOST_TEST(std::vector<int32_t>(out3) == std::vector<int32_t>(in1), boost::test_tools::per_element());
    result.readLatest();
    BOOST_TEST(std::string(result) == "");
  }

  /********************************************************************************************************************/
  /* Test ApplicationModule */

  BOOST_AUTO_TEST_CASE(testApplicationModule) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testApplicationModule" << std::endl;

    TestApp app("testPythonApplicationModule");
    ctk::TestFacility tf(app);

    auto result = tf.getScalar<std::string>("/UserModule/testError");

    tf.runApplication();

    auto list = app.getSubmoduleList();
    for(auto* mod : list) {
      if(mod->getName() == "DisabledMod") {
        // This was disabled in python and should not show up here at all
        BOOST_CHECK(false);
      }
    }

    BOOST_CHECK(!result.readNonBlocking());
    BOOST_TEST(std::string(result) == "");
  }

  /********************************************************************************************************************/
  /* Test DataConsistencyGroup */

  BOOST_AUTO_TEST_CASE(testDataConsistencyGroup) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testDataConsistencyGroup" << std::endl;

    TestApp app("testPythonDataConsistencyGroup");

    ctk::TestFacility tf(app);

    auto result = tf.getScalar<std::string>("/UserModule/testError");

    tf.runApplication();

    result.readLatest();
    BOOST_TEST(std::string(result) == "ok");
  }

  /********************************************************************************************************************/

  struct TestAppReadAny : public ctk::Application {
    TestAppReadAny(const std::string& name) : ctk::Application(name) {}
    ~TestAppReadAny() override { shutdown(); }
  };

  BOOST_AUTO_TEST_CASE(testReadAnyGroup) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testReadAnyGroup" << std::endl;

    TestAppReadAny app("testPythonReadAnyGroup");
    ctk::TestFacility tf(app);

    auto result = tf.getScalar<std::string>("/UserModule/testError");
    auto in1 = tf.getScalar<int32_t>("/UserModule/in1");
    auto in2 = tf.getArray<int32_t>("/UserModule/in2");
    auto in3 = tf.getScalar<int32_t>("/UserModule/in3");
    auto out = tf.getScalar<std::string>("/UserModule/output");

    tf.runApplication();

    in1.setAndWrite(12);
    tf.stepApplication();
    BOOST_CHECK(out.readNonBlocking());
    BOOST_TEST(std::string(out) == "step1");

    in2 = {24, 24, 24, 24};
    in2.write();
    tf.stepApplication();
    BOOST_CHECK(out.readNonBlocking());
    BOOST_CHECK(std::string(out) == "step2");

    in3.setAndWrite(36);
    tf.stepApplication();
    BOOST_CHECK(out.readNonBlocking());
    BOOST_CHECK(std::string(out) == "step3");

    in1.setAndWrite(8);
    tf.stepApplication();
    BOOST_CHECK(out.readNonBlocking());
    BOOST_CHECK(std::string(out) == "step4");

    in2 = {16, 16, 16, 16};
    in2.write();
    tf.stepApplication();
    BOOST_CHECK(out.readNonBlocking());
    BOOST_CHECK(std::string(out) == "step5");

    in1.setAndWrite(13);
    tf.stepApplication();
    BOOST_CHECK(!out.readNonBlocking());

    in2 = {26, 26, 26, 26};
    in2.write();
    tf.stepApplication();
    BOOST_CHECK(out.readNonBlocking());
    BOOST_CHECK(std::string(out) == "step6");

    in1.setAndWrite(42);
    tf.stepApplication();
    BOOST_CHECK(out.readNonBlocking());
    BOOST_CHECK(std::string(out) == "step7");

    BOOST_CHECK(!result.readNonBlocking());
    BOOST_TEST(std::string(result) == "");
  }

  /********************************************************************************************************************/

  struct TestAppVersionNumber : public ctk::Application {
    explicit TestAppVersionNumber(const std::string& name) : ctk::Application(name) {}
    ~TestAppVersionNumber() override { shutdown(); }
  };

  /********************************************************************************************************************/
  /* Very simple test with single Python module and nothing else */

  BOOST_AUTO_TEST_CASE(testPythonVersionNumber) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testPythonVersionNumber" << std::endl;

    TestAppVersionNumber app("testPythonVersionNumber");
    ctk::TestFacility tf(app);

    tf.runApplication();

    // check result of the Python-side tests
    auto error = tf.getScalar<std::string>("/VersionTestRunner/TestError");
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");
  }

  /********************************************************************************************************************/

  struct TestAppUserInputValiador : public ctk::Application {
    explicit TestAppUserInputValiador(const std::string& name) : ctk::Application(name) {}
    ~TestAppUserInputValiador() override { shutdown(); }
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testPythonUserInputValidator) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testPythonUserInputValidator" << std::endl;

    TestAppUserInputValiador app("testPythonUserInputValidator");
    ctk::TestFacility tf(app);

    tf.setScalarDefault("/UserInputValidatorTestRunner/in1", 12);
    tf.setArrayDefault("/UserInputValidatorTestRunner/in2", std::vector<int>{10, 10, 10, 10, 10});

    auto input = tf.getScalar<int>("/UserInputValidatorTestRunner/in1");
    auto input2 = tf.getArray<int>("/UserInputValidatorTestRunner/in2");
    auto errorFunctionCalled = tf.getVoid("/UserInputValidatorTestRunner/errorFunctionCalled");

    tf.runApplication();

    // The initial values were wrong and were corrected
    auto error = tf.getScalar<std::string>("/UserInputValidatorTestRunner/TestError");
    // Should have two values in queue because both two validators failed
    BOOST_TEST(errorFunctionCalled.readNonBlocking() == true);
    BOOST_TEST(errorFunctionCalled.readNonBlocking() == true);
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");

    input.setAndWrite(8);
    tf.stepApplication();
    BOOST_TEST(!input.readLatest());
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");

    input.setAndWrite(10);
    tf.stepApplication();
    BOOST_TEST(input.readLatest());
    BOOST_TEST(input == 8);
    BOOST_TEST(errorFunctionCalled.readNonBlocking() == true);
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");

    input2 = std::vector{2, 2, 2, 2, 1};
    input2.write();
    tf.stepApplication();
    BOOST_TEST(!input2.readLatest());
    BOOST_TEST(errorFunctionCalled.readNonBlocking() == false);
    BOOST_CHECK(static_cast<const std::vector<int>&>(input2) == std::vector<int>({2, 2, 2, 2, 1}));
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");

    input2 = std::vector{1, 2, 3, 4, 5};
    input2.write();
    tf.stepApplication();
    BOOST_TEST(input2.readLatest());
    BOOST_TEST(errorFunctionCalled.readNonBlocking() == true);
    BOOST_CHECK(static_cast<const std::vector<int>&>(input2) == std::vector<int>({2, 2, 2, 2, 1}));
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");

    input2 = std::vector{9, 0, 0, 0, 0};
    input2.write();
    tf.stepApplication();
    BOOST_TEST(input2.readLatest());
    BOOST_TEST(errorFunctionCalled.readNonBlocking() == true);
    BOOST_CHECK(static_cast<const std::vector<int>&>(input2) == std::vector<int>({2, 2, 2, 2, 1}));
    BOOST_TEST(error.readNonBlocking() == false);
    BOOST_TEST(std::string(error) == "");
  }

  /********************************************************************************************************************/
  /* Test accessor tags */

  BOOST_AUTO_TEST_CASE(testAccessorTags) {
    std::cout << "***************************************************************************************" << std::endl;
    std::cout << "==> testAccessorTags" << std::endl;

    TestApp app("testPythonAccessorTags");
    ctk::TestFacility tf(app);

    tf.runApplication();

    // Helper lambda to check tags on a PV via the model API
    auto checkTags = [&](const std::string& pvName, std::unordered_set<std::string> expectedTags) {
      bool found = app.getModel().visitByPath(pvName, [&](auto proxy) {
        if constexpr(ctk::Model::isVariable(proxy)) {
          auto actualTags = proxy.getTags();
          BOOST_TEST(actualTags == expectedTags, "PV " << pvName << ": expected tags don't match actual tags");
        }
        else {
          BOOST_FAIL("Expected a variable at path " << pvName);
        }
      });
      BOOST_TEST(found, "PV not found: " << pvName);
    };

    // Test scalar accessors
    checkTags("/TagTestModule/sOut", {"scalarOutput"});
    checkTags("/TagTestModule/sIn", {"scalarPushInput"});
    checkTags("/TagTestModule/sInWB", {"scalarPushInputWB"});
    checkTags("/TagTestModule/sPoll", {"scalarPollInput"});
    checkTags("/TagTestModule/sPushRB", {"scalarOutputPushRB"});
    checkTags("/TagTestModule/sRevRec", {"scalarRevRec", "_ChimeraTK_DeviceRegister_reverseRecovery"});

    // Test array accessors
    checkTags("/TagTestModule/aOut", {"arrayOutput"});
    checkTags("/TagTestModule/aIn", {"arrayPushInput"});
    checkTags("/TagTestModule/aInWB", {"arrayPushInputWB"});
    checkTags("/TagTestModule/aPoll", {"arrayPollInput"});
    checkTags("/TagTestModule/aPushRB", {"arrayOutputPushRB"});
    checkTags("/TagTestModule/aRevRec", {"arrayRevRec", "_ChimeraTK_DeviceRegister_reverseRecovery"});

    // Test no tags (default empty set)
    checkTags("/TagTestModule/sNoTags", {});

    // Test accessor created with DataType.TheType variant, testing two tags
    checkTags("/TagTestModule/sTwoTags", {"tagA", "tagB"});
  }

  /********************************************************************************************************************/
  /* Test file monitoring / automatic reload of Python modules */

  BOOST_AUTO_TEST_CASE(testFileMonitoring) {
    // Ensure the build copy of the Python source file is pristine before creating the
    // Application (which loads it). A previous test run may have corrupted it if atexit
    // didn't fire (e.g. on crash or when the test binary was rebuilt).
    // The expected initial source has val + 0.5 (not val + 1.0).
    // We hardcode the expected source to avoid depending on paths to the source tree.
    static const std::string pristineSource = R"python(
#!/usr/bin/python3

import sys
import os
import os.path
import time

# fmt: off
# Hack to insert the python path for the locally compiled module in the
# test script
sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore  # NOQA
# fmt: on

class MyMod(PyApplicationCore.ApplicationModule) :

  def __init__(self, owner, name, description) :
    super().__init__(owner, name, description)
    self.myOutput = PyApplicationCore.ScalarOutput(PyApplicationCore.DataType.float32, self, "/Var1" ,"unit", "description")
    self.myInput = PyApplicationCore.ScalarPushInput(PyApplicationCore.DataType.int32, self, "/Var2","unit", "description")

  def mainLoop(self) :
    val = 0
    while True:
      self.myOutput.setAndWrite(val + 0.5)
      val = self.myInput.readAndGet()


# create instance of test module
PyApplicationCore.app.myMod = MyMod(PyApplicationCore.app, "SomeName", "Description")
)python";

    std::string pyFilePath = std::filesystem::absolute("testPythonFileMonitor.py").string();
    {
      std::ifstream buildFile(pyFilePath);
      std::stringstream buildBuf;
      buildBuf << buildFile.rdbuf();
      buildFile.close();
      if(buildBuf.str() != pristineSource) {
        std::ofstream restoreFile(pyFilePath, std::ios::trunc);
        restoreFile << pristineSource;
        restoreFile.close();
      }
    }

    // Use a dedicated app struct that lets us set the file monitoring interval before createModules
    struct FileMonitorApp : public ctk::Application {
      explicit FileMonitorApp(const std::string& name) : ctk::Application(name) {
        getPythonModuleManager().setFileMonitoringCheckInterval(std::chrono::milliseconds(100));
      }
      ~FileMonitorApp() override { shutdown(); }
    };

    FileMonitorApp app("testPythonFileMonitor");
    ctk::TestFacility tf(app);

    auto var1 = tf.getScalar<float>("/Var1");
    auto var2 = tf.getScalar<int32_t>("/Var2");

    tf.runApplication();

    // Confirm the pyFilePath from the config matches our pre-computed path
    auto& config = app.getConfigReader();
    auto modules = config.getModules("PythonModules");
    std::string modulePath;
    for(auto& module : modules) {
      modulePath = config.get<std::string>("PythonModules/" + module + "/path");
      BOOST_REQUIRE(pyFilePath == std::filesystem::absolute(modulePath + ".py").string());
    }
    BOOST_REQUIRE(std::filesystem::exists(pyFilePath));

    // Now we know the file is pristine. Read the original source to restore later.
    std::ifstream origFile(pyFilePath);
    std::stringstream origBuf;
    origBuf << origFile.rdbuf();
    std::string originalSource = origBuf.str();
    origFile.close();

    // Verify initial behavior: Var1 = Var2 + 0.5
    var2 = 42;
    var2.write();
    tf.stepApplication();
    BOOST_TEST(var1.readNonBlocking());
    BOOST_TEST(float(var1) == 42.5, boost::test_tools::tolerance(0.001));

    // Create new module source with modified behavior: output = val + 1.0 instead of val + 0.5
    std::string newSource = R"python(
#!/usr/bin/python3

import sys
import os
import os.path
import time

sys.path.insert(0, os.path.abspath(os.path.join(os.curdir, "..")))
import PyApplicationCore  # NOQA

class MyMod(PyApplicationCore.ApplicationModule) :

  def __init__(self, owner, name, description) :
    super().__init__(owner, name, description)
    self.myOutput = PyApplicationCore.ScalarOutput(PyApplicationCore.DataType.float32, self, "/Var1" ,"unit", "description")
    self.myInput = PyApplicationCore.ScalarPushInput(PyApplicationCore.DataType.int32, self, "/Var2","unit", "description")

  def mainLoop(self) :
    val = 0
    while True:
      self.myOutput.setAndWrite(val + 1.0)
      val = self.myInput.readAndGet()

PyApplicationCore.app.myMod = MyMod(PyApplicationCore.app, "SomeName", "Description")
)python";

    // Use atexit to restore the original file even if Boost.Test calls exit()
    // (which skips destructors of automatic objects).
    // We store the data in static pointers accessed by a captureless lambda,
    // which can convert to void(*)() for std::atexit.
    static std::shared_ptr<std::string> restoreGuard;
    static std::shared_ptr<std::string> restoreFilePath;
    restoreGuard = std::make_shared<std::string>(originalSource);
    restoreFilePath = std::make_shared<std::string>(pyFilePath);
    std::atexit([]() {
      std::ofstream pyFile(*restoreFilePath, std::ios::trunc);
      pyFile << *restoreGuard;
      pyFile.close();
    });

    // Write the modified source
    {
      std::ofstream pyFile(pyFilePath, std::ios::trunc);
      pyFile << newSource;
      pyFile.close();
    }

    // Wait for the file monitoring thread to detect the change.
    // The file monitoring thread starts with the default 1000ms check interval in the Application
    // constructor (before we can set it to 100ms in FileMonitorApp's constructor body).
    // After the first check (at ~1000ms), subsequent checks use the configured 100ms interval.
    // Note: The 1.5s sleep should allow at least one check at 1000ms and one at 1100ms.
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Verify the module was reloaded and the new code is running: Var1 should now be Var2 + 1.0
    var2 = 10;
    var2.write();
    tf.stepApplication();
    // The first read consumes the intermediate value written before the input was read.
    var1.readNonBlocking();
    // The second read gets the value after the input was processed (output = input + 1.0).
    BOOST_TEST(var1.readNonBlocking());
    BOOST_TEST(float(var1) == 11.0, boost::test_tools::tolerance(0.001));
  }

  /********************************************************************************************************************/

} // namespace Tests::testPython
