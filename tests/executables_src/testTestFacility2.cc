// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testTestFaciliy2
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "Application.h"
#include "ApplicationModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

namespace Tests::testTestFaciliy2 {

  namespace ctk = ChimeraTK;

  struct MyModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPushInput<double> input{this, "/input", "", ""};
    ctk::ScalarOutput<double> output{this, "/output", "", ""};

    void mainLoop() override {
      std::cout << "starting main loop" << std::endl;
      output = 2 * double(input);
      output.write();

      while(true) {
        input.read();
        output = 3 * double(input);
        output.write();
      }
    }
  };

  /**********************************************************************************************************************/

  struct TestApp : public ctk::Application {
    TestApp() : Application("TestApp") {}
    ~TestApp() override { shutdown(); }

    MyModule myModule{this, "MyModule", ""};
  };

  /**********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testSumLimiter) {
    TestApp theTestApp;
    ChimeraTK::TestFacility testFacility(theTestApp);
    testFacility.setScalarDefault<double>("/input", 25.);

    testFacility.runApplication();

    // at this point all main loops should have started, default values are processed and inputs waiting in read()

    BOOST_CHECK_CLOSE(testFacility.readScalar<double>("/output"), 50., 0.001);

    testFacility.writeScalar<double>("/input", 30.);
    std::cout << "about to step" << std::endl;
    // however, the main loop only starts in the first step.
    testFacility.stepApplication();
    std::cout << "step finished" << std::endl;

    BOOST_CHECK_CLOSE(testFacility.readScalar<double>("/output"), 90., 0.001);
  }

  /**********************************************************************************************************************/

} // namespace Tests::testTestFaciliy2