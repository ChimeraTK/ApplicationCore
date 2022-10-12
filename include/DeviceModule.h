// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ModuleGroup.h"

#include <ChimeraTK/Device.h>
#include <ChimeraTK/ForwardDeclarations.h>
#include <ChimeraTK/RegisterPath.h>

#include <boost/thread/latch.hpp>

namespace ChimeraTK {
  class DeviceManager;

  /*********************************************************************************************************************/

  /**
   */
  class DeviceModule : public ModuleGroup {
   public:
    /**
     * Create ConnectingDeviceModule which is connected to the control system at the path of the owner.
     *
     * deviceAliasOrURI: identifies the device by either the alias found in the DMAP file or directly a CDD.
     *
     * triggerPath specifies a control system variable which is used as a trigger where needed.
     *
     * initialisationHandler specifies a callback function to initialise the device (optional, default is none).
     *
     * pathInDevice specifies a module in the device register hierarchy which should be used and connected to the
     * control system (optional, default is "/" which connects the entire device).
     *
     * Note about typical usage: A DeviceModule constructed with this constructor is often owned by the ModuleGroup
     * which is using this device. The device should be a logical name mapped device so the variable hierarchy of the
     * ModuleGroup and the Device can be matched. The logical device may be subdivided into several parts, e.g. if
     * different parts of the device are used by independent ModuleGroups, or if different triggers are required. This
     * is possible by use of the pathInDevice prefix.
     *
     * To avoid the creation of multiple DeviceBackends for the same device (which may not even be possible for some
     * transport protocols) make sure that the device CDD is identical for all instances (the alias name does not
     * matter, so multiple DMAP file entries pointing to the same device are possible if needed).
     */
    DeviceModule(ModuleGroup* owner, const std::string& deviceAliasOrCDD, const std::string& triggerPath = {},
        std::function<void(ChimeraTK::Device&)> initialisationHandler = nullptr, const std::string& pathInDevice = "/");

    using ModuleGroup::ModuleGroup;

    /** Move operation with the move constructor */
    DeviceModule(DeviceModule&& other) noexcept { operator=(std::move(other)); }

    /** Move assignment */
    DeviceModule& operator=(DeviceModule&& other) noexcept;

    /**
     *  Return the corresponding DeviceManager
     */
    DeviceManager& getDeviceManager();
    const DeviceManager& getDeviceManager() const;

    Model::DeviceModuleProxy getModel();

    const std::string& getDeviceAliasOrURI() const;

    void addInitialisationHandler(std::function<void(ChimeraTK::Device&)> initialisationHandler);

    /**
     * Use this function to report an exception. It should be called whenever a ChimeraTK::runtime_error has been caught
     * when trying to interact with this device. It is primarily used by the ExceptionHandlingDecorator, but also user
     * modules can report exception and trigger the recovery mechanism like this.
     */
    void reportException(std::string errMsg);

    const std::string& getTriggerPath() const { return _triggerPath; }

   protected:
    /// The corresponding DeviceManager.
    boost::weak_ptr<DeviceManager> _dm;

    Model::DeviceModuleProxy _model;

    std::string _triggerPath;

    std::string _pathInDevice;
  };

  /*********************************************************************************************************************/

  /**
   * Helper class to set the DMAP file path. This shall be used as a first member in an Application to ensure the DMAP
   * file path is set before any DeviceModule is created.
   */
  class SetDMapFilePath {
   public:
    explicit SetDMapFilePath(const std::string& dmapFilePath);
  };

  /*********************************************************************************************************************/

  /**
   * Deprecated type alias for compatibility.
   */
  class ConnectingDeviceModule : public DeviceModule {
   public:
    [[deprecated]] ConnectingDeviceModule(ModuleGroup* owner, const std::string& deviceAliasOrCDD,
        const std::string& triggerPath = {}, std::function<void(ChimeraTK::Device&)> initialisationHandler = nullptr,
        const std::string& pathInDevice = "/")
    : DeviceModule(owner, deviceAliasOrCDD, triggerPath, std::move(initialisationHandler), pathInDevice) {}

    using DeviceModule::DeviceModule;
  };

  /*********************************************************************************************************************/

} /* namespace ChimeraTK */
