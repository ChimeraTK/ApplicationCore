/*
 * VariableNetworkNode.cc
 *
 *  Created on: Jun 23, 2016
 *      Author: Martin Hierholzer
 */

#include "VariableNetworkNode.h"
#include "Application.h"
#include "EntityOwner.h"
#include "VariableNetwork.h"
#include "VariableNetworkNodeDumpingVisitor.h"
#include "Visitor.h"
#include "VariableGroup.h"
#include <boost/container_hash/hash.hpp>
#include "ApplicationModule.h"

namespace ChimeraTK {

  /*********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(const VariableNetworkNode& other) : pdata(other.pdata) {}

  /*********************************************************************************************************************/

  VariableNetworkNode& VariableNetworkNode::operator=(const VariableNetworkNode& rightHandSide) {
    pdata = rightHandSide.pdata;
    return *this;
  }

  /*********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(EntityOwner* owner, ChimeraTK::TransferElementAbstractor* accessorBridge,
      const std::string& name, VariableDirection direction, std::string unit, size_t nElements, UpdateMode mode,
      const std::string& description, const std::type_info* valueType, const std::unordered_set<std::string>& tags)
  : pdata(boost::make_shared<VariableNetworkNode_data>()) {
    pdata->owningModule = owner;
    pdata->type = NodeType::Application;
    pdata->appNode = accessorBridge;
    pdata->name = name;
    pdata->qualifiedName = owner->getQualifiedName() + "/" + name;
    pdata->mode = mode;
    pdata->direction = direction;
    pdata->valueType = valueType;
    pdata->unit = unit;
    pdata->nElements = nElements;
    pdata->description = description;
    pdata->tags = tags;
  }

  /*********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(const std::string& name, const std::string& devAlias,
      const std::string& regName, UpdateMode mode, VariableDirection dir, const std::type_info& valTyp,
      size_t nElements)
  : pdata(boost::make_shared<VariableNetworkNode_data>()) {
    pdata->name = name;
    pdata->type = NodeType::Device;
    pdata->mode = mode;
    pdata->direction = dir;
    pdata->valueType = &valTyp;
    pdata->deviceAlias = devAlias;
    pdata->registerName = regName;
    pdata->nElements = nElements;
  }

  /*********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(
      std::string pubName, VariableDirection dir, const std::type_info& valTyp, size_t nElements)
  : pdata(boost::make_shared<VariableNetworkNode_data>()) {
    pdata->name = pubName;
    pdata->type = NodeType::ControlSystem;
    pdata->mode = UpdateMode::push;
    pdata->direction = dir;
    pdata->valueType = &valTyp;
    pdata->publicName = pubName;
    pdata->nElements = nElements;
  }

  /*********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(VariableNetworkNode& nodeToTrigger, int)
  : pdata(boost::make_shared<VariableNetworkNode_data>()) {
    pdata->type = NodeType::TriggerReceiver;
    pdata->mode = UpdateMode::push;
    pdata->direction = {VariableDirection::consuming, false};
    pdata->nodeToTrigger = nodeToTrigger;
    pdata->name = "trigger:" + nodeToTrigger.getName();
  }

  /*********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(boost::shared_ptr<VariableNetworkNode_data> _pdata) : pdata(_pdata) {}

  /*********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode() : pdata(boost::make_shared<VariableNetworkNode_data>()) {}

  /*********************************************************************************************************************/

