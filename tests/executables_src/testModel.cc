// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ApplicationModule.h"
#include "DeviceModule.h"
#include "Model.h"
#include "ModuleGroup.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

#define BOOST_TEST_MODULE testApplicationPVModel
#include <boost/test/included/unit_test.hpp>

namespace Tests::testApplicationPVModel {

  using namespace boost::unit_test_framework;
  namespace ctk = ChimeraTK;

  /*********************************************************************************************************************/
  /* Simple TestApplication */

  struct MyModule : ctk::ApplicationModule {
    MyModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, tags) {
      actuator.addTag("B");
    }

    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarOutput<int> actuator{this, "actuator", "unit", "Some output scalar"};

    struct PointlessVariableGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPollInput<int> readBack{this, "../readBack", "unit", "Some input scalar"};
    } pointlessVariableGroup{this, "pointlessVariableGroup", ""};

    void mainLoop() override {}
  };

  /*********************************************************************************************************************/

  struct TestModule : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ctk::ScalarPushInput<int> also{this, "also", "unit", "Some push input"};

    struct Need : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPollInput<int> tests{this, "tests", "unit", "Some poll input", {"B"}};
    } need{this, "need", ""};

    void mainLoop() override {}
  };

  /*********************************************************************************************************************/

  struct TestModuleGroup : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;

    TestModule testModule{this, ".", "The test module"};
  };

  /*********************************************************************************************************************/

  struct TestApplication : ctk::Application {
    TestApplication() : Application("testSuite") {}
    ctk::SetDMapFilePath dmap{"test.dmap"};

    ~TestApplication() override { shutdown(); }

    TestModuleGroup deeperHierarchies{this, "Deeper/hierarchies", "The test module group", {"A"}};
    MyModule myModule{this, "MyModule", "ApplicationModule directly owned by app"};
    MyModule myModule2{this, "Deeper/MyModule", "Additional "};
    ctk::DeviceModule dev{this, "Dummy0", "/somepath/dummyTrigger"}; // test2.map
  };

  /*********************************************************************************************************************/
  /* Generic tests */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testGraphViz) {
    // this is no real test, just a smoke test, since there might be too much variance in the dot graph output
    TestApplication app;
    app.getModel().writeGraphViz("test.dot");
    app.getModel().writeGraphViz("test-parenthood.dot", ChimeraTK::Model::keepParenthood);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testGetFullyQualifiedPath) {
    TestApplication app;

    BOOST_TEST(app.getModel().getFullyQualifiedPath() == "/");
    BOOST_TEST(app.deeperHierarchies.getModel().getFullyQualifiedPath() == "/Deeper/hierarchies");
    BOOST_TEST(app.deeperHierarchies.testModule.getModel().getFullyQualifiedPath() == "/Deeper/hierarchies");
    BOOST_TEST(app.deeperHierarchies.testModule.need.getModel().getFullyQualifiedPath() == "/Deeper/hierarchies/need");
    BOOST_TEST(app.deeperHierarchies.testModule.need.tests.getModel().getFullyQualifiedPath() ==
        "/Deeper/hierarchies/need/tests");
    BOOST_TEST(app.myModule.pointlessVariableGroup.readBack.getModel().getFullyQualifiedPath() == "/MyModule/readBack");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testIsValid) {
    TestApplication app;

    ChimeraTK::Model::RootProxy invalid;

    BOOST_TEST(invalid.isValid() == false);
    BOOST_TEST(app.getModel().isValid() == true);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testVisitByPath) {
    TestApplication app;
    bool found;

    ChimeraTK::Model::DirectoryProxy dir;
    found = app.getModel().visitByPath("/Deeper/hierarchies", [&](auto proxy) {
      if constexpr(isDirectory(proxy)) {
        dir = proxy;
      }
      else {
        BOOST_FAIL("Wrong proxy type found.");
      }
    });
    BOOST_TEST(found == true);
    BOOST_TEST(dir.isValid());
    BOOST_TEST(dir.getName() == "hierarchies");

    found = app.getModel().visitByPath(
        "/Deeper/hierarchies/notExisting", [&](auto) { BOOST_FAIL("Visitor must not be called."); });
    BOOST_TEST(found == false);

    ChimeraTK::Model::ProcessVariableProxy var;
    found = app.getModel().visitByPath("/Deeper/hierarchies/also", [&](auto proxy) {
      if constexpr(isVariable(proxy)) {
        var = proxy;
      }
      else {
        BOOST_FAIL("Wrong proxy type found.");
      }
    });
    BOOST_TEST(found == true);
    BOOST_TEST(var.isValid());
    BOOST_TEST(var.getName() == "also");
  }

  /*********************************************************************************************************************/
  /* Test functionality specific to the individual Proxy implementations */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testModuleGroupProxy) {
    TestApplication app;

    ChimeraTK::Model::ModuleGroupProxy proxy = app.deeperHierarchies.getModel();
    BOOST_CHECK(&proxy.getModuleGroup() == &app.deeperHierarchies);
    BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testApplicationModuleProxy) {
    TestApplication app;

    ChimeraTK::Model::ApplicationModuleProxy proxy = app.deeperHierarchies.testModule.getModel();
    BOOST_CHECK(&proxy.getApplicationModule() == &app.deeperHierarchies.testModule);
    BOOST_CHECK(proxy.getName() == ".");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testVariableGroupProxy) {
    TestApplication app;

    ChimeraTK::Model::VariableGroupProxy proxy = app.deeperHierarchies.testModule.need.getModel();
    BOOST_CHECK(&proxy.getVariableGroup() == &app.deeperHierarchies.testModule.need);
    BOOST_CHECK(proxy.getName() == "need");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDeviceModuleProxy) {
    TestApplication app;

    ChimeraTK::Model::DeviceModuleProxy proxy = app.dev.getModel();
    BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
    BOOST_REQUIRE(proxy.getTrigger().isValid());
    BOOST_CHECK(proxy.getTrigger().getName() == "dummyTrigger");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testProcessVariableProxy) {
    TestApplication app;

    ChimeraTK::Model::ProcessVariableProxy pv = app.myModule.actuator.getModel();
    BOOST_TEST(pv.getName() == "actuator");
    auto nodes = pv.getNodes();
    BOOST_TEST(nodes.size() == 2);
    BOOST_CHECK(nodes[0].getType() == ChimeraTK::NodeType::Device || nodes[1].getType() == ChimeraTK::NodeType::Device);
    BOOST_CHECK(nodes[0].getType() == ChimeraTK::NodeType::Application ||
        nodes[1].getType() == ChimeraTK::NodeType::Application);

    auto checker = [](auto proxy) {
      if constexpr(isVariable(proxy)) {
        BOOST_TEST(proxy.getName() == "readBack");
      }
      else {
        BOOST_FAIL("Wrong vertex type found");
      }
    };
    bool found = pv.visitByPath("../readBack", checker);
    BOOST_TEST(found == true);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDirectoryProxy) {
    TestApplication app;

    // get the directory. this relies on some other features...
    auto dir = app.myModule.getModel().visit(ChimeraTK::Model::returnDirectory, ChimeraTK::Model::getNeighbourDirectory,
        ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::DirectoryProxy{}));
    assert(dir.isValid());

    BOOST_TEST(dir.getName() == "MyModule");

    auto checker = [](auto proxy) {
      if constexpr(isVariable(proxy)) {
        BOOST_TEST(proxy.getName() == "readBack");
      }
      else {
        BOOST_FAIL("Wrong vertex type found");
      }
    };
    bool found = dir.visitByPath("./readBack", checker);
    BOOST_TEST(found == true);
  }

  /*********************************************************************************************************************/
  /* Test predicates */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testPredicatesWithProxy) {
    ChimeraTK::Model::RootProxy rp;
    BOOST_TEST(isRoot(rp) == true);
    BOOST_TEST(isModuleGroup(rp) == false);
    BOOST_TEST(isApplicationModule(rp) == false);
    BOOST_TEST(isVariableGroup(rp) == false);
    BOOST_TEST(isDeviceModule(rp) == false);
    BOOST_TEST(isVariable(rp) == false);
    BOOST_TEST(isDirectory(rp) == false);
    BOOST_TEST(hasName(rp) == false);

    ChimeraTK::Model::ModuleGroupProxy mgp;
    BOOST_TEST(isRoot(mgp) == false);
    BOOST_TEST(isModuleGroup(mgp) == true);
    BOOST_TEST(isApplicationModule(mgp) == false);
    BOOST_TEST(isVariableGroup(mgp) == false);
    BOOST_TEST(isDeviceModule(mgp) == false);
    BOOST_TEST(isVariable(mgp) == false);
    BOOST_TEST(isDirectory(mgp) == false);
    BOOST_TEST(hasName(mgp) == true);

    ChimeraTK::Model::ApplicationModuleProxy amp;
    BOOST_TEST(isRoot(amp) == false);
    BOOST_TEST(isModuleGroup(amp) == false);
    BOOST_TEST(isApplicationModule(amp) == true);
    BOOST_TEST(isVariableGroup(amp) == false);
    BOOST_TEST(isDeviceModule(amp) == false);
    BOOST_TEST(isVariable(amp) == false);
    BOOST_TEST(isDirectory(amp) == false);
    BOOST_TEST(hasName(amp) == true);

    ChimeraTK::Model::VariableGroupProxy vgp;
    BOOST_TEST(isRoot(vgp) == false);
    BOOST_TEST(isModuleGroup(vgp) == false);
    BOOST_TEST(isApplicationModule(vgp) == false);
    BOOST_TEST(isVariableGroup(vgp) == true);
    BOOST_TEST(isDeviceModule(vgp) == false);
    BOOST_TEST(isVariable(vgp) == false);
    BOOST_TEST(isDirectory(vgp) == false);
    BOOST_TEST(hasName(vgp) == true);

    ChimeraTK::Model::DeviceModuleProxy dmp;
    BOOST_TEST(isRoot(dmp) == false);
    BOOST_TEST(isModuleGroup(dmp) == false);
    BOOST_TEST(isApplicationModule(dmp) == false);
    BOOST_TEST(isVariableGroup(dmp) == false);
    BOOST_TEST(isDeviceModule(dmp) == true);
    BOOST_TEST(isVariable(dmp) == false);
    BOOST_TEST(isDirectory(dmp) == false);
    BOOST_TEST(hasName(dmp) == false);

    ChimeraTK::Model::ProcessVariableProxy pvp;
    BOOST_TEST(isRoot(pvp) == false);
    BOOST_TEST(isModuleGroup(pvp) == false);
    BOOST_TEST(isApplicationModule(pvp) == false);
    BOOST_TEST(isVariableGroup(pvp) == false);
    BOOST_TEST(isDeviceModule(pvp) == false);
    BOOST_TEST(isVariable(pvp) == true);
    BOOST_TEST(isDirectory(pvp) == false);
    BOOST_TEST(hasName(pvp) == true);

    ChimeraTK::Model::DirectoryProxy dp;
    BOOST_TEST(isRoot(dp) == false);
    BOOST_TEST(isModuleGroup(dp) == false);
    BOOST_TEST(isApplicationModule(dp) == false);
    BOOST_TEST(isVariableGroup(dp) == false);
    BOOST_TEST(isDeviceModule(dp) == false);
    BOOST_TEST(isVariable(dp) == false);
    BOOST_TEST(isDirectory(dp) == true);
    BOOST_TEST(hasName(dp) == true);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testPredicatesWithProperties) {
    TestApplication app;

    ChimeraTK::Model::VertexProperties::RootProperties rp{app};
    BOOST_TEST(isRoot(rp) == true);
    BOOST_TEST(isModuleGroup(rp) == false);
    BOOST_TEST(isApplicationModule(rp) == false);
    BOOST_TEST(isVariableGroup(rp) == false);
    BOOST_TEST(isDeviceModule(rp) == false);
    BOOST_TEST(isVariable(rp) == false);
    BOOST_TEST(isDirectory(rp) == false);
    BOOST_TEST(hasName(rp) == false);

    ChimeraTK::Model::VertexProperties::ModuleGroupProperties mgp{"xxx", app.deeperHierarchies};
    BOOST_TEST(isRoot(mgp) == false);
    BOOST_TEST(isModuleGroup(mgp) == true);
    BOOST_TEST(isApplicationModule(mgp) == false);
    BOOST_TEST(isVariableGroup(mgp) == false);
    BOOST_TEST(isDeviceModule(mgp) == false);
    BOOST_TEST(isVariable(mgp) == false);
    BOOST_TEST(isDirectory(mgp) == false);
    BOOST_TEST(hasName(mgp) == true);

    ChimeraTK::Model::VertexProperties::ApplicationModuleProperties amp{"xxx", app.myModule};
    BOOST_TEST(isRoot(amp) == false);
    BOOST_TEST(isModuleGroup(amp) == false);
    BOOST_TEST(isApplicationModule(amp) == true);
    BOOST_TEST(isVariableGroup(amp) == false);
    BOOST_TEST(isDeviceModule(amp) == false);
    BOOST_TEST(isVariable(amp) == false);
    BOOST_TEST(isDirectory(amp) == false);
    BOOST_TEST(hasName(amp) == true);

    ChimeraTK::Model::VertexProperties::VariableGroupProperties vgp{"xxx", app.myModule.pointlessVariableGroup};
    BOOST_TEST(isRoot(vgp) == false);
    BOOST_TEST(isModuleGroup(vgp) == false);
    BOOST_TEST(isApplicationModule(vgp) == false);
    BOOST_TEST(isVariableGroup(vgp) == true);
    BOOST_TEST(isDeviceModule(vgp) == false);
    BOOST_TEST(isVariable(vgp) == false);
    BOOST_TEST(isDirectory(vgp) == false);
    BOOST_TEST(hasName(vgp) == true);

    ChimeraTK::Model::VertexProperties::DeviceModuleProperties dmp{"xxx", {}, app.dev};
    BOOST_TEST(isRoot(dmp) == false);
    BOOST_TEST(isModuleGroup(dmp) == false);
    BOOST_TEST(isApplicationModule(dmp) == false);
    BOOST_TEST(isVariableGroup(dmp) == false);
    BOOST_TEST(isDeviceModule(dmp) == true);
    BOOST_TEST(isVariable(dmp) == false);
    BOOST_TEST(isDirectory(dmp) == false);
    BOOST_TEST(hasName(dmp) == false);

    ChimeraTK::Model::VertexProperties::ProcessVariableProperties pvp{"xxx", {}, {}};
    BOOST_TEST(isRoot(pvp) == false);
    BOOST_TEST(isModuleGroup(pvp) == false);
    BOOST_TEST(isApplicationModule(pvp) == false);
    BOOST_TEST(isVariableGroup(pvp) == false);
    BOOST_TEST(isDeviceModule(pvp) == false);
    BOOST_TEST(isVariable(pvp) == true);
    BOOST_TEST(isDirectory(pvp) == false);
    BOOST_TEST(hasName(pvp) == true);

    ChimeraTK::Model::VertexProperties::DirectoryProperties dp{"xxx"};
    BOOST_TEST(isRoot(dp) == false);
    BOOST_TEST(isModuleGroup(dp) == false);
    BOOST_TEST(isApplicationModule(dp) == false);
    BOOST_TEST(isVariableGroup(dp) == false);
    BOOST_TEST(isDeviceModule(dp) == false);
    BOOST_TEST(isVariable(dp) == false);
    BOOST_TEST(isDirectory(dp) == true);
    BOOST_TEST(hasName(dp) == true);
  }

  /*********************************************************************************************************************/
  /* Test search types */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testAdjacentIn) {
    TestApplication app;

    // Check on root
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
          // Root is the neighbouring directory of the device module
          BOOST_TEST(proxy.getAliasOrCdd() == "Dummy0");
        }
        else if constexpr(ChimeraTK::Model::isRoot(proxy)) {
          // Root is its own neighbouring directory
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.getModel().visit(checker, ChimeraTK::Model::adjacentInSearch);

      BOOST_TEST(foundElements == 2);
    }

    // Check on MyModule application module
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          // this PV is an input
          BOOST_TEST(proxy.getName() == "readBack");
        }
        else if constexpr(ChimeraTK::Model::isRoot(proxy)) {
          // module is owned by root
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::adjacentInSearch);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testAdjacentOut) {
    TestApplication app;

    // Check on root
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
          // Root owns the DeviceModule
          BOOST_TEST(proxy.getAliasOrCdd() == "Dummy0");
        }
        else if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          // Root owns several directories
          const auto& name = proxy.getName();
          BOOST_CHECK(name == "Deeper" || name == "MyModule" || name == "somepath" || name == "Devices");
        }
        else if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
          // Root owns the ModuleGroup
          const auto& name = proxy.getName();
          BOOST_CHECK(name == "Deeper/hierarchies");
        }
        else if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          const auto& name = proxy.getName();
          BOOST_CHECK(name == "MyModule" || name == "Deeper/MyModule" || name == "/Devices/Dummy0");
        }
        else if constexpr(ChimeraTK::Model::isRoot(proxy)) {
          // Root is its own neighbouring directory
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.getModel().visit(checker, ChimeraTK::Model::adjacentOutSearch);

      BOOST_TEST(foundElements == 10);
    }

    // Check on MyModule application module
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          // this variable is an output
          BOOST_CHECK(proxy.getName() == "actuator");
        }
        else if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          // The neightbouring directory
          BOOST_CHECK(proxy.getName() == "MyModule");
        }
        else if constexpr(ChimeraTK::Model::isVariableGroup(proxy)) {
          // VariableGroup owned by the module
          BOOST_CHECK(proxy.getName() == "pointlessVariableGroup");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::adjacentOutSearch);

      BOOST_TEST(foundElements == 4); // actuator is found twice because of pvAccess and ownership relationships
    }
  }

  /*********************************************************************************************************************/

  // helper class for testAdjacent
  struct Item {
    template<typename PROXY>
    explicit Item(PROXY proxy) : type(typeid(proxy)) {
      if constexpr(ChimeraTK::Model::hasName(proxy)) {
        nameOrAlias = proxy.getName();
      }
      else if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
        nameOrAlias = proxy.getAliasOrCdd();
      }
      else {
        nameOrAlias = "(unnamed)";
      }
    }
    const std::type_info& type;
    std::string nameOrAlias;

    bool operator<(const Item& rhs) const {
      return &type < &rhs.type || (&type == &rhs.type && nameOrAlias < rhs.nameOrAlias);
    }
  };

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testAdjacent) {
    // adjacent is the sum of adjacentIn and adjacentOut
    TestApplication app;

    // First collect information about search results of adjacentOut and adjacentIn.
    std::set<Item> items;
    size_t itemsToFind = 0; // count also duplicates
    auto collector = [&](auto proxy) {
      Item itemToInsert(proxy);
      items.insert(itemToInsert);
      ++itemsToFind;
    };

    app.getModel().visit(collector, ChimeraTK::Model::adjacentOutSearch);
    app.getModel().visit(collector, ChimeraTK::Model::adjacentInSearch);

    // Now compare the result of the adjacent search (without implying a certain ordering)
    size_t itemsFound = 0;
    auto finder = [&](auto proxy) {
      Item itemToFind(proxy);
      // adjacent search result item must be among items previously found in either adjacentOut or adjacentIn
      BOOST_CHECK(items.find(itemToFind) != items.end());
      ++itemsFound;
    };
    app.getModel().visit(finder, ChimeraTK::Model::adjacentSearch);
    // check that all items have been found
    BOOST_TEST(itemsFound == itemsToFind);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testDepthFirstSearch) {
    TestApplication app;

    std::vector<std::string> pvNames;

    auto pvNamesFiller = [&](auto proxy) { pvNames.push_back(proxy.getFullyQualifiedPath()); };

    app.getModel().visit(pvNamesFiller, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepDirectories,
        ChimeraTK::Model::keepParenthood);

    BOOST_TEST(pvNames.size() == 10);

    // All directories have been found
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies/need") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule/pointlessVariableGroup") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/somepath") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/MyModule") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/MyModule/pointlessVariableGroup") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Devices") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Devices/Dummy0") != pvNames.end());

    // Check ordering: depth first, not breadth first
    // Note: The ordering on a single hierarchy is not strictly defined, hence we need to make the test insensitive
    // to allowed reordering. Hence we have two allowed cases:
    //  1) /Deeper/hierarchies is found before /Deeper/MyModule
    //  2) /Deeper/MyModule is found before /Deeper/hierarchies
    // In case 1), /Deeper/hierarchies/need needs to be found before /Deeper/MyModule
    // In case 2), /Deeper/MyModule/pointlessVariableGroup needs to be found before /Deeper/hierarchies
    auto deeperHierarchies = std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies");
    auto deeperHierarchiesNeed = std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies/need");
    auto deeperMyModule = std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule");
    auto deeperMyModulePVG = std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule/pointlessVariableGroup");

    BOOST_CHECK((deeperHierarchies < deeperMyModule && deeperHierarchiesNeed < deeperMyModule) ||
        (deeperMyModule < deeperHierarchies && deeperMyModulePVG < deeperHierarchies));
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testBreadthFirstSearch) {
    TestApplication app;

    std::vector<std::string> pvNames;

    auto pvNamesFiller = [&](auto proxy) { pvNames.push_back(proxy.getFullyQualifiedPath()); };

    app.getModel().visit(pvNamesFiller, ChimeraTK::Model::breadthFirstSearch, ChimeraTK::Model::keepDirectories,
        ChimeraTK::Model::keepParenthood);

    BOOST_TEST(pvNames.size() == 10);

    // All directories have been found
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies/need") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule/pointlessVariableGroup") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/somepath") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/MyModule") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/MyModule/pointlessVariableGroup") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Devices") != pvNames.end());
    BOOST_CHECK(std::find(pvNames.begin(), pvNames.end(), "/Devices/Dummy0") != pvNames.end());

    // Check ordering: breadth first, not depth first
    auto deeperHierarchies = std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies");
    auto deeperHierarchiesNeed = std::find(pvNames.begin(), pvNames.end(), "/Deeper/hierarchies/need");
    auto deeperMyModule = std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule");
    auto deeperMyModulePVG = std::find(pvNames.begin(), pvNames.end(), "/Deeper/MyModule/pointlessVariableGroup");

    BOOST_CHECK(deeperHierarchies < deeperHierarchiesNeed);
    BOOST_CHECK(deeperMyModule < deeperHierarchiesNeed);
    BOOST_CHECK(deeperHierarchies < deeperMyModulePVG);
    BOOST_CHECK(deeperMyModule < deeperMyModulePVG);
  }

  /*********************************************************************************************************************/
  /* Test edge/relationship filters */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepPvAccess) {
    TestApplication app;

    // Run check on ApplicationModule MyModule
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "readBack" || proxy.getName() == "actuator");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::keepPvAccess, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 2);
    }

    // Run check on the PV readBack
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
          BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
        }
        else if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          BOOST_CHECK(proxy.getName() == "MyModule");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.pointlessVariableGroup.readBack.getModel().visit(
          checker, ChimeraTK::Model::keepPvAccess, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepOwnership) {
    TestApplication app;

    // Run check on ApplicationModule MyModule
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "actuator");
        }
        else if constexpr(ChimeraTK::Model::isVariableGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "pointlessVariableGroup");
        }
        else if constexpr(ChimeraTK::Model::isRoot(proxy)) {
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::keepOwnership, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 3);
    }

    // Run check on the PV readBack
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariableGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "pointlessVariableGroup");
        }
        else if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
          BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.pointlessVariableGroup.readBack.getModel().visit(
          checker, ChimeraTK::Model::keepOwnership, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepParenthood) {
    TestApplication app;

    // Run check on directory MyModule
    {
      // get the directory. this relies on some other features...
      auto dir =
          app.myModule.getModel().visit(ChimeraTK::Model::returnDirectory, ChimeraTK::Model::getNeighbourDirectory,
              ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::DirectoryProxy{}));
      assert(dir.isValid());

      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "actuator" || proxy.getName() == "readBack");
        }
        else if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "pointlessVariableGroup");
        }
        else if constexpr(ChimeraTK::Model::isRoot(proxy)) {
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      dir.visit(checker, ChimeraTK::Model::keepParenthood, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 4);
    }

    // Run check on the PV readBack
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "MyModule");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.pointlessVariableGroup.readBack.getModel().visit(
          checker, ChimeraTK::Model::keepParenthood, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepNeighbourhood) {
    TestApplication app;

    // Run check on ApplicationModule MyModule
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "MyModule");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::keepNeighbourhood, ChimeraTK::Model::adjacentOutSearch);

      BOOST_TEST(foundElements == 1);
    }

    // Run check on the directory /Deeper/hierarchies
    {
      // get the directory. this relies on some other features...
      ChimeraTK::Model::DirectoryProxy dir;
      [[maybe_unused]] auto found = app.getModel().visitByPath("/Deeper/hierarchies", [&](auto proxy) {
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          dir = proxy;
        }
      });
      assert(found);

      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
        }
        else if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          BOOST_CHECK(proxy.getName() == ".");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      dir.visit(checker, ChimeraTK::Model::keepNeighbourhood, ChimeraTK::Model::adjacentInSearch);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/
  /* Test vertex/object type filters */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepModuleGroups) {
    TestApplication app;

    // Run check on root
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.getModel().visit(checker, ChimeraTK::Model::keepModuleGroups, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }

    // Run check on the application module "."
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.getModel().visit(
          checker, ChimeraTK::Model::keepModuleGroups, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepApplicationModules) {
    TestApplication app;

    // Run check on module group Deeper/hierarchies
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          BOOST_CHECK(proxy.getName() == ".");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.getModel().visit(
          checker, ChimeraTK::Model::keepApplicationModules, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }

    // Run check on PV "also"
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          BOOST_CHECK(proxy.getName() == ".");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.also.getModel().visit(
          checker, ChimeraTK::Model::keepApplicationModules, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements ==
          2); // The element is found twice because there is an ownership relation and a PV access relation
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepVariableGroups) {
    TestApplication app;

    // Run check on application module MyModule
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariableGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "pointlessVariableGroup");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::keepVariableGroups, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }

    // Run check on PV "tests"
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariableGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "need");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.need.tests.getModel().visit(
          checker, ChimeraTK::Model::keepVariableGroups, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepDeviceModules) {
    TestApplication app;

    // Run check on root
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
          BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.getModel().visit(checker, ChimeraTK::Model::keepDeviceModules, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 2); // found twice because ownership and neighbourhood relation
    }

    // Run check on PV "tests"
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
          BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.need.tests.getModel().visit(
          checker, ChimeraTK::Model::keepDeviceModules, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 2); // found twice because ownership and pv acces relation
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepProcessVariables) {
    TestApplication app;

    // Run check on application module MyModule
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "readBack" || proxy.getName() == "actuator");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::keepProcessVariables, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 3); // actuator is found twice due to pvAccess and ownership relation
    }

    // Run check on the directory /Deeper/hierarchies
    {
      // get the directory. this relies on some other features...
      ChimeraTK::Model::DirectoryProxy dir;
      [[maybe_unused]] auto found = app.getModel().visitByPath("/Deeper/hierarchies", [&](auto proxy) {
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          dir = proxy;
        }
      });
      assert(found);

      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "also");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      dir.visit(checker, ChimeraTK::Model::keepProcessVariables, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepDirectories) {
    TestApplication app;

    // Run check on application module MyModule
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "MyModule");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.myModule.getModel().visit(checker, ChimeraTK::Model::keepDirectories, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 1);
    }

    // Run check on the directory /Deeper/hierarchies
    {
      // get the directory. this relies on some other features...
      ChimeraTK::Model::DirectoryProxy dir;
      [[maybe_unused]] auto found = app.getModel().visitByPath("/Deeper/hierarchies", [&](auto proxy) {
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          dir = proxy;
        }
      });
      assert(found);

      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper" || proxy.getName() == "need");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      dir.visit(checker, ChimeraTK::Model::keepDirectories, ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepName) {
    TestApplication app;

    // Run check on application root
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "MyModule");
        }
        else if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          BOOST_CHECK(proxy.getName() == "MyModule");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.getModel().visit(checker, ChimeraTK::Model::keepName("MyModule"), ChimeraTK::Model::adjacentSearch);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testKeepTag) {
    TestApplication app;

    // Search for tag A
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        BOOST_CHECK(proxy.getFullyQualifiedPath() == "/Deeper/hierarchies/also" ||
            proxy.getFullyQualifiedPath() == "/Deeper/hierarchies/need/tests");
      };

      app.getModel().visit(checker, ChimeraTK::Model::keepTag("A"), ChimeraTK::Model::depthFirstSearch,
          ChimeraTK::Model::keepProcessVariables);

      BOOST_TEST(foundElements == 2);
    }

    // Search for tag B
    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        BOOST_CHECK(proxy.getFullyQualifiedPath() == "/MyModule/actuator" ||
            proxy.getFullyQualifiedPath() == "/Deeper/MyModule/actuator" ||
            proxy.getFullyQualifiedPath() == "/Deeper/hierarchies/need/tests");
      };

      app.getModel().visit(checker, ChimeraTK::Model::keepTag("B"), ChimeraTK::Model::depthFirstSearch,
          ChimeraTK::Model::keepProcessVariables);

      BOOST_TEST(foundElements == 3);
    }
  }

  /*********************************************************************************************************************/
  /* Test search options */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testReturnFirstHit) {
    TestApplication app;
    std::string alias;

    // Check returning a string
    auto returnAlias = [&](auto proxy) -> std::string { return proxy.getAliasOrCdd(); };
    alias = app.getModel().visit(returnAlias, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepDeviceModules,
        ChimeraTK::Model::returnFirstHit(std::string{}));
    BOOST_TEST(alias == "Dummy0");

    // Check returning nothing (void)
    alias = "";
    auto setAlias = [&](auto proxy) -> void { alias = proxy.getAliasOrCdd(); };
    app.getModel().visit(setAlias, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepDeviceModules,
        ChimeraTK::Model::returnFirstHit());
    BOOST_TEST(alias == "Dummy0");
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testContinueSearchDisjunctTrees) {
    TestApplication app;

    size_t hits{0};
    auto countHits = [&](auto) { ++hits; };
    app.getModel().visit(countHits, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepPvAccess,
        ChimeraTK::Model::keepProcessVariables);

    // first make sure nothing is found when doing a DFS without continueSearchDisjunctTrees from root with the
    // keepPvAccess
    BOOST_TEST(hits == 0);

    // same test again with continueSearchDisjunctTrees should now find something as the search is continued in the
    // disjuct parts
    app.getModel().visit(countHits, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepPvAccess,
        ChimeraTK::Model::keepProcessVariables, ChimeraTK::Model::continueSearchDisjunctTrees);
    BOOST_TEST(hits == 10);
  }

  /*********************************************************************************************************************/
  /* Test OrSet and AndSet of filters */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOrSetVertexFilter) {
    TestApplication app;

    size_t foundElements = 0;

    auto checker = [&](auto proxy) {
      ++foundElements;
      if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
        BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
      }
      else if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
        BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
      }
      else {
        BOOST_FAIL("Wrong vertex type found");
      }
    };

    app.getModel().visit(checker, ChimeraTK::Model::keepModuleGroups || ChimeraTK::Model::keepDeviceModules,
        ChimeraTK::Model::adjacentSearch);

    BOOST_TEST(foundElements == 3); // the DeviceModule is found twice (ownership + neighbourhood)
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testAndSetVertexFilter) {
    TestApplication app;

    size_t foundElements = 0;

    auto checker = [&](auto proxy) {
      ++foundElements;
      if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
        BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
      }
      else {
        BOOST_FAIL("Wrong vertex type found");
      }
    };

    app.getModel().visit(checker,
        ChimeraTK::Model::keepModuleGroups && ChimeraTK::Model::keepName("Deeper/hierarchies"),
        ChimeraTK::Model::adjacentSearch);

    BOOST_TEST(foundElements == 1);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testAndSetInOrSetVertexFilter) {
    TestApplication app;

    size_t foundElements = 0;

    auto checker = [&](auto proxy) {
      ++foundElements;
      if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
        BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
      }
      else if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
        BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
      }
      else {
        BOOST_FAIL("Wrong vertex type found");
      }
    };

    app.getModel().visit(checker,
        (ChimeraTK::Model::keepModuleGroups && ChimeraTK::Model::keepName("Deeper/hierarchies")) ||
            ChimeraTK::Model::keepDeviceModules,
        ChimeraTK::Model::adjacentOutSearch);

    BOOST_TEST(foundElements == 2);

    foundElements = 0;

    app.getModel().visit(checker,
        ChimeraTK::Model::keepDeviceModules ||
            (ChimeraTK::Model::keepModuleGroups && ChimeraTK::Model::keepName("Deeper/hierarchies")),
        ChimeraTK::Model::adjacentOutSearch);

    BOOST_TEST(foundElements == 2);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOrSetInAndSetVertexFilter) {
    TestApplication app;

    size_t foundElements = 0;

    auto checker = [&](auto proxy) {
      ++foundElements;
      if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
        BOOST_CHECK(proxy.getName() == "MyModule");
      }
      else if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
        BOOST_CHECK(proxy.getName() == "MyModule");
      }
      else {
        BOOST_FAIL("Wrong vertex type found");
      }
    };

    app.getModel().visit(checker,
        (ChimeraTK::Model::keepApplicationModules || ChimeraTK::Model::keepDirectories) &&
            ChimeraTK::Model::keepName("MyModule"),
        ChimeraTK::Model::adjacentOutSearch);

    BOOST_TEST(foundElements == 2);

    foundElements = 0;

    app.getModel().visit(checker,
        ChimeraTK::Model::keepName("MyModule") &&
            (ChimeraTK::Model::keepApplicationModules || ChimeraTK::Model::keepDirectories),
        ChimeraTK::Model::adjacentOutSearch);

    BOOST_TEST(foundElements == 2);
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOrSetEdgeFilter) {
    TestApplication app;

    size_t foundElements = 0;

    auto checker = [&](auto proxy) {
      ++foundElements;
      if constexpr(ChimeraTK::Model::isDeviceModule(proxy)) {
        BOOST_CHECK(proxy.getAliasOrCdd() == "Dummy0");
      }
      else if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
        BOOST_CHECK(proxy.getName() == "Deeper" || proxy.getName() == "MyModule" || proxy.getName() == "somepath" ||
            proxy.getName() == "Devices");
      }
      else if constexpr(ChimeraTK::Model::isRoot(proxy)) {
      }
      else {
        BOOST_FAIL("Wrong vertex type found");
      }
    };

    app.getModel().visit(checker, ChimeraTK::Model::keepNeighbourhood || ChimeraTK::Model::keepParenthood,
        ChimeraTK::Model::adjacentSearch);

    BOOST_TEST(foundElements == 7); // ROOT is found twice: incoming and outgoing neighbourhood to itself
  }

  // Note: AndSet for edge filters does not really make any sense, since each edge can have only one single type!

  /*********************************************************************************************************************/
  /* Test combined search configurations */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOwnedModuleGroups) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.getModel().visit(checker, ChimeraTK::Model::ownedModuleGroups);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOwnedApplicationModules) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          BOOST_CHECK(proxy.getName() == ".");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.getModel().visit(checker, ChimeraTK::Model::ownedApplicationModules);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOwnedVariableGroups) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariableGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "need");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.getModel().visit(checker, ChimeraTK::Model::ownedVariableGroups);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testOwnedVariables) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "also");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.getModel().visit(checker, ChimeraTK::Model::ownedVariables);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testChildDirectories) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper" || proxy.getName() == "MyModule" || proxy.getName() == "somepath" ||
              proxy.getName() == "Devices");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.getModel().visit(checker, ChimeraTK::Model::childDirectories);

      BOOST_TEST(foundElements == 4);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testChildVariables) {
    TestApplication app;

    {
      // get the directory. this relies on some other features...
      ChimeraTK::Model::DirectoryProxy dir;
      [[maybe_unused]] auto found = app.getModel().visitByPath("/MyModule", [&](auto proxy) {
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          dir = proxy;
        }
      });
      assert(found);

      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "readBack" || proxy.getName() == "actuator");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      dir.visit(checker, ChimeraTK::Model::childVariables);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testChildren) {
    TestApplication app;

    {
      // get the directory. this relies on some other features...
      ChimeraTK::Model::DirectoryProxy dir;
      [[maybe_unused]] auto found = app.getModel().visitByPath("/MyModule", [&](auto proxy) {
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          dir = proxy;
        }
      });
      assert(found);

      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isVariable(proxy)) {
          BOOST_CHECK(proxy.getName() == "readBack" || proxy.getName() == "actuator");
        }
        else if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "pointlessVariableGroup");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      dir.visit(checker, ChimeraTK::Model::children);

      BOOST_TEST(foundElements == 3);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testGetOwner) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.getModel().visit(checker, ChimeraTK::Model::getOwner);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testGetParent) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "need");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.need.tests.getModel().visit(checker, ChimeraTK::Model::getParent);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testGetNeighbourDirectory) {
    TestApplication app;

    {
      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          BOOST_CHECK(proxy.getName() == "hierarchies");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      app.deeperHierarchies.testModule.getModel().visit(checker, ChimeraTK::Model::getNeighbourDirectory);

      BOOST_TEST(foundElements == 1);
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testNeighbourModules) {
    TestApplication app;

    {
      // get the directory. this relies on some other features...
      ChimeraTK::Model::DirectoryProxy dir;
      [[maybe_unused]] auto found = app.getModel().visitByPath("/Deeper/hierarchies", [&](auto proxy) {
        if constexpr(ChimeraTK::Model::isDirectory(proxy)) {
          dir = proxy;
        }
      });
      assert(found);

      size_t foundElements = 0;

      auto checker = [&](auto proxy) {
        ++foundElements;
        if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
          BOOST_CHECK(proxy.getName() == ".");
        }
        else if constexpr(ChimeraTK::Model::isModuleGroup(proxy)) {
          BOOST_CHECK(proxy.getName() == "Deeper/hierarchies");
        }
        else {
          BOOST_FAIL("Wrong vertex type found");
        }
      };

      dir.visit(checker, ChimeraTK::Model::neighbourModules);

      BOOST_TEST(foundElements == 2);
    }
  }

  /*********************************************************************************************************************/
  /* Test pre-defined visitor functors */
  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testReturnModuleGroup) {
    TestApplication app;

    {
      ChimeraTK::Model::ModuleGroupProxy rv =
          app.deeperHierarchies.testModule.getModel().visit(ChimeraTK::Model::returnModuleGroup,
              ChimeraTK::Model::getOwner, ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::ModuleGroupProxy{}));

      BOOST_TEST(rv.isValid());
      BOOST_TEST(rv.getName() == "Deeper/hierarchies");
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testReturnApplicationModule) {
    TestApplication app;

    {
      ChimeraTK::Model::ApplicationModuleProxy rv =
          app.deeperHierarchies.testModule.need.getModel().visit(ChimeraTK::Model::returnApplicationModule,
              ChimeraTK::Model::getOwner, ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::ApplicationModuleProxy{}));

      BOOST_TEST(rv.isValid());
      BOOST_TEST(rv.getName() == ".");
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testReturnVariableGroup) {
    TestApplication app;

    {
      ChimeraTK::Model::VariableGroupProxy rv =
          app.myModule2.pointlessVariableGroup.readBack.getModel().visit(ChimeraTK::Model::returnVariableGroup,
              ChimeraTK::Model::getOwner, ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::VariableGroupProxy{}));

      BOOST_TEST(rv.isValid());
      BOOST_TEST(rv.getName() == "pointlessVariableGroup");
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testReturnProcessVariable) {
    TestApplication app;

    {
      ChimeraTK::Model::ProcessVariableProxy rv = app.deeperHierarchies.testModule.need.getModel().visit(
          ChimeraTK::Model::returnProcessVariable, ChimeraTK::Model::ownedVariables,
          ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::ProcessVariableProxy{}));

      BOOST_TEST(rv.isValid());
      BOOST_TEST(rv.getName() == "tests");
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testReturnDirectory) {
    TestApplication app;

    {
      ChimeraTK::Model::DirectoryProxy rv = app.deeperHierarchies.testModule.need.getModel().visit(
          ChimeraTK::Model::returnDirectory, ChimeraTK::Model::getNeighbourDirectory,
          ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::DirectoryProxy{}));

      BOOST_TEST(rv.isValid());
      BOOST_TEST(rv.getName() == "need");
    }
  }

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testIllegalNames) {
    TestApplication app;

    constexpr std::string_view illegalCharsToTest = "-~!@#$%^&*()-=+{}|[]\\;':\",.<>?` ";

    for(auto c : illegalCharsToTest) {
      std::string nameToTest = "MyModule" + std::string(1, c) + "withIllegalChar";
      BOOST_CHECK_THROW(
          (app.myModule = MyModule(&app, nameToTest, "ApplicationModule directly owned by app")), ctk::logic_error);
    }
  }

  /*********************************************************************************************************************/

  struct RogueModule : ctk::ApplicationModule {
    ctk::ScalarPushInput<int> var{this, "trigger", "", ""};

    // This module has a push input and creates a temporary input in its constructor with the same name
    // The second one is never used and thrown away immediately. This is the smallest possible reproduction
    // for redmine issue 11105
    RogueModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ctk::ApplicationModule(owner, name, description, tags) {
      auto v = ctk::ScalarPushInput<int>{this, "trigger", "", ""};
    }

    void mainLoop() override {}
  };

  struct TestApplication2 : ctk::Application {
    TestApplication2() : Application("testSuite") {}
    ctk::SetDMapFilePath dmap{"test.dmap"};

    ~TestApplication2() override { shutdown(); }

    RogueModule myModule{this, "MyModule", "ApplicationModule directly owned by app"};
  };

  /*********************************************************************************************************************/

  BOOST_AUTO_TEST_CASE(testMassCreationOfUnusedAcecssors) {
    TestApplication2 app;
    ChimeraTK::Model::ProcessVariableProxy rv = app.myModule.getModel().visit(ChimeraTK::Model::returnProcessVariable,
        ChimeraTK::Model::ownedVariables, ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::ProcessVariableProxy{}));

    BOOST_TEST(rv.isValid());
    BOOST_TEST(rv.getName() == "trigger");
  }

  /*********************************************************************************************************************/

} // namespace Tests::testApplicationPVModel
