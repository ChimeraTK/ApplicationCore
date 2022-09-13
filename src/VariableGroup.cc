// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VariableGroup.h"

#include "Application.h"
#include "ModuleGroup.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(VariableGroup* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, hierarchyModifier, tags) {
    if(owner == nullptr) {
      throw ChimeraTK::logic_error("VariableGroups must be owned by ApplicationModule, DeviceModule or "
                                   "other VariableGroups!");
    }
    if(owner->getModel().isValid()) _model = owner->getModel().add(*this);
  }

  /********************************************************************************************************************/

  VariableGroup::VariableGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, hierarchyModifier, tags) {
    // Not registering with model - this constructor will only be used in the special case of Application owning an
    // ApplicationModule
  }
                                   "other VariableGroups!");
    }
  }

  /********************************************************************************************************************/

  VariableGroup& VariableGroup::operator=(VariableGroup&& other) noexcept {
    ModuleImpl::operator=(std::move(other));
    return *this;
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
