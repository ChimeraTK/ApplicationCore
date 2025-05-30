// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testOptimiseUnmappedVariables

#include "Application.h"
#include "Multiplier.h"
#include "Pipe.h"
#include "TestFacility.h"

#include <libxml++/libxml++.h>

#include <boost/filesystem.hpp>
#include <boost/mpl/list.hpp>
#include <boost/thread.hpp>

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

namespace Tests::testOptimiseUnmappedVariables {

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;

  /********************************************************************************************************************/
  /* Application without name */

  struct TestApp : public ctk::Application {
    explicit TestApp(const std::string& name) : ctk::Application(name) {}
    ~TestApp() override { shutdown(); }

    ctk::ConstMultiplier<double> multiplierD{this, "Multiplier", "Some module", 42};
    ctk::ScalarPipe<double> pipe{this, "/Multiplier/output", "/mySubModule/output", "unit", "Some pipe module"};
  };

  /********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOptimiseUnmappedVariables) {
    std::cout << "***************************************************************" << std::endl;
    std::cout << "==> testOptimiseUnmappedVariables" << std::endl;

    // test without even calling the function
    {
      TestApp app("testApp");
      app.getModel().writeGraphViz("testOptimiseUnmappedVariables.dot");
      ctk::TestFacility test{app};
      auto input = test.getScalar<double>("/Multiplier/input");
      auto tap = test.getScalar<double>("/Multiplier/output");
      auto output = test.getScalar<double>("/mySubModule/output");
      test.runApplication();
      input = 10;
      input.write();
      test.stepApplication();
      BOOST_CHECK(tap.readNonBlocking());
      BOOST_CHECK_CLOSE(double(tap), 420., 0.001);
      BOOST_CHECK(!tap.readNonBlocking());
      BOOST_CHECK(output.readNonBlocking());
      BOOST_CHECK_CLOSE(double(output), 420., 0.001);
      BOOST_CHECK(!output.readNonBlocking());
    }

    // test passing empty set
    {
      TestApp app("testApp");
      ctk::TestFacility test{app};
      auto input = test.getScalar<double>("/Multiplier/input");
      auto tap = test.getScalar<double>("/Multiplier/output");
      auto output = test.getScalar<double>("/mySubModule/output");
      app.optimiseUnmappedVariables({});
      test.runApplication();
      input = 10;
      input.write();
      test.stepApplication();
      BOOST_CHECK(tap.readNonBlocking());
      BOOST_CHECK_CLOSE(double(tap), 420., 0.001);
      BOOST_CHECK(!tap.readNonBlocking());
      BOOST_CHECK(output.readNonBlocking());
      BOOST_CHECK_CLOSE(double(output), 420., 0.001);
      BOOST_CHECK(!output.readNonBlocking());
    }

    // test passing single variable
    {
      TestApp app("testApp");
      ctk::TestFacility test{app};
      auto input = test.getScalar<double>("/Multiplier/input");
      auto tap = test.getScalar<double>("/Multiplier/output");
      auto output = test.getScalar<double>("/mySubModule/output");
      app.optimiseUnmappedVariables({"/Multiplier/output"});
      test.runApplication();
      input = 10;
      input.write();
      test.stepApplication();
      BOOST_CHECK(!tap.readNonBlocking());
      BOOST_CHECK(output.readNonBlocking());
      BOOST_CHECK_CLOSE(double(output), 420., 0.001);
      BOOST_CHECK(!output.readNonBlocking());
    }

    // test passing two variables
    {
      TestApp app("testApp");
      ctk::TestFacility test{app};
      auto input = test.getScalar<double>("/Multiplier/input");
      auto tap = test.getScalar<double>("/Multiplier/output");
      auto output = test.getScalar<double>("/mySubModule/output");
      app.optimiseUnmappedVariables({"/Multiplier/output", "/mySubModule/output"});
      test.runApplication();
      input = 10;
      input.write();
      test.stepApplication();
      BOOST_CHECK(!tap.readNonBlocking());
      BOOST_CHECK(!output.readNonBlocking());
    }

    // test passing unknown variables
    {
      TestApp app("testApp");
      ctk::TestFacility test{app};
      auto input = test.getScalar<double>("/Multiplier/input");
      auto tap = test.getScalar<double>("/Multiplier/output");
      auto output = test.getScalar<double>("/mySubModule/output");
      BOOST_CHECK_THROW(app.optimiseUnmappedVariables({"/Multiplier/output", "/this/is/not/known"}), std::out_of_range);
    }
  }

} // namespace Tests::testOptimiseUnmappedVariables
