#include "StatusAggregator.h"
#include "ControlSystemModule.h"

#include <list>
#include <regex>

namespace ChimeraTK {

  /********************************************************************************************************************/

  StatusAggregator::StatusAggregator(EntityOwner* owner, const std::string& name, const std::string& description,
      PriorityMode mode, const std::unordered_set<std::string>& tagsToAggregate,
      const std::unordered_set<std::string>& outputTags)
  : ApplicationModule(owner, "aggregator", description, HierarchyModifier::hideThis, outputTags), _output(this, name),
    _mode(mode), _tagsToAggregate(tagsToAggregate) {
    // add reserved tag tagAggregatedStatus to the status output, so it can be detected by other StatusAggregators
    _output.status.addTag(tagAggregatedStatus);

    // search the variable tree for StatusOutputs and create the matching inputs
    populateStatusInput();

    // check maximum size of tagsToAggregate
    if(tagsToAggregate.size() > 1) {
      throw ChimeraTK::logic_error("StatusAggregator: List of tagsToAggregate must contain at most one tag.");
    }
  }

  /********************************************************************************************************************/

  void StatusAggregator::populateStatusInput() {
    scanAndPopulateFromHierarchyLevel(*getOwner(), ".");

    // Check if no inputs are present (nothing found to aggregate)
    if(_inputs.empty()) {
      throw ChimeraTK::logic_error("StatusAggregator " + VariableNetworkNode(_output.status).getQualifiedName() +
          " has not found anything to aggregate.");
    }
  }

  /********************************************************************************************************************/

  void StatusAggregator::scanAndPopulateFromHierarchyLevel(EntityOwner& module, const std::string& namePrefix) {
    // Search for StatusOutputs to aggregate
    for(auto& node : module.getAccessorList()) {
      // Filter required tags
      // Note: findTag() cannot be used instead, since the search must be done on the original C++ hierarchy. Otherwise
      // it is not possible to identify which StatusOutputs are aggregated by other StatusAggregators.
      const auto& tags = node.getTags();
      if(tags.find(StatusOutput::tagStatusOutput) == tags.end()) {
        // StatusOutput's reserved tag not present: not a StatusOutput
        continue;
      }
      bool skip = false;
      for(const auto& tag : _tagsToAggregate) {
        // Each tag attached to this StatusAggregator must be present at all StatusOutputs to be aggregated
        if(tags.find(tag) == tags.end()) {
          skip = true;
          break;
        }
      }
      if(skip) continue;

      // All variables with the StatusOutput::tagStatusOutput need to be feeding, otherwise someone has used the tag
      // wrongly
      if(node.getDirection().dir != VariableDirection::feeding) {
        throw ChimeraTK::logic_error("BUG DETECTED: StatusOutput's reserved tag has been found on a consumer.");
      }

      // Create matching input for the found StatusOutput of the other StatusAggregator
      _inputs.emplace_back(this, node.getName(), "", std::unordered_set<std::string>{tagInternalVars});
      node >> _inputs.back();
    }

    // Search for StatusAggregators among submodules
    const auto& ml = module.getSubmoduleList();
    for(const auto& submodule : ml) {
      // do nothing, if it is *this
      if(submodule == this) continue;

      auto* aggregator = dynamic_cast<StatusAggregator*>(submodule);
      if(aggregator != nullptr) {
        // never aggregate on the same level
        if(aggregator->getOwner() == getOwner()) {
          continue;
        }

        // if another StatusAggregator has been found, check tags
        bool skip = false;
        const auto& tags = aggregator->_tagsToAggregate;
        for(const auto& tag : _tagsToAggregate) {
          // Each tag attached to this StatusAggregator must be present at all StatusOutputs to be aggregated
          if(tags.find(tag) == tags.end()) {
            skip = true;
            break;
          }
        }
        if(skip) continue;

        // aggregate the StatusAggregator's result
        auto node = VariableNetworkNode(aggregator->_output.status);
        _inputs.emplace_back(this, node.getName(), "", std::unordered_set<std::string>{tagInternalVars});
        node >> _inputs.back();
        return;
      }
    }

    // If no (matching) StatusAggregator is found, recurse into sub-modules
    for(const auto& submodule : ml) {
      if(dynamic_cast<StatusAggregator*>(submodule) != nullptr) continue; // StatusAggregators are already handled
      scanAndPopulateFromHierarchyLevel(*submodule, namePrefix + "/" + submodule->getName());
    }
  }

