// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FanOut.h"

#include <ChimeraTK/NDRegisterAccessor.h>

#include <functional>
#include <sstream>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   * NDRegisterAccessor implementation which distributes values written to this
   * accessor out to any number of slaves.
   */
  template<typename UserType>
  class FeedingFanOut : public FanOut<UserType>, public ChimeraTK::NDRegisterAccessor<UserType> {
   public:
    FeedingFanOut(std::string const& name, std::string const& unit, std::string const& description,
        size_t numberOfElements, bool withReturn,
        ConsumerImplementationPairs<UserType> const& consumerImplementationPairs);

    /** Add a slave to the FanOut. Only sending end-points of a consuming node may
     * be added. */
    void addSlave(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode&) override;

    bool isReadable() const override { return _withReturn; }

    bool isReadOnly() const override { return false; }

    bool isWriteable() const override { return true; }

    void doReadTransferSynchronously() override;

    void doPreRead(TransferType type) override;

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType, VersionNumber) override;

    bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber) override;

    bool doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber = {}) override;

    void doPostWrite(TransferType, VersionNumber) override;

    bool mayReplaceOther(const boost::shared_ptr<const ChimeraTK::TransferElement>&) const override;

    std::list<boost::shared_ptr<ChimeraTK::TransferElement>> getInternalElements() override;

    std::vector<boost::shared_ptr<ChimeraTK::TransferElement>> getHardwareAccessingElements() override;

    void replaceTransferElement(boost::shared_ptr<ChimeraTK::TransferElement>) override;

    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> getReturnSlave() { return _returnSlave; }

    void interrupt() override;

   protected:
    /// Flag whether this FeedingFanOut has a return channel. Is specified in the
    /// constructor
    bool _withReturn;

    /// Used if _withReturn is true: flag whether the corresponding slave with the
    /// return channel has already been added.
    bool _hasReturnSlave{false};

    /// The slave with return channel
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> _returnSlave;

    /// DataValidity to attach to the data
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  FeedingFanOut<UserType>::FeedingFanOut(std::string const& name, std::string const& unit,
      std::string const& description, size_t numberOfElements, bool withReturn,
      ConsumerImplementationPairs<UserType> const& consumerImplementationPairs)
  : FanOut<UserType>(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>()),
    // We pass default-constructed, empty AccessModeFlags, they may later be determined from _returnSlave
    ChimeraTK::NDRegisterAccessor<UserType>("FeedingFanOut:" + name, AccessModeFlags{}, unit, description),
    _withReturn(withReturn) {
    ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D.resize(1);
    ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0].resize(numberOfElements);

    this->_readQueue = cppext::future_queue<void>(3);

    // Add the consuming accessors
    // TODO FanOut constructors and addSlave should get refactoring
    for(auto el : consumerImplementationPairs) {
      addSlave(el.first, el.second);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::addSlave(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode&) {
    // check if array shape is compatible, unless the receiver is a trigger
    // node, so no data is expected
    if(slave->getNumberOfSamples() != 0 &&
        (slave->getNumberOfChannels() != 1 || slave->getNumberOfSamples() != this->getNumberOfSamples())) {
      std::string what = "FeedingFanOut::addSlave(): Trying to add a slave '" + slave->getName();
      what += "' with incompatible array shape! Name of fan out: '" + this->getName() + "'";
      throw ChimeraTK::logic_error(what.c_str());
    }

    // make sure slave is writeable
    if(!slave->isWriteable()) {
      throw ChimeraTK::logic_error("FeedingFanOut::addSlave() has been called "
                                   "with a receiving implementation!");
    }

    // handle return channels
    if(_withReturn) {
      if(slave->isReadable()) {
        if(_hasReturnSlave) {
          throw ChimeraTK::logic_error("FeedingFanOut: Cannot add multiple slaves with return channel!");
        }

        // Assert the assumption about the return channel made in the constructor
        assert(slave->getAccessModeFlags().has(AccessMode::wait_for_new_data));

        _hasReturnSlave = true;
        _returnSlave = slave;

        // Set the readQeue from the return slave
        // As this becomes the implemention of the feeding output, the flags are determined by that slave accessor
        // If not _withReturn, the queue is not relevant because the feeding node is on output which is never read
        this->_readQueue = _returnSlave->getReadQueue();
        this->_accessModeFlags = _returnSlave->getAccessModeFlags();
      }
    }

    // add the slave
    FanOut<UserType>::slaves.push_back(slave);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doReadTransferSynchronously() {
    if(this->_disabled) return;
    assert(_withReturn);
    _returnSlave->readTransfer();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPreRead(TransferType type) {
    if(!_withReturn) throw ChimeraTK::logic_error("Read operation called on write-only variable.");
    if(this->_disabled) return;
    _returnSlave->accessChannel(0).swap(ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0]);
    _returnSlave->preRead(type);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPostRead(TransferType type, bool hasNewData) {
    if(this->_disabled) return;
    assert(_withReturn);
    assert(_hasReturnSlave);

    auto _ = cppext::finally([&] {
      if(!hasNewData) return;
      _returnSlave->accessChannel(0).swap(ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0]);
      // distribute return-channel update to the other slaves
      for(auto& slave : FanOut<UserType>::slaves) { // send out copies to slaves
        if(slave == _returnSlave) continue;
        if(slave->getNumberOfSamples() != 0) { // do not send copy if no data is expected (e.g. trigger)
          slave->accessChannel(0) = ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0];
        }
        slave->writeDestructively(_returnSlave->getVersionNumber());
      }
    });

    _returnSlave->postRead(type, hasNewData);

    this->_versionNumber = _returnSlave->getVersionNumber();
    this->_dataValidity = _returnSlave->dataValidity();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPreWrite(TransferType, VersionNumber) {
    if(this->_disabled) return;
    for(auto& slave : FanOut<UserType>::slaves) {       // send out copies to slaves
      if(slave->getNumberOfSamples() != 0) {            // do not send copy if no data is expected (e.g. trigger)
        if(slave == FanOut<UserType>::slaves.front()) { // in case of first slave, swap instead of copy
          slave->accessChannel(0).swap(ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0]);
        }
        else { // not the first slave: copy the data from the first slave
          slave->accessChannel(0) = FanOut<UserType>::slaves.front()->accessChannel(0);
        }
      }
      slave->setDataValidity(this->dataValidity());
    }

    // Don't call pre-write on the slaves. Each slave has to do it's own exception handling, so we call the whole
    // operation in doWriteTansfer(). To fulfill the TransferElement specification we would have to check the
    // pre-conditions here so no logic error is thrown in the transfer phase (logic_errors are predictable and can
    // always pre prevented. They should be thrown here already).
    // FIXME: At the moment we can be lazy about it. logic_errors are not treated in ApplicationCore and the only
    // effect is that the logic_error would be delayed after postRead() and terminate the application there, and not
    // after the transfer. Advantage about being lazy: It safes a few virtual function calls.
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool FeedingFanOut<UserType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber) {
    if(this->_disabled) return false;
    bool dataLost = false;
    bool isFirst = true;
    for(auto& slave : FanOut<UserType>::slaves) {
      bool ret;
      if(isFirst) {
        isFirst = false;
        ret = slave->write(versionNumber);
      }
      else {
        ret = slave->writeDestructively(versionNumber);
      }
      if(ret) dataLost = true;
    }
    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool FeedingFanOut<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    if(this->_disabled) return false;
    bool dataLost = false;
    for(auto& slave : FanOut<UserType>::slaves) {
      bool ret = slave->writeDestructively(versionNumber);
      if(ret) dataLost = true;
    }
    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPostWrite(TransferType, VersionNumber) {
    if(this->_disabled) return;
    // the postWrite() on the slaves has already been called
    FanOut<UserType>::slaves.front()->accessChannel(0).swap(ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0]);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool FeedingFanOut<UserType>::mayReplaceOther(const boost::shared_ptr<const ChimeraTK::TransferElement>&) const {
    return false; /// @todo implement properly?
  }

  /********************************************************************************************************************/

  template<typename UserType>
  std::list<boost::shared_ptr<ChimeraTK::TransferElement>> FeedingFanOut<UserType>::getInternalElements() {
    return {}; /// @todo implement properly?
  }

  /********************************************************************************************************************/

  template<typename UserType>
  std::vector<boost::shared_ptr<ChimeraTK::TransferElement>> FeedingFanOut<UserType>::getHardwareAccessingElements() {
    return {boost::enable_shared_from_this<ChimeraTK::TransferElement>::shared_from_this()}; /// @todo implement
                                                                                             /// properly?
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::replaceTransferElement(boost::shared_ptr<ChimeraTK::TransferElement>) {
    // You can't replace anything here. Just do nothing.
    /// @todo implement properly?
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::interrupt() {
    // call the interrut sequences of the fan out (interrupts for fan input and all outputs), and the ndRegisterAccessor
    FanOut<UserType>::interrupt();
    if(_withReturn) {
      _returnSlave->interrupt();
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
