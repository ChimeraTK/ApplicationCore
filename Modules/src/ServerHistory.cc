// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ServerHistory.h"

#include <chrono>

namespace ChimeraTK { namespace history {

  /** Callable class for use with  boost::fusion::for_each: Attach the given
   * accessor to the History with proper handling of the UserType. */
  struct AccessorAttacher {
    AccessorAttacher(
        VariableNetworkNode& feeder, ServerHistory* owner, const std::string& name, const VariableNetworkNode& trigger)
    : _feeder(feeder), _trigger(trigger), _owner(owner), _name(name) {}

    template<typename PAIR>
    void operator()(PAIR&) const {
      // only continue if the call is for the right type
      if(typeid(typename PAIR::first_type) != _feeder.getValueType()) return;

      // register connection
      if(_trigger != VariableNetworkNode() && _feeder.getMode() == UpdateMode::poll) {
        _feeder[_trigger] >> _owner->template getAccessor<typename PAIR::first_type>(_name, _feeder.pdata->nElements);
      }
      else {
        _feeder >> _owner->template getAccessor<typename PAIR::first_type>(_name, _feeder.pdata->nElements);
      }
    }

    VariableNetworkNode& _feeder;
    VariableNetworkNode _trigger;
    ServerHistory* _owner;
    const std::string& _name;
  };

  ServerHistory::ServerHistory(EntityOwner* owner, const std::string& name, const std::string& description,
      size_t historyLength, bool enableTimeStamps, HierarchyModifier hierarchyModifier,
      const std::unordered_set<std::string>& tags)
  : ApplicationModule(owner, name, description, hierarchyModifier, tags), _historyLength(historyLength),
    _enbaleTimeStamps(enableTimeStamps) {
    auto virtualLogging = getOwner()->findTag("history");
    auto list = virtualLogging.getAccessorListRecursive();
    size_t accessors = 0;
    for(auto it = list.begin(); it != list.end(); ++it) {
      // do not add the module itself
      if(it->getOwningModule() == this) continue;
      try {
        // virtualLogging.getQualifiedName() returns the name of the app, e.g. /test and we remove that from the module
        // name , e.g. /test/MyModule
        auto namePrefix =
            it->getOwningModule()->getQualifiedName().substr(virtualLogging.getQualifiedName().length() + 1);
        prepareHierarchy(namePrefix);
        boost::fusion::for_each(_accessorListMap.table, AccessorAttacher(*it, this, namePrefix / it->getName(), {}));
        accessors++;
      }
      catch(ChimeraTK::logic_error& e) {
        std::cerr << "Failed to add history variable: " << it->getQualifiedName() << " Error: " << e.what()
                  << std::endl;
      }
    }
    if(accessors == 0) {
      throw logic_error("No accessors for ServerHistory found. Did you use the tag 'history' for any variable?");
    }
    else {
      std::cout << "Added " << accessors << " accessors to the ServerHistory Module." << std::endl;
    }
  }

  void ServerHistory::prepareHierarchy(const RegisterPath& namePrefix) {
    // create variable group map for namePrefix if needed
    if(groupMap.find(namePrefix) == groupMap.end()) {
      // search for existing parent (if any)
      auto parentPrefix = namePrefix;
      while(groupMap.find(parentPrefix) == groupMap.end()) {
        if(parentPrefix == "/") break; // no existing parent found
        parentPrefix = std::string(parentPrefix).substr(0, std::string(parentPrefix).find_last_of("/"));
      }
      // create all not-yet-existing parents
      while(parentPrefix != namePrefix) {
        EntityOwner* owner = this;
        if(parentPrefix != "/") owner = &groupMap[parentPrefix];
        auto stop = std::string(namePrefix).find_first_of("/", parentPrefix.length() + 1);
        if(stop == std::string::npos) stop = namePrefix.length();
        RegisterPath name = std::string(namePrefix).substr(parentPrefix.length(), stop - parentPrefix.length());
        parentPrefix /= name;
        groupMap[parentPrefix] = VariableGroup(owner, std::string(name).substr(1), "");
      }
    }
  }

  void ServerHistory::addSource(
      const Module& source, const RegisterPath& namePrefix, const VariableNetworkNode& trigger) {
    // for simplification, first create a VirtualModule containing the correct
    // hierarchy structure (obeying eliminate hierarchy etc.)
    auto dynamicModel = source.findTag(".*"); /// @todo use virtualise() instead

    prepareHierarchy(namePrefix);

    // add all accessors on this hierarchy level
    for(auto& acc : dynamicModel.getAccessorList()) {
      boost::fusion::for_each(_accessorListMap.table, AccessorAttacher(acc, this, namePrefix / acc.getName(), trigger));
    }

    // recurse into submodules
    for(auto mod : dynamicModel.getSubmoduleList()) {
      addSource(*mod, namePrefix / mod->getName(), trigger);
    }
  }

  void ServerHistory::addSource(const DeviceModule& source, const RegisterPath& namePrefix,
      const std::string& submodule, const VariableNetworkNode& trigger) {
    auto mod = source.virtualiseFromCatalog();
    if(submodule.empty())
      addSource(mod, namePrefix, trigger);
    else
      addSource(mod.submodule(submodule), namePrefix, trigger);
  }

  void ServerHistory::addSource(ConnectingDeviceModule* source, const RegisterPath& namePrefix,
      const std::string& submodule, const VariableNetworkNode& trigger) {
    addSource(source->getDeviceModule(), namePrefix, submodule, trigger);
  }

