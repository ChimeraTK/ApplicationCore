// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ReverseRecoveryDecorator.h"

namespace ChimeraTK {
  /********************************************************************************************************************/

  template<typename UserType>
  ReverseRecoveryDecorator<UserType>::ReverseRecoveryDecorator(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, const VariableNetworkNode& networkNode)
  : ExceptionHandlingDecorator<UserType>(std::move(accessor), networkNode) {
    // Check if we are wrapping a push-type variable and forbid that
    if(TransferElement::getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
      throw ChimeraTK::logic_error("Cannot use reverse recovery on push-type input");
    }

    // Set ourselves as wfnd:
    TransferElement::_accessModeFlags.add(AccessMode::wait_for_new_data);

    _recoveryHelper->notificationQueue = cppext::future_queue<void>(3);
    _recoveryHelper->recoveryDirection = RecoveryHelper::Direction::fromDevice;

    // Set the read queue as continuation of the notification queue
    // The continuation will just trigger a readTransfer on the target accessor
    // pre/post will have been/be called by the decorator
    this->_readQueue = _recoveryHelper->notificationQueue.template then<void>(
        [&, this]() {
          assert(_target->isReadTransactionInProgress());
          _target->readTransfer();
        },
        std::launch::deferred);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ReverseRecoveryDecorator<UserType>::interrupt() {
    this->interrupt_impl(this->_recoveryHelper->notificationQueue);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ReverseRecoveryDecorator<UserType>::setInReadAnyGroup(ReadAnyGroup* rag) {
    // Skip flagging our target as being in a ReadAnyGroup (it isn't since we replace the readQueue with our own)
    // NOLINTNEXTLINE(bugprone-parent-virtual-call)
    NDRegisterAccessor<UserType>::setInReadAnyGroup(rag);
  }

  /********************************************************************************************************************/

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ReverseRecoveryDecorator);
} // namespace ChimeraTK
