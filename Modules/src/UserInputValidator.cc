// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "UserInputValidator.h"

#include "Module.h"

#include <utility>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  void UserInputValidator::setErrorFunction(const std::function<void(const std::string&)>& errorFunction) {
    _errorFunction = errorFunction;
  }

  /*********************************************************************************************************************/

  bool UserInputValidator::validate(const ChimeraTK::TransferElementID& change) {
    if(!change.isValid()) {
      return validateAll();
    }

    // We have downstream channels that signalized a change - invalidate all of our
    if(_downstreamInvalidatingReturnChannels.count(change) > 0) {
      for(auto& v : _variableMap) {
        v.second->reject(VariableBase::RejectionType::downstream);
      }

      return false;
    }

    if(!_validatorMap.count(change)) {
      return false;
    }

    for(auto* validator : _validatorMap.at(change)) {
      if(!validator->isValidFunction()) {
        _errorFunction(validator->errorMessage);
        _variableMap.at(change)->reject(VariableBase::RejectionType::self);
        return true;
      }
    }

    _variableMap.at(change)->accept();
    return false;
  }

  /*********************************************************************************************************************/

  bool UserInputValidator::validateAll() {
    bool rejected = false;
    for(auto& v : _variableMap) {
      rejected |= validate(v.first);
    }
    return rejected;
  }

  void UserInputValidator::enableDeepValidation(ApplicationModule* module) {
    if(module == nullptr) {
      throw ChimeraTK::logic_error(
          "Deep validation requires UserInputValidator to be created with its containing module");
    }

    // Find out accessors with return
    for(auto& accessor : module->getAccessorListRecursive()) {
      if(accessor.getDirection() == VariableDirection{VariableDirection::feeding, true} &&
          accessor.getModel().getTags().count(std::string(UserInputValidator::tagValidatedVariable)) > 0) {
        _downstreamInvalidatingReturnChannels.emplace(accessor.getAppAccessorNoType().getId());
      }
    }

    auto keepPvAccesWithReturnChannel = Model::EdgeFilter([](const Model::EdgeProperties& edge) -> bool {
      return edge.type == Model::EdgeProperties::Type::pvAccess && edge.pvAccessWithReturnChannel;
    });

    // Filter that keeps our tagged variable nodes and application modules
    auto validatedVariablesAndApplicationModulesFilter =
        Model::VertexFilter([](const Model::VertexProperties& vertex) -> bool {
          return vertex.visit([](auto props) -> bool {
            if constexpr(Model::isVariable(props)) {
              return props.tags.count(std::string(UserInputValidator::tagValidatedVariable)) > 0;
            }

            if constexpr(Model::isApplicationModule(props)) {
              return true;
            }

            return false;
          });
        });

    // Find longest path that is validated in our model, starting at this module
    std::deque<Model::ApplicationModuleProxy> stack;
    std::map<ApplicationModule*, int> distances;
    distances[module] = 0;

    auto orderVisitor = [&](auto proxy) {
      if constexpr(Model::isApplicationModule(proxy)) {
        stack.push_front(proxy);
        distances.try_emplace(&proxy.getApplicationModule(), std::numeric_limits<int>::min());
      }
    };

    module->getModel().visit(orderVisitor, Model::visitOrderPost, Model::depthFirstSearch, keepPvAccesWithReturnChannel,
        validatedVariablesAndApplicationModulesFilter);

    std::unordered_set<ApplicationModule*> downstreamModulesWithFeedback;

    auto downstreamModuleCollector = [&](auto proxy) {
      if constexpr(Model::isApplicationModule(proxy)) {
        downstreamModulesWithFeedback.insert(&proxy.getApplicationModule());
      }
    };

    auto connectingVariableVisitor = [&](auto proxy) {
      if constexpr(Model::isVariable(proxy)) {
        proxy.visit(downstreamModuleCollector, Model::adjacentOutSearch, validatedVariablesAndApplicationModulesFilter,
            Model::keepApplicationModules);
      }
    };

    for(const auto& stackEntry : stack) {
      downstreamModulesWithFeedback.clear();
      stackEntry.visit(connectingVariableVisitor, Model::adjacentOutSearch,
          validatedVariablesAndApplicationModulesFilter, keepPvAccesWithReturnChannel);

      for(auto* vtx : downstreamModulesWithFeedback) {
        distances[vtx] = std::max(distances[vtx], distances[&stackEntry.getApplicationModule()] + 1);
      }
    }

    _validationDepth = 1 + std::max_element(distances.begin(), distances.end(), [](auto& a, auto& b) -> bool {
      return a.second < b.second;
    })->second;

    for(auto& v : _variableMap) {
      v.second->setHistorySize(_validationDepth);
    }
  }

  /*********************************************************************************************************************/

  UserInputValidator::Validator::Validator(std::function<bool(void)> isValidTest, std::string initialErrorMessage)
  : isValidFunction(std::move(isValidTest)), errorMessage(std::move(initialErrorMessage)) {}

  /*********************************************************************************************************************/

} // namespace ChimeraTK
