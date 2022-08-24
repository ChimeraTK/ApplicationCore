// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "StatusAggregator.h"

#include "ConfigReader.h"
#include "ControlSystemModule.h"
#include "DeviceModule.h"

#include <list>
#include <regex>

namespace ChimeraTK {

  /********************************************************************************************************************/

  StatusAggregator::StatusAggregator(EntityOwner* owner, const std::string& name, const std::string& description,
      PriorityMode mode, const std::unordered_set<std::string>& tagsToAggregate,
      const std::unordered_set<std::string>& outputTags)
  : ApplicationModule(owner, "aggregator", description, HierarchyModifier::hideThis, outputTags), _output(this, name),
    _mode(mode), _tagsToAggregate(tagsToAggregate) {
    // check maximum size of tagsToAggregate
    if(tagsToAggregate.size() > 1) {
      throw ChimeraTK::logic_error("StatusAggregator: List of tagsToAggregate must contain at most one tag.");
    }

    // add reserved tag tagAggregatedStatus to the status output, so it can be detected by other StatusAggregators
    _output._status.addTag(tagAggregatedStatus);

    // search the variable tree for StatusOutputs and create the matching inputs
    populateStatusInput();
  }

  /********************************************************************************************************************/

  void StatusAggregator::populateStatusInput() {
    scanAndPopulateFromHierarchyLevel(*getOwner(), ".");

    // Special treatment for DeviceModules, because they are formally not owned by the Application: If we are
    // aggregating all tags at the top-level of the Application, include the status outputs of all DeviceModules.
    if(getOwner() == &Application::getInstance() && _tagsToAggregate.empty()) {
      for(auto& p : Application::getInstance().deviceModuleMap) {
        scanAndPopulateFromHierarchyLevel(p.second->deviceError, ".");
      }
    }

    // Check if no inputs are present (nothing found to aggregate)
    if(_inputs.empty()) {
      throw ChimeraTK::logic_error("StatusAggregator " + VariableNetworkNode(_output._status).getQualifiedName() +
          " has not found anything to aggregate.");
    }
  }

  /********************************************************************************************************************/

