// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/TransferElement.h>

#include <utility>

namespace ChimeraTK {

  /** A Helper struct to store an accessor and a version number.
   *  Like this you can set the user buffer of the accessors and the version number which shall be used
   *  when the accessor is written, but delay the writing do a later point in time.
   */
  struct RecoveryHelper {
    enum class Direction { fromDevice, toDevice };
    boost::shared_ptr<TransferElement> accessor;
    VersionNumber versionNumber;
    uint64_t writeOrder;
    bool wasWritten{false};
    cppext::future_queue<void> notificationQueue;
    Direction recoveryDirection{Direction::toDevice};

    explicit RecoveryHelper(boost::shared_ptr<TransferElement> a, VersionNumber v = VersionNumber(nullptr),
        uint64_t order = 0, Direction direction = Direction::toDevice)
    : accessor(std::move(a)), versionNumber(v), writeOrder(order), recoveryDirection(direction) {}
  };

} // end of namespace ChimeraTK
