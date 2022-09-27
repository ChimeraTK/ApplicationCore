// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/DeviceManager.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>

namespace ctk = ChimeraTK;

struct Controller : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPollInput<double> sp{this, "temperatureSetpoint", "degC", "Description"};
  ctk::ScalarPushInput<double> rb{this, "temperatureReadback", "degC", "..."};
  ctk::ScalarOutput<double> cur{this, "heatingCurrent", "mA", "..."};

  void mainLoop() {
    const double gain = 100.0;
    while(true) {
      readAll(); // waits until rb updated, then reads sp

      cur = gain * (sp - rb);
      writeAll(); // writes any outputs
    }
  }
};

struct ExampleApp : public ctk::Application {
  ExampleApp() : Application("demoApp2") {
    // ChimeraTK::setDMapFilePath("example2.dmap");
    //  ovenManger.addInitialisationHandler(&initialiseOven);
  }
  ~ExampleApp() { shutdown(); }

  ctk::SetDMapFilePath dmapPath{"example2.dmap"};

  // We can pick any name for the module. "Oven" is what we want to see in the CS
  Controller controller{this, "Oven", "The controller of the oven"};

  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller", 1000};

  // ctk::DeviceManager ovenManger{this, "oven"};

  // ctk::DeviceModule oven{this, "oven", &initialiseOven}; -> replaced with ConnectingDeviceModule
  ctk::DeviceModule oven{this, "oven", "/Timer/tick", &initialiseOven};
  // ctk::ControlSystemModule cs; -> not needed anymore as all is connected to cs automatically

  // void defineConnections(); -> not needed anymore as all is connected to cs automatically
  static void initialiseOven(ChimeraTK::DeviceManager* oven);
};
static ExampleApp theExampleApp;

void ExampleApp::initialiseOven(ChimeraTK::DeviceManager* ovenManager) {
  // set the gain factors for the voltage monitoring ADCs
  // ovenManager->device.write<uint32_t>("/settings/supplyVoltageAdcGains", {20, 1, 1, 1});
  ovenManager->getDevice().write<uint32_t>("/settings/supplyVoltageAdcGains", {20, 1, 1, 1});
}

// void ExampleApp::defineConnections() {
//   ChimeraTK::setDMapFilePath("example2.dmap");

//  // Connect everything to the CS (except for the device, which is special)
//  findTag(".*").connectTo(cs);
//  // Connect device's "heater" to "Oven" in the CS, and use timer.tick as trigger
//  oven["heater"].connectTo(cs["Oven"], timer.tick);
//}
