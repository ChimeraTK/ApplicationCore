#define BOOST_TEST_MODULE testPropagateDataFaultFlag

// Tests never terminate when an exception is caught and BOOST_NO_EXCEPTIONS is set, but the exeption's what() message
// is printed. Without the define the what() message is not printed, but the test ist not stuck... I leave it commented
// but in the code so you activate when debugging.
//#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
//#undef BOOST_NO_EXCEPTIONS

#include "ApplicationModule.h"
#include "ControlSystemModule.h"
#include "HierarchyModifyingGroup.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "VariableGroup.h"
namespace ctk = ChimeraTK;

// The basic setup has 4 modules connected in a circle

//// The base module has the inputs and outputs for the circular dependency
///
///  To test variable groups for inputs and outputs:
///  Output 1 and input 2 are always from another module, while input 1 and output 2 live in this module.
struct TestModuleBase : ctk::ApplicationModule {
  struct /*InputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPushInput<int> circularInput1{this, "circularOutput1", "", ""};
  } inputGroup;

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> circularOutput2{this, "circularInput2", "", ""};
  } outputGroup;

  ctk::ScalarPushInput<int> circularInput2{this, "circularInput2", "", ""};
  ctk::ScalarOutput<int> circularOutput1{this, "circularOutput1", "", ""};

  TestModuleBase(const std::string& inputName, const std::string& outputName, EntityOwner* owner,
      const std::string& name, const std::string& description,
      ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none)
  : ApplicationModule(owner, name, description, hierarchyModifier),
    inputGroup(this, inputName, "", ctk::HierarchyModifier::oneLevelUp),
    outputGroup(this, outputName, "", ctk::HierarchyModifier::oneLevelUp) {}

  void mainLoop() override {
    while(true) {
      circularOutput1 = static_cast<int>(inputGroup.circularInput1);
      outputGroup.circularOutput2 = static_cast<int>(circularInput2);

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
  ctk::ScalarOutput<int> circleResult{this, "circleResult", "", ""};

  void prepare() override { writeAll(); }

  void mainLoop() override {
    // The circular inputs always are both coming as a pair, but we only want to write once.
    // Hence we only put one of them into the ReadAnyGroup and always read the second one manually if the first one
    // is read by the group.
    ctk::ReadAnyGroup rag({a, b, inputGroup.circularInput1});

    while(true) {
      auto id = rag.readAny();

      // A module with circular inputs and readAny must always actively break the circle. Otherwise for each external
      // input and n-1 internal inputs and additional data element is inserted into the circle, which will let queues
      // run over and re-trigger the circle all the time.
      // This is a very typical scenario for circular connections: A module gets some input, triggers a helper module
      // which calculates a value that is read back by the first module, and then the first module continues without
      // re-triggering the circle.

      assert((id == a.getId() || id == b.getId()) || id == inputGroup.circularInput1.getId() ||
          id == circularInput2.getId());

      if(id == inputGroup.circularInput1.getId()) {
        // Read the other circular input as well. They always come in pairs.
        circularInput2.read();
      }

      if(id == a.getId() || id == b.getId()) {
        circularOutput1 = static_cast<int>(inputGroup.circularInput1) + a;
        outputGroup.circularOutput2 = static_cast<int>(circularInput2) + b;

        circularOutput1.write();
        outputGroup.circularOutput2.write();
      }
      else { // new data is from the circular inputs
        circleResult = static_cast<int>(inputGroup.circularInput1) + circularInput2;
        circleResult.write();
      }

    } // while(true)
  }   // mainLoop
};    // ModuleA

/// ModuleC has a trigger together with a readAll.; (it's a trigger for the circle because there is always something at
/// the circular inputs)
struct ModuleC : TestModuleBase {
  using TestModuleBase::TestModuleBase;
  ctk::ScalarPushInput<int> trigger{this, "trigger", "", ""};

  // Special loop to guarantee that the internal inputs are read first, so we don't have unread data in the queue and
  // can use the testable mode
  void mainLoop() override {
    while(true) {
      circularOutput1 = static_cast<int>(inputGroup.circularInput1);
      outputGroup.circularOutput2 = static_cast<int>(circularInput2);
      writeAll();
      // readAll();
      inputGroup.circularInput1.read();
      circularInput2.read();
      trigger.read();
    }
  }
};

/// Involve the DeviceModule. Here are some variables from a test device.
struct ModuleD : TestModuleBase {
  using TestModuleBase::TestModuleBase;
  ctk::ModifyHierarchy<ctk::ScalarPollInput<int>> i1{this, "/m1/i1", "", ""};
  ctk::ModifyHierarchy<ctk::ScalarPollInput<int>> i3{this, "/m1/i3", "", ""};
  ctk::ModifyHierarchy<ctk::ScalarOutput<int>> o1{this, "/m1/o1", "", ""};
};

struct TestApplication1 : ctk::Application {
  TestApplication1() : Application("testSuite") {}
  ~TestApplication1() { shutdown(); }

  ModuleA A{"D", "B", this, "A", ""}; // reads like: This is A, gets input from D and writes to B
  TestModuleBase B{"A", "C", this, "B", ""};
  ModuleC C{"B", "D", this, "C", ""};
  ModuleD D{"C", "A", this, "D", ""};

  ctk::ConnectingDeviceModule device{this, "(dummy?map=testDataValidity1.map)"};
};

template<typename APP_TYPE>
struct CircularAppTestFixcture {
  APP_TYPE app;
  ctk::TestFacility test;

  ctk::ScalarRegisterAccessor<int> a{test.getScalar<int>("A/a")};
  ctk::ScalarRegisterAccessor<int> b{test.getScalar<int>("A/b")};
  ctk::ScalarRegisterAccessor<int> C_trigger{test.getScalar<int>("C/trigger")};
  ctk::ScalarRegisterAccessor<int> A_out1{test.getScalar<int>("A/circularOutput1")};
  ctk::ScalarRegisterAccessor<int> B_out1{test.getScalar<int>("B/circularOutput1")};
  ctk::ScalarRegisterAccessor<int> C_out1{test.getScalar<int>("C/circularOutput1")};
  ctk::ScalarRegisterAccessor<int> D_out1{test.getScalar<int>("D/circularOutput1")};
  ctk::ScalarRegisterAccessor<int> A_in2{test.getScalar<int>("A/circularInput2")};
  ctk::ScalarRegisterAccessor<int> B_in2{test.getScalar<int>("B/circularInput2")};
  ctk::ScalarRegisterAccessor<int> C_in2{test.getScalar<int>("C/circularInput2")};
  ctk::ScalarRegisterAccessor<int> D_in2{test.getScalar<int>("D/circularInput2")};
  ctk::ScalarRegisterAccessor<int> circleResult{test.getScalar<int>("A/circleResult")};

  void readAllLatest() {
    A_out1.readLatest();
    B_out1.readLatest();
    C_out1.readLatest();
    D_out1.readLatest();
    A_in2.readLatest();
    B_in2.readLatest();
    C_in2.readLatest();
    D_in2.readLatest();
    circleResult.readLatest();
  }

  void checkAllDataValidity(ctk::DataValidity validity) {
    BOOST_CHECK(A_out1.dataValidity() == validity);
    BOOST_CHECK(B_out1.dataValidity() == validity);
    BOOST_CHECK(C_out1.dataValidity() == validity);
    BOOST_CHECK(D_out1.dataValidity() == validity);
    BOOST_CHECK(A_in2.dataValidity() == validity);
    BOOST_CHECK(B_in2.dataValidity() == validity);
    BOOST_CHECK(C_in2.dataValidity() == validity);
    BOOST_CHECK(D_in2.dataValidity() == validity);
    BOOST_CHECK(circleResult.dataValidity() == validity);
  }

  CircularAppTestFixcture() { test.runApplication(); }
};

/** \anchor dataValidity_test_TestCircularInputDetection
 * Tests Technical specification: data validity propagation
 *  * \ref dataValidity_4_1_1 "4.1.1"  Inputs which are part of a circular dependency are marked as circular input.
 *  * \ref dataValidity_4_1_1_1 "4.1.1.1"  (partly, DeviceModule and other ApplciationModules not tested) Inputs from CS
 * are external inputs.
 *  * \ref dataValidity_4_1_2 "4.1.2"  All modules which have a circular dependency form a circular network.
 */
BOOST_AUTO_TEST_CASE(TestCircularInputDetection) {
  TestApplication1 app;
  ctk::TestFacility test;

  test.runApplication();
  // app.dumpConnections();
  // app.dump();

  // just test that the circular inputs have been detected correctly
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.a).isCircularInput() == false);
  // Check that the circular outputs are not marked as circular inputs. They are in the circle, but they are not inputs.
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.A.outputGroup.circularOutput2).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.B.outputGroup.circularOutput2).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.trigger).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.C.outputGroup.circularOutput2).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.inputGroup.circularInput1).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.circularInput2).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.circularOutput1).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.outputGroup.circularOutput2).isCircularInput() == false);
  // although there are inputs from and outputs to the same device this is not part of the circular network
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.i1.value).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.i3.value).isCircularInput() == false);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.D.o1.value).isCircularInput() == false);
}

