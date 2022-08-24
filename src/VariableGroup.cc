// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VariableGroup.h"

#include "ApplicationModule.h"
#include "DeviceModule.h"

namespace ChimeraTK {

  VariableGroup::VariableGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, hierarchyModifier, tags) {
    if(!dynamic_cast<ApplicationModule*>(owner) && !dynamic_cast<DeviceModule*>(owner) &&
        !dynamic_cast<VariableGroup*>(owner)) {
      throw ChimeraTK::logic_error("VariableGroups must be owned by ApplicationModule, DeviceModule or "
                                   "other VariableGroups!");
    }
  }

  VariableGroup::VariableGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      bool eliminateHierarchy, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, eliminateHierarchy, tags) {
    if(!dynamic_cast<ApplicationModule*>(owner) && !dynamic_cast<DeviceModule*>(owner) &&
        !dynamic_cast<VariableGroup*>(owner)) {
      throw ChimeraTK::logic_error("VariableGroups must be owned by ApplicationModule, DeviceModule or "
                                   "other VariableGroups!");
    }
  }

} /* namespace ChimeraTK */
