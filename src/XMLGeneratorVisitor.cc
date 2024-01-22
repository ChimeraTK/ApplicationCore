// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "XMLGeneratorVisitor.h"

#include "Application.h"
#include "VariableGroup.h"
#include "VariableNetworkNode.h"
#include <libxml++/libxml++.h>

#include <ChimeraTK/RegisterPath.h>

#include <cassert>
#include <ConnectionMaker.h>

namespace detail {
  static constexpr std::string_view AC_NAMESPACE_URL{"https://github.com/ChimeraTK/ApplicationCore"};
} // namespace detail

/********************************************************************************************************************/
/********************************************************************************************************************/

namespace ChimeraTK {

  /********************************************************************************************************************/

  XMLGenerator::XMLGenerator(Application& app)
  : NetworkVisitor{app}, _doc{std::make_shared<xmlpp::Document>()}, _rootElement{_doc->create_root_node("application",
                                                                        ::detail::AC_NAMESPACE_URL.data())} {}
  /********************************************************************************************************************/

  void XMLGenerator::run() {
    _rootElement->set_attribute("name", _app.getName());

    std::list<Model::DeviceModuleProxy> deviceModules;

    // Collect all DeviceModule proxies with triggers
    auto deviceModuleCollector = [&](auto proxy) {
      auto trigger = proxy.getTrigger();
      if(not trigger.isValid()) {
        return;
      }

      deviceModules.push_back(proxy);
    };

    _app.getModel().visit(deviceModuleCollector, Model::depthFirstSearch, Model::keepDeviceModules);

    // Add a TriggerReceiver placeholder for every device associated with that trigger
    // Do this in two steps, otherwise we would be modifying the model while iterating
    for(auto& proxy : deviceModules) {
      auto trigger = proxy.getTrigger();
      VariableNetworkNode placeholder(proxy.getAliasOrCdd(), 0);
      proxy.addVariable(trigger, placeholder);
    };

    auto connectingVisitor = [&](auto proxy) { generateXMLNetwork(proxy); };

    // ChimeraTK::Model::keepParenthood - small optimisation for iterating the model only once
    _app.getModel().visit(connectingVisitor, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepProcessVariables,
        ChimeraTK::Model::keepParenthood);
  }

  /********************************************************************************************************************/

  void XMLGenerator::save(const std::string& fileName) {
    _doc->write_to_file_formatted(fileName);
  }

  /********************************************************************************************************************/

  NetworkVisitor::NetworkInformation XMLGenerator::generateXMLNetwork(Model::ProcessVariableProxy& proxy) {
    auto net = checkAndFinaliseNetwork(proxy);

    RegisterPath folder = proxy.getFullyQualifiedPath();
    folder--;

    generateXMLForNode(net, net.feeder);

    for(auto& consumer : net.consumers) {
      generateXMLForNode(net, consumer);
    }

    return net;
  }

  /********************************************************************************************************************/

  std::string XMLGenerator::mapTypeToName(const std::type_info* type) {
    // add sub-element containing the data type
    std::string dataTypeName{"unknown"};
    if(*type == typeid(int8_t)) {
      dataTypeName = "int8";
    }
    else if(*type == typeid(uint8_t)) {
      dataTypeName = "uint8";
    }
    else if(*type == typeid(int16_t)) {
      dataTypeName = "int16";
    }
    else if(*type == typeid(uint16_t)) {
      dataTypeName = "uint16";
    }
    else if(*type == typeid(int32_t)) {
      dataTypeName = "int32";
    }
    else if(*type == typeid(uint32_t)) {
      dataTypeName = "uint32";
    }
    else if(*type == typeid(int64_t)) {
      dataTypeName = "int64";
    }
    else if(*type == typeid(uint64_t)) {
      dataTypeName = "uint64";
    }
    else if(*type == typeid(float)) {
      dataTypeName = "float";
    }
    else if(*type == typeid(double)) {
      dataTypeName = "double";
    }
    else if(*type == typeid(std::string)) {
      dataTypeName = "string";
    }
    else if(*type == typeid(ChimeraTK::Void)) {
      dataTypeName = "Void";
    }
    else if(*type == typeid(ChimeraTK::Boolean)) {
      dataTypeName = "Boolean";
    }

    return dataTypeName;
  }

  /********************************************************************************************************************/

