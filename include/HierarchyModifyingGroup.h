// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "VariableGroup.h"

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   *  A HierarchyModifyingGroup is a VariableGroup which can easily appear at a different place in the hierarchy by
   *  specifying a qualified name. The qualifiedName can contain multiple hierarchy levels separated by slashes "/".
   *  It can also start with a "/" to move the tree to the root of the application, or it can start with "../"
   *  (also repeated times) to move it one or more levels up.
   *
   *  If the qualifiedName is just a plain name without slashes, the HierarchyModifyingGroup behaves identical to a
   *  VariableGroup.
   */
  struct HierarchyModifyingGroup : VariableGroup {
    HierarchyModifyingGroup(EntityOwner* owner, std::string qualifiedName, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    HierarchyModifyingGroup() {}

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

   protected:
    void findTagAndAppendToModule(VirtualModule& virtualParent, const std::string& tag, bool eliminateAllHierarchies,
        bool eliminateFirstHierarchy, bool negate, VirtualModule& root) const override;

    bool moveToRoot{false};
    std::vector<std::string> _splittedPath;
  };

  /********************************************************************************************************************/

  /**
   *  Convenience version of the HierarchyModifyingGroup with exactly one variable inside. The constructor takes the
   *  qualified name of the variable and splits it internally into the path name (for the HierarchyModifyingGroup) and
   *  the unqialified variable name.
   *
   *  The template argument must be one of the Scalar*Input, ScalarOutput classes resp. their Array counterparts.
   */
  template<typename ACCESSOR>
  struct ModifyHierarchy : HierarchyModifyingGroup {
    template<typename... Types>
    ModifyHierarchy(Module* owner, const std::string& qualifiedName, Types... args)
    : HierarchyModifyingGroup(owner, HierarchyModifyingGroup::getPathName(qualifiedName), ""),
      value(this, HierarchyModifyingGroup::getUnqualifiedName(qualifiedName), args...) {}

    ACCESSOR value;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
