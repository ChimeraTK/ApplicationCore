// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>

#include <boost/mpl/list.hpp>

namespace ctk = ChimeraTK;

#define BOOST_TEST_MODULE testConnectedDestructionRunner
#include <boost/test/included/unit_test.hpp>

// This test is meant to be run manual, each test individually since all of them terminate the process

/*********************************************************************************************************************/

struct ScalarOutputModule : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarOutput<int> output{this, "/variable1", "", ""};
  void mainLoop() override {}
};

/*********************************************************************************************************************/

struct ArrayOutputModule : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ArrayOutput<int> output{this, "/variable1", "", 10, ""};
  void mainLoop() override {}
};

/*********************************************************************************************************************/

struct ScalarInputModule : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<int> input{this, "/variable1", "", ""};
  void mainLoop() override {}
};

/*********************************************************************************************************************/

struct ArrayInputModule : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ArrayPushInput<int> input{this, "/variable1", "", 10, ""};
  void mainLoop() override {}
};

/*********************************************************************************************************************/

struct ScalarApplicationModuleTestApp : ctk::Application {
  ScalarApplicationModuleTestApp() : Application("theApp") {
    mod = std::make_unique<ScalarOutputModule>(this, "mod1", "");
  }
  ~ScalarApplicationModuleTestApp() override { shutdown(); }

  std::unique_ptr<ScalarOutputModule> mod;
  ScalarInputModule mod2{this, "mod2", ""};
};

/*********************************************************************************************************************/

struct ArrayApplicationModuleTestApp : ctk::Application {
  ArrayApplicationModuleTestApp() : Application("theApp") {
    mod = std::make_unique<ArrayOutputModule>(this, "mod1", "");
  }
  ~ArrayApplicationModuleTestApp() override { shutdown(); }

  std::unique_ptr<ArrayOutputModule> mod;
  ArrayInputModule mod2{this, "mod2", ""};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDestroyConnectedApplicationModule) {
  std::cout << "*** testDestroyConnectedApplicationModule" << std::endl;
  ScalarApplicationModuleTestApp app;

  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.mod.reset();
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDestroyConnectedApplicationModuleWithArray) {
  std::cout << "*** testDestroyConnectedApplicationModuleWithArray" << std::endl;
  ArrayApplicationModuleTestApp app;

  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.mod.reset();
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMoveConnectedApplicationModule) {
  std::cout << "*** testDestroyMovedApplicationModule" << std::endl;
  ScalarApplicationModuleTestApp app;

  ScalarInputModule mod(&app, "test", "");
  ctk::TestFacility tf{app, false};
  tf.runApplication();
  ScalarInputModule moved(std::move(mod));
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMoveConnectedApplicationModuleWithArray) {
  std::cout << "*** testDestroyMovedApplicationModuleWithArray" << std::endl;
  ArrayApplicationModuleTestApp app;

  ArrayInputModule mod(&app, "test", "");
  ctk::TestFacility tf{app, false};
  tf.runApplication();
  ArrayInputModule moved(std::move(mod));
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

struct ScalarVariableGroup : public ctk::VariableGroup {
  using ctk::VariableGroup::VariableGroup;

  ctk::ScalarPushInput<int> output{this, "/variable1", "", ""};
};

/*********************************************************************************************************************/

struct ScalarVariableGroupTestApp : ctk::Application {
  ScalarVariableGroupTestApp() : Application("theApp") {
    mod2.group = std::make_unique<ScalarVariableGroup>(&mod2, "group", "");
  }

  ScalarInputModule mod{this, "mod", ""};
  struct : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    std::unique_ptr<ScalarVariableGroup> group;
    void mainLoop() override {}
  } mod2{this, "mod", ""};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDestroyConnectedVariableGroupDelete) {
  std::cout << "*** testDestroyConnectedVariableGroupDelete" << std::endl;

  ScalarVariableGroupTestApp app;
  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.mod2.group.reset();
}

/*********************************************************************************************************************/

struct ArrayVariableGroup : public ctk::VariableGroup {
  using ctk::VariableGroup::VariableGroup;

  ctk::ArrayPushInput<int> output{this, "/variable1", "", 10, ""};
};

/*********************************************************************************************************************/

struct ArrayVariableGroupTestApp : ctk::Application {
  ArrayVariableGroupTestApp() : Application("theApp") {
    mod2.group = std::make_unique<ArrayVariableGroup>(&mod2, "group", "");
  }

  ArrayInputModule mod{this, "mod", ""};
  struct : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    std::unique_ptr<ArrayVariableGroup> group;
    void mainLoop() override {}
  } mod2{this, "mod", ""};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDestroyConnectedVariableGroupArrayDelete) {
  std::cout << "*** testDestroyConnectedVariableGroupArrayDelete" << std::endl;

  ArrayVariableGroupTestApp app;
  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.mod2.group.reset();
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

struct ScalarVariableGroupTestAppForMove : ctk::Application {
  ScalarVariableGroupTestAppForMove() : Application("theApp") {}

  ScalarInputModule mod{this, "mod", ""};
  struct : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ScalarVariableGroup group{this, "group", ""};
    void mainLoop() override {}
  } mod2{this, "mod", ""};
};

BOOST_AUTO_TEST_CASE(testMoveConnectedVariableGroup) {
  std::cout << "*** testMoveConnectedVariableGroup" << std::endl;

  ScalarVariableGroupTestAppForMove app;
  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.mod2.group = ScalarVariableGroup(&app.mod2, "group2", "");
}

/*********************************************************************************************************************/

struct ArrayVariableGroupTestAppForMove : ctk::Application {
  ArrayVariableGroupTestAppForMove() : Application("theApp") {}

  ArrayInputModule mod{this, "mod", ""};
  struct : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ArrayVariableGroup group{this, "group", ""};
    void mainLoop() override {}
  } mod2{this, "mod", ""};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMoveConnectedVariableGroupArray) {
  std::cout << "*** testMoveConnectedVariableGroupArray" << std::endl;

  ArrayVariableGroupTestAppForMove app;
  ctk::TestFacility tf{app, false};
  tf.runApplication();
  app.mod2.group = ArrayVariableGroup(&app.mod2, "group2", "");
}
