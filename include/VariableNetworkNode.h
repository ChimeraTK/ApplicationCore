// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ConstantAccessor.h"
#include "Flags.h"
#include "MetaDataPropagatingRegisterDecorator.h"
#include "Model.h"
#include "Visitor.h"
#include <unordered_map>
#include <unordered_set>

#include <ChimeraTK/NDRegisterAccessorAbstractor.h>

#include <boost/shared_ptr.hpp>

#include <cassert>
#include <iostream>

namespace ChimeraTK {

  /********************************************************************************************************************/

  class AccessorBase;
  class EntityOwner;
  struct VariableNetworkNode_data;

  /********************************************************************************************************************/

  /** Pseudo type to identify nodes which can have arbitrary types */
  class AnyType {};

  /********************************************************************************************************************/

  /** Class describing a node of a variable network */
  class VariableNetworkNode {
   public:
    /** Copy-constructor: Just copy the pointer to the data storage object */
    VariableNetworkNode(const VariableNetworkNode& other);

    /** Copy by assignment operator: Just copy the pointer to the data storage object */
    VariableNetworkNode& operator=(const VariableNetworkNode& rightHandSide);

    /** Constructor for an Application node */
    VariableNetworkNode(EntityOwner* owner, ChimeraTK::TransferElementAbstractor* accessorBridge,
        const std::string& name, VariableDirection direction, std::string unit, size_t nElements, UpdateMode mode,
        const std::string& description, const std::type_info* valueType,
        const std::unordered_set<std::string>& tags = {});

    /** Constructor for a Device node */
    VariableNetworkNode(const std::string& name, const std::string& devAlias, const std::string& regName,
        UpdateMode mode, VariableDirection direction, const std::type_info& valTyp = typeid(AnyType),
        size_t nElements = 0);

    /** Constructor for a ControlSystem node */
    VariableNetworkNode(const std::string& pubName, VariableDirection direction,
        const std::type_info& valTyp = typeid(AnyType), size_t nElements = 0);

    /** Constructor for a constant accessor with zero value */
    VariableNetworkNode(const std::type_info* valTyp, bool makeFeeder, size_t length);

    /**
     * Constructor for a TriggerReceiver node triggering the data transfer of another network. The additional dummy
     * argument is only there to discriminate the signature from the copy constructor and will be ignored.
     */
    VariableNetworkNode(const std::string& deviceAliasOrCdd, int);

    /** Constructor to wrap a VariableNetworkNode_data pointer */
    explicit VariableNetworkNode(boost::shared_ptr<VariableNetworkNode_data> pdata);

    /** Default constructor for an invalid node */
    VariableNetworkNode();

    /** Change meta data (name, unit, description and optionally tags). This
     * function may only be used on Application-type nodes. If the optional
     * argument tags is omitted, the tags will not be changed. To clear the
     *  tags, an empty set can be passed. */
    void setMetaData(const std::string& name, const std::string& unit, const std::string& description) const;
    void setMetaData(const std::string& name, const std::string& unit, const std::string& description,
        const std::unordered_set<std::string>& tags) const;

    /** Clear the owner network of this node. */
    void clearOwner();

    /** Set the value type for this node. Only possible of the current value type
     * is undecided (i.e. AnyType). */
    void setValueType(const std::type_info& newType) const;

    /** Set the direction for this node. Only possible if current direction is
     * VariableDirection::feeding and the node type is NodeType::ControlSystem. */
    void setDirection(VariableDirection newDirection) const;

    /** Function checking if the node requires a fixed implementation */
    [[nodiscard]] bool hasImplementation() const;

    /** Compare two nodes */
    bool operator==(const VariableNetworkNode& other) const;
    bool operator!=(const VariableNetworkNode& other) const;
    bool operator<(const VariableNetworkNode& other) const;

    /** Print node information to std::cout */
    void dump(std::ostream& stream = std::cout) const;

    /** Add a tag. This function may only be used on Application-type nodes. Valid
     * names for tags only contain
     *  alpha-numeric characters (i.e. no spaces and no special characters). @todo
     * enforce this!*/
    void addTag(const std::string& tag) const;

    /** Returns true if a circular dependency has been detected and the node is a consumer. */
    [[nodiscard]] bool isCircularInput() const;

    /** Scan the networks and set the isCircularInput() flags if circular dependencies are detected.
     *  Must only be called on consuming nodes.
     */
    [[nodiscard]] std::list<EntityOwner*> scanForCircularDepencency() const;

    /** Get the unique ID of the circular network. It is 0 if the node is not part of a circular network.*/
    [[nodiscard]] size_t getCircularNetworkHash() const;

    [[nodiscard]] bool isValid() const { return pdata && getType() != NodeType::invalid; }

    /** Getter for the properties */
    [[nodiscard]] NodeType getType() const;
    [[nodiscard]] UpdateMode getMode() const;
    [[nodiscard]] VariableDirection getDirection() const;
    [[nodiscard]] const std::type_info& getValueType() const;
    [[nodiscard]] std::string getName() const;
    [[nodiscard]] std::string getQualifiedName() const;
    [[nodiscard]] const std::string& getUnit() const;
    [[nodiscard]] const std::string& getDescription() const;
    [[nodiscard]] VariableNetworkNode getNodeToTrigger() const;
    [[nodiscard]] const std::string& getPublicName() const;
    [[nodiscard]] const std::string& getDeviceAlias() const;
    [[nodiscard]] const std::string& getRegisterName() const;
    [[nodiscard]] const std::unordered_set<std::string>& getTags() const;
    void setNumberOfElements(size_t nElements) const;
    [[nodiscard]] size_t getNumberOfElements() const;
    [[nodiscard]] ChimeraTK::TransferElementAbstractor& getAppAccessorNoType() const;

