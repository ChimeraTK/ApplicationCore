#define BOOST_TEST_MODULE testDirectDeviceToCS

#include <boost/test/included/unit_test.hpp>

#include "Application.h"
#include "ApplicationModule.h"
#include "ControlSystemModule.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

struct MyCs : public ctk::ControlSystemModule {
  using ctk::ControlSystemModule::ControlSystemModule;

  using ctk::ControlSystemModule::variables;
};

struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  using Application::makeConnections;

  void defineConnections() override {}

  MyCs cs1;
  MyCs cs2;

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
  TestApplication app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // Connect a push input to CS1 and a poll output to CS2 for the same variable, reverse order.
  app.module1.data.input >> app.cs1("Foo");
  app.module2.data.input >> app.cs1("Foo");
  app.initialise();
  app.run();
}

BOOST_AUTO_TEST_CASE(testNetworkMerging3) { // Test merging works if if going in different directions
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
  TestApplication app;

  auto pvManagers = ctk::createPVManager();
  app.setPVManager(pvManagers.second);

  // Connect a push input to CS1 and a poll output to CS2 for the same variable, reverse order.
  app.module1.data.output >> app.cs1("Foo");
  app.module2.data.output >> app.cs2("Foo");
  app.initialise();
  app.run();
}
