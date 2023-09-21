// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "DeviceModule.h"
#include "FeedingFanOut.h"
#include "InternalModule.h"

#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/SupportedUserTypes.h>
#include <ChimeraTK/TransferGroup.h>

constexpr useconds_t DeviceOpenTimeout = 500;

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** InternalModule which waits for a trigger, then reads a number of variables
   * and distributes each of them to any number of slaves. */
  class TriggerFanOut : public InternalModule {
   public:
    TriggerFanOut(boost::shared_ptr<ChimeraTK::TransferElement> externalTriggerImpl, DeviceManager& deviceModule);

    ~TriggerFanOut() override;

    void activate() override;

    void deactivate() final;

    /** Add a new network the TriggerFanOut. The network is defined by its feeding node. */
    template<typename UserType>
    void addNetwork(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingNode,
        ConsumerImplementationPairs<UserType> const& consumerImplementationPairs);

    /** Synchronise feeder and the consumers. This function is executed in the
     * separate thread. */
    void run();

   protected:
    /** TransferElement acting as our trigger */
    boost::shared_ptr<ChimeraTK::TransferElement> _externalTrigger;

    /** Map of the feeding NDRegisterAccessor to the corresponding FeedingFanOut
     * for each UserType */
    template<typename UserType>
    using FanOutMap = std::map<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
        boost::shared_ptr<FeedingFanOut<UserType>>>;
    TemplateUserTypeMap<FanOutMap> _fanOutMap;

    /** TransferGroup containing all feeders NDRegisterAccessors */
    ChimeraTK::TransferGroup _transferGroup;

    /** Thread handling the synchronisation, if needed */
    boost::thread _thread;

    /** The DeviceModule of the feeder. Required for exception handling */
    DeviceManager& _deviceModule;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  void TriggerFanOut::addNetwork(boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingNode,
      ConsumerImplementationPairs<UserType> const& consumerImplementationPairs) {
    assert(feedingNode.get() != nullptr);

    // if no consumers present, ignore this network. This primarily might happen if the only consumer was a control
    // system variable which was optimised out.
    if(consumerImplementationPairs.empty()) {
      return;
    }

    // add feeder to TransferGroup
    _transferGroup.addAccessor(feedingNode);

    // create FeedingFanOut to distribute the read value to all consumers (even if just one)
    auto feedingFanOut = boost::make_shared<FeedingFanOut<UserType>>(feedingNode->getName(), feedingNode->getUnit(),
        feedingNode->getDescription(), feedingNode->getNumberOfSamples(),
        false, // in TriggerFanOuts we cannot have return channels
        consumerImplementationPairs);
    boost::fusion::at_key<UserType>(_fanOutMap.table)[feedingNode] = feedingFanOut;
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
