// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "DeviceModule.h"

#include "Application.h"
#include "DeviceManager.h"
#include "ModuleGroup.h"
#include "Utilities.h"

#include <ChimeraTK/DeviceBackend.h>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  DeviceModule::DeviceModule(ModuleGroup* owner, const std::string& deviceAliasOrCDD, const std::string& triggerPath,
      std::function<void(DeviceManager*)> initialisationHandler, const std::string& pathInDevice)
  : ModuleGroup(owner, "**DeviceModule**") {
    // get/create the DeviceManager and add the initialisation handler
    auto dm = Application::getInstance().getDeviceManager(deviceAliasOrCDD);
    _dm = dm;

    if(initialisationHandler) {
      addInitialisationHandler(std::move(initialisationHandler));
    }

    // check and store the trigger path
    if(!triggerPath.empty() && triggerPath[0] != '/') {
      throw ChimeraTK::logic_error("DeviceModule triggerPath must be absolute!");
    }
    _triggerPath = triggerPath;

    // create the process variables in the model from the device registers
    // -------------------------------------------------------------------
    _model = owner->getModel().add(*this);
    auto neighbourDirectory = _model.visit(
        Model::returnDirectory, Model::getNeighbourDirectory, Model::returnFirstHit(Model::DirectoryProxy{}));
    assert(neighbourDirectory.isValid());

    // get the virtualised version of the device for the given pathInDevice
    auto nodeList = dm->getNodesList();

    // iterate all registers and add them to the model
    for(auto& node : nodeList) {
      // skip registers which are in a different directory than specified by pathInDevice
      if(!boost::starts_with(node.getRegisterName(), pathInDevice)) {
        continue;
      }

      // add trigger to feeding nodes, if necessary
      if(node.getDirection().dir == VariableDirection::feeding && node.getMode() != UpdateMode::push &&
          !node.hasExternalTrigger()) {
        if(triggerPath.empty()) {
          throw ChimeraTK::logic_error("DeviceModule '" + deviceAliasOrCDD + "': Feeding poll-typed register '" +
              node.getRegisterName() + "' needs trigger but none provided.");
        }
      }

      // obtain register name relative to pathInDevice
      assert(boost::starts_with(node.getRegisterName(), pathInDevice));
      auto cut = pathInDevice.size();
      if(!boost::ends_with(pathInDevice, "/")) {
        ++cut;
      }
      auto relativePath = node.getRegisterName().substr(cut);

      /// create subdirectory if necessary
      auto dir = neighbourDirectory.addDirectoryRecursive(Utilities::getPathName(relativePath));

      // add the PV to the directory (or get it if already existing)
      auto var = dir.addVariable(Utilities::getUnqualifiedName(relativePath));

      // connect the node and the PV
      _model.addVariable(var, node);
    }
  }

  /*********************************************************************************************************************/

  DeviceManager& DeviceModule::getDeviceManager() {
    return *_dm.lock();
  }

  /*********************************************************************************************************************/

  const DeviceManager& DeviceModule::getDeviceManager() const {
    return *_dm.lock();
  }

  /*********************************************************************************************************************/

  Model::DeviceModuleProxy DeviceModule::getModel() {
    return _model;
  }

  /*********************************************************************************************************************/

  const std::string& DeviceModule::getDeviceAliasOrURI() const {
    return _dm.lock()->getDeviceAliasOrURI();
  }

  /*********************************************************************************************************************/

  void DeviceModule::addInitialisationHandler(std::function<void(DeviceManager*)> initialisationHandler) {
    _dm.lock()->addInitialisationHandler(std::move(initialisationHandler));
  }

  /*********************************************************************************************************************/

  void DeviceModule::reportException(std::string errMsg) {
    _dm.lock()->reportException(std::move(errMsg));
  }

  /*********************************************************************************************************************/

} // namespace ChimeraTK
