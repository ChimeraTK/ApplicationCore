// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testDirectDeviceToCS

#include "Application.h"
#include "ApplicationModule.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

BOOST_AUTO_TEST_CASE(ALL_TESTS_DISABLED) {}

#if 0

struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  void defineConnections() override {}

  ctk::ControlSystemModule cs1;
  ctk::ControlSystemModule cs2;

  struct IO : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;

      ctk::ScalarPushInput<int> input{this, "input", "", ""};
      ctk::ScalarOutput<int> output{this, "output", "", ""};
    } data{this, "data", ""};

    void mainLoop() {
      while(true) {
        data.output = 1 * data.input;
        writeAllDestructively();
        readAll();
      }
    }
  };

  IO module1{this, "M1", ""};
  IO module2{this, "M2", ""};
};

BOOST_AUTO_TEST_CASE(testNetworkMerging1) { // Test merging works if if going in different directions
  std::cout << "testNetworkMerging1" << std::endl;
  TestApplication app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // Connect a push input to CS1 and a poll output to CS2 for the same variable.
  app.module1.data.input >> app.cs1("Foo");
  app.module2.data.output >> app.cs2("Foo");
  app.initialise();
  app.run();
}

BOOST_AUTO_TEST_CASE(testNetworkMerging2) { // Test merging works if if going in different directions
  std::cout << "testNetworkMerging2" << std::endl;
  TestApplication app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // Connect a push input to CS1 and a poll output to CS2 for the same variable, reverse order.
  app.cs1("Foo") >> app.module1.data.input;
  app.cs2("Foo") >> app.module2.data.input;
  app.initialise();
  app.run();
}

BOOST_AUTO_TEST_CASE(testNetworkMerging3) { // Test merging works if if going in different directions
  std::cout << "testNetworkMerging3" << std::endl;
  TestApplication app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // Connect a push input to CS1 and a poll output to CS2 for the same variable, reverse order.
  app.cs1("Foo") >> app.module1.data.input;
  app.module2.data.output >> app.cs2("Foo");
  app.initialise();
  app.run();
}

BOOST_AUTO_TEST_CASE(testNetworkMerging4) { // Test merging works if if going in different directions
  std::cout << "testNetworkMerging4" << std::endl;
  TestApplication app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // Connect a push input to CS1 and a poll output to CS2 for the same variable, reverse order.
  app.module1.data.output >> app.cs1("Foo");
  BOOST_CHECK_THROW(app.module2.data.output >> app.cs2("Foo"), ChimeraTK::logic_error);
}

#endif
