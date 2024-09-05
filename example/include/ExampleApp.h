// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * This example is explained as part of the \ref conceptualOverview. Please refere there for step-by-step explanations.
 * Reading the full example might not be a good starting point for learning ApplicationCore as it can be overwelming
 * and lacks important background information.
 *
 * Please ignore all comments of the format "//! [some name]", those are used for Doxygen to include code snippets in
 * the documentation pages.
 */

#include "AverageCurrent.h"
#include "Controller.h"
#include "SetpointRamp.h"

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>
#include <ChimeraTK/ApplicationCore/ScriptedInitialisationHandler.h>
#include <ChimeraTK/ApplicationCore/VersionInfoProvider.h>

namespace ctk = ChimeraTK;

//! [Snippet: Class Definition Start]
class ExampleApp : public ctk::Application {
 public:
  using ctk::Application::Application;
  ~ExampleApp() override;

 private:
  //! [Snippet: Class Definition Start]
  // Set the name of the DMAP file to define the devices. Must be done before instantiating any DeviceModule.
  // Using the application name as a base helps for automated testing against different config files.
  //! [Snippet: SetDMapFilePath]
  ctk::SetDMapFilePath dmapPath{getName() + ".dmap"};
  //! [Snippet: SetDMapFilePath]

  // Provide version information from the `CMakeLists.txt` as process variables
  // Apart from the line below and the inclusion of the
  // `#include <ChimeraTK/ApplicationCore/VersionInfoProvider.h>` line,
  // The server is also expected to have a module named "Application"
  // with a variable named "configPatchVersion" of type "int32" in its "config.xml" file.
  ctk::VersionInfoProvider vip{this};

  // Periodic trigger used to readout data from the device periodically.
  //! [Snippet: PeriodicTrigger Instance]
  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller"};
  //! [Snippet: PeriodicTrigger Instance]

  // Publish the content of the device "oven" defined in the DMAP file to the control system and to the application
  // modules. The "tick" output of the PeriodicTimer "Timer" defined above is used as a readout trigger (for all
  // poll-typed device registers).
  //! [Snippet: Device]
  ctk::DeviceModule oven{this, "oven", "/Timer/tick"};
  //! [Snippet: Device]

  // Initialisation handler: execute Python script to initialise oven device
  //! [Snippet: ScriptedInitHandler]
  ctk::ScriptedInitHandler ovenInit{this, "ovenInit", "Initialisation of oven device", "./ovenInit.py", oven};
  //! [Snippet: ScriptedInitHandler]

  //! [Snippet: ControlUnit ModuleGroup]
  struct ControlUnit : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;

    //! [Snippet: Controller Instance]
    // Instantiate the temperature controller module
    Controller controller{this, "Controller", "The temperature controller"};
    //! [Snippet: Controller Instance]

    //! [Snippet: AverageCurrent Instance]
    // Instantiate the heater current averaging module
    AverageCurrent averageCurrent{this, "AverageCurrent", "Provide averaged heater current"};
    //! [Snippet: AverageCurrent Instance]
  };
  ControlUnit controlUnit{this, "ControlUnit", "Unit for controlling the oven temperature"};
  //! [Snippet: ControlUnit ModuleGroup]

  // Optionally instantiate the automated setpoint ramping module
  SetpointRamp ramp{getConfigReader().get<ChimeraTK::Boolean>("Configuration/enableSetpointRamping") ?
          SetpointRamp(this, "SetpointRamp", "Slow ramping of temperator setpoint") :
          SetpointRamp()};
  //! [Snippet: Class Definition End]
};
//! [Snippet: Class Definition End]
