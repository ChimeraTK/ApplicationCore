// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "CircularDependencyDetector.h"
#include "ConnectionMaker.h"
#include "Flags.h"
#include "InternalModule.h"
#include "Model.h"
#include "ModuleGroup.h"
#include "TestableMode.h"
#include "VariableNetwork.h"

#include <ChimeraTK/ControlSystemAdapter/ApplicationBase.h>
#include <ChimeraTK/DeviceBackend.h>

#include <atomic>
#include <mutex>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  class Module;
  class AccessorBase;
  class VariableNetwork;
  class TriggerFanOut;
  class TestFacility;
  class DeviceManager;
  class ApplicationModule;

  template<typename UserType>
  class Accessor;
  template<typename UserType>
  class FanOut;
  template<typename UserType>
  class ConsumingFanOut;

  namespace detail {
    struct TestableMode;
  } // namespace detail

  /*********************************************************************************************************************/

  class Application : public ApplicationBase, public ModuleGroup {
   public:
    /** The constructor takes the application name as an argument. The name must
     * have a non-zero length and must not contain any spaces or special
     * characters. Use only alphanumeric characters and underscores. */
    explicit Application(const std::string& name);

    using ApplicationBase::getName;

    /** This will remove the global pointer to the instance and allows creating
     * another instance afterwards. This is mostly useful for writing tests, as it
     * allows to run several applications sequentially in the same executable.
     * Note that any ApplicationModules etc. owned by this Application are no
     * longer
     *  valid after destroying the Application and must be destroyed as well (or
     * at least no longer used). */
    void shutdown() override;

    void initialise() override;

    void optimiseUnmappedVariables(const std::set<std::string>& names) override;

    void run() override;

    /** Return the root of the application model */
    Model::RootProxy getModel() { return _model; }

    /** Instead of running the application, just initialise it and output the
     * published variables to an XML file. */
    void generateXML();

    /** Output the connections requested in the initialise() function to
     * std::cout. This may be done also before
     *  makeConnections() has been called. */
    void dumpConnections(std::ostream& stream = std::cout);

    /** Create Graphviz dot graph and write to file. The graph will contain the
     * connections made in the initialise() function. @see dumpConnections */
    // void dumpConnectionGraph(const std::string& filename = {"connections-graph.dot"}) const;

    /** Create Graphviz dot graph representing the connections between the modules, and write to file.*/
    // void dumpModuleConnectionGraph(const std::string& filename = {"module-connections-graph.dot"}) const;

    /** Enable warning about unconnected variables. This can be helpful to
     * identify missing connections but is
     *  disabled by default since it may often be very noisy. */
    void warnUnconnectedVariables() { enableUnconnectedVariablesWarning = true; }

    /** Obtain instance of the application. Will throw an exception if called
     * before the instance has been created by the control system adapter, or if
     * the instance is not based on the Application class. */
    static Application& getInstance();

    /** Enable the testable mode. This allows to step-wise run the application using testableMode.step()
     *  The application will start in paused state.
     *
     *  This function must be called before the application is initialised (i.e.
     * before the call to initialise()).
     *
     *  Note: Enabling the testable mode will have a significant impact on the
     * performance, since it will prevent any module threads to run at the same
     * time! */
    void enableTestableMode();

    /** Get the TestableMode of this application */
    detail::TestableMode& getTestableMode() { return testableMode; }

    /** Register the thread in the application system and give it a name. This
     * should be done for all threads used by the application to help with
     * debugging and to allow profiling. */
    static void registerThread(const std::string& name);

    void debugMakeConnections() { enableDebugMakeConnections = true; }

    ModuleType getModuleType() const override { return ModuleType::ModuleGroup; }

    std::string getQualifiedName() const override { return "/" + _name; }

    std::string getFullDescription() const override { return ""; }

    /** Enable debug output for a given variable. */
    void enableVariableDebugging(const VariableNetworkNode& node) { debugMode_variableList.insert(node.getUniqueId()); }

    /** Enable debug output for lost data. This will print to stdout every time data is lost in internal queues as it
     *  is counted with the DataLossCounter module. Do not enable in production environments. Do not call after
     *  initialisation phase of application. */
    void enableDebugDataLoss() { debugDataLoss = true; }

    /** Increment counter for how many write() operations have overwritten unread data */
    static void incrementDataLossCounter(const std::string& name);

    static size_t getAndResetDataLossCounter();

    boost::shared_ptr<DeviceManager> getDeviceManager(const std::string& aliasOrCDD);

    LifeCycleState getLifeCycleState() const { return lifeCycleState; }

    VersionNumber getStartVersion() const { return _startVersion; }

   protected:
    friend class Module;
    friend class VariableNetwork;
    friend class VariableNetworkNode;
    friend class VariableNetworkGraphDumpingVisitor;
    friend class VariableNetworkModuleGraphDumpingVisitor;
    friend class XMLGeneratorVisitor;
    friend struct StatusAggregator;
    friend struct detail::TestableMode;
    friend class ConnectionMaker;

    template<typename UserType>
    friend class Accessor;

    template<typename UserType>
    friend class ExceptionHandlingDecorator;

    /** Scan for circular dependencies and mark all affected consuming nodes.
     *  This can only be done after all connections have been established. */
    // void markCircularConsumers(VariableNetwork& variableNetwork);

    /** The model of the application */
    Model::RootProxy _model;

    /** List of InternalModules */
    std::list<boost::shared_ptr<InternalModule>> internalModuleList;

    /** Map of trigger consumers to their corresponding TriggerFanOuts. Note: the
     * key is the ID (address) of the externalTiggerImpl. */
    std::map<const void*, boost::shared_ptr<TriggerFanOut>> triggerMap;

    /** Map of DeviceManagers. The alias name resp. CDD is the key.*/
    std::map<std::string, boost::shared_ptr<DeviceManager>> _deviceManagerMap;

    /** Flag which is set by the TestFacility in runApplication() at the beginning. This is used to make sure
     *  runApplication() is called by the TestFacility and not manually. */
    bool testFacilityRunApplicationCalled{false};

    /** Flag whether initialise() has been called already, to make sure it doesn't get called twice. */
    bool initialiseCalled{false};

    /** Flag whether run() has been called already, to make sure it doesn't get called twice. */
    bool runCalled{false};

    /** Flag whether to warn about unconnected variables or not */
    bool enableUnconnectedVariablesWarning{false};

    /** Flag if debug output is enabled for creation of the variable connections */
    bool enableDebugMakeConnections{false};

    /** Map from ProcessArray uniqueId to the variable ID for control system
     * variables. This is required for the TestFacility. */
    std::map<size_t, size_t> pvIdMap;

    detail::CircularDependencyDetector circularDependencyDetector;
    detail::TestableMode testableMode;

    /** List of variables for which debug output was requested via
     * enableVariableDebugging(). Stored is the unique id of the
     * VariableNetworkNode.*/
    std::unordered_set<const void*> debugMode_variableList;

    /** Counter for how many write() operations have overwritten unread data */
    std::atomic<size_t> dataLossCounter{0};

    /** Flag whether to debug data loss (as counted with the data loss counter). */
    bool debugDataLoss{false};

    /** Life-cycle state of the application */
    std::atomic<LifeCycleState> lifeCycleState{LifeCycleState::initialisation};

    /** Version number used at application start, e.g. to propagate initial values */
    VersionNumber _startVersion;

    /** Map of atomic invalidity counters for each circular dependency network.
     *  The key is the hash of network which serves as a unique identifier.
     *   The invalidity counter is atomic so it can be accessed from all modules in the network.
     */
    std::map<size_t, std::atomic<uint64_t>> circularNetworkInvalidityCounters;

    /** The networks of circular dependencies, reachable by their hash, which serves as unique ID
     */
    std::map<size_t, std::list<EntityOwner*>> circularDependencyNetworks;

    friend class TestFacility; // needs access to testableMode variables

    template<typename UserType>
    friend class DebugPrintAccessorDecorator; // needs access to the idMap
    template<typename UserType>
    friend class MetaDataPropagatingRegisterDecorator; // needs to access circularNetworkInvalidityCounters
    friend class ApplicationModule;                    // needs to access circularNetworkInvalidityCounters
    friend struct detail::CircularDependencyDetector;

    VersionNumber getCurrentVersionNumber() const override {
      throw ChimeraTK::logic_error("getCurrentVersionNumber() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    void setCurrentVersionNumber(VersionNumber) override {
      throw ChimeraTK::logic_error("setCurrentVersionNumber() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    DataValidity getDataValidity() const override {
      throw ChimeraTK::logic_error("getDataValidity() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    void incrementDataFaultCounter() override {
      throw ChimeraTK::logic_error("incrementDataFaultCounter() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    void decrementDataFaultCounter() override {
      throw ChimeraTK::logic_error("decrementDataFaultCounter() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    std::list<EntityOwner*> getInputModulesRecursively([[maybe_unused]] std::list<EntityOwner*> startList) override {
      throw ChimeraTK::logic_error("getInputModulesRecursively() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    size_t getCircularNetworkHash() override {
      throw ChimeraTK::logic_error("getCircularNetworkHash() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
  };

} /* namespace ChimeraTK */
