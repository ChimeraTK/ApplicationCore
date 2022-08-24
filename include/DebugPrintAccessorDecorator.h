// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"

#include <ChimeraTK/NDRegisterAccessorDecorator.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   * Decorator of the NDRegisterAccessor which facilitates tests of the application
   */
  template<typename UserType>
  class DebugPrintAccessorDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
   public:
    DebugPrintAccessorDecorator(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, const std::string& fullyQualifiedName);

    bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber) override;

    bool doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) override;

    void doReadTransferSynchronously() override;

    void doPreRead(TransferType type) override;

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType type, VersionNumber versionNumber) override;

    void doPostWrite(TransferType type, VersionNumber versionNumber) override;

   protected:
    std::string _fullyQualifiedName;
  };

  /********************************************************************************************************************/

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(DebugPrintAccessorDecorator);

  /********************************************************************************************************************/
} /* namespace ChimeraTK */
