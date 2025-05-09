// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/** Example to simulate the working and usage of StatusMonitor.
 *  Look at the server code, then run the server. Watch the values
 *  /Simulation/temperature and /Simulation/TemperatureMonitor/status.
 *  Try to disable the monitor with /Simulation/TemperatureMonitor/disable.
 */

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>
#include <ChimeraTK/ApplicationCore/ModuleGroup.h>
#include <ChimeraTK/ApplicationCore/StatusMonitor.h>

namespace ctk = ChimeraTK;

// Just simulate a temperature going up and down
struct SimulationModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  /**The value to be monitored.*/
  ctk::ScalarOutput<double> temperature{this, "temperature", "degC", "simulated temperature"};

  void mainLoop() override {
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
  ~ExampleApp() override { shutdown(); }

  // Create an instance of the simulation module. We name it "Simulation".
  // There will be a variable /Simulation/temperature from this.
  SimulationModule simulation{this, "Simulation", "temperature simulation"};

  // Now we place a monitor next to the temperature variable. First we create a module group, also with the name
  // "Simulation". Everything in it will be placed next to the variables from the simulation module.
  struct : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;
    // Inside the module group we place the monitor. In the constructor it gets the name of the variable to monitor, and
    // the name of the output variable. The monitor automatically connects to the input variable that is in the same
    // hierarchy level. We add output and parameter tags (STATUS and CONFIG, respectively) for easier connetion of the
    // variables.
    // ctk::RangeMonitor<double> temperatureMonitor{this, "TemperatureMonitor", "monitor for the simulated temperature",
    //    "temperature", "temperatureStatus", ctk::HierarchyModifier::none, {"STATUS"}, {"CONFIG"}, {}};

    ctk::RangeMonitor<double> temperatureMonitor{this, "/TemperatureMonitor/temperature",
        "/TemperatureMonitor/temperatureStatus", "/TemperatureMonitor", "monitor for the simulated temperature",
        ctk::TAGS{"MON_OUTPUT"}, ctk::TAGS{"MON_PARAMS"}};

  } simulationGroup{this, "Simulation", ""};

  ctk::ConfigReader config{this, "Config", "demoStatusMonitor_config.xml"};
};

ExampleApp theExampleApp;
