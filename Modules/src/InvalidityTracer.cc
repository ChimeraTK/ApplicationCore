// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "InvalidityTracer.h"

#include "MetaDataPropagatingRegisterDecorator.h"
#include "Visitor.h"

namespace ChimeraTK {

  namespace {

    /******************************************************************************************************************/

    class InvalidityTracerVisitor : public Visitor<VariableNetworkNode, ApplicationModule> {
     public:
      explicit InvalidityTracerVisitor(ChimeraTK::Logger::StreamProxy& myLog) : _myLog(myLog) {}

      // Print invalidity information for given module. This will also call dispatch() for all inputs of the given module.
      void dispatch(const ApplicationModule& module) override;

      // Print invalidity information for given input decorated with MetaDataPropagatingRegisterDecorator. Other nodes
      // are simply ignored.
      void dispatch(const VariableNetworkNode& node) override;

     private:
      ChimeraTK::Logger::StreamProxy& _myLog;
    };

    /******************************************************************************************************************/

    void InvalidityTracerVisitor::dispatch(const ApplicationModule& module) {
      auto validity = module.getDataValidity();
      // print, if validity is faulty
      if(validity == DataValidity::faulty) {
        _myLog << "Module " << module.getQualifiedName()
               << " has DataValidity::faulty (count: " << module.getDataFaultCounter() << ")";

        auto hash = module.getCircularNetworkHash();
        if(hash != 0) {
          _myLog << " (in circular network " << hash << " with invalidity count "
                 << Application::getInstance().getCircularNetworkInvalidityCounter(hash) << ")\n";
        }

        // dispatch to all accessors of the module
        for(auto& accessor : module.getAccessorListRecursive()) {
          dispatch(accessor);
        }
      }
    }

    /******************************************************************************************************************/

    void InvalidityTracerVisitor::dispatch(const VariableNetworkNode& node) {
      // ignore non-application nodes
      if(node.getType() != NodeType::Application) {
        return;
      }

      // ignore non-inputs
      if(node.getDirection().dir != VariableDirection::consuming) {
        return;
      }

      // get accessor and cast into right type (all application accessors must have the
      // MetaDataPropagatingRegisterDecorator).
      auto accessor = boost::dynamic_pointer_cast<MetaDataPropagationFlagProvider>(
          node.getAppAccessorNoType().getHighLevelImplElement());
      if(!accessor) {
        _myLog << "InvalidityTracerVisitor: The following application node does not derive from "
                  "MetaDataPropagationFlagProvider:\n";
        node.dump(_myLog);
        return;
      }

      // check if invalid
      if(accessor->getLastValidity() == DataValidity::faulty) {
        _myLog << " -> Input " << node.getQualifiedName() << " has DataValidity::faulty\n";
      }
    }

    /******************************************************************************************************************/

  } // namespace

  /********************************************************************************************************************/

  void InvalidityTracer::mainLoop() {
    while(true) {
      // wait for user to hit the button
      printTrace.read();

      // start banner
      auto myLog = logger(Logger::Severity::info);
      InvalidityTracerVisitor visitor(myLog);
      myLog << "==== BEGIN InvalidityTracer trace output ============================================\n";

      // dispatch to all ApplicationModules
      for(auto* module : Application::getInstance().getSubmoduleListRecursive()) {
        auto* appModule = dynamic_cast<ApplicationModule*>(module);
        if(!appModule) {
          continue;
        }
        visitor.dispatch(*appModule);
      }

      // end banner
      myLog << "==== END InvalidityTracer trace output ==============================================";
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