  template<typename UserType>
  VariableNetworkNode ServerHistory::getAccessor(const std::string& variableName, const size_t& nElements) {
    // check if variable name already registered
    for(auto& name : _overallVariableList) {
      if(name == variableName) {
        throw ChimeraTK::logic_error("Cannot add '" + variableName +
            "' to History since a variable with that "
            "name is already registered.");
      }
    }
    _overallVariableList.push_back(variableName);

    // add accessor and name to lists
    auto& tmpList = boost::fusion::at_key<UserType>(_accessorListMap.table);
    auto& nameList = boost::fusion::at_key<UserType>(_nameListMap.table);
    auto dirName = variableName.substr(0, variableName.find_last_of("/"));
    auto baseName = variableName.substr(variableName.find_last_of("/") + 1);
    tmpList.emplace_back(std::piecewise_construct,
        std::forward_as_tuple(
            ArrayPushInput<UserType>{&groupMap[dirName], baseName + "_in", "", 0, "", {"_history_internal"}}),
        std::forward_as_tuple(HistoryEntry<UserType>{_enbaleTimeStamps}));
    for(size_t i = 0; i < nElements; i++) {
      if(nElements == 1) {
        // in case of a scalar history only use the variableName
        tmpList.back().second.data.emplace_back(
            ArrayOutput<UserType>{&groupMap[dirName], baseName, "", _historyLength, "", {getName()}});
        if(_enbaleTimeStamps) {
          tmpList.back().second.timeStamp.emplace_back(
              ArrayOutput<uint64_t>{&groupMap[dirName], baseName + "_timeStamps",
                  "Time stamps for entries in the history buffer", _historyLength, "", {getName()}});
        }
      }
      else {
        // in case of an array history append the index to the variableName
        tmpList.back().second.data.emplace_back(ArrayOutput<UserType>{
            &groupMap[dirName], baseName + "_" + std::to_string(i), "", _historyLength, "", {getName()}});
        if(_enbaleTimeStamps) {
          tmpList.back().second.timeStamp.emplace_back(
              ArrayOutput<uint64_t>{&groupMap[dirName], baseName + "_" + std::to_string(i) + "_timeStamps",
                  "Time stamps for entries in the history buffer", _historyLength, "", {getName()}});
        }
      }
    }
    nameList.push_back(variableName);

    // return the accessor
    return tmpList.back().first;
  }

  struct Update {
    Update(ChimeraTK::TransferElementID id) : _id(id) {}

    template<typename PAIR>
    void operator()(PAIR& pair) const {
      auto& accessorList = pair.second;
      for(auto accessor = accessorList.begin(); accessor != accessorList.end(); ++accessor) {
        if(accessor->first.getId() == _id) {
          for(size_t i = 0; i < accessor->first.getNElements(); i++) {
            std::rotate(accessor->second.data.at(i).begin(), accessor->second.data.at(i).begin() + 1,
                accessor->second.data.at(i).end());
            *(accessor->second.data.at(i).end() - 1) = accessor->first[i];
            accessor->second.data.at(i).write();
            if(accessor->second.withTimeStamps) {
              std::rotate(accessor->second.timeStamp.at(i).begin(), accessor->second.timeStamp.at(i).begin() + 1,
                  accessor->second.timeStamp.at(i).end());
              *(accessor->second.timeStamp.at(i).end() - 1) =
                  std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                      .count();
              accessor->second.timeStamp.at(i).write();
            }
          }
        }
      }
    }

    TransferElementID _id;
  };

  void ServerHistory::prepare() {
    incrementDataFaultCounter(); // the written data is flagged as faulty
    writeAll();                  // send out initial values of all outputs.
    decrementDataFaultCounter(); // when entering the main loop calculate the validiy from the inputs. No artificial increase.
  }

  void ServerHistory::mainLoop() {
    auto group = readAnyGroup();
    while(true) {
      auto id = group.readAny();
      boost::fusion::for_each(_accessorListMap.table, Update(id));
    }
  }

  void ServerHistory::findTagAndAppendToModule(VirtualModule& virtualParent, const std::string& tag,
      bool eliminateAllHierarchies, bool eliminateFirstHierarchy, bool negate, VirtualModule& root) const {
    // Change behaviour to exclude the auto-generated inputs which are connected to the data sources. Otherwise those
    // variables might get published twice to the control system, if findTag(".*") is used to connect the entire
    // application to the control system.
    // This is a temporary solution. In future, instead the inputs should be generated at the same place in the
    // hierarchy as the source variable, and the connetion should not be made by the module itself. This currently would
    // be complicated to implement, since it is difficult to find the correct virtual name for the variables.

    struct MyVirtualModule : VirtualModule {
      using VirtualModule::VirtualModule;
      using VirtualModule::findTagAndAppendToModule;
    };

    MyVirtualModule tempParent("tempRoot", "", ModuleType::ApplicationModule);
    MyVirtualModule tempRoot("tempRoot", "", ModuleType::ApplicationModule);
    EntityOwner::findTagAndAppendToModule(
        tempParent, "_history_internal", eliminateAllHierarchies, eliminateFirstHierarchy, true, tempRoot);
    tempParent.findTagAndAppendToModule(virtualParent, tag, false, true, negate, root);
    tempRoot.findTagAndAppendToModule(root, tag, false, true, negate, root);
  }

}} // namespace ChimeraTK::history
