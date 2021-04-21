#ifndef HIERARCHYMODIFYINGGROUP_H
#define HIERARCHYMODIFYINGGROUP_H

#include "VariableGroup.h"
#include "ApplicationModule.h"

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {

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

    void findTagAndAppendToModule(VirtualModule& virtualParent, const std::string& tag,
        bool eliminateAllHierarchies, bool eliminateFirstHierarchy, bool negate, VirtualModule& root) const override;

    bool moveToRoot{false};
    std::vector<std::string> _splittedPath;
  };

} // namespace ChimeraTK

#endif // HIERARCHYMODIFYINGGROUP_H
