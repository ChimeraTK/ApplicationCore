// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestFacility.h"

#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testVariableGroup

#include "Application.h"
#include "ApplicationModule.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>

namespace Tests::testVariableGroup {

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;

  /********************************************************************************************************************/
  /* the ApplicationModule for the test is a template of the user type */

  struct InputModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct MixedGroup : public ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<int> consumingPush{this, "feedingPush", "MV/m", "Description"};
      ctk::ScalarPushInput<int> consumingPush2{this, "feedingPush2", "MV/m", "Description"};
      ctk::ScalarPushInput<int> consumingPush3{this, "feedingPush3", "MV/m", "Description"};
      ctk::ScalarPollInput<int> consumingPoll{this, "feedingPoll", "MV/m", "Description"};
      ctk::ScalarPollInput<int> consumingPoll2{this, "feedingPoll2", "MV/m", "Description"};
      ctk::ScalarPollInput<int> consumingPoll3{this, "feedingPoll3", "MV/m", "Description"};
    } mixedGroup{this, ".", "A group with both push and poll inputs"};

    void prepare() override {
      incrementDataFaultCounter(); // force all outputs to invalid
      writeAll();
      decrementDataFaultCounter(); // validity according to input validity
    }

    void mainLoop() override {}
  };

  struct OutputModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarOutput<int> feedingPush{this, "feedingPush", "MV/m", "Description"};
    ctk::ScalarOutput<int> feedingPush2{this, "feedingPush2", "MV/m", "Description"};
    ctk::ScalarOutput<int> feedingPush3{this, "feedingPush3", "MV/m", "Description"};
    ctk::ScalarOutput<int> feedingPoll{this, "feedingPoll", "MV/m", "Description"};
    ctk::ScalarOutput<int> feedingPoll2{this, "feedingPoll2", "MV/m", "Description"};
    ctk::ScalarOutput<int> feedingPoll3{this, "feedingPoll3", "MV/m", "Description"};

    void prepare() override {
      incrementDataFaultCounter(); // force all outputs to invalid
      writeAll();
      decrementDataFaultCounter(); // validity according to input validity
    }

    void mainLoop() override {}
  };

  /********************************************************************************************************************/
  /* dummy application */

  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {}
    ~TestApplication() override { shutdown(); }

    InputModule in{this, "out", "The test module"};
    OutputModule out{this, "out", "The other test module"};
  };

  /********************************************************************************************************************/
  /* test module-wide read/write operations */

  BOOST_AUTO_TEST_CASE(testModuleReadWrite) {
    std::cout << "**************************************************************************************************\n";
    std::cout << "*** testModuleReadWrite" << std::endl;

    TestApplication app;
    ctk::TestFacility test(app);

    test.runApplication();

    // single threaded test
    app.in.mixedGroup.consumingPush = 666;
    app.in.mixedGroup.consumingPush2 = 666;
    app.in.mixedGroup.consumingPush3 = 666;
    app.in.mixedGroup.consumingPoll = 666;
    app.in.mixedGroup.consumingPoll2 = 666;
    app.in.mixedGroup.consumingPoll3 = 666;
    app.out.feedingPush = 18;
    app.out.feedingPush2 = 20;
    app.out.feedingPush3 = 22;
    app.out.feedingPoll = 23;
    app.out.feedingPoll2 = 24;
    app.out.feedingPoll3 = 27;
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 666);
    app.out.writeAll();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 666);
    app.in.readAll();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 20);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 24);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 27);

    app.in.readAllNonBlocking();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 20);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 24);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 27);

    app.out.feedingPush2 = 30;
    app.out.feedingPoll2 = 33;
    app.out.writeAll();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 20);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 24);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 27);
    app.in.readAllNonBlocking();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 27);
    app.in.readAllNonBlocking();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 27);

    app.out.feedingPush = 35;
    app.out.feedingPoll3 = 40;
    app.out.writeAll();
    app.out.feedingPush = 36;
    app.out.feedingPoll3 = 44;
    app.out.writeAll();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 27);
    app.in.readAllNonBlocking();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 35);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 44);
    app.in.readAllNonBlocking();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 36);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 44);
    app.in.readAllNonBlocking();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 36);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 44);

    app.out.feedingPush = 45;
    app.out.writeAll();
    app.out.feedingPush = 46;
    app.out.writeAll();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 36);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 44);
    app.in.readAllLatest();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 46);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 44);
    app.in.readAllLatest();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 46);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 44);
  }

  /********************************************************************************************************************/
  /* test trigger by app variable when connecting a polled device register to an app variable */

  BOOST_AUTO_TEST_CASE(testReadAny) {
    std::cout << "**************************************************************************************************\n";
    std::cout << "==> testReadAny" << std::endl;

    TestApplication app;
    ctk::TestFacility test(app);

    test.runApplication();

    auto group = app.in.mixedGroup.readAnyGroup();

    // single threaded test
    app.out.feedingPush = 0;
    app.out.feedingPush2 = 42;
    app.out.feedingPush3 = 120;
    app.out.feedingPoll = 10;
    app.out.feedingPoll2 = 11;
    app.out.feedingPoll3 = 12;
    app.out.feedingPoll.write();
    app.out.feedingPoll2.write();
    app.out.feedingPoll3.write();

    BOOST_CHECK(app.in.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 0);

    // test a single write
    app.out.feedingPush2.write();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 0);
    auto id = group.readAny();
    BOOST_CHECK(id == app.in.mixedGroup.consumingPush2.getId());
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 42);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 12);

    // two more writes
    app.out.feedingPush2 = 666;
    app.out.feedingPush2.write();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 42);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 12);
    id = group.readAny();
    BOOST_CHECK(id == app.in.mixedGroup.consumingPush2.getId());
    app.out.feedingPush3.write();
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 12);
    id = group.readAny();
    BOOST_CHECK(id == app.in.mixedGroup.consumingPush3.getId());
    BOOST_CHECK(app.in.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.in.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.in.mixedGroup.consumingPush3 == 120);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.in.mixedGroup.consumingPoll3 == 12);
  }

} // namespace Tests::testVariableGroup