/** \anchor dataValidity_test_OneInvalidVariable
 * Tests Technical specification: data validity propagation
 *  * \ref dataValidity_4_1_4 "4.1.4"  Propagation of the invalidity flag in a circle.
 *  * \ref dataValidity_4_1_5 "4.1.5"  Breaking the circular dependency.
 *
 *  This test intentionally does set more than one external input to faulty to make it easier to see where problems are
 * coming from.
 */
BOOST_FIXTURE_TEST_CASE(OneInvalidVariable, CircularAppTestFixcture<TestApplication1>) {
  a.setDataValidity(ctk::DataValidity::faulty);
  a.write();
  C_trigger.write();
  test.stepApplication();

  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::faulty);

  // getting a valid variable in the same module does not resolve the flag
  b.write();
  C_trigger.write();
  test.stepApplication();

  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::faulty);

  // now resolve the faulty condition
  a.setDataValidity(ctk::DataValidity::ok);
  a.write();
  test.stepApplication();

  readAllLatest();
  // we check in the app that the input is still invalid, not in the CS
  BOOST_CHECK(app.A.inputGroup.circularInput1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(app.A.circularInput2.dataValidity() == ctk::DataValidity::faulty);
  // the circular outputs of A and B are now valid
  BOOST_CHECK(A_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(B_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(B_in2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(C_in2.dataValidity() == ctk::DataValidity::ok);
  // the outputs of C, D and the circularResult have not been written yet
  BOOST_CHECK(C_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(D_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(A_in2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(D_in2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(circleResult.dataValidity() == ctk::DataValidity::faulty);

  // Now trigger C. The whole circle resolves
  C_trigger.write();
  test.stepApplication();
  readAllLatest();

  checkAllDataValidity(ctk::DataValidity::ok);
}

/** \anchor dataValidity_test_TwoFaultyInOneModule
 * Tests Technical specification: data validity propagation
 *  * \ref dataValidity_4_1_5 "4.1.5"  Breaking the circular dependency only when all variables go to ok.
 */
BOOST_FIXTURE_TEST_CASE(TwoFaultyInOneModule, CircularAppTestFixcture<TestApplication1>) {
  a.setDataValidity(ctk::DataValidity::faulty);
  a.write();
  C_trigger.write();
  test.stepApplication();
  // new in this test: an additional variable comes in while the internal and other external inputs are invalid
  b.setDataValidity(ctk::DataValidity::faulty);
  b.write();
  C_trigger.write();
  test.stepApplication();

  // just a cross check
  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::faulty);

  a.setDataValidity(ctk::DataValidity::ok);
  a.write();
  C_trigger.write();
  test.stepApplication();

  // everything still faulty as b is faulty
  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::faulty);

  b.setDataValidity(ctk::DataValidity::ok);
  b.write();
  C_trigger.write();
  test.stepApplication();

  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::ok);
}

/** \anchor dataValidity_test_outputManuallyFaulty
 * Tests Technical specification: data validity propagation
 *  * \ref dataValidity_4_1_8 "4.1.8"  Programmatically setting an output to faulty behaves like external input faulty.
 */
BOOST_FIXTURE_TEST_CASE(OutputManuallyFaulty, CircularAppTestFixcture<TestApplication1>) {
  app.A.circularOutput1.setDataValidity(ctk::DataValidity::faulty);
  a.write();
  test.stepApplication();

  readAllLatest();
  // The data validity flag is not ignored, although only circular inputs are invalid
  // B transports the flag. The A.outputGroup.circularOutput2 is still valid because all inputs are valid.
  BOOST_CHECK(A_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(B_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(B_in2.dataValidity() == ctk::DataValidity::ok); // this is A.outputGroup.circularOutput2
  BOOST_CHECK(C_in2.dataValidity() == ctk::DataValidity::faulty);
  // the outputs of C, D and the circularResult have already been written yet
  BOOST_CHECK(C_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(D_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(A_in2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(D_in2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(circleResult.dataValidity() == ctk::DataValidity::ok);

  C_trigger.write();
  test.stepApplication();

  // Now the whole circle is invalid, except for A.outputGroup.circularOutput2 which has not been written again yet.
  // (Module A stops the circular propagation because it is using readAny(), which otherwises would lead to more and more
  //  data packages piling up in the cirle because each external read adds one)
  readAllLatest();
  BOOST_CHECK(A_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(B_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(B_in2.dataValidity() == ctk::DataValidity::ok); // this is A.outputGroup.circularOutput2
  BOOST_CHECK(C_in2.dataValidity() == ctk::DataValidity::faulty);
  // the outputs of C, D and the circularResult have already been written yet
  BOOST_CHECK(C_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(D_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(A_in2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(D_in2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(circleResult.dataValidity() == ctk::DataValidity::faulty);

  // If we now complete the circle again, the faulty flag is propagated everywhere
  a.write();
  C_trigger.write();
  test.stepApplication();

  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::faulty);

  // Check that the situation resolved when the data validity of the output is back to ok
  app.A.circularOutput1.setDataValidity(ctk::DataValidity::ok);
  a.write();
  test.stepApplication();

  readAllLatest();
  // Module A goes to valid immediately and ignored the invalid circular inputs
  BOOST_CHECK(A_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(B_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(B_in2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(C_in2.dataValidity() == ctk::DataValidity::ok);
  // the outputs of C, D and the circularResult have not been written yet
  BOOST_CHECK(C_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(D_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(A_in2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(D_in2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(circleResult.dataValidity() == ctk::DataValidity::faulty);

  C_trigger.write();
  test.stepApplication();

  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::ok);
}

/** \anchor dataValidity_test_TwoFaultyInTwoModules
 * Tests Technical specification: data validity propagation
 *  * \ref dataValidity_4_2_3 "4.2.3"  Modules do no go to OK if all its external inputs are OK
 *                                     if other modules in the circular  network have external inputs which are faulty.
 */
BOOST_FIXTURE_TEST_CASE(TwoFaultyInTwoModules, CircularAppTestFixcture<TestApplication1>) {
  a.setDataValidity(ctk::DataValidity::faulty);
  a.write();
  C_trigger.write();
  test.stepApplication();
  // new in this test: the trigger in C bring an additional invalidity flag.
  a.write();
  C_trigger.setDataValidity(ctk::DataValidity::faulty);
  C_trigger.write();
  test.stepApplication();

  // just a cross check
  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::faulty);

  a.setDataValidity(ctk::DataValidity::ok);
  a.write();
  C_trigger.write();
  test.stepApplication();

  // everything still faulty as b is faulty
  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::faulty);

  a.write();
  C_trigger.setDataValidity(ctk::DataValidity::ok);
  C_trigger.write();
  test.stepApplication();

  readAllLatest();
  // the first half of the circle is not OK yet because no external triggers have arrived at A since
  // the faultly condition was resolved
  BOOST_CHECK(A_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(B_out1.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(B_in2.dataValidity() == ctk::DataValidity::faulty);
  BOOST_CHECK(C_in2.dataValidity() == ctk::DataValidity::faulty);
  // the outputs of C, D and the circularResult have already been written yet
  BOOST_CHECK(C_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(D_out1.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(A_in2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(D_in2.dataValidity() == ctk::DataValidity::ok);
  BOOST_CHECK(circleResult.dataValidity() == ctk::DataValidity::ok);

  // writing a  resolves the remainging variables
  a.write();
  test.stepApplication();
  readAllLatest();
  checkAllDataValidity(ctk::DataValidity::ok);
}

// A more complicated network with three entangled circles and one separate circle.
// AA-->BB-->CC-->DD-->AA    /->HH
// ^     |   |     ^       GG<-/
// |-EE<-|   |->FF-|
//
// The important part of this test is to check that the whole network AA,..,FF is always detected for each input,
// even if the scan is only for a variable that starts the scan in only in a local circle (like AA/fromEE).
// In addition it tests that not everything is mixed into a single circular network (GG,HH is detected as separate
// circular network).

// Don't try to pass any data through the network. It will be stuck because there are no real main loops. Only the initial
// value is passed (write exaclty once, then never read). It's just used to test the static circular network detection.

struct TestModuleBase2 : ctk::ApplicationModule {
  using ApplicationModule::ApplicationModule;

  // available in all modules
  ctk::ScalarPushInput<int> fromCS{this, "fromCS", "", ""};

  // default main loop which provides initial values, but does not read or write anything else
  void mainLoop() override { writeAll(); }
};

struct AA : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromEE{this, "fromEE", "", ""};
  ctk::ScalarPushInput<int> fromDD{this, "fromDD", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromAA{this, "fromAA", "", ""};
  } outputGroup{this, "BB", "", ctk::HierarchyModifier::oneLevelUp};

  void prepare() override { writeAll(); } // break circular waiting for initial values
  void mainLoop() override {}
};

struct BB : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromAA{this, "fromAA", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromBB{this, "fromBB", "", ""};
  } outputGroup{this, "CC", "", ctk::HierarchyModifier::oneLevelUp};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromBB{this, "fromBB", "", ""};
  } outputGroup2{this, "EE", "", ctk::HierarchyModifier::oneLevelUp};
};

struct EE : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromBB{this, "fromBB", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromEE{this, "fromEE", "", ""};
  } outputGroup{this, "AA", "", ctk::HierarchyModifier::oneLevelUp};
};

struct CC : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromBB{this, "fromBB", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromCC{this, "fromCC", "", ""};
  } outputGroup{this, "DD", "", ctk::HierarchyModifier::oneLevelUp};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromCC{this, "fromCC", "", ""};
  } outputGroup2{this, "FF", "", ctk::HierarchyModifier::oneLevelUp};
};

struct DD : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromCC{this, "fromCC", "", ""};
  ctk::ScalarPushInput<int> fromFF{this, "fromFF", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromDD{this, "fromDD", "", ""};
  } outputGroup{this, "AA", "", ctk::HierarchyModifier::oneLevelUp};
};

struct FF : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromCC{this, "fromCC", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromFF{this, "fromFF", "", ""};
  } outputGroup{this, "DD", "", ctk::HierarchyModifier::oneLevelUp};
};

struct GG : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromHH{this, "fromHH", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromGG{this, "fromGG", "", ""};
  } outputGroup{this, "HH", "", ctk::HierarchyModifier::oneLevelUp};

  void prepare() override { writeAll(); } // break circular waiting for initial values
  void mainLoop() override {}
};

struct HH : TestModuleBase2 {
  using TestModuleBase2::TestModuleBase2;

  ctk::ScalarPushInput<int> fromGG{this, "fromGG", "", ""};

  struct /*OutputGroup*/ : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<int> fromHH{this, "fromHH", "", ""};
  } outputGroup{this, "GG", "", ctk::HierarchyModifier::oneLevelUp};
};

struct TestApplication2 : ctk::Application {
  TestApplication2() : Application("connectionTestSuite") {}
  ~TestApplication2() { shutdown(); }

  AA aa{this, "AA", ""};
  BB bb{this, "BB", ""};
  CC cc{this, "CC", ""};
  DD dd{this, "DD", ""};
  EE ee{this, "EE", ""};
  FF ff{this, "FF", ""};
  GG gg{this, "GG", ""};
  HH hh{this, "HH", ""};

 public:
  // get a copy of the protected circularDependencyNetworks
  std::map<size_t, std::list<EntityOwner*>> getCircularDependencyNetworks() {
    return Application::circularDependencyNetworks;
  }
};

/** \anchor dataValidity_test_TestCircularInputDetection2
 * Tests Technical specification: data validity propagation
 *  * \ref dataValidity_4_1_2_1 "4.1.2.1" Entangled circles belonhg to the same circular network.
 *  * \ref dataValidity_4_1_2_2 "4.1.2.2" There can be multiple disconnected circular networks.
 *  * \ref dataValidity_4_3_2 "4.3.2" Each module and each circular input knows its circular network.
 */
BOOST_AUTO_TEST_CASE(TestCircularInputDetection2) {
  TestApplication2 app;
  ctk::TestFacility test;

  test.runApplication();
  // app.dumpConnections();

  // Check that all inputs have been identified correctly
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.aa.fromEE).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.aa.fromDD).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.aa.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.bb.fromAA).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.bb.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.cc.fromBB).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.cc.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.cc.fromBB).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.cc.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.dd.fromCC).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.dd.fromFF).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.dd.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.ee.fromBB).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.ee.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.ff.fromCC).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.ff.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.gg.fromHH).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.gg.fromCS).isCircularInput() == false);

  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.hh.fromGG).isCircularInput() == true);
  BOOST_CHECK(static_cast<ctk::VariableNetworkNode>(app.hh.fromCS).isCircularInput() == false);

  // Check that the networks have been identified correctly
  auto circularNetworks = app.getCircularDependencyNetworks();
  BOOST_CHECK_EQUAL(circularNetworks.size(), 2);
  for(auto& networkIter : app.getCircularDependencyNetworks()) {
    auto& id = networkIter.first;
    auto& network = networkIter.second;
    // networks have the correct size
    if(network.size() == 6) {
      std::list<ctk::EntityOwner*> modules = {&app.aa, &app.bb, &app.cc, &app.dd, &app.ee, &app.ff};
      for(auto module : modules) {
        // all modules are in the network
        BOOST_CHECK(std::count(network.begin(), network.end(), module) == 1);
        // each module has the correct network associated
        BOOST_CHECK_EQUAL(module->getCircularNetworkHash(), id);
      }
    }
    else if(network.size() == 2) {
      std::list<ctk::EntityOwner*> modules = {&app.gg, &app.hh};
      for(auto module : modules) {
        BOOST_CHECK(std::count(network.begin(), network.end(), module) == 1);
        BOOST_CHECK_EQUAL(module->getCircularNetworkHash(), id);
      }
    }
    else {
      BOOST_CHECK_MESSAGE(false, "Network with wrong number of modules detected: " + std::to_string(network.size()));
    }
  }
}
