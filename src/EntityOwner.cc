// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "EntityOwner.h"

#include "Module.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <regex>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  const std::string EntityOwner::namePrefixConstant{"@CONST@"};

  /********************************************************************************************************************/

  EntityOwner::EntityOwner(std::string name, std::string description, std::unordered_set<std::string> tags)
  : _name(std::move(name)), _description(std::move(description)), _tags(std::move(tags)) {}

  /********************************************************************************************************************/

  EntityOwner::EntityOwner()
  : _name("**INVALID**"), _description("Invalid EntityOwner created by default constructor just as a place holder") {}

  /********************************************************************************************************************/

  EntityOwner& EntityOwner::operator=(EntityOwner&& other) noexcept {
    _name = std::move(other._name);
    _description = std::move(other._description);
    _accessorList = std::move(other._accessorList);
    _moduleList = std::move(other._moduleList);
    _tags = std::move(other._tags);
    for(auto* mod : _moduleList) {
      mod->setOwner(this);
    }
    for(auto& node : _accessorList) {
      node.setOwningModule(this);
    }

    other._name = "**INVALID**";
    other._description = "This EntityOwner was moved from.";
    assert(other._accessorList.empty());
    assert(other._moduleList.empty());
    assert(other._tags.empty());

    return *this;
  }

  /********************************************************************************************************************/

  void EntityOwner::registerModule(Module* module, bool addTags) {
    if(addTags) {
      for(const auto& tag : _tags) module->addTag(tag);
    }
    _moduleList.push_back(module);
  }

  /********************************************************************************************************************/

  void EntityOwner::unregisterModule(Module* module) {
    _moduleList.remove(module);
  }

  /********************************************************************************************************************/

  std::list<VariableNetworkNode> EntityOwner::getAccessorListRecursive() const {
    // add accessors of this instance itself
    std::list<VariableNetworkNode> list = getAccessorList();

    // iterate through submodules
    for(const auto* submodule : getSubmoduleList()) {
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
    for(const auto* submodule : getSubmoduleList()) {
      auto sublist = submodule->getSubmoduleListRecursive();
      list.insert(list.end(), sublist.begin(), sublist.end());
    }
    return list;
  }

  /********************************************************************************************************************/

  void EntityOwner::registerAccessor(VariableNetworkNode accessor) {
    for(const auto& tag : _tags) accessor.addTag(tag);
    _accessorList.push_back(std::move(accessor));
  }

  /********************************************************************************************************************/

  /********************************************************************************************************************/

  void EntityOwner::dump(const std::string& prefix) const {
    if(prefix.empty()) {
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

  void EntityOwner::addTag(const std::string& tag) {
    for(auto& node : getAccessorList()) node.addTag(tag);
    for(auto& submodule : getSubmoduleList()) submodule->addTag(tag);
    _tags.insert(tag);
  }

  /********************************************************************************************************************/

  bool EntityOwner::hasReachedTestableMode() {
    return _testableModeReached;
  }

  /********************************************************************************************************************/

  std::string EntityOwner::applyHierarchyModifierToName(std::string name, HierarchyModifier hierarchyModifier) {
    if(hierarchyModifier == HierarchyModifier::hideThis) {
      name = ".";
    }
    else if(hierarchyModifier == HierarchyModifier::moveToRoot) {
      name = "/" + name;
    }
    else if(hierarchyModifier == HierarchyModifier::oneLevelUp) {
      name = "../" + name;
    }
    else if(hierarchyModifier == HierarchyModifier::oneUpAndHide) {
      name = "..";
    }
    return name;
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
