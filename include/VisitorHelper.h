// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <string>

namespace ChimeraTK {
  class VariableNetworkNode;

  namespace detail {

    std::string encodeDotNodeName(std::string name);
    std::string nodeName(const VariableNetworkNode& node);

  } // namespace detail
} // namespace ChimeraTK
