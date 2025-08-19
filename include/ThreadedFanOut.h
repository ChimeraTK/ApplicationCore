// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "FanOut.h"
#include "InternalModule.h"
#include "ReverseRecoveryDecorator.h"

#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/ReadAnyGroup.h>
#include <ChimeraTK/SupportedUserTypes.h>
#include <ChimeraTK/SystemTags.h>

#include <boost/smart_ptr/shared_ptr.hpp>

#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** FanOut implementation with an internal thread which waits for new data which
   * is read from the given feeding implementation and distributed to any number
   * of slaves. */
  template<typename UserType>
  class ThreadedFanOut : public FanOut<UserType>, public InternalModule {
   public:
    ThreadedFanOut(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl,
        ConsumerImplementationPairs<UserType> const& consumerImplementationPairs);

    ~ThreadedFanOut() override;

    void activate() override;

    void deactivate() override;

    /** Synchronise feeder and the consumers. This function is executed in the
     * separate thread. */
    virtual void run();

    VersionNumber readInitialValues(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor);

   protected:
    /** Thread handling the synchronisation, if needed */
    boost::thread _thread;

    /** Reference to VariableNetwork which is being realised by this FanOut. **/
    // VariableNetwork& _network;
  };

  /********************************************************************************************************************/

  /** Same as ThreadedFanOut but with return channel */
  template<typename UserType>
  class ThreadedFanOutWithReturn : public ThreadedFanOut<UserType> {
   public:
    ThreadedFanOutWithReturn(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl,
        ConsumerImplementationPairs<UserType> const& consumerImplementationPairs);

    void addSlave(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode& consumer) override;

    void run() override;

   protected:
    /** Thread handling the synchronisation, if needed */
    boost::thread _thread;

    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> _initialValueProvider;
    std::vector<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>> _inputChannels;

    // using ThreadedFanOut<UserType>::_network;
    using ThreadedFanOut<UserType>::readInitialValues;
    using EntityOwner::_testableModeReached;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ThreadedFanOut<UserType>::ThreadedFanOut(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl,
      ConsumerImplementationPairs<UserType> const& consumerImplementationPairs)
  : FanOut<UserType>(feedingImpl) /*, _network(network)*/ {
    assert(feedingImpl->getAccessModeFlags().has(AccessMode::wait_for_new_data));
    for(auto el : consumerImplementationPairs) {
      FanOut<UserType>::addSlave(el.first, el.second);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  ThreadedFanOut<UserType>::~ThreadedFanOut() {
    try {
      deactivate();
    }
    catch(ChimeraTK::logic_error& e) {
      std::terminate();
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ThreadedFanOut<UserType>::activate() {
    if(this->_disabled) {
      return;
    }
    assert(!_thread.joinable());
    _thread = boost::thread([this] { this->run(); });
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ThreadedFanOut<UserType>::deactivate() {
    try {
      if(_thread.joinable()) {
        _thread.interrupt();
        FanOut<UserType>::interrupt();
        _thread.join();
      }
      assert(!_thread.joinable());
    }
    catch(boost::thread_resource_error& e) {
      assert(false);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ThreadedFanOut<UserType>::run() {
    Application::registerThread("ThFO" + FanOut<UserType>::_impl->getName());
    Application::getInstance().getTestableMode().lock("start", true);
    _testableModeReached = true;

    ChimeraTK::VersionNumber version{nullptr};
    version = readInitialValues(FanOut<UserType>::_impl);
    while(true) {
      // send out copies to slaves
      boost::this_thread::interruption_point();
      auto validity = FanOut<UserType>::_impl->dataValidity();
      for(auto& slave : FanOut<UserType>::_slaves) {
        // do not send copy if no data is expected (e.g. trigger)
        if(slave->getNumberOfSamples() != 0) {
          slave->accessChannel(0) = FanOut<UserType>::_impl->accessChannel(0);
        }
        slave->setDataValidity(validity);
        bool dataLoss = slave->writeDestructively(version);
        if(dataLoss) {
          Application::incrementDataLossCounter(slave->getName());
        }
      }
      // receive data
      boost::this_thread::interruption_point();
      FanOut<UserType>::_impl->read();
      version = FanOut<UserType>::_impl->getVersionNumber();
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  VersionNumber ThreadedFanOut<UserType>::readInitialValues(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor) {
    Application::getInstance().getTestableMode().unlock("readInitialValues");
    accessor->read();
    if(!Application::getInstance().getTestableMode().testLock()) {
      Application::getInstance().getTestableMode().lock("readInitialValues", true);
    }
    return accessor->getVersionNumber();
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ThreadedFanOutWithReturn<UserType>::ThreadedFanOutWithReturn(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl,
      ConsumerImplementationPairs<UserType> const& consumerImplementationPairs)
  : ThreadedFanOut<UserType>(feedingImpl, consumerImplementationPairs) {
    _inputChannels.push_back(feedingImpl);
    // By default, we take the initial value from the feeder
    _initialValueProvider = feedingImpl;
    for(auto el : consumerImplementationPairs) {
      ThreadedFanOutWithReturn<UserType>::addSlave(el.first, el.second);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ThreadedFanOutWithReturn<UserType>::addSlave(
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> slave, VariableNetworkNode& consumer) {
    // TODO Adding slaves is currently by done by the ThreadedFanOut base class constructor.
    //      Refactor constructors and addSlaves for all FanOuts?
    // FanOut<UserType>::addSlave(slave, consumer);

    if(consumer.getTags().contains(ChimeraTK::SystemTags::reverseRecovery)) {
      _initialValueProvider = slave;
      // FIXME: Do we need to check here that there is only one reverse recovery accessor
    }

    if(consumer.getDirection().withReturn) {
      _inputChannels.push_back(slave);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ThreadedFanOutWithReturn<UserType>::run() {
    Application::registerThread("ThFO" + FanOut<UserType>::_impl->getName());
    Application::getInstance().getTestableMode().lock("start", true);
    _testableModeReached = true;

    std::map<TransferElementID, boost::shared_ptr<NDRegisterAccessor<UserType>>> accessors;
    for(auto& acc : FanOut<UserType>::_slaves) {
      accessors[acc->getId()] = acc;
    }
    accessors[FanOut<UserType>::_impl->getId()] = FanOut<UserType>::_impl;

    TransferElementID changedVariable = _initialValueProvider->getId();

    auto version = readInitialValues(_initialValueProvider);

    ReadAnyGroup group(_inputChannels.begin(), _inputChannels.end());

    while(true) {
      // send out copies to all receivers (slaves and return channel of feeding node)
      for(auto& [id, accessor] : accessors) {
        // do not feed back value to the accessor we got it from
        if(id == changedVariable) {
          continue;
        }

        // do not send copy if no data is expected (e.g. trigger)
        if(accessor->getNumberOfSamples() != 0) {
          accessor->accessChannel(0) = accessors[changedVariable]->accessChannel(0);
        }

        bool dataLoss = accessor->writeDestructively(version);

        if(dataLoss) {
          Application::incrementDataLossCounter(accessor->getName());
        }
      }

      // receive data
      boost::this_thread::interruption_point();
      changedVariable = group.readAny();
      boost::this_thread::interruption_point();

      version = accessors[changedVariable]->getVersionNumber();
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
