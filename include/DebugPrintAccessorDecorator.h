/*
 * DebugPrintAccessorDecorator.h
 *
 *  Created on: Nov 21, 2017
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_DEBUG_DECORATOR_REGISTER_ACCCESSOR
#define CHIMERATK_DEBUG_DECORATOR_REGISTER_ACCCESSOR

#include "Application.h"

#include <ChimeraTK/NDRegisterAccessorDecorator.h>

namespace ChimeraTK {

  /** Decorator of the NDRegisterAccessor which facilitates tests of the
   * application */
  template<typename UserType>
  class DebugPrintAccessorDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
   public:
    DebugPrintAccessorDecorator(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, const std::string& fullyQualifiedName)
    : ChimeraTK::NDRegisterAccessorDecorator<UserType>(accessor), _fullyQualifiedName(fullyQualifiedName) {
      std::cout << "Enable debug output for variable '" << _fullyQualifiedName << "'." << std::endl;
    }

    bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber) override {
      std::cout << "doWriteTransfer() called on '" << _fullyQualifiedName << "'." << std::flush;
      auto ret = ChimeraTK::NDRegisterAccessorDecorator<UserType>::doWriteTransfer(versionNumber);
      if(ret) {
        std::cout << " -> DATA LOSS!";
      }
      std::cout << std::endl;
      return ret;
    }

    bool doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) override {
      std::cout << "doWriteTransferDestructively() called on '" << _fullyQualifiedName << "'." << std::flush;
      auto ret = ChimeraTK::NDRegisterAccessorDecorator<UserType>::doWriteTransferDestructively(versionNumber);
      if(ret) {
        std::cout << " -> DATA LOSS!";
      }
      std::cout << std::endl;
      return ret;
    }

    void doReadTransferSynchronously() override {
      std::cout << "doReadTransferSynchronously() called on '" << _fullyQualifiedName << "'." << std::endl;
      ChimeraTK::NDRegisterAccessorDecorator<UserType>::doReadTransferSynchronously();
    }

    void doPreRead(TransferType type) override {
      std::cout << "preRead() called on '" << _fullyQualifiedName << "'." << std::endl;
      ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPreRead(type);
    }

    void doPostRead(TransferType type, bool hasNewData) override {
      std::cout << "postRead() called on '" << _fullyQualifiedName << "'." << std::endl;
      ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostRead(type, hasNewData);
    }

    void doPreWrite(TransferType type, VersionNumber versionNumber) override {
      std::cout << "preWrite() called on '" << _fullyQualifiedName << "'." << std::endl;
      ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPreWrite(type, versionNumber);
    }

    void doPostWrite(TransferType type, VersionNumber versionNumber) override {
      std::cout << "postWrite() called on '" << _fullyQualifiedName << "'." << std::endl;
      ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostWrite(type, versionNumber);
    }

   protected:
    std::string _fullyQualifiedName;
  };

} /* namespace ChimeraTK */

#endif /* CHIMERATK_TEST_DECORATOR_REGISTER_ACCCESSOR */
