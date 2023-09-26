// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <future>

#define BOOST_TEST_MODULE testUserInputValidator

#include "Application.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "UserInputValidator.h"

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/*********************************************************************************************************************/
/* Test module with a single validated input *************************************************************************/
/*********************************************************************************************************************/

struct ModuleA : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInputWB<int> in1{
      this, "in1", "", "First validated input", {std::string(ctk::UserInputValidator::tagValidatedVariable)}};
  ctk::ScalarOutputPushRB<int> out1{this, "out1", "", "First output"};

  void mainLoop() override {
    ctk::UserInputValidator validator(this);
    validator.setErrorFunction([&](const std::string& message) { std::cout << message << std::endl; });

    validator.add(
        "in1 needs to be smaller then 10", [&] { return in1 < 10; }, in1);

    validator.finalise();
    auto group = readAnyGroup();
    ctk::TransferElementID change;
    while(true) {
      std::cout << change << " " << in1 << " " << out1 << std::endl;
      validator.validate(change);
      out1.writeIfDifferent(in1);
      change = group.readAny();
    }
  }
};

/*********************************************************************************************************************/
/* Second-level test module connecting to the output of ModuleA ******************************************************/
/*********************************************************************************************************************/

struct ModuleB : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInputWB<int> in1{
      this, "/ModuleA/out1", "", "First validated input", {std::string(ctk::UserInputValidator::tagValidatedVariable)}};
  ctk::ScalarOutput<int> out1{this, "out1", "", "First output"};

  void mainLoop() override {
    auto group = readAnyGroup();
    ctk::TransferElementID change;
    ctk::UserInputValidator validator(this);
    validator.setErrorFunction([&](const std::string& message) { std::cout << message << std::endl; });

    validator.add(
        "in1 needs to be smaller then 10", [&] { return in1 < 5; }, in1);
    validator.finalise();
    while(true) {
      validator.validate(change);
      out1.writeIfDifferent(in1);
      change = group.readAny();
    }
  }
};
/*********************************************************************************************************************/
/* Test dummy application for the InputValidator *********************************************************************/
/*********************************************************************************************************************/

struct TestApplication : public ctk::Application {
  using ctk::Application::Application;
  ~TestApplication() override { shutdown(); }

  ModuleA moduleA{this, "ModuleA", ""};
  ModuleB moduleB{this, "ModuleB", ""};
};

/*********************************************************************************************************************/
/* Test cases ********************************************************************************************************/
/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testSingleVariable) {
  std::cout << "testSingleVariable" << std::endl;

  TestApplication app("TestApp");
  ctk::TestFacility test(app);

  auto modAin1 = test.getScalar<int>("/ModuleA/in1");

  test.runApplication();

  modAin1.setAndWrite(8);
  test.stepApplication();
  BOOST_TEST(!modAin1.readLatest());
  BOOST_TEST(app.moduleA.in1 == 8);

  modAin1.setAndWrite(10);
  test.stepApplication();
  BOOST_TEST(modAin1.readLatest());
  BOOST_TEST(modAin1 == 8);
  BOOST_TEST(app.moduleA.in1 == 8);
}

/*********************************************************************************************************************/