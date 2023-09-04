// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "DeviceModule.h"

#include "Application.h"
#include "DeviceManager.h"
#include "ModuleGroup.h"
#include "Utilities.h"

#include <ChimeraTK/DeviceBackend.h>
#include <ChimeraTK/Utilities.h>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  DeviceModule::DeviceModule(ModuleGroup* owner, const std::string& deviceAliasOrCDD, const std::string& triggerPath,
      std::function<void(ChimeraTK::Device&)> initialisationHandler, const std::string& pathInDevice)
  : ModuleGroup(owner, "**DeviceModule**") {
    // get/create the DeviceManager and add the initialisation handler
    auto dm = Application::getInstance().getDeviceManager(deviceAliasOrCDD);
    _dm = dm;

    _pathInDevice = pathInDevice;

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
      if(!boost::starts_with(node.getRegisterName(), _pathInDevice)) {
        continue;
      }

      // obtain register name relative to pathInDevice
      assert(boost::starts_with(node.getRegisterName(), _pathInDevice));
      auto cut = pathInDevice.size();
      if(!boost::ends_with(pathInDevice, "/")) {
        ++cut;
      }
      auto relativePath = node.getRegisterName().substr(cut);

      /// create subdirectory if necessary
      auto dir = neighbourDirectory.addDirectoryRecursive(Utilities::getPathName(relativePath));

      // add the PV to the directory (or get it if already existing)
      auto var = dir.addVariable(Utilities::getUnqualifiedName(relativePath));
      node.setOwningModule(this);

      // connect the node and the PV
      _model.addVariable(var, node);
    }
  }

  /*********************************************************************************************************************/

  DeviceModule& DeviceModule::operator=(DeviceModule&& other) noexcept {
    // First clean up the model before moving the module itself.
    if(other._model.isValid()) {
      Model::DirectoryProxy neighbourDirectory;
      try {
        neighbourDirectory = other._model.visit(
            Model::returnDirectory, Model::getNeighbourDirectory, Model::returnFirstHit(Model::DirectoryProxy{}));
        assert(neighbourDirectory.isValid());

        auto dm = _dm.lock();
        if(dm) {
          for(const auto& reg : dm->getDevice().getRegisterCatalogue()) {
            if(reg.getNumberOfDimensions() > 1) {
              continue;
            }
            std::string registerName = reg.getRegisterName();
            if(!boost::starts_with(registerName, _pathInDevice)) {
              continue;
            }

            // find the right PV in the model
            neighbourDirectory.visitByPath(registerName, [&](auto proxy) {
              if constexpr(isVariable(proxy)) {
                assert(proxy.isValid());

                // find the right node (belonging to our device) to remove
                for(auto& proxyNode : proxy.getNodes()) {
                  if(proxyNode.getType() == NodeType::Device && proxyNode.getDeviceAlias() == getDeviceAliasOrURI()) {
                    proxy.removeNode(proxyNode);
                    break;
                  }
                }
              }
            });
          }
        }
      }
      catch(ChimeraTK::logic_error& e) {
        std::cerr << e.what() << std::endl;
        std::exit(1);
      }
    }

    _model = std::move(other._model);
    other._model = {};
    if(_model.isValid()) {
      _model.informMove(*this);
    }
    ModuleGroup::operator=(std::move(other));
    return *this;
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

  void DeviceModule::addInitialisationHandler(std::function<void(ChimeraTK::Device&)> initialisationHandler) {
    _dm.lock()->addInitialisationHandler(std::move(initialisationHandler));
  }

  /*********************************************************************************************************************/

  void DeviceModule::reportException(std::string errMsg) {
    _dm.lock()->reportException(std::move(errMsg));
  }

  /*********************************************************************************************************************/

  SetDMapFilePath::SetDMapFilePath(const std::string& dmapFilePath) {
    ChimeraTK::setDMapFilePath(dmapFilePath);
  }

  /*********************************************************************************************************************/

} // namespace ChimeraTK
