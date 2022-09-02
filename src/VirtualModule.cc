// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VirtualModule.h"

#include "Application.h"
#include "Module.h"

#include <ChimeraTK/TransferElement.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  VirtualModule::VirtualModule(const std::string& name, const std::string& description, ModuleType moduleType)
  : Module(nullptr, name, description), _moduleType(moduleType) {
    if(name.find_first_of("/") != std::string::npos) {
      throw ChimeraTK::logic_error("Module names must not contain slashes: '" + name + "'.");
    }
  }

  /********************************************************************************************************************/

  VirtualModule::VirtualModule(const VirtualModule& other) : Module(nullptr, other.getName(), other.getDescription()) {
    // since moduleList stores plain pointers, we need to regenerate this list
    for(auto& mod : other.submodules) addSubModule(mod); // this creates a copy (call by value)
    accessorList = other.accessorList;
    _moduleType = other.getModuleType();
  }

  /********************************************************************************************************************/

  VirtualModule::~VirtualModule() {
    // do not unregister owner in Module destructor
    _owner = nullptr;
  }

  /********************************************************************************************************************/

  VirtualModule& VirtualModule::operator=(const VirtualModule& other) {
    // move-assign a plain new module
    Module::operator=(VirtualModule(other.getName(), other.getDescription(), other.getModuleType()));
    // since moduleList stores plain pointers, we need to regenerate this list
    for(auto& mod : other.submodules) addSubModule(mod); // this creates a copy (call by value)
    accessorList = other.accessorList;
    return *this;
  }

  /********************************************************************************************************************/

  VariableNetworkNode VirtualModule::operator()(const std::string& variableName) const {
    for(auto& variable : getAccessorList()) {
      if(variable.getName() == variableName) return VariableNetworkNode(variable);
    }
    throw ChimeraTK::logic_error("Variable '" + variableName + "' is not part of the variable group '" + _name + "'.");
  }

  /********************************************************************************************************************/

  Module& VirtualModule::operator[](const std::string& moduleName) const {
    for(auto submodule : getSubmoduleList()) {
      if(submodule->getName() == moduleName) return *submodule;
    }
    throw ChimeraTK::logic_error("Sub-module '" + moduleName + "' is not part of the variable group '" + _name + "'.");
  }

  /********************************************************************************************************************/

  void VirtualModule::connectTo(const Module& target, VariableNetworkNode trigger) const {
    // connect all direct variables of this module to their counter-parts in the
    // right-hand-side module
    for(auto& variable : getAccessorList()) {
      if(variable.getDirection().dir == VariableDirection::feeding) {
        // use trigger?
        if(trigger != VariableNetworkNode() && target(variable.getName()).getMode() == UpdateMode::push &&
            variable.getMode() == UpdateMode::poll) {
          variable[trigger] >> target(variable.getName());
        }
        else {
          variable >> target(variable.getName());
        }
      }
      else {
        // use trigger?
        if(trigger != VariableNetworkNode() && target(variable.getName()).getMode() == UpdateMode::poll &&
            variable.getMode() == UpdateMode::push) {
          target(variable.getName())[trigger] >> variable;
        }
        else {
          target(variable.getName()) >> variable;
        }
      }
    }

    // connect all sub-modules to their couter-parts in the right-hand-side module
    for(auto submodule : getSubmoduleList()) {
      submodule->connectTo(target[submodule->getName()], trigger);
    }
  }

  /********************************************************************************************************************/

  void VirtualModule::addAccessor(VariableNetworkNode accessor) {
    accessorList.push_back(accessor);
  }

  /********************************************************************************************************************/

  void VirtualModule::addSubModule(VirtualModule module) {
    if(!hasSubmodule(module.getName())) {
      // Submodule doesn'st exist already: register the given module as a new submodule
      submodules.push_back(module);
      registerModule(&(submodules.back()));
      submodules.back()._owner = this;
    }
    else {
      // Submodule does exist already: copy content into the existing submodule
      VirtualModule theSubmodule = dynamic_cast<VirtualModule&>(submodule(module.getName()));
      for(auto& submod : module.getSubmoduleList()) {
        theSubmodule.addSubModule(dynamic_cast<VirtualModule&>(*submod));
      }
      for(auto& acc : module.getAccessorList()) {
        theSubmodule.addAccessor(acc);
      }
    }
  }

  /********************************************************************************************************************/

  void VirtualModule::removeSubModule(const std::string& name) {
    for(auto module = submodules.begin(); module != submodules.end(); ++module) {
      if(module->getName() == name) {
        unregisterModule(&*module);
        submodules.erase(module);
        break;
      }
    }
  }

  /********************************************************************************************************************/

  const Module& VirtualModule::virtualise() const {
    return *this;
  }

  /********************************************************************************************************************/

  VirtualModule& VirtualModule::createAndGetSubmodule(const RegisterPath& moduleName) {
    for(auto& sm : submodules) {
      if(moduleName == sm.getName()) return sm;
    }
    addSubModule(VirtualModule(std::string(moduleName).substr(1), getDescription(), getModuleType()));
    return submodules.back();
  }

  /********************************************************************************************************************/

  VirtualModule& VirtualModule::createAndGetSubmoduleRecursive(const RegisterPath& moduleName) {
    if(moduleName == "") return *this;
    auto slash = std::string(moduleName).find_first_of("/", 1);
    if(slash == std::string::npos) {
      return createAndGetSubmodule(moduleName);
    }
    else {
      auto firstSubmodule = std::string(moduleName).substr(0, slash);
      auto remainingSubmodules = std::string(moduleName).substr(slash + 1);
      return createAndGetSubmodule(firstSubmodule).createAndGetSubmoduleRecursive(remainingSubmodules);
    }
  }

  /********************************************************************************************************************/

  void VirtualModule::stripEmptyChildsRecursive() {
    // first recurse into childs, to make sure to remove all we an
    for(auto& child : submodules) {
      child.stripEmptyChildsRecursive();
    }

    // strip empty virtual childs
    // Note: getSubmoduleList() returns a copy of the list, hence it is ok to call removeSubModule() in the loop here!
    for(auto& child : getSubmoduleList()) {
      if(child->getAccessorList().size() == 0 && child->getSubmoduleList().size() == 0) {
        removeSubModule(child->getName());
      }
    }
  }

  /********************************************************************************************************************/

  std::string VirtualModule::getVirtualQualifiedName() const {
    /// @todo FIXME change implementation to use model instead!
    std::string qualifiedName = "/" + getName();
    auto mod = dynamic_cast<Module*>(_owner);
    while(mod != nullptr) {
      qualifiedName = "/" + mod->getName() + qualifiedName;
      mod = dynamic_cast<Module*>(mod->getOwner());
    }
    return qualifiedName;
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
