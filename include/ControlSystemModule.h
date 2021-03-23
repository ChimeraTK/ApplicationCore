/*
 * ControlSystemModule.h
 *
 *  Created on: Jun 28, 2016
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_CONTROL_SYSTEM_MODULE_H
#define CHIMERATK_CONTROL_SYSTEM_MODULE_H

#include <list>

#include <ChimeraTK/RegisterPath.h>

#include "Module.h"
#include "VariableNetworkNode.h"

namespace ChimeraTK {

  class ControlSystemModule : public Module {
   public:
    /** Constructor */
    ControlSystemModule();

    /** Move operation with the move constructor */
    ControlSystemModule(ControlSystemModule&& other) { operator=(std::move(other)); }

    /** Move assignment */
    ControlSystemModule& operator=(ControlSystemModule&& other) {
      Module::operator=(std::move(other));
      variableNamePrefix = std::move(other.variableNamePrefix);
      subModules = std::move(other.subModules);
      return *this;
    }

    /** The function call operator returns a VariableNetworkNode which can be used
     * in the Application::initialise() function to connect the control system
     * variable with another variable. */
    VariableNetworkNode operator()(
        const std::string& variableName, const std::type_info& valueType, size_t nElements = 0) const;
    VariableNetworkNode operator()(const std::string& variableName) const override {
      return operator()(variableName, typeid(AnyType));
    }

    void connectTo(const Module&, VariableNetworkNode = {}) const override {
      throw; /// @todo make proper exception
    }

    Module& operator[](const std::string& moduleName) const override;

    ModuleType getModuleType() const override { return ModuleType::ControlSystem; }

    const Module& virtualise() const override;

    std::list<VariableNetworkNode> getAccessorList() const override;

    std::list<Module*> getSubmoduleList() const override;

   protected:
    /** Constructor: the variableNamePrefix will be prepended to all control system variable names (separated by a
     *  slash). Applications should use the [] operator to obtain submodules instead. */
    ControlSystemModule(const std::string& variableNamePrefix);

    ChimeraTK::RegisterPath variableNamePrefix;

    // List of sub modules accessed through the operator[]. This is mutable since
    // it is little more than a cache and thus does not change the logical state
    // of this module
    mutable std::map<std::string, ControlSystemModule> subModules;

  };

} /* namespace ChimeraTK */

#endif /* CHIMERATK_CONTROL_SYSTEM_MODULE_H */
