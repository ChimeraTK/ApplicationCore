// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "HierarchyModifyingGroup.h"
#include "ScalarAccessor.h"

namespace ChimeraTK {

  class DeviceModule;

  /** Initialisation handler which calls an external application (usually a script),
   * captures its output (both stdout and stderr) and publishes it in a control system variable.
   * The variable is placed in /Devices/ALIAS_OR_URI, where also the other status information
   * for the device is located. The default name is "initScriptOutput".
   *
   * The output is published to the CS line by line, each time adding to the string. This is not
   * super efficient, but allows to monitor the script while running and see intermediate output
   * in case it gets stuck.
   *
   * The content is also printed to stdout, but only after the script has ended.
   * If the script has failed, only the output of the first run is printed to avoid
   * spaming the log file, because the DeviceModule is constantly retrying.
   *
   * After a failed run, the init handler function is sleeping for some time to lower the
   * retry-frequency. This grace period can be configures via constructor parameter.
   *
   * Usage:
   * Simply instantiate the ScriptedInitHandler after the creation of the DeviceModule, and pass
   * the command to be executed together with the DeviceModule in the constructor. The
   * ScriptedInitHandler automatically registers its "doInit()" function as initialisation handle with the
   * DeviceModule.
   */
  struct ScriptedInitHandler : ApplicationModule {
    /**
     * @brief Constructor
     * @param owner Argument for the ApplicationModule, usually "this"
     * @param name Irrelevant, will be taken from the device module
     * @param description
     * @param command The system command which is executed for device initialisation. Must return 0 on success and an
     * error code if initialisation failed.
     * @param deviceModule The device module on which the initialisation handler is registered.
     * @param outputName Name of the PV with the output string. Defaults to "initScriptOutput", but can be changed in
     * case more than one script is needed for the device.
     * @param errorGracePeriod Additional time in seconds before a retry after an error.
     */
    ScriptedInitHandler(ModuleGroup* owner, const std::string& name, const std::string& description,
        std::string command, DeviceModule& deviceModule, std::string outputName = "initScriptOutput",
        unsigned int errorGracePeriod = 10);
    void mainLoop() override {
    } // no main loop needed. doInit() is called from the DeviceModule thread as initialisation handler
    void doInit();

   protected:
    bool _lastFailed{false};
    std::string _command;
    std::string _deviceAlias;
    std::string _outputName;
    unsigned int _errorGracePeriod; // additional sleep time before a retry after an error
    //_scriptOutput must be in this file after _outputName so the latter can be used as constructor parameter
    ScalarOutput<std::string> _scriptOutput{this,
        RegisterPath("/Devices") / Utilities::escapeName(_deviceAlias, false) / _outputName, "",
        "stdout+stderr of init script"};
  };

} // namespace ChimeraTK
