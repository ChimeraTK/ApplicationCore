// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ReverseRecoveryDecorator.h"

namespace ChimeraTK {
  /*********************************************************************************************************************/

  template<typename UserType>
  ReverseRecoveryDecorator<UserType>::ReverseRecoveryDecorator(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor,
      boost::shared_ptr<RecoveryHelper> recoveryHelper)
  : ChimeraTK::NDRegisterAccessorDecorator<UserType>(std::move(accessor)), _recoveryHelper(std::move(recoveryHelper)) {
    // Check if we are wrapping a push-type variable and forbid that
    if(TransferElement::getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
      throw ChimeraTK::logic_error("Cannot use reverse recovery on push-type input");
    }

    // Set ourselves as wfnd:
    TransferElement::_accessModeFlags.add(AccessMode::wait_for_new_data);

    _recoveryHelper->notificationQueue = cppext::future_queue<void>(3);
    _recoveryHelper->recoveryDirection = RecoveryHelper::Direction::fromDevice;

    // Set the read queue as continuation of the notification queue
    this->_readQueue =
        _recoveryHelper->notificationQueue.template then<void>([&, this]() { _target->read(); }, std::launch::deferred);
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  void ReverseRecoveryDecorator<UserType>::interrupt() {
    this->interrupt_impl(this->_recoveryHelper->notificationQueue);
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  void ReverseRecoveryDecorator<UserType>::setInReadAnyGroup(ReadAnyGroup* rag) {
    // Skip flagging our target as being in a ReadAnyGroup (it isn't since we replace the readQueue with our own)
    // NOLINTNEXTLINE(bugprone-parent-virtual-call)
    NDRegisterAccessor<UserType>::setInReadAnyGroup(rag);
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  void ReverseRecoveryDecorator<UserType>::doPreRead(TransferType) {}

  /********************************************************************************************************************/

  template<typename UserType>
  void ReverseRecoveryDecorator<UserType>::doPostRead(TransferType, bool updateBuffer) {
    // Do the same as NDRegisterAccessorDecorator::doPostRead() but without delegating to the target. We must
    // not delegate, because we did not call preRead() and the entire operation is executed inside the
    // continuation of the readQueue (see constructor implementation).
    _target->setActiveException(this->_activeException);

    // Decorators have to copy meta data even if updateDataBuffer is false
    this->_dataValidity = _target->dataValidity();
    this->_versionNumber = _target->getVersionNumber();

    if(!updateBuffer) {
      return;
    }

    for(size_t i = 0; i < _target->getNumberOfChannels(); ++i) {
      this->buffer_2D[i].swap(_target->accessChannel(i));
    }
  }

  /********************************************************************************************************************/

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ReverseRecoveryDecorator);
} // namespace ChimeraTK
