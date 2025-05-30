// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ModuleGroup.h"

#include "ApplicationModule.h"
#include "DeviceModule.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : Module(owner, name, description, tags) {
    if(!owner) {
      throw ChimeraTK::logic_error("ModuleGroup owner cannot be nullptr");
    }

    if(owner->getModel().isValid()) {
      _model = owner->getModel().add(*this);
    }
  }

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(ModuleGroup* owner, const std::string& name) : Module(owner, name, "") {}

  /********************************************************************************************************************/

  ModuleGroup& ModuleGroup::operator=(ModuleGroup&& other) noexcept {
    _model = std::move(other._model);
    other._model = {};
    if(_model.isValid()) {
      _model.informMove(*this);
    }
    Module::operator=(std::move(other));
    return *this;
  }

  /********************************************************************************************************************/

  std::string ModuleGroup::getVirtualQualifiedName() const {
    return _model.getFullyQualifiedPath();
  }

  /********************************************************************************************************************/

  void ModuleGroup::unregisterModule(Module* module) {
    EntityOwner::unregisterModule(module);

    // unregister from model
    if(_model.isValid()) {
      auto* mg = dynamic_cast<ModuleGroup*>(module);
      if(mg) {
        _model.remove(*mg);
        return;
      }
      auto* am = dynamic_cast<ApplicationModule*>(module);
      if(am) {
        _model.remove(*am);
        return;
      }
      auto* dm = dynamic_cast<DeviceModule*>(module);
      if(dm) {
        _model.remove(*dm);
        return;
      }

      // ModuleGroups own either other ModuleGroups or ApplicationModules, but during destruction unregisterModule
      // is called from the base class destructor where the dynamic_cast already fails.
      return;
    }
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
