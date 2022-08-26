// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TriggerFanOut.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  TriggerFanOut::TriggerFanOut(const boost::shared_ptr<ChimeraTK::TransferElement>& externalTriggerImpl,
      DeviceModule& deviceModule, VariableNetwork& network)
  : externalTrigger(externalTriggerImpl), _deviceModule(deviceModule), _network(network) {}

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
      if(externalTrigger->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
        externalTrigger->interrupt();
      }
      _thread.join();
    }
    assert(!_thread.joinable());
  }

  /********************************************************************************************************************/

  namespace {
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
  } // namespace

  /********************************************************************************************************************/

  void TriggerFanOut::run() {
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
      externalTrigger->read();
      boost::this_thread::interruption_point();
      version = externalTrigger->getVersionNumber();
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
