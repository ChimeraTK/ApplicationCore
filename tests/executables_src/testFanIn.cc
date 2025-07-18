// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "UserInputValidator.h"
#define BOOST_TEST_MODULE testFanIn

#include "Application.h"
#include "ApplicationModule.h"
#include "FanIn.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

namespace Tests::testFanIn {

  /********************************************************************************************************************/

  class TheSender : public ctk::ApplicationModule {
   public:
    using ctk::ApplicationModule::ApplicationModule;
    ctk::ScalarOutput<int32_t> out{this, "/path/to/fanIn", "", ""};
    void mainLoop() override {}
    void prepare() override { out.setAndWrite(1); }
  };

  /********************************************************************************************************************/

  class TheReceiverBase : public ctk::ApplicationModule {
   public:
    using ctk::ApplicationModule::ApplicationModule;

    void mainLoop() override {
      auto rag = readAnyGroup();
      while(true) {
        change = rag.readAny();
      }
    }

    ctk::TransferElementID change;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  /********************************************************************************************************************/
  // Test result of the aggregated value output

  class TestAggregatedValueApp : public ctk::Application {
   public:
    TestAggregatedValueApp() : ctk::Application("TestApp") {}
    ~TestAggregatedValueApp() override { shutdown(); }

    TheSender a{this, "a", ""};
    TheSender b{this, "b", ""};

    class TheReceiver : public TheReceiverBase {
     public:
      using TheReceiverBase::TheReceiverBase;

      // just for checking that we can use stateful lambdas as well...
      int offset{10};

      ctk::ScalarFanIn<int32_t> in{this, "fanIn", "", "", [&](auto id, auto map) { return map[id] + offset; }};

      void mainLoop() override {
        offset = 17;
        TheReceiverBase::mainLoop();
      }
    };
    TheReceiver r{this, "/path/to", ""};
  };

  BOOST_AUTO_TEST_CASE(TestAggregatedValue) {
    std::cout << "***************************************************************" << std::endl;
    std::cout << "==> TestAggregatedValue" << std::endl;

    TestAggregatedValueApp app;

    ctk::TestFacility test{app};

    auto out = test.getScalar<int32_t>("/path/to/fanIn");

    test.runApplication();

    // initial value (both a and b are sending 1 in prepare())
    BOOST_TEST(out == 1 + 10); // offset is 10 at the beginning and changed to 17 only after entering the mainloop
    BOOST_TEST(!out.readNonBlocking());
    BOOST_TEST(app.r.in == 1 + 10);

    app.b.out.setAndWrite(42);
    test.stepApplication();
    BOOST_TEST(app.r.in == 42 + 17);
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 42 + 17);
    BOOST_TEST(!out.readNonBlocking());

