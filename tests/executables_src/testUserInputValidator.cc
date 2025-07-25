// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#define BOOST_TEST_MODULE testUserInputValidator

#include "Application.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "UserInputValidator.h"

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>

namespace Tests::testUserInputValidator {

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;

  /********************************************************************************************************************/
  /* Test module with a single validated input, used stand alone or as a downstream module */
  /********************************************************************************************************************/

  struct ModuleA : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPushInputWB<int> in1{this, "in1", "", "First validated input"};

    ctk::UserInputValidator validator;

    std::string in1ErrorMessage;

    void prepare() override {
      in1ErrorMessage = "(" + getName() + ") in1 needs to be smaller than 10";
      validator.add(in1ErrorMessage, [&] { return in1 < 10; }, in1);
    }

    void mainLoop() override {
      auto group = readAnyGroup();
      ctk::TransferElementID change;
      while(true) {
        validator.validate(change);
        change = group.readAny();
      }
    }
  };

  /********************************************************************************************************************/
  /* Variant of ModuleA with a second input */
  /********************************************************************************************************************/

  struct ModuleAwithSecondInput : public ModuleA {
    using ModuleA::ModuleA;

    ctk::ScalarPushInputWB<int> in2{this, "in2", "", "Second validated input"};

    std::string in2ErrorMessage;

    void prepare() override {
      ModuleA::prepare();
      in2ErrorMessage = "(" + getName() + ") in2 needs to be bigger than 10";
      validator.add(in2ErrorMessage, [&] { return in2 > 10; }, in2);
    }
  };

  /********************************************************************************************************************/
  /* Test module with a single validated input and one output for connection to another validated input */
  /********************************************************************************************************************/

  struct UpstreamSingleOut : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPushInputWB<int> in1{this, "in1", "", "First validated input"};
    ctk::ScalarOutputPushRB<int> out1{this, "/Downstream/in1", "", "Output"};

    ctk::UserInputValidator validator;

    void prepare() override {
      validator.add("(" + getName() + ") in1 needs to be smaller than 20", [&] { return in1 < 20; }, in1);
    }

    void mainLoop() override {
      auto group = readAnyGroup();
      ctk::TransferElementID change;
      while(true) {
        validator.validate(change);
        out1.writeIfDifferent(in1 + 1);
        change = group.readAny();
      }
    }
  };
  /********************************************************************************************************************/
  /* Test module with a single validated input and two outputs for connection to another validated input */
  /********************************************************************************************************************/

  struct UpstreamTwinOut : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPushInputWB<int> in1{this, "in1", "", "First validated input"};
    ctk::ScalarOutputPushRB<int> out1{this, "/Downstream1/in1", "", "Output"};
    ctk::ScalarOutputPushRB<int> out2{this, "/Downstream2/in1", "", "Output"};

    ctk::UserInputValidator validator;

    void prepare() override {
      validator.add("(" + getName() + ") in1 needs to be smaller than 20", [&] { return in1 < 20; }, in1);
    }

    void mainLoop() override {
      auto group = readAnyGroup();
      ctk::TransferElementID change;
      while(true) {
        validator.validate(change);

        out1.writeIfDifferent(in1 + 1);
        out2.writeIfDifferent(in1 + 2);

        change = group.readAny();
      }
    }
  };

  /********************************************************************************************************************/
  /* Test cases */
  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testSingleVariable) {
    std::cout << "testSingleVariable" << std::endl;

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      ModuleA moduleA{this, "ModuleA", ""};
    };

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

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testFallback) {
    std::cout << "testFallback" << std::endl;

    struct ModuleAmod : public ModuleA {
      using ModuleA::ModuleA;
      void prepare() override {
        ModuleA::prepare();
        validator.setFallback(in1, 7);
      }
    };

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      ModuleAmod moduleA{this, "ModuleA", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto modAin1 = test.getScalar<int>("/ModuleA/in1");

    test.setScalarDefault<int>("/ModuleA/in1", 12);

    test.runApplication();

    BOOST_TEST(!modAin1.readLatest());
    BOOST_TEST(app.moduleA.in1 == 7);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testMultipleVariablesDifferentChecks) {
    std::cout << "testMultipleVariablesDifferentChecks" << std::endl;
    // add another input which is validated with another UserInputValidator::add() call

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      ModuleAwithSecondInput moduleA{this, "ModuleA", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto in1 = test.getScalar<int>("/ModuleA/in1");
    auto in2 = test.getScalar<int>("/ModuleA/in2");

    test.setScalarDefault<int>("/ModuleA/in1", 3);
    test.setScalarDefault<int>("/ModuleA/in2", 12);

    test.runApplication();

    BOOST_TEST(!in1.readLatest());
    BOOST_TEST(!in2.readLatest());

    in1.setAndWrite(15);
    test.stepApplication();
    BOOST_TEST(in1.readLatest());
    BOOST_TEST(in1 == 3);
    BOOST_TEST(app.moduleA.in1 == 3);
    BOOST_TEST(app.moduleA.in2 == 12);

    in1.setAndWrite(9);
    test.stepApplication();
    BOOST_TEST(!in1.readLatest());
    BOOST_TEST(app.moduleA.in1 == 9);
    BOOST_TEST(app.moduleA.in2 == 12);

    BOOST_TEST(!in2.readLatest());

    in2.setAndWrite(7);
    test.stepApplication();
    BOOST_TEST(in2.readLatest());
    BOOST_TEST(in2 == 12);
    BOOST_TEST(app.moduleA.in1 == 9);
    BOOST_TEST(app.moduleA.in2 == 12);

    in2.setAndWrite(13);
    test.stepApplication();
    BOOST_TEST(!in2.readLatest());
    BOOST_TEST(app.moduleA.in1 == 9);
    BOOST_TEST(app.moduleA.in2 == 13);

    BOOST_TEST(!in1.readLatest());
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testMultipleVariablesSameCheck) {
    std::cout << "testMultipleVariablesSameCheck" << std::endl;
    // add another input which is validated in the same UserInputValidator::add() call as the existing input, replacing
    // the existing add() call

    struct ModuleAmod : public ModuleA {
      using ModuleA::ModuleA;

      ctk::ScalarPushInputWB<int> in2{this, "in2", "", "Second validated input"};

      void prepare() override {
        // Do not call ModuleA::prepare() here, we do not want the original check!
        validator.add(
            "in1 needs to be smaller than 10 and in2 needs to be bigger than 10", [&] { return in1 < 10 && in2 > 10; },
            in1, in2);
      }
    };

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      ModuleAmod moduleA{this, "ModuleA", ""};
    };

    // Implementation note about the test: Except for the setting (one single add() call combining both checks instead of
    // two separate add() calls) this test can be identical to testMultipleVariablesDifferentChecks. The only difference
    // in behaviour is the different message, which is defined in the add() call (and hence outside the code under test).

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto in1 = test.getScalar<int>("/ModuleA/in1");
    auto in2 = test.getScalar<int>("/ModuleA/in2");

    test.setScalarDefault<int>("/ModuleA/in1", 3);
    test.setScalarDefault<int>("/ModuleA/in2", 12);

    test.runApplication();

    BOOST_TEST(!in1.readLatest());
    BOOST_TEST(!in2.readLatest());

    in1.setAndWrite(15);
    test.stepApplication();
    BOOST_TEST(in1.readLatest());
    BOOST_TEST(in1 == 3);
    BOOST_TEST(app.moduleA.in1 == 3);
    BOOST_TEST(app.moduleA.in2 == 12);

    in1.setAndWrite(9);
    test.stepApplication();
    BOOST_TEST(!in1.readLatest());
    BOOST_TEST(app.moduleA.in1 == 9);
    BOOST_TEST(app.moduleA.in2 == 12);

    BOOST_TEST(!in2.readLatest());

    in2.setAndWrite(7);
    test.stepApplication();
    BOOST_TEST(in2.readLatest());
    BOOST_TEST(in2 == 12);
    BOOST_TEST(app.moduleA.in1 == 9);
    BOOST_TEST(app.moduleA.in2 == 12);

    in2.setAndWrite(13);
    test.stepApplication();
    BOOST_TEST(!in2.readLatest());
    BOOST_TEST(app.moduleA.in1 == 9);
    BOOST_TEST(app.moduleA.in2 == 13);

    BOOST_TEST(!in1.readLatest());
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testMultipleChecksSameVariable) {
    std::cout << "testMultipleChecksSameVariable" << std::endl;
    // add multiple UserInputValidator::add() calls all checking the same variable i1

    struct ModuleAmod : public ModuleA {
      using ModuleA::ModuleA;

      void prepare() override {
        ModuleA::prepare(); // defines check for in1 < 10
        validator.add("in1 needs to be greater than -5", [&] { return in1 > -5; }, in1);
      }
    };

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      ModuleAmod moduleA{this, "ModuleA", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto in1 = test.getScalar<int>("/ModuleA/in1");

    test.setScalarDefault<int>("/ModuleA/in1", 3);

    test.runApplication();

    BOOST_TEST(!in1.readLatest());

    in1.setAndWrite(15);
    test.stepApplication();
    BOOST_TEST(in1.readLatest());
    BOOST_TEST(in1 == 3);
    BOOST_TEST(app.moduleA.in1 == 3);

    in1.setAndWrite(9);
    test.stepApplication();
    BOOST_TEST(!in1.readLatest());
    BOOST_TEST(app.moduleA.in1 == 9);

    in1.setAndWrite(-7);
    test.stepApplication();
    BOOST_TEST(in1.readLatest());
    BOOST_TEST(in1 == 9);
    BOOST_TEST(app.moduleA.in1 == 9);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testSetErrorFunction) {
    std::cout << "testSetErrorFunction" << std::endl;
    // check that setErrorFunction is called with the right message (need multiple checks with different messages)

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      ModuleAwithSecondInput moduleA{this, "ModuleA", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    std::string errorMessage;

    app.moduleA.validator.setErrorFunction([&](const std::string& msg) { errorMessage = msg; });

    auto modAin1 = test.getScalar<int>("/ModuleA/in1");
    auto modAin2 = test.getScalar<int>("/ModuleA/in2");

    test.setScalarDefault<int>("/ModuleA/in2", 20);

    test.runApplication();

    modAin1.setAndWrite(8);
    test.stepApplication();
    BOOST_TEST(!modAin1.readLatest());
    BOOST_TEST(app.moduleA.in1 == 8);
    BOOST_TEST(errorMessage.empty());

    modAin1.setAndWrite(10);
    test.stepApplication();
    BOOST_TEST(modAin1.readLatest());
    BOOST_TEST(modAin1 == 8);
    BOOST_TEST(app.moduleA.in1 == 8);
    BOOST_TEST(errorMessage == app.moduleA.in1ErrorMessage);

    modAin2.setAndWrite(1);
    test.stepApplication();
    BOOST_TEST(modAin2.readLatest());
    BOOST_TEST(modAin2 == 20);
    BOOST_TEST(app.moduleA.in2 == 20);
    BOOST_TEST(errorMessage == app.moduleA.in2ErrorMessage);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testBackwardsPropagationSingleDownstream) {
    std::cout << "testBackwardsPropagationSingleDownstream" << std::endl;
    // check that two modules with each one validator connected to each other propagate rejections from the downstream
    // module to the upstream and the control system eventually
    // Note: This is new functionality implemeted as part of #11558

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      UpstreamSingleOut upstream{this, "Upstream", ""};
      ModuleA downstream{this, "Downstream", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto upstrIn = test.getScalar<int>("/Upstream/in1");
    auto downstrIn = test.getScalar<int>("/Downstream/in1");

    test.setScalarDefault<int>("/Upstream/in1", 5);

    test.runApplication();

    // discard initial values
    downstrIn.readLatest();
    BOOST_TEST(downstrIn == 6);

    upstrIn.setAndWrite(30);
    test.stepApplication();
    BOOST_TEST(upstrIn.readNonBlocking());
    BOOST_TEST(upstrIn == 5);
    BOOST_TEST(!downstrIn.readNonBlocking()); // validation happens in upstream, not really part of this test case

    upstrIn.setAndWrite(12);
    test.stepApplication();
    BOOST_TEST(upstrIn.readNonBlocking());
    BOOST_TEST(upstrIn == 5);
    BOOST_TEST(downstrIn.readLatest());
    BOOST_TEST(downstrIn == 6);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testBackwardsPropagationTwoDownstream) {
    std::cout << "testBackwardsPropagationTwoDownstream" << std::endl;
    // Same as testBackwardsPropagationSingleDownstream but with two downstream modules (different PVs)
    // Note: This is new functionality implemeted as part of #11558

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      UpstreamTwinOut upstream{this, "Upstream", ""};
      ModuleA downstream1{this, "Downstream1", ""};
      ModuleA downstream2{this, "Downstream2", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto upstrIn = test.getScalar<int>("/Upstream/in1");
    auto downstr1In = test.getScalar<int>("/Downstream1/in1");
    auto downstr2In = test.getScalar<int>("/Downstream2/in1");

    test.setScalarDefault<int>("/Upstream/in1", 5);

    test.runApplication();

    // discard initial values
    downstr1In.readLatest();
    BOOST_TEST(downstr1In == 6);
    downstr2In.readLatest();
    BOOST_TEST(downstr2In == 7);

    upstrIn.setAndWrite(30);
    test.stepApplication();
    BOOST_TEST(upstrIn.readNonBlocking());
    BOOST_TEST(upstrIn == 5);
    BOOST_TEST(!downstr1In.readNonBlocking()); // validation happens in upstream, not really part of this test case
    BOOST_TEST(!downstr2In.readNonBlocking());

    upstrIn.setAndWrite(12);
    test.stepApplication();
    BOOST_TEST(upstrIn.readNonBlocking());
    BOOST_TEST(upstrIn == 5);
    BOOST_TEST(downstr1In.readLatest());
    BOOST_TEST(downstr1In == 6);
    BOOST_TEST(downstr2In.readLatest());
    BOOST_TEST(downstr2In == 7);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testFunnelThreadedFanOut) {
    std::cout << "testFunnelThreadedFanOut" << std::endl;
    // Similar to testFunnelThreadedFanOut but with an upstream module the return channel is funneled into rather
    // than the control system

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      ModuleA module1{this, "Module", ""};
      ModuleA module2{this, "Module", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto in1 = test.getScalar<int>("/Module/in1");

    test.setScalarDefault<int>("/Module/in1", 5);

    test.runApplication();

    in1.setAndWrite(30);
    test.stepApplication();
    BOOST_TEST(in1.readNonBlocking());
    BOOST_TEST(in1 == 5);
    BOOST_TEST(app.module1.in1 == 5);
    BOOST_TEST(app.module2.in1 == 5);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testFunnelFeedingFanOut) {
    std::cout << "testFunnelFeedingFanOut" << std::endl;
    // Two modules both having the same PV as an input (with return channel) which is validated
    // Note: This is new functionality implemeted as part of #11558

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      UpstreamSingleOut upstream{this, "Upstream", ""};
      ModuleA module1{this, "Downstream", ""};
      ModuleA module2{this, "Downstream", ""};
    };

    TestApplication app("TestApp");
    ctk::TestFacility test(app);

    auto in1 = test.getScalar<int>("/Upstream/in1");

    test.setScalarDefault<int>("/Upstream/in1", 5);

    test.runApplication();

    in1.setAndWrite(30);
    test.stepApplication();
    BOOST_TEST(in1.readNonBlocking());
    BOOST_TEST(in1 == 5);
    BOOST_TEST(app.module1.in1 == 6);
    BOOST_TEST(app.module2.in1 == 6);
  }

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDeepBackwardsPropagation) {
    std::cout << "testDeepBackwardsPropagation" << std::endl;
    // Like testBackwardsPropagationSingleDownstream, but with deeper validation chain and new input values arriving at
    // the upstream module before rejections from downstream.

    struct TestApplication : public ctk::Application {
      using ctk::Application::Application;
      ~TestApplication() override { shutdown(); }
      UpstreamSingleOut upstream{this, "Upstream", ""};
      UpstreamSingleOut midstream{this, "Midstream", ""};
      ModuleA downstream{this, "Downstream", ""};
    };

    TestApplication app("TestApp");
    app.upstream.out1 = {&app.upstream, "/Midstream/in1", "", "First validated input"};

    ctk::TestFacility test(app);

    auto upstrIn = test.getScalar<int>("/Upstream/in1");
    auto midstreamIn = test.getScalar<int>("/Midstream/in1");
    auto downstrIn = test.getScalar<int>("/Downstream/in1");

    test.setScalarDefault<int>("/Upstream/in1", 5);

    test.runApplication();

    // discard initial values
    midstreamIn.readLatest();
    downstrIn.readLatest();
    BOOST_TEST(midstreamIn == 6);
    BOOST_TEST(downstrIn == 7);
    // test a single value being discarded at the lowest level (Downstream)
    upstrIn.setAndWrite(12);
    test.stepApplication();
    BOOST_TEST(midstreamIn.readNonBlocking());
    BOOST_TEST(midstreamIn == 13); // first value is coming from Upstream
    BOOST_TEST(downstrIn.readNonBlocking());
    BOOST_TEST(downstrIn == 14); // first value is passed through by Midstream
    BOOST_TEST(downstrIn.readNonBlocking());
    BOOST_TEST(downstrIn == 7); // correction value coming back from Downstream
    BOOST_TEST(!downstrIn.readNonBlocking());
    BOOST_TEST(midstreamIn.readNonBlocking());
    BOOST_TEST(midstreamIn == 6); // correction value coming back from Midstream
    BOOST_TEST(!midstreamIn.readNonBlocking());
    BOOST_TEST(upstrIn.readNonBlocking());
    BOOST_TEST(upstrIn == 5); // correction value coming back from Upstream
    BOOST_TEST(!upstrIn.readNonBlocking());

    // test two consecutive values both being discarded at the lowest level
    // Note: Writing two values into the upstrIn queue will make Upstream process the second value before the correction
    // for the first value coming from Downstream, because readAny() will process updates in sequences of arrival
    // (on notification queue). Apart from this it is not well defined (aka subject to race condition) where the second
    // value from Upstream and the correction of the first value from Downstream cross.
    upstrIn.setAndWrite(12);
    upstrIn.setAndWrite(13);
    test.stepApplication();
    BOOST_TEST(midstreamIn.readNonBlocking());
    BOOST_TEST(midstreamIn == 13); // first value is coming from upstream
    BOOST_TEST(downstrIn.readNonBlocking());
    BOOST_TEST(downstrIn == 14); // first value is coming from upstream/midstream

    BOOST_TEST(downstrIn.readLatest()); // just observe final state, because intermediate states might be subject
    BOOST_TEST(downstrIn == 7);         // to race conditions
    BOOST_TEST(midstreamIn.readLatest());
    BOOST_TEST(midstreamIn == 6);
    BOOST_TEST(upstrIn.readLatest());
    BOOST_TEST(upstrIn == 5);

    // test two consecutive values, only the first being discarded at the lowest level and the second is accepted
    size_t retry = 0;
  repeat:
    upstrIn.setAndWrite(12);
    upstrIn.setAndWrite(3);

    test.stepApplication();

    // There are two acceptable outcomes of this test:
    //
    // 1) Likely: The first value was rejected and the second was accepted
    //
    // 2) Unlikely: Both values are rejected. This can happen because the UserInputValidator of midstream needs to use a
    //    fresh VersionNumber to propagate the rejection of the first value from downstream (otherwise the scenario in
    //    testBackwardsPropagationTwoDownstream would break). Since that fresh VersionNumber is bigger then the one of
    //    the second, valid value, it can overwrite the second value and hence that gets effectively rejected.
    //
    // Currently, we accept both scenarios but require that the likely scenario is observed (by retrying a couple of
    // times if we see the second scenario). This problem can be solved by extending the VersionNumber with a
    // sub-version, so we can both distinguish the corrected from the rejected values as well as find out to which
    // original VersionNumber the corrected value belongs. Due to the fact that this problem only occurs when writing
    // inputs faster than the values get rejected and the UserInputValidator being designed for inputs by users, this
    // problem does not seem to play a big role in real scenarios.

    BOOST_TEST(downstrIn.readLatest()); // just observe final state, to avoid intermediate races
    BOOST_TEST(midstreamIn.readLatest());
    if(downstrIn == 7) {
      BOOST_TEST(upstrIn.readLatest());
      BOOST_TEST(midstreamIn == 6);
      BOOST_TEST(upstrIn == 5);
      if(++retry < 100) {
        goto repeat;
      }
      BOOST_ERROR("The wanted 'likely' scenario could not be observed, only the unwanted 'unlikely'.");
    }
    BOOST_TEST(!upstrIn.readLatest());
    BOOST_TEST(downstrIn == 5);
    BOOST_TEST(midstreamIn == 4);
    BOOST_TEST(upstrIn == 3);
  }

  /********************************************************************************************************************/

} // namespace Tests::testUserInputValidator
