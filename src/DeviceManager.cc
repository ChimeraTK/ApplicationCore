// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "DeviceManager.h"

#include "Utilities.h"

namespace ChimeraTK {

  /*********************************************************************************************************************/

  DeviceManager::DeviceManager(Application* application, const std::string& deviceAliasOrURI)
  : ApplicationModule(application, "/Devices/" + Utilities::stripName(deviceAliasOrURI, false), ""),
    _device(deviceAliasOrURI), _deviceAliasOrURI(deviceAliasOrURI), _owner{application} {}

  /*********************************************************************************************************************/

  std::vector<VariableNetworkNode> DeviceManager::getNodesList() const {
    std::vector<VariableNetworkNode> rv;

    // obtain register catalogue
    auto catalog = _device.getRegisterCatalogue();

    // iterate catalogue, create VariableNetworkNode for all registers
    for(const auto& reg : catalog) {
      // ignore 2D registers
      if(reg.getNumberOfDimensions() > 1) continue;

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

      // guess type
      const std::type_info* valTyp{&typeid(AnyType)};
      const auto& dd = reg.getDataDescriptor(); // numeric, string, boolean, nodata, undefined
      if(dd.fundamentalType() == DataDescriptor::FundamentalType::numeric) {
        if(dd.isIntegral()) {
          if(dd.isSigned()) {
            if(dd.nDigits() > 11) {
              valTyp = &typeid(int64_t);
            }
            else if(dd.nDigits() > 6) {
              valTyp = &typeid(int32_t);
            }
            else if(dd.nDigits() > 4) {
              valTyp = &typeid(int16_t);
            }
            else {
              valTyp = &typeid(int8_t);
            }
          }
          else {
            if(dd.nDigits() > 10) {
              valTyp = &typeid(uint64_t);
            }
            else if(dd.nDigits() > 5) {
              valTyp = &typeid(uint32_t);
            }
            else if(dd.nDigits() > 3) {
              valTyp = &typeid(uint16_t);
            }
            else {
              valTyp = &typeid(uint8_t);
            }
          }
        }
        else { // fractional
          // Maximum number of decimal digits to display a float without loss in non-exponential display, including
          // sign, leading 0, decimal dot and one extra digit to avoid rounding issues (hence the +4).
          // This computation matches the one performed in the NumericAddressedBackend catalogue.
          size_t floatMaxDigits = 4 +
              size_t(std::max(std::log10(std::numeric_limits<float>::max()),
                  -std::log10(std::numeric_limits<float>::denorm_min())));
          if(dd.nDigits() > floatMaxDigits) {
            valTyp = &typeid(double);
          }
          else {
            valTyp = &typeid(float);
          }
        }
      }
      else if(dd.fundamentalType() == DataDescriptor::FundamentalType::boolean) {
        valTyp = &typeid(ChimeraTK::Boolean);
      }
      else if(dd.fundamentalType() == DataDescriptor::FundamentalType::string) {
        valTyp = &typeid(std::string);
      }
      else if(dd.fundamentalType() == DataDescriptor::FundamentalType::nodata) {
        valTyp = &typeid(ChimeraTK::Void);
      }

      // create node and add to list
      rv.emplace_back(reg.getRegisterName(), _deviceAliasOrURI, reg.getRegisterName(), updateMode, direction, *valTyp,
          reg.getNumberOfElements());
    }

    return rv;
  }

  /*********************************************************************************************************************/

