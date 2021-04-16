/** Example to simulate the working and usage of StatusMonitor. 
 *  Look at the server code, then run the server. Watch the values
 *  /Simulation/temperature and /Simulation/TemperatureMonitor/status.
 *  Try to disable the monitor with /Simulation/TemperatureMonitor/disable.
*/

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/StatusMonitor.h>
#include <ChimeraTK/ApplicationCore/ModuleGroup.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>

namespace ctk = ChimeraTK;

// Just simulate a temperature going up and down
struct SimulationModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  /**The value to be monitored.*/
  ctk::ScalarOutput<double> temperature{this, "temperature", "degC", "simulated temperature"};

  void mainLoop() {
    /**Intialize temperature.*/
    temperature = 0;
    temperature.write();
    double direction = 1;

    while(true) {
      // go down with the temperature if too hot
      if(temperature > 50) {
        direction = -1;
      }
      // go up with the temperature if too cold
      if(temperature < -50) {
        direction = 1;
      }

      temperature += direction * 1; // one dregree steps
      setCurrentVersionNumber({});  // We generate data without trigger or other input.
                                    // So we must update the version number manually.
                                    // This automatically updates the time stamp as well.
      temperature.write();
      usleep(100000);
    }
  }
};

struct ExampleApp : public ctk::Application {
  ExampleApp() : Application("exampleApp") {}
  ~ExampleApp() { shutdown(); }

  // Create an instance of the simulation module. We name it "Simulation".
  // There will be a variable /Simulation/temperature from this.
  SimulationModule simulation{this, "Simulation", "temperature simulation"};

  // Now we place a monitor next to the temperature variable. First we create a module group, also with the name "Simulation".
  // Everything in it will be placed next to the variables from the simulation module.
  struct : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;
    // Inside the module group we place the monitor. In the constructor it gets the name of the variable to monitor, and the name of the output variable.
    // The monitor automatically connects to the input variable that is in the same hierarchy level.
    // We add output and parameter tags (STATUS and CONFIG, respectively) for easier connetion of the variables.
    ctk::RangeMonitor<double> temperatureMonitor{this, "TemperatureMonitor", "monitor for the simulated temperature",
        "temperature", "temperatureStatus", ctk::HierarchyModifier::none, {"STATUS"}, {"CONFIG"}, {}};
  } simulationGroup{this, "Simulation", ""};

  ctk::ConfigReader config{this, "Config", "demoStatusMonitor_config.xml"};
  ctk::ControlSystemModule cs;

  void defineConnections();
};

ExampleApp theExampleApp;

void ExampleApp::defineConnections() {
  // Usually you set the dmap file here. This example does not have one.

  // Connect everything in the app to the cs. This makes the connection of temperature from Simulation to the input of the monitor because they are the same variable in the CS module.
  // Also the disable variable if the monitor is connected to the CS. Try what happens if you disable the monitor.
  findTag(".*").connectTo(cs);

  /* The trick of connecting the temperature automatically only worked because we put the temperatureMonitor into the correct place in the hierarchy
   * by putting it into the variable group "Simulation". However, the threshold parameters inside the monitor are not connected yet.
   *
   * When connecting the app, the config created the following variables:
   * /Config/TemperatureMonitor/lowerWarningThreshold
   * /Config/TemperatureMonitor/upperWarningThreshold
   * /Config/TemperatureMonitor/lowerFaultThreshold
   * /Config/TemperatureMonitor/upperFaultThreshold
   */

  // Now we connect the parameters of the temperature monitor to the control system, right into the Config directory so the variable names match.
  // Like this the parameters are connected to the values coming from the configuration.
  findTag("CONFIG").flatten().connectTo(cs["Config"]["TemperatureMonitor"]);

  // FIXME: At this point a status aggregator would connect everything with tag STATUS

  // show how it looks in the application (C++ hierarchy)
  dump();

  // show how it looks on the cs side (virtual hierarchy)
  cs.dump();

  // show how it is connected
  dumpConnections();
}
