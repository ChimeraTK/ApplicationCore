// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ArrayAccessor.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/VariantUserTypes.h>

#include <variant>

namespace ChimeraTK {

  /********************************************************************************************************************/
  // Helper to nest std::variant types.
  /********************************************************************************************************************/

  namespace detail {

    template<typename... Variants>
    struct NestedVariant;

    template<typename V>
    struct NestedVariant<V> {
      using type = V;
    };

    template<typename... Ts, typename... Us, typename... Rest>
    struct NestedVariant<std::variant<Ts...>, std::variant<Us...>, Rest...> {
      using type = typename NestedVariant<std::variant<Ts..., Us...>, Rest...>::type;
    };

  } // namespace detail

  /********************************************************************************************************************/
  // Combined variant types that include all derived accessor types alongside the base accessor type.
  //
  // This avoids object slicing: when the variant stores (e.g.) a ScalarPollInput<UserType> instead of slicing
  // it to ScalarAccessor<UserType>, std::visit dispatches to ScalarPollInput::read() (which calls readLatest())
  // rather than the base ScalarAccessor::read(). The same applies to ArrayPollInput and the NoInitialValue
  // variants.
  //
  // All types the factory functions might create must be included as alternatives in the variant.
  /********************************************************************************************************************/

  /// All concrete scalar accessor types used by the Python bindings.
  using ScalarAccessorVariant = typename detail::NestedVariant<UserTypeTemplateVariantNoVoid<ScalarAccessor>,
      UserTypeTemplateVariantNoVoid<ScalarPollInput>, UserTypeTemplateVariantNoVoid<ScalarPushInput>,
      UserTypeTemplateVariantNoVoid<ScalarOutput>, UserTypeTemplateVariantNoVoid<ScalarPushInputWB>,
      UserTypeTemplateVariantNoVoid<ScalarOutputPushRB>, UserTypeTemplateVariantNoVoid<ScalarOutputReverseRecovery>,
      UserTypeTemplateVariantNoVoid<ScalarPushInputNoInitialValue>,
      UserTypeTemplateVariantNoVoid<ScalarPollInputNoInitialValue>>::type;

  /// All concrete array accessor types used by the Python bindings.
  using ArrayAccessorVariant = typename detail::NestedVariant<UserTypeTemplateVariantNoVoid<ArrayAccessor>,
      UserTypeTemplateVariantNoVoid<ArrayPollInput>, UserTypeTemplateVariantNoVoid<ArrayPushInput>,
      UserTypeTemplateVariantNoVoid<ArrayOutput>, UserTypeTemplateVariantNoVoid<ArrayPushInputWB>,
      UserTypeTemplateVariantNoVoid<ArrayOutputPushRB>, UserTypeTemplateVariantNoVoid<ArrayOutputReverseRecovery>,
      UserTypeTemplateVariantNoVoid<ArrayPushInputNoInitialValue>,
      UserTypeTemplateVariantNoVoid<ArrayPollInputNoInitialValue>>::type;

  /********************************************************************************************************************/

} // namespace ChimeraTK
