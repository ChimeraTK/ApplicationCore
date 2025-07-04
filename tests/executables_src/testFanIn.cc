// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
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

  class TestApp : public ctk::Application {
   public:
    TestApp() : ctk::Application("TestApp") {}
    ~TestApp() override { shutdown(); }

    class TheSender : public ctk::ApplicationModule {
     public:
      using ctk::ApplicationModule::ApplicationModule;
      ctk::ScalarOutput<int32_t> out{this, "/path/to/fanIn", "", ""};
      void mainLoop() override {}
      void prepare() override { out.setAndWrite(1); }
    };
    TheSender a{this, "a", ""};
    TheSender b{this, "b", ""};

    class TheReceiver : public ctk::ApplicationModule {
     public:
      using ctk::ApplicationModule::ApplicationModule;

      // just for checking that we can use stateful lambdas as well...
      int offset{10};

      ctk::ScalarFanIn<int32_t> in{this, "fanIn", "", "", [&](auto id, auto map) { return map[id] + offset; }};

      void mainLoop() override {
        offset = 17;
        auto rag = readAnyGroup();
        while(true) {
          change = rag.readAny();
        }
      }

      ctk::TransferElementID change;
    };
    TheReceiver r{this, "/path/to", ""};
  };

  /********************************************************************************************************************/

  // Test result of the aggregated value output
  BOOST_AUTO_TEST_CASE(TestAggregatedValue) {
    std::cout << "***************************************************************" << std::endl;
    std::cout << "==> TestAggregatedValue" << std::endl;

    TestApp app;
    ctk::TestFacility test{app};

    auto out = test.getScalar<int32_t>("/path/to/fanIn");

    test.runApplication();

    // initial value (both a and b are sending 1 in prepare())
    BOOST_TEST(out.readNonBlocking());
    BOOST_TEST(out == 1 + 10); // offset is 10 at the beginning and changed to 17 only after entering the mainloop
    BOOST_TEST(!out.readNonBlocking());

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

} // namespace Tests::testFanIn
