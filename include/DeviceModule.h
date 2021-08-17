/*
 * DeviceModule.h
 *
 *  Created on: Jun 27, 2016
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_DEVICE_MODULE_H
#define CHIMERATK_DEVICE_MODULE_H

#include "ControlSystemModule.h"
#include "Module.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"
#include "VariableNetworkNode.h"
#include "VirtualModule.h"
#include "RecoveryHelper.h"
#include <ChimeraTK/ForwardDeclarations.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/Device.h>
#include "ModuleGroup.h"
#include "StatusAccessor.h"
#include <boost/thread/latch.hpp>

namespace ChimeraTK {
  class Application;
  class DeviceModule;
  namespace history {
    struct ServerHistory;
  }

  /*********************************************************************************************************************/

  namespace detail {
    struct DeviceModuleProxy : Module {
      DeviceModuleProxy(const DeviceModule& owner, const std::string& registerNamePrefix);
      DeviceModuleProxy(DeviceModuleProxy&& other);
      DeviceModuleProxy() {}

      VariableNetworkNode operator()(const std::string& registerName, UpdateMode mode,
          const std::type_info& valueType = typeid(AnyType), size_t nElements = 0) const;
      VariableNetworkNode operator()(const std::string& registerName, const std::type_info& valueType,
          size_t nElements = 0, UpdateMode mode = UpdateMode::poll) const;
      VariableNetworkNode operator()(const std::string& variableName) const override;
      Module& operator[](const std::string& moduleName) const override;

      const Module& virtualise() const override;
      void connectTo(const Module& target, VariableNetworkNode trigger = {}) const override;
      ModuleType getModuleType() const override { return ModuleType::Device; }

      DeviceModuleProxy& operator=(DeviceModuleProxy&& other);

     private:
      friend class ChimeraTK::DeviceModule;
      const DeviceModule* _myowner;
      std::string _registerNamePrefix;
    };
  } // namespace detail

  /*********************************************************************************************************************/

  /** Implementes access to a ChimeraTK::Device.
   */
  class DeviceModule : public Module {
   public:
    /**
     *  Create (non-connecting) DeviceModule
     *
     *  The device represented by this DeviceModule is identified by either the device alias found in the DMAP file or
     *  directly a CDD.
     *
     *  A callback function to initialise the device can be registered as an optional argument (see
     *  addInitialisationHandler() for more information).
     *
     *  Connecting the device to other modules is up to the user, hence using this class directly is discouraged in
     *  new applications. Instead use the ConnectingDeviceModule.
     */
    DeviceModule(Application* application, const std::string& deviceAliasOrCDD,
        std::function<void(DeviceModule*)> initialisationHandler = nullptr);

    /** Destructor */
    virtual ~DeviceModule();

    /** Move operation with the move constructor */
    DeviceModule(DeviceModule&& other) {
      operator=(std::move(other));
    }

    /** Move assignment */
    DeviceModule& operator=(DeviceModule&& other) {
      assert(!moduleThread.joinable());
      assert(other.isHoldingInitialValueLatch);
      if(owner) owner->unregisterDeviceModule(this);
      Module::operator=(std::move(other));
      device = std::move(other.device);
      deviceAliasOrURI = std::move(other.deviceAliasOrURI);
      registerNamePrefix = std::move(other.registerNamePrefix);
      deviceError = std::move(other.deviceError);
      owner = other.owner;
      proxies = std::move(other.proxies);
      deviceHasError = other.deviceHasError;
      for(auto& proxy : proxies) proxy.second._myowner = this;
      owner->registerDeviceModule(this);
      return *this;
    }
    /** The subscript operator returns a VariableNetworkNode which can be used in
     * the Application::initialise()
     *  function to connect the register with another variable. */
    VariableNetworkNode operator()(const std::string& registerName, UpdateMode mode,
        const std::type_info& valueType = typeid(AnyType), size_t nElements = 0) const;
    VariableNetworkNode operator()(const std::string& registerName, const std::type_info& valueType,
        size_t nElements = 0, UpdateMode mode = UpdateMode::poll) const {
      return operator()(registerName, mode, valueType, nElements);
    }
    VariableNetworkNode operator()(const std::string& variableName) const override {
      return operator()(variableName, UpdateMode::poll);
    }

    Module& operator[](const std::string& moduleName) const override;

    const Module& virtualise() const override;

    void connectTo(const Module& target, VariableNetworkNode trigger = {}) const override;

    ModuleType getModuleType() const override { return ModuleType::Device; }

    /** Use this function to report an exception. It should be called whenever a
     * ChimeraTK::runtime_error has been caught when trying to interact with this
     * device. It is primarily used by the ExceptionHandlingDecorator, but also user modules
     * can report exception and trigger the recovery mechanism like this. */
    void reportException(std::string errMsg);

    void prepare() override;

    void run() override;

    void terminate() override;

    VersionNumber getCurrentVersionNumber() const override { return currentVersionNumber; }

    void setCurrentVersionNumber(VersionNumber versionNumber) override {
      if(versionNumber > currentVersionNumber) currentVersionNumber = versionNumber;
    }

    VersionNumber currentVersionNumber{nullptr};

    /** This function connects DeviceError VariableGroup to ContolSystem*/
    void defineConnections() override;

    mutable Device device;

    DataValidity getDataValidity() const override { return DataValidity::ok; }
    void incrementDataFaultCounter() override {
      throw ChimeraTK::logic_error("incrementDataFaultCounter() called on a DeviceModule. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    void decrementDataFaultCounter() override {
      throw ChimeraTK::logic_error("decrementDataFaultCounter() called on a DeviceModule. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }

    /** Add initialisation handlers to the device.
     *
     *  Initialisation handlers are called after the device has been opened, or after the device is recovering
     *  from an error (i.e. an accessor has thrown an exception and Device::isFunctional() returns true afterwards).
     *
     *  You can add mupltiple handlers. They are executed in the sequence in which they are registered. If a handler
     *  has been registered in the constructor, it is called first.
     *
     *  The handler function is called from the DeviceModule thread (not from the thread with the accessor that threw the exception).
     *  It is handed a pointer to the instance of the DeviceModule
     *  where the handler was registered. The handler function may throw a ChimeraTK::runtime_error, so you don't have to
     *  catch errors thrown when accessing the Device inside the handler. After a handler has thrown an exception, the
     *  following handlers are not called. The DeviceModule will wait until the Device reports isFunctional() again and retry.
     *  The exception is reported to other modules and the control system.
     *
     *  Notice: Especially in network based devices which do not hold a permanent connection, it is not always possible
     *  to predict whether the next read()/write() will succeed. In this case the Device will always report isFunctional()
     *  and one just has to retry. In this case the DeviceModule will start the initialisation sequence every 500 ms.
     */
    void addInitialisationHandler(std::function<void(DeviceModule*)> initialisationHandler);

    /** A trigger that indicated that the device just became available again an error (in contrast to the
      *  error status which is also send when the device goes away).
      *  The output is public so your module can connect to it and trigger re-sending of variables that
      *  have to be send to the device again. e.g. after this has re-booted.
      *  Attention: It is not send the first time the device is being opened. In this case the normal startup
      *  mechanism takes care that the data is send.
      *  Like the deviceError, it is automatically published to the control systen to ensure that there is at least one
      *  consumer connected.
      */
    ScalarOutput<int> deviceBecameFunctional{
        this, "deviceBecameFunctional", "", ""}; // should be changed to data type void

    /** Add a TransferElement to the list DeviceModule::writeRecoveryOpen. This list will be written during a recovery,
     * after the constant accessors DeviceModule::writeAfterOpen are written. This is locked by a unique_lock.
     * You can get a shared_lock with getRecoverySharedLock(). */
    void addRecoveryAccessor(boost::shared_ptr<RecoveryHelper> recoveryAccessor);

    /** Each call to this function gives a unique number. It is atomically increased with each call.
     *  The smalled valid write order is 1.
     */
    uint64_t writeOrder();

    /** Returns a shared lock for the DeviceModule::recoveryMutex. This locks writing
     * the list DeviceModule::writeRecoveryOpen, during a recovery.*/
    boost::shared_lock<boost::shared_mutex> getRecoverySharedLock();

    /** 
     *  Wait for initial values coming from the device. This function will block until the device is opened and
     *  initialised, and initial values can be read from it.
     */
    void waitForInitialValues();

    std::list<EntityOwner*> getInputModulesRecursively(std::list<EntityOwner*> startList) override;

    size_t getCircularNetworkHash() override;

    /**
     *  Return associated device alias resp. URI
     */
    std::string getDeviceAliasOrURI() const { return deviceAliasOrURI; }

   protected:
    // populate virtualisedModuleFromCatalog based on the information in the
    // device's catalogue
    VirtualModule& virtualiseFromCatalog() const;

    mutable VirtualModule virtualisedModuleFromCatalog{"INVALID", "", ModuleType::Invalid};
    mutable bool virtualisedModuleFromCatalog_isValid{false};

    std::string deviceAliasOrURI;
    ChimeraTK::RegisterPath registerNamePrefix;

    // List of proxies accessed through the operator[]. This is mutable since
    // it is little more than a cache and thus does not change the logical state
    // of this module
    mutable std::map<std::string, detail::DeviceModuleProxy> proxies;

    // create or return a proxy for a submodule (full hierarchy)
    detail::DeviceModuleProxy& getProxy(const std::string& fullName) const;

    /** A  VariableGroup for exception status and message. It can be protected, as
     * it is automatically connected to the control system in
     * DeviceModule::defineConnections() */
    struct DeviceError : public VariableGroup {
      using VariableGroup::VariableGroup;
      StatusOutput status{this, "status", "Device status"};
      ScalarOutput<std::string> message{this, "message", "", "Error message"};
    };
    DeviceError deviceError{this, "DeviceError", "Error status of the device"};

    /** The thread waiting for reportException(). It runs handleException() */
    boost::thread moduleThread;

    /** Queue used for communication between reportException() and the
     * moduleThread. */
    cppext::future_queue<std::string> errorQueue{5};

    /** Mutex to protect deviceHasError.
        Attention: In testable mode this mutex must only be locked when holding the testable mode mutex!*/
    boost::shared_mutex errorMutex;

    /** Version number of the last exception. Only access under the error mutex. */
    VersionNumber exceptionVersionNumber = {};
    //Intentionally not initialised with nullptr. It is propagated as long as the device is not successfully opened.

    /** The error flag whether the device is functional. protected by the errorMutex. */
    bool deviceHasError{true};

    /** Use this function to read the exception version number. It is locking the variable mutex correctly for you. */
    VersionNumber getExceptionVersionNumber();

    /** This functions tries to open the device and set the deviceError. Once done it notifies the waiting thread(s).
     *  The function is running an endless loop inside its own thread (moduleThread). */
    void handleException();

    /** List of TransferElements to be written after the device has been recovered.
     *  See function addRecoveryAccessor() for details.*/
    std::list<boost::shared_ptr<RecoveryHelper>> recoveryHelpers;

    Application* owner{nullptr};

    mutable bool deviceIsInitialized = false;

    /* The list of initialisation handler callback functions */
    std::list<std::function<void(DeviceModule*)>> initialisationHandlers;

    /** Mutex for writing the DeviceModule::writeRecoveryOpen.*/
    boost::shared_mutex recoveryMutex;

    /** Latch to halt accessors until initial values can be received.
     *  Must be a latch and not a mutex as it is locked in a different thread than unlocked. */
    bool isHoldingInitialValueLatch{true};
    boost::latch initialValueLatch{1};

    std::atomic<int64_t> synchronousTransferCounter{0};
    std::atomic<uint64_t> writeOrderCounter{0};

    std::list<RegisterPath> writeRegisterPaths;
    std::list<RegisterPath> readRegisterPaths;

    friend class Application;
    // Access to virtualiseFromCatalog() is needed by ServerHistory
    friend struct history::ServerHistory;
    // Access to virtualiseFromCatalog() is needed by MicroDAQ
    template<typename TRIGGERTYPE>
    friend class MicroDAQ;
    friend struct detail::DeviceModuleProxy;

    template<typename T>
    friend class ExceptionHandlingDecorator;

    friend class ConnectingDeviceModule;
  };

  /*********************************************************************************************************************/

  /**
   */
  class ConnectingDeviceModule : public ModuleGroup {
   public:
    /**
    *  Create ConnectingDeviceModule which is connected to the control system at the path of the owner.
    *
    *  deviceAliasOrURI: identifies the device by either the alias found in the DMAP file or directly a CDD.
    *
    *  triggerPath specifies a control system variable which is used as a trigger where needed.
    *
    *  initialisationHandler specifies a callback function to initialise the device (optional, default is none).
    *
    *  pathInDevice specifies a module in the device register hierarchy which should be used and connected to the
    *  control system (optional, default is "/" which connects the entire device).
    *
    *  Note about typical usage: A DeviceModule constructed with this constructer is often owned by the ModuleGroup
    *  which is using this device. The device should be a logical name mapped device so the variable hierarchy of the
    *  ModuleGroup and the Device can be matched. The logical device may be subdivided into several parts, e.g. if
    *  different parts of the device are used by independent ModuleGroups, or if different triggers are required. This
    *  is possible by use of the pathInDevice prefix. To avoid the creation of multiple DeviceBackends for the same
    *  device (which may not even be possible for some transport protocols) make sure that the device CDD is identical
    *  for all instances (the alias name does not matter, so multiple DMAP file entires pointing to the same device
    *  are possible if needed).
    *
    *  Keep in mind that mulitple DeviceModules will perform independent and asynchronous recovery procedures after
    *  an exception, even when pointing to the same device.
    */
    ConnectingDeviceModule(EntityOwner* owner, const std::string& deviceAliasOrCDD, const std::string& triggerPath = {},
        std::function<void(DeviceModule*)> initialisationHandler = nullptr, const std::string& pathInDevice = "/");

    /**
     *  Return the underlying DeviceModule
     */
    DeviceModule& getDeviceModule() { return *_dm; }

   protected:
    void defineConnections() override;

    std::string pathToConnectTo;
    std::string triggerPath;
    std::string pathInDevice;

    /// The DeviceModule represented by this ConnectingDeviceModule
    DeviceModule* _dm;

    /// Initialisation handler to add to the DeviceModule. This must be done only in defineConnections(), as otherwise
    /// the initialisation handler would need to be removed in the destructor which is not possible. Not doing so at
    /// least creates issues with move operations, especially if the initialisation handler points to a moved object.
    std::function<void(DeviceModule*)> _initHandler;

    /// Shared pointer holding the DeviceModule if (and only if) this ConnectingDeviceModule owns the DeviceModule
    boost::shared_ptr<DeviceModule> _dmHolder;
  };

} /* namespace ChimeraTK */

#endif /* CHIMERATK_DEVICE_MODULE_H */