  void XMLGenerator::generateXMLForNode(NetworkInformation& net, const VariableNetworkNode& node) {
    if(node.getType() != NodeType::ControlSystem) {
      return;
    }
    // Create the directory for the path name in the XML document with all parent
    // directories, if not yet existing: First split the publication name into
    // components and loop over each component. For each component, try to find
    // the directory node and create it it does not exist. After the loop, the
    // "current" will point to the Element representing the directory.

    // strip the variable name from the path
    ChimeraTK::RegisterPath directory(net.proxy->getFullyQualifiedPath());
    directory--;

    // the namespace map is needed to properly refer to elements with an xpath
    // expression in xmlpp::Element::find()
    xmlpp::Node::PrefixNsMap nsMap{{"ac", ::detail::AC_NAMESPACE_URL.data()}};

    // go through each directory path component
    xmlpp::Element* current = _rootElement;
    for(auto& pathComponent : directory.getComponents()) {
      // find directory for this path component in the current directory
      std::string xpath = std::string("ac:directory[@name='") + pathComponent + std::string("']");
      auto list = current->find(xpath, nsMap);
      if(list.empty()) { // not found: create it
        xmlpp::Element* newChild = current->add_child("directory");
        newChild->set_attribute("name", pathComponent);
        current = newChild;
      }
      else {
        assert(list.size() == 1);
        current = dynamic_cast<xmlpp::Element*>(list[0]);
        assert(current != nullptr);
      }
    }

    // now add the variable to the directory
    xmlpp::Element* variable = current->add_child("variable");
    ChimeraTK::RegisterPath pathName(node.getPublicName());
    auto pathComponents = pathName.getComponents();

    // set the name attribute
    variable->set_attribute("name", pathComponents[pathComponents.size() - 1]);

    auto dataTypeName = mapTypeToName(net.valueType);

    xmlpp::Element* valueTypeElement = variable->add_child("value_type");
    valueTypeElement->set_child_text(dataTypeName);

    // add sub-element containing the data flow direction
    std::string dataFlowName{"application_to_control_system"};
    if(net.feeder == node) {
      dataFlowName = "control_system_to_application";
      if(!net.feeder.getDirection().withReturn) {
        dataFlowName = "control_system_to_application";
      }
      else {
        dataFlowName = "control_system_to_application_with_return";
      }
    }
    xmlpp::Element* directionElement = variable->add_child("direction");
    directionElement->set_child_text(dataFlowName);

    // add sub-element containing the engineering unit
    xmlpp::Element* unitElement = variable->add_child("unit");
    unitElement->set_child_text(net.unit);

    // add sub-element containing the description
    xmlpp::Element* descriptionElement = variable->add_child("description");
    descriptionElement->set_child_text(net.description);

    // add sub-element containing the description
    xmlpp::Element* nElementsElement = variable->add_child("numberOfElements");
    nElementsElement->set_child_text(std::to_string(net.valueLength));

    // add sub-element describing how this variable is connected
    xmlpp::Element* connectedModules = variable->add_child("connections");
    std::list nodeList(net.consumers.begin(), net.consumers.end());
    nodeList.push_back(net.feeder);

    generatePeerList(connectedModules, nodeList);
  }

  /********************************************************************************************************************/

  void XMLGenerator::generatePeerList(
      xmlpp::Element* connectedModules, const std::list<VariableNetworkNode>& nodeList) {
    for(const auto& peerNode : nodeList) {
      if(peerNode.getType() == NodeType::ControlSystem) {
        continue;
      }
      bool feeding = peerNode == nodeList.back();
      xmlpp::Element* peer = connectedModules->add_child("peer");

      if(peerNode.getType() == NodeType::Application) {
        peer->set_attribute("type", "ApplicationModule");
        // find ApplicationModule (owner might be VariableGroup deep down)
        const auto* owningModule = peerNode.getOwningModule();
        while(owningModule->getModuleType() == EntityOwner::ModuleType::VariableGroup) {
          owningModule = dynamic_cast<const VariableGroup&>(*owningModule).getOwner();
        }
        // get name of ApplicationModule
        auto qname = owningModule->getQualifiedName();
        // strip leading application name
        auto secondSlash = qname.find_first_of('/', 1);
        if(secondSlash != std::string::npos) {
          qname = qname.substr(secondSlash);
        }
        peer->set_attribute("name", qname);
        const auto& owningModuleRef = *owningModule; // dereferencing in separate line to avoid linter warning
        auto className = boost::core::demangle(typeid(owningModuleRef).name());
        peer->set_attribute("class", className);
      }
      else if(peerNode.getType() == NodeType::Constant) {
        peer->set_attribute("type", "Constant");
      }
      else if(peerNode.getType() == NodeType::Device) {
        peer->set_attribute("type", "Device");
        peer->set_attribute("name", peerNode.getDeviceAlias());
      }
      else if(peerNode.getType() == NodeType::TriggerProvider) {
        peer->set_attribute("type", "TriggerProvider");
      }
      else if(peerNode.getType() == NodeType::TriggerReceiver) {
        peer->set_attribute("type", "TriggerReceiver");
        peer->set_attribute("name", "Device = " + peerNode.getDeviceAlias());
      }
      else {
        peer->set_attribute("type", "Unknown (" + std::to_string(int(peerNode.getType())) + ")");
      }

      peer->set_attribute("direction", feeding ? "feeding" : "consuming");
    }
  }
} // namespace ChimeraTK