  /********************************************************************************************************************/

  int StatusAggregator::getPriority(StatusOutput::Status status) {
    using Status = StatusOutput::Status;

    const std::map<PriorityMode, std::map<Status, int32_t>> map_priorities{
        {PriorityMode::fwko, {{Status::OK, 1}, {Status::FAULT, 3}, {Status::OFF, 0}, {Status::WARNING, 2}}},
        {PriorityMode::fwok, {{Status::OK, 0}, {Status::FAULT, 3}, {Status::OFF, 1}, {Status::WARNING, 2}}},
        {PriorityMode::ofwk, {{Status::OK, 0}, {Status::FAULT, 2}, {Status::OFF, 3}, {Status::WARNING, 1}}},
        {PriorityMode::fw_warn_mixed, {{Status::OK, -1}, {Status::FAULT, 3}, {Status::OFF, -1}, {Status::WARNING, 2}}}};

    return map_priorities.at(_mode).at(status);
  }

  /********************************************************************************************************************/

  void StatusAggregator::mainLoop() {
    auto rag = readAnyGroup();
    DataValidity lastStatusValidity = DataValidity::ok;
    while(true) {
      // find highest priority status of all inputs
      StatusOutput::Status status;
      bool statusSet = false; // flag whether status has been set from an input already
      for(auto& input : _inputs) {
        auto prio = getPriority(input);
        if(!statusSet || prio > getPriority(status)) {
          status = input;
          statusSet = true;
        }
        else if(prio == -1) { //  -1 means, we need to warn about mixed values
          if(getPriority(status) == -1 && input != status) {
            status = StatusOutput::Status::WARNING;
          }
        }
      }
      assert(statusSet);

      // write status only if changed, but always write initial value out
      if(status != _output.status || _output.status.getVersionNumber() == VersionNumber{nullptr} ||
          getDataValidity() != lastStatusValidity) {
        _output.status = status;
        _output.status.write();
        lastStatusValidity = getDataValidity();
      }

      // wait for changed inputs
      rag.readAny();
    }
  }

  /********************************************************************************************************************/

  void StatusAggregator::findTagAndAppendToModule(VirtualModule& virtualParent, const std::string& tag,
      bool eliminateAllHierarchies, bool eliminateFirstHierarchy, bool negate, VirtualModule& root) const {
    // Change behaviour to exclude the auto-generated inputs which are connected to the data sources. Otherwise those
    // variables might get published twice to the control system, if findTag(".*") is used to connect the entire
    // application to the control system.
    // This is a temporary solution. In future, instead the inputs should be generated at the same place in the
    // hierarchy as the source variable, and the connetion should not be made by the module itself. This currently would
    // be complicated to implement, since it is difficult to find the correct virtual name for the variables.

    struct MyVirtualModule : VirtualModule {
      using VirtualModule::VirtualModule;
      using VirtualModule::findTagAndAppendToModule;
    };

    MyVirtualModule tempParent("tempRoot", "", ModuleType::ApplicationModule);
    MyVirtualModule tempRoot("tempRoot", "", ModuleType::ApplicationModule);
    EntityOwner::findTagAndAppendToModule(
        tempParent, tagInternalVars, eliminateAllHierarchies, eliminateFirstHierarchy, true, tempRoot);
    tempParent.findTagAndAppendToModule(virtualParent, tag, false, true, negate, root);
    tempRoot.findTagAndAppendToModule(root, tag, false, true, negate, root);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
