#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>

namespace ctk = ChimeraTK;

struct ExampleApp : public ctk::Application {
  ExampleApp() : Application("exampleApp3") {}
  ~ExampleApp() { shutdown(); }

  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller", 1000};

  ctk::DeviceModule dev{this, "oven"};
  ctk::ControlSystemModule cs;

  void defineConnections();
};
static ExampleApp theExampleApp;

void ExampleApp::defineConnections() {
  ChimeraTK::setDMapFilePath("example2.dmap");
  dev.connectTo(cs, timer.tick);
}
