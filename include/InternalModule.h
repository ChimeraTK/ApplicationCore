// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "EntityOwner.h"

#include <ChimeraTK/ControlSystemAdapter/ProcessArray.h>

#include <thread>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** Base class for internal modules which are created by the variable connection code (e.g.
   *  Application::makeConnections()). These modules have to be handled  differently since the instance is created
   *  dynamically and thus we cannot store the plain pointer in Application::overallModuleList.
   *
   *  @todo Currently this class is based on EntityOwner somewhat artificially. Instead the InternalModule class needs
   *  to be properly unified with the normal Module classes. */
  class InternalModule : public EntityOwner {
   public:
    ~InternalModule() override = default;

    /** Activate synchronisation thread if needed
     *  @todo: Unify with Module::run() */
    virtual void activate() {}

    /** Deactivate synchronisation thread if running
     *  @todo: Unify with Module::terminate() */
    virtual void deactivate() {}

    /** Below all pure virtual functions of EntityOwner are "implemented" just to make the program compile for now.
     *  They are currently not used. */
    std::string getQualifiedName() const override { throw; }
    std::string getFullDescription() const override { throw; }
    ModuleType getModuleType() const override { throw; }
    VersionNumber getCurrentVersionNumber() const override { throw; }
    void setCurrentVersionNumber(VersionNumber /*versionNumber*/) override { throw; }
    DataValidity getDataValidity() const override { throw; }
    void incrementDataFaultCounter() override { throw; }
    void decrementDataFaultCounter() override { throw; }
    std::list<EntityOwner*> getInputModulesRecursively([[maybe_unused]] std::list<EntityOwner*> startList) override;
    size_t getCircularNetworkHash() const override;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  inline std::list<EntityOwner*> InternalModule::getInputModulesRecursively(
      [[maybe_unused]] std::list<EntityOwner*> startList) {
    throw ChimeraTK::logic_error("getInputModulesRecursively() called on an InternalModule (ThreadedFanout or "
                                 "TriggerFanout). This is probably "
                                 "caused by incorrect ownership of variables/accessors or VariableGroups.");
  }

  /********************************************************************************************************************/

  inline size_t InternalModule::getCircularNetworkHash() const {
    throw ChimeraTK::logic_error("getCircularNetworkHash() called on an InternalModule (ThreadedFanout or "
                                 "TriggerFanout). This is probably "
                                 "caused by incorrect ownership of variables/accessors or VariableGroups.");
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
