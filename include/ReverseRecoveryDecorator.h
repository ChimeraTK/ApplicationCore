// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ExceptionHandlingDecorator.h"

#include <ChimeraTK/Exception.h>

namespace ChimeraTK {

  template<typename UserType>
  class ReverseRecoveryDecorator : public ChimeraTK::ExceptionHandlingDecorator<UserType> {
   public:
    ReverseRecoveryDecorator(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, const VariableNetworkNode& networkNode);

    void interrupt() override;

    void setInReadAnyGroup(ReadAnyGroup* rag) override;

   protected:
    using ExceptionHandlingDecorator<UserType>::_recoveryHelper;
    using ExceptionHandlingDecorator<UserType>::_target;
  };

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ReverseRecoveryDecorator);
} // namespace ChimeraTK
