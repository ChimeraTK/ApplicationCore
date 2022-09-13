// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "CircularDependencyDetectionRecursionStopper.h"
#include "VariableGroup.h"

#include <boost/thread.hpp>

#include <list>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  class Application;
  class ModuleGroup;
  struct ConfigReader;

  /*********************************************************************************************************************/

  class ApplicationModule : public VariableGroup {
   public:
    /**
     * Create ApplicationModule and register it with its owner
     *
     * The hierarchy will be modified according to the hierarchyModifier (when VirtualModules are created e.g.
     * in findTag()). The specified list of tags will be added to all elements directly or indirectly owned by this
     * instance.
     *
     * @param owner The owner to register the ApplicationMoule with (ModuleGroup or Application)
     * @param name The name of the new ApplicationModule
     * @param description A description visible to the control system
     * @param hierarchyModifier Specifies how the hierarchy should be modified
     * @param tags List of tags to be added to all child variables (default: empty)
     *
     * @exception ChimeraTK::logic_error thrown if owner is of the wrong type or name is illegal.
     */
    ApplicationModule(EntityOwner* owner, const std::string& name, const std::string& description,
        HierarchyModifier hierarchyModifier = HierarchyModifier::none,
        const std::unordered_set<std::string>& tags = {});

    /** Deprecated form of the constructor. Use the new signature instead. */
    ApplicationModule(EntityOwner* owner, const std::string& name, const std::string& description,
        bool eliminateHierarchy, const std::unordered_set<std::string>& tags = {});

    /** Default constructor: Allows late initialisation of modules (e.g. when
     * creating arrays of modules).
     *
     *  This constructor also has to be here to mitigate a bug in gcc. It is needed
     * to allow constructor inheritance of modules owning other modules. This
     * constructor will not actually be called then. See this bug report:
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67054 */
    ApplicationModule() {}

    /** Move operation with the move constructor */
    ApplicationModule(ApplicationModule&& other) { operator=(std::move(other)); }

    /** Move assignment */
    ApplicationModule& operator=(ApplicationModule&& other);

    /** Destructor */
    virtual ~ApplicationModule();

    /** To be implemented by the user: function called in a separate thread
     * executing the main loop of the module */
    virtual void mainLoop() = 0;

    void run() override;

    void terminate() override;

    ModuleType getModuleType() const override { return ModuleType::ApplicationModule; }

    VersionNumber getCurrentVersionNumber() const override { return currentVersionNumber; }

    DataValidity getDataValidity() const override;

    void incrementDataFaultCounter() override;
    void decrementDataFaultCounter() override;

    void setCurrentVersionNumber(VersionNumber versionNumber) override;

    std::list<EntityOwner*> getInputModulesRecursively(std::list<EntityOwner*> startList) override;

    size_t getCircularNetworkHash() override;

    /** Set the ID of the circular dependency network. This function can be called multiple times and throws if the
     *  value is not identical.
     */
    void setCircularNetworkHash(size_t circularNetworkHash);

   protected:
    /** Wrapper around mainLoop(), to execute additional tasks in the thread
     * before entering the main loop */
    void mainLoopWrapper();

    /** The thread executing mainLoop() */
    boost::thread moduleThread;

    /** Version number of last push-type read operation - will be passed on to any
     * write operations */
    VersionNumber currentVersionNumber{nullptr};

    /**
     *  Number of inputs which report DataValidity::faulty.
     *  This is atomic to allow the InvalidityTracer module to access this information.
     */
    std::atomic<size_t> dataFaultCounter{0};

    /**
     *  Unique ID for the circular dependency network. 0 if the EntityOwner is not in a circular dependency network.
     *  Only write when in LifeCycleState::initialisation (so getDataValidity() is thread safe, required by
     *  InvalidityTracer).
     */
    size_t _circularNetworkHash{0};

    /** Helper needed to stop the recursion when detecting circular dependency networks.
     *  Only used in the setup phase.
     */
    detail::CircularDependencyDetectionRecursionStopper _recursionStopper;
  };

  /*********************************************************************************************************************/

} /* namespace ChimeraTK */
