// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "ApplicationModule.h"
#include "Module.h"
#include "Utilities.h"
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

    /**
     * Change meta data (name, unit, description and optionally tags). This function may only be used on
     * Application-type nodes. If the optional argument tags is omitted, the tags will not be changed. To clear the
     * tags, an empty set can be passed.
     */
    void setMetaData(const std::optional<std::string>& name, const std::optional<std::string>& unit = {},
        const std::optional<std::string>& description = {},
        const std::optional<std::unordered_set<std::string>>& tags = {});

    /** Add a tag. Valid names for tags only contain alpha-numeric characters
     * (i.e. no spaces and no special characters). */
    void addTag(const std::string& tag) { _node.addTag(tag); }

    /** Add multiple tags. Valid names for tags only contain alpha-numeric
     * characters (i.e. no spaces and no special characters). */
    void addTags(const std::unordered_set<std::string>& tags);

    /** Return set of tags. */
    const std::unordered_set<std::string>& getTags();

    /** Convert into VariableNetworkNode */
    explicit operator VariableNetworkNode() { return _node; }
    explicit operator VariableNetworkNode() const { return _node; }

    /** Replace with other accessor */
    void replace(Derived&& other);

    /** Return the owning module */
    [[nodiscard]] EntityOwner* getOwner() const { return _node.getOwningModule(); }

    [[nodiscard]] Model::ProcessVariableProxy getModel() const { return _node.getModel(); }

   protected:
    /** Constructor, only used by child class accessors */
    InversionOfControlAccessor(Module* owner, const std::string& name, VariableDirection direction, std::string unit,
        size_t nElements, UpdateMode mode, const std::string& description, const std::type_info* valueType,
        const std::unordered_set<std::string>& tags = {});

    /** Default constructor creates a dysfunctional accessor (to be assigned with a real accessor later) */
    InversionOfControlAccessor() = default;

    /** complete the description with the full description from the owner */
    [[nodiscard]] std::string completeDescription(EntityOwner* owner, const std::string& description) const;

    /** Register the variable in the model */
    void registerInModel();

    VariableNetworkNode _node;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename Derived>
  InversionOfControlAccessor<Derived>::~InversionOfControlAccessor() {
    if(getOwner() != nullptr) {
      if(static_cast<Derived*>(this)->_impl != nullptr) {
        auto* entity = getOwner();

        if(entity != nullptr) {
          auto* owner = dynamic_cast<Module*>(entity);
          while(owner->getOwner() != nullptr) {
            owner = dynamic_cast<Module*>(owner->getOwner());
          }

          auto* application = dynamic_cast<Application*>(owner);
          assert(application != nullptr);

          if(application->getLifeCycleState() == LifeCycleState::run) {
            try {
              throw ChimeraTK::logic_error(
                  "Variable has been destroyed with active connections while application is still running");
            }
            catch(ChimeraTK::logic_error&) {
              std::terminate();
            }
          }
        }
      }
      getOwner()->unregisterAccessor(_node);
    }
    if(getModel().isValid()) {
      try {
        getModel().removeNode(_node);
      }
      catch(ChimeraTK::logic_error& e) {
        std::cerr << "ChimeraTK::logic_error caught: " << e.what() << std::endl;
        std::terminate();
      }
    }
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::setMetaData(const std::optional<std::string>& name,
      const std::optional<std::string>& unit, const std::optional<std::string>& description,
      const std::optional<std::unordered_set<std::string>>& tags) {
    std::optional<std::string> description_ = description;
    if(description.has_value()) {
      description_ = completeDescription(getOwner(), description.value());
    }
    _node.setMetaData(name, unit, description_, tags);
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::addTags(const std::unordered_set<std::string>& tags) {
    for(const auto& tag : tags) {
      _node.addTag(tag);
    }
  }

  /********************************************************************************************************************/

  template<typename Derived>
  const std::unordered_set<std::string>& InversionOfControlAccessor<Derived>::getTags() {
    return _node.getTags();
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::replace(Derived&& other) {
    if(static_cast<Derived*>(this)->_impl != nullptr || other._impl != nullptr) {
      try {
        throw ChimeraTK::logic_error(
            "Variable has been destroyed with active connections while application is still running");
      }
      catch(ChimeraTK::logic_error& ex) {
        std::terminate();
      }
    }

    // remove accessor from owning module
    if(getOwner() != nullptr) {
      getOwner()->unregisterAccessor(_node);
    }

    // remove node from model
    if(getModel().isValid()) {
      getModel().removeNode(_node);
    }

    // transfer the node
    _node = std::move(other._node);
    other._node = VariableNetworkNode(); // Make sure the destructor of other sees an invalid node

    // update the app accessor pointer in the node
    if(_node.getType() == NodeType::Application) {
      _node.setAppAccessorPointer(static_cast<Derived*>(this));
    }
    else {
      assert(_node.getType() == NodeType::invalid);
    }
    // Note: the accessor is registered by the VariableNetworkNode, so we don't have to re-register.
  }

  /********************************************************************************************************************/

  template<typename Derived>
  std::string InversionOfControlAccessor<Derived>::completeDescription(
      EntityOwner* owner, const std::string& description) const {
    auto ownerDescription = owner->getFullDescription();
    if(ownerDescription.empty()) {
      return description;
    }
    if(description.empty()) {
      return ownerDescription;
    }
    return ownerDescription + " - " + description;
  }

  /********************************************************************************************************************/

  template<typename Derived>
  InversionOfControlAccessor<Derived>::InversionOfControlAccessor(Module* owner, const std::string& name,
      VariableDirection direction, std::string unit, size_t nElements, UpdateMode mode, const std::string& description,
      const std::type_info* valueType, const std::unordered_set<std::string>& tags)
  : _node(owner, static_cast<Derived*>(this), ChimeraTK::Utilities::raiseIftrailingSlash(name, false), direction, unit,
        nElements, mode, completeDescription(owner, description), valueType, tags) {
    static_assert(std::is_base_of<InversionOfControlAccessor<Derived>, Derived>::value,
        "InversionOfControlAccessor<> must be used in a curiously recurring template pattern!");

    registerInModel();

    owner->registerAccessor(_node);
  }

  /********************************************************************************************************************/

  template<typename Derived>
  void InversionOfControlAccessor<Derived>::registerInModel() {
    _node.registerInModel();
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
