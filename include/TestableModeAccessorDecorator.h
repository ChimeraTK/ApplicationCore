// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "FeedingFanOut.h"

#include <ChimeraTK/NDRegisterAccessorDecorator.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** Decorator of the NDRegisterAccessor which facilitates tests of the
   * application */
  template<typename UserType>
  class TestableModeAccessorDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
   public:
    TestableModeAccessorDecorator(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, bool handleRead,
        bool handleWrite, size_t variableIdRead, size_t variableIdWrite);

    bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber = {}) override;

    bool doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber = {}) override;

    void doReadTransferSynchronously() override { _target->readTransfer(); }

    /** Release the testableModeLock */
    void releaseLock();

    void doPreRead(TransferType type) override;

    /** Obtain the testableModeLock if not owned yet, and decrement the counter.
     */
    void obtainLockAndDecrementCounter(bool hasNewData);

    /** Obtain the testableModeLock if not owned yet, decrement the counter, and
     * release the lock again. */
    void decrementCounter();

    void doPostRead(TransferType type, bool hasNewData) override;

   protected:
    using ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D;
    using ChimeraTK::NDRegisterAccessorDecorator<UserType>::_target;

    bool _handleRead, _handleWrite;
    size_t _variableIdRead, _variableIdWrite;
  };

  /********************************************************************************************************************/

  template<typename UserType>
  TestableModeAccessorDecorator<UserType>::TestableModeAccessorDecorator(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, bool handleRead, bool handleWrite,
      size_t variableIdRead, size_t variableIdWrite)
  : ChimeraTK::NDRegisterAccessorDecorator<UserType>(accessor), _handleRead(handleRead), _handleWrite(handleWrite),
    _variableIdRead(variableIdRead), _variableIdWrite(variableIdWrite) {
    assert(_variableIdRead != 0);
    assert(_variableIdWrite != 0);

    // if receiving end, register for testable mode (stall detection)
    if(this->isReadable() && handleRead) {
      Application::getInstance().testableMode_processVars[_variableIdRead] = accessor;
      assert(accessor->getAccessModeFlags().has(AccessMode::wait_for_new_data));
    }

    // if this decorating a bidirectional process variable, set the
    // valueRejectCallback
    auto bidir = boost::dynamic_pointer_cast<BidirectionalProcessArray<UserType>>(accessor);
    if(bidir) {
      bidir->setValueRejectCallback([this] { decrementCounter(); });
    }
    else {
      assert(!(handleRead && handleWrite));
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool TestableModeAccessorDecorator<UserType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber) {
    if(!_handleWrite) return _target->writeTransfer(versionNumber);

    bool dataLost = false;
    if(!Application::testableModeTestLock()) {
      // may happen if first write in thread is done before first blocking read
      Application::testableModeLock("write " + this->getName());
    }
    dataLost = _target->writeTransfer(versionNumber);
    if(!dataLost) {
      ++Application::getInstance().testableMode_counter;
      ++Application::getInstance().testableMode_perVarCounter[_variableIdWrite];
      if(Application::getInstance().enableDebugTestableMode) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode_counter increased, now at value "
                  << Application::getInstance().testableMode_counter << std::endl;
      }
    }
    else {
      if(Application::getInstance().enableDebugTestableMode) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode_counter not increased due to lost data" << std::endl;
      }
    }
    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool TestableModeAccessorDecorator<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    if(!_handleWrite) return _target->writeTransferDestructively(versionNumber);

    bool dataLost = false;
    if(!Application::testableModeTestLock()) {
      // may happen if first write in thread is done before first blocking read
      Application::testableModeLock("write " + this->getName());
    }
    dataLost = _target->writeTransferDestructively(versionNumber);
    if(!dataLost) {
      ++Application::getInstance().testableMode_counter;
      ++Application::getInstance().testableMode_perVarCounter[_variableIdWrite];
      if(Application::getInstance().enableDebugTestableMode) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode_counter increased, now at value "
                  << Application::getInstance().testableMode_counter << std::endl;
      }
    }
    else {
      if(Application::getInstance().enableDebugTestableMode) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode_counter not increased due to lost data" << std::endl;
      }
    }
    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableModeAccessorDecorator<UserType>::releaseLock() {
    if(Application::testableModeTestLock()) Application::testableModeUnlock("doReadTransfer " + this->getName());
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableModeAccessorDecorator<UserType>::doPreRead(TransferType type) {
    _target->preRead(type);

    // Blocking reads have to release the lock so the data transport can happen
    if(_handleRead && type == TransferType::read &&
        TransferElement::_accessModeFlags.has(AccessMode::wait_for_new_data)) {
      releaseLock();
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableModeAccessorDecorator<UserType>::obtainLockAndDecrementCounter(bool hasNewData) {
    if(!Application::testableModeTestLock()) Application::testableModeLock("doReadTransfer " + this->getName());
    if(!hasNewData) return;
    if(Application::getInstance().testableMode_perVarCounter[_variableIdRead] > 0) {
      assert(Application::getInstance().testableMode_counter > 0);
      --Application::getInstance().testableMode_counter;
      --Application::getInstance().testableMode_perVarCounter[_variableIdRead];
      if(Application::getInstance().enableDebugTestableMode) {
        std::cout << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
                  << "]: testableMode_counter decreased, now at value "
                  << Application::getInstance().testableMode_counter << " / "
                  << Application::getInstance().testableMode_perVarCounter[_variableIdRead] << std::endl;
      }
    }
    else {
      if(Application::getInstance().enableDebugTestableMode) {
        std::cout << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
                  << "]: testableMode_counter NOT decreased, was already at value "
                  << Application::getInstance().testableMode_counter << " / "
                  << Application::getInstance().testableMode_perVarCounter[_variableIdRead] << std::endl;
        std::cout << Application::getInstance().testableMode_names[_variableIdRead] << std::endl;
      }
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableModeAccessorDecorator<UserType>::decrementCounter() {
    obtainLockAndDecrementCounter(true);
    releaseLock();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableModeAccessorDecorator<UserType>::doPostRead(TransferType type, bool hasNewData) {
    if(_handleRead) obtainLockAndDecrementCounter(hasNewData);
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostRead(type, hasNewData);
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
