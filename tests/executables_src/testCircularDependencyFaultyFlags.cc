#define BOOST_TEST_MODULE testPropagateDataFaultFlag

#include <boost/test/included/unit_test.hpp>

#include "ApplicationModule.h"
#include "VariableGroup.h"
#include "ScalarAccessor.h"
#include "ControlSystemModule.h"
#include "TestFacility.h"
namespace ctk = ChimeraTK;

// The basic setup has 4 modules connected in a circle

//// The base module has the inputs and outputs for the circular dependency
struct TestModuleBase : ctk::ApplicationModule {
  struct /*InputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPushInput<int> circularInput1{this, "circularOutput1", "", ""};
    ctk::ScalarPushInput<int> circularInput2{this, "circularOutput2", "", ""};
  } inputGroup;

  ctk::ScalarOutput<int> circularOutput1{this, "circularOutput1", "", ""};
  ctk::ScalarOutput<int> circularOutput2{this, "circularOutput2", "", ""};

  TestModuleBase(const std::string& inputName, EntityOwner* owner, const std::string& name,
      const std::string& description, ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none)
  : ApplicationModule(owner, name, description, hierarchyModifier),
    inputGroup(this, inputName, "", ctk::HierarchyModifier::oneLevelUp) {}

  void mainLoop() override {
    while(true) {
      circularOutput1 = static_cast<int>(inputGroup.circularInput1);
      circularOutput2 = static_cast<int>(inputGroup.circularInput2);
      writeAll();
      readAll();
    }
  }
};

/// ModuleA has two additonal inputs to get invalidity flags. It is reading all inputs with ReadAny
struct ModuleA : TestModuleBase {
  using TestModuleBase::TestModuleBase;

  ctk::ScalarPushInput<int> a{this, "a", "", ""};
  ctk::ScalarPushInput<int> b{this, "b", "", ""};

  void prepare() override { writeAll(); }

  void mainLoop() override {
    auto rag = readAnyGroup();

    while(true) {
      rag.readAny(); // we con't care which input has been read. Just update all content

      circularOutput1 = static_cast<int>(inputGroup.circularInput1) + a;
      circularOutput2 = static_cast<int>(inputGroup.circularInput2) + b;

      writeAll();
    }
  }
};

/// ModuleC has a trigger together with a readAll.; (it's a trigger for the circle because there is always something at the circular inputs)
struct ModuleC : TestModuleBase {
  using TestModuleBase::TestModuleBase;
  ctk::ScalarPushInput<int> trigger{this, "trigger", "", ""};

  // no need to override the main loop again. Nothing to do with the data content of the trigger, and readAll() considers it.
};

struct TestApplication1 : ctk::Application {
  TestApplication1() : Application("testSuite") {}
  ~TestApplication1() { shutdown(); }

  void defineConnections() { findTag(".*").connectTo(cs); }

  ModuleA A{"D", this, "A", ""};
  TestModuleBase B{"A", this, "B", ""};
  ModuleC C{"B", this, "C", ""};
  TestModuleBase D{"C", this, "D", ""};

  ctk::ControlSystemModule cs;
};

BOOST_AUTO_TEST_CASE(TestCircularInputDetection) {
  TestApplication1 app;
  ctk::TestFacility test;

  test.runApplication();

  // just test that the circular inputs have been detected correctly
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.inputGroup.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.a).isCircularInput() == false);
  // Check that the circular outputs are not marked as circular inputs. They are in the circle, but they are not inputs.
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.circularOutput2).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.inputGroup.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.circularOutput2).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.inputGroup.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.trigger).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.circularOutput2).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.inputGroup.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.circularOutput2).isCircularInput() == false);
}
