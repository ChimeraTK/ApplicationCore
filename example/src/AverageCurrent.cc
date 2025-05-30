// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AverageCurrent.h"

/*
 * This example is explained as part of the \ref conceptualOverview. Please refere there for step-by-step explanations.
 * Reading the full example might not be a good starting point for learning ApplicationCore as it can be overwelming
 * and lacks important background information.
 *
 * Please ignore all comments of the format "//! [some name]", those are used for Doxygen to include code snippets in
 * the documentation pages.
 */

/**********************************************************************************************************************/

//! [Snippet: mainLoop implementation]
void AverageCurrent::mainLoop() {
  const float coeff = 0.1;
  currentAveraged.setAndWrite(current); // initialise currentAveraged with initial value

  while(true) {
    current.read();

    // Often, it can be considered a good practise to only write values if they have actually changed. This will
    // prevent subsequent computations from running unneccessarily. On the other hand, it may prevent receivers from
    // getting a consistent "snapshot" for each trigger. This has to be decided case by case.
    currentAveraged.writeIfDifferent((1 - coeff) * currentAveraged + coeff * current);
  }
}
//! [Snippet: mainLoop implementation]

/**********************************************************************************************************************/
