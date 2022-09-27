// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VariableGroup.h"

#include "ApplicationModule.h"
#include "ModuleGroup.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(VariableGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : Module(owner, name, description, tags) {
    if(owner == nullptr) {
      throw ChimeraTK::logic_error("VariableGroups: owner cannot be nullptr!");
    }
    if(owner->getModel().isValid()) {
      _model = owner->getModel().add(*this);
    }
    else if(dynamic_cast<ApplicationModule*>(owner) && dynamic_cast<ApplicationModule*>(owner)->getModel().isValid()) {
      _model = dynamic_cast<ApplicationModule*>(owner)->getModel().add(*this);
    }
  }

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(VariableGroup* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : VariableGroup(owner, name, description, tags) {
    applyHierarchyModifierToName(hierarchyModifier);
  }

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : Module(owner, name, description, tags) {
    // Not registering with model - this constructor will only be used in the special case of Application owning an
    // ApplicationModule
  }

  /********************************************************************************************************************/

  VariableGroup& VariableGroup::operator=(VariableGroup&& other) noexcept {
    _model = std::move(other._model);
    other._model = {};
    if(_model.isValid()) _model.informMove(*this);
    Module::operator=(std::move(other));
    return *this;
  }

  /*********************************************************************************************************************/

  std::string VariableGroup::getVirtualQualifiedName() const {
    return _model.getFullyQualifiedPath();
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
