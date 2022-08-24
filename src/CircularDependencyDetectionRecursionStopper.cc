#include "CircularDependencyDetectionRecursionStopper.h"

#include <ChimeraTK/Exception.h>

namespace ChimeraTK { namespace detail {

  size_t CircularDependencyDetectionRecursionStopper::_globalScanCounter{0};

  void CircularDependencyDetectionRecursionStopper::startNewScan() {
    ++_globalScanCounter;
  }
  void CircularDependencyDetectionRecursionStopper::setRecursionDetected() {
    _localScanCounter = _globalScanCounter;
  }
  bool CircularDependencyDetectionRecursionStopper::recursionDetected() {
    if(_globalScanCounter == 0) {
      throw ChimeraTK::logic_error(
          "CircularDependencyDetectionRecursionStopper::recursionDetected() called without starting a scan.");
    }
    return _localScanCounter == _globalScanCounter;
  }

}} // namespace ChimeraTK::detail
