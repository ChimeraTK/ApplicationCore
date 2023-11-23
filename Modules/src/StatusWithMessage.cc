// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "StatusWithMessage.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  StatusWithMessage::StatusWithMessage(VariableGroup* owner, const std::string& qualifiedStatusVariableName,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : VariableGroup(owner, Utilities::getPathName(qualifiedStatusVariableName), description, tags),
    _status(this, Utilities::getUnqualifiedName(qualifiedStatusVariableName), description, {tagStatusHasMessage}),
    _message(this, Utilities::getUnqualifiedName(qualifiedStatusVariableName) + "_message", "", "status message") {}

  /********************************************************************************************************************/

  void StatusWithMessage::write(StatusOutput::Status status, std::string message) {
    set(status, std::move(message));
    writeAll();
  }

  /********************************************************************************************************************/

  void StatusWithMessage::writeIfDifferent(StatusOutput::Status status, std::string message) {
    if(status != _status || message != std::string(_message) || _status.getVersionNumber() == VersionNumber{nullptr}) {
      write(status, std::move(message));
    }
  }

  /********************************************************************************************************************/

  void StatusWithMessage::writeOk() {
    setOk();
    writeAll();
  }

  /********************************************************************************************************************/

  void StatusWithMessage::writeOkIfDifferent() {
    if(_status != StatusOutput::Status::OK || _status.getVersionNumber() == VersionNumber{nullptr}) {
      setOk();
      writeAll();
    }
    // This assert makes sure the above "if" condition is sufficient. There is no way to set the status to OK and have
    // a non-empty message string.
    assert(std::string(_message).empty());
  }

  /********************************************************************************************************************/

  void StatusWithMessage::set(StatusOutput::Status status, std::string message) {
    assert(status != StatusOutput::Status::OK);
    _status = status;
    _message = std::move(message);
  }

  /********************************************************************************************************************/

  void StatusWithMessage::setOk() {
    _status = StatusOutput::Status::OK;
    _message = "";
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  StatusWithMessageInput::StatusWithMessageInput(ApplicationModule* owner, const std::string& qualifiedName,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : VariableGroup(owner, Utilities::getPathName(qualifiedName), "", tags),
    _status(this, Utilities::getUnqualifiedName(qualifiedName), description) {
    hasMessageSource = false;
    _statusNameLong = description;
  }

  /********************************************************************************************************************/

  void StatusWithMessageInput::setMessageSource(std::string msgInputName) {
    // at the time this function is called, TransferElement impl is not yet set, so don't look there for name
    if(msgInputName.empty()) {
      msgInputName = ((VariableNetworkNode)_status).getName() + "_message";
    }
    // late initialization of _message
    _message = ScalarPushInput<std::string>(this, msgInputName, "", "");
    hasMessageSource = true;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK