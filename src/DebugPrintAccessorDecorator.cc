// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DebugPrintAccessorDecorator.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  template<typename UserType>
  DebugPrintAccessorDecorator<UserType>::DebugPrintAccessorDecorator(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, std::string fullyQualifiedName)
  : ChimeraTK::NDRegisterAccessorDecorator<UserType>(accessor), _fullyQualifiedName(std::move(fullyQualifiedName)) {
    logger(Logger::Severity::trace, "DebugPrintAccessorDecorator")
        << "Enable debug output for variable '" << _fullyQualifiedName << "'.";
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool DebugPrintAccessorDecorator<UserType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber) {
    auto myLog = logger(Logger::Severity::trace, "DebugPrintAccessorDecorator");
    myLog << "doWriteTransfer() called on '" << _fullyQualifiedName << "'.";
    auto ret = ChimeraTK::NDRegisterAccessorDecorator<UserType>::doWriteTransfer(versionNumber);
    if(ret) {
      myLog << " -> DATA LOSS!";
    }
    return ret;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool DebugPrintAccessorDecorator<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    auto myLog = logger(Logger::Severity::trace, "DebugPrintAccessorDecorator");
    myLog << "doWriteTransferDestructively() called on '" << _fullyQualifiedName << "'.";
    auto ret = ChimeraTK::NDRegisterAccessorDecorator<UserType>::doWriteTransferDestructively(versionNumber);
    if(ret) {
      myLog << " -> DATA LOSS!";
    }
    return ret;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doReadTransferSynchronously() {
    logger(Logger::Severity::trace, "DebugPrintAccessorDecorator")
        << "doReadTransferSynchronously() called on '" << _fullyQualifiedName << "'.";
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doReadTransferSynchronously();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPreRead(TransferType type) {
    logger(Logger::Severity::trace, "DebugPrintAccessorDecorator")
        << "preRead() called on '" << _fullyQualifiedName << "'.";
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPreRead(type);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPostRead(TransferType type, bool hasNewData) {
    logger(Logger::Severity::trace, "DebugPrintAccessorDecorator")
        << "postRead() called on '" << _fullyQualifiedName << "'.";
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostRead(type, hasNewData);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPreWrite(TransferType type, VersionNumber versionNumber) {
    logger(Logger::Severity::trace, "DebugPrintAccessorDecorator")
        << "preWrite() called on '" << _fullyQualifiedName << "'.";
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPreWrite(type, versionNumber);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPostWrite(TransferType type, VersionNumber versionNumber) {
    logger(Logger::Severity::trace, "DebugPrintAccessorDecorator")
        << "postWrite() called on '" << _fullyQualifiedName << "'.";
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostWrite(type, versionNumber);
  }

  /********************************************************************************************************************/

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(DebugPrintAccessorDecorator);

  /********************************************************************************************************************/

} // namespace ChimeraTK