  void DeviceManager::reportException(std::string errMsg) {
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
      if(_errorQueue.push(std::move(errMsg))) {
        if(_owner->getTestableMode().isEnabled()) ++_owner->getTestableMode().counter;
      } // else do nothing. There are plenty of errors reported already: The queue is full.
      // set the error flag and notify the other threads
      _deviceHasError = true;
      _exceptionVersionNumber = {}; // generate a new exception version number
      errorLock.unlock();
    }
    else {
      errorLock.unlock();
    }
  }

  /*********************************************************************************************************************/

  VersionNumber DeviceManager::getExceptionVersionNumber() {
    boost::shared_lock<boost::shared_mutex> errorLock(_errorMutex);
    return _exceptionVersionNumber;
  }

  /*********************************************************************************************************************/

  void DeviceManager::mainLoop() {
    Application::registerThread("DM_" + getName());
    std::string error;

    // We have the testable mode lock. The device has not been initialised yet, but from now on the
    // testableMode.deviceInitialisationCounter will take care or it
    _testableModeReached = true;

    // flag whether the devices was opened+initialised for the first time
    bool firstSuccess = true;

    while(true) {
      // [Spec: 2.3.1] (Re)-open the device.
      do {
        _owner->getTestableMode().unlock("Wait before open/recover device");
        usleep(500000);
        boost::this_thread::interruption_point();
        _owner->getTestableMode().lock("Attempt open/recover device");
        try {
          // The globalDeviceOpenMutex is a work around for backends which do not implement open() in a thread-safe
          // manner. This seems to be the case for most backends currently, hence it was decided to implement this
          // workaround for now (see #11478).
          static std::mutex globalDeviceOpenMutex;
          std::lock_guard<std::mutex> globalDeviceOpenLock(globalDeviceOpenMutex);
          _device.open();
        }
        catch(ChimeraTK::runtime_error& e) {
          assert(_deviceError._status != StatusOutput::Status::OK); // any error must already be reported...
          if(std::string(_deviceError._message) != e.what()) {
            std::cerr << "Device " << _deviceAliasOrURI << " reports error: " << e.what() << std::endl;
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
        if(_owner->getTestableMode().enabled) {
          assert(_owner->getTestableMode().counter > 0);
          --_owner->getTestableMode().counter;
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

      // [Spec: 2.3.2] Run initialisation handlers
      try {
        for(auto& initHandler : _initialisationHandlers) {
          initHandler(_device);
        }
      }
      catch(ChimeraTK::runtime_error& e) {
        assert(_deviceError._status != StatusOutput::Status::OK); // any error must already be reported...
        // update error message, since it might have been changed...
        if(std::string(_deviceError._message) != e.what()) {
          std::cerr << "Device " << _deviceAliasOrURI << " reports error: " << e.what() << std::endl;
          setCurrentVersionNumber({});
          _deviceError.write(StatusOutput::Status::FAULT, e.what());
        }
        // Jump back to re-opening the device
        continue;
      }

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
          std::cerr << "Device " << _deviceAliasOrURI << " reports error: " << e.what() << std::endl;
          setCurrentVersionNumber({});
          _deviceError.write(StatusOutput::Status::FAULT, e.what());
        }
        // Jump back to re-opening the device
        continue;
      }

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
        std::cerr << "Device " << _deviceAliasOrURI << " error cleared." << std::endl;
      }
      firstSuccess = false;

      // decrement special testable mode counter, was incremented manually above to make sure initialisation completes
      // within one "application step"
      if(Application::getInstance().getTestableMode().enabled) --_owner->getTestableMode().deviceInitialisationCounter;

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
      if(Application::getInstance().getTestableMode().enabled) {
        ++_owner->getTestableMode().deviceInitialisationCounter; // matched above with a decrement
      }

      errorLock.lock(); // we need both locks to modify the queue

      auto popResult = _errorQueue.pop(error);
      assert(popResult); // this if should always be true, otherwise the waiting did not work.
      (void)popResult;   // avoid warning in production build. g++5.4 does not support [[maybe_unused]] yet.
      if(_owner->getTestableMode().enabled) {
        assert(_owner->getTestableMode().counter > 0);
        --_owner->getTestableMode().counter;
      }

      // [ExceptionHandling Spec: C.3.3.14] report exception to the control system
      std::cerr << "Device " << _deviceAliasOrURI << " reports error: " << error << std::endl;
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

  /*********************************************************************************************************************/

  void DeviceManager::prepare() {
    // Set initial status to error
    setCurrentVersionNumber({});
    _deviceError.write(StatusOutput::Status::FAULT, "Attempting to open device...");

    // Increment special testable mode counter to make sure the initialisation completes within one
    // "application step". Start with counter increased (device not initialised yet, wait).
    // We can to this here without testable mode lock because the application is still single threaded.
    if(Application::getInstance().getTestableMode().enabled) {
      ++_owner->getTestableMode().deviceInitialisationCounter; // released and increased in handeException loop
    }
  }

  /*********************************************************************************************************************/

  void DeviceManager::addInitialisationHandler(std::function<void(ChimeraTK::Device&)> initialisationHandler) {
    _initialisationHandlers.push_back(std::move(initialisationHandler));
  }

  /*********************************************************************************************************************/

  void DeviceManager::addRecoveryAccessor(boost::shared_ptr<RecoveryHelper> recoveryAccessor) {
    _recoveryHelpers.push_back(std::move(recoveryAccessor));
  }

  /*********************************************************************************************************************/

  uint64_t DeviceManager::writeOrder() {
    return ++_writeOrderCounter;
  }

  /*********************************************************************************************************************/

  boost::shared_lock<boost::shared_mutex> DeviceManager::getRecoverySharedLock() {
    return boost::shared_lock<boost::shared_mutex>(_recoveryMutex);
  }

  /*********************************************************************************************************************/

  void DeviceManager::waitForInitialValues() {
    _initialValueLatch.wait();
  }

  /*********************************************************************************************************************/

  std::list<EntityOwner*> DeviceManager::getInputModulesRecursively(std::list<EntityOwner*> startList) {
    // The DeviceManager does not process the device registers, and hence circular networks involving the DeviceManager
    // are not truely circular. Hence no real circular network cecking is done here.

    // If the startList is empty, the recursion scan might be about the status/control variables of the DeviceManager.
    // Hence we add the DeviceManager to the empty list.
    if(startList.empty()) {
      startList.push_back(this);
    }
    return startList;
  }

  /*********************************************************************************************************************/

  size_t DeviceManager::getCircularNetworkHash() const {
    return 0; // The device module is never part of a circular network
  }

  /*********************************************************************************************************************/

  void DeviceManager::incrementDataFaultCounter() {
    throw ChimeraTK::logic_error("incrementDataFaultCounter() called on a DeviceManager. This is probably "
                                 "caused by incorrect ownership of variables/accessors or VariableGroups.");
  }

  /*********************************************************************************************************************/

  void DeviceManager::decrementDataFaultCounter() {
    throw ChimeraTK::logic_error("decrementDataFaultCounter() called on a DeviceManager. This is probably "
                                 "caused by incorrect ownership of variables/accessors or VariableGroups.");
  }

  /*********************************************************************************************************************/

  void DeviceManager::terminate() {
    if(moduleThread.joinable()) {
      moduleThread.interrupt();
      // try joining the thread
      while(!moduleThread.try_join_for(boost::chrono::milliseconds(10))) {
        // send boost interrupted exception through the _errorQueue
        try {
          throw boost::thread_interrupted();
        }
        catch(boost::thread_interrupted&) {
          _errorQueue.push_exception(std::current_exception());
        }

        // it may not suffice to send the exception once, as the exception might get overwritten in the queue, thus we
        // repeat this until the thread was joined.
      }
    }
    assert(!moduleThread.joinable());
  }

  /*********************************************************************************************************************/

} // namespace ChimeraTK
