// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ModuleGroup.h"

#include "Application.h"

namespace ChimeraTK {

  ModuleGroup::ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, hierarchyModifier, tags) {
    if(!dynamic_cast<Application*>(owner) && !dynamic_cast<ModuleGroup*>(owner)) {
      throw ChimeraTK::logic_error("ModuleGroups must be owned either by the "
                                   "Application or other ModuleGroups!");
    }
  }

  ModuleGroup::ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      bool eliminateHierarchy, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, eliminateHierarchy, tags) {
    if(!dynamic_cast<Application*>(owner) && !dynamic_cast<ModuleGroup*>(owner)) {
      throw ChimeraTK::logic_error("ModuleGroups must be owned either by the "
                                   "Application or other ModuleGroups!");
    }
  }

} /* namespace ChimeraTK */
