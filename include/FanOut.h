// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "VariableNetworkNode.h"

#include <ChimeraTK/NDRegisterAccessor.h>

#include <list>
#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  template<typename UserType>
  using ConsumerImplementationPairs =
      std::list<std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>, VariableNetworkNode>>;

  /********************************************************************************************************************/

  /** Type independent base */
  class FanOutBase {
   public:
    virtual ~FanOutBase() = default;
    virtual void removeSlave(const boost::shared_ptr<ChimeraTK::TransferElement>& slave) = 0;

    /** Disable the FanOut so it does nothing. Used by Application::optimiseUnmappedVariables(). FeedingFanOut simply
     *  do nothing instead of read/write operations. ThreadedFanOuts will not launch their thread. Has to be called
     *  before launching the application/fanout threads. */
    void disable() { _disabled = true; }

   protected:
    bool _disabled{false};
  };

  /********************************************************************************************************************/

  /** Base class for several implementations which distribute values from one
   * feeder to multiple consumers */
  template<typename UserType>
  class FanOut : public FanOutBase {
   public:
    explicit FanOut(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl)
    : _impl(std::move(feedingImpl)) {}

    /** Add a slave to the FanOut. Only sending end-points of a consuming node may
     * be added. */
    virtual void addSlave(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode& /*consumer*/);

    // remove a slave identified by its consuming node from the FanOut
    void removeSlave(const boost::shared_ptr<ChimeraTK::TransferElement>& slave) override;

    // interrupt the input and all slaves
    virtual void interrupt();

   protected:
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> _impl;

    std::list<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>> _slaves;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  void FanOut<UserType>::addSlave(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode& /*consumer*/) {
    if(!slave->isWriteable()) {
      throw ChimeraTK::logic_error("FanOut::addSlave() has been called with a "
                                   "receiving implementation!");
    }
    // check if array shape is compatible, unless the receiver is a trigger
    // node, so no data is expected
    if(slave->getNumberOfSamples() != 0 &&
        (slave->getNumberOfChannels() != _impl->getNumberOfChannels() ||
            slave->getNumberOfSamples() != _impl->getNumberOfSamples())) {
      std::string what = "FanOut::addSlave(): Trying to add a slave '";
      what += slave->getName();
      what += "' with incompatible array shape! Name of master: ";
      what += _impl->getName();
      what += " Length of master: " + std::to_string(_impl->getNumberOfChannels()) + " x " +
          std::to_string(_impl->getNumberOfSamples());
      what += " Length of slave: " + std::to_string(slave->getNumberOfChannels()) + " x " +
          std::to_string(slave->getNumberOfSamples());
      throw ChimeraTK::logic_error(what);
    }
    _slaves.push_back(slave);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FanOut<UserType>::removeSlave(const boost::shared_ptr<ChimeraTK::TransferElement>& slave) {
    // make sure the slave is actually currently in the list, and get it by the right typ
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave_typed;
    for(auto& s : _slaves) {
      if(s == slave) {
        slave_typed = s;
        break;
      }
    }
    assert(slave_typed != nullptr);

    [[maybe_unused]] size_t nOld = _slaves.size();
    _slaves.remove(slave_typed);
    assert(_slaves.size() == nOld - 1);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void FanOut<UserType>::interrupt() {
    if(_impl) {
      _impl->interrupt();
    }
    for(auto& slave : _slaves) {
      slave->interrupt();
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
