// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "EntityOwner.h"

#include "Application.h"
#include "Module.h"
#include "ModuleGraphVisitor.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <regex>

namespace ChimeraTK {

  /********************************************************************************************************************/

  EntityOwner::EntityOwner(
      const std::string& name, const std::string& description, const std::unordered_set<std::string>& tags)
  : _name(name), _description(description), _tags(tags) {}

  /********************************************************************************************************************/

  EntityOwner::EntityOwner()
  : _name("**INVALID**"), _description("Invalid EntityOwner created by default constructor just "
                                       "as a place holder") {}

  /********************************************************************************************************************/

  EntityOwner::~EntityOwner() {}

  /********************************************************************************************************************/

  EntityOwner& EntityOwner::operator=(EntityOwner&& other) {
    _name = std::move(other._name);
    _description = std::move(other._description);
    accessorList = std::move(other.accessorList);
    moduleList = std::move(other.moduleList);
    _tags = std::move(other._tags);
    for(auto mod : moduleList) {
      mod->setOwner(this);
    }
    for(auto& node : accessorList) {
      node.setOwningModule(this);
    }
    return *this;
  }

  /********************************************************************************************************************/

  void EntityOwner::registerModule(Module* module, bool addTags) {
    if(addTags)
      for(auto& tag : _tags) module->addTag(tag);
    moduleList.push_back(module);
  }

  /********************************************************************************************************************/

  void EntityOwner::unregisterModule(Module* module) {
    moduleList.remove(module);
  }

  /********************************************************************************************************************/

  std::list<VariableNetworkNode> EntityOwner::getAccessorListRecursive() const {
    // add accessors of this instance itself
    std::list<VariableNetworkNode> list = getAccessorList();

    // iterate through submodules
    for(auto submodule : getSubmoduleList()) {
      auto sublist = submodule->getAccessorListRecursive();
      list.insert(list.end(), sublist.begin(), sublist.end());
    }
    return list;
  }

  /********************************************************************************************************************/

  std::list<Module*> EntityOwner::getSubmoduleListRecursive() const {
    // add modules of this instance itself
    std::list<Module*> list = getSubmoduleList();

    // iterate through submodules
    for(auto submodule : getSubmoduleList()) {
      auto sublist = submodule->getSubmoduleListRecursive();
      list.insert(list.end(), sublist.begin(), sublist.end());
    }
    return list;
  }

  /********************************************************************************************************************/

  void EntityOwner::registerAccessor(VariableNetworkNode accessor) {
    for(auto& tag : _tags) accessor.addTag(tag);
    accessorList.push_back(accessor);
  }

  /********************************************************************************************************************/

  /********************************************************************************************************************/

  void EntityOwner::dump(const std::string& prefix) const {
    if(prefix == "") {
      std::cout << "==== Hierarchy dump of module '" << _name << "':" << std::endl;
    }

    for(auto& node : getAccessorList()) {
      std::cout << prefix << "+ ";
      node.dump();
    }

    for(auto& submodule : getSubmoduleList()) {
      std::cout << prefix << "| " << submodule->getName() << std::endl;
      submodule->dump(prefix + "| ");
    }
  }

  /********************************************************************************************************************/

  void EntityOwner::dumpGraph(const std::string& fileName) const {
    std::fstream file(fileName, std::ios_base::out);
    ModuleGraphVisitor v{file, true};
    v.dispatch(*this);
  }

  /********************************************************************************************************************/

  void EntityOwner::dumpModuleGraph(const std::string& fileName) const {
    std::fstream file(fileName, std::ios_base::out);
    ModuleGraphVisitor v{file, false};
    v.dispatch(*this);
  }

  /********************************************************************************************************************/

  void EntityOwner::addTag(const std::string& tag) {
    for(auto& node : getAccessorList()) node.addTag(tag);
    for(auto& submodule : getSubmoduleList()) submodule->addTag(tag);
    _tags.insert(tag);
  }

  /********************************************************************************************************************/

  bool EntityOwner::hasReachedTestableMode() {
    return testableModeReached;
  }

  /********************************************************************************************************************/

  void EntityOwner::applyHierarchyModifierToName(HierarchyModifier hierarchyModifier) {
    if(hierarchyModifier == HierarchyModifier::hideThis) {
      _name = ".";
    }
    else if(hierarchyModifier == HierarchyModifier::moveToRoot) {
      _name = "/" + _name;
    }
    else if(hierarchyModifier == HierarchyModifier::oneLevelUp) {
      _name = "../" + _name;
    }
    else if(hierarchyModifier == HierarchyModifier::oneUpAndHide) {
      _name = "..";
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
