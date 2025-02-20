// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PythonInitialisationHandler.h"

#include "DeviceModule.h"
#include <pybind11/embed.h>
#include <pybind11/gil.h>

#include <ChimeraTK/Exception.h>
namespace py = pybind11;
using namespace py::literals;

// #include <functional>
// #include <utility>

namespace ChimeraTK {

  /**********************************************************************************************************************/

  PythonInitHandler::PythonInitHandler(ModuleGroup* owner, const std::string& name, const std::string& description,
      std::string script, DeviceModule& deviceModule, std::string outputName, unsigned int errorGracePeriod)
  : ApplicationModule(owner, name, description), _script(std::move(script)),
    _deviceAlias(deviceModule.getDeviceAliasOrURI()), _outputName(std::move(outputName)),
    _errorGracePeriod(errorGracePeriod) {
    Application::getInstance().getPythonModuleManager().init();

    if(_script.size() < 4) {
      throw ChimeraTK::logic_error(
          R"(PythonInitHandler: File name ")" + _script + R"(" is too short! It has to end on ".py".)");
    }
    if(_script.substr(_script.size() - 3) != ".py") {
      throw ChimeraTK::logic_error(R"(PythonInitHandler: File name ")" + _script + R"(" does not end on ".py"!)");
    }
    _moduleName = _script.substr(0, _script.size() - 3);

    deviceModule.addInitialisationHandler([this](Device&) { doInit(); });
  }
  /**********************************************************************************************************************/

  void PythonInitHandler::doInit() {
    _scriptOutput = "";
    _scriptOutput.write();

    pybind11::gil_scoped_acquire gil;

    auto locals = py::dict("loggername"_a = _deviceAlias);
    int exitCode{};
    std::string output;
    try {
      locals["script"] = py::module_::import(_moduleName.c_str());

      py::exec(R"(
        import logging
        import io

        # Get an individual logger for the device alias (aka. loggername)
        l = logging.getLogger(loggername)
        l.setLevel(logging.INFO)

        # Store the a StringIO instance to be used by the logger. It is available in the local
        # variables after the script is done.
        init_script_log = io.StringIO()
        l.addHandler(logging.StreamHandler(init_script_log))

        exit_code = script.initDevice(l)
      )",
          py::globals(), locals);

      py::print(locals);
      auto logHandler = locals["init_script_log"];
      output = logHandler.attr("getvalue")().cast<std::string>();
      exitCode = locals["exit_code"].cast<int>();
      std::cout << "DEBUG: PyInitHandler exit code: " << exitCode << std::endl;
    }
    catch(std::exception& e) {
      throw ChimeraTK::logic_error(
          "Caught exception while executing \"" + _script + "\" for device " + _deviceAlias + ": " + e.what());
    }

    if(exitCode != 0) {
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

  /**********************************************************************************************************************/
} // namespace ChimeraTK
