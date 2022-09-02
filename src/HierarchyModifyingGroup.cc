// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "HierarchyModifyingGroup.h"

#include "ApplicationModule.h"
#include "VariableGroup.h"

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getUnqualifiedName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of("/");
    if(found == std::string::npos) return qualifiedName;
    return qualifiedName.substr(found + 1);
  }

  /********************************************************************************************************************/

  std::string HierarchyModifyingGroup::getPathName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of("/");
    if(found == std::string::npos) return ".";
    return qualifiedName.substr(0, found);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
