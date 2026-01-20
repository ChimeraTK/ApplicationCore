// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "StatusAggregator.h"

#include <ChimeraTK/SystemTags.h>

#include <list>
#include <regex>
#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  StatusAggregator::StatusAggregator(ModuleGroup* owner, const std::string& name, const std::string& description,
      PriorityMode mode, std::unordered_set<std::string> tagsToAggregate,
      const std::unordered_set<std::string>& outputTags, std::string warnMixedMessage)
  : ApplicationModule(owner, ".", description, outputTags), _output(this, name), _mode(mode),
    _tagsToAggregate(std::move(tagsToAggregate)), _warnMixedMessage(std::move(warnMixedMessage)) {
    // Check that size of tagsToAggregate is 1.
    // There is a design decision pending whether multiple tags should be logical AND or logical OR (#13256).
    if(_tagsToAggregate.size() > 1) {
      throw ChimeraTK::logic_error(
          "StatusAggregator: List of tagsToAggregate is currently limited to one tag (see #13256).");
    }
    // add reserved tag tagAggregatedStatus to the status output, so it can be detected by other StatusAggregators
    _output._status.addTag(tagAggregatedStatus);
    // search the variable tree for StatusOutputs and create the matching inputs
    populateStatusInput();
  }

  /********************************************************************************************************************/

  void StatusAggregator::populateStatusInput() {
    auto model = dynamic_cast<ModuleGroup*>(_owner)->getModel();

    // set of potential inputs for this StatusAggregator instance
    std::set<std::string> inputPathsSet;
    // set of inputs for other, already created, StatusAggregator instances
    std::set<std::string> anotherStatusAgregatorInputSet;
    // map which assigns fully qualified path of StatusAggregator output to the ully qualified path StatusAggregator output message
    std::map<std::string, std::string> statusToMessagePathsMap;

    auto scanModel = [&](auto proxy) {
      if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
        auto* staAggPtr = dynamic_cast<StatusAggregator*>(&proxy.getApplicationModule());
        if(staAggPtr != nullptr) {
          if(staAggPtr == this) {
            return;
          }
          // ATTENTION. This loop is implementing a logical AND.
          // There is a design decision pending whether this is the wanted behaviour (#13256).
          // The constructor is currently limiting to one tag, so existing logic will not break.
          for(const auto& tagToAgregate : _tagsToAggregate) {
            // Each tag attached to this StatusAggregator must be present at all StatusOutputs to be aggregated
            auto outputNode = VariableNetworkNode(staAggPtr->_output._status);
            if(outputNode.getTags().find(tagToAgregate) == outputNode.getTags().end()) {
              return;
            }
          }

          inputPathsSet.insert(staAggPtr->_output._status.getModel().getFullyQualifiedPath());

          statusToMessagePathsMap[staAggPtr->_output._status.getModel().getFullyQualifiedPath()] =
              staAggPtr->_output._message.getModel().getFullyQualifiedPath();

          for(auto& anotherStatusAgregatorInput : staAggPtr->_inputs) {
            anotherStatusAgregatorInputSet.insert(
                anotherStatusAgregatorInput._status.getModel().getFullyQualifiedPath());
          }
        }
      }

      if constexpr(ChimeraTK::Model::isVariable(proxy)) {
        // check whether its not output of this (current) StatusAggregator. 'Current' StatusAggregator output is also
        // visible in the scanned model and should be ignored
        if(proxy.getFullyQualifiedPath() == _output._status.getModel().getFullyQualifiedPath()) {
          return;
        }

        auto tags = proxy.getTags();

        // find another aggregator output - this is already covered by checking if given module is a StatusAggregator
        if(tags.find(StatusAggregator::tagAggregatedStatus) != tags.end()) {
          return;
        }

        // find status output - this is potential candidate to be aggregated
        if(tags.find(ChimeraTK::SystemTags::statusOutput) != tags.end()) {
          for(const auto& tagToAgregate : _tagsToAggregate) {
            // ATTENTION. This loop is implementing a logical AND.
            // There is a design decision pending whether this is the wanted behaviour (#13256).
            // The constructor is currently limiting to one tag, so existing logic will not break.
            //
            // Each tag attached to this StatusAggregator must be present at all StatusOutputs to be aggregated
            if(tags.find(tagToAgregate) == tags.end()) {
              return;
            }
          }
          inputPathsSet.insert(proxy.getFullyQualifiedPath());

          // check for presence of message
          std::string fqn = proxy.getFullyQualifiedPath();
          if(tags.find(StatusWithMessage::tagStatusHasMessage) != tags.end()) {
            statusToMessagePathsMap[proxy.getFullyQualifiedPath()] = fqn + "_message";
          }
        }
      }
    };

    model.visit(scanModel, ChimeraTK::Model::keepApplicationModules || ChimeraTK::Model::keepProcessVariables,
        ChimeraTK::Model::breadthFirstSearch, ChimeraTK::Model::keepOwnership);

    for(const auto& pathToBeRemoved : anotherStatusAgregatorInputSet) {
      inputPathsSet.erase(pathToBeRemoved);
    }

    for(const auto& pathToBeAggregated : inputPathsSet) {
      _inputs.emplace_back(
          this, pathToBeAggregated, pathToBeAggregated, std::unordered_set<std::string>{tagInternalVars});
      if(!statusToMessagePathsMap[pathToBeAggregated].empty()) {
        _inputs.back().setMessageSource(statusToMessagePathsMap[pathToBeAggregated]);
      }
    }
  }

  int StatusAggregator::getPriority(StatusOutput::Status status) const {
    using Status = StatusOutput::Status;

    // Attention! priority_table must follow the order of PriorityMode and Status values!
    static_assert(int(PriorityMode::fwok) == 0);
    static_assert(int(PriorityMode::fwko) == 1);
    static_assert(int(PriorityMode::fw_warn_mixed) == 2);
    static_assert(int(PriorityMode::ofwk) == 3);

    static_assert(int(Status::OK) == 0);
    static_assert(int(Status::FAULT) == 1);
    static_assert(int(Status::OFF) == 2);
    static_assert(int(Status::WARNING) == 3);

    constexpr auto priority_table = std::array{
        // PriorityMode::fwok
        std::array<int32_t, 4>{
            /* OK */ 0,
            /* FAULT */ 3,
            /* OFF */ 1,
            /* WARNING */ 2,
        },
        // PriorityMode::fwko
        std::array<int32_t, 4>{
            /* OK */ 1,
            /* FAULT */ 3,
            /* OFF */ 0,
            /* WARNING */ 2,
        },
        // PriorityMode::fw_warn_mixed
        std::array<int32_t, 4>{
            /* OK */ -1,
            /* FAULT */ 3,
            /* OFF */ -1,
            /* WARNING */ 2,
        },
        // PriorityMode::ofwk
        std::array<int32_t, 4>{
            /* OK */ 0,
            /* FAULT */ 2,
            /* OFF */ 3,
            /* WARNING */ 1,
        },
    };

    return priority_table[int(_mode)][int(status)];

    /*
      For reference, we keep the old code here with an std::map. This creates issues since the static instance may go
      away too early in the shutdown phase, resulting in a use-after-free. Once we have C++26 fully supported, we should
      be able to use a constexpr std::map.

      static const std::map<PriorityMode, std::map<Status, int32_t>> map_priorities{
        {PriorityMode::fwok, {{Status::OK, 0}, {Status::FAULT, 3}, {Status::OFF, 1}, {Status::WARNING, 2}}},
        {PriorityMode::fwko, {{Status::OK, 1}, {Status::FAULT, 3}, {Status::OFF, 0}, {Status::WARNING, 2}}},
        {PriorityMode::fw_warn_mixed, {{Status::OK, -1}, {Status::FAULT, 3}, {Status::OFF, -1}, {Status::WARNING, 2}}},
        {PriorityMode::ofwk, {{Status::OK, 0}, {Status::FAULT, 2}, {Status::OFF, 3}, {Status::WARNING, 1}}},
      };
      return map_priorities.at(_mode).at(status);
    */
  }

  /********************************************************************************************************************/

  void StatusAggregator::mainLoop() {
    // set up inputsMap which gives StatusWithMessageInput for transferelementId of members
    std::map<TransferElementID, StatusWithMessageInput*> inputsMap;
    for(auto& x : _inputs) {
      inputsMap[x._status.getId()] = &x;
      if(x.hasMessageSource) {
        inputsMap[x._message.getId()] = &x;
      }
    }

    auto rag = readAnyGroup();
    while(true) {
      // find highest priority status of all inputs
      StatusOutput::Status status{StatusOutput::Status::FAULT}; // initialised just to prevent warning

      // store the input at which the highest-priority status was found ("selected input"), so we can access the
      // corresponding message
      StatusWithMessageInput* statusOrigin = nullptr;

      // cache priority of the current "selected" status input
      int statusPrio = 0; // initialised just to prevent warning

      // flag whether status has been set from an input already
      bool statusSet = false;

      for(auto& inputPair : _inputs) {
        StatusPushInput& input = inputPair._status;
        auto prio = getPriority(input);

        // Select the input if:
        // - no input has been selected so far, or
        // - the priority of the value is higher than the previously selected one, or
        // - the priority is the same but the input has a lower version number than the previously selected one.
        if(!statusSet || prio > statusPrio ||
            (input == status && statusOrigin != nullptr &&
                input.getVersionNumber() < statusOrigin->_status.getVersionNumber())) {
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

      // some input must be selected due to the logic
      assert(statusSet);

      // write status output with message from selected input. The output is written only if the output's value has
      // changed (writeIfDifferent).
      if(!statusOrigin) {
        // this can only happen if warning about mixed values
        assert(status == StatusOutput::Status::WARNING);
        _output.writeIfDifferent(status, _warnMixedMessage);
      }
      else {
        if(status != StatusOutput::Status::OK) {
          // this either copies the message from corresponding string variable, or sets a generic message
          auto msg = statusOrigin->getMessage();
          _output.writeIfDifferent(status, msg);
        }
        else {
          _output.writeOkIfDifferent();
        }
      }

      // wait for changed inputs
    waitForChange:
      auto change = rag.readAny();
      auto f = inputsMap.find(change);
      if(f != inputsMap.end()) {
        auto* varPair = f->second;
        if(!varPair->update(change)) {
          goto waitForChange; // inputs not in consistent state yet
        }
      }

      // handle request for debug info
      if(change == _debug.getId()) {
        auto myLog = logger(Logger::Severity::info);
        myLog << "StatusAggregtor (";
        switch(_mode) {
          case PriorityMode::fwok:
            myLog << "fwok";
            break;
          case PriorityMode::fwko:
            myLog << "fwko";
            break;
          case PriorityMode::fw_warn_mixed:
            myLog << "fw_warn_mixed";
            break;
          case PriorityMode::ofwk:
            myLog << "ofwk";
            break;
        };
        myLog << ") " << static_cast<VariableNetworkNode>(_output._status).getQualifiedName()
              << " debug info:" << std::endl;
        for(auto& inputPair : _inputs) {
          StatusPushInput& input = inputPair._status;
          myLog << static_cast<VariableNetworkNode>(input).getQualifiedName() << " = " << input;
          if(inputPair.hasMessageSource) {
            myLog << " ; " << static_cast<VariableNetworkNode>(inputPair._message).getQualifiedName() << " = "
                  << std::string(inputPair._message);
          }
          myLog << std::endl;
        }
        myLog << "output status is " << _output._status << " (" << std::string(_output._message) << ")" << std::endl;

        myLog << "debug info finished." << std::endl;
        goto waitForChange;
      }
    }
  }

  /********************************************************************************************************************/

  DataValidity StatusAggregator::getDataValidity() const {
    return DataValidity::ok;
  }

  /********************************************************************************************************************/

  void StatusAggregator::setWarnMixedMessage(std::string message) {
    _warnMixedMessage = std::move(message);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
