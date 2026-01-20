// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "InversionOfControlAccessor.h"

#include <ChimeraTK/OneDRegisterAccessor.h>
#include <ChimeraTK/SystemTags.h>

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** Accessor for array variables (i.e. vectors). Note for users: Use the
   * convenience classes ArrayPollInput, ArrayPushInput, ArrayOutput instead of
   * this class directly. */
  template<typename UserType>
  class ArrayAccessor : public ChimeraTK::OneDRegisterAccessor<UserType>,
                        public InversionOfControlAccessor<ArrayAccessor<UserType>> {
   public:
    using InversionOfControlAccessor<ArrayAccessor<UserType>>::operator VariableNetworkNode;
    void replace(const ChimeraTK::NDRegisterAccessorAbstractor<UserType>& newAccessor) = delete;
    using InversionOfControlAccessor<ArrayAccessor<UserType>>::replace;
    ArrayAccessor<UserType>& operator=(ArrayAccessor<UserType>& other) = delete;
    using ChimeraTK::OneDRegisterAccessor<UserType>::operator=;

    /** Move constructor */
    ArrayAccessor(ArrayAccessor<UserType>&& other) noexcept;

    /** Move assignment */
    ArrayAccessor<UserType>& operator=(ArrayAccessor<UserType>&& other) noexcept;

    bool write(ChimeraTK::VersionNumber versionNumber) = delete;
    bool writeDestructively(ChimeraTK::VersionNumber versionNumber) = delete;
    void writeIfDifferent(
        const std::vector<UserType>& newValue, VersionNumber versionNumber, DataValidity validity) = delete;
    void setAndWrite(const std::vector<UserType>& newValue, VersionNumber versionNumber) = delete;

    bool write();

    bool writeDestructively();

    void writeIfDifferent(const std::vector<UserType>& newValue);

    void setAndWrite(const std::vector<UserType>& newValue);

    using value_type = UserType;

    ~ArrayAccessor() { InversionOfControlAccessor<ArrayAccessor<UserType>>::deinit(); }

   protected:
    friend class InversionOfControlAccessor<ArrayAccessor<UserType>>;

    ArrayAccessor(Module* owner, const std::string& name, VariableDirection direction, std::string unit,
        size_t nElements, UpdateMode mode, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    /** Default constructor creates a dysfunctional accessor (to be assigned with a real accessor later) */
    ArrayAccessor() = default;
  };

  /********************************************************************************************************************/

  /** Convenience class for input array accessors with UpdateMode::push */
  template<typename UserType>
  class ArrayPushInput : public ArrayAccessor<UserType> {
   public:
    ArrayPushInput(Module* owner, const std::string& name, std::string unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tags = {});
    ArrayPushInput() = default;
    using ArrayAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for input array accessors with UpdateMode::poll */
  template<typename UserType>
  class ArrayPollInput : public ArrayAccessor<UserType> {
   public:
    ArrayPollInput(Module* owner, const std::string& name, std::string unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tags = {});
    ArrayPollInput() = default;
    void read() { this->readLatest(); }
    using ArrayAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for output array accessors (always UpdateMode::push) */
  template<typename UserType>
  class ArrayOutput : public ArrayAccessor<UserType> {
   public:
    ArrayOutput(Module* owner, const std::string& name, std::string unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tags = {});
    ArrayOutput() = default;
    using ArrayAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for input array accessors with return channel ("write
   * back") and UpdateMode::push */
  template<typename UserType>
  class ArrayPushInputWB : public ArrayAccessor<UserType> {
   public:
    ArrayPushInputWB(Module* owner, const std::string& name, std::string unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tags = {});
    ArrayPushInputWB() = default;
    using ArrayAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for output array accessors with return channel ("read
   * back") (always UpdateMode::push) */
  template<typename UserType>
  class ArrayOutputPushRB : public ArrayAccessor<UserType> {
   public:
    ArrayOutputPushRB(Module* owner, const std::string& name, std::string unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tags = {});
    ArrayOutputPushRB() = default;
    using ArrayAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Deprecated, do not use. Use ArrayOutputPushRB instead (works identically). */
  template<typename UserType>
  class ArrayOutputRB : public ArrayAccessor<UserType> {
   public:
    [[deprecated]] ArrayOutputRB(Module* owner, const std::string& name, std::string unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tags = {});
    ArrayOutputRB() = default;
    using ArrayAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  template<typename UserType>
  class ArrayOutputReverseRecovery : public ArrayAccessor<UserType> {
   public:
    ArrayOutputReverseRecovery(Module* owner, const std::string& name, std::string unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tags = {});
    ArrayOutputReverseRecovery() = default;
    using ArrayAccessor<UserType>::operator=;
  };
  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /* Implementations below this point                                                                                 */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayAccessor<UserType>::ArrayAccessor(ArrayAccessor<UserType>&& other) noexcept {
    InversionOfControlAccessor<ArrayAccessor<UserType>>::replace(std::move(other));
  }

  /********************************************************************************************************************/

  template<typename UserType>
  ArrayAccessor<UserType>& ArrayAccessor<UserType>::operator=(ArrayAccessor<UserType>&& other) noexcept {
    // Having a move-assignment operator is required to use the move-assignment
    // operator of a module containing an accessor.
    InversionOfControlAccessor<ArrayAccessor<UserType>>::replace(std::move(other));
    return *this;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool ArrayAccessor<UserType>::write() {
    auto versionNumber = this->getOwner()->getCurrentVersionNumber();
    bool dataLoss = ChimeraTK::OneDRegisterAccessor<UserType>::write(versionNumber);
    if(dataLoss) {
      Application::incrementDataLossCounter(this->_node.getQualifiedName());
    }
    return dataLoss;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool ArrayAccessor<UserType>::writeDestructively() {
    auto versionNumber = this->getOwner()->getCurrentVersionNumber();
    bool dataLoss = ChimeraTK::OneDRegisterAccessor<UserType>::writeDestructively(versionNumber);
    if(dataLoss) {
      Application::incrementDataLossCounter(this->_node.getQualifiedName());
    }
    return dataLoss;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ArrayAccessor<UserType>::writeIfDifferent(const std::vector<UserType>& newValue) {
    if(!std::equal(newValue.begin(), newValue.end(), this->get()->accessChannel(0).begin()) ||
        this->template checkMetadataWriteDifference<UserType>()) {
      setAndWrite(newValue);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ArrayAccessor<UserType>::setAndWrite(const std::vector<UserType>& newValue) {
    operator=(newValue);
    this->write();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  ArrayAccessor<UserType>::ArrayAccessor(Module* owner, const std::string& name, VariableDirection direction,
      std::string unit, size_t nElements, UpdateMode mode, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : InversionOfControlAccessor<ArrayAccessor<UserType>>(
        owner, name, direction, unit, nElements, mode, description, &typeid(UserType), tags) {
    InversionOfControlAccessor<ArrayAccessor<UserType>>::init();
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayPushInput<UserType>::ArrayPushInput(Module* owner, const std::string& name, std::string unit, size_t nElements,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ArrayAccessor<UserType>(
        owner, name, {VariableDirection::consuming, false}, unit, nElements, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayPollInput<UserType>::ArrayPollInput(Module* owner, const std::string& name, std::string unit, size_t nElements,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ArrayAccessor<UserType>(
        owner, name, {VariableDirection::consuming, false}, unit, nElements, UpdateMode::poll, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayOutput<UserType>::ArrayOutput(Module* owner, const std::string& name, std::string unit, size_t nElements,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ArrayAccessor<UserType>(
        owner, name, {VariableDirection::feeding, false}, unit, nElements, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayPushInputWB<UserType>::ArrayPushInputWB(Module* owner, const std::string& name, std::string unit,
      size_t nElements, const std::string& description, const std::unordered_set<std::string>& tags)
  : ArrayAccessor<UserType>(
        owner, name, {VariableDirection::consuming, true}, unit, nElements, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayOutputPushRB<UserType>::ArrayOutputPushRB(Module* owner, const std::string& name, std::string unit,
      size_t nElements, const std::string& description, const std::unordered_set<std::string>& tags)
  : ArrayAccessor<UserType>(
        owner, name, {VariableDirection::feeding, true}, unit, nElements, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayOutputRB<UserType>::ArrayOutputRB(Module* owner, const std::string& name, std::string unit, size_t nElements,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ArrayAccessor<UserType>(
        owner, name, {VariableDirection::feeding, true}, unit, nElements, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ArrayOutputReverseRecovery<UserType>::ArrayOutputReverseRecovery(Module* owner, const std::string& name,
      std::string unit, size_t nElements, const std::string& description, const std::unordered_set<std::string>& tags)
  : ArrayAccessor<UserType>(
        owner, name, {VariableDirection::feeding, true}, unit, nElements, UpdateMode::push, description, tags) {
    this->addTag(ChimeraTK::SystemTags::reverseRecovery);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

} /* namespace ChimeraTK */
