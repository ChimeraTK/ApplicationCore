// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DebugPrintAccessorDecorator.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  template<typename UserType>
  DebugPrintAccessorDecorator<UserType>::DebugPrintAccessorDecorator(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, std::string fullyQualifiedName)
  : ChimeraTK::NDRegisterAccessorDecorator<UserType>(accessor), _fullyQualifiedName(std::move(fullyQualifiedName)) {
    std::cout << "Enable debug output for variable '" << _fullyQualifiedName << "'." << std::endl;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool DebugPrintAccessorDecorator<UserType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber) {
    std::cout << "doWriteTransfer() called on '" << _fullyQualifiedName << "'." << std::flush;
    auto ret = ChimeraTK::NDRegisterAccessorDecorator<UserType>::doWriteTransfer(versionNumber);
    if(ret) {
      std::cout << " -> DATA LOSS!";
    }
    std::cout << std::endl;
    return ret;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool DebugPrintAccessorDecorator<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    std::cout << "doWriteTransferDestructively() called on '" << _fullyQualifiedName << "'." << std::flush;
    auto ret = ChimeraTK::NDRegisterAccessorDecorator<UserType>::doWriteTransferDestructively(versionNumber);
    if(ret) {
      std::cout << " -> DATA LOSS!";
    }
    std::cout << std::endl;
    return ret;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doReadTransferSynchronously() {
    std::cout << "doReadTransferSynchronously() called on '" << _fullyQualifiedName << "'." << std::endl;
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doReadTransferSynchronously();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPreRead(TransferType type) {
    std::cout << "preRead() called on '" << _fullyQualifiedName << "'." << std::endl;
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPreRead(type);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPostRead(TransferType type, bool hasNewData) {
    std::cout << "postRead() called on '" << _fullyQualifiedName << "'." << std::endl;
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostRead(type, hasNewData);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPreWrite(TransferType type, VersionNumber versionNumber) {
    std::cout << "preWrite() called on '" << _fullyQualifiedName << "'." << std::endl;
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPreWrite(type, versionNumber);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DebugPrintAccessorDecorator<UserType>::doPostWrite(TransferType type, VersionNumber versionNumber) {
    std::cout << "postWrite() called on '" << _fullyQualifiedName << "'." << std::endl;
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostWrite(type, versionNumber);
  }

  /********************************************************************************************************************/

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(DebugPrintAccessorDecorator);

  /********************************************************************************************************************/

} // namespace ChimeraTK
