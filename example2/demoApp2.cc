#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>

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

  // We can pick any name for the module. "Oven" is what we want to see in the CS
  Controller controller{this, "Oven", "The controller of the oven"};

  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller", 1000};

  ctk::ConnectingDeviceModule oven{this, "oven", "/Timer/tick"};
};

static ExampleApp theExampleApp;
