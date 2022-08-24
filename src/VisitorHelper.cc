// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VisitorHelper.h"

#include "VariableNetworkNode.h"

namespace ChimeraTK { namespace detail {

  std::string encodeDotNodeName(std::string name) {
    std::replace(name.begin(), name.end(), '-', 'm'); // minus
    std::replace(name.begin(), name.end(), ':', 'c'); // colon
    std::replace(name.begin(), name.end(), '/', 's'); // slash
    std::replace(name.begin(), name.end(), '.', 'd'); // dot
    std::replace(name.begin(), name.end(), ' ', '_'); // Generic space replacer
    std::replace(name.begin(), name.end(), '*', 'a'); // asterisk
    std::replace(name.begin(), name.end(), '@', 'A'); // at

    return name;
  }

  std::string nodeName(const VariableNetworkNode& node) {
    return node.getQualifiedName().empty() ? node.getName() : node.getQualifiedName();
  }

}} // namespace ChimeraTK::detail
