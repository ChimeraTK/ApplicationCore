// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/ControlSystemAdapter/PVManager.h>

#include <libxml++/libxml++.h>

#include <boost/filesystem.hpp>
#include <boost/mpl/list.hpp>

#include <functional>
#include <future>
#include <string>
#include <vector>

#define BOOST_NO_EXCEPTIONS
#define BOOST_TEST_MODULE testAppModuleConnections
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

namespace Tests::testAppModuleConnections {

  // list of user types the accessors are tested with
  using test_types = boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double>;

  /********************************************************************************************************************/
  /* the ApplicationModule for the test is a template of the user type */

  template<typename T>
  struct TestModuleFeed : public ctk::ApplicationModule {
    TestModuleFeed(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {}, bool unregister = false)
    : ApplicationModule(owner, name, description, tags), mainLoopStarted(2) {
      if(unregister) {
        owner->unregisterModule(this);
      }
    }

    ctk::ScalarOutput<T> feedingPush;
    ctk::ArrayOutput<T> feedingArray;
    ctk::ArrayOutput<T> feedingPseudoArray;

    // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
    // mainLoop(). This is required since the mainLoopWrapper accesses the module variables before the start of the
    // mainLoop.
    // execute this right after the Application::run():
    //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
    boost::barrier mainLoopStarted;

    void prepare() override {
      incrementDataFaultCounter(); // force all outputs  to invalid
      writeAll();                  // write initial values
      decrementDataFaultCounter(); // validity according to input validity
    }

    void mainLoop() override { mainLoopStarted.wait(); }
  };

  template<typename T>
  struct TestModuleConsume : public ctk::ApplicationModule {
    TestModuleConsume(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {}, bool unregister = false)
    : ApplicationModule(owner, name, description, tags), mainLoopStarted(2) {
      if(unregister) {
        owner->unregisterModule(this);
      }
    }

    ctk::ScalarPushInput<T> consumingPush;
    ctk::ScalarPushInput<T> consumingPush2;
    ctk::ScalarPushInput<T> consumingPush3;

    ctk::ScalarPollInput<T> consumingPoll;
    ctk::ArrayPushInput<T> consumingPushArray;

    // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
    // mainLoop(). This is required since the mainLoopWrapper accesses the module variables before the start of the
    // mainLoop.
    // execute this right after the Application::run():
    //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
    boost::barrier mainLoopStarted;

    void prepare() override {
      incrementDataFaultCounter(); // force all outputs  to invalid
      writeAll();                  // write initial values
      decrementDataFaultCounter(); // validity according to input validity
    }

    void mainLoop() override { mainLoopStarted.wait(); }
  };

  /********************************************************************************************************************/
  /* dummy application */

  template<typename T>
  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {}
    ~TestApplication() override { shutdown(); }

