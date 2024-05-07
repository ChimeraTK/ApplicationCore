// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ScriptedInitialisationHandler.h"

#include "DeviceModule.h"

#include <boost/process.hpp>

#include <functional>
#include <utility>
namespace bp = boost::process;

namespace ChimeraTK {

  /**********************************************************************************************************************/

  ScriptedInitHandler::ScriptedInitHandler(ModuleGroup* owner, const std::string& name, const std::string& description,
      std::string command, DeviceModule& deviceModule, std::string outputName, unsigned int errorGracePeriod)
  : ApplicationModule(owner, name, description), _command(std::move(command)),
    _deviceAlias(deviceModule.getDeviceAliasOrURI()), _outputName(std::move(outputName)),
    _errorGracePeriod(errorGracePeriod) {
    deviceModule.addInitialisationHandler([this](Device&) { doInit(); });
  }
  /**********************************************************************************************************************/

  void ScriptedInitHandler::doInit() {
    std::string output;
    _scriptOutput = "";
    _scriptOutput.write();

    try {
      bp::ipstream out;
      bp::child initScript(_command, (bp::std_out & bp::std_err) > out);
      std::string line;
      // Publish every line that is read from the script. It is appended to the output string
      // such that a growing message is published.
      // For debugging it is important to get the intermediate information. In case the script gets stuck
      // you want to know what has already been printed.
      while(initScript.running() && std::getline(out, line)) {
        output += line + "\n";
        _scriptOutput = output;
        _scriptOutput.write();
      }
      initScript.wait();

      if(initScript.exit_code() != 0) {
        output += "!!! " + _deviceAlias + " initialisation FAILED!";
        _scriptOutput = output;
        _scriptOutput.write();
        if(!_lastFailed) {
          ChimeraTK::logger(Logger::Severity::error, "Device " + _deviceAlias) << output << std::endl;
        }
        _lastFailed = true;
        std::this_thread::sleep_for(std::chrono::seconds(_errorGracePeriod));
        throw ChimeraTK::runtime_error(_deviceAlias + " initialisation failed.");
      }
      output += _deviceAlias + " initialisation SUCCESS!";
      _scriptOutput = output;
      _scriptOutput.write();
      ChimeraTK::logger(Logger::Severity::info, "Device " + _deviceAlias) << output << std::endl;
      _lastFailed = false;
    }
    catch(bp::process_error& e) {
      // this
      throw ChimeraTK::logic_error("Caught boost::process::process_error while executing \"" + _command +
          "\" for device " + _deviceAlias + ": " + e.what());
    }
  }

  /**********************************************************************************************************************/
} // namespace ChimeraTK
