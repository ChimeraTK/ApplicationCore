// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "RecoveryHelper.h"

#include <ChimeraTK/Exception.h>
#include <ChimeraTK/NDRegisterAccessorDecorator.h>

namespace ChimeraTK {

  template<typename UserType>
  class ReverseRecoveryDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
   public:
    ReverseRecoveryDecorator(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor,
        boost::shared_ptr<RecoveryHelper> recoveryHelper);

    void doPreRead(TransferType) override;
    void doPostRead(TransferType, bool updateBuffer) override;

    void interrupt() override;

    void setInReadAnyGroup(ReadAnyGroup* rag) override;

   protected:
    boost::shared_ptr<RecoveryHelper> _recoveryHelper;
    using ChimeraTK::NDRegisterAccessorDecorator<UserType>::_target;
  };

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ReverseRecoveryDecorator);
} // namespace ChimeraTK
