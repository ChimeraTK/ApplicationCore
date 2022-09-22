// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "RecoveryHelper.h"
#include "StatusWithMessage.h"
#include "VoidAccessor.h"

#include <ChimeraTK/Device.h>

#include <boost/thread/latch.hpp>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  /** Implements access to a ChimeraTK::Device.
   */
  class DeviceManager : public ApplicationModule {
   public:
    /**
     * Create DeviceManager which handles device exceptions and performs the recovery.
     */
    DeviceManager(Application* application, const std::string& deviceAliasOrCDD);

    // Move constructor/assignment is not used since DeviceManagers are stored as shared_ptr only.
    DeviceManager(DeviceManager&& other) = delete;
    DeviceManager& operator=(DeviceManager&& other) = delete;

    /**
     * Use this function to report an exception. It should be called whenever a ChimeraTK::runtime_error has been caught
     * when trying to interact with this device. It is primarily used by the ExceptionHandlingDecorator, but also user
     * modules can report exception and trigger the recovery mechanism like this.
     */
    void reportException(std::string errMsg);

    void prepare() override;

    /**
     * This functions tries to open the device and set the deviceError. Once done it notifies the waiting thread(s).
     * The function is running an endless loop inside its own thread (moduleThread).
     */
    void mainLoop() override;

    DataValidity getDataValidity() const override { return DataValidity::ok; }

    void incrementDataFaultCounter() override;

    void decrementDataFaultCounter() override;

    /**
     * Add initialisation handlers to the device.
     *
     * Initialisation handlers are called after the device has been opened, or after the device is recovering
     * from an error (i.e. an accessor has thrown an exception and Device::isFunctional() returns true afterwards).
     *
     * You can add multiple handlers. They are executed in the sequence in which they are registered. If a handler
     * has been registered in the constructor, it is called first.
     *
     * The handler function is called from the DeviceModule thread (not from the thread with the accessor that threw
     * the exception). It is handed a pointer to the instance of the DeviceModule where the handler was registered. The
     * handler function may throw a ChimeraTK::runtime_error, so you don't have to catch errors thrown when accessing
     * the Device inside the handler. After a handler has thrown an exception, the following handlers are not called.
     * The DeviceModule will wait until the Device reports isFunctional() again and retry. The exception is reported to
     * other modules and the control system.
     *
     * Notice: Especially in network based devices which do not hold a permanent connection, it is not always possible
     * to predict whether the next read()/write() will succeed. In this case the Device will always report
     * isFunctional() and one just has to retry. In this case the DeviceModule will start the initialisation sequence
     * every 500 ms.
     */
    void addInitialisationHandler(std::function<void(DeviceManager*)> initialisationHandler);

    /**
     * A trigger that indicated that the device just became available again an error (in contrast to the
     * error status which is also send when the device goes away).
     * The output is public so your module can connect to it and trigger re-sending of variables that
     * have to be send to the device again. e.g. after this has re-booted.
     * Attention: It is not send the first time the device is being opened. In this case the normal startup
     * mechanism takes care that the data is send.
     * Like the deviceError, it is automatically published to the control systen to ensure that there is at least one
     * consumer connected.
     */
    VoidOutput deviceBecameFunctional{this, "deviceBecameFunctional", "", ""};

    /**
     * Add a TransferElement to the list DeviceModule::writeRecoveryOpen. This list will be written during a recovery,
     * after the constant accessors DeviceModule::writeAfterOpen are written. This is locked by a unique_lock.
     * You can get a shared_lock with getRecoverySharedLock().
     */
    void addRecoveryAccessor(boost::shared_ptr<RecoveryHelper> recoveryAccessor);

    /**
     * Each call to this function gives a unique number. It is atomically increased with each call.
     * The smallest valid write order is 1.
     */
    uint64_t writeOrder();

    /**
     * Returns a shared lock for the DeviceModule::recoveryMutex. This locks writing
     * the list DeviceModule::writeRecoveryOpen, during a recovery.
     */
    boost::shared_lock<boost::shared_mutex> getRecoverySharedLock();

    /**
     * Wait for initial values coming from the device. This function will block until the device is opened and
     * initialised, and initial values can be read from it.
     */
    void waitForInitialValues();

    std::list<EntityOwner*> getInputModulesRecursively(std::list<EntityOwner*> startList) override;

    size_t getCircularNetworkHash() override;

    /**
     * Return associated device alias resp. URI
     */
    const std::string& getDeviceAliasOrURI() const { return _deviceAliasOrURI; }

    std::string getVirtualQualifiedName() const override { throw ChimeraTK::logic_error("Not implemented."); }

    /**
     * Create and return list of VariableNetworkNodes for all device registers
     */
    std::vector<VariableNetworkNode> getNodesList() const;

    /**
     * Return the underlying ChimeraTK::Device object.
     */
    [[nodiscard]] Device& getDevice() { return _device; }

   protected:
    /**
     * Use this function to read the exception version number. It is locking the variable mutex correctly for you.
     */
    VersionNumber getExceptionVersionNumber();

    Device _device;

    std::string _deviceAliasOrURI;

    Application* _owner{nullptr};

    /**
     * A VariableGroup for exception status and message. It can be protected, as it is automatically connected to the
     * control system in DeviceModule::defineConnections()
     */
    StatusWithMessage _deviceError{this, "status", "Error status of the device"};

    /**
     * Queue used for communication between reportException() and the moduleThread.
     */
    cppext::future_queue<std::string> _errorQueue{5};

    /**
     * Mutex to protect deviceHasError.
     * Attention: In testable mode this mutex must only be locked when holding the testable mode mutex!
     */
    boost::shared_mutex _errorMutex;

    /**
     * Version number of the last exception. Only access under the error mutex. Intentionally not initialised with
     * nullptr. It is propagated as long as the device is not successfully opened.
     */
    VersionNumber _exceptionVersionNumber = {};

    /** The error flag whether the device is functional. protected by the errorMutex. */
    bool _deviceHasError{true};

    /**
     * List of TransferElements to be written after the device has been recovered.
     * See function addRecoveryAccessor() for details.
     */
    std::list<boost::shared_ptr<RecoveryHelper>> _recoveryHelpers;

    /* The list of initialisation handler callback functions */
    std::list<std::function<void(DeviceManager*)>> _initialisationHandlers;

    /** Mutex for writing the DeviceModule::writeRecoveryOpen.*/
    boost::shared_mutex _recoveryMutex;

    /**
     * Latch to halt accessors until initial values can be received.
     * Must be a latch and not a mutex as it is locked in a different thread than unlocked.
     */
    bool _isHoldingInitialValueLatch{true};
    boost::latch _initialValueLatch{1};

    std::atomic<int64_t> _synchronousTransferCounter{0};
    std::atomic<uint64_t> _writeOrderCounter{0};

    std::list<RegisterPath> _writeRegisterPaths;
    std::list<RegisterPath> _readRegisterPaths;

    friend struct detail::CircularDependencyDetector;

    template<typename UserType>
    friend class ExceptionHandlingDecorator;
  };

  /*********************************************************************************************************************/

} // namespace ChimeraTK
