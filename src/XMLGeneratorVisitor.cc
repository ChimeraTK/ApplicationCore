// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "XMLGeneratorVisitor.h"

#include "Application.h"
#include "VariableGroup.h"
#include "VariableNetworkNode.h"
#include <libxml++/libxml++.h>

#include <ChimeraTK/RegisterPath.h>

#include <cassert>
#include <cxxabi.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  XMLGeneratorVisitor::XMLGeneratorVisitor()
  : Visitor<ChimeraTK::Application, ChimeraTK::VariableNetworkNode>(), _doc(std::make_shared<xmlpp::Document>()),
    _rootElement(_doc->create_root_node("application", "https://github.com/ChimeraTK/ApplicationCore")) {}

  /********************************************************************************************************************/

  void XMLGeneratorVisitor::save(const std::string& fileName) {
    _doc->write_to_file_formatted(fileName);
  }

  /********************************************************************************************************************/

  void XMLGeneratorVisitor::dispatch(const Application& app) {
    _rootElement->set_attribute("name", app.getName());
#if 0
    for(auto& network : app.networkList) {
      network.check();

      auto feeder = network.getFeedingNode();
      feeder.accept(*this);

      for(auto& consumer : network.getConsumingNodes()) {
        consumer.accept(*this);
      }
    }
#endif
  }

  /********************************************************************************************************************/

  void XMLGeneratorVisitor::dispatch(const VariableNetworkNode& node) {
    if(node.getType() != NodeType::ControlSystem) return;
    throw ChimeraTK::logic_error("XML dumping is currently NOT WORKING!");
#if 0
    // Create the directory for the path name in the XML document with all parent
    // directories, if not yet existing: First split the publication name into
    // components and loop over each component. For each component, try to find
    // the directory node and create it it does not exist. After the loop, the
    // "current" will point to the Element representing the directory.

    // strip the variable name from the path
    ChimeraTK::RegisterPath directory(node.getPublicName());
    directory--;

    // the namespace map is needed to properly refer to elements with an xpath
    // expression in xmlpp::Element::find()
    /// @todo TODO move this somewhere else, or at least take the namespace URI
    /// from a common place!
    xmlpp::Node::PrefixNsMap nsMap{{"ac", "https://github.com/ChimeraTK/ApplicationCore"}};

    // go through each directory path component
    xmlpp::Element* current = _rootElement;
    for(auto& pathComponent : directory.getComponents()) {
      // find directory for this path component in the current directory
      std::string xpath = std::string("ac:directory[@name='") + pathComponent + std::string("']");
      auto list = current->find(xpath, nsMap);
      if(list.size() == 0) { // not found: create it
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

    // add sub-element containing the data type
    std::string dataTypeName{"unknown"};
    auto& owner = node.getOwner();
    auto& type = owner.getValueType();
    if(type == typeid(int8_t)) {
      dataTypeName = "int8";
    }
    else if(type == typeid(uint8_t)) {
      dataTypeName = "uint8";
    }
    else if(type == typeid(int16_t)) {
      dataTypeName = "int16";
    }
    else if(type == typeid(uint16_t)) {
      dataTypeName = "uint16";
    }
    else if(type == typeid(int32_t)) {
      dataTypeName = "int32";
    }
    else if(type == typeid(uint32_t)) {
      dataTypeName = "uint32";
    }
    else if(type == typeid(int64_t)) {
      dataTypeName = "int64";
    }
    else if(type == typeid(uint64_t)) {
      dataTypeName = "uint64";
    }
    else if(type == typeid(float)) {
      dataTypeName = "float";
    }
    else if(type == typeid(double)) {
      dataTypeName = "double";
    }
    else if(type == typeid(std::string)) {
      dataTypeName = "string";
    }
    else if(type == typeid(ChimeraTK::Void)) {
      dataTypeName = "Void";
    }
    else if(type == typeid(ChimeraTK::Boolean)) {
      dataTypeName = "Boolean";
    }
    xmlpp::Element* valueTypeElement = variable->add_child("value_type");
    valueTypeElement->set_child_text(dataTypeName);

    // add sub-element containing the data flow direction
    std::string dataFlowName{"application_to_control_system"};
    if(owner.getFeedingNode() == node) {
      dataFlowName = "control_system_to_application";
      if(!owner.getFeedingNode().getDirection().withReturn) {
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
    unitElement->set_child_text(owner.getUnit());

    // add sub-element containing the description
    xmlpp::Element* descriptionElement = variable->add_child("description");
    descriptionElement->set_child_text(owner.getDescription());

    // add sub-element containing the description
    xmlpp::Element* nElementsElement = variable->add_child("numberOfElements");
    nElementsElement->set_child_text(std::to_string(owner.getFeedingNode().getNumberOfElements()));

    // add sub-element describing how this variable is connected
    xmlpp::Element* connectedModules = variable->add_child("connections");
    auto nodeList = node.getOwner().getConsumingNodes();
    nodeList.push_back(node.getOwner().getFeedingNode());
    for(const auto& peerNode : nodeList) {
      if(peerNode.getType() == NodeType::ControlSystem) continue;
      bool feeding = peerNode == node.getOwner().getFeedingNode();
      xmlpp::Element* peer = connectedModules->add_child("peer");

      if(peerNode.getType() == NodeType::Application) {
        peer->set_attribute("type", "ApplicationModule");
        // find ApplicationModule (owner might be VariableGroup deep down)
        const auto* owningModule = peerNode.getOwningModule();
        while(owningModule->getModuleType() == EntityOwner::ModuleType::VariableGroup) {
          owningModule = dynamic_cast<const VariableGroup&>(*owningModule).getOwner();
        }
        // get name of ApplicatioModule
        auto qname = owningModule->getQualifiedName();
        // strip leading application name
        auto secondSlash = qname.find_first_of('/', 1);
        if(secondSlash != std::string::npos) qname = qname.substr(secondSlash);
        peer->set_attribute("name", qname);
        int status;
        const auto& owningModuleRef = *owningModule; // dereferencing in separate line to avoid linter warning
        const char* mangledName = typeid(owningModuleRef).name();
        std::string className = abi::__cxa_demangle(mangledName, nullptr, nullptr, &status);
        if(status != 0) {
          className = std::string(mangledName) + " (demangling failed)";
        }
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
      else {
        peer->set_attribute("type", "Unknown (" + std::to_string(int(peerNode.getType())) + ")");
      }

      peer->set_attribute("direction", feeding ? "feeding" : "consuming");
    }
#endif
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
