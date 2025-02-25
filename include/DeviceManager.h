// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "RecoveryHelper.h"
#include "StatusWithMessage.h"
#include "VoidAccessor.h"

#include <ChimeraTK/Device.h>

#include <boost/thread/latch.hpp>

#include <barrier>
#include <memory>

namespace ChimeraTK {

  /********************************************************************************************************************/

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
    void reportException(const std::string& errMsg);

    void prepare() override;

    /**
     * Wrapper around the actual main loop implementation to add unsubscribing from the barrier to allow a clean
     * application termination.
     */
    void mainLoop() override;

    /**
     * This functions tries to open the device and set the deviceError. Once done it notifies the waiting thread(s).
     * The function is running an endless loop inside its own thread (moduleThread).
     */
    void mainLoopImpl();

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
     * The handler function is called from the DeviceManager thread (not from the thread with the accessor that threw
     * the exception). It is handed a pointer to the instance of the DeviceManager where the handler was registered. The
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
    void addInitialisationHandler(std::function<void(ChimeraTK::Device&)> initialisationHandler);

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
    VoidOutput deviceBecameFunctional{this, "deviceBecameFunctional", ""};

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

    size_t getCircularNetworkHash() const override;

    /**
     * Return associated device alias resp. URI
     */
    const std::string& getDeviceAliasOrURI() const { return _deviceAliasOrCDD; }

    /**
     * Create and return list of VariableNetworkNodes for all device registers
     */
    std::vector<VariableNetworkNode> getNodesList() const;

    /**
     * Return the underlying ChimeraTK::Device object.
     */
    [[nodiscard]] Device& getDevice() { return _device; }

    void terminate() override;

   protected:
    /**
     * Use this function to read the exception version number. It is locking the variable mutex correctly for you.
     */
    VersionNumber getExceptionVersionNumber();

    Device _device;

    std::string _deviceAliasOrCDD;

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
    std::list<std::function<void(ChimeraTK::Device&)>> _initialisationHandlers;

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

    /**
     * The shared state of a group of DeviceManagers which are recovering together.
     */
    struct RecoveryGroup {
      enum class RecoveryStage { NO_ERROR, DETECTION, OPEN, INIT_HANDLERS, RECOVERY_ACCESSORS, CLEAR_ERROR };
      static constexpr const char* stageToString(RecoveryStage stage) {
        switch(stage) {
          case RecoveryStage::NO_ERROR:
            return "RecoveryStage::NO_ERROR";
          case RecoveryStage::DETECTION:
            return "RecoveryStage::DETECTION";
          case RecoveryStage::OPEN:
            return "RecoveryStage::OPEN";
          case RecoveryStage::INIT_HANDLERS:
            return "RecoveryStage::INIT_HANDLERS";
          case RecoveryStage::RECOVERY_ACCESSORS:
            return "RecoveryStage::RECOVERY_ACCESSORS";
          case RecoveryStage::CLEAR_ERROR:
            return "RecoveryStage::CLEAR_ERROR";
        }
        throw ChimeraTK::logic_error("Unknown recovery stage, cannot convert to string.");
      }

      std::set<DeviceBackend::BackendID> recoveryBackendIDs; ///< All backend ID in this recovery group
      Application* app{nullptr}; ///< Pointer to the application to access the recovery lock.

      /**
       * A barrier is used to ensure that each stage of the recovery process is completed
       * by all DeviceManagers in the recovery group before the next stage is started.
       * \li Detection of the error condition
       * \li Re-opening of the device
       * \li Running the initialisation handlers
       * \li Writing the recovery accessors
       */
      std::barrier<> recoveryBarrier{1};

      /** Indicator whether recovery has to be repeated, and from which barrier.
       * It is important to specify at which stage the error has occured to avoid a race condition (see code comment in
       * the \ref waitForRecoveryStage_comment waitForRecoveryStage implementation).
       */
      std::atomic<RecoveryStage> errorAtStage{RecoveryStage::NO_ERROR};

      /** Indicate that all DeviceManagers in the group should terminate their main loop. */
      std::atomic<bool> shutdown{false};

      // Wait at the barrier for a stage to complete.
      // Returns 'true' if the stage was completed successfully.
      bool waitForRecoveryStage(RecoveryStage stage);

      void setErrorAtStage(RecoveryStage stage);

      // contains a barrier to wait that all threads have seen the change.
      void resetErrorAtStage();
    };
    std::shared_ptr<RecoveryGroup> _recoveryGroup;

    /// Helper function for better error messages
    std::string stageToString(RecoveryGroup::RecoveryStage stage);
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
