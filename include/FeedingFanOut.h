// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FanOut.h"

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

    [[nodiscard]] bool isReadable() const override { return _withReturn; }

    [[nodiscard]] bool isReadOnly() const override { return false; }

    [[nodiscard]] bool isWriteable() const override { return true; }

    void doReadTransferSynchronously() override;

    void doPreRead(TransferType type) override;

    void doPostRead(TransferType type, bool hasNewData) override;

    void doPreWrite(TransferType, VersionNumber) override;

    bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber) override;

    // FIXME: https://redmine.msktools.desy.de/issues/12242
    // NOLINTNEXTLINE(google-default-arguments)
    bool doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber = {}) override;

    void doPostWrite(TransferType, VersionNumber) override;

    [[nodiscard]] bool mayReplaceOther(const boost::shared_ptr<const ChimeraTK::TransferElement>&) const override;

    std::list<boost::shared_ptr<ChimeraTK::TransferElement>> getInternalElements() override;

    std::vector<boost::shared_ptr<ChimeraTK::TransferElement>> getHardwareAccessingElements() override;

    void replaceTransferElement(boost::shared_ptr<ChimeraTK::TransferElement>) override;

    void interrupt() override;

   protected:
    /** Add a slave to the FanOut. Only sending end-points of a consuming node may be added. */
    void addSlave(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode&) override;

    /// Finalise the return channel
    void finalise();

    /// Flag whether this FeedingFanOut has a return channel. Is specified in the constructor
    bool _withReturn;

    /// Flag whether finalise() has been called
    bool _finalised{false};

    /// list of return slaves, if any
    std::vector<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>> _returnSlaves;

    /// index to _returnSlaves for the last update
    size_t _idxLastUpdate{std::numeric_limits<size_t>::max()};
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

    if(_withReturn) {
      this->_accessModeFlags = {AccessMode::wait_for_new_data};
    }

    // Add the consuming accessors
    // TODO FanOut constructors and addSlave should get refactoring
    for(auto el : consumerImplementationPairs) {
      FeedingFanOut<UserType>::addSlave(el.first, el.second);
    }

    finalise();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::addSlave(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode& node) {
    assert(!_finalised);
    // check if array shape is compatible, unless the receiver is a trigger
    // node, so no data is expected
    if(slave->getNumberOfSamples() != 0 &&
        (slave->getNumberOfChannels() != 1 || slave->getNumberOfSamples() != this->getNumberOfSamples())) {
      std::string what = "FeedingFanOut::addSlave(): Trying to add a slave '" + slave->getName();
      what += "' with incompatible array shape! Name of fan out: '" + this->getName() + "'";
      throw ChimeraTK::logic_error(what);
    }

    // make sure slave is writeable
    if(!slave->isWriteable()) {
      throw ChimeraTK::logic_error("FeedingFanOut::addSlave() has been called "
                                   "with a receiving implementation!");
    }

    // handle return channels
    if(_withReturn) {
      if(node.getDirection().withReturn) {
        // These assumptions should be guaranteed by the connection making code which created the PV
        assert(slave->isReadable());
        assert(slave->getAccessModeFlags().has(AccessMode::wait_for_new_data));
        _returnSlaves.push_back(slave);
      }
    }

    // add the slave
    FanOut<UserType>::_slaves.push_back(slave);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::finalise() {
    // create read queue as when-any continuation from all return slave read queues
    std::vector<cppext::future_queue<void>> queueList;
    for(auto& slave : _returnSlaves) {
      queueList.push_back(slave->getReadQueue());
    }

    auto notificationQueue = cppext::when_any(queueList.begin(), queueList.end());
    this->_readQueue = notificationQueue.then<void>(
        [this](size_t idx) {
          _idxLastUpdate = idx;
          try {
            _returnSlaves[idx]->getReadQueue().pop_wait();
          }
          catch(detail::DiscardValueException&) {
            // This value should never be actually exposed anywhere since the read transfer will be retried,
            // but we set it anyway to make sure the logic is correct (would trigger asserts if not).
            _idxLastUpdate = std::numeric_limits<size_t>::max() - 1;
            throw;
          }
        },
        std::launch::deferred);

    _finalised = true;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doReadTransferSynchronously() {
    assert(false);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPreRead(TransferType type) {
    if(!_withReturn) {
      throw ChimeraTK::logic_error("Read operation called on write-only variable.");
    }
    if(this->_disabled) {
      return;
    }

    assert(_idxLastUpdate != std::numeric_limits<size_t>::max() - 1);
    if(_idxLastUpdate == std::numeric_limits<size_t>::max()) {
      for(auto& slave : _returnSlaves) {
        slave->preRead(TransferType::read);
      }
    }
    else {
      _returnSlaves[_idxLastUpdate]->preRead(type);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPostRead(TransferType type, bool hasNewData) {
    assert(_withReturn);
    if(this->_disabled) {
      return;
    }

    if(!hasNewData && type != TransferType::read) {
      // No post read handling for readNonBlocking and readLatest if there was no new data, since there was actually no
      // corresponding read operation on any of the underlying accessors (just checking the notification queue).
      return;
    }

    assert(_idxLastUpdate < std::numeric_limits<size_t>::max() - 1);

    auto _ = cppext::finally([&] {
      if(!hasNewData || TransferElement::_activeException) {
        return;
      }
      _returnSlaves[_idxLastUpdate]->accessChannel(0).swap(ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0]);
      // distribute return-channel update to the other slaves

      for(auto& slave : FanOut<UserType>::_slaves) { // send out copies to slaves
        if(slave == _returnSlaves[_idxLastUpdate]) {
          continue;
        }
        if(slave->getNumberOfSamples() != 0) { // do not send copy if no data is expected (e.g. trigger)
          slave->accessChannel(0) = ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0];
        }
        slave->writeDestructively(this->_versionNumber);
      }
    });

    _returnSlaves[_idxLastUpdate]->postRead(type, hasNewData);

    this->_versionNumber = _returnSlaves[_idxLastUpdate]->getVersionNumber();
    this->_dataValidity = _returnSlaves[_idxLastUpdate]->dataValidity();
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPreWrite(TransferType, VersionNumber) {
    if(this->_disabled) {
      return;
    }
    for(auto& slave : FanOut<UserType>::_slaves) {       // send out copies to slaves
      if(slave->getNumberOfSamples() != 0) {             // do not send copy if no data is expected (e.g. trigger)
        if(slave == FanOut<UserType>::_slaves.front()) { // in case of first slave, swap instead of copy
          slave->accessChannel(0).swap(ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0]);
        }
        else { // not the first slave: copy the data from the first slave
          slave->accessChannel(0) = FanOut<UserType>::_slaves.front()->accessChannel(0);
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
    if(this->_disabled) {
      return false;
    }
    bool dataLost = false;
    bool isFirst = true;
    for(auto& slave : FanOut<UserType>::_slaves) {
      bool ret;
      if(isFirst) {
        isFirst = false;
        ret = slave->write(versionNumber);
      }
      else {
        ret = slave->writeDestructively(versionNumber);
      }
      if(ret) {
        dataLost = true;
      }
    }

    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  // FIXME: https://redmine.msktools.desy.de/issues/12242
  // NOLINTNEXTLINE(google-default-arguments)
  bool FeedingFanOut<UserType>::doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber) {
    if(this->_disabled) {
      return false;
    }
    bool dataLost = false;
    for(auto& slave : FanOut<UserType>::_slaves) {
      bool ret = slave->writeDestructively(versionNumber);
      if(ret) {
        dataLost = true;
      }
    }
    return dataLost;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FeedingFanOut<UserType>::doPostWrite(TransferType, VersionNumber) {
    if(this->_disabled) {
      return;
    }
    // the postWrite() on the slaves has already been called
    FanOut<UserType>::_slaves.front()->accessChannel(0).swap(ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D[0]);
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
    for(auto returnSlave : _returnSlaves) {
      returnSlave->interrupt();
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
