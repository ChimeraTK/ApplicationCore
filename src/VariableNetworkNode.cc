// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VariableNetworkNode.h"

#include "Application.h"
#include "ApplicationModule.h"
#include "CircularDependencyDetectionRecursionStopper.h"
#include "EntityOwner.h"
#include "VariableGroup.h"
#include "VariableNetworkNodeDumpingVisitor.h"
#include "Visitor.h"

#include <boost/container_hash/hash.hpp>

#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(const VariableNetworkNode& other) = default;

  /********************************************************************************************************************/

  VariableNetworkNode& VariableNetworkNode::operator=(const VariableNetworkNode& rightHandSide) = default;

  /********************************************************************************************************************/

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
    pdata->unit = std::move(unit);
    pdata->nElements = nElements;
    pdata->description = description;
    pdata->tags = tags;
  }

  /********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(const std::string& name, const std::string& devAlias,
      const std::string& regName, UpdateMode mode, VariableDirection dir, bool isReadable, const std::type_info& valTyp,
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
    pdata->isReadable = isReadable;
  }

  /********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(
      const std::string& pubName, VariableDirection dir, const std::type_info& valTyp, size_t nElements)
  : pdata(boost::make_shared<VariableNetworkNode_data>()) {
    pdata->name = pubName;
    pdata->type = NodeType::ControlSystem;
    pdata->mode = UpdateMode::push;
    pdata->direction = dir;
    pdata->valueType = &valTyp;
    pdata->publicName = pubName;
    pdata->nElements = nElements;
  }

  /********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(const std::string& deviceAliasOrCdd, int)
  : pdata(boost::make_shared<VariableNetworkNode_data>()) {
    pdata->type = NodeType::TriggerReceiver;
    pdata->mode = UpdateMode::push;
    pdata->direction = {VariableDirection::consuming, false};
    pdata->deviceAlias = deviceAliasOrCdd;
  }

  /********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(const std::type_info* valTyp, bool makeFeeder, size_t length)
  : pdata(boost::make_shared<VariableNetworkNode_data>()) {
    pdata = boost::make_shared<VariableNetworkNode_data>();
    pdata->type = NodeType::Constant;
    pdata->valueType = valTyp;
    pdata->nElements = length;
    pdata->name = "*UNNAMED CONSTANT*";
    if(makeFeeder) {
      pdata->direction = {VariableDirection::feeding, false};
      pdata->mode = UpdateMode::push;
    }
    else {
      pdata->direction = {VariableDirection::consuming, false};
      pdata->mode = UpdateMode::poll;
    }
  }

  /********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode(boost::shared_ptr<VariableNetworkNode_data> _pdata)
  : pdata(std::move(_pdata)) {}

  /********************************************************************************************************************/

  VariableNetworkNode::VariableNetworkNode() : pdata(boost::make_shared<VariableNetworkNode_data>()) {}

  /********************************************************************************************************************/

  bool VariableNetworkNode::hasImplementation() const {
    return pdata->type == NodeType::Device || pdata->type == NodeType::ControlSystem ||
        pdata->type == NodeType::Constant;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::accept(Visitor<VariableNetworkNode>& visitor) const {
    visitor.dispatch(*this);
  }

  /********************************************************************************************************************/

  bool VariableNetworkNode::operator==(const VariableNetworkNode& other) const {
    return (other.pdata == pdata) || (pdata->type == NodeType::invalid && other.pdata->type == NodeType::invalid);
  }

  /********************************************************************************************************************/

  bool VariableNetworkNode::operator!=(const VariableNetworkNode& other) const {
    return !operator==(other);
  }

  /********************************************************************************************************************/

  bool VariableNetworkNode::operator<(const VariableNetworkNode& other) const {
    if(pdata->type == NodeType::invalid && other.pdata->type == NodeType::invalid) {
      return false;
    }
    return (other.pdata < pdata);
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setValueType(const std::type_info& newType) const {
    assert(*pdata->valueType == typeid(AnyType));
    pdata->valueType = &newType;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setDirection(VariableDirection newDirection) const {
    assert((pdata->type == NodeType::ControlSystem && pdata->direction.dir == VariableDirection::feeding) ||
        (pdata->type == NodeType::Device && pdata->direction.dir == VariableDirection::consuming));
    pdata->direction = newDirection;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::dump(std::ostream& stream) const {
    VariableNetworkNodeDumpingVisitor visitor(stream, " ");
    visitor.dispatch(*this);
  }

  /********************************************************************************************************************/

  NodeType VariableNetworkNode::getType() const {
    if(!pdata) {
      return NodeType::invalid;
    }
    return pdata->type;
  }

  /********************************************************************************************************************/

  UpdateMode VariableNetworkNode::getMode() const {
    return pdata->mode;
  }

  /********************************************************************************************************************/

  VariableDirection VariableNetworkNode::getDirection() const {
    return pdata->direction;
  }

  /********************************************************************************************************************/

  const std::type_info& VariableNetworkNode::getValueType() const {
    return *(pdata->valueType);
  }

  /********************************************************************************************************************/

  std::string VariableNetworkNode::getName() const {
    return pdata->name;
  }

  /********************************************************************************************************************/

  std::string VariableNetworkNode::getQualifiedName() const {
    return pdata->qualifiedName;
  }

  /********************************************************************************************************************/

  const std::string& VariableNetworkNode::getUnit() const {
    return pdata->unit;
  }

  /********************************************************************************************************************/

  const std::string& VariableNetworkNode::getDescription() const {
    return pdata->description;
  }

  /********************************************************************************************************************/

  VariableNetworkNode VariableNetworkNode::getNodeToTrigger() const {
    assert(pdata->nodeToTrigger.getType() != NodeType::invalid);
    return pdata->nodeToTrigger;
  }

  /********************************************************************************************************************/

  const std::string& VariableNetworkNode::getPublicName() const {
    assert(pdata->type == NodeType::ControlSystem);
    return pdata->publicName;
  }

  /********************************************************************************************************************/

  const std::string& VariableNetworkNode::getDeviceAlias() const {
    assert(pdata->type == NodeType::Device || pdata->type == NodeType::TriggerReceiver);
    return pdata->deviceAlias;
  }

  /********************************************************************************************************************/

  const std::string& VariableNetworkNode::getRegisterName() const {
    assert(pdata->type == NodeType::Device);
    return pdata->registerName;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setNumberOfElements(size_t nElements) const {
    pdata->nElements = nElements;
  }

  /********************************************************************************************************************/

  size_t VariableNetworkNode::getNumberOfElements() const {
    return pdata->nElements;
  }

  /********************************************************************************************************************/

  bool VariableNetworkNode::isReadable() const {
    return pdata->isReadable;
  }

  /********************************************************************************************************************/

  ChimeraTK::TransferElementAbstractor& VariableNetworkNode::getAppAccessorNoType() const {
    return *(pdata->appNode);
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setMetaData(const std::optional<std::string>& name, const std::optional<std::string>& unit,
      const std::optional<std::string>& description, const std::optional<std::unordered_set<std::string>>& tags) {
    if(getType() != NodeType::Application) {
      throw ChimeraTK::logic_error("Calling VariableNetworkNode::updateMetaData() is not allowed for "
                                   "non-application type nodes.");
    }

    bool needModelUpdate = getModel().isValid() && name.has_value();

    if(needModelUpdate) {
      getModel().removeNode(*this);
    }

    if(name.has_value()) {
      pdata->name = name.value();
      pdata->qualifiedName = pdata->owningModule->getQualifiedName() + "/" + name.value();
    }
    if(unit.has_value()) {
      pdata->unit = unit.value();
    }
    if(description.has_value()) {
      pdata->description = description.value();
    }
    if(tags.has_value()) {
      pdata->tags = tags.value();
    }

    if(needModelUpdate) {
      registerInModel();
    }
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::addTag(const std::string& tag) const {
    if(pdata->tags.erase(negateTag(tag)) == 0) {
      // negated tag was not found, so insert the tag
      pdata->tags.insert(tag);
    }
    auto model = pdata->model.lock();
    if(model.isValid()) {
      model.addTag(tag);
    }
  }

  /********************************************************************************************************************/

  bool VariableNetworkNode::isCircularInput() const {
    return pdata->circularNetworkHash != 0;
  }

  /********************************************************************************************************************/

  std::list<EntityOwner*> VariableNetworkNode::scanForCircularDepencency() const {
    // We are starting a new scan. Reset the indicator for already found circular dependencies.
    detail::CircularDependencyDetectionRecursionStopper::startNewScan();

    if(getType() != NodeType::Application && getType() != NodeType::TriggerReceiver &&
        getType() != NodeType::TriggerProvider) {
      assert(getType() == NodeType::ControlSystem || getType() == NodeType::Device || getType() == NodeType::Constant);
      return {};
    }

    if(!getModel().isValid()) {
      return {};
    }

    // find the feeder of the network
    auto amProxy = getModel().visit(Model::returnApplicationModule, Model::keepPvAccess, Model::keepApplicationModules,
        Model::adjacentInSearch, Model::returnFirstHit(Model::ApplicationModuleProxy{}));
    // CS modules and device modules don't have an owning module. They stop the circle anyway. So if either the feeder
    // or the receiver (this) don't have an owning module, there is nothing to do here.
    if(!amProxy.isValid()) {
      return {};
    }
    assert(getDirection().dir == VariableDirection::consuming);

    Module* owningModule = &amProxy.getApplicationModule();

    // We do not put ourselves in the list right away. The called code will do this as well and detect a circle
    // immediately, even if there is just a simple connection, we leave the marking of the already visited node
    // to the recursive call.
    auto inputModuleList = owningModule->getInputModulesRecursively({});

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
      auto* applicationModule = dynamic_cast<ApplicationModule*>(owningModule);
      applicationModule->setCircularNetworkHash(pdata->circularNetworkHash);

      // Find the MetaDataPropagatingRegisterDecorator which is involved and set the _isCirularInput flag
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

    // No circular network. Return an empty list.
    return {};
  }

  /********************************************************************************************************************/

  const std::unordered_set<std::string>& VariableNetworkNode::getTags() const {
    return pdata->tags;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setAppAccessorPointer(ChimeraTK::TransferElementAbstractor* accessor) const {
    assert(getType() == NodeType::Application || getType() == NodeType::invalid);
    pdata->appNode = accessor;
  }

  /********************************************************************************************************************/

  EntityOwner* VariableNetworkNode::getOwningModule() const {
    return pdata->owningModule;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setOwningModule(EntityOwner* newOwner) const {
    pdata->owningModule = newOwner;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setPublicName(const std::string& name) const {
    pdata->publicName = name;
  }

  /********************************************************************************************************************/

  size_t VariableNetworkNode::getCircularNetworkHash() const {
    return pdata->circularNetworkHash;
  }

  /********************************************************************************************************************/

  Model::ProcessVariableProxy VariableNetworkNode::getModel() const {
    return pdata->model.lock();
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setModel(const Model::ProcessVariableProxy& model) const {
    pdata->model = model;
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::setAppAccessorConstImplementation(const VariableNetworkNode& feeder) const {
    callForType(getValueType(), [&](auto t) {
      using UserType = decltype(t);
      auto impl = boost::dynamic_pointer_cast<ChimeraTK::NDRegisterAccessor<UserType>>(
          boost::make_shared<ConstantAccessor<UserType>>(feeder.getConstantValue<UserType>(), getNumberOfElements(),
              getMode() == UpdateMode::push ? AccessModeFlags{AccessMode::wait_for_new_data} : AccessModeFlags{}));

      if(getMode() == UpdateMode::push && getDirection().dir == VariableDirection::consuming) {
        setAppAccessorImplementation<UserType>(Application::getInstance().getTestableMode().decorate(
            impl, detail::TestableMode::DecoratorType::READ, "Constant"));
      }
      else {
        setAppAccessorImplementation<UserType>(impl);
      }
    });
  }

  /********************************************************************************************************************/

  void VariableNetworkNode::registerInModel() {
    auto* owner = getOwningModule();
    auto name = getName();

    // Since we have to try out the possible owner types via dynamic_cast, the actual code is in this lambda:
    auto addToOnwer = [&](auto& owner_casted) {
      auto model = owner_casted.getModel();
      if(!model.isValid()) {
        // this happens e.g. for default-constructed owners and their sub-modules
        return;
      }
      auto neighbourDir = model.visit(
          Model::returnDirectory, Model::getNeighbourDirectory, Model::returnFirstHit(Model::DirectoryProxy{}));

      auto dir = neighbourDir.addDirectoryRecursive(Utilities::getPathName(name));
      auto var = dir.addVariable(Utilities::getUnqualifiedName(name));

      model.addVariable(var, *this);
    };

    // Try for all possible module types
    auto* owner_am = dynamic_cast<ApplicationModule*>(owner);
    auto* owner_vg = dynamic_cast<VariableGroup*>(owner);
    if(owner_am) {
      addToOnwer(*owner_am);
    }
    else if(owner_vg) {
      addToOnwer(*owner_vg);
    }
    else {
      throw ChimeraTK::logic_error("Trying to add " + name + " to " + owner->getQualifiedName() +
          " which is neither an ApplicationModule nor a VariableGroup, but a " +
          boost::core::demangled_name(typeid(owner)));
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
