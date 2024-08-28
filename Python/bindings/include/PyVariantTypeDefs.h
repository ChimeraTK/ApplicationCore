// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/SupportedUserTypes.h>

#include <variant>

namespace ChimeraTK {

  // This should maybe go to DeviceAccess SupportedUserTypes.h or so (although this would force <variant> to
  // our entire code base, which might slow down compilation!?)
  // NOTE: The strictest type must come first, as pybind11 will pick the first matching type in order when converting
  // from a Python type into the variant.
  template<template<typename> class TPL>
  using userTypeTemplateVariant =
      std::variant<TPL<ChimeraTK::Boolean>, TPL<int8_t>, TPL<uint8_t>, TPL<int16_t>, TPL<uint16_t>, TPL<int32_t>,
          TPL<uint32_t>, TPL<int64_t>, TPL<uint64_t>, TPL<float>, TPL<double>, TPL<std::string>, TPL<ChimeraTK::Void>>;

  using userTypeVariant = std::variant<ChimeraTK::Boolean, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
      int64_t, uint64_t, float, double, std::string, ChimeraTK::Void>;

  template<template<typename> class TPL>
  using userTypeTemplateVariantNoVoid =
      std::variant<TPL<ChimeraTK::Boolean>, TPL<int8_t>, TPL<uint8_t>, TPL<int16_t>, TPL<uint16_t>, TPL<int32_t>,
          TPL<uint32_t>, TPL<int64_t>, TPL<uint64_t>, TPL<float>, TPL<double>, TPL<std::string>>;

  using userTypeVariantNoVoid = std::variant<ChimeraTK::Boolean, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
      int64_t, uint64_t, float, double, std::string>;

} // namespace ChimeraTK