    app.a.out.setAndWrite(43);
    test.stepApplication();
    BOOST_TEST(app.r.in == 43 + 17);
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 43 + 17);
    BOOST_TEST(!out.readNonBlocking());
  }

  /********************************************************************************************************************/

  class TestAdditionalInputsApp : public ctk::Application {
   public:
    TestAdditionalInputsApp() : ctk::Application("TestApp") {}
    ~TestAdditionalInputsApp() override { shutdown(); }

    TheSender a{this, "a", ""};
    TheSender b{this, "b", ""};

    class TheReceiver : public TheReceiverBase {
     public:
      using TheReceiverBase::TheReceiverBase;

      ctk::ScalarFanIn<int32_t> in{
          this, "fanIn", {"myAdditionalInput", "/absolute/path/input"}, "", "", ctk::fanInKeepLastValue};
    };
    TheReceiver r{this, "/path/to", ""};
  };

  BOOST_AUTO_TEST_CASE(TestAdditionalInputs) {
    std::cout << "***************************************************************" << std::endl;
    std::cout << "==> TestAdditionalInputs" << std::endl;

    TestAdditionalInputsApp app;

    ctk::TestFacility test{app};

    auto out = test.getScalar<int32_t>("/path/to/fanIn");
    auto addIn = test.getScalar<int32_t>("/path/to/myAdditionalInput");
    auto absIn = test.getScalar<int32_t>("/absolute/path/input");

    // keep all inital values identical, to avoid undefined result
    test.setScalarDefault("/path/to/myAdditionalInput", 1);
    test.setScalarDefault("/absolute/path/input", 1);

    test.runApplication();

    // initial value (both a and b are sending 1 in prepare())
    BOOST_TEST(out == 1);
    BOOST_TEST(!out.readNonBlocking());
    BOOST_TEST(app.r.in == 1);

    app.a.out.setAndWrite(42);
    test.stepApplication();
    BOOST_TEST(app.r.in == 42);

    addIn.setAndWrite(43);
    test.stepApplication();
    BOOST_TEST(app.r.in == 43);

    absIn.setAndWrite(44);
    test.stepApplication();
    BOOST_TEST(app.r.in == 44);
  }

  /********************************************************************************************************************/

  class TestUserInputValidationApp : public ctk::Application {
   public:
    TestUserInputValidationApp() : ctk::Application("TestApp") {}
    ~TestUserInputValidationApp() override { shutdown(); }

    class TheReceiver : public TheReceiverBase {
     public:
      using TheReceiverBase::TheReceiverBase;

      static constexpr auto aggregateSum = [](auto, const auto& map) {
        int result = 0;
        for(auto& a : map) {
          result += a.second;
        }
        return result;
      };

      ctk::ScalarFanInWB<int32_t> in{this, "fanIn", {"a", "b"}, "", "", aggregateSum};
      ctk::ScalarOutput<std::string> err{this, "err", "", ""};
      ctk::UserInputValidator validator;

      void prepare() override {
        validator.setErrorFunction([&](const std::string& msg) {
          std::cout << "---> " << msg << std::endl;
          err.setAndWrite(msg);
        });

        validator.add("testOnAggregated", [&] { return in < 10; }, in.inputs());
        for(auto& acc : in.inputs()) {
          validator.add("testOnIndividual", [&] { return acc > -10; }, acc);
          validator.setFallback(acc, 1);
        }
      }

      void mainLoop() override {
        auto rag = readAnyGroup();
        validator.validate({}); // initial values won't be validated internally by the FanIn

        while(true) {
          change = rag.readAny();

          // this is not needed here in this case, but shouldn't really hurt (will call validators twice though):
          validator.validate(change);
        }
      }
    };
    TheReceiver r{this, "/path/to", ""};
  };

  BOOST_AUTO_TEST_CASE(TestUserInputValidation) {
    std::cout << "***************************************************************" << std::endl;
    std::cout << "==> TestUserInputValidation" << std::endl;

    TestUserInputValidationApp app;

    ctk::TestFacility test{app};

    auto out = test.getScalar<int32_t>("/path/to/fanIn");
    auto a = test.getScalar<int32_t>("/path/to/a");
    auto b = test.getScalar<int32_t>("/path/to/b");
    auto err = test.getScalar<std::string>("/path/to/err");

    test.setScalarDefault<int32_t>("/path/to/a", 1);
    test.setScalarDefault<int32_t>("/path/to/b", -20);

    test.runApplication();

    // initial value (after rejection of out-of-range initial values)
    BOOST_TEST(app.r.in == 2);
    BOOST_TEST(!err.readNonBlocking());
    BOOST_TEST(out == 2);
    BOOST_TEST(!out.readNonBlocking());

    a.setAndWrite(20);
    test.stepApplication();
    BOOST_TEST(app.r.in == 2);
    BOOST_TEST(err.readNonBlocking());
    BOOST_TEST(std::string(err) == "testOnAggregated");
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 2);
    BOOST_TEST(!out.readNonBlocking());

    b.setAndWrite(20);
    test.stepApplication();
    BOOST_TEST(app.r.in == 2);
    BOOST_TEST(err.readNonBlocking());
    BOOST_TEST(std::string(err) == "testOnAggregated");
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 2);
    BOOST_TEST(!out.readNonBlocking());

    a.setAndWrite(-20);
    test.stepApplication();
    BOOST_TEST(app.r.in == 2);
    BOOST_TEST(err.readNonBlocking());
    BOOST_TEST(std::string(err) == "testOnIndividual");
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 2);
    BOOST_TEST(!out.readNonBlocking());

    b.setAndWrite(-20);
    test.stepApplication();
    BOOST_TEST(app.r.in == 2);
    BOOST_TEST(err.readNonBlocking());
    BOOST_TEST(std::string(err) == "testOnIndividual");
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 2);
    BOOST_TEST(!out.readNonBlocking());

    a.setAndWrite(3);
    test.stepApplication();
    BOOST_TEST(app.r.in == 4);
    BOOST_TEST(!err.readNonBlocking());
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 4);
    BOOST_TEST(!out.readNonBlocking());
  }

  /********************************************************************************************************************/

} // namespace Tests::testFanIn
