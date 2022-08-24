// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "VariableNetworkNodeDumpingVisitor.h"
#include "Visitor.h"

#include <string>

namespace ChimeraTK {

  // Forward declarations
  class VariableNetwork;

  /**
   * @brief The VariableNetworkDumpingVisitor class
   *
   * This class provides a textual dump of the VariableNetwork
   */
  class VariableNetworkDumpingVisitor : public Visitor<VariableNetwork>, public VariableNetworkNodeDumpingVisitor {
   public:
    VariableNetworkDumpingVisitor(const std::string& prefix, std::ostream& stream);
    virtual ~VariableNetworkDumpingVisitor() {}
    void dispatch(const VariableNetwork& t);
    using Visitor<VariableNetworkNode>::dispatch;

   private:
    std::string _prefix;
  };

} // namespace ChimeraTK
