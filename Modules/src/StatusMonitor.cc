// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "StatusMonitor.h"

namespace ChimeraTK {

  /*******************************************************************************************************************/
  /* Implementation of MonitorBase ***********************************************************************************/
  /*******************************************************************************************************************/

  MonitorBase::MonitorBase(EntityOwner* owner, const std::string& description, const std::string& outputPath,
      const std::string& disablePath, const std::unordered_set<std::string>& outputTags,
      const std::unordered_set<std::string>& parameterTags)
  : ApplicationModule(owner, "hidden", description, HierarchyModifier::hideThis),
    disable(this, disablePath, "", "Disable the status monitor", parameterTags),
    status(this, outputPath, "Resulting status", outputTags) {}

  /*******************************************************************************************************************/

  void MonitorBase::setStatus(StatusOutput::Status newStatus) {
    // update only if status has changed, but always in case of initial value
    if(status.value != newStatus || getDataValidity() != lastStatusValidity ||
        status.value.getVersionNumber() == VersionNumber{nullptr}) {
      status.value = newStatus;
      status.value.write();
      lastStatusValidity = getDataValidity();
    }
  }

  /*******************************************************************************************************************/

} // namespace ChimeraTK
