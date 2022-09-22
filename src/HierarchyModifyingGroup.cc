// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "HierarchyModifyingGroup.h"

#include "Utilities.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getUnqualifiedName(const std::string& qualifiedName) {
    return Utilities::getUnqualifiedName(qualifiedName);
  }

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getPathName(const std::string& qualifiedName) {
    return Utilities::getPathName(qualifiedName);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
