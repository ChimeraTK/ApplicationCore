// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/SupportedUserTypes.h>

namespace ChimeraTK {

  template<typename UserType>
  class ScalarAccessor;

  template<typename UserType>
  class ScalarPushInput;

  template<typename UserType>
  class ScalarPushInputWB;

  template<typename UserType>
  class ArrayAccessor;

  template<typename UserType>
  class ArrayPushInput;

  template<typename UserType>
  class ArrayPushInputWB;

  /**
   * Concept requiring a type to be one of the ApplicationCore push input accessor types (scalar or array).
   */
  template<typename T>
  concept push_input = requires {
    typename T::value_type;
    requires std::is_base_of<ScalarPushInput<typename T::value_type>, T>::value ||
        std::is_base_of<ScalarPushInputWB<typename T::value_type>, T>::value ||
        std::is_base_of<ArrayPushInput<typename T::value_type>, T>::value ||
        std::is_base_of<ArrayPushInputWB<typename T::value_type>, T>::value;
    requires user_type<typename T::value_type>;
  };

  template<typename T>
  concept scalar_accessor = std::is_base_of<ScalarAccessor<typename T::value_type>, T>::value;

  template<typename T>
  concept array_accessor = std::is_base_of<ArrayAccessor<typename T::value_type>, T>::value;

} // namespace ChimeraTK
