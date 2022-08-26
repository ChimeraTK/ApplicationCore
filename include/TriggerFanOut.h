/*
 * TriggerFanOut.h
 *
 *  Created on: Jun 15, 2016
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_TRIGGER_FAN_OUT_H
#define CHIMERATK_TRIGGER_FAN_OUT_H

#include "Application.h"
#include "DeviceModule.h"
#include "FeedingFanOut.h"
#include "InternalModule.h"
#include "Profiler.h"

#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/SupportedUserTypes.h>
#include <ChimeraTK/TransferGroup.h>

constexpr useconds_t DeviceOpenTimeout = 500;

namespace ChimeraTK {

  /** InternalModule which waits for a trigger, then reads a number of variables
   * and distributes each of them to any number of slaves. */
  class TriggerFanOut : public InternalModule {
   public:
    TriggerFanOut(const boost::shared_ptr<ChimeraTK::TransferElement>& externalTriggerImpl, DeviceModule& deviceModule,
        VariableNetwork& network)
    : externalTrigger(externalTriggerImpl), _deviceModule(deviceModule), _network(network) {}

    ~TriggerFanOut() { deactivate(); }

    void activate() override {
      assert(!_thread.joinable());
      _thread = boost::thread([this] { this->run(); });
    }

    void deactivate() override {
      if(_thread.joinable()) {
        _thread.interrupt();
        if(externalTrigger->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
          externalTrigger->interrupt();
        }
        _thread.join();
      }
      assert(!_thread.joinable());
    }

    /** Add a new network the TriggerFanOut. The network is defined by its feeding
     * node. This function will return the corresponding FeedingFanOut, to which
     * all slaves have to be added. */
    template<typename UserType>
    boost::shared_ptr<FeedingFanOut<UserType>> addNetwork(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingNode,
        ConsumerImplementationPairs<UserType> const& consumerImplementationPairs) {
      assert(feedingNode.get() != nullptr);
      transferGroup.addAccessor(feedingNode);
      auto feedingFanOut = boost::make_shared<FeedingFanOut<UserType>>(feedingNode->getName(), feedingNode->getUnit(),
          feedingNode->getDescription(), feedingNode->getNumberOfSamples(),
          false, // in TriggerFanOuts we cannot have return channels
          consumerImplementationPairs);
      boost::fusion::at_key<UserType>(fanOutMap.table)[feedingNode] = feedingFanOut;
      return feedingFanOut;
    }

    /** Synchronise feeder and the consumers. This function is executed in the
     * separate thread. */
    void run() {
      Application::registerThread("TrFO" + externalTrigger->getName());
      Application::testableModeLock("start");
      testableModeReached = true;

      ChimeraTK::VersionNumber version = Application::getInstance().getStartVersion();

      // Wait for the initial value of the trigger. There always will be one, and if we don't read it here we would
      // trigger the loop twice.
      externalTrigger->read();
      version = externalTrigger->getVersionNumber();

      // Wait until the device has been initialised for the first time. This means it
      // has been opened, and the check in TransferGroup::read() will not throw a logic_error
      // We don't have to store the lock. Just need it as a synchronisation point.
      // But we have to increase the testable mode counter because we don't want to fall out of testable mode at this
      // point already.
      if(Application::getInstance().testableMode) ++Application::getInstance().testableMode_deviceInitialisationCounter;
      Application::testableModeUnlock("WaitInitialValueLock");
      (void)_deviceModule.waitForInitialValues();
      Application::testableModeLock("Enter while loop");
      if(Application::getInstance().testableMode) --Application::getInstance().testableMode_deviceInitialisationCounter;

      while(true) {
        transferGroup.read();
        // send the version number to the consumers
        boost::fusion::for_each(fanOutMap.table, SendDataToConsumers(version, externalTrigger->dataValidity()));

        // wait for external trigger
        boost::this_thread::interruption_point();
        Profiler::stopMeasurement();
        externalTrigger->read();
        Profiler::startMeasurement();
        boost::this_thread::interruption_point();
        version = externalTrigger->getVersionNumber();
      }
    }

   protected:
    /** Functor class to send data to the consumers, suitable for
     * boost::fusion::for_each(). */
    struct SendDataToConsumers {
      SendDataToConsumers(VersionNumber version, DataValidity triggerValidity)
      : _version(version), _triggerValidity(triggerValidity) {}

      template<typename PAIR>
      void operator()(PAIR& pair) const {
        auto theMap = pair.second; // map of feeder to FeedingFanOut (i.e. part of
                                   // the fanOutMap)

        // iterate over all feeder/FeedingFanOut pairs
        for(auto& network : theMap) {
          auto feeder = network.first;
          auto fanOut = network.second;
          fanOut->setDataValidity((_triggerValidity == DataValidity::ok && feeder->dataValidity() == DataValidity::ok) ?
                  DataValidity::ok :
                  DataValidity::faulty);
          fanOut->accessChannel(0).swap(feeder->accessChannel(0));
          // don't use write destructively. In case of an exception we still need the data for the next read (see
          // Exception Handling spec B.2.2.6)
          bool dataLoss = fanOut->write(_version);
          if(dataLoss) Application::incrementDataLossCounter(fanOut->getName());
          // swap the data back to the feeder so we have a valid copy there.
          fanOut->accessChannel(0).swap(feeder->accessChannel(0));
        }
      }

      VersionNumber _version;
      DataValidity _triggerValidity;
    };

    /** TransferElement acting as our trigger */
    boost::shared_ptr<ChimeraTK::TransferElement> externalTrigger;

    /** Map of the feeding NDRegisterAccessor to the corresponding FeedingFanOut
     * for each UserType */
    template<typename UserType>
    using FanOutMap = std::map<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
        boost::shared_ptr<FeedingFanOut<UserType>>>;
    TemplateUserTypeMap<FanOutMap> fanOutMap;

    /** TransferGroup containing all feeders NDRegisterAccessors */
    ChimeraTK::TransferGroup transferGroup;

    /** Thread handling the synchronisation, if needed */
    boost::thread _thread;

    /** The DeviceModule of the feeder. Required for exception handling */
    DeviceModule& _deviceModule;

    /** Reference to VariableNetwork which is being realised by this FanOut. **/
    VariableNetwork& _network;
  };

} /* namespace ChimeraTK */

#endif /* CHIMERATK_TRIGGER_FAN_OUT_H */
