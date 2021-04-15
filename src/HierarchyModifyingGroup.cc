#include "HierarchyModifyingGroup.h"

#include "VariableGroup.h"
#include "ApplicationModule.h"

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {

  HierarchyModifyingGroup::HierarchyModifyingGroup(EntityOwner* owner, std::string qualifiedName,
    const std::string& description, const std::unordered_set<std::string>& tags)
  : VariableGroup() // must not use the standard constructor here and instead instead things on our own
  {
    if(!dynamic_cast<ApplicationModule*>(owner) && !dynamic_cast<VariableGroup*>(owner)) {
      throw ChimeraTK::logic_error("HierarchyModifyingGroup must be owned by ApplicationModule or VariableGroups!");
    }

    // special treatment for extra slashes at the end: remove them to avoid extra empty-named hierarchies
    while(qualifiedName.substr(qualifiedName.size()-1) == "/") {
      qualifiedName = qualifiedName.substr(0, qualifiedName.size()-1);
    }

    _name = getUnqualifiedName(qualifiedName);
    _description = description;
    _tags = tags;
    _owner = makeOwnerTree(owner, qualifiedName);
    _owner->registerModule(this);
  }

  /********************************************************************************************************************/

  HierarchyModifyingGroup::~HierarchyModifyingGroup() {
    // need to destroy _ownerTree in reverse order
    while(_ownerTree.size() > 0) _ownerTree.pop_back();

    // our owner has been destroyed already, no need to unregister (see Module destructor)
    _owner = nullptr;
  }

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getUnqualifiedName(const std::string &qualifiedName) {
    auto found = qualifiedName.find_last_of("/");
    if(found == std::string::npos) return qualifiedName;
    return qualifiedName.substr(found+1);
  }

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getPathName(const std::string &qualifiedName) {
    auto found = qualifiedName.find_last_of("/");
    if(found == std::string::npos) return "";
    return qualifiedName.substr(0,found);
  }

  /********************************************************************************************************************/

  EntityOwner* HierarchyModifyingGroup::makeOwnerTree(EntityOwner *originalOwner, const std::string &qualifiedName) {
    std::vector<std::string> splittedPath;
    boost::split(splittedPath, qualifiedName, boost::is_any_of("/"));
    bool moveToRoot = false;
    size_t index = 0;
    for(auto &pathElement : splittedPath) {
      ++index;
      if(pathElement.size() == 0) {
        // If first element is an empty string, the qualifiedName starts with a slash, which means the entire tree
        // shall be moved to the root.
        if(_ownerTree.size() == 0) {
          // the next path element needs to be moved to root
          moveToRoot = true;
        }
        // Any empty strings should not create a VariableGroup. Multiple consecutive slashes are treated as single.
        continue;
      }

      // Do not create a group for the last element - this one is being represented directly by the
      // HierarchyModifyingGroup
      if(index == splittedPath.size()) {
        break;
      }

      // Create variable group
      EntityOwner *theOwner;
      if(_ownerTree.size() == 0) {
        // The first group must be owned by the original owner passed to the contructor of the
        // HierarchyModifyingGroup
        theOwner = originalOwner;
      }
      else {
        // Following groups are owned by the previous level
        theOwner = &_ownerTree.back();
      }
      auto modifier = moveToRoot ? HierarchyModifier::moveToRoot : HierarchyModifier::none;
      if(pathElement == "..") {
        if(moveToRoot) {
          throw ChimeraTK::logic_error("QualifiedName of HierarchyModifyingGroup must not start with '/../'!");
        }
        modifier = HierarchyModifier::oneUpAndHide;
      }
      _ownerTree.emplace_back(theOwner, pathElement, "", modifier);
      moveToRoot = false;
    }

    // If the last element is "..", the hierarchy modifier of the group itself needs to be changed to oneUpAndHide
    if(splittedPath.back() == "..") {
      assert(_hierarchyModifier == HierarchyModifier::none);
      if(moveToRoot) {
        throw ChimeraTK::logic_error("QualifiedName of HierarchyModifyingGroup must not be '/..'!");
      }
      _hierarchyModifier = HierarchyModifier::oneUpAndHide;
    }

    // Special case: the qualifiedName is a single hierarchy element only. No additional groups have been created,
    // and the resulting HierarchyModifyingGroup is identical to a plain VariableGroup.
    if(_ownerTree.size() == 0) {
      if(moveToRoot) {
        // Only one path element which needs to be moved to root? Move the HierachyModifyingGroup itelf to root.
        assert(_hierarchyModifier == HierarchyModifier::none);
        _hierarchyModifier = HierarchyModifier::moveToRoot;
      }
      return originalOwner;
    }

    // Otherwise return the last created group as owner for the HierarchyModifyingGroup itself.
    return &_ownerTree.back();
  }

}