  void VariableNetworkNode::setOwner(VariableNetwork* net) {
    assert(pdata->network == nullptr);
    assert(pdata->type != NodeType::invalid);
    pdata->network = net;
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::clearOwner() { pdata->network = nullptr; }

  /*********************************************************************************************************************/

  bool VariableNetworkNode::hasImplementation() const {
    return pdata->type == NodeType::Device || pdata->type == NodeType::ControlSystem ||
        pdata->type == NodeType::Constant;
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::accept(Visitor<VariableNetworkNode>& visitor) const { visitor.dispatch(*this); }

  /*********************************************************************************************************************/

  bool VariableNetworkNode::operator==(const VariableNetworkNode& other) const {
    return (other.pdata == pdata) || (pdata->type == NodeType::invalid && other.pdata->type == NodeType::invalid);
  }

  /*********************************************************************************************************************/

  bool VariableNetworkNode::operator!=(const VariableNetworkNode& other) const { return !operator==(other); }

  /*********************************************************************************************************************/

  bool VariableNetworkNode::operator<(const VariableNetworkNode& other) const {
    if(pdata->type == NodeType::invalid && other.pdata->type == NodeType::invalid) return false;
    return (other.pdata < pdata);
  }

  /*********************************************************************************************************************/

  VariableNetworkNode VariableNetworkNode::operator>>(VariableNetworkNode other) {
    if(Application::getInstance().initialiseCalled) {
      throw ChimeraTK::logic_error("Cannot make connections after Application::initialise() has been run.");
    }

    if(pdata->direction.dir == VariableDirection::invalid) {
      if(!other.hasOwner()) {
        pdata->direction = {VariableDirection::feeding, false};
      }
      else {
        if(other.getOwner().hasFeedingNode()) {
          pdata->direction = {VariableDirection::consuming, false};
          if(getType() == NodeType::Device) { // special treatment for Device-type variables:
                                              // consumers are push-type
            pdata->mode = UpdateMode::push;
          }
        }
        else {
          pdata->direction = {VariableDirection::feeding, false};
        }
      }
    }
    if(other.pdata->direction.dir == VariableDirection::invalid) {
      if(!hasOwner()) {
        other.pdata->direction = {VariableDirection::consuming, false};
        if(other.getType() == NodeType::Device) { // special treatment for Device-type variables:
                                                  // consumers are push-type
          other.pdata->mode = UpdateMode::push;
        }
      }
      else {
        if(getOwner().hasFeedingNode()) {
          other.pdata->direction = {VariableDirection::consuming, false};
          if(other.getType() == NodeType::Device) { // special treatment for Device-type variables:
                                                    // consumers are push-type
            other.pdata->mode = UpdateMode::push;
          }
        }
        else {
          other.pdata->direction = {VariableDirection::feeding, false};
        }
      }
    }
    Application::getInstance().connect(*this, other);
    return *this;
  }

  /*********************************************************************************************************************/

  VariableNetworkNode VariableNetworkNode::operator[](VariableNetworkNode trigger) {
    // check if node already has a trigger
    if(pdata->externalTrigger.getType() != NodeType::invalid) {
      throw ChimeraTK::logic_error("Only one external trigger per variable network is allowed.");
    }

    // force direction of the node we are operating on to be feeding
    if(pdata->direction.dir == VariableDirection::invalid) pdata->direction = {VariableDirection::feeding, false};
    assert(pdata->direction.dir == VariableDirection::feeding);

    // set direction of the triggering node to be feeding, if not yet defined
    if(trigger.pdata->direction.dir == VariableDirection::invalid)
      trigger.pdata->direction = {VariableDirection::feeding, false};

    // check if already existing in map
    if(pdata->nodeWithTrigger.count(trigger) > 0) {
      return pdata->nodeWithTrigger[trigger];
    }

    // create copy of the node
    pdata->nodeWithTrigger[trigger].pdata = boost::make_shared<VariableNetworkNode_data>(*pdata);

    // add ourselves as a trigger receiver to the other network
    if(!trigger.hasOwner()) {
      Application::getInstance().createNetwork().addNode(trigger);
    }
    trigger.getOwner().addNodeToTrigger(pdata->nodeWithTrigger[trigger]);

    // set flag and store pointer to other network
    pdata->nodeWithTrigger[trigger].pdata->externalTrigger = trigger;

    // return the new node
    return pdata->nodeWithTrigger[trigger];
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setValueType(const std::type_info& newType) const {
    assert(*pdata->valueType == typeid(AnyType));
    pdata->valueType = &newType;
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setDirection(VariableDirection newDirection) const {
    assert(pdata->type == NodeType::ControlSystem);
    assert(pdata->direction.dir == VariableDirection::feeding);
    pdata->direction = newDirection;
  }

  /*********************************************************************************************************************/

  bool VariableNetworkNode::hasExternalTrigger() const { return pdata->externalTrigger.getType() != NodeType::invalid; }

  /*********************************************************************************************************************/

  VariableNetworkNode VariableNetworkNode::getExternalTrigger() {
    assert(pdata->externalTrigger.getType() != NodeType::invalid);
    return pdata->externalTrigger;
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::removeExternalTrigger() {
    assert(hasExternalTrigger());
    pdata->externalTrigger.getOwner().removeNodeToTrigger(*this);
    pdata->externalTrigger = {nullptr};
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::dump(std::ostream& stream) const {
    VariableNetworkNodeDumpingVisitor visitor(stream, " ");
    visitor.dispatch(*this);
  }

  /*********************************************************************************************************************/

  bool VariableNetworkNode::hasOwner() const { return pdata->network != nullptr; }

  /*********************************************************************************************************************/

  NodeType VariableNetworkNode::getType() const {
    if(!pdata) return NodeType::invalid;
    return pdata->type;
  }

  /*********************************************************************************************************************/

  UpdateMode VariableNetworkNode::getMode() const { return pdata->mode; }

  /*********************************************************************************************************************/

  VariableDirection VariableNetworkNode::getDirection() const { return pdata->direction; }

  /*********************************************************************************************************************/

  const std::type_info& VariableNetworkNode::getValueType() const { return *(pdata->valueType); }

  /*********************************************************************************************************************/

  std::string VariableNetworkNode::getName() const { return pdata->name; }

  /*********************************************************************************************************************/

  std::string VariableNetworkNode::getQualifiedName() const { return pdata->qualifiedName; }

  /*********************************************************************************************************************/

  const std::string& VariableNetworkNode::getUnit() const { return pdata->unit; }

  /*********************************************************************************************************************/

  const std::string& VariableNetworkNode::getDescription() const { return pdata->description; }

  /*********************************************************************************************************************/

  VariableNetwork& VariableNetworkNode::getOwner() const {
    assert(pdata->network != nullptr);
    return *(pdata->network);
  }

  /*********************************************************************************************************************/

  VariableNetworkNode VariableNetworkNode::getNodeToTrigger() {
    assert(pdata->nodeToTrigger.getType() != NodeType::invalid);
    return pdata->nodeToTrigger;
  }

  /*********************************************************************************************************************/

  const std::string& VariableNetworkNode::getPublicName() const {
    assert(pdata->type == NodeType::ControlSystem);
    return pdata->publicName;
  }

  /*********************************************************************************************************************/

  const std::string& VariableNetworkNode::getDeviceAlias() const {
    assert(pdata->type == NodeType::Device);
    return pdata->deviceAlias;
  }

  /*********************************************************************************************************************/

  const std::string& VariableNetworkNode::getRegisterName() const {
    assert(pdata->type == NodeType::Device);
    return pdata->registerName;
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setNumberOfElements(size_t nElements) { pdata->nElements = nElements; }

  /*********************************************************************************************************************/

  size_t VariableNetworkNode::getNumberOfElements() const { return pdata->nElements; }

  /*********************************************************************************************************************/

  ChimeraTK::TransferElementAbstractor& VariableNetworkNode::getAppAccessorNoType() { return *(pdata->appNode); }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setMetaData(
      const std::string& name, const std::string& unit, const std::string& description) {
    if(getType() != NodeType::Application) {
      throw ChimeraTK::logic_error("Calling VariableNetworkNode::updateMetaData() is not allowed for "
                                   "non-application type nodes.");
    }
    pdata->name = name;
    pdata->qualifiedName = pdata->owningModule->getQualifiedName() + "/" + name;
    pdata->unit = unit;
    pdata->description = description;
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setMetaData(const std::string& name, const std::string& unit,
      const std::string& description, const std::unordered_set<std::string>& tags) {
    setMetaData(name, unit, description);
    pdata->tags = tags;
  }

  /*********************************************************************************************************************/

  void VariableNetworkNode::addTag(const std::string& tag) { pdata->tags.insert(tag); }

  /*********************************************************************************************************************/

  bool VariableNetworkNode::isCircularInput() const {
    if(!pdata) {
      std::cout << "pdata is nullprt in COMPLETELY UNKNOWN" << std::endl;
      return false;
    }
    std::cout << pdata->name << " has network hash " << pdata->circularNetworkHash << std::endl;
    return pdata->circularNetworkHash != 0;
  }

  /*********************************************************************************************************************/

  std::string printModuleType(EntityOwner::ModuleType type) {
    if(type == EntityOwner::ModuleType::ApplicationModule) return "ApplicationModule";
    if(type == EntityOwner::ModuleType::VariableGroup) return "VariableGroup";
    return "don't care";
  }
  std::list<EntityOwner*> VariableNetworkNode::scanForCircularDepencency() {
    // find the feeder of the network
    auto feeder = getOwner().getFeedingNode();
    auto feedingModule = feeder.getOwningModule();
    // CS modules and device modules don't have an owning module. They stop the circle anyway. So if either the feeder or the
    // receiver (this) don't have an owning module, there is nothing to do here.
    if(!feedingModule || !getOwningModule()) {
      return {};
    }
    assert(getDirection().dir == VariableDirection::consuming);

    auto owningModule = getOwningModule();
    // if the entity owner is a variable group we must go up the hierarchy until we find the applciation module
    while(owningModule->getModuleType() == EntityOwner::ModuleType::VariableGroup) {
      auto variableGroup = static_cast<VariableGroup*>(owningModule);
      owningModule = variableGroup->getOwner();
    }
    assert(owningModule->getModuleType() == EntityOwner::ModuleType::ApplicationModule);
    auto inputModuleList = feedingModule->getInputModulesRecursively({owningModule});

    auto nInstancesFound = std::count(inputModuleList.begin(), inputModuleList.end(), owningModule);
    assert(nInstancesFound >= 1); // the start list must not have been deleted in the call
    // The owning module has been found again when scanning inputs recursively -> There is a circular dependency
    if(nInstancesFound > 1) {
      // clean up the circular network we found and return it.
      inputModuleList.sort();
      inputModuleList.unique();

      // Remember that we are part of a circle, and of which circle
      pdata->circularNetworkHash = boost::hash_range(inputModuleList.begin(), inputModuleList.end());
      // we already did the assertion that the owning module is an application module above, so we can static cast here
      auto applicationModule = static_cast<ApplicationModule*>(owningModule);
      applicationModule->setCircularNetworkHash(pdata->circularNetworkHash);

      // Find the MetaDataPropagatingRegisterDecorator which is involed and set the _isCurularInput flag
      auto internalTargetElements = getAppAccessorNoType().getInternalElements();
      // This is a list of all the nested decorators, so we will find the right point to cast
      for(auto& elem : internalTargetElements) {
        auto flagProvider = boost::dynamic_pointer_cast<MetaDataPropagationFlagProvider>(elem);
        if(flagProvider) {
          flagProvider->_isCircularInput = true;
        }
      }

      return inputModuleList;
    }

    // No circlular network. Return an empty list.
    return {};
  }

  /*********************************************************************************************************************/

  const std::unordered_set<std::string>& VariableNetworkNode::getTags() const { return pdata->tags; }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setAppAccessorPointer(ChimeraTK::TransferElementAbstractor* accessor) {
    assert(getType() == NodeType::Application);
    pdata->appNode = accessor;
  }

  /*********************************************************************************************************************/

  EntityOwner* VariableNetworkNode::getOwningModule() const { return pdata->owningModule; }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setOwningModule(EntityOwner* newOwner) const { pdata->owningModule = newOwner; }

  /*********************************************************************************************************************/

  void VariableNetworkNode::setPublicName(const std::string& name) const { pdata->publicName = name; }

  /*********************************************************************************************************************/

  size_t VariableNetworkNode::getCircularNetworkHash() const { return pdata->circularNetworkHash; }

} // namespace ChimeraTK
