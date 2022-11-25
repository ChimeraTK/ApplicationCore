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

        // feeding a constant (created with ApplicationModule::constant()) is not allowed
        if(boost::starts_with(node.getName(), ApplicationModule::namePrefixConstant)) {
          throw ChimeraTK::logic_error("Feeding a constant is not allowed (" + node.getQualifiedName() + ")");
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
          throw ChimeraTK::logic_error("Variable network " + proxy.getFullyQualifiedPath() +
              " contains nodes with different types: " + net.valueType->name() + " != " + node.getValueType().name());
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
      throw ChimeraTK::logic_error(
          "Variable network '" + proxy.getFullyQualifiedPath() + "' is empty. Must not happen");
    }

    return net;
  }

  /*********************************************************************************************************************/

  NetworkVisitor::NetworkInformation NetworkVisitor::checkAndFinaliseNetwork(Model::ProcessVariableProxy& proxy) {
    // This will do two things:
    //  - Check the network consistency
    //  - Return feeder and consumers, if available
    auto info = checkNetwork(proxy);
    finaliseNetwork(info);
    return info;
  }

  /*********************************************************************************************************************/

  void NetworkVisitor::finaliseNetwork(NetworkInformation& net) {
    // check whether this is a constant created via ApplicationModule::constant()
    bool isConstant{net.consumers.size() > 0 &&
        boost::starts_with(net.consumers.front().getName(), ApplicationModule::namePrefixConstant)};
    if(isConstant) {
      assert(!net.feeder.isValid());

      net.feeder =
          VariableNetworkNode{&net.consumers.front().getValueType(), true, net.consumers.front().getNumberOfElements()};

      std::string stringValue = net.consumers.front().getName().substr(ApplicationModule::namePrefixConstant.length());

      callForType(net.consumers.front().getValueType(), [&](auto t) {
        using UserType = decltype(t);
        net.feeder.setConstantValue(userTypeToUserType<UserType>(stringValue));
      });
    }

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

    if(not neededFeeder and not isConstant) {
      // Only add CS consumer if we did not previously add CS feeder, we will add one or the other, but never both
      // Also we will not add CS consumers for constants.
      debug("  Network has a non-CS feeder, can create additional ControlSystem consumer");
      net.consumers.push_back(VariableNetworkNode(
          net.proxy->getFullyQualifiedPath(), {VariableDirection::consuming, false}, *net.valueType, net.valueLength));
    }
    assert(not net.consumers.empty());

    // register PVs with the control system adapter
    callForType(*net.valueType, [&](auto t) {
      using UserType = decltype(t);

      for(auto& node : net.consumers) {
        if(node.getType() != NodeType::ControlSystem) {
          continue;
        }
        this->createProcessVariable<UserType>(
            node, net.valueLength, net.unit, net.description, {AccessMode::wait_for_new_data});
      }

      if(net.feeder.getType() == NodeType::ControlSystem) {
        AccessModeFlags flags = {AccessMode::wait_for_new_data};

        if(net.consumers.size() == 1) {
          auto consumer = net.consumers.front();
          if(consumer.getType() == NodeType::Application && consumer.getMode() == UpdateMode::poll) {
            flags = {};
          }
        }

        AccessModeFlags{};
        this->createProcessVariable<UserType>(net.feeder, net.valueLength, net.unit, net.description, flags);
      }
    });
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

  void ConnectionMaker::connectNetwork(Model::ProcessVariableProxy& proxy) {
    auto path = proxy.getFullyQualifiedPath();
    debug("Network found: ", path);

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
    if(_networks.at(path).feeder.getMode() == UpdateMode::poll && _networks.at(path).numberOfPollingConsumers != 1) {
      _networks.at(path).useExternalTrigger = true;
      std::tie(trigger, device) =
          proxy.visit(triggerFinder, Model::adjacentInSearch, Model::keepPvAccess, Model::keepDeviceModules,
              Model::returnFirstHit(std::make_pair(Model::ProcessVariableProxy{}, Model::DeviceModuleProxy{})));
      if(!trigger.isValid()) {
        throw ChimeraTK::logic_error(
            "Poll-Type feeder " + _networks.at(path).feeder.getName() + " needs trigger, but none provided");
      }
    }

    auto constantFeeder = _networks.at(path).feeder.getType() == NodeType::Constant;

    if(_networks.at(path).feeder.hasImplementation() && !constantFeeder) {
      debug("  Creating fixed implementation for feeder '", _networks.at(path).feeder.getName(), "'...");

      if(_networks.at(path).consumers.size() == 1 && !_networks.at(path).useExternalTrigger) {
        debug("    One consumer without external trigger, creating direct connection");
        makeDirectConnectionForFeederWithImplementation(_networks.at(path));
      }
      else {
        // More than one consuming node
        debug("    More than one consuming node or having external trigger, setting up FanOut");
        makeFanOutConnectionForFeederWithImplementation(_networks.at(path), device, trigger);
      }
    }
    else if(not constantFeeder) {
      debug("    Feeder '", _networks.at(path).feeder.getName(), "' does not require a fixed implementation.");
      assert(not trigger.isValid());
      makeConnectionForFeederWithoutImplementation(_networks.at(path));
    }
    else { // constant feeder
      debug("    Using constant feeder '", _networks.at(path).feeder.getName(), "'.");
      makeConnectionForConstantFeeder(_networks.at(path));
    }

    // Mark circular networks
    for(auto& node : _networks.at(path).consumers) {
      // A variable network is a tree-like network of VariableNetworkNodes (one feeder and one or more multiple
      // consumers) A circular network is a list of modules (EntityOwners) which have a circular dependency
      auto circularNetwork = node.scanForCircularDepencency();
      if(not circularNetwork.empty()) {
        auto circularNetworkHash = boost::hash_range(circularNetwork.begin(), circularNetwork.end());
        _app.circularDependencyNetworks[circularNetworkHash] = circularNetwork;
        _app.circularNetworkInvalidityCounters[circularNetworkHash] = 0;
      }
    }
  }

  /*********************************************************************************************************************/

  void ConnectionMaker::finalise() {
    debug("Calling finalise()...");

    _app.getTestableMode()._debugDecorating = _debugConnections;

    debug("  Preparing trigger networks");
    debug("    Collecting triggers");

    // Collect all triggers, add a TriggerReceiver placeholder for every device associated with that trigger
    auto triggerCollector = [&](auto proxy) {
      auto trigger = proxy.getTrigger();
      if(not trigger.isValid()) return;

      triggers.insert(trigger);
      proxy.addVariable(trigger, VariableNetworkNode(proxy.getAliasOrCdd(), 0));
    };
    _app.getModel().visit(triggerCollector, Model::depthFirstSearch, Model::keepDeviceModules);

    debug("    Finalising trigger networks");
    for(auto trigger : triggers) {
      auto info = checkAndFinaliseNetwork(trigger);
      _triggerNetworks.insert(trigger.getFullyQualifiedPath());
      _networks.insert({trigger.getFullyQualifiedPath(), info});
      debug("      trigger network: " + trigger.getFullyQualifiedPath());
    }

    debug("  Finalising other networks");
    auto connectingVisitor = [&](auto proxy) {
      if(_triggerNetworks.count(proxy.getFullyQualifiedPath()) != 0) {
        return;
      }

      _networks.insert({proxy.getFullyQualifiedPath(), checkAndFinaliseNetwork(proxy)});
    };

    // ChimeraTK::Model::keepParenthood - small optimisation for iterating the model only once
    _app.getModel().visit(connectingVisitor, ChimeraTK::Model::depthFirstSearch, ChimeraTK::Model::keepProcessVariables,
        ChimeraTK::Model::keepParenthood);
  }

  /*********************************************************************************************************************/

  void ConnectionMaker::connect() {
    debug("Calling connect()...");

    _app.getTestableMode()._debugDecorating = _debugConnections;

    // Improve: Likely no need to distinguish trigger and normal networks here... Also just iterate _networks instead
    // of the model!

    debug("  Connecting trigger networks");
    for(auto trigger : triggers) {
      connectNetwork(trigger);
    }

    debug("  Connecting other networks");
    auto connectingVisitor = [&](auto proxy) {
      if(_triggerNetworks.count(proxy.getFullyQualifiedPath()) != 0) {
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
        feedingImpl = getProcessVariable<UserType>(net.feeder);
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
          consumingImpl = getProcessVariable<UserType>(consumer);
          break;
        case NodeType::Device:
          consumingImpl = createDeviceVariable<UserType>(consumer);
          debug("    Node type is Device");
          break;
        case NodeType::TriggerReceiver: {
          needsFanOut = false;
          debug("    Node type is TriggerReceiver (Alias = " + consumer.getDeviceAlias() + ")");

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
        feedingImpl = getProcessVariable<UserType>(net.feeder);
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

        debug("        Using external trigger (Alias = " + device.getAliasOrCdd() + ")");

        auto& triggerNet = _networks.at(trigger.getFullyQualifiedPath());
        auto jt = triggerNet.triggerImpl.find(device.getAliasOrCdd());
        assert(jt != triggerNet.triggerImpl.end());

        // if external trigger is enabled, use externally triggered threaded
        // FanOut. Create one per external trigger impl.

        jt->second->addNetwork(feedingImpl, consumerImplementationPairs);
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
        // Trigger by single poll-type consumer
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
  void NetworkVisitor::createProcessVariable(const VariableNetworkNode& node, size_t length, const std::string& unit,
      const std::string& description, AccessModeFlags flags) {
    // Implementation note: This function needs to create the PV in the control system PV manager, so the control system
    // adapter already sees the PVs before calling run().
    // It also has to decorate the implementation with the testable mode decorator (if in testable mode), because this
    // must happen before the TestFacility hands out decorated PVs to the tests.

    // If we are generating the XML file only, there will be no PV manager and we will not use the PVs later anyway,
    // so simply do nothing in that case. Note that Application::initialise() checks for the presence of a PV manager,
    // so if the real application starts we have the guarantee of the presence of a PV manager.
    if(!_app.getPVManager()) {
      return;
    }

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

    auto pv = _app.getPVManager()->createProcessArray<UserType>(
        dir, node.getPublicName(), length, unit, description, {}, 3, flags);

    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> pvImpl = pv;

    if(node.getDirection().dir == VariableDirection::feeding) {
      // Wrap push-type CS->App PVs in testable mode decorator
      if(flags.has(AccessMode::wait_for_new_data)) {
        auto varId = detail::TestableMode::getNextVariableId();
        _app.pvIdMap[pv->getUniqueId()] = varId;
        pvImpl = _app.getTestableMode().decorate<UserType>(
            pvImpl, detail::TestableMode::DecoratorType::READ, "ControlSystem:" + node.getPublicName(), varId);
      }
      // poll-type CS->App PVs are not wrapped
    }
    else if(dir == SynchronizationDirection::bidirectional) {
      // App->CS PVs are only wrapped into testablemode decorator if they are bidirectional
      auto varId = detail::TestableMode::getNextVariableId();
      _app.pvIdMap[pv->getUniqueId()] = varId;
      pvImpl = _app.getTestableMode().decorate<UserType>(
          pvImpl, detail::TestableMode::DecoratorType::READ, "ControlSystem:" + node.getPublicName());
    }

    boost::fusion::at_key<UserType>(_decoratedPvImpls.table)[node.getPublicName()] = pvImpl;
  }

  /*********************************************************************************************************************/

  template<typename UserType>
  boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> ConnectionMaker::getProcessVariable(
      const VariableNetworkNode& node) {
    return boost::fusion::at_key<UserType>(_decoratedPvImpls.table).at(node.getPublicName());
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
    debug("    setConsumerImplementations");
    ConsumerImplementationPairs<UserType> consumerImplPairs;

    for(const auto& consumer : net.consumers) {
      typename ConsumerImplementationPairs<UserType>::value_type pair{
          boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>(), consumer};

      if(consumer.getType() == NodeType::Application) {
        debug("      Node type is Application: " + consumer.getQualifiedName());
        auto impls = createApplicationVariable<UserType>(consumer);
        consumer.setAppAccessorImplementation<UserType>(impls.second);
        pair = std::make_pair(impls.first, consumer);
      }
      else if(consumer.getType() == NodeType::ControlSystem) {
        debug("      Node type is ControlSystem");
        auto impl = getProcessVariable<UserType>(consumer);
        pair = std::make_pair(impl, consumer);
      }
      else if(consumer.getType() == NodeType::Device) {
        debug("      Node type is Device");
        auto impl = createDeviceVariable<UserType>(consumer);
        pair = std::make_pair(impl, consumer);
      }
      else if(consumer.getType() == NodeType::TriggerReceiver) {
        debug("      Node type is TriggerReceiver");
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
            auto impl = getProcessVariable<UserType>(consumer);
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
          debug("       Node type is TriggerReceiver");

          // create a PV implementation to connect the Application with the TriggerFanOut.
          {
            boost::shared_ptr<TransferElement> consumingImpl;
            callForType(*net.valueType, [&](auto t) {
              using UserType = decltype(t);
              auto impls = createApplicationVariable<UserType>(net.feeder, consumer);
              net.feeder.setAppAccessorImplementation<UserType>(impls.first);
              consumingImpl = impls.second;
            });

            // create the trigger fan out and store it in the map and the internalModuleList
            auto triggerFanOut =
                boost::make_shared<TriggerFanOut>(consumingImpl, *_app.getDeviceManager(consumer.getDeviceAlias()));
            _app.internalModuleList.push_back(triggerFanOut);
            net.triggerImpl[consumer.getDeviceAlias()] = triggerFanOut;
          }

          break;
        case NodeType::Constant:
          debug("       Node type is Constant");
          net.feeder.setAppAccessorConstImplementation(net.feeder);
          break;
        default:
          throw ChimeraTK::logic_error("Unexpected node type!");
      }
    }
    else if(net.consumers.size() > 1) {
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
    else {
      debug("   No consumer (presumably optimised out)");
      net.feeder.setAppAccessorConstImplementation(VariableNetworkNode(net.valueType, true, net.valueLength));
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
        if(consumer.getType() == NodeType::Application) {
          consumer.setAppAccessorConstImplementation(net.feeder);
        }
        else if(consumer.getType() == NodeType::ControlSystem) {
          throw ChimeraTK::logic_error("Using constants as feeders for control system variables is not supported!");
        }
        else if(consumer.getType() == NodeType::Device) {
          // We register the required accessor as a recovery accessor. This is just a bare RegisterAccessor without
          // any decorations directly from the backend.
          auto deviceManager = _app.getDeviceManager(consumer.getDeviceAlias());
          auto dev = deviceManager->getDevice().getBackend();
          auto impl =
              dev->getRegisterAccessor<UserType>(consumer.getRegisterName(), consumer.getNumberOfElements(), 0, {});

          // Set the value
          impl->accessChannel(0) =
              std::vector<UserType>(consumer.getNumberOfElements(), net.feeder.getConstantValue<UserType>());

          // The accessor implementation already has its data in the user buffer. We now just have to add a valid
          // version number and have a recovery accessors (RecoveryHelper to be exact) which we can register at the
          // DeviceModule. As this is a constant we don't need to change it later and don't have to store it somewhere
          // else.
          deviceManager->addRecoveryAccessor(
              boost::make_shared<RecoveryHelper>(impl, VersionNumber(), deviceManager->writeOrder()));
        }
        else if(consumer.getType() == NodeType::TriggerReceiver) {
          throw ChimeraTK::logic_error("Using constants as triggers is not supported!");
        }
        else {
          throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
        }
      });
    }
  }

  /*********************************************************************************************************************/

  void ConnectionMaker::optimiseUnmappedVariables(const std::set<std::string>& names) {
    for(const auto& name : names) {
      auto& network = _networks.at(name);
      // if the control system is the feeder, change it into a constant
      if(network.feeder.getType() == NodeType::ControlSystem) {
        network.feeder = VariableNetworkNode(network.valueType, true, network.valueLength);
      }
      else {
        // control system is a consumer: remove it from the list of consumers
        network.consumers.remove_if([](auto& consumer) { return consumer.getType() == NodeType::ControlSystem; });
      }
    }
  }

} // namespace ChimeraTK
