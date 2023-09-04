// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FanOut.h"

#include <ChimeraTK/NDRegisterAccessorDecorator.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** FanOut implementation which acts as a read-only (i.e. consuming)
   * NDRegisterAccessor. The values read through this accessor will be obtained
   * from the given feeding implementation and distributed to any number of
   * slaves. */
  template<typename UserType>
  class ConsumingFanOut : public FanOut<UserType>, public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
   public:
    ConsumingFanOut(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl,
        ConsumerImplementationPairs<UserType> const& consumerImplementationPairs);

    void doPostRead(TransferType type, bool updateDataBuffer) override;

    void interrupt() override;

   protected:
    using ChimeraTK::NDRegisterAccessor<UserType>::buffer_2D;
    std::vector<UserType> _lastReceivedValue;
  };

  /********************************************************************************************************************/

  template<typename UserType>
  ConsumingFanOut<UserType>::ConsumingFanOut(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl,
      ConsumerImplementationPairs<UserType> const& consumerImplementationPairs)
  : FanOut<UserType>(feedingImpl), ChimeraTK::NDRegisterAccessorDecorator<UserType>(feedingImpl) {
    assert(feedingImpl->isReadable());

    _lastReceivedValue.resize(buffer_2D[0].size());

    // Add the consuming accessors
    for(auto el : consumerImplementationPairs) {
      FanOut<UserType>::addSlave(el.first, el.second);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ConsumingFanOut<UserType>::doPostRead(TransferType type, bool updateDataBuffer) {
    ChimeraTK::NDRegisterAccessorDecorator<UserType>::doPostRead(type, updateDataBuffer);

    if(updateDataBuffer) {
      // We have to keep a copy to write into the slaves. There might
      // be decorators arount this fanout which swap out buffer_2D, so it is
      // not available any more for a second read witout updateDataBuffer (exception case).
      _lastReceivedValue = buffer_2D[0];
    }

    // The ConsumingFanOut conceptually never has a wait_fow_new_data flags. Hence each read
    // operation returns with "new" data, even in case of an exception. So each read
    // always synchronises all slaves and pushes the content of the data buffer.
    for(auto& slave : FanOut<UserType>::_slaves) { // send out copies to slaves
      // do not send copy if no data is expected (e.g. trigger)
      if(slave->getNumberOfSamples() != 0) {
        slave->accessChannel(0) = _lastReceivedValue;
      }
      slave->setDataValidity(this->dataValidity());
      slave->writeDestructively();
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ConsumingFanOut<UserType>::interrupt() {
    // call the interrut sequences of the fan out (interrupts for fan input and all outputs), and the ndRegisterAccessor
    FanOut<UserType>::interrupt();
    if(this->_accessModeFlags.has(AccessMode::wait_for_new_data)) {
      ChimeraTK::NDRegisterAccessor<UserType>::interrupt();
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
