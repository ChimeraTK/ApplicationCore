// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ScalarAccessor.h"
#include "StatusAccessor.h"
#include "Utilities.h"
#include "VariableGroup.h"

#include <ChimeraTK/ControlSystemAdapter/StatusWithMessageReader.h>
#include <ChimeraTK/DataConsistencyGroup.h>
#include <ChimeraTK/ForwardDeclarations.h>
#include <ChimeraTK/RegisterPath.h>

#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   *  A VariableGroup for error status and message reporting.
   *  Convenience methods ensure that status and message are updated consistently.
   */
  struct StatusWithMessage : VariableGroup {
    StatusWithMessage(VariableGroup* owner, const std::string& qualifiedStatusVariableName,
        const std::string& description = "", const std::unordered_set<std::string>& tags = {});

    StatusWithMessage() = default;

    /// to be use only for status != OK
    void write(StatusOutput::Status status, std::string message);

    void writeOk();

    // FIXME: This needs additional modification in ControlSystemAdapter if changed
    // https://redmine.msktools.desy.de/issues/12241
    // NOLINTNEXTLINE(readability-identifier-naming)
    StatusOutput _status;
    // NOLINTNEXTLINE(readability-identifier-naming)
    ScalarOutput<std::string> _message;
  };

  /********************************************************************************************************************/

  /**
   * This is for consistent readout of StatusWithMessage - ApplicationCore version.
   *  It can be instantiated with or without message string.
   *  If instantiated without message, the message is generated automatically from the status.
   */
  struct StatusWithMessageInput : StatusWithMessageReaderBase<StatusWithMessageInput>, public VariableGroup {
    /// Construct StatusWithMessageInput which reads only status, not message
    StatusWithMessageInput(ApplicationModule* owner, const std::string& qualifiedName, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    /// read associated status message from given (fully qualified) msgInputName.
    ///  If not given, it is selected automatically by the naming convention
    void setMessageSource(std::string msgInputName = "");

    // TODO: This needs additional modification in ControlSystemAdapter if changed
    // https://redmine.msktools.desy.de/issues/12241
    // NOLINTNEXTLINE(readability-identifier-naming)
    StatusPushInput _status;
    // NOLINTNEXTLINE(readability-identifier-naming)
    ScalarPushInput<std::string> _message; // left uninitialized, if no message source provided
  };

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
