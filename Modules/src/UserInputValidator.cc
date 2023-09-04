// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "UserInputValidator.h"

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

  /*********************************************************************************************************************/

  UserInputValidator::Validator::Validator(std::function<bool(void)> isValidTest, std::string initialErrorMessage)
  : isValidFunction(std::move(isValidTest)), errorMessage(std::move(std::move(initialErrorMessage))) {}

  /*********************************************************************************************************************/

} // namespace ChimeraTK