    [[nodiscard]] Model::ProcessVariableProxy getModel() const;

    void setModel(Model::ProcessVariableProxy model) const;

    void setPublicName(const std::string& name) const;

    template<typename UserType>
    ChimeraTK::NDRegisterAccessorAbstractor<UserType>& getAppAccessor() const;

    template<typename UserType>
    void setAppAccessorImplementation(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> impl) const;

    void setAppAccessorConstImplementation(const VariableNetworkNode& feeder) const;

    /** Return the unique ID of this node (will change every time the application
     * is started). */
    [[nodiscard]] const void* getUniqueId() const { return pdata.get(); }

    /** Change pointer to the accessor. May only be used for application nodes. */
    void setAppAccessorPointer(ChimeraTK::TransferElementAbstractor* accessor) const;

    [[nodiscard]] EntityOwner* getOwningModule() const;

    void setOwningModule(EntityOwner* newOwner) const;

    void accept(Visitor<VariableNetworkNode>& visitor) const;

    template<typename UserType>
    void setConstantValue(UserType value);

    template<typename UserType>
    UserType getConstantValue() const;

    // protected:  @todo make protected again (with proper interface extension)

    boost::shared_ptr<VariableNetworkNode_data> pdata;
  };

  /********************************************************************************************************************/

  /** We use a pimpl pattern so copied instances of VariableNetworkNode refer to
   * the same instance of the data structure and thus stay consistent all the
   * time. */
  struct VariableNetworkNode_data {
    VariableNetworkNode_data() = default;

    /** Type of the node (Application, Device, ControlSystem, Trigger) */
    NodeType type{NodeType::invalid};

    /** Update mode: poll or push */
    UpdateMode mode{UpdateMode::invalid};

    /** Node direction: feeding or consuming */
    VariableDirection direction{VariableDirection::invalid, false};

    /** Value type of this node. If the type_info is the typeid of AnyType, the
     * actual type can be decided when making the connections. */
    const std::type_info* valueType{&typeid(AnyType)};

    /** Engineering unit. If equal to ChimeraTK::TransferElement::unitNotSet, no
     * unit has been defined (and any unit is allowed). */
    std::string unit{ChimeraTK::TransferElement::unitNotSet};

    /** Description */
    std::string description;

    /** Pointer to implementation if type == Application */
    ChimeraTK::TransferElementAbstractor* appNode{nullptr};

    /** Pointer to network which should be triggered by this node */
    VariableNetworkNode nodeToTrigger{nullptr};

    /** Pointer to the network providing the external trigger. May only be used
     * for feeding nodes with an update mode poll. When enabled, the update mode
     * will be converted into push. */
    VariableNetworkNode externalTrigger{nullptr};

    /** Public name if type == ControlSystem */
    std::string publicName;

    /** Accessor name if type == Application */
    std::string name;
    std::string qualifiedName;

    /** Device information if type == Device */
    std::string deviceAlias;
    std::string registerName;

    /** Number of elements in the variable. 0 means not yet decided. */
    size_t nElements{0};

    /** Set of tags  if type == Application */
    std::unordered_set<std::string> tags;

    /** Map to store triggered versions of this node. The map key is the trigger
     * node and the value is the node with the respective trigger added. */
    std::map<VariableNetworkNode, VariableNetworkNode> nodeWithTrigger;

    /** Pointer to the module owning this node */
    EntityOwner* owningModule{nullptr};

    /** Hash which idientifies a circular network. 0 if the node is not part if a circular dependency. */
    size_t circularNetworkHash{0};

    /** Model representation of this variable */
    Model::ProcessVariableProxy model;

    /** Value in case of a constant */
    userTypeMap constantValue;
  };

  /********************************************************************************************************************/
  /*** Implementations ************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ChimeraTK::NDRegisterAccessorAbstractor<UserType>& VariableNetworkNode::getAppAccessor() const {
    assert(typeid(UserType) == getValueType());
    assert(pdata->type == NodeType::Application);
    auto accessor = static_cast<ChimeraTK::NDRegisterAccessorAbstractor<UserType>*>(pdata->appNode);
    assert(accessor != nullptr);
    return *accessor;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void VariableNetworkNode::setAppAccessorImplementation(boost::shared_ptr<NDRegisterAccessor<UserType>> impl) const {
    auto decorated = boost::make_shared<MetaDataPropagatingRegisterDecorator<UserType>>(impl, getOwningModule());
    getAppAccessor<UserType>().replace(decorated);
    auto flagProvider = boost::dynamic_pointer_cast<MetaDataPropagationFlagProvider>(decorated);
    assert(flagProvider);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void VariableNetworkNode::setConstantValue(UserType value) {
    assert(pdata->type == NodeType::Constant);
    boost::fusion::at_key<UserType>(pdata->constantValue) = value;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  UserType VariableNetworkNode::getConstantValue() const {
    assert(pdata->type == NodeType::Constant);
    return boost::fusion::at_key<UserType>(pdata->constantValue);
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
