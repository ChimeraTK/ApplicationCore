// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "UserInputValidator.h"

#include "Module.h"

#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  UserInputValidator::Validator* UserInputValidator::addValidator(
      const std::function<bool(void)>& isValidFunction, const std::string& errorMessage) {
    // create validator and store in list
    _validators.emplace_back(isValidFunction, errorMessage);

    return &_validators.back();
  }

  /********************************************************************************************************************/

  void UserInputValidator::setErrorFunction(const std::function<void(const std::string&)>& errorFunction) {
    _errorFunction = errorFunction;
  }

  /********************************************************************************************************************/

  bool UserInputValidator::validate(const ChimeraTK::TransferElementID& change) {
    if(!change.isValid()) {
      return validateAll();
    }

    if(!_finalised) {
      throw ChimeraTK::logic_error("Initial values were not validated");
    }

    // We have downstream channels that signalized a change - invalidate all of our
    if(_downstreamInvalidatingReturnChannels.count(change) > 0) {
      _module->setCurrentVersionNumber({});
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

  /********************************************************************************************************************/

  bool UserInputValidator::validateAll() {
    if(!_finalised) {
      finalise();
    }
    bool rejected = false;
    for(auto& v : _variableMap) {
      rejected |= validate(v.first);
    }
    return rejected;
  }

  /********************************************************************************************************************/

  void UserInputValidator::finalise() {
    if(_finalised) {
      return;
    }

    if(_module == nullptr) {
      throw ChimeraTK::logic_error("UserInputValidator was finalised without any call to add()");
    }

    for(auto& accessor : _module->getAccessorListRecursive()) {
      if(accessor.getDirection() == VariableDirection{VariableDirection::feeding, true} &&
          accessor.getModel().getTags().count(std::string(UserInputValidator::tagValidatedVariable)) > 0) {
        _downstreamInvalidatingReturnChannels.emplace(accessor.getAppAccessorNoType().getId());
      }
    }

    // Find longest path that is validated in our model, starting at this module

    // Step 1: Do a topological order of the tree of modules with a return channel, starting from this module
    std::deque<Model::ApplicationModuleProxy> stack;
    std::map<ApplicationModule*, int> distances;

    // Visitor that a) builds the order in stack and b) initialises the distance array used later to calculate
    // the distance from this node to the entry in that array
    auto orderVisitor = [&](auto proxy) {
      if constexpr(Model::isApplicationModule(proxy)) {
        stack.push_front(proxy);
        distances.try_emplace(&proxy.getApplicationModule(), std::numeric_limits<int>::min());
      }
    };

    // March through the tree with post order to properly build the stack. Technically sort a tree that is larger
    // that what we want to look at, because we cannot stop the visit if there is a PV access with return channel
    // that does not correspond to another UserInputValidator. However these distances should then left
    // uninitialised in the distance calculation below and not taken into account
    _module->getModel().visit(orderVisitor, Model::visitOrderPost, Model::depthFirstSearch,
        Model::keepPvAccesWithReturnChannel, Model::keepApplicationModules);

    // Step 2: From the topological sort of the subtree, calculate the distances from our module to the currently
    // checked module.
    distances[_module] = 0;
    std::unordered_set<ApplicationModule*> downstreamModulesWithFeedback;

    auto downstreamModuleCollector = [&](auto proxy) {
      if constexpr(Model::isApplicationModule(proxy)) {
        downstreamModulesWithFeedback.insert(&proxy.getApplicationModule());
      }
    };

    auto connectingVariableVisitor = [&](auto proxy) {
      if constexpr(Model::isVariable(proxy)) {
        proxy.visit(downstreamModuleCollector, Model::adjacentOutSearch, Model::keepPvAccesWithReturnChannel,
            Model::keepProcessVariables);
      }
    };

    for(const auto& stackEntry : stack) {
      downstreamModulesWithFeedback.clear();

      // We need to find all connected modules to the module currently looked at. Unfortunately we need to
      // do that with a double visit, because the connection via PV access edges is not directly between modules
      // but through a variable Vertex. So we first jump to the Variable using adjacentOut and in that visitor
      // collect the modules into the downstreamModulesWithFeedback set
      stackEntry.visit(connectingVariableVisitor, Model::adjacentOutSearch, Model::keepApplicationModules,
          Model::keepPvAccesWithReturnChannel);

      // The distances from this module to the module on the stack is then just updated to be either what it
      // was or the distance from the currently looked-at stack entry + 1 (since we have equal weights for the
      // edges)
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

    _finalised = true;
  }

  /********************************************************************************************************************/

  UserInputValidator::Validator::Validator(std::function<bool(void)> isValidTest, std::string initialErrorMessage)
  : isValidFunction(std::move(isValidTest)), errorMessage(std::move(initialErrorMessage)) {}

  /********************************************************************************************************************/

} // namespace ChimeraTK
