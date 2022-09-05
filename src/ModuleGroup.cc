// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ModuleGroup.h"

#include "Application.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, hierarchyModifier, tags) {
    if(!dynamic_cast<Application*>(owner) && !dynamic_cast<ModuleGroup*>(owner)) {
      throw ChimeraTK::logic_error("ModuleGroups must be owned either by the "
                                   "Application, other ModuleGroups or be the Application!");
    }
  }
  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(const std::string& name) : ModuleImpl(nullptr, name, "") {}

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      bool eliminateHierarchy, const std::unordered_set<std::string>& tags)
  : ModuleGroup(
        owner, name, description, eliminateHierarchy ? HierarchyModifier::hideThis : HierarchyModifier::none, tags) {}

  /********************************************************************************************************************/

  ModuleGroup& ModuleGroup::operator=(ModuleGroup&& other) noexcept {
    ModuleImpl::operator=(std::move(other));
    return *this;
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
