#include "CircularDependencyRecursionBreaker.h"
#include <ChimeraTK/Exception.h>

namespace ChimeraTK { namespace detail {

  size_t CircularDependencyRecursionBreaker::_globalScanCounter{0};

  void CircularDependencyRecursionBreaker::startNewScan() { ++_globalScanCounter; }
  void CircularDependencyRecursionBreaker::setRecursionDetected() { _localScanCounter = _globalScanCounter; }
  bool CircularDependencyRecursionBreaker::recursionDetected() {
    if(_globalScanCounter == 0) {
      throw ChimeraTK::logic_error(
          "CircularDependencyRecursionBreaker::recursionDetected() called without starting a scan.");
    }
    return _localScanCounter == _globalScanCounter;
  }

}} // namespace ChimeraTK::detail
