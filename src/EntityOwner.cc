// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "EntityOwner.h"

#include "Module.h"

#include <boost/core/demangle.hpp>

#include <cassert>
#include <fstream>
#include <iostream>
#include <regex>

namespace ChimeraTK {

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
      for(const auto& tag : _tags) {
        module->addTag(tag);
      }
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
    for(const auto& tag : _tags) {
      accessor.addTag(tag);
    }
    _accessorList.push_back(std::move(accessor));
  }

  /********************************************************************************************************************/

  /********************************************************************************************************************/

  void EntityOwner::dump(const std::string& prefix, std::ostream& stream) const {
    if(prefix.empty()) {
      stream << "==== Hierarchy dump of module '" << _name << "':" << std::endl;
    }

    for(auto& node : getAccessorList()) {
      stream << prefix << "+ ";
      node.dump(stream);
    }

    for(auto& submodule : getSubmoduleList()) {
      stream << prefix << "| " << submodule->getName() << std::endl;
      submodule->dump(prefix + "| ", stream);
    }
  }

  /********************************************************************************************************************/

  void EntityOwner::addTag(const std::string& tag) {
    for(auto& node : getAccessorList()) {
      node.addTag(tag);
    }
    for(auto& submodule : getSubmoduleList()) {
      submodule->addTag(tag);
    }
    if(_tags.erase(negateTag(tag)) == 0) {
      // negated tag was not found, so insert the tag
      _tags.insert(tag);
    }
  }

  /********************************************************************************************************************/

  bool EntityOwner::hasReachedTestableMode() {
    return _testableModeReached;
  }

  /********************************************************************************************************************/

  std::string negateTag(const std::string& tag) {
    if(!tag.empty() && tag[0] == '!') {
      return tag.substr(1);
    }
    return '!' + tag;
  }

  /********************************************************************************************************************/

  std::string EntityOwner::getQualifiedNameWithType() const {
    return getQualifiedName() + "<" + boost::core::demangle(typeid(*this).name()) + ">";
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
