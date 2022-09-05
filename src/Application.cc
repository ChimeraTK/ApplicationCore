// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Application.h"

#include "CircularDependencyDetector.h"
#include "ConsumingFanOut.h"
#include "DebugPrintAccessorDecorator.h"
#include "DeviceModule.h"
#include "ExceptionHandlingDecorator.h"
#include "FeedingFanOut.h"
#include "TestableModeAccessorDecorator.h"
#include "ThreadedFanOut.h"
#include "TriggerFanOut.h"
#include "VariableNetworkGraphDumpingVisitor.h"
#include "VariableNetworkModuleGraphDumpingVisitor.h"
#include "VariableNetworkNode.h"
#include "XMLGeneratorVisitor.h"

#include <ChimeraTK/BackendFactory.h>

#include <boost/fusion/container/map.hpp>

#include <exception>
#include <fstream>
#include <string>
#include <thread>

using namespace ChimeraTK;

/*********************************************************************************************************************/

Application::Application(const std::string& name) : ApplicationBase(name), ModuleGroup(name) {
  // check if the application name has been set
  if(applicationName == "") {
    shutdown();
    throw ChimeraTK::logic_error("Error: An instance of Application must have its applicationName set.");
  }
  // check if application name contains illegal characters
  std::string legalChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_";
  bool nameContainsIllegalChars = name.find_first_not_of(legalChars) != std::string::npos;
  if(nameContainsIllegalChars) {
    shutdown();
    throw ChimeraTK::logic_error("Error: The application name may only contain "
                                 "alphanumeric characters and underscores.");
  }
}

/*********************************************************************************************************************/

void Application::defineConnections() {
  ControlSystemModule cs;
  findTag(".*").connectTo(cs);
}

/*********************************************************************************************************************/

void Application::enableTestableMode() {
  assert(not initialiseCalled);
  testableMode.enable();
}

/*********************************************************************************************************************/

void Application::registerThread(const std::string& name) {
  getInstance().testableMode.setThreadName(name);
  pthread_setname_np(pthread_self(), name.substr(0, std::min<std::string::size_type>(name.length(), 15)).c_str());
}

/*********************************************************************************************************************/

void Application::incrementDataLossCounter(const std::string& name) {
  if(getInstance().debugDataLoss) {
    std::cout << "Data loss in variable " << name << std::endl;
  }
  getInstance().dataLossCounter++;
}

/*********************************************************************************************************************/

size_t Application::getAndResetDataLossCounter() {
  size_t counter = getInstance().dataLossCounter.load(std::memory_order_relaxed);
  while(!getInstance().dataLossCounter.compare_exchange_weak(
      counter, 0, std::memory_order_release, std::memory_order_relaxed)) {
  }
  return counter;
}

/*********************************************************************************************************************/

void Application::initialise() {
  if(initialiseCalled) {
    throw ChimeraTK::logic_error("Application::initialise() was already called before.");
  }

  // call the user-defined defineConnections() function which describes the structure of the application
  defineConnections();
  for(auto& module : getSubmoduleListRecursive()) {
    module->defineConnections();
  }

  // call defineConnections() for all device modules
  for(auto& devModule : deviceModuleMap) {
    devModule.second->defineConnections();
  }

  // find and handle constant nodes
  findConstantNodes();

  // connect any unconnected accessors with constant values
  processUnconnectedNodes();

  // realise the connections between variable accessors as described in the
  // initialise() function
  makeConnections();

  // set flag to prevent further calls to this function and to prevent definition of additional connections.
  initialiseCalled = true;
}

/*********************************************************************************************************************/

void Application::optimiseUnmappedVariables(const std::set<std::string>& names) {
  for(const auto& pv : names) {
    auto& node = controlSystemVariables.at(pv);
    auto& network = node.getOwner();
    if(network.getFeedingNode() == node) {
      // cannot optimise if network is fed by unmapped control system variable (anyway not eating CPU ressources)
      continue;
    }
    if(network.getConsumingNodes().size() == 1) {
      if(network.getFeedingNode().getType() == NodeType::Application) {
        callForType(network.getValueType(), [&](auto t) {
          using UserType = decltype(t);

          // replace consuming node with constant in the model
          network.removeNode(network.getConsumingNodes().front());
          auto constNode = VariableNetworkNode::makeConstant<UserType>(
              false, UserType(), network.getFeedingNode().getNumberOfElements());
          network.addNode(constNode);

          // change application accessor into constant
          auto& acc = network.getFeedingNode().getAppAccessor<UserType>();
          auto constImpl = constNode.template createConstAccessor<UserType>({});
          acc.replace(constImpl);
        });
      }
      else {
        auto fanOut = network.getFanOut();
        assert(fanOut);
        // FanOut is present: disable it
        fanOut->disable();
      }
    }
    else {
      // more than one consumer: we have a fan out -> remove control system consumer from it.
      auto fanOut = network.getFanOut();
      assert(fanOut != nullptr);
      fanOut->removeSlave(_processVariableManager->getProcessVariable(pv));
    }
  }
}

/*********************************************************************************************************************/

/** Functor class to create a constant for otherwise unconnected variables,
 * suitable for boost::fusion::for_each(). */
namespace {
  struct CreateConstantForUnconnectedVar {
    /// @todo test unconnected variables for all types!
    CreateConstantForUnconnectedVar(const std::type_info& typeInfo, bool makeFeeder, size_t length)
    : _typeInfo(typeInfo), _makeFeeder(makeFeeder), _length(length) {}

    template<typename PAIR>
    void operator()(PAIR&) const {
      if(typeid(typename PAIR::first_type) != _typeInfo) return;
      theNode = VariableNetworkNode::makeConstant<typename PAIR::first_type>(
          _makeFeeder, typename PAIR::first_type(), _length);
      done = true;
    }

    const std::type_info& _typeInfo;
    bool _makeFeeder;
    size_t _length;
    mutable bool done{false};
    mutable VariableNetworkNode theNode;
  };
} // namespace

