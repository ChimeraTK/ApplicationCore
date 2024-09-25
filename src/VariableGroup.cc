// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VariableGroup.h"

#include "ApplicationModule.h"
#include "ModuleGroup.h"
#include "Utilities.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(VariableGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : Module(owner, ChimeraTK::Utilities::stripTrailingSlashes(name), description, tags) {
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
  : VariableGroup(owner,
        applyHierarchyModifierToName(ChimeraTK::Utilities::stripTrailingSlashes(name), hierarchyModifier), description,
        tags) {}

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : VariableGroup(dynamic_cast<VariableGroup*>(owner),
        applyHierarchyModifierToName(ChimeraTK::Utilities::stripTrailingSlashes(name), hierarchyModifier), description,
        tags) {}

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : Module(owner, ChimeraTK::Utilities::stripTrailingSlashes(name), description, tags) {
    // Not registering with model - this constructor will only be used in the special case of Application owning an
    // ApplicationModule
  }

  /********************************************************************************************************************/

  VariableGroup& VariableGroup::operator=(VariableGroup&& other) noexcept {
    // Keep the model as is (except from updating the pointers to the C++ objects). To do so, we have to hide it from
    // unregisterModule() which is executed in Module::operator=(), because it would destroy the model.
    ChimeraTK::Model::VariableGroupProxy model = std::move(other._model);
    other._model = {};

    Module::operator=(std::move(other));

    if(model.isValid()) {
      model.informMove(*this);
    }
    _model = model;

    return *this;
  }

  /*********************************************************************************************************************/

  std::string VariableGroup::getVirtualQualifiedName() const {
    return _model.getFullyQualifiedPath();
  }

  /********************************************************************************************************************/

  void VariableGroup::unregisterModule(Module* module) {
    EntityOwner::unregisterModule(module);
    if(_model.isValid()) {
      auto* vg = dynamic_cast<VariableGroup*>(module);
      if(!vg) {
        // during destruction unregisterModule is called from the base class destructor where the dynamic_cast already
        // fails.
        return;
      }
      _model.remove(*vg);
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
