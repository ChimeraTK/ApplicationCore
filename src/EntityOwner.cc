// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "EntityOwner.h"

#include "Application.h"
#include "Module.h"
#include "ModuleGraphVisitor.h"
#include "VirtualModule.h"

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

  VirtualModule EntityOwner::findTag(const std::string& tag) const {
    // create new module to return
    VirtualModule module{_name, _description, getModuleType()};

    // add everything matching the tag to the virtual module and return it
    if(this == &Application::getInstance()) {
      // if this module is the top-level application, we need special treatment for HierarchyModifier::moveToRoot
      findTagAndAppendToModule(module, tag, false, true, false, module);
    }
    else {
      // Not the top-level module: Things that are moved to the top-level are simply discarded
      VirtualModule discard("discarded", "", ModuleType::Invalid);
      findTagAndAppendToModule(module, tag, false, true, false, discard);
    }

    // remove all empty virtual modules
    module.stripEmptyChildsRecursive();

    return module;
  }

  /********************************************************************************************************************/

  VirtualModule EntityOwner::excludeTag(const std::string& tag) const {
    // create new module to return
    VirtualModule module{_name, _description, getModuleType()};

    // add everything matching the tag to the virtual module and return it
    if(this == &Application::getInstance()) {
      // if this module is the top-level application, we need special treatment for HierarchyModifier::moveToRoot
      findTagAndAppendToModule(module, tag, false, true, true, module);
    }
    else {
      // Not the top-level module: Things that are moved to the top-level are simply discarded
      VirtualModule discard("discarded", "", ModuleType::Invalid);
      findTagAndAppendToModule(module, tag, false, true, true, discard);
    }

    // remove all empty virtual modules
    module.stripEmptyChildsRecursive();

    return module;
  }

  /********************************************************************************************************************/

  void EntityOwner::registerAccessor(VariableNetworkNode accessor) {
    for(auto& tag : _tags) accessor.addTag(tag);
    accessorList.push_back(accessor);
  }

  /********************************************************************************************************************/

  // The function adds virtual versions of the EntityOwner itself anf all its children to a virtual module (parent).
  void EntityOwner::findTagAndAppendToModule(VirtualModule& virtualParent, const std::string& tag,
      bool eliminateAllHierarchies, bool eliminateFirstHierarchy, bool negate, VirtualModule& root) const {
    throw ChimeraTK::logic_error("sorry, currently not implemented ;-)");
    /*
    // It might be that it is requested to hide ourselves. In this case we do not add
    // ourselves but directly put the children into the parent (or grand parent, depending on the hierarchy modifier).
    // So we store which module to add to (either a new virtual module, the virtual parent or virtual grand parent)
    VirtualModule* moduleToAddTo;

    // Check whether we are supposed to hide. This can be the case if
    // * the hierarchy modifier says so (hideThis or oneUpAndHide -> getEliminateHierarchy is true)
    // * flatten was called (eliminateAllHierarchies is true)
    // * the virtual hierarchy level of virtualMe already exists elsewhere and we just add our children to it
    if(!getEliminateHierarchy() && !eliminateAllHierarchies && !eliminateFirstHierarchy) {
      // add the virtualMe to the right owner
      if(_hierarchyModifier == HierarchyModifier::moveToRoot) {
        moduleToAddTo = &root.createAndGetSubmodule(_name);
      }
      else if((_hierarchyModifier == HierarchyModifier::oneLevelUp) ||
          (_hierarchyModifier == HierarchyModifier::oneUpAndHide)) {
        auto virtualGrandparent = dynamic_cast<VirtualModule*>(virtualParent.getOwner());
        if(virtualGrandparent == nullptr) {
          throw logic_error(std::string("Module ") + virtualParent.getName() +
              ": cannot have hierarchy modifier 'oneLevelUp' or 'oneUpAndHide' in root of the application." +
              "\nNon-virtual path to the offending module: " + getQualifiedName());
        }
        moduleToAddTo = &virtualGrandparent->createAndGetSubmodule(_name);
      }
      else {
        moduleToAddTo = &virtualParent.createAndGetSubmodule(_name);
      }
    }
    else {
      // We are hiding. Find the correct ancestor to add to:
      if(_hierarchyModifier == HierarchyModifier::oneUpAndHide) {
        // don't just hide but also move one level up -> add to the grandparent
        moduleToAddTo = dynamic_cast<VirtualModule*>(virtualParent.getOwner());
        if(moduleToAddTo == nullptr) {
          throw logic_error(std::string("Module ") + virtualParent.getName() +
              ": cannot have hierarchy modifier 'oneLevelUp' or 'oneUpAndHide' in root of the application." +
              "\nNon-virtual path to the offending module: " + getQualifiedName());
        }
      }
      else {
        // just hide -> add to the parent (could also an already existing object with the same hierarchy level as
        // virtualMe, where we just add to)
        moduleToAddTo = &virtualParent;
      }
    }

    // This should have already been caught by previous checks.
    assert(moduleToAddTo != nullptr);

    // add nodes to the module if matching the tag
    std::regex expr(tag);
    for(auto& node : getAccessorList()) {
      bool addNode = false;
      for(auto& nodeTag : node.getTags()) {
        if(std::regex_match(nodeTag, expr)) {
          addNode = true;
          break;
        }
      }
      if(node.getTags().size() == 0)
        if(std::regex_match("", expr)) addNode = true; // check if empty tag matches, if no tag applied to node
      if(negate) addNode = !addNode;
      if(addNode) moduleToAddTo->registerAccessor(node);
    }

    // iterate through submodules
    // nomencature: submodule = EntityOwner in C++ hierarchy, child = VirtualModule in virtual hierarcy
    for(auto submodule : getSubmoduleList()) {
      // Duplicate childs will not be created, since createAndGetSubmodule() prevents that.
      submodule->findTagAndAppendToModule(*moduleToAddTo, tag, eliminateAllHierarchies, false, negate, root);
    }
*/
  }

  /********************************************************************************************************************/

  bool EntityOwner::hasSubmodule(const std::string& name) const {
    for(auto submodule : getSubmoduleList()) {
      if(submodule->getName() == name) return true;
    }
    return false;
  }

  /********************************************************************************************************************/

  Module* EntityOwner::getSubmodule(const std::string& name) const {
    for(auto submodule : getSubmoduleList()) {
      if(submodule->getName() == name) return submodule;
    }
    throw ChimeraTK::logic_error("Submodule '" + name + "' not found in module '" + getName() + "'!");
  }

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

  VirtualModule EntityOwner::flatten() {
    VirtualModule nextmodule{_name, _description, getModuleType()};
    for(auto& node : getAccessorListRecursive()) {
      nextmodule.registerAccessor(node);
    }
    return nextmodule;
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
