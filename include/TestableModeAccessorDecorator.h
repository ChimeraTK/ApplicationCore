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
      Application::getInstance().getTestableMode().variables[_variableIdRead].processVariable = accessor;
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
    if(!Application::getInstance().getTestableMode().testLock()) {
      // may happen if first write in thread is done before first blocking read
      Application::getInstance().getTestableMode().lock("write " + this->getName());
    }
    dataLost = _target->writeTransfer(versionNumber);
    if(!dataLost) {
      ++Application::getInstance().getTestableMode().counter;
      ++Application::getInstance().getTestableMode().variables[_variableIdWrite].counter;
      if(Application::getInstance().getTestableMode().enableDebug) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode.counter increased, now at value "
                  << Application::getInstance().getTestableMode().counter << std::endl;
      }
    }
    else {
      if(Application::getInstance().getTestableMode().enableDebug) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode.counter not increased due to lost data" << std::endl;
      }
    }
    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool TestableModeAccessorDecorator<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    if(!_handleWrite) return _target->writeTransferDestructively(versionNumber);

    bool dataLost = false;
    if(!Application::getInstance().getTestableMode().testLock()) {
      // may happen if first write in thread is done before first blocking read
      Application::getInstance().getTestableMode().lock("write " + this->getName());
    }
    dataLost = _target->writeTransferDestructively(versionNumber);
    if(!dataLost) {
      ++Application::getInstance().getTestableMode().counter;
      ++Application::getInstance().getTestableMode().variables[_variableIdWrite].counter;
      if(Application::getInstance().getTestableMode().enableDebug) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode.counter increased, now at value "
                  << Application::getInstance().getTestableMode().counter << std::endl;
      }
    }
    else {
      if(Application::getInstance().getTestableMode().enableDebug) {
        std::cout << "TestableModeAccessorDecorator::write[name='" << this->getName() << "', id=" << _variableIdWrite
                  << "]: testableMode.counter not increased due to lost data" << std::endl;
      }
    }
    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void TestableModeAccessorDecorator<UserType>::releaseLock() {
    if(Application::getInstance().getTestableMode().testLock())
      Application::getInstance().getTestableMode().unlock("doReadTransfer " + this->getName());
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
    if(!Application::getInstance().getTestableMode().testLock())
      Application::getInstance().getTestableMode().lock("doReadTransfer " + this->getName());
    if(!hasNewData) return;
    auto& variable = Application::getInstance().getTestableMode().variables[_variableIdRead];
    if(variable.counter > 0) {
      assert(Application::getInstance().getTestableMode().counter > 0);
      --Application::getInstance().getTestableMode().counter;
      --variable.counter;
      if(Application::getInstance().getTestableMode().enableDebug) {
        std::cout << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
                  << "]: testableMode.counter decreased, now at value "
                  << Application::getInstance().getTestableMode().counter << " / " << variable.counter << std::endl;
      }
    }
    else {
      if(Application::getInstance().getTestableMode().enableDebug) {
        std::cout << "TestableModeAccessorDecorator[name='" << this->getName() << "', id=" << _variableIdRead
                  << "]: testableMode.counter NOT decreased, was already at value "
                  << Application::getInstance().getTestableMode().counter << " / " << variable.counter << std::endl;
        std::cout << variable.name << std::endl;
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