/*********************************************************************************************************************/

void Application::processUnconnectedNodes() {
  for(auto& module : getSubmoduleListRecursive()) {
    for(auto& accessor : module->getAccessorList()) {
      if(!accessor.hasOwner()) {
        if(enableUnconnectedVariablesWarning) {
          std::cerr << "*** Warning: Variable '" << accessor.getQualifiedName()
                    << "' is not connected. " // LCOV_EXCL_LINE
                       "Reading will always result in 0, writing will be ignored."
                    << std::endl; // LCOV_EXCL_LINE
        }
        networkList.emplace_back();
        networkList.back().addNode(accessor);

        bool makeFeeder = !(networkList.back().hasFeedingNode());
        size_t length = accessor.getNumberOfElements();
        auto callable = CreateConstantForUnconnectedVar(accessor.getValueType(), makeFeeder, length);
        boost::fusion::for_each(ChimeraTK::userTypeMap(), std::ref(callable));
        assert(callable.done);
        constantList.emplace_back(callable.theNode);
        networkList.back().addNode(constantList.back());
      }
    }
  }
}

/*********************************************************************************************************************/

void Application::checkConnections() {
  // check all networks for validity
  for(auto& network : networkList) {
    network.check();
  }

  // check if all accessors are connected
  // note: this in principle cannot happen, since processUnconnectedNodes() is
  // called before
  for(auto& module : getSubmoduleListRecursive()) {
    for(auto& accessor : module->getAccessorList()) {
      if(!accessor.hasOwner()) {
        throw ChimeraTK::logic_error("The accessor '" + accessor.getName() + "' of the module '" +
            module->getName() +      // LCOV_EXCL_LINE
            "' was not connected!"); // LCOV_EXCL_LINE
      }
    }
  }
}

/*********************************************************************************************************************/

void Application::run() {
  assert(applicationName != "");

  if(testableMode.enabled) {
    if(!testFacilityRunApplicationCalled) {
      throw ChimeraTK::logic_error(
          "Testable mode enabled but Application::run() called directly. Call TestFacility::runApplication() instead.");
    }
  }

  if(runCalled) {
    throw ChimeraTK::logic_error("Application::run() has already been called before.");
  }
  runCalled = true;

  // set all initial version numbers in the modules to the same value
  for(auto& module : getSubmoduleListRecursive()) {
    if(module->getModuleType() != ModuleType::ApplicationModule) continue;
    module->setCurrentVersionNumber(getStartVersion());
  }

  // prepare the modules
  for(auto& module : getSubmoduleListRecursive()) {
    module->prepare();
  }
  for(auto& deviceModule : deviceModuleMap) {
    deviceModule.second->prepare();
  }

  // Switch life-cycle state to run
  lifeCycleState = LifeCycleState::run;

  // start the necessary threads for the FanOuts etc.
  for(auto& internalModule : internalModuleList) {
    internalModule->activate();
  }

  for(auto& deviceModule : deviceModuleMap) {
    deviceModule.second->run();
  }

  // start the threads for the modules
  for(auto& module : getSubmoduleListRecursive()) {
    module->run();
  }

  // When in testable mode, wait for all modules to report that they have reached the testable mode.
  // We have to start all module threads first because some modules might only send the initial
  // values in their main loop, and following modules need them to enter testable mode.

  // just a small helper lambda to avoid code repetition
  auto waitForTestableMode = [](EntityOwner* module) {
    while(!module->hasReachedTestableMode()) {
      Application::getInstance().getTestableMode().unlock("releaseForReachTestableMode");
      usleep(100);
      Application::getInstance().getTestableMode().lock("acquireForReachTestableMode");
    }
  };

  if(Application::getInstance().getTestableMode().enabled) {
    for(auto& internalModule : internalModuleList) {
      waitForTestableMode(internalModule.get());
    }
    for(auto& deviceModule : deviceModuleMap) {
      waitForTestableMode(deviceModule.second);
    }
    for(auto& module : getSubmoduleListRecursive()) {
      waitForTestableMode(module);
    }
  }

  // Launch circular dependency detector thread
  circularDependencyDetector.startDetectBlockedModules();
}

/*********************************************************************************************************************/

void Application::shutdown() {
  // switch life-cycle state
  lifeCycleState = LifeCycleState::shutdown;

  // first allow to run the application threads again, if we are in testable
  // mode
  if(testableMode.enabled && testableMode.testLock()) {
    testableMode.unlock("shutdown");
  }

  // deactivate the FanOuts first, since they have running threads inside
  // accessing the modules etc. (note: the modules are members of the
  // Application implementation and thus get destroyed after this destructor)
  for(auto& internalModule : internalModuleList) {
    internalModule->deactivate();
  }

  // next deactivate the modules, as they have running threads inside as well
  for(auto& module : getSubmoduleListRecursive()) {
    module->terminate();
  }

  for(auto& deviceModule : deviceModuleMap) {
    deviceModule.second->terminate();
  }

  circularDependencyDetector.terminate();

  ApplicationBase::shutdown();
}
/*********************************************************************************************************************/

void Application::generateXML() {
  assert(applicationName != "");

  // define the connections
  defineConnections();
  for(auto& module : getSubmoduleListRecursive()) {
    module->defineConnections();
  }

  // create connections for exception handling
  for(auto& devModule : deviceModuleMap) {
    devModule.second->defineConnections();
  }

  // find and handle constant nodes
  findConstantNodes();

  // also search for unconnected nodes - this is here only executed to print the
  // warnings
  processUnconnectedNodes();

  // finalise connections: decide still-undecided details, in particular for
  // control-system and device variables, which get created "on the fly".
  finaliseNetworks();

  XMLGeneratorVisitor visitor;
  visitor.dispatch(*this);
  visitor.save(applicationName + ".xml");
}

/*********************************************************************************************************************/

