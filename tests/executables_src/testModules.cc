// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testModules

#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ModuleGroup.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "VariableGroup.h"

#include <boost/mpl/list.hpp>

namespace Tests::testModules {

  namespace ctk = ChimeraTK;

  /********************************************************************************************************************/
  /* Variable group used in the modules */

  struct SomeGroup : ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPushInput<std::string> inGroup{this, "inGroup", "", "This is a string", {"C", "A"}};
    ctk::ArrayPushInput<int64_t> alsoInGroup{
        this, "alsoInGroup", "justANumber", 16, "A 64 bit number array", {"A", "D"}};
  };

  /********************************************************************************************************************/
  /* A plain application module for testing */

  struct TestModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPushInput<int> someInput{
        this, "nameOfSomeInput", "cm", "This is just some input for testing", {"A", "B"}};
    ctk::ScalarOutput<double> someOutput{this, "someOutput", "V", "Description", {"A", "C"}};

    SomeGroup someGroup{this, "someGroup", "Description of my test group"};

    struct AnotherGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<uint8_t> foo{this, "foo", "counts", "Some counter", {"D"}};
    } anotherGroup{this, "anotherName", "Description of my other group"};

    void mainLoop() override {
      while(true) {
        someInput.read();
        int val = someInput;
        someOutput = val;
        someOutput.write();
      }
    }
  };

  /********************************************************************************************************************/
  /* Simple application with just one module */

  struct OneModuleApp : public ctk::Application {
    OneModuleApp() : Application("myApp") {}
    ~OneModuleApp() override { shutdown(); }

    TestModule testModule{this, "testModule", "Module to test"};
  };

  /********************************************************************************************************************/
  /* Application with a vector of modules */

  struct VectorOfModulesApp : public ctk::Application {
    explicit VectorOfModulesApp(size_t numberOfInstances) : Application("myApp"), nInstances(numberOfInstances) {
      for(size_t i = 0; i < numberOfInstances; ++i) {
        std::string moduleName = "testModule_" + std::to_string(i) + "_instance";
        vectorOfTestModule.emplace_back(this, moduleName, "Description");
      }
    }
    ~VectorOfModulesApp() override { shutdown(); }

    size_t nInstances;
    std::vector<TestModule> vectorOfTestModule;
  };

  /********************************************************************************************************************/
  /* An application module with a vector of a variable group*/

  struct VectorModule : public ctk::ApplicationModule {
    VectorModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description, size_t nInstances,
        const std::unordered_set<std::string>& tags = {})
    : ctk::ApplicationModule(owner, name, description, tags) {
      for(size_t i = 0; i < nInstances; ++i) {
        std::string groupName = "testGroup_" + std::to_string(i);
        vectorOfSomeGroup.emplace_back(this, groupName, "Description 2");
        BOOST_CHECK(vectorOfSomeGroup.back().getModel().isValid());
      }
      for(size_t i = 0; i < nInstances; ++i) {
        BOOST_CHECK(vectorOfSomeGroup[i].getModel().isValid());
      }
    }
    VectorModule() = default;

    ctk::ScalarPushInput<int> someInput{
        this, "nameOfSomeInput", "cm", "This is just some input for testing", {"A", "B"}};
    ctk::ArrayOutput<double> someOutput{this, "someOutput", "V", 1, "Description", {"A", "C"}};

    std::vector<SomeGroup> vectorOfSomeGroup;

    struct AnotherGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<uint8_t> foo{this, "foo", "counts", "Some counter", {"D"}};
    } anotherGroup{this, "anotherName", "Description of my other group"};

    void mainLoop() override {
      while(true) {
        someInput.read();
        int val = someInput;
        someOutput[0] = val;
        someOutput.write();
      }
    }
  };

  /********************************************************************************************************************/
  /* An module group with a vector of a application modules */

  struct VectorModuleGroup : public ctk::ModuleGroup {
    VectorModuleGroup(ModuleGroup* owner, const std::string& name, const std::string& description, size_t nInstances,
        const std::unordered_set<std::string>& tags = {})
    : ctk::ModuleGroup(owner, name, description, tags) {
      for(size_t i = 0; i < nInstances; ++i) {
        std::string vovModuleName = "test_" + std::to_string(i);
        vectorOfVectorModule.emplace_back(this, vovModuleName, "Description 3", nInstances);
        BOOST_CHECK(vectorOfVectorModule.back().getModel().isValid());
      }
      for(size_t i = 0; i < nInstances; ++i) {
        BOOST_CHECK(vectorOfVectorModule[i].getModel().isValid());
      }
    }

    VectorModuleGroup() = default;

    std::vector<VectorModule> vectorOfVectorModule;
  };

  /********************************************************************************************************************/
  /* Application with a vector of module groups containing a vector of modules
   * containing a vector of variable groups */

  struct VectorOfEverythingApp : public ctk::Application {
    explicit VectorOfEverythingApp(size_t numberOfInstances) : Application("myApp"), nInstances(numberOfInstances) {
      for(size_t i = 0; i < nInstances; ++i) {
        std::string name = "testModule_" + std::to_string(i) + "_instance";
        vectorOfVectorModuleGroup.emplace_back(this, name, "Description", nInstances);
        BOOST_CHECK(vectorOfVectorModuleGroup.back().getModel().isValid());
      }
      for(size_t i = 0; i < nInstances; ++i) {
        BOOST_CHECK(vectorOfVectorModuleGroup[i].getModel().isValid());
      }
    }
    ~VectorOfEverythingApp() override { shutdown(); }

    size_t nInstances;
    std::vector<VectorModuleGroup> vectorOfVectorModuleGroup;
  };

  /********************************************************************************************************************/
  /* Application with various modules that get initialised late in the constructor. */

  struct AssignModuleLaterApp : public ctk::Application {
    AssignModuleLaterApp() : Application("myApp") {
      modGroupInstanceToAssignLater = std::move(modGroupInstanceSource);
      modInstanceToAssignLater = std::move(modInstanceSource);
    }
    ~AssignModuleLaterApp() override { shutdown(); }

    VectorModuleGroup modGroupInstanceSource{this, "modGroupInstanceToAssignLater",
        "This instance of VectorModuleGroup was assigned using the operator=()", 42};
    VectorModule modInstanceSource{
        this, "modInstanceToAssignLater", "This instance of VectorModule was assigned using the operator=()", 13};

    VectorModuleGroup modGroupInstanceToAssignLater;
    VectorModule modInstanceToAssignLater;
  };

  /********************************************************************************************************************/
  /* test module and variable ownerships */

  BOOST_AUTO_TEST_CASE(test_ownership) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_ownership" << std::endl;

    OneModuleApp app;

    BOOST_CHECK(app.testModule.getOwner() == &app);
    BOOST_CHECK(app.testModule.someGroup.getOwner() == &(app.testModule));
    BOOST_CHECK(app.testModule.anotherGroup.getOwner() == &(app.testModule));

    BOOST_CHECK(app.testModule.someInput.getOwner() == &(app.testModule));
    BOOST_CHECK(app.testModule.someOutput.getOwner() == &(app.testModule));

    BOOST_CHECK(app.testModule.someGroup.inGroup.getOwner() == &(app.testModule.someGroup));
    BOOST_CHECK(app.testModule.someGroup.alsoInGroup.getOwner() == &(app.testModule.someGroup));

    BOOST_CHECK(app.testModule.anotherGroup.foo.getOwner() == &(app.testModule.anotherGroup));
  }

  /********************************************************************************************************************/
  /* test that modules cannot be owned by the wrong types */

  BOOST_AUTO_TEST_CASE(test_badHierarchies) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_badHierarchies" << std::endl;

    // ******************************************
    // *** Tests for ApplicationModule

    // check app ApplicationModules cannot be owned by nothing
    {
      OneModuleApp app;
      try {
        TestModule willFail(static_cast<ChimeraTK::ModuleGroup*>(nullptr), "willFail", "");
        BOOST_FAIL("Exception expected");
      }
      catch(ChimeraTK::logic_error&) {
      }
    }

    // ******************************************
    // *** Tests for VariableGroup

    // check app VariableGroup cannot be owned by nothing
    {
      OneModuleApp app;
      try {
        SomeGroup willFail(static_cast<ChimeraTK::VariableGroup*>(nullptr), "willFail", "");
        BOOST_FAIL("Exception expected");
      }
      catch(ChimeraTK::logic_error&) {
      }
    }

    // ******************************************
    // *** Tests for ModuleGroup

    // check app ModuleGroups cannot be owned by nothing
    {
      OneModuleApp app;
      try {
        VectorModuleGroup willFail(nullptr, "willFail", "", 1);
        BOOST_FAIL("Exception expected");
      }
      catch(ChimeraTK::logic_error&) {
      }
    }
  }

  /********************************************************************************************************************/
  /* test that modules can be owned by the right types */

  BOOST_AUTO_TEST_CASE(test_allowedHierarchies) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_allowedHierarchies" << std::endl;

    // ******************************************
    // *** Tests for ApplicationModule
    // check ApplicationModules can be owned by Applications
    try {
      OneModuleApp app;
      TestModule shouldNotFail(&(app), "shouldNotFail", "");
    }
    catch(ChimeraTK::logic_error&) {
      BOOST_CHECK(false);
    }

    // check ApplicationModules can be owned by ModuleGroups
    try {
      VectorOfEverythingApp app(1);
      auto* v = &(app.vectorOfVectorModuleGroup[0]);
      BOOST_CHECK(v != nullptr);
      TestModule shouldNotFail(v, "shouldNotFail", "");
    }
    catch(ChimeraTK::logic_error&) {
      BOOST_FAIL("Exception not expected!");
    }

    // ******************************************
    // *** Tests for VariableGroup

    // check VariableGroup can be owned by ApplicationModules
    try {
      OneModuleApp app;
      SomeGroup shouldNotFail(&(app.testModule), "shouldNotFail", "");
    }
    catch(ChimeraTK::logic_error&) {
      BOOST_CHECK(false);
    }

    // check VariableGroup can be owned by VariableGroup
    try {
      OneModuleApp app;
      SomeGroup shouldNotFail(&(app.testModule.someGroup), "shouldNotFail", "");
    }
    catch(ChimeraTK::logic_error&) {
      BOOST_CHECK(false);
    }

    // ******************************************
    // *** Tests for ModuleGroup

    // check ModuleGroup can be owned by Applications
    try {
      OneModuleApp app;
      VectorModuleGroup shouldNotFail(&(app), "shouldNotFail", "", 1);
    }
    catch(ChimeraTK::logic_error&) {
      BOOST_CHECK(false);
    }

    // check ModuleGroup can be owned by ModuleGroups
    try {
      VectorOfEverythingApp app(1);
      VectorModuleGroup shouldNotFail(&(app.vectorOfVectorModuleGroup[0]), "shouldNotFail", "", 1);
    }
    catch(ChimeraTK::logic_error&) {
      BOOST_CHECK(false);
    }
  }

  /********************************************************************************************************************/
  /* test getSubmoduleList() and getSubmoduleListRecursive() */

  BOOST_AUTO_TEST_CASE(test_getSubmoduleList) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_getSubmoduleList" << std::endl;

    OneModuleApp app;

    {
      std::list<ctk::Module*> list = app.getSubmoduleList();
      BOOST_CHECK(list.size() == 1);
      BOOST_CHECK(list.front() == &(app.testModule));
    }

    {
      std::list<ctk::Module*> list = app.testModule.getSubmoduleList();
      BOOST_CHECK(list.size() == 2);
      size_t foundSomeGroup = 0;
      size_t foundAnotherGroup = 0;
      for(const auto* mod : list) {
        if(mod == &(app.testModule.someGroup)) {
          foundSomeGroup++;
        }
        if(mod == &(app.testModule.anotherGroup)) {
          foundAnotherGroup++;
        }
      }
      BOOST_CHECK(foundSomeGroup == 1);
      BOOST_CHECK(foundAnotherGroup == 1);
    }

    {
      std::list<ctk::Module*> list = app.getSubmoduleListRecursive();
      BOOST_CHECK(list.size() == 3);
      size_t foundTestModule = 0;
      size_t foundSomeGroup = 0;
      size_t foundAnotherGroup = 0;
      for(const auto* mod : list) {
        if(mod == &(app.testModule)) {
          foundTestModule++;
        }
        if(mod == &(app.testModule.someGroup)) {
          foundSomeGroup++;
        }
        if(mod == &(app.testModule.anotherGroup)) {
          foundAnotherGroup++;
        }
      }
      BOOST_CHECK(foundTestModule == 1);
      BOOST_CHECK(foundSomeGroup == 1);
      BOOST_CHECK(foundAnotherGroup == 1);
    }

    {
      std::list<ctk::Module*> list = app.testModule.getSubmoduleListRecursive(); // identical to getSubmoduleList(),
                                                                                 // since no deeper hierarchies
      BOOST_CHECK(list.size() == 2);
      size_t foundSomeGroup = 0;
      size_t foundAnotherGroup = 0;
      for(const auto* mod : list) {
        if(mod == &(app.testModule.someGroup)) {
          foundSomeGroup++;
        }
        if(mod == &(app.testModule.anotherGroup)) {
          foundAnotherGroup++;
        }
      }
      BOOST_CHECK(foundSomeGroup == 1);
      BOOST_CHECK(foundAnotherGroup == 1);
    }
  }

  /********************************************************************************************************************/
  /* test getAccessorList() and getAccessorListRecursive() */

  BOOST_AUTO_TEST_CASE(test_getAccessorList) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_getAccessorList" << std::endl;

    OneModuleApp app;

    {
      std::list<ctk::VariableNetworkNode> list = app.testModule.getAccessorList();
      BOOST_CHECK(list.size() == 2);
      size_t foundSomeInput = 0;
      size_t foundSomeOutput = 0;
      for(const auto& var : list) {
        if(var == ctk::VariableNetworkNode(app.testModule.someInput)) {
          foundSomeInput++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someOutput)) {
          foundSomeOutput++;
        }
      }
      BOOST_CHECK(foundSomeInput == 1);
      BOOST_CHECK(foundSomeOutput == 1);
    }

    {
      const SomeGroup& someGroup(app.testModule.someGroup);
      const std::list<ctk::VariableNetworkNode> list = someGroup.getAccessorList();
      BOOST_CHECK(list.size() == 2);
      size_t foundInGroup = 0;
      size_t foundAlsoInGroup = 0;
      for(const auto& var : list) {
        if(var == ctk::VariableNetworkNode(app.testModule.someGroup.inGroup)) {
          foundInGroup++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someGroup.alsoInGroup)) {
          foundAlsoInGroup++;
        }
      }
      BOOST_CHECK(foundInGroup == 1);
      BOOST_CHECK(foundAlsoInGroup == 1);
    }

    {
      std::list<ctk::VariableNetworkNode> list = app.getAccessorListRecursive();
      BOOST_CHECK(list.size() == 5);
      size_t foundSomeInput = 0;
      size_t foundSomeOutput = 0;
      size_t foundInGroup = 0;
      size_t foundAlsoInGroup = 0;
      size_t foundFoo = 0;
      for(const auto& var : list) {
        if(var == ctk::VariableNetworkNode(app.testModule.someInput)) {
          foundSomeInput++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someOutput)) {
          foundSomeOutput++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someGroup.inGroup)) {
          foundInGroup++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someGroup.alsoInGroup)) {
          foundAlsoInGroup++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.anotherGroup.foo)) {
          foundFoo++;
        }
      }
      BOOST_CHECK(foundSomeInput == 1);
      BOOST_CHECK(foundSomeOutput == 1);
      BOOST_CHECK(foundInGroup == 1);
      BOOST_CHECK(foundAlsoInGroup == 1);
      BOOST_CHECK(foundFoo == 1);
    }

    {
      std::list<ctk::VariableNetworkNode> list = app.testModule.getAccessorListRecursive();
      BOOST_CHECK(list.size() == 5);
      size_t foundSomeInput = 0;
      size_t foundSomeOutput = 0;
      size_t foundInGroup = 0;
      size_t foundAlsoInGroup = 0;
      size_t foundFoo = 0;
      for(const auto& var : list) {
        if(var == ctk::VariableNetworkNode(app.testModule.someInput)) {
          foundSomeInput++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someOutput)) {
          foundSomeOutput++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someGroup.inGroup)) {
          foundInGroup++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.someGroup.alsoInGroup)) {
          foundAlsoInGroup++;
        }
        if(var == ctk::VariableNetworkNode(app.testModule.anotherGroup.foo)) {
          foundFoo++;
        }
      }
      BOOST_CHECK(foundSomeInput == 1);
      BOOST_CHECK(foundSomeOutput == 1);
      BOOST_CHECK(foundInGroup == 1);
      BOOST_CHECK(foundAlsoInGroup == 1);
      BOOST_CHECK(foundFoo == 1);
    }

    {
      std::list<ctk::VariableNetworkNode> list = app.testModule.anotherGroup.getAccessorListRecursive();
      BOOST_CHECK(list.size() == 1);
      size_t foundFoo = 0;
      for(const auto& var : list) {
        if(var == ctk::VariableNetworkNode(app.testModule.anotherGroup.foo)) {
          foundFoo++;
        }
      }
      BOOST_CHECK(foundFoo == 1);
    }
  }

  /********************************************************************************************************************/
  /* test addTag() */

  BOOST_AUTO_TEST_CASE(testAddTag) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> testAddTag" << std::endl;

    OneModuleApp app;
    app.testModule.addTag("newTag");

    size_t nFound = 0;
    auto checker = [&](auto proxy) {
      ++nFound;
      auto name = proxy.getFullyQualifiedPath();
      BOOST_CHECK(name == "/testModule/nameOfSomeInput" || name == "/testModule/someOutput" ||
          name == "/testModule/anotherName/foo" || name == "/testModule/someGroup/inGroup" ||
          name == "/testModule/someGroup/alsoInGroup");
    };

    app.testModule.getModel().visit(
        checker, ctk::Model::keepProcessVariables && ctk::Model::keepTag("newTag"), ChimeraTK::Model::depthFirstSearch);

    BOOST_TEST(nFound == 5);
  }

  /********************************************************************************************************************/
  /* test addTag() with negated tags, in order to remove tags */

  BOOST_AUTO_TEST_CASE(testAddTagNegated) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> testAddTagNegated" << std::endl;

    BOOST_TEST(ChimeraTK::negateTag("newTag") == "!newTag");
    BOOST_TEST(ChimeraTK::negateTag("!newTag") == "newTag");

    {
      // negated tags on module level
      OneModuleApp app;
      BOOST_TEST(ChimeraTK::negateTag("newTag") == "!newTag");
      BOOST_TEST(ChimeraTK::negateTag("!newTag") == "newTag");
      app.testModule.addTag("!newTag");
      app.testModule.addTag("newTag");

      const auto& tags = app.testModule.someOutput.getTags();
      BOOST_CHECK(tags.find("newTag") == tags.end());
    }
    {
      // negated tags on variable level
      OneModuleApp app;
      app.testModule.someOutput.addTag("newTag");
      app.testModule.someOutput.addTag("!newTag");

      const auto& tags = app.testModule.someOutput.getTags();
      BOOST_CHECK(tags.find("newTag") == tags.end());
    }
    {
      // negated tags on variable and module level, mixed
      OneModuleApp app;
      app.testModule.addTag("newTag");
      app.testModule.someOutput.addTag("!newTag");

      const auto& tags = app.testModule.someOutput.getTags();
      BOOST_CHECK(tags.find("newTag") == tags.end());
    }
    // note, we currently do not test the tag set of the model associated with the accessors.
    // the tags on the model level are not clear since a vertex in the model represents an output pv and its input
    // in some other module at the same time
  }

  /********************************************************************************************************************/
  /* test correct behaviour when using a std::vector of ApplicationModules */

  BOOST_AUTO_TEST_CASE(testVectorOfApplicationModule) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> testVectorOfApplicationModule" << std::endl;

    // create app with a vector containing 10 modules
    size_t nInstances = 10;
    VectorOfModulesApp app(nInstances);

    // the app creates the 10 module instances, check if this is done proplery (a quite redundant test...)
    BOOST_TEST(app.vectorOfTestModule.size() == nInstances);

    // some direct checks on the created instances
    for(size_t i = 0; i < nInstances; ++i) {
      std::string name = "testModule_" + std::to_string(i) + "_instance";
      BOOST_TEST(app.vectorOfTestModule[i].getName() == name);
      auto node = static_cast<ctk::VariableNetworkNode>(app.vectorOfTestModule[i].someInput);
      BOOST_TEST(node.getQualifiedName() == "/myApp/" + name + "/nameOfSomeInput");

      // check accessor list
      std::list<ctk::VariableNetworkNode> accList = app.vectorOfTestModule[i].getAccessorList();
      BOOST_TEST(accList.size() == 2);
      size_t foundSomeInput = 0;
      size_t foundSomeOutput = 0;
      for(auto& acc : accList) {
        if(acc == ctk::VariableNetworkNode(app.vectorOfTestModule[i].someInput)) {
          foundSomeInput++;
        }
        if(acc == ctk::VariableNetworkNode(app.vectorOfTestModule[i].someOutput)) {
          foundSomeOutput++;
        }
      }
      BOOST_TEST(foundSomeInput == 1);
      BOOST_TEST(foundSomeOutput == 1);

      // check submodule list
      std::list<ctk::Module*> modList = app.vectorOfTestModule[i].getSubmoduleList();
      BOOST_TEST(modList.size() == 2);
      size_t foundSomeGroup = 0;
      size_t foundAnotherGroup = 0;
      for(const auto* mod : modList) {
        if(mod == &(app.vectorOfTestModule[i].someGroup)) {
          foundSomeGroup++;
        }
        if(mod == &(app.vectorOfTestModule[i].anotherGroup)) {
          foundAnotherGroup++;
        }
      }
      BOOST_TEST(foundSomeGroup == 1);
      BOOST_TEST(foundAnotherGroup == 1);
    }

    // check if instances appear properly in getSubmoduleList()
    {
      std::list<ctk::Module*> list = app.getSubmoduleList();
      BOOST_TEST(list.size() == nInstances);
      std::map<size_t, size_t> instancesFound;
      for(size_t i = 0; i < nInstances; ++i) {
        instancesFound[i] = 0;
      }
      for(const auto* mod : list) {
        for(size_t i = 0; i < nInstances; ++i) {
          if(mod == &(app.vectorOfTestModule[i])) {
            instancesFound[i]++;
          }
        }
      }
      for(size_t i = 0; i < nInstances; ++i) {
        BOOST_TEST(instancesFound[i] == 1);
      }
    }

    // check if instances appear properly in getSubmoduleListRecursive() as well
    {
      std::list<ctk::Module*> list = app.getSubmoduleListRecursive();
      BOOST_TEST(list.size() == 3 * nInstances);
      std::map<size_t, size_t> instancesFound, instancesSomeGroupFound, instancesAnotherGroupFound;
      for(size_t i = 0; i < nInstances; ++i) {
        instancesFound[i] = 0;
        instancesSomeGroupFound[i] = 0;
        instancesAnotherGroupFound[i] = 0;
      }
      for(const auto* mod : list) {
        for(size_t i = 0; i < nInstances; ++i) {
          if(mod == &(app.vectorOfTestModule[i])) {
            instancesFound[i]++;
          }
          if(mod == &(app.vectorOfTestModule[i].someGroup)) {
            instancesSomeGroupFound[i]++;
          }
          if(mod == &(app.vectorOfTestModule[i].anotherGroup)) {
            instancesAnotherGroupFound[i]++;
          }
        }
      }
      for(size_t i = 0; i < nInstances; ++i) {
        BOOST_TEST(instancesFound[i] == 1);
        BOOST_TEST(instancesSomeGroupFound[i] == 1);
        BOOST_TEST(instancesAnotherGroupFound[i] == 1);
      }
    }

    // check ownerships
    for(size_t i = 0; i < nInstances; ++i) {
      BOOST_CHECK(app.vectorOfTestModule[i].getOwner() == &app);
      BOOST_CHECK(app.vectorOfTestModule[i].someInput.getOwner() == &(app.vectorOfTestModule[i]));
      BOOST_CHECK(app.vectorOfTestModule[i].someOutput.getOwner() == &(app.vectorOfTestModule[i]));
      BOOST_CHECK(app.vectorOfTestModule[i].someGroup.getOwner() == &(app.vectorOfTestModule[i]));
      BOOST_CHECK(app.vectorOfTestModule[i].someGroup.inGroup.getOwner() == &(app.vectorOfTestModule[i].someGroup));
      BOOST_CHECK(app.vectorOfTestModule[i].someGroup.alsoInGroup.getOwner() == &(app.vectorOfTestModule[i].someGroup));
      BOOST_CHECK(app.vectorOfTestModule[i].anotherGroup.getOwner() == &(app.vectorOfTestModule[i]));
      BOOST_CHECK(app.vectorOfTestModule[i].anotherGroup.foo.getOwner() == &(app.vectorOfTestModule[i].anotherGroup));
    }
  }

  /********************************************************************************************************************/
  /* test correct behaviour when using a std::vector of ModuleGroup, ApplicationModule and VariableGroup at the same
   * time */

  BOOST_AUTO_TEST_CASE(testVectorsOfAllModules) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> testVectorsOfAllModules" << std::endl;

    // create app with a vector containing 10 modules
    size_t nInstances = 10;
    VectorOfEverythingApp app(nInstances);

    /*----------------------------------------------------------------------------------------------------------------*/
    // the app creates the 10 module instances, check if this is done proplery (a quite redundant test...)
    BOOST_CHECK(app.vectorOfVectorModuleGroup.size() == nInstances);
    for(size_t i = 0; i < nInstances; ++i) {
      BOOST_CHECK(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule.size() == nInstances);
      for(size_t k = 0; k < nInstances; ++k) {
        BOOST_CHECK(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup.size() == nInstances);
      }
    }

    /*----------------------------------------------------------------------------------------------------------------*/
    // check presence in lists (getSubmoduleList() and getAccessorList())

    { // checks on first hierarchy level (application has the list of module groups)
      std::list<ctk::Module*> list = app.getSubmoduleList();
      BOOST_CHECK(list.size() == nInstances);
      std::map<size_t, size_t> found;
      for(size_t i = 0; i < nInstances; ++i) {
        found[i] = 0;
      }
      for(const auto* mod : list) {
        for(size_t i = 0; i < nInstances; ++i) {
          if(mod == &(app.vectorOfVectorModuleGroup[i])) {
            found[i]++;
          }
        }
      }
      for(size_t i = 0; i < nInstances; ++i) {
        BOOST_CHECK(found[i] == 1);
      }
    }

    { // checks on second hierarchy level (each module group has the list of modules)
      for(size_t i = 0; i < nInstances; ++i) {
        std::list<ctk::Module*> list = app.vectorOfVectorModuleGroup[i].getSubmoduleList();
        BOOST_CHECK(list.size() == nInstances);

        std::map<size_t, size_t> found;
        for(size_t k = 0; k < nInstances; ++k) {
          found[k] = 0;
        }
        for(const auto* mod : list) {
          for(size_t k = 0; k < nInstances; ++k) {
            if(mod == &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k])) {
              found[k]++;
            }
          }
        }
        for(size_t k = 0; k < nInstances; ++k) {
          BOOST_CHECK(found[k] == 1);
        }
      }
    }

    { // checks on third hierarchy level (each module has accessors and variable groups)
      for(size_t i = 0; i < nInstances; ++i) {
        for(size_t k = 0; k < nInstances; ++k) {
          // search for accessors
          std::list<ctk::VariableNetworkNode> accList =
              app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].getAccessorList();
          BOOST_CHECK_EQUAL(accList.size(), 2);
          size_t someInputFound = 0;
          size_t someOutputFound = 0;
          for(const auto& acc : accList) {
            if(acc == ctk::VariableNetworkNode(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someInput)) {
              someInputFound++;
            }
            if(acc == ctk::VariableNetworkNode(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someOutput)) {
              someOutputFound++;
            }
          }
          BOOST_CHECK_EQUAL(someInputFound, 1);
          BOOST_CHECK_EQUAL(someOutputFound, 1);

          // search for variable groups
          std::list<ctk::Module*> modList = app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].getSubmoduleList();
          BOOST_CHECK_EQUAL(modList.size(), nInstances + 1);

          std::map<size_t, size_t> someGroupFound;
          for(size_t m = 0; m < nInstances; ++m) {
            someGroupFound[m] = 0;
          }
          size_t anotherGroupFound = 0;
          for(const auto* mod : modList) {
            for(size_t m = 0; m < nInstances; ++m) {
              if(mod == &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m])) {
                someGroupFound[m]++;
              }
            }
            if(mod == &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].anotherGroup)) {
              anotherGroupFound++;
            }
          }
          for(size_t m = 0; m < nInstances; ++m) {
            BOOST_CHECK_EQUAL(someGroupFound[m], 1);
          }
          BOOST_CHECK_EQUAL(anotherGroupFound, 1);
        }
      }
    }

    { // checks on fourth hierarchy level (each variable group has accessors)
      for(size_t i = 0; i < nInstances; ++i) {
        for(size_t k = 0; k < nInstances; ++k) {
          for(size_t m = 0; m < nInstances; ++m) {
            // search for accessors
            std::list<ctk::VariableNetworkNode> accList =
                app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].getAccessorList();
            BOOST_CHECK_EQUAL(accList.size(), 2);
            size_t inGroupFound = 0;
            size_t alsoInGroupFound = 0;
            for(const auto& acc : accList) {
              if(acc ==
                  ctk::VariableNetworkNode(
                      app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].inGroup)) {
                inGroupFound++;
              }
              if(acc ==
                  ctk::VariableNetworkNode(
                      app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].alsoInGroup)) {
                alsoInGroupFound++;
              }
            }
            BOOST_CHECK_EQUAL(inGroupFound, 1);
            BOOST_CHECK_EQUAL(alsoInGroupFound, 1);

            // make sure no further subgroups exist
            BOOST_CHECK_EQUAL(
                app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].getSubmoduleList().size(),
                0);
          }
        }
      }
    }

    /*----------------------------------------------------------------------------------------------------------------*/
    // check ownerships
    for(size_t i = 0; i < nInstances; ++i) {
      BOOST_CHECK(app.vectorOfVectorModuleGroup[i].getOwner() == &app);
      for(size_t k = 0; k < nInstances; ++k) {
        BOOST_CHECK(
            app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].getOwner() == &(app.vectorOfVectorModuleGroup[i]));
        BOOST_CHECK(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someInput.getOwner() ==
            &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k]));
        BOOST_CHECK(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someOutput.getOwner() ==
            &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k]));
        for(size_t m = 0; m < nInstances; ++m) {
          BOOST_CHECK(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].getOwner() ==
              &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k]));
          BOOST_CHECK(
              app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].inGroup.getOwner() ==
              &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m]));
          BOOST_CHECK(
              app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].alsoInGroup.getOwner() ==
              &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m]));
        }
      }
    }

    /*----------------------------------------------------------------------------------------------------------------*/
    // check pointers to accessors in VariableNetworkNode
    for(size_t i = 0; i < nInstances; ++i) {
      for(size_t k = 0; k < nInstances; ++k) {
        {
          const auto* a = &(
              static_cast<ctk::VariableNetworkNode>(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someInput)
                  .getAppAccessorNoType());
          const auto* b = &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someInput);
          BOOST_CHECK(a == b);
        }
        {
          const auto* a = &(
              static_cast<ctk::VariableNetworkNode>(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someOutput)
                  .getAppAccessorNoType());
          const auto* b = &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].someOutput);
          BOOST_CHECK(a == b);
        }
        for(size_t m = 0; m < nInstances; ++m) {
          {
            const auto* a = &(static_cast<ctk::VariableNetworkNode>(
                app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].inGroup)
                    .getAppAccessorNoType());
            const auto* b = &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].inGroup);
            BOOST_CHECK(a == b);
          }
          {
            const auto* a = &(static_cast<ctk::VariableNetworkNode>(
                app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].alsoInGroup)
                    .getAppAccessorNoType());
            const auto* b =
                &(app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].vectorOfSomeGroup[m].alsoInGroup);
            BOOST_CHECK(a == b);
          }
        }
      }
    }

    /*----------------------------------------------------------------------------------------------------------------*/
    // check model
    { // check presence of all PVs (and indirectly the directories)
      size_t nFound = 0;
      std::set<std::string> pvNames;

      auto checker = [&](auto proxy) {
        pvNames.emplace(proxy.getFullyQualifiedPath());
        ++nFound;
      };

      app.getModel().visit(
          checker, ctk::Model::depthFirstSearch, ctk::Model::keepProcessVariables, ctk::Model::keepParenthood);

      size_t nExpected = 0;
      for(size_t i = 0; i < nInstances; ++i) {
        std::string mgName = "/testModule_" + std::to_string(i) + "_instance";
        for(size_t k = 0; k < nInstances; ++k) {
          std::string amName = mgName + "/test_" + std::to_string(k);
          for(size_t l = 0; l < nInstances; ++l) {
            std::string vgName = amName + "/testGroup_" + std::to_string(l);
            BOOST_CHECK(pvNames.count(vgName + "/inGroup"));
            BOOST_CHECK(pvNames.count(vgName + "/alsoInGroup"));
            nExpected += 2;
          }

          BOOST_CHECK(pvNames.count(amName + "/nameOfSomeInput"));
          BOOST_CHECK(pvNames.count(amName + "/someOutput"));
          BOOST_CHECK(pvNames.count(amName + "/anotherName/foo"));
          nExpected += 3;
        }
      }

      assert(nExpected == 2 * pow(nInstances, 3) + 3 * pow(nInstances, 2)); // = 2300 with nInstances = 10
      BOOST_TEST(nFound == nExpected);
    }

    { // check presence of all module groups
      size_t nFound = 0;
      std::set<std::string> mgNames;

      auto checker = [&](auto proxy) {
        mgNames.emplace(proxy.getName());
        ++nFound;
      };

      app.getModel().visit(
          checker, ctk::Model::adjacentOutSearch, ctk::Model::keepOwnership, ctk::Model::keepModuleGroups);

      for(size_t i = 0; i < nInstances; ++i) {
        std::string mgName = "testModule_" + std::to_string(i) + "_instance";
        BOOST_CHECK(mgNames.count(mgName));
      }

      BOOST_TEST(nFound == nInstances);
    }

    { // check presence of all application modules
      for(size_t i = 0; i < nInstances; ++i) {
        size_t nFound = 0;
        std::set<std::string> amNames;

        auto checker = [&](auto proxy) {
          amNames.emplace(proxy.getName());
          ++nFound;
        };

        app.vectorOfVectorModuleGroup[i].getModel().visit(
            checker, ctk::Model::adjacentOutSearch, ctk::Model::keepOwnership, ctk::Model::keepApplicationModules);

        for(size_t k = 0; k < nInstances; ++k) {
          std::string amName = "test_" + std::to_string(k);
          BOOST_CHECK(amNames.count(amName));
        }

        BOOST_TEST(nFound == nInstances);
      }
    }

    { // check presence of all variable groups
      for(size_t i = 0; i < nInstances; ++i) {
        for(size_t k = 0; k < nInstances; ++k) {
          size_t nFound = 0;
          std::set<std::string> vgNames;

          auto checker = [&](auto proxy) {
            vgNames.emplace(proxy.getName());
            ++nFound;
          };

          app.vectorOfVectorModuleGroup[i].vectorOfVectorModule[k].getModel().visit(
              checker, ctk::Model::adjacentOutSearch, ctk::Model::keepOwnership, ctk::Model::keepVariableGroups);

          BOOST_CHECK(vgNames.count("anotherName"));
          for(size_t l = 0; l < nInstances; ++l) {
            std::string vgName = "testGroup_" + std::to_string(l);
            BOOST_CHECK(vgNames.count(vgName));
          }

          BOOST_TEST(nFound == nInstances + 1);
        }
      }
    }
  }

  /********************************************************************************************************************/
  /* test late initialisation of modules using the assignment operator */

  BOOST_AUTO_TEST_CASE(test_moveAssignmentOperator) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_moveAssignmentOperator" << std::endl;
    std::cout << std::endl;
    {
      AssignModuleLaterApp app;

      BOOST_CHECK(app.modGroupInstanceToAssignLater.getName() == "modGroupInstanceToAssignLater");
      BOOST_CHECK(app.modGroupInstanceToAssignLater.getDescription() ==
          "This instance of VectorModuleGroup was assigned using the operator=()");

      BOOST_CHECK(app.modInstanceToAssignLater.getName() == "modInstanceToAssignLater");
      BOOST_CHECK(app.modInstanceToAssignLater.getDescription() ==
          "This instance of VectorModule was assigned using the operator=()");

      auto list = app.getSubmoduleList();
      BOOST_CHECK(list.size() == 2);

      bool modGroupInstanceToAssignLater_found = false;
      bool modInstanceToAssignLater_found = false;
      for(const auto* mod : list) {
        if(mod == &(app.modGroupInstanceToAssignLater)) {
          modGroupInstanceToAssignLater_found = true;
        }
        if(mod == &(app.modInstanceToAssignLater)) {
          modInstanceToAssignLater_found = true;
        }
      }

      BOOST_CHECK(modGroupInstanceToAssignLater_found);
      BOOST_CHECK(modInstanceToAssignLater_found);
      BOOST_CHECK_EQUAL(app.modGroupInstanceToAssignLater.getSubmoduleList().size(), 42);
      BOOST_CHECK_EQUAL(app.modInstanceToAssignLater.getSubmoduleList().size(), 14);
      BOOST_CHECK(app.modGroupInstanceSource.getName() == "**INVALID**");
      BOOST_CHECK(app.modGroupInstanceSource.getSubmoduleList().size() == 0);
      BOOST_CHECK(app.modGroupInstanceSource.vectorOfVectorModule.size() == 0);

      BOOST_CHECK(app.modInstanceSource.getName() == "**INVALID**");
      BOOST_CHECK(app.modInstanceSource.getSubmoduleList().size() == 0);
    }
    {
      struct MovedTwiceAssignModuleLaterApp : public ctk::Application {
        MovedTwiceAssignModuleLaterApp() : Application("myApp") {
          modGroupInstanceToAssignLater = std::move(modGroupInstanceSource);
          modInstanceToAssignLater = std::move(modInstanceSource);
          modGroupInstanceToAssignedAfterMove = std::move(modGroupInstanceSource);
          modInstanceToAssignedAfterMove = std::move(modInstanceSource);
        }
        ~MovedTwiceAssignModuleLaterApp() override { shutdown(); }

        VectorModuleGroup modGroupInstanceSource{this, "modGroupInstanceToAssignLater",
            "This instance of VectorModuleGroup was assigned using the operator=()", 42};
        VectorModule modInstanceSource{
            this, "modInstanceToAssignLater", "This instance of VectorModule was assigned using the operator=()", 13};

        VectorModuleGroup modGroupInstanceToAssignLater;
        VectorModule modInstanceToAssignLater;
        VectorModuleGroup modGroupInstanceToAssignedAfterMove;
        VectorModule modInstanceToAssignedAfterMove;
      };

      {
        auto appAgain = std::make_unique<MovedTwiceAssignModuleLaterApp>();
        VectorModuleGroup externalModGroup{appAgain.get(), "externalModGroup",
            "This instance of VectorModuleGroup was created to be destroyed after the correspondig app to check for "
            "errors and leaks",
            42};
        BOOST_CHECK(appAgain->modInstanceToAssignedAfterMove.getName() == "**INVALID**");
        BOOST_CHECK(appAgain->modGroupInstanceToAssignedAfterMove.vectorOfVectorModule.size() == 0);
        appAgain->modGroupInstanceToAssignLater = std::move(externalModGroup);
        // destroy app before externalModGroup
        appAgain.reset();
        BOOST_CHECK(externalModGroup.getName() == "**INVALID**");
      }
    }
  }

  /********************************************************************************************************************/
  /* test tailing slashes in module names and group names will throw error*/

  struct SlashModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct AnotherGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<uint8_t> foo{this, "foo", "", "", {}};

    } anotherGroup{this, "anotherGroupName", ""};

    void mainLoop() override {}
  };

  // module group

  struct SlashApp : public ctk::Application {
    SlashApp() : Application("myApp") {}
    ~SlashApp() override { shutdown(); }

    SlashModule slashModule{this, "slashModule/", ""};
  };

  BOOST_AUTO_TEST_CASE(test_trailingSlashes) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_trailingSlashes" << std::endl;
    std::cout << std::endl;

    BOOST_CHECK_THROW(SlashApp app, ctk::logic_error);
  }

  /********************************************************************************************************************/
  /*  test tailing slashes in scalar variable name */

  struct VariableSlashScalarApp : public ctk::Application {
    VariableSlashScalarApp() : Application("myApp") {}
    ~VariableSlashScalarApp() override { shutdown(); }

    struct SomeModule : public ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ScalarPushInput<std::string> scalar{this, "scalar/", "", "", {}};

      void mainLoop() override {}
    } someModule{this, "someModule", ""};
  };

  BOOST_AUTO_TEST_CASE(test_trailingSlashesInScalarVariableNames) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_trailingSlashesInScalarVariableNames" << std::endl;
    std::cout << std::endl;
    BOOST_CHECK_THROW(VariableSlashScalarApp app, ctk::logic_error);
  }

  /********************************************************************************************************************/
  /* test tailing slashes in variable names */

  struct VariableSlashArrayApp : public ctk::Application {
    VariableSlashArrayApp() : Application("myApp") {}
    ~VariableSlashArrayApp() override { shutdown(); }

    struct SomeModule : public ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ArrayPushInput<int64_t> array{this, "array/", "", 16, "", {}};

      void mainLoop() override {}
    } someModule{this, "someModule", ""};
  };

  BOOST_AUTO_TEST_CASE(test_trailingSlashesInArrayVariableNames) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_trailingSlashesInArrayVariableNames" << std::endl;
    std::cout << std::endl;
    BOOST_CHECK_THROW(VariableSlashArrayApp app, ctk::logic_error);
  }

  /********************************************************************************************************************/
  /* test slash as variable name */

  struct OnlySlashNameArrayApp : public ctk::Application {
    OnlySlashNameArrayApp() : Application("myApp") {}
    ~OnlySlashNameArrayApp() override { shutdown(); }

    struct SomeModule : public ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ArrayPushInput<int64_t> array{this, "/", "", 16, "", {}};

      void mainLoop() override {}
    } someModule{this, "someModule", ""};
  };

  BOOST_AUTO_TEST_CASE(test_onlySlashAsVariableName) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_trailingSlashesInArrayVariableNames" << std::endl;
    std::cout << std::endl;
    BOOST_CHECK_THROW(OnlySlashNameArrayApp app, ctk::logic_error);
  }

  /********************************************************************************************************************/
  /* test tailing slash as module name */

  struct OnlySlashModuleName : public ctk::Application {
    OnlySlashModuleName() : Application("myApp") {}
    ~OnlySlashModuleName() override { shutdown(); }

    struct SomeModule : public ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ArrayPushInput<int64_t> array{this, "someArray", "", 16, "", {}};

      void mainLoop() override {}
    } someModule{this, "/", ""};
  };

  BOOST_AUTO_TEST_CASE(test_onlySlashasModuleName) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_trailingSlashesInArrayVariableNames" << std::endl;
    std::cout << std::endl;

    OnlySlashModuleName app;
    ctk::TestFacility tf{app};
    tf.runApplication();
    BOOST_CHECK(app.someModule.getName() == "/");
    BOOST_CHECK(app.someModule.array.getName() == "/someArray");
  }

  /********************************************************************************************************************/
  /* test multiple slashes in module name */

  struct MultiSlashModule : public ctk::Application {
    MultiSlashModule() : Application("myApp") {}
    ~MultiSlashModule() override { shutdown(); }

    struct SomeModule : public ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ArrayPushInput<int64_t> array{this, "someArray", "", 16, "", {}};

      void mainLoop() override {}
    } someModule{this, "aModule//withSlahsesInTheName/", ""};
  };

  BOOST_AUTO_TEST_CASE(test_multipleSlashesInModuleName) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_trailingSlashesInArrayVariableNames" << std::endl;
    std::cout << std::endl;

    BOOST_CHECK_THROW(MultiSlashModule app, ctk::logic_error);
    ;
  }

  /********************************************************************************************************************/
  /* test multiple slashes in variable name */

  struct MultiSlashVarModule : public ctk::Application {
    MultiSlashVarModule() : Application("myApp") {}
    ~MultiSlashVarModule() override { shutdown(); }

    struct SomeModule : public ctk::ApplicationModule {
      using ctk::ApplicationModule::ApplicationModule;

      ctk::ArrayPushInput<int64_t> array{this, "someArray/with//multiple///slashes", "", 16, "", {}};

      void mainLoop() override {}
    } someModule{this, "someModule", ""};
  };

  BOOST_AUTO_TEST_CASE(test_multipleSlashesInVariableName) {
    std::cout << "***************************************************************"
                 "******************************************************"
              << std::endl;
    std::cout << "==> test_trailingSlashesInArrayVariableNames" << std::endl;
    std::cout << std::endl;

    BOOST_CHECK_THROW(MultiSlashVarModule app, ctk::logic_error);
    ;
  }

} // namespace Tests::testModules
