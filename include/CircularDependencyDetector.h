// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Module.h"
#include "VariableNetworkNode.h"
#include <unordered_set>

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

#include <map>
#include <ostream>

namespace ChimeraTK::detail {

  /** Detection mechanism for circular dependencies of initial values in ApplicationModules */
  struct CircularDependencyDetector : boost::noncopyable {
    CircularDependencyDetector() = default;
    ~CircularDependencyDetector();

    /// Call before an ApplicationModule waits for an initial value on the given node. Calls with
    /// non-Application-typed nodes are ignored.
    void registerDependencyWait(VariableNetworkNode& node);

    /// Call after an ApplicationModule has received an initial value on the given node. Calls with
    /// non-Application-typed nodes are ignored.
    void unregisterDependencyWait(VariableNetworkNode& node);

    /// Print modules which are currently waiting for initial values to the given stream.
    /// By default, std::cout is used as a stream (the ChimeraTK::Logger cannot be used here as we
    /// cannot take a reference to a temporary object).
    void printWaiters(std::ostream& stream = std::cout);

    /// Stop the thread before ApplicationBase::terminate() is called.
    void terminate();

    /// Start detection thread
    void startDetectBlockedModules();

    /// Function executed in thread
    void detectBlockedModules();

   protected:
    std::mutex _mutex;
    std::map<Module*, Module*> _waitMap;
    std::map<Module*, std::string> _awaitedVariables;
    std::map<EntityOwner*, VariableNetworkNode> _awaitedNodes;
    std::unordered_set<Module*> _modulesWeHaveWarnedAbout;
    std::unordered_set<std::string> _devicesWeHaveWarnedAbout;
    std::unordered_set<NodeType> _otherThingsWeHaveWarnedAbout;
    boost::thread _thread;

   private:
  };
} // namespace ChimeraTK::detail
