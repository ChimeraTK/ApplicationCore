// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <EventMessageAggregator.h>
#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  AggregatableMessage::AggregatableMessage(Module* owner, const std::string& name, std::string unit,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ScalarOutput(owner, name, std::move(unit), description, tags) {
    addTag(aggregatableMessageTag.data());
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  EventMessageAggregator::Impl::Impl(ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : ApplicationModule(owner, name, description, tags) {
    // Walk through the entire application to find all AggregatableMessages
    std::vector<Model::ProcessVariableProxy> messageVars;
    Application::getInstance().getModel().visit(
        [&](const Model::ProcessVariableProxy& pv) { messageVars.push_back(pv); }, Model::depthFirstSearch,
        Model::keepProcessVariables && Model::keepTag(AggregatableMessage::aggregatableMessageTag.data()));

    // Create input for each message
    for(const auto& pv : messageVars) {
      _inputs.emplace_back(this, pv.getFullyQualifiedPath(), "", "");
    }
  }

  /********************************************************************************************************************/

  void EventMessageAggregator::Impl::mainLoop() {
    size_t autoClearTimerCounter{0};

    // send initial value
    _output.writeOk();

    // build map of input IDs to inputs
    std::map<TransferElementID, ScalarRegisterAccessor<std::string>> idMap;
    for(auto& input : _inputs) {
      idMap[input.getId()].replace(input);
    }

    auto group = readAnyGroup();
    while(true) {
      // wait for change
      auto id = group.readAny();

      if(id == _clear.getId()) {
        // clear requested
        _output.writeOkIfDifferent();
      }
      else if(id == _autoClearTimer.getId()) {
        if(_autoClearSeconds > 0 && _output._status != StatusOutput::Status::OK) {
          // auto clear timer tick
          ++autoClearTimerCounter;
          if(autoClearTimerCounter == _autoClearSeconds) {
            _output.writeOkIfDifferent();
          }
        }
      }
      else {
        // message received
        _output.writeIfDifferent(StatusOutput::Status::WARNING, idMap.at(id));
        autoClearTimerCounter = 0;
      }
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK