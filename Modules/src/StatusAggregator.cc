#include "StatusAggregator.h"

#include <list>
#include <regex>

namespace ChimeraTK {

  /********************************************************************************************************************/

  StatusAggregator::StatusAggregator(EntityOwner* owner, const std::string& name, const std::string& description,
      const std::string& output, HierarchyModifier modifier, const std::unordered_set<std::string>& tags)
  : ApplicationModule(owner, name, description, modifier, tags), status(this, output, "", "", {}) {
    populateStatusInput();
  }

  /********************************************************************************************************************/

  void StatusAggregator::populateStatusInput() {
    std::cout << "Populating aggregator " << getName() << ", fully qualified name is: " << getQualifiedName()
              << std::endl;

    // Search for the StatusOutput's reserved tag
    VirtualModule searchBase = getOwner()->findTag(StatusOutput::tagStatusOutput);

    // Search also for all tags the StatusAggregator has. The result is an AND condition of all tags, which is applied
    // as a requirement for all aggregated StatusOutputs.
    searchBase = searchBase.findTag(StatusOutput::tagStatusOutput);
    scanAndPopulateFromHierarchyLevel(searchBase, true);

    std::cout << std::endl << std::endl;
  }

  /********************************************************************************************************************/

  void StatusAggregator::scanAndPopulateFromHierarchyLevel(const Module& virtualModule, bool firstLevel) {
    bool statusAggregatorFound = false;

    // First, search for other StatusAggregators
    for(const auto& node : virtualModule.getAccessorList()) {
      // All variables with the StatusOutput::tagStatusOutput need to be feeding, otherwise someone has used the tag
      // wrongly
      if(node.getDirection().dir != VariableDirection::feeding) {
        throw ChimeraTK::logic_error("BUG DETECTED: StatusOutput's reserved tag has been found on a consumer.");
      }

      // Check whether another StatusAggregator has been found
      auto tags = node.getTags();
      if(tags.find(tagAggregatedStatus) != tags.end()) {
        if(statusAggregatorFound) {
          throw ChimeraTK::logic_error("StatusAggregator: Colliding instances found.");
        }
        statusAggregatorFound = true;
      }
      if(!statusAggregatorFound) continue;

      // Make sure no other StatusAggregator is found on the same level as *this
      if(firstLevel) {
        if(node.getOwningModule() != this) {
          throw ChimeraTK::logic_error("StatusAggregator: Colliding instance found on the same hierarchy level.");
        }

        // do not include our own aggregated output
        statusAggregatorFound = false;
        continue;
      }

      // Create matching input for the found StatusOutput of the other StatusAggregator
      std::cout << "Other StatusAggregator found: " << node.getQualifiedName() << std::endl;
      inputs.emplace_back(this, node.getQualifiedName());
    }

    // If another StatusAggregator has been found, do not recurse further down this branch
    if(statusAggregatorFound) {
      return;
    }

    // No other StatusAggregator found: add all StatusOutputs
    for(const auto& node : virtualModule.getAccessorList()) {
      // Create matching input for the found StatusOutput
      std::cout << "StatusOutput found: " << node.getQualifiedName() << std::endl;
      inputs.emplace_back(this, node.getQualifiedName());
    }

    // ... and recurse into sub-modules
    for(const auto& submodule : virtualModule.getSubmoduleList()) {
      scanAndPopulateFromHierarchyLevel(*submodule, false);
    }
  }

  /********************************************************************************************************************/

  void StatusAggregator::mainLoop() {}

  /********************************************************************************************************************/

} // namespace ChimeraTK
