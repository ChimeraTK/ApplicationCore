#include "UserInputValidator.h"

namespace ChimeraTK {

  /*********************************************************************************************************************/

  void UserInputValidator::setErrorFunction(const std::function<void(const std::string&)>& errorFunction) {
    _errorFunction = errorFunction;
  }

  /*********************************************************************************************************************/

  bool UserInputValidator::validate(const ChimeraTK::TransferElementID& change) {
    if(!change.isValid()) return validateAll();
    if(!_validatorMap.count(change)) return false;

    for(auto validator : _validatorMap.at(change)) {
      if(!validator->_isValidFunction()) {
        _errorFunction(validator->_errorMessage);
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

  UserInputValidator::Validator::Validator(const std::function<bool(void)>& isValidFunction, std::string errorMessage)
  : _isValidFunction(isValidFunction), _errorMessage(errorMessage) {}

  /*********************************************************************************************************************/

} // namespace ChimeraTK
