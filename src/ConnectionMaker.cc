// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ConnectionMaker.h"

#include "Application.h"
#include "ConsumingFanOut.h"
#include "DebugPrintAccessorDecorator.h"
#include "DeviceManager.h"
#include "ExceptionHandlingDecorator.h"
#include "FanOut.h"
#include "TestableMode.h"
#include "ThreadedFanOut.h"
#include "TriggerFanOut.h"

#include <ChimeraTK/NDRegisterAccessor.h>

#include <memory>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  NetworkVisitor::NetworkInformation NetworkVisitor::checkNetwork(Model::ProcessVariableProxy& proxy) {
    NetworkInformation net{&proxy};
    // Sanity check for the type and lengths of the nodes, extract the feeding node if any

    for(const auto& node : proxy.getNodes()) {
      if(node.getDirection().withReturn) {
        net.numberOfBidirectionalNodes++;
      }
      if(node.getDirection().dir == VariableDirection::feeding) {
        std::stringstream ss;
        node.dump(ss);
        debug("    Feeder:   ", ss.str());

        if(net.feeder.getType() == NodeType::invalid) {
          net.feeder = node;
        }
        else {
          throw ChimeraTK::logic_error(
              "Variable network " + proxy.getFullyQualifiedPath() + " has more than one feeder");
        }
      }
      else if(node.getDirection().dir == VariableDirection::consuming) {
        std::stringstream ss;
        node.dump(ss);
        debug("    Consumer: ", ss.str());
        net.consumers.push_back(node);
        if(node.getMode() == UpdateMode::poll) {
          net.numberOfPollingConsumers++;
        }
      }
      else {
        // There should not be an invalid direction variable in here. FIXME: is that true?
        assert(false);
      }

      if(*net.valueType == typeid(AnyType)) {
        net.valueType = &node.getValueType();
      }
      else {
        if(*net.valueType != node.getValueType() && node.getValueType() != typeid(AnyType)) {
          throw ChimeraTK::logic_error(
              "Variable network " + proxy.getFullyQualifiedPath() + " contains nodes with different types");
        }
      }

      if(net.valueLength == 0) {
        net.valueLength = node.getNumberOfElements();
      }
      else {
        if(net.valueLength != node.getNumberOfElements() && node.getNumberOfElements() != 0) {
          throw ChimeraTK::logic_error(
              "Variable network " + proxy.getFullyQualifiedPath() + " contains nodes with different sizes");
        }
      }

      // Get unit and description of network from nodes. First one wins
      if(net.description.empty()) {
        net.description = node.getDescription();
      }

      if(net.unit.empty()) {
        net.unit = node.getUnit();
      }
    }

    // If we are left with an undefined network at this point this should be trigger network and can be assumed
    // to be void
    if(*net.valueType == typeid(AnyType)) {
      net.valueType = &typeid(ChimeraTK::Void);
    }

    // For void, a length of 0 is ok, otherwise this is not allowed
    if(net.valueLength == 0 && *net.valueType != typeid(ChimeraTK::Void)) {
      throw ChimeraTK::logic_error("Cannot determine length of network " + proxy.getFullyQualifiedPath());
    }

    if(net.feeder.getType() == NodeType::invalid && net.consumers.empty()) {
      throw ChimeraTK::logic_error("Variable network '" + proxy.getFullyQualifiedPath() + "' is empty. Must not happen");
    }

    return net;
  }

  /*********************************************************************************************************************/

  void NetworkVisitor::finaliseNetwork(NetworkInformation& net) {
    bool neededFeeder{false};
    if(not net.feeder.isValid()) {
      debug("  No feeder in network, creating ControlSystem feeder ", net.proxy->getFullyQualifiedPath());
      debug("    Bi-directional consumers: ", net.numberOfBidirectionalNodes);

      // If we have exactly one bi-directional consumer, mark this CS feeder as bidirectional as well
      net.feeder = VariableNetworkNode(net.proxy->getFullyQualifiedPath(),
          VariableDirection{VariableDirection::feeding, net.numberOfBidirectionalNodes == 1}, *net.valueType,
          net.valueLength);

      neededFeeder = true;
    }
    assert(net.feeder.isValid());

    if(not neededFeeder) {
      // Only add CS consumer if we did not previously add CS feeder, we will add one or the other, but never both
      debug("  Network has a non-CS feeder, can create additional ControlSystem consumer");
      net.consumers.push_back(VariableNetworkNode(
          net.proxy->getFullyQualifiedPath(), {VariableDirection::consuming, false}, *net.valueType, net.valueLength));
    }
    assert(not net.consumers.empty());
  }

  /*********************************************************************************************************************/

  template<typename... Args>
  void NetworkVisitor::debug(Args&&... args) {
    if(not _debugConnections) return;

    // FIXME: Use the proper logging mechanism once in place
    // https://redmine.msktools.desy.de/issues/8305

    // Fold expression printer from https://en.cppreference.com/w/cpp/language/fold
    (std::cout << ... << args) << std::endl;
  }

  /*********************************************************************************************************************/
  /* ConnectionMaker implementations */
  /*********************************************************************************************************************/

  NetworkVisitor::NetworkInformation ConnectionMaker::connectNetwork(Model::ProcessVariableProxy& proxy) {
    debug("Network found: ", proxy.getFullyQualifiedPath());
    // This will do two things:
    //  - Check the network consistency
    //  - Return feeder and consumers, if available
    auto net = checkNetwork(proxy);
    finaliseNetwork(net);

    auto triggerFinder = [&](auto p) {
      auto deviceTrigger = p.getTrigger();

      if(deviceTrigger.isValid()) {
        debug("    Found Feeding device ", p.getAliasOrCdd(), " with trigger ", p.getTrigger().getFullyQualifiedPath());
      }
      else {
        debug("    Feeding from device ", p.getAliasOrCdd(), " but without any trigger");
      }

      return std::make_pair(deviceTrigger, p);
    };

    Model::ProcessVariableProxy trigger{};
    Model::DeviceModuleProxy device{};

    // Use external trigger if feeder is poll-type and number of poll-type consumers != 1.
    // If there is exactly one poll-type consumer, transfers will be triggered by that consumer.
    if(net.feeder.getMode() == UpdateMode::poll && net.numberOfPollingConsumers != 1) {
      net.useExternalTrigger = true;
      std::tie(trigger, device) =
          proxy.visit(triggerFinder, Model::adjacentInSearch, Model::keepPvAccess, Model::keepDeviceModules,
              Model::returnFirstHit(std::make_pair(Model::ProcessVariableProxy{}, Model::DeviceModuleProxy{})));
      if(!trigger.isValid()) {
        throw ChimeraTK::logic_error("Poll-Type feeder " + net.feeder.getName() + " needs trigger, but none provided");
      }
    }

    auto constantFeeder = net.feeder.getType() == NodeType::Constant;

    if(net.feeder.hasImplementation()) {
      debug("  Creating fixed implementation for feeder '", net.feeder.getName(), "'...");

      if(net.consumers.size() == 1 && !net.useExternalTrigger) {
        debug("    One consumer without external trigger, creating direct connection");
        makeDirectConnectionForFeederWithImplementation(net);
      }
      else {
        // More than one consuming node
        debug("    More than one consuming node or having external trigger, setting up FanOut");
        makeFanOutConnectionForFeederWithImplementation(net, device, trigger);
      }
    }
    else if(not constantFeeder) {
      debug("    Feeder '", net.feeder.getName(), "' does not require a fixed implementation.");
      assert(not trigger.isValid());
      makeConnectionForFeederWithoutImplementation(net);
    }
    else { // constant feeder
      debug("    Using constant feeder '", net.feeder.getName(), "'.");
      makeConnectionForConstantFeeder(net);
    }

    return net;
  }

  /*********************************************************************************************************************/

  void ConnectionMaker::connect() {
    debug("Calling Connect...");

    _app.getTestableMode()._debugDecorating = _debugConnections;

    debug("  Preparing trigger networks");
    debug("    Collecting triggers");
    std::set<Model::ProcessVariableProxy, ProcessVariableComperator> triggers;

    // Collect all triggers, add a TriggerReceiver placeholder for every device associated with that trigger
    auto triggerCollector = [&](auto proxy) {
      auto trigger = proxy.getTrigger();
      if(not trigger.isValid()) return;

      triggers.insert(trigger);
      proxy.addVariable(trigger, VariableNetworkNode(proxy.getAliasOrCdd(), 0));
    };
    _app.getModel().visit(triggerCollector, Model::depthFirstSearch, Model::keepDeviceModules);

    // Finalize the trigger networks
    debug("    Connecting trigger networks");
    for(auto trigger : triggers) {
      _triggerNetworks.insert({trigger.getFullyQualifiedPath(), connectNetwork(trigger)});
    }

    debug("Finishing other networks...");
    auto connectingVisitor = [&](auto proxy) {
      if(auto it = _triggerNetworks.find(proxy.getFullyQualifiedPath()); it != _triggerNetworks.end()) {
        return;
      }

      connectNetwork(proxy);
    };

    // ChimeraTK::Model::keepParenthood - small optimisation for iterating the model only once
    _app.getModel().visit(connectingVisitor, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepProcessVariables,
        ChimeraTK::Model::keepParenthood);
  }

  /*********************************************************************************************************************/

  void ConnectionMaker::makeDirectConnectionForFeederWithImplementation(NetworkInformation& net) {
    debug("    Making direct connection for feeder with implementation");

    callForType(*net.valueType, [&](auto t) {
      using UserType = decltype(t);

      auto consumer = net.consumers.front();
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl;

      if(net.feeder.getType() == NodeType::Device) {
        feedingImpl = createDeviceVariable<UserType>(net.feeder);
      }
      else if(net.feeder.getType() == NodeType::ControlSystem) {
        // What we here already is:
        // - we have a 1:1 connection, so consumers is 1
        // - We want a feeder from the pv manager

        AccessModeFlags flags = {AccessMode::wait_for_new_data};
        if(consumer.getType() == NodeType::Application && consumer.getMode() == UpdateMode::poll) {
          flags = {};
        }

        feedingImpl = createProcessVariable<UserType>(net.feeder, net.valueLength, net.unit, net.description, flags);
      }
      else {
        throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
      }

      // We need a threaded fan-out most of the time, unless the consumer is an application node
      // Then we have a thread in the application module already
      auto needsFanOut{true};
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> consumingImpl;

      switch(consumer.getType()) {
        case NodeType::Application:
          debug("    Node type is Application");
          consumer.setAppAccessorImplementation(feedingImpl);
          needsFanOut = false;
          break;
        case NodeType::ControlSystem:
          debug("    Node type is ControlSystem");
          consumingImpl = createProcessVariable<UserType>(
              consumer, net.valueLength, net.unit, net.description, {AccessMode::wait_for_new_data});
          break;
        case NodeType::Device:
          consumingImpl = createDeviceVariable<UserType>(consumer);
          debug("    Node type is Device");
          break;
        case NodeType::TriggerReceiver: {
          needsFanOut = false;
          debug("    Node type is TriggerReceiver");

          // create the trigger fan out and store it in the map and the internalModuleList
          auto triggerFanOut =
              boost::make_shared<TriggerFanOut>(feedingImpl, *_app.getDeviceManager(consumer.getDeviceAlias()));
          _app.internalModuleList.push_back(triggerFanOut);
          net.triggerImpl[consumer.getDeviceAlias()] = triggerFanOut;
        } break;
        default:
          throw ChimeraTK::logic_error("Unexpected node type!");
      }

      if(needsFanOut) {
        assert(consumingImpl != nullptr);
        auto consumerImplPair = ConsumerImplementationPairs<UserType>{{consumingImpl, consumer}};
        auto fanOut = boost::make_shared<ThreadedFanOut<UserType>>(feedingImpl, consumerImplPair);
        _app.internalModuleList.push_back(fanOut);
      }
    });
  }

  /*********************************************************************************************************************/

  void ConnectionMaker::makeFanOutConnectionForFeederWithImplementation(
      NetworkInformation& net, const Model::DeviceModuleProxy& device, const Model::ProcessVariableProxy& trigger) {
    // TODO: needs sanity check?
    auto feederTrigger = !net.useExternalTrigger && net.feeder.getMode() == UpdateMode::push;
    assert(feederTrigger || net.useExternalTrigger || net.numberOfPollingConsumers == 1);

    callForType(*net.valueType, [&](auto t) {
      using UserType = decltype(t);

      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl;
      if(net.feeder.getType() == NodeType::Device) {
        feedingImpl = createDeviceVariable<UserType>(net.feeder);
      }
      else if(net.feeder.getType() == NodeType::ControlSystem) {
        debug("      CS feeder, creating CS variable");
        feedingImpl = createProcessVariable<UserType>(
            net.feeder, net.valueLength, net.unit, net.description, {AccessMode::wait_for_new_data});
      }
      else {
        throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
      }

      boost::shared_ptr<FanOut<UserType>> fanOut;
      boost::shared_ptr<ConsumingFanOut<UserType>> consumingFanOut;

      // Fanouts need to know the consumers on construction, so we collect them first
      auto consumerImplementationPairs = setConsumerImplementations<UserType>(net);

      if(net.useExternalTrigger) {
        assert(trigger.isValid());
        auto it = _triggerNetworks.find(trigger.getFullyQualifiedPath());
        assert(it != _triggerNetworks.end());

        debug("        Using external trigger.");
        NetworkInformation triggerNetwork = it->second;

        auto jt = triggerNetwork.triggerImpl.find(device.getAliasOrCdd());
        assert(jt != triggerNetwork.triggerImpl.end());

        // if external trigger is enabled, use externally triggered threaded
        // FanOut. Create one per external trigger impl.

        fanOut = jt->second->addNetwork(feedingImpl, consumerImplementationPairs);
      }
      else if(feederTrigger) {
        debug("        Using feeder trigger.");
        // if the trigger is provided by the pushing feeder, use the threaded
        // version of the FanOut to distribute new values immediately to all
        // consumers. Depending on whether we have a return channel or not, pick
        // the right implementation of the FanOut
        boost::shared_ptr<ThreadedFanOut<UserType>> threadedFanOut;
        if(not net.feeder.getDirection().withReturn) {
          debug("            No return channel");
          threadedFanOut = boost::make_shared<ThreadedFanOut<UserType>>(feedingImpl, consumerImplementationPairs);
        }
        else {
          debug("            With return channel");
          threadedFanOut =
              boost::make_shared<ThreadedFanOutWithReturn<UserType>>(feedingImpl, consumerImplementationPairs);
        }
        _app.internalModuleList.push_back(threadedFanOut);
        fanOut = threadedFanOut;
      }
      else {
        // FIXME: Is this case even used?
        debug("        No trigger, using consuming fanout.");
        consumingFanOut = boost::make_shared<ConsumingFanOut<UserType>>(feedingImpl, consumerImplementationPairs);

        // TODO Is this correct? we already added all consumer as slaves in the fanout  constructor.
        //      Maybe assert that we only have a single poll-type node (is there a check in checkConnections?)
        for(const auto& consumer : net.consumers) {
          if(consumer.getMode() == UpdateMode::poll) {
            consumer.setAppAccessorImplementation<UserType>(consumingFanOut);
            break;
          }
        }
      }
    });
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> ConnectionMaker::createProcessVariable(
      const VariableNetworkNode& node, size_t length, const std::string& unit, const std::string& description,
      AccessModeFlags flags) {
    SynchronizationDirection dir;
    if(node.getDirection().withReturn) {
      dir = SynchronizationDirection::bidirectional;
    }
    else if(node.getDirection().dir == VariableDirection::feeding) {
      dir = SynchronizationDirection::controlSystemToDevice;
    }
    else {
      dir = SynchronizationDirection::deviceToControlSystem;
    }

    debug("   calling createProcessArray()");

    auto pvImpl = _app.getPVManager()->createProcessArray<UserType>(
        dir, node.getPublicName(), length, unit, description, {}, 3, flags);

    if(node.getDirection().dir == VariableDirection::feeding) {
      // Wrap push-type CS->App PVs in testable mode decorator
      if(flags.has(AccessMode::wait_for_new_data)) {
        auto varId = detail::TestableMode::getNextVariableId();
        _app.pvIdMap[pvImpl->getUniqueId()] = varId;
        return _app.getTestableMode().decorate<UserType>(
            pvImpl, detail::TestableMode::DecoratorType::READ, "ControlSystem:" + node.getPublicName(), varId);
      }
      // Wrap poll-type CS->App PVs are not wrapped
      return pvImpl;
    }

    // App->CS PVs are only wrapped into testablemode decorator if they are bidirectional
    if(dir == SynchronizationDirection::bidirectional) {
      auto varId = detail::TestableMode::getNextVariableId();
      _app.pvIdMap[pvImpl->getUniqueId()] = varId;
      return _app.getTestableMode().decorate<UserType>(
          pvImpl, detail::TestableMode::DecoratorType::READ, "ControlSystem:" + node.getPublicName());
    }
    return pvImpl;
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  boost::shared_ptr<NDRegisterAccessor<UserType>> ConnectionMaker::createDeviceVariable(
      VariableNetworkNode const& node) {
    const auto& deviceAlias = node.getDeviceAlias();
    const auto& registerName = node.getRegisterName();
    auto direction = node.getDirection();
    auto mode = node.getMode();
    auto nElements = node.getNumberOfElements();

    auto dev = _app._deviceManagerMap.at(deviceAlias)->getDevice().getBackend();

    // use wait_for_new_data mode if push update mode was requested
    // Feeding to the network means reading from a device to feed it into the network.
    AccessModeFlags flags{};
    if(mode == UpdateMode::push && direction.dir == VariableDirection::feeding) flags = {AccessMode::wait_for_new_data};

    // obtain the register accessor from the device
    auto accessor = dev->getRegisterAccessor<UserType>(registerName, nElements, 0, flags);

    // Receiving accessors should be faulty after construction,
    // see data validity propagation spec 2.6.1
    if(node.getDirection().dir == VariableDirection::feeding) {
      accessor->setDataValidity(DataValidity::faulty);
    }

    // decorate push-type feeders with testable mode decorator, if needed
    if(mode == UpdateMode::push && direction.dir == VariableDirection::feeding) {
      accessor = _app.getTestableMode().decorate(accessor, detail::TestableMode::DecoratorType::READ);
    }

    return boost::make_shared<ExceptionHandlingDecorator<UserType>>(accessor, node);
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  ConsumerImplementationPairs<UserType> ConnectionMaker::setConsumerImplementations(NetworkInformation& net) {
    ConsumerImplementationPairs<UserType> consumerImplPairs;

    for(const auto& consumer : net.consumers) {
      typename ConsumerImplementationPairs<UserType>::value_type pair{
          boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>(), consumer};

      if(consumer.getType() == NodeType::Application) {
        auto impls = createApplicationVariable<UserType>(consumer);
        consumer.setAppAccessorImplementation<UserType>(impls.second);
        pair = std::make_pair(impls.first, consumer);
      }
      else if(consumer.getType() == NodeType::ControlSystem) {
        auto impl = createProcessVariable<UserType>(
            consumer, net.valueLength, net.unit, net.description, {AccessMode::wait_for_new_data});
        pair = std::make_pair(impl, consumer);
      }
      else if(consumer.getType() == NodeType::Device) {
        auto impl = createDeviceVariable<UserType>(consumer);
        pair = std::make_pair(impl, consumer);
      }
      else if(consumer.getType() == NodeType::TriggerReceiver) {
        auto triggerConnection = createApplicationVariable<UserType>(net.feeder);

        auto triggerFanOut = boost::make_shared<TriggerFanOut>(
            triggerConnection.second, *_app.getDeviceManager(consumer.getDeviceAlias()));
        _app.internalModuleList.push_back(triggerFanOut);
        net.triggerImpl[consumer.getDeviceAlias()] = triggerFanOut;

        pair = std::make_pair(triggerConnection.first, consumer);
      }
      else {
        throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
      }

      consumerImplPairs.push_back(pair);
    }

    return consumerImplPairs;
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>
      ConnectionMaker::createApplicationVariable(VariableNetworkNode const& node, VariableNetworkNode const& consumer) {
    // obtain the meta data
    size_t nElements = node.getNumberOfElements();
    std::string name = node.getName();
    assert(not name.empty());
    AccessModeFlags flags = {};
    if(consumer.isValid()) {
      if(consumer.getMode() == UpdateMode::push) flags = {AccessMode::wait_for_new_data};
    }
    else {
      if(node.getMode() == UpdateMode::push) flags = {AccessMode::wait_for_new_data};
    }

    // create the ProcessArray for the proper UserType
    std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>
        pvarPair;
    if(consumer.isValid()) {
      assert(node.getDirection().withReturn == consumer.getDirection().withReturn);
    }

    if(!node.getDirection().withReturn) {
      pvarPair = createSynchronizedProcessArray<UserType>(
          nElements, name, node.getUnit(), node.getDescription(), {}, 3, flags);
    }
    else {
      pvarPair = createBidirectionalSynchronizedProcessArray<UserType>(
          nElements, name, node.getUnit(), node.getDescription(), {}, 3, flags);
    }
    assert(pvarPair.first->getName() != "");
    assert(pvarPair.second->getName() != "");

    if(flags.has(AccessMode::wait_for_new_data)) {
      pvarPair = _app.getTestableMode().decorate(pvarPair, node, consumer);
    }

    // if debug mode was requested for either node, decorate both accessors
    if(_app.debugMode_variableList.count(node.getUniqueId()) ||
        (consumer.getType() != NodeType::invalid && _app.debugMode_variableList.count(consumer.getUniqueId()))) {
      if(consumer.getType() != NodeType::invalid) {
        assert(node.getDirection().dir == VariableDirection::feeding);
        assert(consumer.getDirection().dir == VariableDirection::consuming);
        pvarPair.first =
            boost::make_shared<DebugPrintAccessorDecorator<UserType>>(pvarPair.first, node.getQualifiedName());
        pvarPair.second =
            boost::make_shared<DebugPrintAccessorDecorator<UserType>>(pvarPair.second, consumer.getQualifiedName());
      }
      else {
        pvarPair.first =
            boost::make_shared<DebugPrintAccessorDecorator<UserType>>(pvarPair.first, node.getQualifiedName());
        pvarPair.second =
            boost::make_shared<DebugPrintAccessorDecorator<UserType>>(pvarPair.second, node.getQualifiedName());
      }
    }

    // return the pair
    return pvarPair;
  }

  /*********************************************************************************************************************/

  void ChimeraTK::ConnectionMaker::makeConnectionForFeederWithoutImplementation(NetworkInformation& net) {
    // we should be left with an application feeder node
    if(net.feeder.getType() != NodeType::Application) {
      throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
    }

    if(net.consumers.size() == 1) {
      debug("    Network of two nodes, connect directly");

      const auto& consumer = net.consumers.front();

      switch(consumer.getType()) {
        case NodeType::Application:
          debug("       Node type is Application");
          callForType(*net.valueType, [&](auto t) {
            using UserType = decltype(t);
            auto impls = createApplicationVariable<UserType>(net.feeder, consumer);
            net.feeder.setAppAccessorImplementation<UserType>(impls.first);
            consumer.setAppAccessorImplementation<UserType>(impls.second);
          });
          break;
        case NodeType::ControlSystem:
          debug("       Node type is ControlSystem");
          callForType(*net.valueType, [&](auto t) {
            using UserType = decltype(t);
            auto impl = createProcessVariable<UserType>(
                consumer, net.valueLength, net.unit, net.description, {AccessMode::wait_for_new_data});
            net.feeder.setAppAccessorImplementation(impl);
          });
          break;
        case NodeType::Device:
          debug("       Node type is Device");
          callForType(*net.valueType, [&](auto t) {
            using UserType = decltype(t);
            auto impl = createDeviceVariable<UserType>(consumer);
            net.feeder.setAppAccessorImplementation(impl);
          });
          break;
        case NodeType::TriggerReceiver:
          // This cannot happen. In a network Application -> TriggerReceiver, the connectNetwork()
          // code will always add a CS consumer, so there is never a 1:1 connection
          debug("       Node type is TriggerReceiver");
          assert(false);
          break;
        case NodeType::Constant:
          debug("       Node type is Constant");
          callForType(*net.valueType, [&](auto t) {
            using UserType = decltype(t);
            auto impl = consumer.createConstAccessor<UserType>({AccessMode::wait_for_new_data});
            net.feeder.setAppAccessorImplementation(impl);
          });
          break;
        default:
          throw ChimeraTK::logic_error("Unexpected node type!");
      }
    }
    else {
      debug("   More than one consumer, using fan-out as feeder impl");
      callForType(*net.valueType, [&](auto t) {
        using UserType = decltype(t);
        auto consumerImplementationPairs = setConsumerImplementations<UserType>(net);

        // create FanOut and use it as the feeder implementation
        auto fanOut = boost::make_shared<FeedingFanOut<UserType>>(net.feeder.getName(), net.unit, net.description,
            net.valueLength, net.feeder.getDirection().withReturn, consumerImplementationPairs);
        net.feeder.setAppAccessorImplementation<UserType>(fanOut);
      });
    }
  }

  /*********************************************************************************************************************/

  void ConnectionMaker::makeConnectionForConstantFeeder(NetworkInformation& net) {
    assert(net.feeder.getType() == NodeType::Constant);
    for(const auto& consumer : net.consumers) {
      AccessModeFlags flags{};
      if(consumer.getMode() == UpdateMode::push) {
        flags = {AccessMode::wait_for_new_data};
      }

      callForType(*net.valueType, [&](auto t) {
        using UserType = decltype(t);
        // each consumer gets its own implementation
        auto feedingImpl = net.feeder.createConstAccessor<UserType>(flags);
        if(consumer.getType() == NodeType::Application) {
          if(consumer.getMode() == UpdateMode::push) {
            consumer.setAppAccessorImplementation<UserType>(
                _app.getTestableMode().decorate(feedingImpl, detail::TestableMode::DecoratorType::READ, "Constant"));
          }
          else {
            consumer.setAppAccessorImplementation<UserType>(feedingImpl);
          }
        }
        else if(consumer.getType() == NodeType::ControlSystem) {
          auto impl = createProcessVariable<UserType>(
              consumer, net.valueLength, net.unit, net.description, {AccessMode::wait_for_new_data});
          impl->accessChannel(0) = feedingImpl->accessChannel(0);
          impl->write();
        }
        else if(consumer.getType() == NodeType::Device) {
          // we register the required accessor as a recovery accessor. This is just a bare RegisterAccessor without
          // any decorations directly from the backend.
          auto deviceManager = _app.getDeviceManager(consumer.getDeviceAlias());
          auto dev = deviceManager->getDevice().getBackend();
          auto impl =
              dev->getRegisterAccessor<UserType>(consumer.getRegisterName(), consumer.getNumberOfElements(), 0, {});
          impl->accessChannel(0) = feedingImpl->accessChannel(0);

          // assert(_deviceManagerMap.find(consumer.getDeviceAlias()) != _deviceManagerMap.end());

          // The accessor implementation already has its data in the user buffer. We now just have to add a valid
          // version number and have a recovery accessors (RecoveryHelper to be exact) which we can register at the
          // DeviceModule. As this is a constant we don't need to change it later and don't have to store it somewhere
          // else.
          deviceManager->addRecoveryAccessor(
              boost::make_shared<RecoveryHelper>(impl, VersionNumber(), deviceManager->writeOrder()));
        }
        else if(consumer.getType() == NodeType::TriggerReceiver) {
          assert(false);
          throw ChimeraTK::logic_error("Using constants as triggers is not supported!");
        }
        else {
          throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
        }
      });
    }
  }

} // namespace ChimeraTK