  void StatusAggregator::scanAndPopulateFromHierarchyLevel(EntityOwner& module, const std::string& namePrefix) {
    // a map used just for optimization of node lookup by name, which happens in inner loop below
    std::map<std::string, VariableNetworkNode> nodesByName;
    for(auto& node : module.getAccessorList()) {
      nodesByName[node.getName()] = node;
    }
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
      // Unfortunately, the qualified name of the newly-created node is useless,
      // so we save the original one as description for indication in a message
      _inputs.emplace_back(this, node.getName(), node.getQualifiedName(), HierarchyModifier::hideThis,
          std::unordered_set<std::string>{tagInternalVars});
      node >> _inputs.back()._status;
      // look for matching status message output node
      auto result = nodesByName.find(node.getName() + "_message");
      if(result != nodesByName.end()) {
        // tell the StatusWithMessageInput that it should consider the message source, and connect it
        auto statusMsgNode = result->second;
        _inputs.back().setMessageSource();
        statusMsgNode >> _inputs.back()._message;
      }
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
        auto statusNode = VariableNetworkNode(aggregator->_output._status);
        _inputs.emplace_back(this, statusNode.getName(), "", HierarchyModifier::hideThis,
            std::unordered_set<std::string>{tagInternalVars});
        statusNode >> _inputs.back()._status;
        auto msgNode = VariableNetworkNode(aggregator->_output._message);
        _inputs.back().setMessageSource();
        msgNode >> _inputs.back()._message;
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

  int StatusAggregator::getPriority(StatusOutput::Status status) const {
    using Status = StatusOutput::Status;

    // static helps against initializing over and over again
    static const std::map<PriorityMode, std::map<Status, int32_t>> map_priorities{
        {PriorityMode::fwko, {{Status::OK, 1}, {Status::FAULT, 3}, {Status::OFF, 0}, {Status::WARNING, 2}}},
        {PriorityMode::fwok, {{Status::OK, 0}, {Status::FAULT, 3}, {Status::OFF, 1}, {Status::WARNING, 2}}},
        {PriorityMode::ofwk, {{Status::OK, 0}, {Status::FAULT, 2}, {Status::OFF, 3}, {Status::WARNING, 1}}},
        {PriorityMode::fw_warn_mixed, {{Status::OK, -1}, {Status::FAULT, 3}, {Status::OFF, -1}, {Status::WARNING, 2}}}};

    return map_priorities.at(_mode).at(status);
  }

  /********************************************************************************************************************/

  void StatusAggregator::mainLoop() {
    // set up inputsMap which gives StatusWithMessageInput for transferelementId of members
    std::map<TransferElementID, StatusWithMessageInput*> inputsMap;
    for(auto& x : _inputs) {
      inputsMap[x._status.getId()] = &x;
      if(x.hasMessageSource) inputsMap[x._message.getId()] = &x;
    }

    auto rag = readAnyGroup();
    DataValidity lastStatusValidity = DataValidity::ok;
    while(true) {
      // find highest priority status of all inputs
      StatusOutput::Status status;
      StatusWithMessageInput* statusOrigin = nullptr;
      // flag whether status has been set from an input already
      bool statusSet = false;
      // this stores getPriority(status) if statusSet=true
      // Intent is to reduce evaluation frequency of getPriority
      // the initial value provided here is only to prevent compiler warnings
      int statusPrio = 0;
      for(auto& inputPair : _inputs) {
        StatusPushInput& input = inputPair._status;
        auto prio = getPriority(input);
        if(!statusSet || prio > statusPrio) {
          status = input;
          statusOrigin = &inputPair;
          statusPrio = prio;
          statusSet = true;
        }
        else if(prio == -1) { //  -1 means, we need to warn about mixed values
          if(statusPrio == -1 && input != status) {
            status = StatusOutput::Status::WARNING;
            statusOrigin = nullptr;
            statusPrio = getPriority(status);
          }
        }
      }
      assert(statusSet);

      // write status only if changed, but always write initial value out
      if(status != _output._status || _output._status.getVersionNumber() == VersionNumber{nullptr} ||
          getDataValidity() != lastStatusValidity) {
        if(!statusOrigin) {
          // this can only happen if warning about mixed values
          assert(status == StatusOutput::Status::WARNING);
          _output.write(status, "warning - StatusAggregator inputs have mixed values");
        }
        else {
          if(status != StatusOutput::Status::OK) {
            // this either copies the message from corresponding string variable, or sets a generic message
            auto msg = statusOrigin->getMessage();
            _output.write(status, msg);
          }
          else {
            _output.writeOk();
          }
        }

        lastStatusValidity = getDataValidity();
      }

      // wait for changed inputs
    waitForChange:
      auto change = rag.readAny();
      auto f = inputsMap.find(change);
      if(f != inputsMap.end()) {
        auto varPair = f->second;
        if(!varPair->update(change)) goto waitForChange; // inputs not in consistent state yet
      }

      // handle request for debug info
      if(change == debug.value.getId()) {
        static std::mutex debugMutex; // all aggregators trigger at the same time => lock for clean output
        std::unique_lock<std::mutex> lk(debugMutex);

        std::cout << "StatusAggregtor " << getQualifiedName() << " debug info:" << std::endl;
        for(auto& inputPair : _inputs) {
          StatusPushInput& input = inputPair._status;
          std::cout << input.getName() << " = " << input;
          if(inputPair.hasMessageSource) {
            std::cout << inputPair._message.getName() << " = " << (std::string)inputPair._message;
          }
          std::cout << std::endl;
        }
        std::cout << "debug info finished." << std::endl;
        goto waitForChange;
      }
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