VariableNetwork& Application::connect(VariableNetworkNode a, VariableNetworkNode b) {
  // if one of the nodes has the value type AnyType, set it to the type of the
  // other if both are AnyType, nothing changes.
  if(a.getValueType() == typeid(AnyType)) {
    a.setValueType(b.getValueType());
  }
  else if(b.getValueType() == typeid(AnyType)) {
    b.setValueType(a.getValueType());
  }

  // if one of the nodes has not yet a defined number of elements, set it to the
  // number of elements of the other. if both are undefined, nothing changes.
  if(a.getNumberOfElements() == 0) {
    a.setNumberOfElements(b.getNumberOfElements());
  }
  else if(b.getNumberOfElements() == 0) {
    b.setNumberOfElements(a.getNumberOfElements());
  }
  if(a.getNumberOfElements() != b.getNumberOfElements()) {
    std::stringstream what;
    what << "*** ERROR: Cannot connect array variables with difference number "
            "of elements!"
         << std::endl;
    what << "Node A:" << std::endl;
    a.dump(what);
    what << "Node B:" << std::endl;
    b.dump(what);
    throw ChimeraTK::logic_error(what.str());
  }

  // if both nodes already have an owner, we are either already done (same
  // owners) or we need to try to merge the networks
  if(a.hasOwner() && b.hasOwner()) {
    if(&(a.getOwner()) != &(b.getOwner())) {
      auto& networkToMerge = b.getOwner();
      bool success = a.getOwner().merge(networkToMerge);
      if(!success) {
        std::stringstream what;
        what << "*** ERROR: Trying to connect two nodes which are already part "
                "of different networks, and merging these"
                " networks is not possible (cannot have two non-control-system "
                "or two control-system feeders)!"
             << std::endl;
        what << "Node A:" << std::endl;
        a.dump(what);
        what << "Node B:" << std::endl;
        b.dump(what);
        what << "Owner of node A:" << std::endl;
        a.getOwner().dump("", what);
        what << "Owner of node B:" << std::endl;
        b.getOwner().dump("", what);
        throw ChimeraTK::logic_error(what.str());
      }
      for(auto n = networkList.begin(); n != networkList.end(); ++n) {
        if(&*n == &networkToMerge) {
          networkList.erase(n);
          break;
        }
      }
    }
  }
  // add b to the existing network of a
  else if(a.hasOwner()) {
    a.getOwner().addNode(b);
  }

  // add a to the existing network of b
  else if(b.hasOwner()) {
    b.getOwner().addNode(a);
  }
  // create new network
  else {
    networkList.emplace_back();
    networkList.back().addNode(a);
    networkList.back().addNode(b);
  }
  return a.getOwner();
}

/*********************************************************************************************************************/

template<typename UserType>
boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> Application::createDeviceVariable(
    VariableNetworkNode const& node) {
  const auto& deviceAlias = node.getDeviceAlias();
  const auto& registerName = node.getRegisterName();
  auto direction = node.getDirection();
  auto mode = node.getMode();
  auto nElements = node.getNumberOfElements();

  // Device opens in DeviceModule
  if(deviceMap.count(deviceAlias) == 0) {
    deviceMap[deviceAlias] = ChimeraTK::BackendFactory::getInstance().createBackend(deviceAlias);
  }

  // use wait_for_new_data mode if push update mode was requested
  // Feeding to the network means reading from a device to feed it into the network.
  ChimeraTK::AccessModeFlags flags{};
  if(mode == UpdateMode::push && direction.dir == VariableDirection::feeding) flags = {AccessMode::wait_for_new_data};

  // obtain the register accessor from the device
  auto accessor = deviceMap[deviceAlias]->getRegisterAccessor<UserType>(registerName, nElements, 0, flags);

  // Receiving accessors should be faulty after construction,
  // see data validity propagation spec 2.6.1
  if(node.getDirection().dir == VariableDirection::feeding) {
    accessor->setDataValidity(DataValidity::faulty);
  }

  // decorate push-type feeders with testable mode decorator, if needed
  if(testableMode.enabled) {
    if(mode == UpdateMode::push && direction.dir == VariableDirection::feeding) {
      auto varId = getNextVariableId();
      accessor = boost::make_shared<TestableModeAccessorDecorator<UserType>>(accessor, true, false, varId, varId);
    }
  }

  return boost::make_shared<ExceptionHandlingDecorator<UserType>>(accessor, node);
}

/*********************************************************************************************************************/

template<typename UserType>
boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> Application::createProcessVariable(
    VariableNetworkNode const& node) {
  // determine the SynchronizationDirection
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
  AccessModeFlags flags = {};
  if(node.getDirection().dir == VariableDirection::consuming) { // Application-to-controlsystem must be
                                                                // push-type
    flags = {AccessMode::wait_for_new_data};
  }
  else {
    if(node.getOwner().getConsumingNodes().size() == 1 &&
        node.getOwner().getConsumingNodes().front().getType() == NodeType::Application) {
      // exactly one consumer which is an ApplicationModule input: decide flag depending on input type
      auto consumer = node.getOwner().getConsumingNodes().front();
      if(consumer.getMode() == UpdateMode::push) flags = {AccessMode::wait_for_new_data};
    }
    else {
      // multiple consumers (or a single Device/CS consumer) result in a ThreadedFanOut which requires push-type input
      flags = {AccessMode::wait_for_new_data};
    }
  }

  // create the ProcessArray for the proper UserType
  auto pvar = _processVariableManager->createProcessArray<UserType>(dir, node.getPublicName(),
      node.getNumberOfElements(), node.getOwner().getUnit(), node.getOwner().getDescription(), {}, 3, flags);
  assert(pvar->getName() != "");

  // create variable ID
  auto varId = getNextVariableId();
  pvIdMap[pvar->getUniqueId()] = varId;

  // Decorate the process variable if testable mode is enabled and this is the receiving end of the variable (feeding
  // to the network), or a bidirectional consumer. Also don't decorate, if the mode is polling. Instead flag the
  // variable to be polling, so the TestFacility is aware of this.
  if(testableMode.enabled) {
    auto& variable = testableMode.variables[varId];
    if(node.getDirection().dir == VariableDirection::feeding) {
      // The transfer mode of this process variable is considered to be polling,
      // if only one consumer exists and this consumer is polling. Reason:
      // multiple consumers will result in the use of a FanOut, so the
      // communication up to the FanOut will be push-type, even if all consumers
      // are poll-type.
      /// @todo Check if this is true!
      auto mode = UpdateMode::push;
      if(node.getOwner().countConsumingNodes() == 1) {
        if(node.getOwner().getConsumingNodes().front().getMode() == UpdateMode::poll) mode = UpdateMode::poll;
      }

      if(mode != UpdateMode::poll) {
        auto pvarDec = boost::make_shared<TestableModeAccessorDecorator<UserType>>(pvar, true, false, varId, varId);
        variable.name = "ControlSystem:" + node.getPublicName();
        return pvarDec;
      }
      else {
        variable.isPollMode = true;
      }
    }
    else if(node.getDirection().withReturn) {
      // Return channels are always push. The decorator must handle only reads on the return channel, since writes into
      // the control system do not block the testable mode.
      auto pvarDec = boost::make_shared<TestableModeAccessorDecorator<UserType>>(pvar, true, false, varId, varId);
      variable.name = "ControlSystem:" + node.getPublicName();
      return pvarDec;
    }
  }

  // return the process variable
  return pvar;
}

