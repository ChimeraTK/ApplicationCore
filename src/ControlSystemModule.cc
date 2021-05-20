/*
 * ControlSystemModule.cc
 *
 *  Created on: Jun 28, 2016
 *      Author: Martin Hierholzer
 */

#include "ControlSystemModule.h"
#include "Application.h"

namespace ChimeraTK {

  ControlSystemModule::ControlSystemModule() : Module(nullptr, "<ControlSystem>", "") {}

  /*********************************************************************************************************************/

  ControlSystemModule::ControlSystemModule(const std::string& _variableNamePrefix)
  : Module(nullptr,
        _variableNamePrefix.empty() ? "<ControlSystem>" :
                                      _variableNamePrefix.substr(_variableNamePrefix.find_last_of("/") + 1),
        ""),
    variableNamePrefix(_variableNamePrefix) {}

  /*********************************************************************************************************************/

  VariableNetworkNode ControlSystemModule::operator()(
      const std::string& variableName, const std::type_info& valueType, size_t nElements) const {
    assert(variableName.find_first_of("/") == std::string::npos);
    auto& variables = Application::getInstance().controlSystemVariables;
    auto fqn = variableNamePrefix / variableName;
    if(variables.count(fqn) == 0) {
      variables[fqn] = {fqn, {VariableDirection::invalid, false}, valueType, nElements};
    }
    return variables[fqn];
  }

  /*********************************************************************************************************************/

  Module& ControlSystemModule::operator[](const std::string& moduleName) const {
    assert(moduleName.find_first_of("/") == std::string::npos);
    if(subModules.count(moduleName) == 0) {
      subModules[moduleName] = {variableNamePrefix / moduleName};
    }
    return subModules[moduleName];
  }

  /*********************************************************************************************************************/

  const Module& ControlSystemModule::virtualise() const { return *this; }

  /*********************************************************************************************************************/

  std::list<VariableNetworkNode> ControlSystemModule::getAccessorList() const {
    std::list<VariableNetworkNode> list;
    auto& variables = Application::getInstance().controlSystemVariables;
    for(auto& v : variables) {
      // check if variable has the right prefix
      auto idx = v.first.rfind("/"); // position of last slasht
      if(!v.first.compare(0, idx, variableNamePrefix)) {
        // All characters until the last slash are equal to our variableNamePrefix: The variable belongs to our module.
        list.push_back(v.second);
      }
    }
    return list;
  }

  /*********************************************************************************************************************/

  std::list<Module*> ControlSystemModule::getSubmoduleList() const {
    std::list<Module*> list;
    for(auto& m : subModules) list.push_back(&m.second);
    return list;
  }

  /*********************************************************************************************************************/

  std::list<EntityOwner*> ControlSystemModule::getInputModulesRecursively(std::list<EntityOwner*> startList) {
    // The ControlSystemModule is the end of the recursion, and is not considered recursive to itself.
    // There will always be circular connections to the CS module which does not pose a problem.
    // Just return the startList without adding anything (not even the CS module itself)
    return startList;
  }

  /*********************************************************************************************************************/

} // namespace ChimeraTK
