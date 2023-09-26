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
        v.second->reject();
      }

      return false;
    }

    if(!_validatorMap.count(change)) {
      return false;
    }

    for(auto* validator : _validatorMap.at(change)) {
      if(!validator->isValidFunction()) {
        _errorFunction(validator->errorMessage);
        _variableMap.at(change)->reject();
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

  void UserInputValidator::finalise() {
    bool downstreamValidatorFound{false};

    // Find out accessors with return
    std::list<std::string> outAccessors{};
    for(auto& accessor : _module->getAccessorListRecursive()) {
      if(accessor.getDirection().dir == VariableDirection::feeding && accessor.getDirection().withReturn) {

        auto modelIsValidated =
            accessor.getModel().getTags().count(std::string(UserInputValidator::tagValidatedVariable)) > 0;

        _downstreamInvalidatingReturnChannels.emplace(accessor.getAppAccessorNoType().getId());
        downstreamValidatorFound = true;
      }
    }
  }

  /*********************************************************************************************************************/

  UserInputValidator::Validator::Validator(std::function<bool(void)> isValidTest, std::string initialErrorMessage)
  : isValidFunction(std::move(isValidTest)), errorMessage(std::move(initialErrorMessage)) {}

  /*********************************************************************************************************************/

} // namespace ChimeraTK
