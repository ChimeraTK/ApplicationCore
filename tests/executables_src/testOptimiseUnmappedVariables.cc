/*
 * testApplication.cc
 *
 *  Created on: Nov 15, 2017
 *      Author: Martin Hierholzer
 */

#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testApplication

#include <boost/filesystem.hpp>
#include <boost/mpl/list.hpp>
#include <boost/thread.hpp>

#include <libxml++/libxml++.h>

#include "Application.h"
#include "ControlSystemModule.h"
#include "Multiplier.h"
#include "Pipe.h"
#include "TestFacility.h"

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/*********************************************************************************************************************/
/* Application without name */

struct TestApp : public ctk::Application {
  TestApp(const std::string& name) : ctk::Application(name) {}
  ~TestApp() { shutdown(); }

  void defineConnections() {
    csmod["Multiplier"]("input") >> multiplierD.input;
    multiplierD.output >> csmod["Multiplier"]("tap");
    multiplierD.output >> pipe.input;
    pipe.output >> csmod["mySubModule"]("output");
    pipe.output >> csmod["mySubModule"]("output_copy");
  }

  ctk::ConstMultiplier<double> multiplierD{this, "multiplierD", "Some module", 42};
  ctk::ScalarPipe<double> pipe{this, "pipe", "unit", "Some pipe module"};
  ctk::ControlSystemModule csmod;
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testOptimiseUnmappedVariables) {
  std::cout << "***************************************************************" << std::endl;
  std::cout << "==> testOptimiseUnmappedVariables" << std::endl;

  // test without even calling the function
  {
    TestApp app("testApp");
    ctk::TestFacility test;
    auto input = test.getScalar<double>("/Multiplier/input");
    auto tap = test.getScalar<double>("/Multiplier/tap");
    auto output = test.getScalar<double>("/mySubModule/output");
    auto output_copy = test.getScalar<double>("/mySubModule/output_copy");
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
    BOOST_CHECK(output_copy.readNonBlocking());
    BOOST_CHECK_CLOSE(double(output_copy), 420., 0.001);
    BOOST_CHECK(!output_copy.readNonBlocking());
  }

  // test passing empty set
  {
    TestApp app("testApp");
    ctk::TestFacility test;
    auto input = test.getScalar<double>("/Multiplier/input");
    auto tap = test.getScalar<double>("/Multiplier/tap");
    auto output = test.getScalar<double>("/mySubModule/output");
    auto output_copy = test.getScalar<double>("/mySubModule/output_copy");
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
    BOOST_CHECK(output_copy.readNonBlocking());
    BOOST_CHECK_CLOSE(double(output_copy), 420., 0.001);
    BOOST_CHECK(!output_copy.readNonBlocking());
  }

  // test passing single variable
  {
    TestApp app("testApp");
    ctk::TestFacility test;
    auto input = test.getScalar<double>("/Multiplier/input");
    auto tap = test.getScalar<double>("/Multiplier/tap");
    auto output = test.getScalar<double>("/mySubModule/output");
    auto output_copy = test.getScalar<double>("/mySubModule/output_copy");
    app.optimiseUnmappedVariables({"/Multiplier/tap"});
    test.runApplication();
    input = 10;
    input.write();
    test.stepApplication();
    BOOST_CHECK(!tap.readNonBlocking());
    BOOST_CHECK(output.readNonBlocking());
    BOOST_CHECK_CLOSE(double(output), 420., 0.001);
    BOOST_CHECK(!output.readNonBlocking());
    BOOST_CHECK(output_copy.readNonBlocking());
    BOOST_CHECK_CLOSE(double(output_copy), 420., 0.001);
    BOOST_CHECK(!output_copy.readNonBlocking());
  }

  // test passing two variables
  {
    TestApp app("testApp");
    ctk::TestFacility test;
    auto input = test.getScalar<double>("/Multiplier/input");
    auto tap = test.getScalar<double>("/Multiplier/tap");
    auto output = test.getScalar<double>("/mySubModule/output");
    auto output_copy = test.getScalar<double>("/mySubModule/output_copy");
    app.optimiseUnmappedVariables({"/Multiplier/tap", "/mySubModule/output"});
    test.runApplication();
    input = 10;
    input.write();
    test.stepApplication();
    BOOST_CHECK(!tap.readNonBlocking());
    BOOST_CHECK(!output.readNonBlocking());
    BOOST_CHECK(output_copy.readNonBlocking());
    BOOST_CHECK_CLOSE(double(output_copy), 420., 0.001);
    BOOST_CHECK(!output_copy.readNonBlocking());
  }

  // test passing two variables, now with the other copy of the output
  {
    TestApp app("testApp");
    ctk::TestFacility test;
    auto input = test.getScalar<double>("/Multiplier/input");
    auto tap = test.getScalar<double>("/Multiplier/tap");
    auto output = test.getScalar<double>("/mySubModule/output");
    auto output_copy = test.getScalar<double>("/mySubModule/output_copy");
    app.optimiseUnmappedVariables({"/Multiplier/tap", "/mySubModule/output_copy"});
    test.runApplication();
    input = 10;
    input.write();
    test.stepApplication();
    BOOST_CHECK(!tap.readNonBlocking());
    BOOST_CHECK(output.readNonBlocking());
    BOOST_CHECK_CLOSE(double(output), 420., 0.001);
    BOOST_CHECK(!output.readNonBlocking());
    BOOST_CHECK(!output_copy.readNonBlocking());
  }

  // test passing unknown variables
  {
    TestApp app("testApp");
    ctk::TestFacility test;
    auto input = test.getScalar<double>("/Multiplier/input");
    auto tap = test.getScalar<double>("/Multiplier/tap");
    auto output = test.getScalar<double>("/mySubModule/output");
    BOOST_CHECK_THROW(app.optimiseUnmappedVariables({"/Multiplier/tap", "/this/is/not/known"}), std::out_of_range);
  }

}
