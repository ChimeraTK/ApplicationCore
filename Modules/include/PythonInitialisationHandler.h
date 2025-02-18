// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "ScalarAccessor.h"

namespace ChimeraTK {

  class DeviceModule;

  struct PythonInitHandler : ApplicationModule {
    PythonInitHandler(ModuleGroup* owner, const std::string& name, const std::string& description, std::string script,
        DeviceModule& deviceModule, std::string outputName = "initScriptOutput", unsigned int errorGracePeriod = 10);
    void mainLoop() override {
    } // no main loop needed. doInit() is called from the DeviceModule thread as initialisation handler
    void doInit();

   protected:
    bool _lastFailed{false};
    std::string _script;
    std::string _moduleName; // the script name without the .py
    std::string _deviceAlias;
    std::string _outputName;
    unsigned int _errorGracePeriod; // additional sleep time before a retry after an error
    //_scriptOutput must be in this file after _outputName so the latter can be used as constructor parameter
    ScalarOutput<std::string> _scriptOutput{this,
        RegisterPath("/Devices") / Utilities::escapeName(_deviceAlias, false) / _outputName, "",
        "stdout+stderr of init script"};
  };

} // namespace ChimeraTK
