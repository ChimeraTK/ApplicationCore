// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TriggerFanOut.h"

#include "DeviceManager.h"

#include <utility>

namespace ChimeraTK {

  /********************************************************************************************************************/

  TriggerFanOut::TriggerFanOut(
      boost::shared_ptr<ChimeraTK::TransferElement> externalTriggerImpl, DeviceManager& deviceModule)
  : _externalTrigger(std::move(externalTriggerImpl)), _deviceModule(deviceModule) {}

  /********************************************************************************************************************/

  TriggerFanOut::~TriggerFanOut() {
    deactivate();
  }

  /********************************************************************************************************************/

  void TriggerFanOut::activate() {
    assert(!_thread.joinable());
    _thread = boost::thread([this] { this->run(); });
  }

  /********************************************************************************************************************/

  void TriggerFanOut::deactivate() {
    if(_thread.joinable()) {
      _thread.interrupt();
      if(_externalTrigger->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
        _externalTrigger->interrupt();
      }
      _thread.join();
    }
    assert(!_thread.joinable());
  }

  /********************************************************************************************************************/

  namespace {
    struct SendDataToConsumers {
      SendDataToConsumers(VersionNumber theVersion, DataValidity initialTriggerValidity)
      : version(theVersion), triggerValidity(initialTriggerValidity) {}

      template<typename PAIR>
      void operator()(PAIR& pair) const {
        auto theMap = pair.second; // map of feeder to FeedingFanOut (i.e. part of
                                   // the fanOutMap)

        // iterate over all feeder/FeedingFanOut pairs
        for(auto& network : theMap) {
          auto feeder = network.first;
          auto fanOut = network.second;
          fanOut->setDataValidity((triggerValidity == DataValidity::ok && feeder->dataValidity() == DataValidity::ok) ?
                  DataValidity::ok :
                  DataValidity::faulty);
          fanOut->accessChannel(0).swap(feeder->accessChannel(0));
          // don't use write destructively. In case of an exception we still need the data for the next read (see
          // Exception Handling spec B.2.2.6)
          bool dataLoss = fanOut->write(version);
          if(dataLoss) {
            Application::incrementDataLossCounter(fanOut->getName());
          }
          // swap the data back to the feeder so we have a valid copy there.
          fanOut->accessChannel(0).swap(feeder->accessChannel(0));
        }
      }

      VersionNumber version;
      DataValidity triggerValidity;
    };
  } // namespace

  /********************************************************************************************************************/

  void TriggerFanOut::run() {
    Application::registerThread("TrFO" + _externalTrigger->getName());
    Application::getInstance().getTestableMode().lock("start");
    _testableModeReached = true;

    ChimeraTK::VersionNumber version = Application::getInstance().getStartVersion();

    // Wait for the initial value of the trigger. There always will be one, and if we don't read it here we would
    // trigger the loop twice.
    _externalTrigger->read();
    version = _externalTrigger->getVersionNumber();

    // Wait until the device has been initialised for the first time. This means it
    // has been opened, and the check in TransferGroup::read() will not throw a logic_error
    // We don't have to store the lock. Just need it as a synchronisation point.
    // But we have to increase the testable mode counter because we don't want to fall out of testable mode at this
    // point already.
    if(Application::getInstance().getTestableMode().isEnabled()) {
      ++Application::getInstance().getTestableMode()._deviceInitialisationCounter;
    }
    Application::getInstance().getTestableMode().unlock("WaitInitialValueLock");
    (void)_deviceModule.waitForInitialValues();
    Application::getInstance().getTestableMode().lock("Enter while loop");
    if(Application::getInstance().getTestableMode().isEnabled()) {
      --Application::getInstance().getTestableMode()._deviceInitialisationCounter;
    }

    while(true) {
      _transferGroup.read();
      // send the version number to the consumers
      boost::fusion::for_each(_fanOutMap.table, SendDataToConsumers(version, _externalTrigger->dataValidity()));

      // wait for external trigger
      boost::this_thread::interruption_point();
      _externalTrigger->read();
      boost::this_thread::interruption_point();
      version = _externalTrigger->getVersionNumber();
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
