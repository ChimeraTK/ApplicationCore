// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "VariableNetworkNodeDumpingVisitor.h"
#include "Visitor.h"

#include <map>

namespace ChimeraTK {

  // Forward Declarations

  class Application;
  class VariableNetwork;
  class Module;

  /**
   * @brief The VariableNetworkModuleGraphDumpingVisitor class
   *
   * This class provides a Graphiviz dump of the connections between modiles.
   * Due to the potential size of the resulting graph, it is recommended to use
   * SVG for rendering the resulting graph.
   */
  class VariableNetworkModuleGraphDumpingVisitor : public Visitor<Application, Module, VariableNetwork>,
                                                   VariableNetworkNodeDumpingVisitor {
   public:
    VariableNetworkModuleGraphDumpingVisitor(std::ostream& stream);
    virtual ~VariableNetworkModuleGraphDumpingVisitor() {}
    void dispatch(const Application& t) override;
    void dispatch(const Module& t);
    void dispatch(const VariableNetwork& t);

   private:
    // edge map contains all edges. key is the edge in graphviz notation ("NodeA->NodeB"), value is the label and
    // a flag whether the arrow needs to be inverted
    std::map<std::string, std::pair<std::string, bool>> _edgeMap;
    std::list<std::string> _deviceList;
  };

} // namespace ChimeraTK
