// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>

namespace ctk = ChimeraTK;

struct Controller : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPollInput<float> sp{this, "temperatureSetpoint", "degC", "Description"};
  ctk::ScalarPushInput<float> rb{this, "temperatureReadback", "degC", "..."};
  ctk::ScalarOutput<float> cur{this, "heatingCurrent", "mA", "..."};

  void mainLoop() {
    const float gain = 100.0;
    while(true) {
      readAll(); // waits until rb updated, then reads sp

      cur = gain * (sp - rb);
      writeAll(); // writes any outputs
    }
  }
};

struct ExampleApp : public ctk::Application {
  ExampleApp() : Application("demoApp2") { ChimeraTK::setDMapFilePath("example2.dmap"); }
  ~ExampleApp() { shutdown(); }

  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller", 1000};

  // Look at the map file: This device provides "Heater/temperatureReadback" and "Heater/heatingCurrent".
  ctk::ConnectingDeviceModule oven{this, "oven", "/Timer/tick"};

  // Pick the name "Heater" for the controller module. Now the variable "Heater/temperatureReadback" and
  // "Heater/heatingCurrent" are automatically connected to the matching variables on the device.
  Controller controller{this, "Heater", "A controller for the heater of the oven."};
};

static ExampleApp theExampleApp;
