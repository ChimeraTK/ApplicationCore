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
    [[deprecated]] HierarchyModifyingGroup(VariableGroup* owner, const std::string& name,
        const std::string& description, const std::unordered_set<std::string>& tags = {})
    : VariableGroup(owner, name, description, tags) {}

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
    [[deprecated]] ModifyHierarchy(VariableGroup* owner, const std::string& qualifiedName, Types... args)
    : VariableGroup(owner, Utilities::getPathName(qualifiedName), ""),
      value(this, Utilities::getUnqualifiedName(qualifiedName), args...) {}

    [[deprecated]] ModifyHierarchy() = default;
    ACCESSOR value;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
