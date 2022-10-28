// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>

namespace ChimeraTK::detail {

  /** A helper class do stop the recursion when scanning for circular dependency networks.
   *
   * When scanning, each time the whole network has to be detected. This means even if a circular depencency is
   * already detected, a module has to scan all of its inputs at least once. So the detection of the
   * circle cannot be the point where the recursion is stopped.
   *
   * The task of this class is to set an indicator the first time a module detects the circle and will then do the
   * scan of all inuts, so that following calls can end the recursion because they know the job is done.
   * This is done with setRecursionDetected().
   *
   * Each input of a module must do a complete scan to determine wheter it is part
   * of a circle or not, even if the module itself has other variables in a circle. So the flag must be
   * reset at the beginning of each scan. This is done by the static function startNewScan().
   *
   * After the call of startNewScan(), recusionDetected() returns false until setRecursionDetected() is called.
   * If recursionDetected() after construction before calling startNewScan, an exeption is thrown.
   */
  class CircularDependencyDetectionRecursionStopper {
    static size_t _globalScanCounter;
    size_t _localScanCounter{0};

   public:
    static void startNewScan();
    void setRecursionDetected();
    bool recursionDetected();
  };

} // namespace ChimeraTK::detail
