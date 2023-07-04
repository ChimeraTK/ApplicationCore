// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>

namespace ctk = ChimeraTK;

struct Controller : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPollInput<float> sp{this, "temperatureSetpoint", "degC", "Description"};
  ctk::ScalarPushInput<float> rb{this, "/heater/temperatureReadback", "degC", "..."};
  ctk::ScalarOutput<float> cur{this, "/heater/heatingCurrent", "mA", "..."};

  void mainLoop() final {
    const float gain = 100.0F;
    while(true) {
      readAll(); // waits until rb updated, then reads sp

      cur = gain * (sp - rb);
      writeAll(); // writes any outputs
    }
  }
};

struct Automation : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPollInput<float> opSp{this, "operatorSetpoint", "degC", "..."};
  ctk::ScalarOutput<float> actSp{this, "/Controller/temperatureSetpoint", "degC", "..."};
  ctk::ScalarPushInput<uint64_t> trigger{this, "/Timer/tick", "", "..."};

  void mainLoop() final {
    const float maxStep = 0.1F;
    while(true) {
      readAll(); // waits until trigger received, then read opSp
      actSp += std::max(std::min(opSp - actSp, maxStep), -maxStep);
      writeAll();
    }
  }
};

struct ExampleApp final : public ctk::Application {
  ExampleApp() : Application("exampleApp2a") {
    if(config.get<int>("enableAutomation")) {
      automation = Automation(this, "Automation", "Slow setpoint ramping algorithm");
    }
  }
  ~ExampleApp() final { shutdown(); }

  ctk::SetDMapFilePath dmapPath{"example2.dmap"};

  ctk::ConfigReader config{this, "config", "demoApp2a.xml"};

  Controller controller{this, "Controller", "The Controller"};

  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller", 1000};

  // ctk::DeviceModule oven{this, "oven"};
  ctk::DeviceModule oven{this, "oven", "/Timer/tick"};

  Automation automation;
};
static ExampleApp theExampleApp;