/*********************************************************************************************************************/

template<typename UserType>
std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>
    Application::createApplicationVariable(VariableNetworkNode const& node, VariableNetworkNode const& consumer) {
  // obtain the meta data
  size_t nElements = node.getNumberOfElements();
  std::string name = node.getName();
  assert(name != "");
  AccessModeFlags flags = {};
  if(consumer.getType() != NodeType::invalid) {
    if(consumer.getMode() == UpdateMode::push) flags = {AccessMode::wait_for_new_data};
  }
  else {
    if(node.getMode() == UpdateMode::push) flags = {AccessMode::wait_for_new_data};
  }

  // create the ProcessArray for the proper UserType
  std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>
      pvarPair;
  if(consumer.getType() != NodeType::invalid)
    assert(node.getDirection().withReturn == consumer.getDirection().withReturn);
  if(!node.getDirection().withReturn) {
    pvarPair =
        createSynchronizedProcessArray<UserType>(nElements, name, node.getUnit(), node.getDescription(), {}, 3, flags);
  }
  else {
    pvarPair = createBidirectionalSynchronizedProcessArray<UserType>(
        nElements, name, node.getUnit(), node.getDescription(), {}, 3, flags);
  }
  assert(pvarPair.first->getName() != "");
  assert(pvarPair.second->getName() != "");

  // create variable IDs
  size_t varId = getNextVariableId();
  size_t varIdReturn;
  if(node.getDirection().withReturn) varIdReturn = getNextVariableId();

  // decorate the process variable if testable mode is enabled and mode is push-type
  if(testableMode.enabled && flags.has(AccessMode::wait_for_new_data)) {
    if(!node.getDirection().withReturn) {
      pvarPair.first =
          boost::make_shared<TestableModeAccessorDecorator<UserType>>(pvarPair.first, false, true, varId, varId);
      pvarPair.second =
          boost::make_shared<TestableModeAccessorDecorator<UserType>>(pvarPair.second, true, false, varId, varId);
    }
    else {
      pvarPair.first =
          boost::make_shared<TestableModeAccessorDecorator<UserType>>(pvarPair.first, true, true, varIdReturn, varId);
      pvarPair.second =
          boost::make_shared<TestableModeAccessorDecorator<UserType>>(pvarPair.second, true, true, varId, varIdReturn);
    }

    // put the decorators into the list
    auto& variable = testableMode.variables[varId];
    variable.name = "Internal:" + node.getQualifiedName();
    if(consumer.getType() != NodeType::invalid) {
      variable.name += "->" + consumer.getQualifiedName();
    }
    auto& returnVariable = testableMode.variables[varIdReturn];
    if(node.getDirection().withReturn) returnVariable.name = variable.name + " (return)";
  }

  // if debug mode was requested for either node, decorate both accessors
  if(debugMode_variableList.count(node.getUniqueId()) ||
      (consumer.getType() != NodeType::invalid && debugMode_variableList.count(consumer.getUniqueId()))) {
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

void Application::makeConnections() {
  // finalise connections: decide still-undecided details, in particular for
  // control-system and device variables, which get created "on the fly".
  finaliseNetworks();

  // apply optimisations
  // note: checks may not be run before since sometimes networks may only be
  // valid after optimisations
  optimiseConnections();

  // run checks
  checkConnections();

  // make the connections for all networks
  for(auto& network : networkList) {
    makeConnectionsForNetwork(network);
  }

  // check for circular dependencies
  for(auto& network : networkList) {
    markCircularConsumers(network);
  }
}

/*********************************************************************************************************************/

void Application::findConstantNodes() {
  for(auto& module : getSubmoduleListRecursive()) {
    for(auto& accessor : module->getAccessorList()) {
      if(accessor.getName().substr(0, 7) == "@CONST@") {
        VariableNetworkNode node(accessor);

        // must be a consumer node
        if(node.getDirection().dir != VariableDirection::consuming) {
          std::cout << "Variable name '@CONST@' found for a feeding type node:" << std::endl;
          node.dump();
          if(node.hasOwner()) {
            std::cout << "In network:" << std::endl;
            node.getOwner().dump();
          }
          throw ChimeraTK::logic_error("Variable name '@CONST@' found for a feeding type node.");
        }

        // must have no feeder, a constant feeder, or a control-system feeder
        if(node.hasOwner() && node.getOwner().hasFeedingNode() &&
            node.getOwner().getFeedingNode().getType() != NodeType::Constant &&
            node.getOwner().getFeedingNode().getType() != NodeType::ControlSystem) {
          std::cout << "Error: Variable name '@CONST@' found with wrong feeder type:" << std::endl;
          node.getOwner().getFeedingNode().dump();
          std::cout << "In network:" << std::endl;
          node.getOwner().dump();
          throw ChimeraTK::logic_error("Variable name '@CONST@' found with wrong feeder type");
        }

        if(node.hasOwner()) {
          // The @CONST@ variable names do not distinguish types, hence we can have differently typed consumers in our
          // network. To mitigate this issue, we remove the node from the existing network before creating a new one
          // below
          auto& network = node.getOwner();
          network.removeNode(node);
          // if network has no consumer left, abolish it
          if(network.countConsumingNodes() == 0) {
            // remove feeder if existing
            if(network.hasFeedingNode()) {
              auto feeder = network.getFeedingNode();
              network.removeNode(feeder);
            }
            // remove network from application
            networkList.remove(network);
          }
        }

        // Connect node with constant (in a new VariableNetwork)
        callForType(node.getValueType(), [&node](auto t) {
          using UserType = decltype(t);
          auto value = userTypeToUserType<UserType>(node.getName().substr(7));
          makeConstant(value) >> node;
        });
      }
    }
  }
}

/*********************************************************************************************************************/

void Application::finaliseNetworks() {
  // check for control system variables which should be made bidirectional
  for(auto& network : networkList) {
    size_t nBidir = network.getFeedingNode().getDirection().withReturn ? 1 : 0;
    for(auto& consumer : network.getConsumingNodes()) {
      if(consumer.getDirection().withReturn) ++nBidir;
    }
    if(nBidir != 1) {
      // only if there is exactly one node with return channel we need to guess its peer
      continue;
    }
    if(network.getFeedingNode().getType() != NodeType::ControlSystem) {
      // only a feeding control system variable can be made bidirectional
      continue;
    }
    network.getFeedingNode().setDirection({VariableDirection::feeding, true});
  }

  // check for networks which have an external trigger but should be triggered by pollling consumer
  for(auto& network : networkList) {
    if(network.getTriggerType() == VariableNetwork::TriggerType::external) {
      size_t pollingComsumers{0};
      for(auto& consumer : network.getConsumingNodes()) {
        if(consumer.getMode() == UpdateMode::poll) ++pollingComsumers;
      }
      if(pollingComsumers == 1) {
        network.getFeedingNode().removeExternalTrigger();
      }
    }
  }
}

/*********************************************************************************************************************/

void Application::optimiseConnections() {
  // list of iterators of networks to be removed from the networkList after the
  // merge operation
  std::list<VariableNetwork*> deleteNetworks;

  // search for networks with the same feeder
  for(auto it1 = networkList.begin(); it1 != networkList.end(); ++it1) {
    for(auto it2 = it1; it2 != networkList.end(); ++it2) {
      if(it1 == it2) continue;

      auto feeder1 = it1->getFeedingNode();
      auto feeder2 = it2->getFeedingNode();

      // this optimisation is only necessary for device-type nodes, since
      // application and control-system nodes will automatically create merged
      // networks when having the same feeder
      /// @todo check if this assumption is true! control-system nodes can be
      /// created with different types, too!
      if(feeder1.getType() != NodeType::Device || feeder2.getType() != NodeType::Device) continue;

      // check if referring to same register
      if(feeder1.getDeviceAlias() != feeder2.getDeviceAlias()) continue;
      if(feeder1.getRegisterName() != feeder2.getRegisterName()) continue;

      // check if directions are the same
      if(feeder1.getDirection() != feeder2.getDirection()) continue;

      // check if value types and number of elements are compatible
      if(feeder1.getValueType() != feeder2.getValueType()) continue;
      if(feeder1.getNumberOfElements() != feeder2.getNumberOfElements()) continue;

      // check if transfer mode is the same
      if(feeder1.getMode() != feeder2.getMode()) continue;

      // check if triggers are compatible, if present
      if(feeder1.hasExternalTrigger() != feeder2.hasExternalTrigger()) continue;
      if(feeder1.hasExternalTrigger()) {
        if(feeder1.getExternalTrigger() != feeder2.getExternalTrigger()) continue;
      }

      // everything should be compatible at this point: merge the networks. We
      // will merge the network of the outer loop into the network of the inner
      // loop, since the network of the outer loop will not be found a second
      // time in the inner loop.
      for(auto& consumer : it1->getConsumingNodes()) {
        consumer.clearOwner();
        it2->addNode(consumer);
      }

      // if trigger present, remove corresponding trigger receiver node from the
      // trigger network
      if(feeder1.hasExternalTrigger()) {
        feeder1.getExternalTrigger().getOwner().removeNodeToTrigger(it1->getFeedingNode());
      }

      // schedule the outer loop network for deletion and stop processing it
      deleteNetworks.push_back(&(*it1));
      break;
    }
  }

  // remove networks from the network list
  for(auto net : deleteNetworks) {
    networkList.remove(*net);
  }
}

/*********************************************************************************************************************/

void Application::dumpConnections(std::ostream& stream) {                                          // LCOV_EXCL_LINE
  stream << "==== List of all variable connections of the current Application =====" << std::endl; // LCOV_EXCL_LINE
  for(auto& network : networkList) {                                                               // LCOV_EXCL_LINE
    network.dump("", stream);                                                                      // LCOV_EXCL_LINE
  }                                                                                                // LCOV_EXCL_LINE
  stream << "==== List of all circular connections in the current Application ====" << std::endl;  // LCOV_EXCL_LINE
  for(auto& circularDependency : circularDependencyNetworks) {
    stream << "Circular dependency network " << circularDependency.first << " : ";
    for(auto& module : circularDependency.second) {
      stream << module->getName() << ", ";
    }
    stream << std::endl;
  }
  stream << "======================================================================" << std::endl; // LCOV_EXCL_LINE
} // LCOV_EXCL_LINE

/*********************************************************************************************************************/

void Application::dumpConnectionGraph(const std::string& fileName) {
  std::fstream file{fileName, std::ios_base::out};

  VariableNetworkGraphDumpingVisitor visitor{file};
  visitor.dispatch(*this);
}

/*********************************************************************************************************************/

void Application::dumpModuleConnectionGraph(const std::string& fileName) const {
  std::fstream file{fileName, std::ios_base::out};

  VariableNetworkModuleGraphDumpingVisitor visitor{file};
  visitor.dispatch(*this);
}

/*********************************************************************************************************************/

Application::TypedMakeConnectionCaller::TypedMakeConnectionCaller(Application& owner, VariableNetwork& network)
: _owner(owner), _network(network) {}

/*********************************************************************************************************************/

template<typename PAIR>
void Application::TypedMakeConnectionCaller::operator()(PAIR&) const {
  if(typeid(typename PAIR::first_type) != _network.getValueType()) return;
  _owner.typedMakeConnection<typename PAIR::first_type>(_network);
  done = true;
}

/*********************************************************************************************************************/

void Application::makeConnectionsForNetwork(VariableNetwork& network) {
  // if the network has been created already, do nothing
  if(network.isCreated()) return;

  // if the trigger type is external, create the trigger first
  if(network.getFeedingNode().hasExternalTrigger()) {
    VariableNetwork& dependency = network.getFeedingNode().getExternalTrigger().getOwner();
    if(!dependency.isCreated()) makeConnectionsForNetwork(dependency);
  }

  // defer actual network creation to templated function
  auto callable = TypedMakeConnectionCaller(*this, network);
  boost::fusion::for_each(ChimeraTK::userTypeMap(), std::ref(callable));
  assert(callable.done);

  // mark the network as created
  network.markCreated();
}

/*********************************************************************************************************************/

void Application::markCircularConsumers(VariableNetwork& variableNetwork) {
  for(auto& node : variableNetwork.getConsumingNodes()) {
    // A variable network is a tree-like network of VariableNetworkNodes (one feeder and one or more multiple consumers)
    // A circular network is a list of modules (EntityOwners) which have a circular dependency
    auto circularNetwork = node.scanForCircularDepencency();
    if(circularNetwork.size() > 0) {
      auto circularNetworkHash = boost::hash_range(circularNetwork.begin(), circularNetwork.end());
      circularDependencyNetworks[circularNetworkHash] = circularNetwork;
      circularNetworkInvalidityCounters[circularNetworkHash] = 0;
    }
  }
}
/*********************************************************************************************************************/

template<typename UserType>
void Application::typedMakeConnection(VariableNetwork& network) {
  if(enableDebugMakeConnections) {
    std::cout << std::endl << "Executing typedMakeConnections for network:" << std::endl;
    network.dump("", std::cout);
    std::cout << std::endl;
  }
  try {                          // catch exceptions to add information about the failed network
    bool connectionMade = false; // to check the logic...

    size_t nNodes = network.countConsumingNodes() + 1;
    auto feeder = network.getFeedingNode();
    auto consumers = network.getConsumingNodes();
    bool useExternalTrigger = network.getTriggerType() == VariableNetwork::TriggerType::external;
    bool useFeederTrigger = network.getTriggerType() == VariableNetwork::TriggerType::feeder;
    bool constantFeeder = feeder.getType() == NodeType::Constant;

    // first case: the feeder requires a fixed implementation
    if(feeder.hasImplementation() && !constantFeeder) {
      if(enableDebugMakeConnections) {
        std::cout << "  Creating fixed implementation for feeder '" << feeder.getName() << "'..." << std::endl;
      }

      // Create feeding implementation.
      boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> feedingImpl;
      if(feeder.getType() == NodeType::Device) {
        feedingImpl = createDeviceVariable<UserType>(feeder);
      }
      else if(feeder.getType() == NodeType::ControlSystem) {
        feedingImpl = createProcessVariable<UserType>(feeder);
      }
      else {
        throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
      }

      // if we just have two nodes, directly connect them
      if(nNodes == 2 && !useExternalTrigger) {
        if(enableDebugMakeConnections) {
          std::cout << "    Setting up direct connection without external trigger." << std::endl;
        }
        bool needsFanOut{false};
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> consumingImpl;

        auto consumer = consumers.front();
        if(consumer.getType() == NodeType::Application) {
          consumer.setAppAccessorImplementation(feedingImpl);

          connectionMade = true;
        }
        else if(consumer.getType() == NodeType::Device) {
          consumingImpl = createDeviceVariable<UserType>(consumer);
          // connect the Device with e.g. a ControlSystem node via a
          // ThreadedFanOut
          needsFanOut = true;
        }
        else if(consumer.getType() == NodeType::ControlSystem) {
          consumingImpl = createProcessVariable<UserType>(consumer);
          // connect the ControlSystem with e.g. a Device node via an
          // ThreadedFanOut
          needsFanOut = true;
        }
        else if(consumer.getType() == NodeType::TriggerReceiver) {
          consumer.getNodeToTrigger().getOwner().setExternalTriggerImpl(feedingImpl);
        }
        else {
          throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
        }

        if(needsFanOut) {
          assert(consumingImpl != nullptr);
          auto consumerImplPair = ConsumerImplementationPairs<UserType>{{consumingImpl, consumer}};
          auto fanOut = boost::make_shared<ThreadedFanOut<UserType>>(feedingImpl, network, consumerImplPair);
          internalModuleList.push_back(fanOut);
          network.setFanOut(fanOut);
        }

        connectionMade = true;
      }
      else { /* !(nNodes == 2 && !useExternalTrigger) */
        if(enableDebugMakeConnections) {
          std::cout << "    Setting up triggered connection." << std::endl;
        }

        // create the right FanOut type
        boost::shared_ptr<FanOut<UserType>> fanOut;
        boost::shared_ptr<ConsumingFanOut<UserType>> consumingFanOut;

        // Fanouts need to know the consumers on construction, so we collect them first
        auto consumerImplementationPairs = setConsumerImplementations<UserType>(feeder, consumers);

        if(useExternalTrigger) {
          if(enableDebugMakeConnections) {
            std::cout << "      Using external trigger." << std::endl;
          }

          // if external trigger is enabled, use externally triggered threaded
          // FanOut. Create one per external trigger impl.
          void* triggerImplId = network.getExternalTriggerImpl().get();
          auto triggerFanOut = triggerMap[triggerImplId];
          if(!triggerFanOut) {
            assert(deviceModuleMap.find(feeder.getDeviceAlias()) != deviceModuleMap.end());

            // create the trigger fan out and store it in the map and the internalModuleList
            triggerFanOut = boost::make_shared<TriggerFanOut>(
                network.getExternalTriggerImpl(), *deviceModuleMap[feeder.getDeviceAlias()], network);
            triggerMap[triggerImplId] = triggerFanOut;
            internalModuleList.push_back(triggerFanOut);
          }
          fanOut = triggerFanOut->addNetwork(feedingImpl, consumerImplementationPairs);
          network.setFanOut(fanOut);
        }
        else if(useFeederTrigger) {
          if(enableDebugMakeConnections) {
            std::cout << "      Using trigger provided by the feeder." << std::endl;
          }

          // if the trigger is provided by the pushing feeder, use the threaded
          // version of the FanOut to distribute new values immediately to all
          // consumers. Depending on whether we have a return channel or not, pick
          // the right implementation of the FanOut
          boost::shared_ptr<ThreadedFanOut<UserType>> threadedFanOut;
          if(!feeder.getDirection().withReturn) {
            threadedFanOut =
                boost::make_shared<ThreadedFanOut<UserType>>(feedingImpl, network, consumerImplementationPairs);
          }
          else {
            threadedFanOut = boost::make_shared<ThreadedFanOutWithReturn<UserType>>(
                feedingImpl, network, consumerImplementationPairs);
          }
          internalModuleList.push_back(threadedFanOut);
          fanOut = threadedFanOut;
          network.setFanOut(fanOut);
        }
        else {
          if(enableDebugMakeConnections) {
            std::cout << "      No trigger, using consuming fanout." << std::endl;
          }
          assert(network.hasApplicationConsumer()); // checkConnections should
                                                    // catch this
          consumingFanOut = boost::make_shared<ConsumingFanOut<UserType>>(feedingImpl, consumerImplementationPairs);
          fanOut = consumingFanOut;
          network.setFanOut(fanOut);

          // TODO Is this correct? we already added all consumer as slaves in the fanout  constructor.
          //      Maybe assert that we only have a single poll-type node (is there a check in checkConnections?)
          for(auto& consumer : consumers) {
            if(consumer.getMode() == UpdateMode::poll) {
              consumer.setAppAccessorImplementation<UserType>(consumingFanOut);
              break;
            }
          }
        }
        connectionMade = true;
      }
    }
    // second case: the feeder does not require a fixed implementation
    else if(!constantFeeder) { /* !feeder.hasImplementation() */

      if(enableDebugMakeConnections) {
        std::cout << "  Feeder '" << feeder.getName() << "' does not require a fixed implementation." << std::endl;
      }

      // we should be left with an application feeder node
      if(feeder.getType() != NodeType::Application) {
        throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
      }
      assert(!useExternalTrigger);
      // if we just have two nodes, directly connect them
      if(nNodes == 2) {
        auto consumer = consumers.front();
        if(consumer.getType() == NodeType::Application) {
          auto impls = createApplicationVariable<UserType>(feeder, consumer);
          feeder.setAppAccessorImplementation<UserType>(impls.first);
          consumer.setAppAccessorImplementation<UserType>(impls.second);
        }
        else if(consumer.getType() == NodeType::ControlSystem) {
          auto impl = createProcessVariable<UserType>(consumer);
          feeder.setAppAccessorImplementation<UserType>(impl);
        }
        else if(consumer.getType() == NodeType::Device) {
          auto impl = createDeviceVariable<UserType>(consumer);
          feeder.setAppAccessorImplementation<UserType>(impl);
        }
        else if(consumer.getType() == NodeType::TriggerReceiver) {
          auto impls = createApplicationVariable<UserType>(feeder, consumer);
          feeder.setAppAccessorImplementation<UserType>(impls.first);
          consumer.getNodeToTrigger().getOwner().setExternalTriggerImpl(impls.second);
        }
        else if(consumer.getType() == NodeType::Constant) {
          auto impl = consumer.createConstAccessor<UserType>({});
          feeder.setAppAccessorImplementation<UserType>(impl);
        }
        else {
          throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
        }
        connectionMade = true;
      }
      else {
        auto consumerImplementationPairs = setConsumerImplementations<UserType>(feeder, consumers);

        // create FanOut and use it as the feeder implementation
        auto fanOut =
            boost::make_shared<FeedingFanOut<UserType>>(feeder.getName(), feeder.getUnit(), feeder.getDescription(),
                feeder.getNumberOfElements(), feeder.getDirection().withReturn, consumerImplementationPairs);
        feeder.setAppAccessorImplementation<UserType>(fanOut);
        network.setFanOut(fanOut);

        connectionMade = true;
      }
    }
    else { /* constantFeeder */

      if(enableDebugMakeConnections) {
        std::cout << "  Using constant feeder '" << feeder.getName() << "'..." << std::endl;
      }
      assert(feeder.getType() == NodeType::Constant);

      for(auto& consumer : consumers) {
        AccessModeFlags flags{};
        if(consumer.getMode() == UpdateMode::push) {
          flags = {AccessMode::wait_for_new_data};
        }

        // each consumer gets its own implementation
        auto feedingImpl = feeder.createConstAccessor<UserType>(flags);

        if(consumer.getType() == NodeType::Application) {
          if(testableMode.enabled && consumer.getMode() == UpdateMode::push) {
            auto varId = getNextVariableId();
            auto pvarDec =
                boost::make_shared<TestableModeAccessorDecorator<UserType>>(feedingImpl, true, false, varId, varId);
            testableMode.variables[varId].name = "Constant";
            consumer.setAppAccessorImplementation<UserType>(pvarDec);
          }
          else {
            consumer.setAppAccessorImplementation<UserType>(feedingImpl);
          }
        }
        else if(consumer.getType() == NodeType::ControlSystem) {
          auto impl = createProcessVariable<UserType>(consumer);
          impl->accessChannel(0) = feedingImpl->accessChannel(0);
          impl->write();
        }
        else if(consumer.getType() == NodeType::Device) {
          // we register the required accessor as a recovery accessor. This is just a bare RegisterAccessor without any
          // decorations directly from the backend.
          if(deviceMap.count(consumer.getDeviceAlias()) == 0) {
            deviceMap[consumer.getDeviceAlias()] =
                ChimeraTK::BackendFactory::getInstance().createBackend(consumer.getDeviceAlias());
          }
          auto impl = deviceMap[consumer.getDeviceAlias()]->getRegisterAccessor<UserType>(
              consumer.getRegisterName(), consumer.getNumberOfElements(), 0, {});
          impl->accessChannel(0) = feedingImpl->accessChannel(0);

          assert(deviceModuleMap.find(consumer.getDeviceAlias()) != deviceModuleMap.end());
          DeviceModule* devmod = deviceModuleMap[consumer.getDeviceAlias()];

          // The accessor implementation already has its data in the user buffer. We now just have to add a valid
          // version number and have a recovery accessors (RecoveryHelper to be exact) which we can register at the
          // DeviceModule. As this is a constant we don't need to change it later and don't have to store it somewhere
          // else.
          devmod->addRecoveryAccessor(boost::make_shared<RecoveryHelper>(impl, VersionNumber(), devmod->writeOrder()));
        }
        else if(consumer.getType() == NodeType::TriggerReceiver) {
          throw ChimeraTK::logic_error("Using constants as triggers is not supported!");
        }
        else {
          throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
        }
      }
      connectionMade = true;
    }

    if(!connectionMade) {                                                     // LCOV_EXCL_LINE (assert-like)
      throw ChimeraTK::logic_error(                                           // LCOV_EXCL_LINE (assert-like)
          "The variable network cannot be handled. Implementation missing!"); // LCOV_EXCL_LINE (assert-like)
    }                                                                         // LCOV_EXCL_LINE (assert-like)
  }
  catch(ChimeraTK::logic_error& e) {
    std::stringstream ss;
    ss << "ChimeraTK::logic_error thrown in Application::typedMakeConnection():" << std::endl
       << "  " << e.what() << std::endl
       << "For network:" << std::endl;
    network.dump("", ss);
    throw ChimeraTK::logic_error(ss.str());
  }
}

/*********************************************************************************************************************/

template<typename UserType>
std::list<std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>, VariableNetworkNode>> Application::
    setConsumerImplementations(VariableNetworkNode const& feeder, std::list<VariableNetworkNode> consumers) {
  std::list<std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>, VariableNetworkNode>>
      consumerImplPairs;

  /** Map of deviceAliases to their corresponding TriggerFanOuts */
  std::map<std::string, boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>> triggerFanOuts;

  for(auto& consumer : consumers) {
    bool addToConsumerImplPairs{true};
    std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>, VariableNetworkNode> pair{
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>(), consumer};

    if(consumer.getType() == NodeType::Application) {
      auto impls = createApplicationVariable<UserType>(consumer);
      consumer.setAppAccessorImplementation<UserType>(impls.second);
      pair = std::make_pair(impls.first, consumer);
    }
    else if(consumer.getType() == NodeType::ControlSystem) {
      auto impl = createProcessVariable<UserType>(consumer);
      pair = std::make_pair(impl, consumer);
    }
    else if(consumer.getType() == NodeType::Device) {
      auto impl = createDeviceVariable<UserType>(consumer);
      pair = std::make_pair(impl, consumer);
    }
    else if(consumer.getType() == NodeType::TriggerReceiver) {
      // In case we have one or more trigger receivers among our consumers, we produce one
      // consuming application variable for each device. Later this will create a TriggerFanOut for
      // each trigger consumer, i.e. one per device so one blocking device does not affect the others.

      std::string deviceAlias = consumer.getNodeToTrigger().getOwner().getFeedingNode().getDeviceAlias();
      auto triggerFanOut = triggerFanOuts[deviceAlias];
      if(triggerFanOut == nullptr) {
        // create a new process variable pair and set the sender/feeder to the fan out
        auto triggerConnection = createApplicationVariable<UserType>(feeder);
        triggerFanOut = triggerConnection.second;
        triggerFanOuts[deviceAlias] = triggerFanOut;

        assert(triggerConnection.first != nullptr);
        pair = std::make_pair(triggerConnection.first, consumer);
      }
      else {
        // We already have added a pair for this device
        addToConsumerImplPairs = false;
      }
      consumer.getNodeToTrigger().getOwner().setExternalTriggerImpl(triggerFanOut);
    }
    else {
      throw ChimeraTK::logic_error("Unexpected node type!"); // LCOV_EXCL_LINE (assert-like)
    }

    if(addToConsumerImplPairs) {
      consumerImplPairs.push_back(pair);
    }
  }

  return consumerImplPairs;
}

/*********************************************************************************************************************/

VariableNetwork& Application::createNetwork() {
  networkList.emplace_back();
  return networkList.back();
}

/*********************************************************************************************************************/

Application& Application::getInstance() {
  return dynamic_cast<Application&>(ApplicationBase::getInstance());
}

/*********************************************************************************************************************/

void Application::registerDeviceModule(DeviceModule* deviceModule) {
  deviceModuleMap[deviceModule->deviceAliasOrURI] = deviceModule;
}

/*********************************************************************************************************************/

void Application::unregisterDeviceModule(DeviceModule* deviceModule) {
  deviceModuleMap.erase(deviceModule->deviceAliasOrURI);
}
