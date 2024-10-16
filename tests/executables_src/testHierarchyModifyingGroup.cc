// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestFacility.h"

#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testHierarchyModifyingGroup

#include "Application.h"
#include "ApplicationModule.h"
#include "ModuleGroup.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>

namespace Tests::testHierarchyModifyingGroup {

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;

  /*********************************************************************************************************************/
  /*********************************************************************************************************************/

  /**
   * This test checks use of relative paths in modules at the example of a VariableGroup.
   *
   * TODO
   * - Rename this test source file
   * - Add checks for relative paths in ModuleGroups and ApplicationModules
   * - Add checks for relative paths in accessors
   */

  /*********************************************************************************************************************/
  /*********************************************************************************************************************/

  struct TestGroup : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPushInput<int> myVar{this, "myVar", "MV/m", "Descrption"};
  };

  struct TestApplication : public ctk::Application {
    TestApplication() : Application("testSuite") {}
    ~TestApplication() override { shutdown(); }

    struct TestModule : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      TestGroup g;

      struct ExtraHierarchy : public ctk::VariableGroup {
        using ctk::VariableGroup::VariableGroup;
        TestGroup g;
      } extraHierarchy{this, "ExtraHierarchy", "Extra depth"};

      void mainLoop() override {
        if(getAccessorListRecursive().empty()) {
          return;
        }
        assert(getAccessorListRecursive().size() == 1);
        while(true) {
          readAll();
        }
      }
    } testModule{this, "mod", "The test module"};
  };

  /*********************************************************************************************************************/

  void check(TestApplication& app, TestGroup& group, const std::string& name) {
    static int myCounter = 42;

    ChimeraTK::TestFacility test{app};
    auto acc = test.getScalar<int>(name + "/myVar");
    test.runApplication();

    acc = myCounter;
    acc.write();

    test.stepApplication();

    BOOST_TEST(int(group.myVar) == myCounter);

    ++myCounter;
  }

  /*********************************************************************************************************************/
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(VariableGroupLike) {
    std::cout << "*** VariableGroupLike" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "VariableGroupLike", "Use like normal VariableGroup"};
    check(app, app.testModule.g, "/mod/VariableGroupLike");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(MoveToRoot) {
    std::cout << "*** MoveToRoot" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "/MoveToRoot", "Use like normal VariableGroup with MoveToRoot"};
    check(app, app.testModule.g, "/MoveToRoot");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(oneUp) {
    std::cout << "*** ../oneUp" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "../oneUp", "Use like normal VariableGroup with oneUp"};
    check(app, app.testModule.g, "/oneUp");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(dotdot) {
    std::cout << "*** .." << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "..", "Use like normal VariableGroup with oneUpAndHide"};
    check(app, app.testModule.g, "");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(local_hierarchy) {
    std::cout << "*** local/hierarchy" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "local/hierarchy", "Create hierarchy locally"};
    check(app, app.testModule.g, "/mod/local/hierarchy");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(AtRoot_hierarchy) {
    std::cout << "*** /AtRoot/hierarchy" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "/AtRoot/hierarchy", "Create hierarchy at root"};
    check(app, app.testModule.g, "/AtRoot/hierarchy");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(oneUp_hierarchy) {
    std::cout << "*** ../oneUp/hierarchy" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "../oneUp/hierarchy", "Create hierarchy one level up"};
    check(app, app.testModule.g, "/oneUp/hierarchy");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(local_very_deep_hierarchy) {
    std::cout << "*** local/very/deep/hierarchy" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "local/very/deep/hierarchy", "Create deep hierarchy locally"};
    check(app, app.testModule.g, "/mod/local/very/deep/hierarchy");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(root_very_deep_hierarchy) {
    std::cout << "*** /root/very/deep/hierarchy" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "/root/very/deep/hierarchy", "Create deep hierarchy at root"};
    check(app, app.testModule.g, "/root/very/deep/hierarchy");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(oneUp_very_deep_hierarchy) {
    std::cout << "*** ../oneUp/very/deep/hierarchy" << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "../oneUp/very/deep/hierarchy", "Create deep hierarchy one level up"};
    check(app, app.testModule.g, "/oneUp/very/deep/hierarchy");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(extra_slashes_everywhere) {
    std::cout << "*** //extra//slashes////everywhere///" << std::endl;
    TestApplication app;
    BOOST_CHECK_THROW((app.testModule.g = {&app.testModule, "//extra//slashes////everywhere///", "Extra slashes"}),
        ChimeraTK::logic_error);
    BOOST_CHECK_THROW((app.testModule.g = {&app.testModule, "/extra/slashes/everywhere/", "Extra slashs at the end"}),
        ChimeraTK::logic_error);
    BOOST_CHECK_THROW((app.testModule.g = {&app.testModule, "/extra/slashes//everywhere", "Extra slash in the middle"}),
        ChimeraTK::logic_error);
    BOOST_CHECK_THROW(
        (app.testModule.g = {&app.testModule, "//extra/slashes/everywhere", "Extra slash in the beginning"}),
        ChimeraTK::logic_error);
    BOOST_CHECK_NO_THROW((app.testModule.g = {&app.testModule, "/extra/slashes/everywhere", "No extra slash"}));
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(twoUp) {
    std::cout << "*** twoUp" << std::endl;
    TestApplication app;
    app.testModule.extraHierarchy.g = {&app.testModule.extraHierarchy, "../../twoUp", "Two levels up"};
    check(app, app.testModule.extraHierarchy.g, "/twoUp");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(hierarchy_with_dots_anywhere_also_single_dots) {
    std::cout << "*** hierarchy/with/../dots/../../anywhere/./also/./single/./dots/.." << std::endl;
    TestApplication app;
    app.testModule.g = {
        &app.testModule, "hierarchy/with/../dots/../../anywhere/./also/./single/./dots/..", "Dots everywhere "};
    app.getModel().writeGraphViz("vg_test.dot");
    check(app, app.testModule.g, "/mod/anywhere/also/single");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(dot) {
    std::cout << "*** ." << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, ".", "This is like hideThis"};
    check(app, app.testModule.g, "/mod");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(dot_at_end) {
    std::cout << "*** dot/at/end/." << std::endl;
    TestApplication app;
    app.testModule.g = {&app.testModule, "dot/at/end/.", "Gets effectively ignored..."};
    check(app, app.testModule.g, "/mod/dot/at/end");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(MoveToRootFromHidden) {
    std::cout << "*** MoveToRootFromHidden" << std::endl;
    TestApplication app;
    app.testModule = {&app, ".", "The test module is hidden now"};
    app.testModule.g = {&app.testModule, "/MoveToRootFromHidden",
        "Use like normal VariableGroup with MoveToRoot, and place inside a hidden to-level module"};
    check(app, app.testModule.g, "/MoveToRootFromHidden");
  }

  /*********************************************************************************************************************/

  struct TestApplicationEmpty : public ctk::Application {
    TestApplicationEmpty() : Application("testSuite") {}
    ~TestApplicationEmpty() override { shutdown(); }

    struct TestModule : ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;
      void mainLoop() override {}
    } testModule{this, "TestModule", "The test module"};
  };

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(bad_path_exception) {
    std::cout << "*** bad_path_exception" << std::endl;
    TestApplicationEmpty app;
    BOOST_CHECK_THROW(TestGroup tg(&app.testModule, "/../cannot/work", "This is not allowed"), ctk::logic_error);
    BOOST_CHECK_THROW(TestGroup tg(&app.testModule, "/..", "This is not allowed either"), ctk::logic_error);
    BOOST_CHECK_THROW(
        TestGroup tg(&app.testModule, "/somthing/less/../../../obvious", "This is also not allowed"), ctk::logic_error);
  }

  /*********************************************************************************************************************/

} // namespace Tests::testHierarchyModifyingGroup