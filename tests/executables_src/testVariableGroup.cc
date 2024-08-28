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

  /*********************************************************************************************************************/
  /* the ApplicationModule for the test is a template of the user type */

  struct TestModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct MixedGroup : public ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<int> consumingPush{this, "feedingPush", "MV/m", "Descrption"};
      ctk::ScalarPushInput<int> consumingPush2{this, "feedingPush2", "MV/m", "Descrption"};
      ctk::ScalarPushInput<int> consumingPush3{this, "feedingPush3", "MV/m", "Descrption"};
      ctk::ScalarPollInput<int> consumingPoll{this, "feedingPoll", "MV/m", "Descrption"};
      ctk::ScalarPollInput<int> consumingPoll2{this, "feedingPoll2", "MV/m", "Descrption"};
      ctk::ScalarPollInput<int> consumingPoll3{this, "feedingPoll3", "MV/m", "Descrption"};
    };
    MixedGroup mixedGroup{this, ".", "A group with both push and poll inputs"};

    ctk::ScalarOutput<int> feedingPush{this, "feedingPush", "MV/m", "Descrption"};
    ctk::ScalarOutput<int> feedingPush2{this, "feedingPush2", "MV/m", "Descrption"};
    ctk::ScalarOutput<int> feedingPush3{this, "feedingPush3", "MV/m", "Descrption"};
    ctk::ScalarOutput<int> feedingPoll{this, "feedingPoll", "MV/m", "Descrption"};
    ctk::ScalarOutput<int> feedingPoll2{this, "feedingPoll2", "MV/m", "Descrption"};
    ctk::ScalarOutput<int> feedingPoll3{this, "feedingPoll3", "MV/m", "Descrption"};

    void prepare() override {
      incrementDataFaultCounter(); // foce all outputs to invalid
      writeAll();
      decrementDataFaultCounter(); // validity according to input validity
    }

    void mainLoop() override {}
  };

  /*********************************************************************************************************************/
  /* dummy application */

  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {}
    ~TestApplication() override { shutdown(); }

    TestModule testModule{this, "testModule", "The test module"};
  };

  /*********************************************************************************************************************/
  /* test module-wide read/write operations */

  BOOST_AUTO_TEST_CASE(testModuleReadWrite) {
    std::cout << "**************************************************************************************************\n";
    std::cout << "*** testModuleReadWrite" << std::endl;

    TestApplication app;
    ctk::TestFacility test(app);

    test.runApplication();

    // single theaded test
    app.testModule.mixedGroup.consumingPush = 666;
    app.testModule.mixedGroup.consumingPush2 = 666;
    app.testModule.mixedGroup.consumingPush3 = 666;
    app.testModule.mixedGroup.consumingPoll = 666;
    app.testModule.mixedGroup.consumingPoll2 = 666;
    app.testModule.mixedGroup.consumingPoll3 = 666;
    app.testModule.feedingPush = 18;
    app.testModule.feedingPush2 = 20;
    app.testModule.feedingPush3 = 22;
    app.testModule.feedingPoll = 23;
    app.testModule.feedingPoll2 = 24;
    app.testModule.feedingPoll3 = 27;
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 666);
    app.testModule.writeAll();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 666);
    app.testModule.readAll();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 20);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 24);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 27);

    app.testModule.readAllNonBlocking();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 20);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 24);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 27);

    app.testModule.feedingPush2 = 30;
    app.testModule.feedingPoll2 = 33;
    app.testModule.writeAll();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 20);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 24);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 27);
    app.testModule.readAllNonBlocking();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 27);
    app.testModule.readAllNonBlocking();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 27);

    app.testModule.feedingPush = 35;
    app.testModule.feedingPoll3 = 40;
    app.testModule.writeAll();
    app.testModule.feedingPush = 36;
    app.testModule.feedingPoll3 = 44;
    app.testModule.writeAll();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 18);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 27);
    app.testModule.readAllNonBlocking();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 35);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 44);
    app.testModule.readAllNonBlocking();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 36);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 44);
    app.testModule.readAllNonBlocking();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 36);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 44);

    app.testModule.feedingPush = 45;
    app.testModule.writeAll();
    app.testModule.feedingPush = 46;
    app.testModule.writeAll();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 36);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 44);
    app.testModule.readAllLatest();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 46);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 44);
    app.testModule.readAllLatest();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 46);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 30);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 22);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 23);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 33);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 44);
  }

  /*********************************************************************************************************************/
  /* test trigger by app variable when connecting a polled device register to an app variable */

  BOOST_AUTO_TEST_CASE(testReadAny) {
    std::cout << "**************************************************************************************************\n";
    std::cout << "==> testReadAny" << std::endl;

    TestApplication app;
    ctk::TestFacility test(app);

    test.runApplication();

    auto group = app.testModule.mixedGroup.readAnyGroup();

    // single theaded test
    app.testModule.feedingPush = 0;
    app.testModule.feedingPush2 = 42;
    app.testModule.feedingPush3 = 120;
    app.testModule.feedingPoll = 10;
    app.testModule.feedingPoll2 = 11;
    app.testModule.feedingPoll3 = 12;
    app.testModule.feedingPoll.write();
    app.testModule.feedingPoll2.write();
    app.testModule.feedingPoll3.write();

    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 0);

    // test a single write
    app.testModule.feedingPush2.write();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 0);
    auto id = group.readAny();
    BOOST_CHECK(id == app.testModule.mixedGroup.consumingPush2.getId());
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 42);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 12);

    // two more writes
    app.testModule.feedingPush2 = 666;
    app.testModule.feedingPush2.write();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 42);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 12);
    id = group.readAny();
    BOOST_CHECK(id == app.testModule.mixedGroup.consumingPush2.getId());
    app.testModule.feedingPush3.write();
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 12);
    id = group.readAny();
    BOOST_CHECK(id == app.testModule.mixedGroup.consumingPush3.getId());
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush == 0);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush2 == 666);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPush3 == 120);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll == 10);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll2 == 11);
    BOOST_CHECK(app.testModule.mixedGroup.consumingPoll3 == 12);
  }

} // namespace Tests::testVariableGroup