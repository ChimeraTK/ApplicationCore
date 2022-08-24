// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/**
 *  InvalidityTracer application module.
 *
 *  This module can be instantiated in any application for the purpose of debugging unexpected stats of
 *  DataValidity::faulty.
 */
#include "ApplicationCore.h"

namespace ChimeraTK {

  struct InvalidityTracer : ApplicationModule {
    using ApplicationModule::ApplicationModule;
    ScalarPushInput<int> printTrace{this, "printTrace", "", "Write to this variable to print the trace to stdout"};

    void mainLoop() override;
  };

} // namespace ChimeraTK
