// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

namespace ctk = ChimeraTK;

struct TemplateModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  /*
   * Add input/output definition
   * */
  void mainLoop() override;
};