    TestModuleFeed<T> testModuleFeed{this, "testModuleFeed", "The test module"};
    TestModuleConsume<T> testModuleConsume{this, "testModuleConsume", "The other test module"};
  };

  /********************************************************************************************************************/
  /* test case for two scalar accessors in push mode */

  BOOST_AUTO_TEST_CASE_TEMPLATE(testTwoScalarPushAccessors, T, test_types) {
    // FIXME: With the new scheme, there cannot be a 1:1 module connection any more, it will always be a network
    // involving the ControlSystem
    std::cout << "*** testTwoScalarPushAccessors<" << typeid(T).name() << ">" << std::endl;

    TestApplication<T> app;
    app.testModuleFeed.feedingPush = {&app.testModuleFeed, "/testTwoScalarPushAccessors", "", ""};
    app.testModuleConsume.consumingPush = {&app.testModuleConsume, "/testTwoScalarPushAccessors", "", ""};

    ctk::TestFacility tf{app, false};
    tf.runApplication();
    app.testModuleFeed.mainLoopStarted.wait();    // make sure the module's mainLoop() is entered
    app.testModuleConsume.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

    // single threaded test
    app.testModuleConsume.consumingPush = 0;
    app.testModuleFeed.feedingPush = 42;
    BOOST_CHECK(app.testModuleConsume.consumingPush == 0);
    app.testModuleFeed.feedingPush.write();
    BOOST_CHECK(app.testModuleConsume.consumingPush == 0);
    app.testModuleConsume.consumingPush.read();
    BOOST_CHECK(app.testModuleConsume.consumingPush == 42);

    // launch read() on the consumer asynchronously and make sure it does not yet
    // receive anything
    auto futRead = std::async(std::launch::async, [&app] { app.testModuleConsume.consumingPush.read(); });
    BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);

    BOOST_CHECK(app.testModuleConsume.consumingPush == 42);

    // write to the feeder
    app.testModuleFeed.feedingPush = 120;
    app.testModuleFeed.feedingPush.write();

    // check that the consumer now receives the just written value
    BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
    BOOST_CHECK(app.testModuleConsume.consumingPush == 120);
  }

  /********************************************************************************************************************/
  /* test case for four scalar accessors in push mode: one feeder and three
   * consumers */

  BOOST_AUTO_TEST_CASE_TEMPLATE(testFourScalarPushAccessors, T, test_types) {
    std::cout << "*** testFourScalarPushAccessors<" << typeid(T).name() << ">" << std::endl;

    TestApplication<T> app;
    app.testModuleConsume.consumingPush = {&app.testModuleConsume, "/testFourScalarPushAccessors", "", ""};
    app.testModuleConsume.consumingPush2 = {&app.testModuleConsume, "/testFourScalarPushAccessors", "", ""};
    app.testModuleFeed.feedingPush = {&app.testModuleFeed, "/testFourScalarPushAccessors", "", ""};
    app.testModuleConsume.consumingPush3 = {&app.testModuleConsume, "/testFourScalarPushAccessors", "", ""};

    ctk::TestFacility tf{app, false};
    tf.runApplication();
    app.testModuleFeed.mainLoopStarted.wait();    // make sure the module's mainLoop() is entered
    app.testModuleConsume.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

    // single threaded test
    app.testModuleConsume.consumingPush = 0;
    app.testModuleConsume.consumingPush2 = 2;
    app.testModuleConsume.consumingPush3 = 3;
    app.testModuleFeed.feedingPush = 42;
    BOOST_CHECK(app.testModuleConsume.consumingPush == 0);
    BOOST_CHECK(app.testModuleConsume.consumingPush2 == 2);
    BOOST_CHECK(app.testModuleConsume.consumingPush3 == 3);
    app.testModuleFeed.feedingPush.write();
    BOOST_CHECK(app.testModuleConsume.consumingPush == 0);
    BOOST_CHECK(app.testModuleConsume.consumingPush2 == 2);
    BOOST_CHECK(app.testModuleConsume.consumingPush3 == 3);
    app.testModuleConsume.consumingPush.read();
    BOOST_CHECK(app.testModuleConsume.consumingPush == 42);
    BOOST_CHECK(app.testModuleConsume.consumingPush2 == 2);
    BOOST_CHECK(app.testModuleConsume.consumingPush3 == 3);
    app.testModuleConsume.consumingPush2.read();
    BOOST_CHECK(app.testModuleConsume.consumingPush == 42);
    BOOST_CHECK(app.testModuleConsume.consumingPush2 == 42);
    BOOST_CHECK(app.testModuleConsume.consumingPush3 == 3);
    app.testModuleConsume.consumingPush3.read();
    BOOST_CHECK(app.testModuleConsume.consumingPush == 42);
    BOOST_CHECK(app.testModuleConsume.consumingPush2 == 42);
    BOOST_CHECK(app.testModuleConsume.consumingPush3 == 42);

    // launch read() on the consumers asynchronously and make sure it does not yet
    // receive anything
    auto futRead = std::async(std::launch::async, [&app] { app.testModuleConsume.consumingPush.read(); });
    auto futRead2 = std::async(std::launch::async, [&app] { app.testModuleConsume.consumingPush2.read(); });
    auto futRead3 = std::async(std::launch::async, [&app] { app.testModuleConsume.consumingPush3.read(); });
    BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
    BOOST_CHECK(futRead2.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout);
    BOOST_CHECK(futRead3.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout);

    BOOST_CHECK(app.testModuleConsume.consumingPush == 42);
    BOOST_CHECK(app.testModuleConsume.consumingPush2 == 42);
    BOOST_CHECK(app.testModuleConsume.consumingPush3 == 42);

    // write to the feeder
    app.testModuleFeed.feedingPush = 120;
    app.testModuleFeed.feedingPush.write();

    // check that the consumers now receive the just written value
    BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
    BOOST_CHECK(futRead2.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
    BOOST_CHECK(futRead3.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
    BOOST_CHECK(app.testModuleConsume.consumingPush == 120);
    BOOST_CHECK(app.testModuleConsume.consumingPush2 == 120);
    BOOST_CHECK(app.testModuleConsume.consumingPush3 == 120);
  }

  /********************************************************************************************************************/
  /* test case for two scalar accessors, feeder in push mode and consumer in poll
   * mode */

  BOOST_AUTO_TEST_CASE_TEMPLATE(testTwoScalarPushPollAccessors, T, test_types) {
    std::cout << "*** testTwoScalarPushPollAccessors<" << typeid(T).name() << ">" << std::endl;

    TestApplication<T> app;

    app.testModuleFeed.feedingPush = {&app.testModuleFeed, "/testTwoScalarPushPollAccessors", "", ""};
    app.testModuleConsume.consumingPoll = {&app.testModuleConsume, "/testTwoScalarPushPollAccessors", "", ""};

    ctk::TestFacility tf{app, false};
    tf.runApplication();
    app.testModuleFeed.mainLoopStarted.wait();    // make sure the module's mainLoop() is entered
    app.testModuleConsume.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

    // single threaded test only, since read() does not block in this case
    app.testModuleConsume.consumingPoll = 0;
    app.testModuleFeed.feedingPush = 42;
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 0);
    app.testModuleFeed.feedingPush.write();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 0);
    app.testModuleConsume.consumingPoll.read();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 42);
    app.testModuleConsume.consumingPoll.read();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 42);
    app.testModuleConsume.consumingPoll.read();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 42);
    app.testModuleFeed.feedingPush = 120;
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 42);
    app.testModuleFeed.feedingPush.write();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 42);
    app.testModuleConsume.consumingPoll.read();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 120);
    app.testModuleConsume.consumingPoll.read();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 120);
    app.testModuleConsume.consumingPoll.read();
    BOOST_CHECK(app.testModuleConsume.consumingPoll == 120);
  }

  /********************************************************************************************************************/
  /* test case for two array accessors in push mode */

  BOOST_AUTO_TEST_CASE_TEMPLATE(testTwoArrayAccessors, T, test_types) {
    std::cout << "*** testTwoArrayAccessors<" << typeid(T).name() << ">" << std::endl;

    TestApplication<T> app;

    // app.testModuleFeed.feedingArray >> app.testModuleConsume.consumingPushArray;
    app.testModuleFeed.feedingArray = {&app.testModuleFeed, "/testFourScalarPushAccessors", "", 10, ""};
    app.testModuleConsume.consumingPushArray = {&app.testModuleConsume, "/testFourScalarPushAccessors", "", 10, ""};
    ctk::TestFacility tf{app, false};
    tf.runApplication();

    app.testModuleFeed.mainLoopStarted.wait();    // make sure the module's mainLoop() is entered
    app.testModuleConsume.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

    BOOST_CHECK(app.testModuleFeed.feedingArray.getNElements() == 10);
    BOOST_CHECK(app.testModuleConsume.consumingPushArray.getNElements() == 10);

    // single threaded test
    for(auto& val : app.testModuleConsume.consumingPushArray) {
      val = 0;
    }
    for(unsigned int i = 0; i < 10; ++i) {
      app.testModuleFeed.feedingArray[i] = 99 + (T)i;
    }
    for(auto& val : app.testModuleConsume.consumingPushArray) {
      BOOST_CHECK(val == 0);
    }
    app.testModuleFeed.feedingArray.write();
    for(auto& val : app.testModuleConsume.consumingPushArray) {
      BOOST_CHECK(val == 0);
    }
    app.testModuleConsume.consumingPushArray.read();
    for(unsigned int i = 0; i < 10; ++i) {
      BOOST_CHECK(app.testModuleConsume.consumingPushArray[i] == 99 + (T)i);
    }

    // launch read() on the consumer asynchronously and make sure it does not yet
    // receive anything
    auto futRead = std::async(std::launch::async, [&app] { app.testModuleConsume.consumingPushArray.read(); });
    BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);

    for(unsigned int i = 0; i < 10; ++i) {
      BOOST_CHECK(app.testModuleConsume.consumingPushArray[i] == 99 + (T)i);
    }

    // write to the feeder
    for(unsigned int i = 0; i < 10; ++i) {
      app.testModuleFeed.feedingArray[i] = 42 - (T)i;
    }
    app.testModuleFeed.feedingArray.write();

    // check that the consumer now receives the just written value
    BOOST_CHECK(futRead.wait_for(std::chrono::milliseconds(2000)) == std::future_status::ready);
    for(unsigned int i = 0; i < 10; ++i) {
      BOOST_CHECK(app.testModuleConsume.consumingPushArray[i] == 42 - (T)i);
    }
  }

  /********************************************************************************************************************/
  /* test case for connecting array of length 1 with scalar */

  BOOST_AUTO_TEST_CASE_TEMPLATE(testPseudoArray, T, test_types) {
    std::cout << "*** testPseudoArray<" << typeid(T).name() << ">" << std::endl;

    TestApplication<T> app;

    // app.testModuleFeed.feedingPseudoArray >> app.testModuleConsume.consumingPush;
    app.testModuleFeed.feedingPseudoArray = {&app.testModuleFeed, "/testPseudoArray", "", 1, ""};
    app.testModuleConsume.consumingPush = {&app.testModuleConsume, "/testPseudoArray", "", ""};

    // run the app
    ctk::TestFacility tf{app, false};
    tf.runApplication();
    app.testModuleFeed.mainLoopStarted.wait();    // make sure the module's mainLoop() is entered
    app.testModuleConsume.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

    // test data transfer
    app.testModuleFeed.feedingPseudoArray[0] = 33;
    app.testModuleFeed.feedingPseudoArray.write();
    app.testModuleConsume.consumingPush.read();
    BOOST_CHECK(app.testModuleConsume.consumingPush == 33);
  }

  /********************************************************************************************************************/
  /* test case for EntityOwner::constant() */

  template<typename T>
  struct ConstantTestModule : public ctk::ApplicationModule {
    ConstantTestModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, tags), mainLoopStarted(2) {}

    ctk::ScalarPushInput<T> consumingPush{this, constant(T(66)), "", ""};
    ctk::ScalarPollInput<T> consumingPoll{this, constant(T(77)), "", ""};
    // test a second accessor of a different type but defining the constant with the same type as before
    ctk::ScalarPollInput<std::string> myStringConstant{this, constant(T(66)), "", ""};

    // We do not use testable mode for this test, so we need this barrier to synchronise to the beginning of the
    // mainLoop(). This is required since the mainLoopWrapper accesses the module variables before the start of the
    // mainLoop.
    // execute this right after the Application::run():
    //   app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered
    boost::barrier mainLoopStarted;

    void prepare() override {
      incrementDataFaultCounter(); // force all outputs  to invalid
      writeAll();                  // write initial values
      decrementDataFaultCounter(); // validity according to input validity
    }

    void mainLoop() override { mainLoopStarted.wait(); }
  };

  template<typename T>
  struct ConstantTestApplication : public ctk::Application {
    ConstantTestApplication() : Application("testSuite") {}
    ~ConstantTestApplication() override { shutdown(); }

    ConstantTestModule<T> testModule{this, "testModule", "The test module"};
  };

  BOOST_AUTO_TEST_CASE_TEMPLATE(testConstants, T, test_types) {
    std::cout << "*** testConstants<" << typeid(T).name() << ">" << std::endl;

    ConstantTestApplication<T> app;

    ctk::TestFacility tf{app, false};
    tf.runApplication();
    app.testModule.mainLoopStarted.wait(); // make sure the module's mainLoop() is entered

    BOOST_TEST(app.testModule.consumingPush == 66);
    BOOST_TEST(app.testModule.consumingPoll == 77);
    BOOST_TEST(boost::starts_with(std::string(app.testModule.myStringConstant), "66")); // might be 66 or 66.000000

    BOOST_TEST(app.testModule.consumingPush.readNonBlocking() == false);

    app.testModule.consumingPoll = 0;
    app.testModule.consumingPoll.read();
    BOOST_TEST(app.testModule.consumingPoll == 77);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  struct SelfUnregisteringModule : public ctk::ApplicationModule {
    SelfUnregisteringModule(
        ctk::ModuleGroup* owner, const std::string& name, const std::string& description, bool unregister = false)
    : ApplicationModule(owner, name, description) {
      if(unregister) {
        disable();
      }
    }

    ctk::ScalarOutput<int> out{this, "out", "", "Some output"};
    ctk::ScalarPushInput<int> in{this, "in", "", "Some input"};

    void mainLoop() override {
      while(true) {
        out = 1 + in;
        writeAll();
        readAll();
      }
    }
  };

  /********************************************************************************************************************/

  struct TestAppSelfUnregisteringModule : public ctk::Application {
    TestAppSelfUnregisteringModule() : Application("SelfUnregisteringModuleApp") {}
    ~TestAppSelfUnregisteringModule() override { shutdown(); }

    SelfUnregisteringModule a{this, "a", "First test module which stays"};
    SelfUnregisteringModule b{this, "b", "The test module which unregisters itself", true};
    SelfUnregisteringModule c{this, "c", "Another test module which stays"};
  };

  /********************************************************************************************************************/
  /* test case for EntityOwner::constant() */

  BOOST_AUTO_TEST_CASE(testSelfUnregisteringModule) {
    std::cout << "*** testSelfUnregisteringModule" << std::endl;

    TestAppSelfUnregisteringModule app;

    ctk::TestFacility tf{app};

    auto& pvm = *tf.getPvManager();
    BOOST_TEST(pvm.hasProcessVariable("a/out"));
    BOOST_TEST(pvm.hasProcessVariable("a/in"));
    BOOST_TEST(!pvm.hasProcessVariable("b/out"));
    BOOST_TEST(!pvm.hasProcessVariable("b/in"));
    BOOST_TEST(pvm.hasProcessVariable("c/out"));
    BOOST_TEST(pvm.hasProcessVariable("c/in"));

    auto aout = tf.getScalar<int>("a/out");
    auto ain = tf.getScalar<int>("a/in");
    auto cout = tf.getScalar<int>("c/out");
    auto cin = tf.getScalar<int>("c/in");

    tf.setScalarDefault("a/in", 1000);
    tf.setScalarDefault("c/in", 2000);

    tf.runApplication();

    BOOST_TEST(aout == 1001);
    BOOST_TEST(cout == 2001);

    ain.setAndWrite(42);
    tf.stepApplication();
    BOOST_TEST(aout.readNonBlocking());
    BOOST_TEST(!cout.readNonBlocking());
    BOOST_TEST(aout == 43);

    cin.setAndWrite(120);
    tf.stepApplication();
    BOOST_TEST(!aout.readNonBlocking());
    BOOST_TEST(cout.readNonBlocking());
    BOOST_TEST(cout == 121);
  }

  /********************************************************************************************************************/
  /* Application and tests for the CR-001 description/unit priority handling.
   *
   * The accessor assignment operator implemented in the Scalar* classes accepts a
   * braced initialiser {owner, name/path, unit, description}, which reconstructs
   * the node with the given (absolute) path, unit and description (the description
   * is composed with the owning module hierarchy). Assigning the feeder and one or
   * more consumers to the same path forms a variable network, whose winning
   * description/unit are then exposed on the single control-system variable.
   */

  struct FeedModule : public ctk::ApplicationModule {
    FeedModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description)
    : ApplicationModule(owner, name, description) {}
    void mainLoop() override {}
    ctk::ScalarOutput<double> feederA{this, "feederA", "", "ch1"};
  };

  struct ConsumeModule : public ctk::ApplicationModule {
    ConsumeModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description)
    : ApplicationModule(owner, name, description) {}
    void mainLoop() override {}
    ctk::ScalarPushInput<double> consumerPrio1{this, "consumerPrio1", "", ""};
    ctk::ScalarPushInput<double> consumerPrio2{this, "consumerPrio2", "", ""};
  };

  struct PriorityTestApp : public ctk::Application {
    PriorityTestApp() : Application("PriorityTestApp") {}
    ~PriorityTestApp() override { shutdown(); }

    // module with a non-empty description (composes to "Oscilloscope - ...")
    FeedModule moduleB{this, "moduleB", "Oscilloscope"};

    // module with empty description (variable descriptions pass through unchanged)
    ConsumeModule moduleD{this, "moduleD", ""};
  };

  // Helper: return the <peer> elements of the given variable (found anywhere in
  // the document), with the processed per-peer <unit>/<description> and the
  // description priority attribute.
  struct PeerInfo {
    std::string name;
    std::string direction;
    std::string unit; // empty if no <unit> peer element
    bool hasDescription{false};
    std::string description;
    int priority{0};
  };

  static std::vector<PeerInfo> getPeersForVariable(const std::string& xmlFile, const std::string& var) {
    std::vector<PeerInfo> result;
    xmlpp::DomParser parser;
    parser.parse_file(xmlFile);
    const auto* root = parser.get_document()->get_root_node();

    // depth-first search for a <variable name="var"> element
    std::function<void(const xmlpp::Node*)> walk = [&](const xmlpp::Node* n) {
      const auto* el = dynamic_cast<const xmlpp::Element*>(n);
      if(el == nullptr) {
        for(const auto* c = n->get_first_child(); c != nullptr; c = c->get_next_sibling()) {
          walk(c);
        }
        return;
      }
      if(el->get_name() == "variable") {
        const auto* an = el->get_attribute("name");
        if(an != nullptr && an->get_value() == var) {
          for(const auto* c = el->get_first_child(); c != nullptr; c = c->get_next_sibling()) {
            const auto* cc = dynamic_cast<const xmlpp::Element*>(c);
            if(cc == nullptr || cc->get_name() != "connections") continue;
            for(const auto* p = cc->get_first_child(); p != nullptr; p = p->get_next_sibling()) {
              const auto* pc = dynamic_cast<const xmlpp::Element*>(p);
              if(pc == nullptr || pc->get_name() != "peer") continue;
              PeerInfo pi;
              if(auto* a = pc->get_attribute("name")) pi.name = a->get_value();
              if(auto* a = pc->get_attribute("direction")) pi.direction = a->get_value();
              for(const auto* ch = pc->get_first_child(); ch != nullptr; ch = ch->get_next_sibling()) {
                const auto* chc = dynamic_cast<const xmlpp::Element*>(ch);
                if(chc == nullptr) continue;
                if(chc->get_name() == "unit") {
                  if(const auto* t = dynamic_cast<const xmlpp::TextNode*>(chc->get_first_child())) {
                    pi.unit = t->get_content();
                  }
                }
                else if(chc->get_name() == "description") {
                  pi.hasDescription = true;
                  if(auto* a = chc->get_attribute("priority")) pi.priority = std::stoi(a->get_value());
                  if(const auto* t = dynamic_cast<const xmlpp::TextNode*>(chc->get_first_child())) {
                    pi.description = t->get_content();
                  }
                }
              }
              result.push_back(pi);
            }
          }
        }
        return; // a variable has no variable children
      }
      for(const auto* c = el->get_first_child(); c != nullptr; c = c->get_next_sibling()) {
        walk(c);
      }
    };
    walk(root);
    return result;
  }

  /********************************************************************************************************************/
  /* A consumer with a higher priority (leading '!') overwrites the feeder
   * description (CR-001 example). */

  BOOST_AUTO_TEST_CASE(testDescriptionConsumerOverwritesFeeder) {
    std::cout << "*** testDescriptionConsumerOverwritesFeeder" << std::endl;
    PriorityTestApp app;
    app.moduleB.feederA = {&app.moduleB, "/n1", "V", "ch1"};                    // "Oscilloscope - ch1", priority 0
    app.moduleD.consumerPrio1 = {&app.moduleD, "/n1", "V", "!phase deviation"}; // "phase deviation", priority 1

    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);
    app.initialise();

    auto pv = pvManagers.first->getProcessArray<double>("/n1");
    BOOST_REQUIRE(pv != nullptr);
    BOOST_TEST(pv->getDescription() == "phase deviation");
    BOOST_TEST(pv->getUnit() == "V");
  }

  /********************************************************************************************************************/
  /* If the priorities tie, the feeder description wins (CR-001 default). */

  BOOST_AUTO_TEST_CASE(testDescriptionFeederWinsOnTie) {
    std::cout << "*** testDescriptionFeederWinsOnTie" << std::endl;
    PriorityTestApp app;
    app.moduleB.feederA = {&app.moduleB, "/n2", "V", "ch1"};
    app.moduleD.consumerPrio1 = {&app.moduleD, "/n2", "V", "consumer description"};

    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);
    app.initialise();

    auto pv = pvManagers.first->getProcessArray<double>("/n2");
    BOOST_REQUIRE(pv != nullptr);
    BOOST_TEST(pv->getDescription() == "Oscilloscope - ch1");
  }

  /********************************************************************************************************************/
  /* Multiple consumers: the one with the highest priority wins ("!!" beats "!"). */

  BOOST_AUTO_TEST_CASE(testDescriptionHighestConsumerPriorityWins) {
    std::cout << "*** testDescriptionHighestConsumerPriorityWins" << std::endl;
    PriorityTestApp app;
    app.moduleB.feederA = {&app.moduleB, "/n3", "V", "ch1"};
    app.moduleD.consumerPrio1 = {&app.moduleD, "/n3", "V", "!first"};
    app.moduleD.consumerPrio2 = {&app.moduleD, "/n3", "V", "!!second"};

    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);
    app.initialise();

    auto pv = pvManagers.first->getProcessArray<double>("/n3");
    BOOST_REQUIRE(pv != nullptr);
    BOOST_TEST(pv->getDescription() == "second");
  }

  /********************************************************************************************************************/
  /* A leading '?' marks a negative (uncertain) priority: a '!!'-consumer still
   * wins, and the '?' markers are stripped from the exposed text. */

  BOOST_AUTO_TEST_CASE(testDescriptionNegativeAndPositivePriorities) {
    std::cout << "*** testDescriptionNegativeAndPositivePriorities" << std::endl;
    PriorityTestApp app;
    app.moduleB.feederA = {&app.moduleB, "/n4", "V", "ch1"};
    app.moduleD.consumerPrio1 = {&app.moduleD, "/n4", "V", "?uncertain"}; // priority -1
    app.moduleD.consumerPrio2 = {&app.moduleD, "/n4", "V", "!!definite"}; // priority 2

    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);
    app.initialise();

    auto pv = pvManagers.first->getProcessArray<double>("/n4");
    BOOST_REQUIRE(pv != nullptr);
    // only '!!definite' has the highest (non-negative) priority
    BOOST_TEST(pv->getDescription() == "definite");
  }

  /********************************************************************************************************************/
  /* A '?'-marked feeder, tied with a '?'-marked consumer (both negative) picks a
   * description without warning. */

  BOOST_AUTO_TEST_CASE(testDescriptionAllNegativePicksAny) {
    std::cout << "*** testDescriptionAllNegativePicksAny" << std::endl;
    PriorityTestApp app;
    app.moduleB.feederA = {&app.moduleB, "/n5", "V", "?ch1"};
    app.moduleD.consumerPrio1 = {&app.moduleD, "/n5", "V", "?consumer"};

    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);
    app.initialise();

    auto pv = pvManagers.first->getProcessArray<double>("/n5");
    BOOST_REQUIRE(pv != nullptr);
    // both negative, feeder preferred for determinism; '?' stripped
    BOOST_TEST(pv->getDescription() == "Oscilloscope - ch1");
  }

  /********************************************************************************************************************/
  /* Unit mismatch: no priority concept for units, winner is deterministic (first
   * distinct candidate) and a warning is emitted. */

  BOOST_AUTO_TEST_CASE(testUnitMismatchSelectsFirstDistinct) {
    std::cout << "*** testUnitMismatchSelectsFirstDistinct" << std::endl;
    PriorityTestApp app;
    app.moduleB.feederA = {&app.moduleB, "/n6", "V", "ch1"};
    app.moduleD.consumerPrio1 = {&app.moduleD, "/n6", "mV", "phase deviation"}; // different unit -> warning

    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);
    app.initialise();

    auto pv = pvManagers.first->getProcessArray<double>("/n6");
    BOOST_REQUIRE(pv != nullptr);
    BOOST_TEST(pv->getUnit() == "V");
    // both descriptions have priority 0, so the feeder wins on the tie-breaker
    BOOST_TEST(pv->getDescription() == "Oscilloscope - ch1");
  }

  /********************************************************************************************************************/
  /* Two consumers with distinct texts sharing the highest (non-negative)
   * priority constitute an ambiguity: a warning is emitted and one of them is
   * selected deterministically. */

  BOOST_AUTO_TEST_CASE(testDescriptionAmbiguityAmongConsumers) {
    std::cout << "*** testDescriptionAmbiguityAmongConsumers" << std::endl;
    PriorityTestApp app;
    app.moduleB.feederA = {&app.moduleB, "/n7", "V", "ch1"};
    app.moduleD.consumerPrio1 = {&app.moduleD, "/n7", "V", "!first"};  // priority 1
    app.moduleD.consumerPrio2 = {&app.moduleD, "/n7", "V", "!second"}; // priority 1

    auto pvManagers = ctk::createPVManager();
    app.setPVManager(pvManagers.second);
    app.initialise();

    auto pv = pvManagers.first->getProcessArray<double>("/n7");
    BOOST_REQUIRE(pv != nullptr);
    const auto& desc = pv->getDescription();
    // an ambiguity warning is emitted; one of the two competing texts wins
    BOOST_TEST((desc == "first" || desc == "second"));
  }

  /********************************************************************************************************************/
  /* xmlGenerator: per-peer <unit> and <description> with processed (stripped)
   * markers and a priority attribute. */

  BOOST_AUTO_TEST_CASE(testXmlPeersCarryUnitAndProcessedDescription) {
    std::cout << "*** testXmlPeersCarryUnitAndProcessedDescription" << std::endl;
    const auto xmlFileName = "PriorityTestApp.xml";
    boost::filesystem::remove(xmlFileName);

    {
      PriorityTestApp app;
      app.moduleB.feederA = {&app.moduleB, "/shared", "V", "ch1"};
      app.moduleD.consumerPrio1 = {&app.moduleD, "/shared", "V", "!phase deviation"};
      app.generateXML();
    } // shut down before parsing

    auto peers = getPeersForVariable(xmlFileName, "shared");
    BOOST_REQUIRE_EQUAL(peers.size(), 2u);

    bool foundFeeder = false;
    bool foundConsumer = false;
    for(const auto& p : peers) {
      if(p.direction == "feeding") {
        foundFeeder = true;
        // moduleB peer: raw composed description "Oscilloscope - ch1", priority 0
        BOOST_TEST(p.description == "Oscilloscope - ch1");
        BOOST_TEST(p.priority == 0);
        BOOST_TEST(p.unit == "V");
      }
      else if(p.direction == "consuming") {
        foundConsumer = true;
        // moduleD peer: raw composed description "!phase deviation" -> processed "phase deviation", priority 1
        BOOST_TEST(p.description == "phase deviation");
        BOOST_TEST(p.priority == 1);
        BOOST_TEST(p.unit == "V");
      }
    }
    BOOST_TEST(foundFeeder);
    BOOST_TEST(foundConsumer);
  }

  /********************************************************************************************************************/
  /* xmlGenerator: an escaped leading marker is treated as a literal (not a
   * marker), so the priority attribute is 0 and the text keeps the resolved '!'
   * in the leading position. */

  struct LiteralDescApp : public ctk::Application {
    LiteralDescApp() : Application("LiteralDescApp") {}
    ~LiteralDescApp() override { shutdown(); }
    // module description starts with an escaped '\!' -> literal, not a marker
    FeedModule moduleB{this, "moduleB", "\\!Important"};
  };

  BOOST_AUTO_TEST_CASE(testXmlPeerEscapedLeadingMarkerIsLiteral) {
    std::cout << "*** testXmlPeerEscapedLeadingMarkerIsLiteral" << std::endl;
    const auto xmlFileName = "LiteralDescApp.xml";
    boost::filesystem::remove(xmlFileName);

    {
      LiteralDescApp app;
      app.moduleB.feederA = {&app.moduleB, "/sharedLit", "V", "ch1"};
      app.generateXML();
    }

    auto peers = getPeersForVariable(xmlFileName, "sharedLit");
    BOOST_REQUIRE_EQUAL(peers.size(), 1u);
    const auto& p = peers.front();
    BOOST_TEST(p.direction == "feeding");
    // the escaped '\!' is a literal, so priority is 0 and the leading '!' is kept
    BOOST_TEST(p.priority == 0);
    BOOST_TEST(p.description == "!Important - ch1");
    BOOST_TEST(p.unit == "V");
  }

  /********************************************************************************************************************/

} // namespace Tests::testAppModuleConnections
