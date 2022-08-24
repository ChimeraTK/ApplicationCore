#include "ScriptedInitialisationHandler.h"

#include "DeviceModule.h"

#include <boost/process.hpp>

#include <functional>
namespace bp = boost::process;

namespace ChimeraTK {

  /**********************************************************************************************************************/

  ScriptedInitHandler::ScriptedInitHandler(EntityOwner* owner, const std::string& name, const std::string& description,
      const std::string& command, DeviceModule& deviceModule, const std::string& outputName,
      unsigned int errorGracePeriod)
  : ApplicationModule(owner, name, description), _command(command), _deviceAlias(deviceModule.getDeviceAliasOrURI()),
    _outputName(outputName), _errorGracePeriod(errorGracePeriod) {
    deviceModule.addInitialisationHandler(std::bind(&ScriptedInitHandler::doInit, this));
  }
  /**********************************************************************************************************************/

  void ScriptedInitHandler::doInit() {
    std::string output;
    _scriptOutput.value = "";
    _scriptOutput.value.write();

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
        _scriptOutput.value = output;
        _scriptOutput.value.write();
      }
      initScript.wait();

      if(initScript.exit_code() != 0) {
        output += "!!! " + _deviceAlias + " initialisation FAILED!";
        _scriptOutput.value = output;
        _scriptOutput.value.write();
        if(!_lastFailed) {
          std::cerr << output << std::endl;
        }
        _lastFailed = true;
        std::this_thread::sleep_for(std::chrono::seconds(_errorGracePeriod));
        throw ChimeraTK::runtime_error(_deviceAlias + " initialisation failed.");
      }
      else {
        output += _deviceAlias + " initialisation SUCCESS!";
        _scriptOutput.value = output;
        _scriptOutput.value.write();
        std::cerr << output << std::endl;
        _lastFailed = false;
      }
    }
    catch(bp::process_error& e) {
      // this
      throw ChimeraTK::logic_error("Caught boost::process::process_error while executing \"" + _command +
          "\" for device " + _deviceAlias + ": " + e.what());
    }
  }

  /**********************************************************************************************************************/
} // namespace ChimeraTK
