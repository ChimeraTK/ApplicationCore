// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ScriptedInitialisationHandler.h"

#include "DeviceModule.h"

#include <boost/process.hpp>

#include <functional>
#include <utility>
namespace bp = boost::process;

namespace ChimeraTK {

  /********************************************************************************************************************/

  ScriptedInitHandler::ScriptedInitHandler(ModuleGroup* owner, const std::string& name, const std::string& description,
      std::string command, DeviceModule& deviceModule, std::string outputName, std::string exitCodeName,
      unsigned int errorGracePeriod)
  : ApplicationModule(owner, name, description), _command(std::move(command)),
    _deviceAlias(deviceModule.getDeviceAliasOrURI()), _outputName(std::move(outputName)),
    _exitCodeName(std::move(exitCodeName)), _errorGracePeriod(errorGracePeriod) {
    deviceModule.addInitialisationHandler([this](Device&) { doInit(); });
  }
  /********************************************************************************************************************/

  ScriptedInitHandler::ScriptedInitHandler(ModuleGroup* owner, const std::string& name, const std::string& description,
      std::string command, DeviceModule& deviceModule, std::string outputName, unsigned int errorGracePeriod)
  : ScriptedInitHandler(owner, name, description, std::move(command), deviceModule, std::move(outputName),
        "initScriptExitCode", errorGracePeriod) {}

  /********************************************************************************************************************/

  void ScriptedInitHandler::doInit() {
    std::string output;
    _scriptOutput.setAndWrite("");

    try {
      bp::ipstream out;
      bp::child initScript(_command, (bp::std_out & bp::std_err) > out);
      std::string line;
      // Publish every line that is read from the script. It is appended to the output string
      // such that a growing message is published.
      // For debugging it is important to get the intermediate information. In case the script gets stuck
      // you want to know what has already been printed.
      while(std::getline(out, line)) {
        output += line + "\n";
        _scriptOutput.setAndWrite(output);
      }
      initScript.wait();

      _scriptExitCode.setAndWrite(initScript.exit_code());

      if(_scriptExitCode != 0) {
        output += "!!! " + _deviceAlias + " initialisation FAILED!";
        _scriptOutput.setAndWrite(output);
        if(!_lastFailed) {
          ChimeraTK::logger(Logger::Severity::error, "Device " + _deviceAlias) << output << std::endl;
        }
        _lastFailed = true;
        std::this_thread::sleep_for(std::chrono::seconds(_errorGracePeriod));
        throw ChimeraTK::runtime_error(_deviceAlias + " initialisation failed.");
      }
      output += _deviceAlias + " initialisation SUCCESS!";
      _scriptOutput.setAndWrite(output);
      ChimeraTK::logger(Logger::Severity::info, "Device " + _deviceAlias) << output << std::endl;
      _lastFailed = false;
    }
    catch(bp::process_error& e) {
      // this
      throw ChimeraTK::logic_error("Caught boost::process::process_error while executing \"" + _command +
          "\" for device " + _deviceAlias + ": " + e.what());
    }
  }

  /********************************************************************************************************************/
} // namespace ChimeraTK
