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

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

namespace ctk = ChimeraTK;

class SetpointRamp : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPollInput<float> operatorSetpoint{this, "operatorSetpoint", "degC", "..."};

  struct ControllerInterface : ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<float> actualSetpoint{this, "temperatureSetpoint", "degC", "..."};
  };

  ControllerInterface ctrl{this, "/ControlUnit/Controller", ""};

  ctk::ScalarPushInput<uint64_t> trigger{this, "/Timer/tick", "", "..."};

  void mainLoop() override;
};
