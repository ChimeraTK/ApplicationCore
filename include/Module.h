// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "EntityOwner.h"

#include <ChimeraTK/ReadAnyGroup.h>
#include <ChimeraTK/TransferElement.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  class ApplicationModule;
  struct ConfigReader;

  /********************************************************************************************************************/

  /** Base class for ApplicationModule and DeviceModule, to
   * have a common interface for these module types. */
  class Module : public EntityOwner {
   public:
    /** Constructor: Create Module by the given name with the given description and register it with its owner. The
     * specified list of tags will be added to all elements directly or indirectly owned by this instance. */
    Module(EntityOwner* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    /** Default constructor: Allows late initialisation of modules (e.g. when
     * creating arrays of modules).
     *
     *  This construtor also has to be here to mitigate a bug in gcc. It is needed
     * to allow constructor inheritance of modules owning other modules. This
     * constructor will not actually be called then. See this bug report:
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67054 */
    Module() : _owner{nullptr} {}

    /** Destructor */
    ~Module() override;

    /** Move constructor */
    Module(Module&& other) noexcept { operator=(std::move(other)); }

    /** Move assignment operator */
    Module& operator=(Module&& other) noexcept;

    /** Prepare the execution of the module. This function is called before any module is started (including internal
     *  modules like FanOuts) and before the initial values of the variables are pushed into the queues. Reading and
     *  writing variables at this point may result in undefined behaviour. */
    virtual void prepare() {}

    /** Execute the module. */
    virtual void run();

    /** Terminate the module. Must/will be called before destruction, if run() was
     * called previously. */
    virtual void terminate() {}

    /** Create a ChimeraTK::ReadAnyGroup for all readable variables in this
     * Module. */
    ChimeraTK::ReadAnyGroup readAnyGroup();

    /** Read all readable variables in the group. If there are push-type variables in the group, this call will block
     *  until all of the variables have received an update. All push-type variables are read first, the poll-type
     *  variables are therefore updated with the latest values upon return.
     *  includeReturnChannels determines whether return channels of *OutputRB accessors are included in the read. */
    void readAll(bool includeReturnChannels = false);

    /** Just call readNonBlocking() on all readable variables in the group.
     *  includeReturnChannels determines whether return channels of *OutputRB accessors are included in the read. */
    void readAllNonBlocking(bool includeReturnChannels = false);

    /** Just call readLatest() on all readable variables in the group.
     *  includeReturnChannels determines whether return channels of *OutputRB accessors are included in the read. */
    void readAllLatest(bool includeReturnChannels = false);

    /** Just call write() on all writable variables in the group.
     *  includeReturnChannels determines whether return channels of *InputWB accessors are included in the write. */
    void writeAll(bool includeReturnChannels = false);

    /** Just call writeDestructively() on all writable variables in the group.
     *  includeReturnChannels determines whether return channels of *InputWB accessors are included in the write. */
    void writeAllDestructively(bool includeReturnChannels = false);

    std::string getQualifiedName() const override;

    virtual std::string getVirtualQualifiedName() const = 0;

    std::string getFullDescription() const override;

    /**
     * Set a new owner. The caller has to take care himself that the Module gets unregistered with the old owner
     * and registered with the new one. Do not use in user code!
     */
    void setOwner(EntityOwner* newOwner) { _owner = newOwner; }

    EntityOwner* getOwner() const { return _owner; }

    VersionNumber getCurrentVersionNumber() const override { return _owner->getCurrentVersionNumber(); }

    void setCurrentVersionNumber(VersionNumber version) override { _owner->setCurrentVersionNumber(version); }

    DataValidity getDataValidity() const override { return _owner->getDataValidity(); }

    void incrementDataFaultCounter() override { _owner->incrementDataFaultCounter(); }

    void decrementDataFaultCounter() override { _owner->decrementDataFaultCounter(); }

    std::list<EntityOwner*> getInputModulesRecursively(std::list<EntityOwner*> startList) override;

    size_t getCircularNetworkHash() override { return _owner->getCircularNetworkHash(); }

    /**
     *  Find ApplicationModule owner. If "this" is an ApplicationModule, "this" is returned. If "this" is a
     *  VariableGroup, the tree of owners is followed, until the ApplicationModule is found. If "this" is neither an
     *  ApplicationModule nor a VariableGroup, a ChimeraTK::logic_error is thrown.
     *
     *  Note: This function treats ApplicationModules and DeviceModules the same (hence the return type must be the
     *  common base class).
     */
    Module* findApplicationModule();

    /** Obtain the ConfigReader instance of the application. If no or multiple ConfigReader instances are found, a
     *  ChimeraTK::logic_error is thrown.
     *  Note: This function is expensive. It should be called only during the constructor of the ApplicationModule and
     *  the obtained configuration values should be stored for later use in member variables.
     *  Beware that the ConfigReader instance can obly be found if it has been constructed before calling this function.
     *  Hence, the Application should have the ConfigReader as its first member. */
    static ConfigReader& appConfig();

   protected:
    /** Owner of this instance */
    EntityOwner* _owner{nullptr};
  };

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
