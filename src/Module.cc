// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Module.h"

#include "Application.h"
#include "ApplicationModule.h"
#include "ConfigReader.h"
#include "DeviceModule.h"

namespace ChimeraTK {

  Module::Module(EntityOwner* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : EntityOwner(name, description, tags), _owner(owner) {
    if(_owner != nullptr) {
      _owner->registerModule(this);
    }
  }

  /*********************************************************************************************************************/

  Module::~Module() {
    if(_owner != nullptr) {
      _owner->unregisterModule(this);
    }
  }

  /*********************************************************************************************************************/

  Module& Module::operator=(Module&& other) noexcept {
    if(_owner != nullptr) {
      _owner->unregisterModule(this);
    }
    if(other._owner != nullptr) {
      other._owner->unregisterModule(&other);
    }
    _owner = other._owner;
    other._owner = nullptr;
    EntityOwner::operator=(std::move(other));
    if(_owner != nullptr) {
      _owner->registerModule(this, false);
    }
    return *this;
  }
  /*********************************************************************************************************************/

  void Module::run() {
    _testableModeReached = true; // Modules which don't implement run() have now reached testable mode
  }

  /*********************************************************************************************************************/

  ChimeraTK::ReadAnyGroup Module::readAnyGroup() {
    auto recursiveAccessorList = getAccessorListRecursive();

    // put push-type transfer elements into a ReadAnyGroup
    ChimeraTK::ReadAnyGroup group;
    for(auto& accessor : recursiveAccessorList) {
      if(accessor.getDirection() == VariableDirection{VariableDirection::feeding, false}) {
        continue;
      }
      group.add(accessor.getAppAccessorNoType());
    }

    group.finalise();
    return group;
  }

  /*********************************************************************************************************************/

  void Module::readAll(bool includeReturnChannels) {
    auto recursiveAccessorList = getAccessorListRecursive();
    // first blockingly read all push-type variables
    for(auto& accessor : recursiveAccessorList) {
      if(accessor.getMode() != UpdateMode::push) {
        continue;
      }
      if(includeReturnChannels) {
        if(accessor.getDirection() == VariableDirection{VariableDirection::feeding, false}) {
          continue;
        }
      }
      else {
        if(accessor.getDirection().dir != VariableDirection::consuming) {
          continue;
        }
      }
      accessor.getAppAccessorNoType().read();
    }
    // next non-blockingly read the latest values of all poll-type variables
    for(auto& accessor : recursiveAccessorList) {
      if(accessor.getMode() == UpdateMode::push) {
        continue;
      }
      // poll-type accessors cannot have a readback channel
      if(accessor.getDirection().dir != VariableDirection::consuming) {
        continue;
      }
      accessor.getAppAccessorNoType().readLatest();
    }
  }

  /*********************************************************************************************************************/

  void Module::readAllNonBlocking(bool includeReturnChannels) {
    auto recursiveAccessorList = getAccessorListRecursive();
    for(auto& accessor : recursiveAccessorList) {
      if(accessor.getMode() != UpdateMode::push) {
        continue;
      }
      if(includeReturnChannels) {
        if(accessor.getDirection() == VariableDirection{VariableDirection::feeding, false}) {
          continue;
        }
      }
      else {
        if(accessor.getDirection().dir != VariableDirection::consuming) {
          continue;
        }
      }
      accessor.getAppAccessorNoType().readNonBlocking();
    }
    for(auto& accessor : recursiveAccessorList) {
      if(accessor.getMode() == UpdateMode::push) {
        continue;
      }
      // poll-type accessors cannot have a readback channel
      if(accessor.getDirection().dir != VariableDirection::consuming) {
        continue;
      }
      accessor.getAppAccessorNoType().readLatest();
    }
  }

  /*********************************************************************************************************************/

  void Module::readAllLatest(bool includeReturnChannels) {
    auto recursiveAccessorList = getAccessorListRecursive();
    for(auto& accessor : recursiveAccessorList) {
      if(includeReturnChannels) {
        if(accessor.getDirection() == VariableDirection{VariableDirection::feeding, false}) {
          continue;
        }
      }
      else {
        if(accessor.getDirection().dir != VariableDirection::consuming) {
          continue;
        }
      }
      accessor.getAppAccessorNoType().readLatest();
    }
  }

  /*********************************************************************************************************************/

  void Module::writeAll(bool includeReturnChannels) {
    auto versionNumber = getCurrentVersionNumber();
    auto recursiveAccessorList = getAccessorListRecursive();
    for(auto& accessor : recursiveAccessorList) {
      if(includeReturnChannels) {
        if(accessor.getDirection() == VariableDirection{VariableDirection::consuming, false}) {
          continue;
        }
      }
      else {
        if(accessor.getDirection().dir != VariableDirection::feeding) {
          continue;
        }
      }
      accessor.getAppAccessorNoType().write(versionNumber);
    }
  }

  /*********************************************************************************************************************/

  void Module::writeAllDestructively(bool includeReturnChannels) {
    auto versionNumber = getCurrentVersionNumber();
    auto recursiveAccessorList = getAccessorListRecursive();
    for(auto& accessor : recursiveAccessorList) {
      if(includeReturnChannels) {
        if(accessor.getDirection() == VariableDirection{VariableDirection::consuming, false}) {
          continue;
        }
      }
      else {
        if(accessor.getDirection().dir != VariableDirection::feeding) {
          continue;
        }
      }
      accessor.getAppAccessorNoType().writeDestructively(versionNumber);
    }
  }

  /*********************************************************************************************************************/

  Module* Module::findApplicationModule() {
    if(getModuleType() == ModuleType::ApplicationModule) {
      auto* ret = dynamic_cast<ApplicationModule*>(this);
      assert(ret != nullptr);
      return ret;
    }
    if(getModuleType() == ModuleType::Device) {
      auto* ret = dynamic_cast<DeviceModule*>(this);
      assert(ret != nullptr);
      return ret;
    }
    if(getModuleType() == ModuleType::VariableGroup) {
      auto* owningModule = dynamic_cast<Module*>(getOwner());
      assert(owningModule != nullptr);
      return owningModule->findApplicationModule();
    }
    throw ChimeraTK::logic_error(
        "EntityOwner::findApplicationModule() called on neither an ApplicationModule nor a VariableGroup.");
  }

  /*********************************************************************************************************************/

  std::string Module::getQualifiedName() const {
    return ((_owner != nullptr) ? _owner->getQualifiedName() : "") + "/" + _name;
  }

  /*********************************************************************************************************************/

  std::string Module::getFullDescription() const {
    if(_owner == nullptr) {
      return _description;
    }
    auto ownerDescription = _owner->getFullDescription();
    if(ownerDescription.empty()) {
      return _description;
    }
    if(_description.empty()) {
      return ownerDescription;
    }
    return ownerDescription + " - " + _description;
  }

  /*********************************************************************************************************************/

  std::list<EntityOwner*> Module::getInputModulesRecursively(std::list<EntityOwner*> startList) {
    if(_owner == nullptr) {
      return {};
    }

    return _owner->getInputModulesRecursively(startList);
  }

  /*********************************************************************************************************************/

  ConfigReader& Module::appConfig() {
    size_t nConfigReaders = 0;
    ConfigReader* instance = nullptr;
    for(auto* mod : Application::getInstance().getSubmoduleListRecursive()) {
      if(!dynamic_cast<ConfigReader*>(mod)) {
        continue;
      }
      ++nConfigReaders;
      instance = dynamic_cast<ConfigReader*>(mod);
    }
    if(nConfigReaders != 1) {
      std::string message = "ApplicationModule::appConfig() called but " + std::to_string(nConfigReaders) +
          " instances of ChimeraTK::ConfigReader have been found.";
      // Printing the message as well; there is a situation when running under Boost::Test where this
      // is caught by Boost and causes a weird destructor message from AppBase.cc instead with no means of
      // finding out the actual error
      std::cerr << message << std::endl;
      throw ChimeraTK::logic_error(message);
    }
    return *instance;
  }
  /*********************************************************************************************************************/

} /* namespace ChimeraTK */
