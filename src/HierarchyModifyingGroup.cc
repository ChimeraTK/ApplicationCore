// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "HierarchyModifyingGroup.h"

#include "ApplicationModule.h"
#include "VariableGroup.h"

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {

  HierarchyModifyingGroup::HierarchyModifyingGroup(VariableGroup* owner, std::string qualifiedName,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : VariableGroup(owner, "temporaryName", description, HierarchyModifier::none, tags) {
    // qualifiedName must not be empty
    if(qualifiedName.size() == 0) {
      throw ChimeraTK::logic_error("HierarchyModifyingGroup: qualifiedName must not be empty.");
    }

    // If path starts with slash, the entire tree is moved to root
    if(qualifiedName[0] == '/') {
      moveToRoot = true;
    }

    // Split path into pieces
    std::vector<std::string> splittedPath;
    boost::split(splittedPath, qualifiedName, boost::is_any_of("/"));

    // Clean splitted path by removing extra slashes and resolving internal . and .. elements where possible. As a
    // result, no "." element may occur, and ".." can only occur at the beginning.
    for(auto& pathElement : splittedPath) {
      if(pathElement.size() == 0) continue;
      if(pathElement == "." && splittedPath.size() != 1) continue;
      if(pathElement == ".." && _splittedPath.size() > 0 && _splittedPath.back() != "..") {
        _splittedPath.pop_back();
        continue;
      }
      if(pathElement == ".." && moveToRoot) {
        throw ChimeraTK::logic_error("QualifiedName of HierarchyModifyingGroup must not start with '/..'!");
      }
      _splittedPath.push_back(pathElement);
    }

    // Change name to last element of cleaned path
    _name = _splittedPath.back();
  }

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getUnqualifiedName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of("/");
    if(found == std::string::npos) return qualifiedName;
    return qualifiedName.substr(found + 1);
  }

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getPathName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of("/");
    if(found == std::string::npos) return ".";
    return qualifiedName.substr(0, found);
  }

  /********************************************************************************************************************/

  void HierarchyModifyingGroup::findTagAndAppendToModule(VirtualModule& virtualParent, const std::string& tag,
      bool eliminateAllHierarchies, bool eliminateFirstHierarchy, bool negate, VirtualModule& root) const {
    // the virtual parent to use depends on moveToRoot and will change while walking through the tree
    VirtualModule* currentVirtualParent = &virtualParent;
    if(moveToRoot) {
      currentVirtualParent = &root;
    }

    // if no (extra) hierarchies are wanted, delegate to original function
    if(eliminateAllHierarchies || _splittedPath.size() == 1 || (eliminateFirstHierarchy && _splittedPath.size() == 2)) {
      if(eliminateFirstHierarchy && _splittedPath.size() == 2) {
        // the first hierarchy is already eliminated by directly using the original function in this case
        eliminateFirstHierarchy = false;
      }
      if(_splittedPath.size() == 1 && _splittedPath.front() == "..") {
        assert(!moveToRoot);
        eliminateFirstHierarchy = true;
        currentVirtualParent = dynamic_cast<VirtualModule*>(currentVirtualParent->getOwner());
        assert(currentVirtualParent != nullptr);
      }
      if(_splittedPath.size() == 1 && _splittedPath.front() == ".") {
        assert(!moveToRoot);
        eliminateFirstHierarchy = true;
      }
      VariableGroup::findTagAndAppendToModule(
          *currentVirtualParent, tag, eliminateAllHierarchies, eliminateFirstHierarchy, negate, root);
      return;
    }

    // Create virtual ownership tree.
    // At this point, there is always at least one extra VirtualModule to be created.
    size_t index = 0;
    for(auto& pathElement : _splittedPath) {
      ++index;

      // Last element: create and fill through original base class implementation
      if(index == _splittedPath.size()) {
        VariableGroup::findTagAndAppendToModule(*currentVirtualParent, tag, false, false, negate, root);
        break;
      }

      // ".." element (only appearing at the beginning due to cleaning in constructor): move parent one level higher
      if(pathElement == "..") {
        currentVirtualParent = dynamic_cast<VirtualModule*>(currentVirtualParent->getOwner());
        assert(currentVirtualParent != nullptr);
        continue;
      }

      // Create VirtualModule
      currentVirtualParent = &(currentVirtualParent->createAndGetSubmodule(pathElement));
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
