// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "RecoveryHelper.h"

#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/NDRegisterAccessorDecorator.h>

#include <boost/smart_ptr/shared_ptr.hpp>

namespace ChimeraTK {

  /** Decorator of the NDRegisterAccessor which facilitates tests of the
   * application */
  template<typename UserType>
  class ExceptionHandlingDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
   public:
    /**
     * Decorate the accessors which is handed in the constructor.
     * All information to get the DeviceModule and to create a recovery accessor are
     * taken from the VariableNetworkNode.
     */
    ExceptionHandlingDecorator(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, const VariableNetworkNode& networkNode);

    void doPreWrite(TransferType type, VersionNumber versionNumber) override;

    void doPostWrite(TransferType type, VersionNumber versionNumber) override;

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreRead(TransferType type) override;

    bool doWriteTransfer(VersionNumber versionNumber) override;
    bool doWriteTransferDestructively(VersionNumber versionNumber) override;

   protected:
    using ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D;
    using ChimeraTK::NDRegisterAccessorDecorator<UserType>::_target;
    using ChimeraTK::TransferElement::_versionNumber;
    using ChimeraTK::TransferElement::_dataValidity;
    using ChimeraTK::TransferElement::_activeException;

    boost::weak_ptr<DeviceManager> _deviceManager;

    bool _previousReadFailed{true};

    boost::shared_ptr<RecoveryHelper> _recoveryHelper{nullptr};
    // store the recoveryAccessor separately. The RecoveryHelper only contains a pointer to TransferElement and can't be
    // used to fill in data.
    boost::shared_ptr<NDRegisterAccessor<UserType>> _recoveryAccessor{nullptr};

    VariableDirection _direction;

    // We have to throw in read transfers because the outermost TransferElement has to see the exception
    bool _hasThrownToInhibitTransfer{false};
    // For writing we must not throw. The overridden doWriteTransfer() must return the correct data loss flag.
    bool _inhibitWriteTransfer{false};
    bool _hasThrownLogicError{false};
    bool _dataLostInPreviousWrite{false};
    bool _hasReportedException{false}; // valid only with wait_forNewData

    template<typename Callable>
    bool genericWriteWrapper(Callable writeFunction);
  };

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ExceptionHandlingDecorator);

} /* namespace ChimeraTK */
