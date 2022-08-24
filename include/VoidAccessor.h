// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "InversionOfControlAccessor.h"

#include <ChimeraTK/VoidRegisterAccessor.h>

#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** Accessor for void variables (i.e. no data, just the "trigger" information). Note for users: Use the
   * convenience classes VoidInput and VoidOutput instead of this class directly. */
  class VoidAccessor : public ChimeraTK::VoidRegisterAccessor, public InversionOfControlAccessor<VoidAccessor> {
   public:
    using InversionOfControlAccessor<VoidAccessor>::operator VariableNetworkNode;
    using InversionOfControlAccessor<VoidAccessor>::operator>>;
    void replace(const ChimeraTK::NDRegisterAccessorAbstractor<ChimeraTK::Void>& newAccessor) = delete;
    using InversionOfControlAccessor<VoidAccessor>::replace;
    VoidAccessor& operator=(VoidAccessor& other) = delete;
    using ChimeraTK::VoidRegisterAccessor::operator=;

    /** Move constructor */
    VoidAccessor(VoidAccessor&& other) noexcept { InversionOfControlAccessor<VoidAccessor>::replace(std::move(other)); }

    /** Move assignment. */
    VoidAccessor& operator=(VoidAccessor&& other) noexcept {
      // Having a move-assignment operator is required to use the move-assignment
      // operator of a module containing an accessor.
      InversionOfControlAccessor<VoidAccessor>::replace(std::move(other));
      return *this;
    }

    bool write(ChimeraTK::VersionNumber versionNumber) = delete;
    bool writeDestructively(ChimeraTK::VersionNumber versionNumber) = delete;
    // void writeIfDifferent(UserType newValue, VersionNumber versionNumber) = delete;

    bool write() {
      auto versionNumber = this->getOwner()->getCurrentVersionNumber();
      bool dataLoss = ChimeraTK::VoidRegisterAccessor::write(versionNumber);
      if(dataLoss) Application::incrementDataLossCounter(this->node.getQualifiedName());
      return dataLoss;
    }

    bool writeDestructively() {
      auto versionNumber = this->getOwner()->getCurrentVersionNumber();
      bool dataLoss = ChimeraTK::VoidRegisterAccessor::writeDestructively(versionNumber);
      if(dataLoss) Application::incrementDataLossCounter(this->node.getQualifiedName());
      return dataLoss;
    }

   protected:
    friend class InversionOfControlAccessor<VoidAccessor>;

    VoidAccessor(Module* owner, const std::string& name, VariableDirection direction, std::string& unit,
        UpdateMode mode, const std::string& description, const std::unordered_set<std::string>& tags = {})
    : InversionOfControlAccessor<VoidAccessor>(
          owner, name, direction, unit, 1, mode, description, &typeid(ChimeraTK::Void), tags) {}

    /** Default constructor creates a dysfunctional accessor (to be assigned with a real accessor later) */
    VoidAccessor() = default;
  };

  /********************************************************************************************************************/

  /** Convenience class for input accessors. For Void there is only UpdateMode::push */
  struct VoidInput : public VoidAccessor {
    VoidInput(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : VoidAccessor(owner, name, {VariableDirection::consuming, false}, unit, UpdateMode::push, description, tags) {}
    VoidInput() = default;
    using VoidAccessor::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for output void (always UpdateMode::push) */
  struct VoidOutput : public VoidAccessor {
    VoidOutput(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : VoidAccessor(owner, name, {VariableDirection::feeding, false}, unit, UpdateMode::push, description, tags) {}
    VoidOutput() = default;
    using VoidAccessor::operator=;
  };

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
