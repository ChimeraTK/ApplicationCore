// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "StatusAggregator.h"

#include <list>
#include <regex>
#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  StatusAggregator::StatusAggregator(ModuleGroup* owner, const std::string& name, const std::string& description,
      PriorityMode mode, std::unordered_set<std::string> tagsToAggregate,
      const std::unordered_set<std::string>& outputTags)
  : ApplicationModule(owner, ".", description, outputTags), _output(this, name), _mode(mode),
    _tagsToAggregate(std::move(tagsToAggregate)) {
    // check maximum size of tagsToAggregate
    if(_tagsToAggregate.size() > 1) {
      throw ChimeraTK::logic_error("StatusAggregator: List of tagsToAggregate must contain at most one tag.");
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

          if(_tagsToAggregate == staAggPtr->_tagsToAggregate) {
            inputPathsSet.insert(staAggPtr->_output._status.getModel().getFullyQualifiedPath());

            statusToMessagePathsMap[staAggPtr->_output._status.getModel().getFullyQualifiedPath()] =
                staAggPtr->_output._message.getModel().getFullyQualifiedPath();

            for(auto& anotherStatusAgregatorInput : staAggPtr->_inputs) {
              anotherStatusAgregatorInputSet.insert(
                  anotherStatusAgregatorInput._status.getModel().getFullyQualifiedPath());
            }
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
        if(tags.find(StatusOutput::tagStatusOutput) != tags.end()) {
          for(const auto& tagToAgregate : _tagsToAggregate) {
            // Each tag attached to this StatusAggregator must be present at all StatusOutputs to be aggregated
            if(tags.find(tagToAgregate) == tags.end()) {
              return;
            }
          }
          inputPathsSet.insert(proxy.getFullyQualifiedPath());
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
      if(x.hasMessageSource) {
        inputsMap[x._message.getId()] = &x;
      }
    }

    auto rag = readAnyGroup();
    DataValidity lastStatusValidity = DataValidity::ok;
    while(true) {
      // find highest priority status of all inputs
      StatusOutput::Status status{StatusOutput::Status::FAULT}; // initialised just to prevent warning
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
        auto* varPair = f->second;
        if(!varPair->update(change)) {
          goto waitForChange; // inputs not in consistent state yet
        }
      }

      // handle request for debug info
      if(change == _debug.getId()) {
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

} // namespace ChimeraTK
