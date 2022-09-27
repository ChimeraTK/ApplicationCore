// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/ConfigReader.h>

// TODO Note to optionally use to trigger
//#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>

#include "TemplateModule.h"

namespace ctk = ChimeraTK;

/**
 * Server -  An ApplicationCore template server
 *
 * The Application object for this server. It provides
 * dummy Device- and ApplicationModules and a ConfigReader.
 *
 * All modules are simply connected to the ControlSystem.
 * No triggering is implemented, as this is specific to the application
 * (either provided by a Device or a ChimeraTK::PeriodicTrigger).
 */
struct Server : public ctk::Application {
  Server(std::string appName = "TemplateServer");
  ~Server() override;
  ctk::SetDMapFilePath dmapPath;
  ctk::ConfigReader config;
  ctk::DeviceModule device;
  TemplateModule templateModule;
};
