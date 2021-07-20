#include "VariableNetworkModuleGraphDumpingVisitor.h"
#include "Application.h"
#include "VariableNetwork.h"
#include "VisitorHelper.h"
#include "Module.h"
#include "DeviceModule.h"

#include <algorithm>
#include <sstream>
#include <typeinfo>

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {

  /********************************************************************************************************************/

  VariableNetworkModuleGraphDumpingVisitor::VariableNetworkModuleGraphDumpingVisitor(std::ostream& stream)
  : Visitor<Application, Module, VariableNetwork>(), VariableNetworkNodeDumpingVisitor(stream, "\\n") {}

  /********************************************************************************************************************/

  void VariableNetworkModuleGraphDumpingVisitor::dispatch(const Application& t) {
    stream() << "digraph application {\n"
             << "  label=\"<" << boost::core::demangle(typeid(t).name()) << ">" << t.getName() << "\";\n"
             << "  tooltip=\"\";\n"
             << "  layout=\"twopi\";\n"
             << "  fontname=\"Sans\";\n"
             << "  fontsize=\"36\";\n"
             << "  overlap=\"scalexy\";\n"
             << "  node [style=\"filled,rounded\", shape=box, fontsize=\"10\"];\n"
             << "  edge [fontsize=\"10\"];\n"
             << "  root=\"TheCenter\";\n"
             << "  splines=true;\n"
             << "  \n";

    // create nodes for all ApplicationModules
    for(const auto& module : t.getSubmoduleListRecursive()) {
      if(module->getModuleType() != EntityOwner::ModuleType::ApplicationModule) continue;
      module->accept(*this);
    }

    // create nodes for all DeviceModules
    for(const auto& pair : t.deviceModuleMap) {
      pair.second->accept(*this);
    }

    // collect edges (one edge per pair of connected modules)
    for(const auto& network : t.networkList) {
      network.accept(*this);
    }

    // Correct directions of the edges: to get useful graphs, graphviz must be able to rank the modules properly
    // The Devices should have the first rank. (The ControlSystem is not represented by a node in the graph...)
    // Arrows are pointing to the last rank
    // In the twopi mode, the first rank is in the middle
    // First, create a symmetric connection map containing all connections (twice, because symmetric)
    std::map<std::string, std::unordered_set<std::string>> connectionMap;
    for(const auto& edge : _edgeMap) {
      auto arrowPos = edge.first.find("->");
      std::string first = edge.first.substr(0, arrowPos);
      std::string second = edge.first.substr(arrowPos + 2);
      connectionMap[first].insert(second);
      connectionMap[second].insert(first);
    }
    // Now compute for each edge the longest distance to any device for both ends of the edge
    std::list<std::pair<std::string, std::string>> swapList;
    std::unordered_set<std::string> constraintMap;
    for(const auto& edge : _edgeMap) {
      std::list<std::string> longestPath;

      // Define functor to recurse the connection map
      std::function<void(const std::list<std::string>& path, size_t&)> recurse;
      recurse = [&](const std::list<std::string>& path, size_t& distance) {
        auto nextList = connectionMap[path.back()];
        for(const auto& next : nextList) {
          // stop recursion if device is found
          if(next.substr(0, 7) == "Device_") {
            if(path.size() > distance) {
              // update distance
              distance = path.size();
              // update longestPath
              longestPath = path;
              longestPath.push_back(next);
            }
            return;
          }

          // stop recursion if loop is found
          if(std::find(path.begin(), path.end(), next) != path.end()) {
            return;
          }

          // continue recursion
          std::list<std::string> nextPath = path;
          nextPath.push_back(next);
          recurse(nextPath, distance);
        }

        // if highest level: update constraintMap from longestPath
        if(path.size() == 1) {
          std::string prevNode;
          for(const auto& node : longestPath) {
            if(!prevNode.empty()) {
              constraintMap.insert(prevNode + "->" + node);
              constraintMap.insert(node + "->" + prevNode);
            }
            prevNode = node;
          }
        }
      };

      // split edge name to get the two module names
      auto arrowPos = edge.first.find("->");
      std::string first = edge.first.substr(0, arrowPos);
      std::string second = edge.first.substr(arrowPos + 2);

      size_t firstDistance = 0;
      size_t secondDistance = 0;

      // fast path: if one of the modules is a Device, there is no need to compute the distances
      if(first.substr(0, 7) == "Device_") {
        // first is the device, do not swap
        secondDistance = 1;
        constraintMap.insert(first + "->" + second);
        constraintMap.insert(second + "->" + first);
      }
      else if(second.substr(0, 7) == "Device_") {
        // second is the device, swap
        firstDistance = 1;
        constraintMap.insert(first + "->" + second);
        constraintMap.insert(second + "->" + first);
      }
      else {
        // compute the distances
        recurse({first}, firstDistance);
        recurse({second}, secondDistance);
      }

      // the end with the smaller (longest) distance must come first
      if(secondDistance < firstDistance) {
        // swap the two ends later (we cannot alter the map in the loop)
        swapList.emplace_back(std::make_pair(first, second));
      }
    }

    // perform the pre-recorded swaps
    for(auto& swapPair : swapList) {
      _edgeMap[swapPair.second + "->" + swapPair.first] = _edgeMap.at(swapPair.first + "->" + swapPair.second);
      _edgeMap[swapPair.second + "->" + swapPair.first].second = true;
      auto it = _edgeMap.find(swapPair.first + "->" + swapPair.second);
      assert(it != _edgeMap.end());
      _edgeMap.erase(it);
    }

    // create edges from device to invisble center
    stream() << "TheCenter[label=\"TheCenter\",style=\"invis\"]\n";
    for(const auto& device : _deviceList) {
      stream() << "TheCenter->" << device << " [style=\"invis\"]\n";
    }

    // create collected edges
    for(const auto& edge : _edgeMap) {
      stream() << edge.first;
      stream() << "[tooltip=\"" << edge.second.first << "\"";
      if(!constraintMap.count(edge.first)) {
        stream() << ",constraint=false";
      }
      if(edge.second.second) {
        stream() << ",dir=back";
      }
      stream() << "]\n";
    }

    stream() << "}\n";
  }

  /********************************************************************************************************************/

  void VariableNetworkModuleGraphDumpingVisitor::dispatch(const Module& t) {
    if(t.getModuleType() == EntityOwner::ModuleType::ApplicationModule) {
      // use pointer to module as unique ID
      stream() << "Module_" + std::to_string(size_t(&t)) << "[\n";
      stream() << "    fillcolor=\"#0099ff\";\n";
      stream() << "    tooltip=\"" << boost::core::demangle(typeid(t).name()) << "\";\n";
    }
    else if(t.getModuleType() == EntityOwner::ModuleType::Device) {
      const auto& tc = dynamic_cast<const DeviceModule&>(t);
      stream() << "Device_" + tc.getDeviceAliasOrURI() << "[\n";
      stream() << "    fillcolor=\"#00ff00\";\n";
      stream() << "    tooltip=\"\";\n";
      _deviceList.push_back("Device_" + tc.getDeviceAliasOrURI());
    }
    else {
      return; // ignore
    }

    // use qualified name as label, but strip leading application name for ApplicationModules (DeviceModules don't have
    // this)
    auto name = t.getQualifiedName();
    auto secondSlash = name.find_first_of('/', 1);
    if(secondSlash != std::string::npos) name = name.substr(secondSlash);
    name = name.substr(1); // strip leading slash (for both ApplicationModules and DeviceModules)
    // replace slashes with new line (<br/>)
    boost::replace_all(name, "<", ""); // remove "<" and ">" as they would confuse the HTML parser (cf. DeviceModule)
    boost::replace_all(name, ">", "");
    boost::replace_all(name, "/", "/<br/>");
    // write label
    stream() << "    label=<" << name << ">\n";
    stream() << "]\n";
  }

  /********************************************************************************************************************/

  void VariableNetworkModuleGraphDumpingVisitor::dispatch(const VariableNetwork& t) {
    std::string feedingId;
    const auto& feeder = t.getFeedingNode();
    if(feeder.getType() == NodeType::Application) {
      auto& owner = dynamic_cast<Module&>(*feeder.getOwningModule());
      try {
        feedingId = "Module_" + std::to_string(size_t(owner.findApplicationModule()));
      }
      catch(ChimeraTK::logic_error& e) {
        // ignore if findApplicationModule() throws (may happen for the application variables coming from DeviceModules)
        // TODO: handle these cases properly!
        return;
      }
    }
    else if(feeder.getType() == NodeType::Device) {
      feedingId = "Device_" + feeder.getDeviceAlias();
    }
    else {
      // ignore ControlSystem and constants etc.
      return;
    }

    for(auto& consumer : t.getConsumingNodes()) {
      std::string consumingId;
      if(consumer.getType() == NodeType::Application) {
        auto& owner = dynamic_cast<Module&>(*consumer.getOwningModule());
        try {
          consumingId = "Module_" + std::to_string(size_t(owner.findApplicationModule()));
        }
        catch(ChimeraTK::logic_error& e) {
          // ignore if findApplicationModule() throws (may happen for the application variables coming from DeviceModules)
          // TODO: handle these cases properly!
          return;
        }
      }
      else if(consumer.getType() == NodeType::Device) {
        consumingId = "Device_" + consumer.getDeviceAlias();
      }
      else {
        // ignore ControlSystem and constants etc.
        continue;
      }

      // put/extend entry in edgeMap
      if(!_edgeMap[feedingId + "->" + consumingId].first.empty()) {
        _edgeMap[feedingId + "->" + consumingId].first += ",";
      }
      else {
        _edgeMap[feedingId + "->" + consumingId].second = false;
      }
      // use variable name without path for better readability
      auto name = feeder.getName();
      size_t idx = name.find_last_of('/');
      if(idx != std::string::npos) {
        name = name.substr(idx + 1);
      }
      _edgeMap[feedingId + "->" + consumingId].first += name;
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
