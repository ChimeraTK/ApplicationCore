// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "CircularDependencyDetectionRecursionStopper.h"
#include "Logger.h"
#include "Model.h"
#include "VariableGroup.h"

#include <boost/thread.hpp>

#include <list>

namespace ChimeraTK {

  /********************************************************************************************************************/

  class Application;
  class ModuleGroup;
  struct ConfigReader;

  /********************************************************************************************************************/

  class ApplicationModule : public VariableGroup {
   public:
    /**
     * Create ApplicationModule and register it with its owner
     *
     * The specified list of tags will be added to all elements directly or indirectly owned by this instance.
     *
     * @param owner The owner to register the ApplicationMoule with (ModuleGroup or Application)
     * @param name The name of the new ApplicationModule
     * @param description A description visible to the control system
     * @param tags List of tags to be added to all child variables (default: empty)
     *
     * @exception ChimeraTK::logic_error thrown if owner is of the wrong type or name is illegal.
     */
    ApplicationModule(ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    /** Default constructor: Allows late initialisation of modules (e.g. when creating arrays of modules). */
    ApplicationModule() = default;

    /** Move operation with the move constructor */
    ApplicationModule(ApplicationModule&& other) noexcept { operator=(std::move(other)); }

    /** Move assignment */
    ApplicationModule& operator=(ApplicationModule&& other) noexcept;

    /** Destructor */
    ~ApplicationModule() override;

    /** To be implemented by the user: function called in a separate thread
     * executing the main loop of the module */
    virtual void mainLoop() = 0;

    void run() override;

    void terminate() override;

    ModuleType getModuleType() const override { return ModuleType::ApplicationModule; }

    VersionNumber getCurrentVersionNumber() const override { return _currentVersionNumber; }

    DataValidity getDataValidity() const override;

    void incrementDataFaultCounter() override;
    void decrementDataFaultCounter() override;

    /** Get the Number of inputs which report DataValidity::faulty. */
    size_t getDataFaultCounter() const { return _dataFaultCounter; }

    void setCurrentVersionNumber(VersionNumber versionNumber) override;

    std::list<EntityOwner*> getInputModulesRecursively(std::list<EntityOwner*> startList) override;

    size_t getCircularNetworkHash() const override;

    /** Set the ID of the circular dependency network. This function can be called multiple times and throws if the
     *  value is not identical.
     */
    void setCircularNetworkHash(size_t circularNetworkHash);

    /** Return the application model proxy representing this module */
    ChimeraTK::Model::ApplicationModuleProxy getModel() { return _model; }

    void unregisterModule(Module* module) override;

    /**
     * Convenicene function to obtain a logger stream with the given Severity. The context string will be obtained from
     * the class name of the module.
     */
    Logger::StreamProxy logger(Logger::Severity severity);

   protected:
    /** Wrapper around mainLoop(), to execute additional tasks in the thread
     * before entering the main loop */
    void mainLoopWrapper();

    /** The thread executing mainLoop() */
    boost::thread _moduleThread;

    /** Version number of last push-type read operation - will be passed on to any
     * write operations */
    VersionNumber _currentVersionNumber{nullptr};

    /**
     *  Number of inputs which report DataValidity::faulty.
     *  This is atomic to allow the InvalidityTracer module to access this information.
     */
    std::atomic<size_t> _dataFaultCounter{0};

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

    /**
     * Name of the module class, used for logging and debugging purposes
     */
    std::string className() {
      auto name = boost::core::demangle(typeid(*this).name());
      return name.substr(name.find_last_of(':') + 1);
    }

    ChimeraTK::Model::ApplicationModuleProxy _model;
    friend class ChimeraTK::Model::Impl;
  };

  /********************************************************************************************************************/

  inline Logger::StreamProxy ApplicationModule::logger(Logger::Severity severity) {
    return ChimeraTK::logger(severity, className());
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
