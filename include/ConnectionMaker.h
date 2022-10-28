// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "FanOut.h"
#include "Model.h"
#include "VariableNetworkNode.h"

#include <list>

namespace ChimeraTK {
  class Application;
  class TriggerFanOut;

  /**********************************************************************************************************************/

  class NetworkVisitor {
   public:
    explicit NetworkVisitor(Application& app) : _app(app) {}

    void setDebugConnections(bool enable) { _debugConnections = enable; }

    /**
     * @brief Helper predicate to put ProcessVariableProxies into std::set
     */
    struct ProcessVariableComperator {
      bool operator()(const Model::ProcessVariableProxy& a, const Model::ProcessVariableProxy& b) const {
        return a.getFullyQualifiedPath() < b.getFullyQualifiedPath();
      }
    };

   protected:
    struct NetworkInformation {
      explicit NetworkInformation(const Model::ProcessVariableProxy* p) : proxy(p) {}

      const Model::ProcessVariableProxy* proxy{nullptr};
      // Variables related to the current network
      VariableNetworkNode feeder{};
      std::map<std::string, boost::shared_ptr<TriggerFanOut>> triggerImpl;
      std::list<VariableNetworkNode> consumers;
      const std::type_info* valueType{&typeid(AnyType)};
      size_t valueLength{0};
      std::string description{};
      std::string unit{};
      size_t numberOfBidirectionalNodes{0};
      size_t numberOfPollingConsumers{0};
      bool useExternalTrigger{false};
    };
    std::set<std::string> _triggerNetworks{};
    std::map<std::string, NetworkInformation> _networks{};
    bool _debugConnections{false};

    NetworkInformation checkNetwork(Model::ProcessVariableProxy& proxy);
    void finaliseNetwork(NetworkInformation& net);
    NetworkInformation checkAndFinaliseNetwork(Model::ProcessVariableProxy& proxy);

    template<typename UserType>
    void createProcessVariable(const VariableNetworkNode& node, size_t length, const std::string& unit,
        const std::string& description, AccessModeFlags flags);

    template<typename... Args>
    void debug(Args&&...);

    // Map of control system PVs with decorator
    template<typename UserType>
    using AccessorMap = std::map<std::string, boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>;
    mutable ChimeraTK::TemplateUserTypeMap<AccessorMap> _decoratedPvImpls;

    Application& _app;
  };

  /**********************************************************************************************************************/

  class ConnectionMaker : public NetworkVisitor {
   public:
    explicit ConnectionMaker(Application& app) : NetworkVisitor(app) {}

    /**
     * Finalise the model and register all PVs with the control system adapter. The connections itself are not yet
     * realised, to allow optimising them with information from the control system adapter.
     *
     * Must be called exactly once before connect().
     */
    void finalise();

    /**
     * Execute the optimisation request from the control system adapter (remove unused variables)
     */
    void optimiseUnmappedVariables(const std::set<std::string>& names);

    /**
     * Realise connections.
     *
     * Must be called exactly once after finalise().
     */
    void connect();

   private:

    std::set<Model::ProcessVariableProxy, ProcessVariableComperator> triggers;

    void connectNetwork(Model::ProcessVariableProxy& proxy);

    void makeDirectConnectionForFeederWithImplementation(NetworkInformation& net);
    void makeFanOutConnectionForFeederWithImplementation(NetworkInformation& net,
        const Model::DeviceModuleProxy& device, const Model::ProcessVariableProxy& trigger);
    void makeConnectionForFeederWithoutImplementation(NetworkInformation& net);
    void makeConnectionForConstantFeeder(NetworkInformation& net);

    template<typename UserType>
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> getProcessVariable(const VariableNetworkNode& node);

    template<typename UserType>
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> createDeviceVariable(VariableNetworkNode const& node);

    template<typename UserType>
    ConsumerImplementationPairs<UserType> setConsumerImplementations(NetworkInformation& net);

    template<typename UserType>
    std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>
        createApplicationVariable(VariableNetworkNode const& node, VariableNetworkNode const& consumer = {});
  };

  /**********************************************************************************************************************/

} // namespace ChimeraTK
