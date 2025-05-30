// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * This example is explained as part of the \ref conceptualOverview. Please refere there for step-by-step explanations.
 * Reading the full example might not be a good starting point for learning ApplicationCore as it can be overwelming
 * and lacks important background information.
 *
 * Please ignore all comments of the format "//! [some name]", those are used for Doxygen to include code snippets in
 * the documentation pages.
 */

#include "ExampleApp.h"

#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>

/**********************************************************************************************************************/

static ctk::ApplicationFactory<ExampleApp> appFactory("demo_example");

/**********************************************************************************************************************/
