// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DeviceManager.h"

#include "Utilities.h"

#include <ChimeraTK/cppext/finally.hpp>

#include <boost/thread/exceptions.hpp>

#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  DeviceManager::DeviceManager(Application* application, const std::string& deviceAliasOrCDD)
  : ApplicationModule(application, "/Devices/" + Utilities::escapeName(deviceAliasOrCDD, false), ""),
    _device(deviceAliasOrCDD), _deviceAliasOrCDD(deviceAliasOrCDD), _owner{application} {
    auto involvedBackends = _device.getInvolvedBackendIDs();
    // Create a recovery group with barrier size 1.
    _recoveryGroup = std::make_shared<RecoveryGroup>(involvedBackends, _owner);

    // loop all already existing DeviceManagers and look for shared backends
    int64_t recoveryGroupSize{1};
    for(const auto& [alias, existingDeviceManager] : Application::getInstance().getDeviceManagerMap()) {
      for(auto backendID : involvedBackends) {
        if(existingDeviceManager->_recoveryGroup->recoveryBackendIDs.contains(backendID)) {
          // Note: The next line modifies involvedBackends while iterating over it.
          // This is only allowed because the iteration is terminated with a break below!
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
      // The barrier does not allow modification of the number of participants.
      // We put a placement new to replace it. Before this, we have to call the constructor of the old instance.
      _recoveryGroup->recoveryBarrier.~barrier();
      new(&_recoveryGroup->recoveryBarrier) std::barrier(recoveryGroupSize);
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
      _device.setException(errMsg);
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
      _recoveryGroup->shutdown = true;
      _recoveryGroup->recoveryBarrier.arrive_and_drop();
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
      // Sync point DETECTION:
      // The manager has seen an error and (re)starts recovery. Wait until all
      // involved DeviceManagers have seen it.
      _recoveryGroup->waitForRecoveryStage(RecoveryGroup::RecoveryStage::DETECTION);
      // Reset error stage to NO_ERROR. Contains a barrier to make sure all threads have seen it.
      _recoveryGroup->resetErrorAtStage();

      // Starting stage OPEN
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
          _owner->getTestableMode().unlock("throwing" + std::string(writeMe) + " is not writeable!");
          throw ChimeraTK::logic_error(std::string(writeMe) + " is not writeable!");
        }
      }

      for(auto& readMe : _readRegisterPaths) {
        auto reg = _device.getOneDRegisterAccessor<std::string>(readMe); // the user data type does not matter here.
        if(!reg.isReadable()) {
          _owner->getTestableMode().unlock("throwing" + std::string(readMe) + " is not readable!");
          throw ChimeraTK::logic_error(std::string(readMe) + " is not readable!");
        }
      }

      // Sync point (stage OPEN complete): Device opened. Synchronise before starting init scripts.
      assert(_recoveryGroup->errorAtStage ==
          RecoveryGroup::RecoveryStage::NO_ERROR); // no other thread must have modified the flag until here.

      // no need to check the return value. No error reported in the OPEN stage.
      _recoveryGroup->waitForRecoveryStage(RecoveryGroup::RecoveryStage::OPEN);

      // Starting stage INIT_HANDLERS
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
        _recoveryGroup->setErrorAtStage(RecoveryGroup::RecoveryStage::INIT_HANDLERS);
      }
      catch(...) {
        // This will terminate this DeviceManager main loop
        // Drop the testable mode lock before we bail out.
        // The other DeviceManagers in this recovery group will be informed in the mainLoop's catch all block.
        _owner->getTestableMode().unlock("ERROR in INIT_HANDLERS");
        throw;
      }

      // Sync point (stage INIT_HANDLERS complete): Wait until all init scripts are done before writing recovery
      // accessors.
      if(!_recoveryGroup->waitForRecoveryStage(RecoveryGroup::RecoveryStage::INIT_HANDLERS)) {
        // If another thread has already continued and set an error for recovery stage RECOVERY_ACCESSORS,
        // waitForRecoveryStage(INIT_HANDLERS) will still return 'true', so all threads arrive at the
        // barrier for stage RECOVERY_ACCESSORS.
        // If there was error in stage INIT_HANDLERS, all threads will see it here and continue.
        continue;
      }

      // Starting stage RECOVERY_ACCESSORS
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
        _recoveryGroup->setErrorAtStage(RecoveryGroup::RecoveryStage::RECOVERY_ACCESSORS);
      }
      catch(...) {
        // This will terminate this DeviceManager main loop
        // Drop the testable mode lock before we bail out.
        // The other DeviceManagers in this recovery group will be informed in the mainLoop's catch all block.
        _owner->getTestableMode().unlock("ERROR in RECOVERY_ACCESSORS");
        throw;
      }

      // Sync point (stage RECOVERY_ACCESSORS complete): All recovery accessors have been written.
      if(!_recoveryGroup->waitForRecoveryStage(RecoveryGroup::RecoveryStage::RECOVERY_ACCESSORS)) {
        // In case of error, jump back to the beginning of the recovery/open procedure
        continue;
      }

      // complete the recovery, then wait for the exception
      errorLock.lock();
      _deviceHasError = false;
      errorLock.unlock();

      // Sync point (stage CLEAR_ERROR complete): All DeviceManagers have cleared the error flag.
      // This barrier protects against exceptions being reported. From now on read/write operations can
      // report exceptions again, and they will not be suppressed by reportException due to an existing error condition.
      _recoveryGroup->waitForRecoveryStage(RecoveryGroup::RecoveryStage::CLEAR_ERROR);

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

  bool DeviceManager::RecoveryGroup::waitForRecoveryStage(RecoveryStage stage) {
    app->getTestableMode().unlock(std::string("DeviceManager: Sync device recovery after ") + stageToString(stage));
    if(shutdown) {
      throw boost::thread_interrupted(); // NOLINT hicpp-exception-baseclass
    }
    boost::this_thread::interruption_point();

    recoveryBarrier.arrive_and_wait();

    if(shutdown) {
      throw boost::thread_interrupted(); // NOLINT hicpp-exception-baseclass
    }
    boost::this_thread::interruption_point();
    app->getTestableMode().lock(
        std::string("DeviceManager: Starting next device recovery stage after ") + stageToString(stage));

    /** Return false if errorAtStage is the current stage.
     *   \anchor waitForRecoveryStage_comment
     * It is important to check whether the reported error has happened at the stage for which we waited at the barrier,
     * or already for the next stage which about to start.
     *
     * Only in case the error is for the stage we just completed we must return false. In this case the error has been
     * set before the barrier and all DeviceManagers see it and restart the recovery from here.
     *
     * In case the error is from the following stage, the according DeviceManager will wait at the next barrier. So
     * this DeviceManager must also proceed to that barrier to keep them in sync. Hence, we must return true here
     * (meaning the requested stage completed successfully), even though we know that an error has occured.
     */
    return !(errorAtStage == stage);
  }

  /********************************************************************************************************************/

  void DeviceManager::RecoveryGroup::setErrorAtStage(RecoveryStage stage) {
    assert((errorAtStage == RecoveryStage::NO_ERROR) || (errorAtStage == stage));
    errorAtStage = stage;
  }

  /********************************************************************************************************************/

  void DeviceManager::RecoveryGroup::resetErrorAtStage() {
    errorAtStage = RecoveryStage::NO_ERROR;

    app->getTestableMode().unlock("DeviceManager: Sync after resetting recovery group stage");
    recoveryBarrier.arrive_and_wait();
    boost::this_thread::interruption_point();
    app->getTestableMode().lock("DeviceManager: Starting recovery");
  }

} // namespace ChimeraTK
