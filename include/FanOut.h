// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "VariableNetworkNode.h"

#include <ChimeraTK/NDRegisterAccessor.h>

#include <list>
#include <utility>

namespace ChimeraTK {

  template<typename UserType>
  using ConsumerImplementationPairs =
      std::list<std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>, VariableNetworkNode>>;

  /** Type independent base */
  class FanOutBase {
   public:
    virtual ~FanOutBase() {}
    virtual void removeSlave(const boost::shared_ptr<ChimeraTK::TransferElement>& slave) = 0;

    /** Disable the FanOut so it does nothing. Used by Application::optimiseUnmappedVariables(). FeedingFanOut simply
     *  do nothing instead of read/write operations. ThreadedFanOuts will not launch their thread. Has to be called
     *  before launching the application/fanout threads. */
    void disable() { _disabled = true; }

   protected:
    bool _disabled{false};
  };

  /** Base class for several implementations which distribute values from one
   * feeder to multiple consumers */
  template<typename UserType>
  class FanOut : public FanOutBase {
   public:
    FanOut(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl) : impl(feedingImpl) {}

    /** Add a slave to the FanOut. Only sending end-points of a consuming node may
     * be added. */
    virtual void addSlave(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode& /*consumer*/) {
      if(!slave->isWriteable()) {
        throw ChimeraTK::logic_error("FanOut::addSlave() has been called with a "
                                     "receiving implementation!");
      }
      // check if array shape is compatible, unless the receiver is a trigger
      // node, so no data is expected
      if(slave->getNumberOfSamples() != 0 &&
          (slave->getNumberOfChannels() != impl->getNumberOfChannels() ||
              slave->getNumberOfSamples() != impl->getNumberOfSamples())) {
        std::string what = "FanOut::addSlave(): Trying to add a slave '";
        what += slave->getName();
        what += "' with incompatible array shape! Name of master: ";
        what += impl->getName();
        what += " Length of master: " + std::to_string(impl->getNumberOfChannels()) + " x " +
            std::to_string(impl->getNumberOfSamples());
        what += " Length of slave: " + std::to_string(slave->getNumberOfChannels()) + " x " +
            std::to_string(slave->getNumberOfSamples());
        throw ChimeraTK::logic_error(what.c_str());
      }
      slaves.push_back(slave);
    }

    // remove a slave identified by its consuming node from the FanOut
    void removeSlave(const boost::shared_ptr<ChimeraTK::TransferElement>& slave) override {
      // make sure the slave is actually currently in the list, and get it by the right typ
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave_typed;
      for(auto& s : slaves) {
        if(s == slave) {
          slave_typed = s;
          break;
        }
      }
      assert(slave_typed != nullptr);

      size_t nOld = slaves.size();
      slaves.remove(slave_typed);
      assert(slaves.size() == nOld - 1);
    }

    // interrupt the input and all slaves
    virtual void interrupt() {
      if(impl) {
        if(impl->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
          impl->interrupt();
        }
      }
      for(auto& slave : slaves) {
        if(slave->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
          slave->interrupt();
        }
      }
    }

   protected:
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> impl;

    std::list<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>> slaves;
  };

} /* namespace ChimeraTK */
