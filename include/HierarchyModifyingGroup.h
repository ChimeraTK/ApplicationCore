// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "VariableGroup.h"

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   *  Deprecated class for backwards compatibility ownly. Use VariableGroup instead!
   */
  struct HierarchyModifyingGroup : VariableGroup {
    using VariableGroup::VariableGroup;

    /**
     *  Return the last component of the given qualified path name.
     *  Example: "/some/deep/hierarchy/levels" would return "levels"
     *
     *  This function is useful together with getPathName(), when a qualified variable name is given, and a
     *  HierarchyModifyingGroup with the variable inside needs to be created.
     */
    static std::string getUnqualifiedName(const std::string& qualifiedName);

    /**
     *  Return all but the last components of the given qualified name.
     *  Example: "/some/deep/hierarchy/levels" would return "/some/deep/hierarchy"
     *
     *  This function is useful together with getUnqualifiedName(), when a qualified variable name is given, and a
     *  HierarchyModifyingGroup with the variable inside needs to be created.
     */
    static std::string getPathName(const std::string& qualifiedName);
  };

  /********************************************************************************************************************/

  /**
   *  Convenience version of the HierarchyModifyingGroup with exactly one variable inside. The constructor takes the
   *  qualified name of the variable and splits it internally into the path name (for the HierarchyModifyingGroup) and
   *  the unqualified variable name.
   *
   *  The template argument must be one of the Scalar*Input, ScalarOutput classes resp. their Array counterparts.
   */
  template<typename ACCESSOR>
  struct ModifyHierarchy : HierarchyModifyingGroup {
    template<typename... Types>
    ModifyHierarchy(ApplicationModule* owner, const std::string& qualifiedName, Types... args)
    : HierarchyModifyingGroup(owner, HierarchyModifyingGroup::getPathName(qualifiedName), ""),
      value(this, HierarchyModifyingGroup::getUnqualifiedName(qualifiedName), args...) {}

    ModifyHierarchy() = default;
    ACCESSOR value;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
