// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "HierarchyModifyingGroup.h"
#include "Module.h"
#include "VariableGroup.h"
#include "VariableNetworkNode.h"

#include <boost/smart_ptr/shared_ptr.hpp>

#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** Adds features required for inversion of control to an accessor. This is
   * needed for both the ArrayAccessor and the ScalarAccessor classes, thus it
   * uses a CRTP. */
  template<typename Derived>
  class InversionOfControlAccessor {
   public:
    /** Unregister at its owner when deleting */
    ~InversionOfControlAccessor();

    /** Change meta data (name, unit, description and optionally tags). This
     * function may only be used on Application-type nodes. If the optional
     * argument tags is omitted, the tags will not be changed. To clear the
     *  tags, an empty set can be passed. */
    void setMetaData(const std::string& name, const std::string& unit, const std::string& description);
    void setMetaData(const std::string& name, const std::string& unit, const std::string& description,
        const std::unordered_set<std::string>& tags);

    /** Add a tag. Valid names for tags only contain alpha-numeric characters
     * (i.e. no spaces and no special characters). */
    void addTag(const std::string& tag) { node.addTag(tag); }

    /** Add multiple tags. Valid names for tags only contain alpha-numeric
     * characters (i.e. no spaces and no special characters). */
    void addTags(const std::unordered_set<std::string>& tags);

    /** Convert into VariableNetworkNode */
    operator VariableNetworkNode() { return node; }
    operator const VariableNetworkNode() const { return node; }

    /** Connect with other node */
    VariableNetworkNode operator>>(const VariableNetworkNode& otherNode) { return node >> otherNode; }

    /** Replace with other accessor */
    void replace(Derived&& other);

    /** Return the owning module */
    EntityOwner* getOwner() const { return node.getOwningModule(); }

    Model::ProcessVariableProxy getModel() const { return node.getModel(); };

   protected:
    /// complete the description with the full description from the owner
    std::string completeDescription(EntityOwner* owner, const std::string& description);

    InversionOfControlAccessor(Module* owner, const std::string& name, VariableDirection direction, std::string unit,
        size_t nElements, UpdateMode mode, const std::string& description, const std::type_info* valueType,
        const std::unordered_set<std::string>& tags = {});

    /** Default constructor creates a dysfunctional accessor (to be assigned with a real accessor later) */
    InversionOfControlAccessor() = default;

    VariableNetworkNode node;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename Derived>
  InversionOfControlAccessor<Derived>::~InversionOfControlAccessor() {
    if(getOwner() != nullptr) getOwner()->unregisterAccessor(node);
    if(getModel().isValid()) {
      getModel().removeNode(node);
    }
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::setMetaData(
      const std::string& name, const std::string& unit, const std::string& description) {
    node.setMetaData(name, unit, completeDescription(getOwner(), description));
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::setMetaData(const std::string& name, const std::string& unit,
      const std::string& description, const std::unordered_set<std::string>& tags) {
    node.setMetaData(name, unit, completeDescription(getOwner(), description), tags);
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::addTags(const std::unordered_set<std::string>& tags) {
    for(const auto& tag : tags) node.addTag(tag);
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::replace(Derived&& other) {
    assert(static_cast<Derived*>(this)->_impl == nullptr && other._impl == nullptr);
    if(getModel().isValid()) {
      getModel().removeNode(node);
    }
    node = other.node; // just copies the pointer, but other will be destroyed right after this move constructor
    other.node = VariableNetworkNode();
    if(node.getType() == NodeType::Application) {
      node.setAppAccessorPointer(static_cast<Derived*>(this));
    }
    else {
      assert(node.getType() == NodeType::invalid);
    }
    // Note: the accessor is registered by the VariableNetworkNode, so we don't have to re-register.
  }

  /********************************************************************************************************************/

  template<typename Derived>
  std::string InversionOfControlAccessor<Derived>::completeDescription(
      EntityOwner* owner, const std::string& description) {
    auto ownerDescription = owner->getFullDescription();
    if(ownerDescription == "") return description;
    if(description == "") return ownerDescription;
    return ownerDescription + " - " + description;
  }

  /********************************************************************************************************************/

  template<typename Derived>
  InversionOfControlAccessor<Derived>::InversionOfControlAccessor(Module* owner, const std::string& name,
      VariableDirection direction, std::string unit, size_t nElements, UpdateMode mode, const std::string& description,
      const std::type_info* valueType, const std::unordered_set<std::string>& tags)
  : node(owner, static_cast<Derived*>(this), name, direction, unit, nElements, mode,
        completeDescription(owner, description), valueType, tags) {
    static_assert(std::is_base_of<InversionOfControlAccessor<Derived>, Derived>::value,
        "InversionOfControlAccessor<> must be used in a curiously recurring template pattern!");

    auto path = HierarchyModifyingGroup::getPathName(name);
    auto unqualName = HierarchyModifyingGroup::getUnqualifiedName(name);

    /// @todo FIXME eliminate dynamic_cast and the "lambda trick" by changing owner pointer type
    auto addToOnwer = [&](auto& owner_casted) {
      auto model = owner_casted.getModel();
      if(!model.isValid()) {
        // this happens e.g. for default-constructed owners and their sub-modules
        return;
      }
      auto neighbourDir = model.visit(
          Model::returnDirectory, Model::getNeighbourDirectory, Model::returnFirstHit(Model::DirectoryProxy{}));

      auto dir = neighbourDir.addDirectoryRecursive(ChimeraTK::HierarchyModifyingGroup::getPathName(name));
      auto var = dir.addVariable(ChimeraTK::HierarchyModifyingGroup::getUnqualifiedName(name));

      node.setModel(var);

      model.addVariable(var, node);
    };
    auto* owner_am = dynamic_cast<ApplicationModule*>(owner);
    auto* owner_vg = dynamic_cast<VariableGroup*>(owner);
    if(owner_am) {
      addToOnwer(*owner_am);
    }
    else if(owner_vg) {
      addToOnwer(*owner_vg);
    }
    else {
      throw ChimeraTK::logic_error("Hierarchy error!?");
    }

    owner->registerAccessor(node);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
