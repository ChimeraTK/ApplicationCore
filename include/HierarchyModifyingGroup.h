// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Utilities.h"
#include "VariableGroup.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   *  Deprecated class for backwards compatibility ownly. Use VariableGroup instead!
   */
  struct HierarchyModifyingGroup : VariableGroup {
    [[deprecated("Use VariableGroup directly instead")]] HierarchyModifyingGroup(EntityOwner* owner,
        const std::string& name, const std::string& description, const std::unordered_set<std::string>& tags = {})
    : VariableGroup(dynamic_cast<VariableGroup*>(owner), name, description, tags) {}

    using VariableGroup::VariableGroup;

    [[deprecated]] static std::string getUnqualifiedName(const std::string& qualifiedName);

    [[deprecated]] static std::string getPathName(const std::string& qualifiedName);
  };

  /********************************************************************************************************************/

  /**
   *  Deprecated class for backwards compatibility ownly. Just provide qualified path to variables instead!
   */
  template<typename ACCESSOR>
  struct ModifyHierarchy : VariableGroup {
    template<typename... Types>
    [[deprecated("Use an accessor with a qualified name instead")]] ModifyHierarchy(
        VariableGroup* owner, const std::string& qualifiedName, Types... args)
    : VariableGroup(owner, Utilities::getPathName(qualifiedName), ""),
      value(this, Utilities::getUnqualifiedName(qualifiedName), args...) {}

    [[deprecated]] ModifyHierarchy() = default;
    ACCESSOR value;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
