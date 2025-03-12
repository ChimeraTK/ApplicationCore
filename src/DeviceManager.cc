// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DeviceManager.h"

#include "Utilities.h"

#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  DeviceManager::DeviceManager(Application* application, const std::string& deviceAliasOrCDD)
  : ApplicationModule(application, "/Devices/" + Utilities::escapeName(deviceAliasOrCDD, false), ""),
    _device(deviceAliasOrCDD), _deviceAliasOrCDD(deviceAliasOrCDD), _owner{application} {
    auto involvedBackends = _device.getInvolvedBackendIDs();
    _recoveryGroup =
        std::make_shared<RecoveryGroup>(std::make_shared<std::barrier<>>(1), true, involvedBackends, _owner);

    // loop all already existing DeviceManagers and look for shared backends
    size_t recoveryGroupSize{1};
    for(const auto& [alias, existingDeviceManager] : Application::getInstance().getDeviceManagerMap()) {
      for(auto backendID : involvedBackends) {
        if(existingDeviceManager->_recoveryGroup->recoveryBackendIDs.contains(backendID)) {
          involvedBackends.merge(existingDeviceManager->_recoveryGroup->recoveryBackendIDs);
          existingDeviceManager->_recoveryGroup = _recoveryGroup;
          ++recoveryGroupSize;
          break;
        }
      }
    }

    if(recoveryGroupSize > 1) {
      // update the recovery group
      _recoveryGroup->recoveryBackendIDs = involvedBackends;
      _recoveryGroup->recoveryBarrier = std::make_shared<std::barrier<>>(recoveryGroupSize);
    }
  }

  /********************************************************************************************************************/

  std::vector<VariableNetworkNode> DeviceManager::getNodesList() const {
    std::vector<VariableNetworkNode> rv;

    // obtain register catalogue
    auto catalog = _device.getRegisterCatalogue();

    // iterate catalogue, create VariableNetworkNode for all registers
    for(const auto& reg : catalog) {
      // ignore 2D registers
      if(reg.getNumberOfDimensions() > 1) {
        continue;
      }

      // guess direction and determine update mode
      VariableDirection direction{};
      UpdateMode updateMode;
      if(reg.isWriteable()) {
        direction = {VariableDirection::consuming, false};
        updateMode = UpdateMode::push;
      }
      else {
        direction = {VariableDirection::feeding, false};
        if(reg.getSupportedAccessModes().has(AccessMode::wait_for_new_data)) {
          updateMode = UpdateMode::push;
        }
        else {
          updateMode = UpdateMode::poll;
        }
      }

      // find minimum type required to represent data
      const auto* valTyp = &(reg.getDataDescriptor().minimumDataType().getAsTypeInfo());

      // create node and add to list
      rv.emplace_back(reg.getRegisterName(), _deviceAliasOrCDD, reg.getRegisterName(), updateMode, direction, *valTyp,
          reg.getNumberOfElements());
    }

    return rv;
  }

  /********************************************************************************************************************/

  void DeviceManager::reportException(const std::string& errMsg) {
    if(_owner->getTestableMode().isEnabled()) {
      assert(_owner->getTestableMode().testLock());
    }

    // The error queue must only be modified when holding both mutexes (error mutex and testable mode mutex), because
    // the testable mode counter must always be consistent with the content of the queue.
    // To avoid deadlocks you must always first aquire the testable mode mutex if you need both.
    // You can hold the error mutex without holding the testable mode mutex (for instance for checking the error
    // predicate), but then you must not try to aquire the testable mode mutex!
    boost::unique_lock<boost::shared_mutex> errorLock(_errorMutex);

    if(!_deviceHasError) { // only report new errors if the device does not have reported errors already
      if(_errorQueue.push(errMsg)) {
        if(_owner->getTestableMode().isEnabled()) {
          ++_owner->getTestableMode()._counter;
        }
      } // else do nothing. There are plenty of errors reported already: The queue is full.
      // set the error flag and notify the other threads
      _deviceHasError = true;
      _exceptionVersionNumber = {}; // generate a new exception version number

      // Release the error lock before notifying the other device managers in the recovery group.
      // They will re-inform us, and holding the lock would deadlock trying to re-acquire this error mutex.
      errorLock.unlock();

      // Inform the other DeviceManagers in the recovery group.
      for(auto const& [cdd, deviceManager] : _owner->getDeviceManagerMap()) {
        if(deviceManager.get() == this) {
          continue;
        }
        if(deviceManager->_recoveryGroup == _recoveryGroup) {
          // Only append a device info to the recovery message if there is none.
          auto deviceInfo =
              (errMsg.find("[in device ") == std::string::npos) ? "[in device " + _deviceAliasOrCDD + "]" : "";
          deviceManager->reportException(errMsg + deviceInfo);
        }
      }
    }
  }

  /********************************************************************************************************************/

  VersionNumber DeviceManager::getExceptionVersionNumber() {
    boost::shared_lock<boost::shared_mutex> errorLock(_errorMutex);
    return _exceptionVersionNumber;
  }

  /********************************************************************************************************************/

  void DeviceManager::mainLoop() {
    // Whenever the loop is left, we have to drop the barrier so the other DeviceManagers are not blocked indefinitely.
    // They could never terminate in this situation, and hence the whole application could never terminate.
    try {
      mainLoopImpl();
    }
    catch(...) {
      _recoveryGroup->recoveryBarrier->arrive_and_drop();
      throw;
    }
  }

  /********************************************************************************************************************/

  void DeviceManager::mainLoopImpl() {
    Application::registerThread("DM_" + getName());
    std::string error;

    // We have the testable mode lock. The device has not been initialised yet, but from now on the
    // testableMode.deviceInitialisationCounter will take care or it
    _testableModeReached = true;

    // flag whether the devices was opened+initialised for the first time
    bool firstSuccess = true;

    while(true) {
      /****************************************************************************************************************/
      // Sync point (stage 1 complete):
      // The manager has seen an error and (re)starts recovery. Wait until all
      // involved DeviceManagers have seen it.
      /****************************************************************************************************************/
      _recoveryGroup->waitForRecoveryStage(1);
      // Reset error stage to 0. Contains a barrier to make sure all threads have seen it.
      _recoveryGroup->resetErrorStage();

      // Starting stage 2
      // [Spec: 2.3.1] (Re)-open the device.
      do {
        _owner->getTestableMode().unlock("Wait before open/recover device");
        usleep(500000);
        boost::this_thread::interruption_point();
        _owner->getTestableMode().lock("Attempt open/recover device");

        try {
          std::lock_guard<std::mutex> deviceOpenLock(_recoveryGroup->deviceOpenCloseMutex);
          _device.open();
        }
        catch(ChimeraTK::runtime_error& e) {
          assert(_deviceError._status != StatusOutput::Status::OK); // any error must already be reported...
          if(std::string(_deviceError._message) != e.what()) {
            ChimeraTK::logger(Logger::Severity::error, "Device " + _deviceAliasOrCDD) << e.what() << std::endl;
            // set proper error message in very first attempt to open the device
            setCurrentVersionNumber({});
            _deviceError.write(StatusOutput::Status::FAULT, e.what());
          }

          continue; // should not be necessary because isFunctional() should return false. But no harm in leaving it in.
        }
      } while(!_device.isFunctional());

      boost::unique_lock<boost::shared_mutex> errorLock(_errorMutex);

      // [Spec: 2.3.3] Empty exception reporting queue.
      while(_errorQueue.pop()) {
        if(_owner->getTestableMode()._enabled) {
          assert(_owner->getTestableMode()._counter > 0);
          --_owner->getTestableMode()._counter;
        }
      }
      errorLock.unlock(); // we don't need to hold the lock for now, but we will need it later

      for(auto& writeMe : _writeRegisterPaths) {
        auto reg = _device.getOneDRegisterAccessor<std::string>(writeMe); // the user data type does not matter here.
        if(!reg.isWriteable()) {
          throw ChimeraTK::logic_error(std::string(writeMe) + " is not writeable!");
        }
      }

      for(auto& readMe : _readRegisterPaths) {
        auto reg = _device.getOneDRegisterAccessor<std::string>(readMe); // the user data type does not matter here.
        if(!reg.isReadable()) {
          throw ChimeraTK::logic_error(std::string(readMe) + " is not readable!");
        }
      }

      /****************************************************************************************************************/
      // Sync point (stage 2 complete): Device opened. Synchronise before stating init scripts.
      /****************************************************************************************************************/
      assert(_recoveryGroup->errorAtStage == 0); // no other thread must have modified the flag until here.

      // no need to check the return value. No error reported in stage 2
      _recoveryGroup->waitForRecoveryStage(2);

      // Starting stage 3
      // [Spec: 2.3.2] Run initialisation handlers
      try {
        for(auto& initHandler : _initialisationHandlers) {
          {
            // Hold the open/close lock while executing the init handler, so no other
            // DeviceManager closes the device while the init handler is running.
            std::lock_guard<std::mutex> openCloseLock(_recoveryGroup->deviceOpenCloseMutex);
            _device.close();
            initHandler(_device);
            _device.open();
          }
        }
      }
      catch(ChimeraTK::runtime_error& e) {
        assert(_deviceError._status != StatusOutput::Status::OK); // any error must already be reported...
        // update error message, since it might have been changed...
        if(std::string(_deviceError._message) != e.what()) {
          ChimeraTK::logger(Logger::Severity::error, "Device " + _deviceAliasOrCDD) << e.what() << std::endl;
          setCurrentVersionNumber({});
          _deviceError.write(StatusOutput::Status::FAULT, e.what());
        }
        // Mark recovery as failed. All DeviceManagers will return to the beginning of the recovery after the next
        // synchronisation point
        _recoveryGroup->setErrorAtStage(3);
      }

      /****************************************************************************************************************/
      // Sync point (stage 3 complete): Wait until all init scripts are done before writing recovery accessors.
      /****************************************************************************************************************/
      if(!_recoveryGroup->waitForRecoveryStage(3)) {
        // If another thread has already continued and set an error for recovery stage 4,
        // waitForRecoveryStage(3) will still return 'true', so all threads arrive at the
        // barrier for stage 4.
        // If there was error in stage 3, all threads will see it here and continue.
        continue;
      }

      // Starting stage 4
      // Write all recovery accessors
      // We are now entering the critical recovery section. It is protected by the recovery mutex until the
      // deviceHasError flag has been cleared.
      boost::unique_lock<boost::shared_mutex> recoveryLock(_recoveryMutex);
      try {
        // sort recovery helpers according to write order
        _recoveryHelpers.sort([](boost::shared_ptr<RecoveryHelper>& a, boost::shared_ptr<RecoveryHelper>& b) {
          return a->writeOrder < b->writeOrder;
        });
        for(auto& recoveryHelper : _recoveryHelpers) {
          if(recoveryHelper->versionNumber != VersionNumber{nullptr}) {
            recoveryHelper->accessor->write();
            recoveryHelper->wasWritten = true;
          }
        }
      }
      catch(ChimeraTK::runtime_error& e) {
        // update error message, since it might have been changed...
        if(std::string(_deviceError._message) != e.what()) {
          ChimeraTK::logger(Logger::Severity::error, "Device " + _deviceAliasOrCDD) << e.what() << std::endl;
          setCurrentVersionNumber({});
          _deviceError.write(StatusOutput::Status::FAULT, e.what());
        }
        // Mark recovery as failed. All DeviceManagers will return to the beginning of the recovery after the next
        // synchronisation point
        _recoveryGroup->setErrorAtStage(4);
      }

      /****************************************************************************************************************/
      // Sync point (stage 4 complete): All recovery accessors written.
      /****************************************************************************************************************/
      if(!_recoveryGroup->waitForRecoveryStage(4)) {
        // In case of error, jump back to the beginning of the recovery/open procedure
        continue;
      }

      // complete the recovery, then wait for the exception
      errorLock.lock();
      _deviceHasError = false;
      errorLock.unlock();

      recoveryLock.unlock();

      // send the trigger that the device is available again
      _device.activateAsyncRead();
      if(_isHoldingInitialValueLatch) {
        _isHoldingInitialValueLatch = false;
        _initialValueLatch.count_down();
      }

      // [Spec: 2.3.5] Reset exception state and wait for the next error to be reported.
      _deviceError.writeOk();
      deviceBecameFunctional.write();

      if(!firstSuccess) {
        ChimeraTK::logger(Logger::Severity::info, "Device " + _deviceAliasOrCDD) << "Error cleared." << std::endl;
      }
      firstSuccess = false;

      // decrement special testable mode counter, was incremented manually above to make sure initialisation completes
      // within one "application step"
      if(Application::getInstance().getTestableMode()._enabled) {
        --_owner->getTestableMode()._deviceInitialisationCounter;
      }

      // [Spec: 2.3.8] Wait for an exception being reported by the ExceptionHandlingDecorators
      // release the testable mode mutex for waiting for the exception.
      _owner->getTestableMode().unlock("Wait for exception");

      // Do not modify the queue without holding the testable mode lock, because we also consistently have to modify
      // the counter protected by that mutex.
      // Just call wait(), not pop_wait().
      boost::this_thread::interruption_point();
      _errorQueue.wait();
      boost::this_thread::interruption_point();

      _owner->getTestableMode().lock("Process exception");
      // increment special testable mode counter to make sure the initialisation completes within one
      // "application step"
      if(Application::getInstance().getTestableMode()._enabled) {
        ++_owner->getTestableMode()._deviceInitialisationCounter; // matched above with a decrement
      }

      errorLock.lock(); // we need both locks to modify the queue

      auto popResult = _errorQueue.pop(error);
      assert(popResult); // this if should always be true, otherwise the waiting did not work.
      (void)popResult;   // avoid warning in production build. g++5.4 does not support [[maybe_unused]] yet.
      if(_owner->getTestableMode()._enabled) {
        assert(_owner->getTestableMode()._counter > 0);
        --_owner->getTestableMode()._counter;
      }

      // [ExceptionHandling Spec: C.3.3.14] report exception to the control system
      ChimeraTK::logger(Logger::Severity::error, "Device " + _deviceAliasOrCDD) << error << std::endl;
      setCurrentVersionNumber({});
      _deviceError.write(StatusOutput::Status::FAULT, error);

      // We must not hold the lock while waiting for the synchronousTransferCounter to go back to 0. Only release it
      // after deviceError has been written, so the CircularDependencyDetector can read the error message from its
      // thread for printing.
      errorLock.unlock();

      // [ExceptionHandling Spec: C.3.3.15] Wait for all synchronous transfers to finish before starting recovery.
      while(_synchronousTransferCounter > 0) {
        usleep(1000);
      }

    } // while(true)
  }

  /********************************************************************************************************************/

  void DeviceManager::prepare() {
    // Set initial status to error
    setCurrentVersionNumber({});
    _deviceError.write(StatusOutput::Status::FAULT, "Attempting to open device...");

    // Increment special testable mode counter to make sure the initialisation completes within one
    // "application step". Start with counter increased (device not initialised yet, wait).
    // We can to this here without testable mode lock because the application is still single threaded.
    if(Application::getInstance().getTestableMode()._enabled) {
      ++_owner->getTestableMode()._deviceInitialisationCounter; // released and increased in handeException loop
    }
  }

  /********************************************************************************************************************/

  void DeviceManager::addInitialisationHandler(std::function<void(ChimeraTK::Device&)> initialisationHandler) {
    _initialisationHandlers.push_back(std::move(initialisationHandler));
  }

  /********************************************************************************************************************/

  void DeviceManager::addRecoveryAccessor(boost::shared_ptr<RecoveryHelper> recoveryAccessor) {
    _recoveryHelpers.push_back(std::move(recoveryAccessor));
  }

  /********************************************************************************************************************/

  uint64_t DeviceManager::writeOrder() {
    return ++_writeOrderCounter;
  }

  /********************************************************************************************************************/

  boost::shared_lock<boost::shared_mutex> DeviceManager::getRecoverySharedLock() {
    return boost::shared_lock<boost::shared_mutex>(_recoveryMutex);
  }

  /********************************************************************************************************************/

  void DeviceManager::waitForInitialValues() {
    _initialValueLatch.wait();
  }

  /********************************************************************************************************************/

  std::list<EntityOwner*> DeviceManager::getInputModulesRecursively(std::list<EntityOwner*> startList) {
    // The DeviceManager does not process the device registers, and hence circular networks involving the
    // DeviceManager are not truly circular. Hence no real circular network checking is done here.

    // If the startList is empty, the recursion scan might be about the status/control variables of the DeviceManager.
    // Hence we add the DeviceManager to the empty list.
    if(startList.empty()) {
      startList.push_back(this);
    }
    return startList;
  }

  /********************************************************************************************************************/

  size_t DeviceManager::getCircularNetworkHash() const {
    return 0; // The device module is never part of a circular network
  }

  /********************************************************************************************************************/

  void DeviceManager::incrementDataFaultCounter() {
    throw ChimeraTK::logic_error("incrementDataFaultCounter() called on a DeviceManager. This is probably "
                                 "caused by incorrect ownership of variables/accessors or VariableGroups.");
  }

  /********************************************************************************************************************/

  void DeviceManager::decrementDataFaultCounter() {
    throw ChimeraTK::logic_error("decrementDataFaultCounter() called on a DeviceManager. This is probably "
                                 "caused by incorrect ownership of variables/accessors or VariableGroups.");
  }

  /********************************************************************************************************************/

  void DeviceManager::terminate() {
    if(_moduleThread.joinable()) {
      _moduleThread.interrupt();
      // try joining the thread
      while(!_moduleThread.try_join_for(boost::chrono::milliseconds(10))) {
        // send boost interrupted exception through the _errorQueue
        _errorQueue.push_exception(std::make_exception_ptr(boost::thread_interrupted()));

        // it may not suffice to send the exception once, as the exception might get overwritten in the queue, thus we
        // repeat this until the thread was joined.
      }
    }
    assert(!_moduleThread.joinable());
  }

  /********************************************************************************************************************/

  bool DeviceManager::RecoveryGroup::waitForRecoveryStage(size_t stage) {
    app->getTestableMode().unlock("Sync after after " + std::to_string(stage));
    recoveryBarrier->arrive_and_wait();
    boost::this_thread::interruption_point();
    app->getTestableMode().lock("Starting stage " + std::to_string(stage + 1));

    // Return false if errorAtStage is the current stage.
    return !(errorAtStage == stage);
  }

  /********************************************************************************************************************/

  void DeviceManager::RecoveryGroup::setErrorAtStage(size_t stage) {
    assert((errorAtStage == 0) || (errorAtStage == stage));
    errorAtStage = stage;
  }

  /********************************************************************************************************************/

  void DeviceManager::RecoveryGroup::resetErrorStage() {
    errorAtStage = 0;

    app->getTestableMode().unlock("Sync before resetting recovery group stage");
    recoveryBarrier->arrive_and_wait();
    boost::this_thread::interruption_point();
    app->getTestableMode().lock("Starting recovery");
  }

} // namespace ChimeraTK
