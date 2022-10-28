// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ConnectionMaker.h"

#include <memory>
#include <string>

// Forward declarations
namespace xmlpp {
  class Document;
  class Element;
} // namespace xmlpp

namespace ChimeraTK {

  /**
   * @brief Generate XML representation of variables
   *
   * This class is responsible for generating the XML representation of the variables in an Application.
   */
  class XMLGenerator : NetworkVisitor {
   public:
    XMLGenerator(Application& app);
    ~XMLGenerator() = default;

    void run();
    void save(const std::string& fileName);

   private:
    using NetworkVisitor::_triggerNetworks;

    std::shared_ptr<xmlpp::Document> _doc;
    xmlpp::Element* _rootElement;

    NetworkInformation generateXMLNetwork(Model::ProcessVariableProxy& proxy);

    /**
     * @brief generate the XML representation of @node
     * @param net The currently processed network meta-data
     * @param node The network node
     */
    void generateXMLForNode(NetworkInformation& net, const VariableNetworkNode& node);

    /**
     * @brief generate XML list of peers
     * @param connectedModules XML node to fill
     * @param nodeList list of peer nodes
     */
    void generatePeerList(xmlpp::Element* connectedModules, const std::list<VariableNetworkNode>& nodeList);

    /**
     * @brief convert std::type_info to user-readable string
     * @param type std::type_info
     * @return the string representation of type or {unknown} if not mappable
     */
    std::string mapTypeToName(const std::type_info* type);
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
