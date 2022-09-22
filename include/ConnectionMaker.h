// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ChimeraTK/ControlSystemAdapter/ProcessVariable.h"
#include "FanOut.h"
#include "Model.h"
#include "VariableNetworkNode.h"

#include <list>

namespace ChimeraTK {
  class Application;

  class ConnectionMaker {
   public:
    struct NetworkInformation {
      explicit NetworkInformation(const Model::ProcessVariableProxy* p) : proxy(p) {}

      const Model::ProcessVariableProxy* proxy{nullptr};
      // Variables related to the current network
      VariableNetworkNode feeder{};
      boost::shared_ptr<ChimeraTK::ProcessVariable> feederImpl;
      std::list<VariableNetworkNode> consumers;
      const std::type_info* valueType{&typeid(AnyType)};
      size_t valueLength{0};
      std::string description{};
      std::string unit{};
      size_t numberOfBidirectionalNodes{0};
      size_t numberOfPollingConsumers{0};
      bool useExternalTrigger{false};
    };

    explicit ConnectionMaker(Application& app) : _app(app) {}

    void connect();
    void setDebugConnections(bool enable) { _debugConnections = enable; }

   private:
    Application& _app;
    bool _debugConnections{false};
    std::map<std::string, NetworkInformation> _triggerNetworks{};

    template<typename... Args>
    void debug(Args&&...);

    NetworkInformation connectNetwork(Model::ProcessVariableProxy& proxy);
    NetworkInformation checkNetwork(Model::ProcessVariableProxy& proxy);

    void makeDirectConnectionForFeederWithImplementation(const NetworkInformation& net);
    void makeFanOutConnectionForFeederWithImplementation(const NetworkInformation& net,
        const Model::DeviceModuleProxy& device, const Model::ProcessVariableProxy& trigger);
    void makeConnectionForFeederWithoutImplementation(const NetworkInformation& net);
    void makeConnectionForConstantFeeder(const NetworkInformation& net);

    template<typename UserType>
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> createProcessVariable(const VariableNetworkNode& node,
        size_t length, const std::string& unit, const std::string& description, AccessModeFlags flags);

    template<typename UserType>
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> createDeviceVariable(VariableNetworkNode const& node);

    template<typename UserType>
    ConsumerImplementationPairs<UserType> setConsumerImplementations(const NetworkInformation& net);

    template<typename UserType>
    std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>
        createApplicationVariable(VariableNetworkNode const& node, VariableNetworkNode const& consumer = {});
  };
} // namespace ChimeraTK
