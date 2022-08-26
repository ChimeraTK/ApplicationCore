// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*!
 * Provide an aggregateable StatusOutputs which can have one of the four states: OFF, OK, WARNING, FAULT.
 *
 * Multiple StatusOutputs can be aggregated using the StatusAggregator. StatusOutputs are typically provided by
 * StatusMonitors, but also custom ApplicationModules can provide them.
 *
 * For convenience, also StatusPushInputs and StatusPollInput are provided for use in custom ApplicationModules.
 */

#include "Module.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/ControlSystemAdapter/StatusAccessorBase.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** Special StatusAccessor - used to avoid code duplication in StatusOutput, StatusPushInput and StatusPollInput. */
  template<typename ACCESSOR>
  struct StatusAccessor : ACCESSOR, StatusAccessorBase {
    /** Note: In contrast to normal ScalarInput accessors, this constructor omits the unit argument. */
    StatusAccessor(Module* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ACCESSOR(owner, name, "", description, tags) {}
    StatusAccessor() {}

    using ACCESSOR::ACCESSOR;

    /** Reserved tag which is used to mark status outputs */
    constexpr static auto tagStatusOutput = "_ChimeraTK_StatusOutput_statusOutput";

    /** Implicit type conversion to user type T to access the value. */
    operator Status&() { return *reinterpret_cast<Status*>(&ACCESSOR::get()->accessData(0, 0)); }

    /** Implicit type conversion to user type T to access the const value. */
    operator const Status&() const { return *reinterpret_cast<Status*>(&ACCESSOR::get()->accessData(0, 0)); }

    /** Assignment operator, assigns the first element. */
    StatusAccessor& operator=(Status rightHandSide) {
      ACCESSOR::get()->accessData(0, 0) = static_cast<int32_t>(rightHandSide);
      return *this;
    }

    /* Delete increment/decrement operators since they do not make much sense with a Status */
    void operator++() = delete;
    void operator++(int) = delete;
    void operator--() = delete;
    void operator--(int) = delete;
  };

  /********************************************************************************************************************/

  /** Special ScalarOutput which represents a status which can be aggregated by the StatusAggregator. */
  struct StatusOutput : StatusAccessor<ScalarOutput<int32_t>> {
    /** Note: In contrast to normal ScalarOutput accessors, this constructor omits the unit argument. */
    StatusOutput(Module* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : StatusAccessor<ScalarOutput<int32_t>>(owner, name, "", description, tags) {
      addTag(tagStatusOutput);
    }
    StatusOutput() {}
    using StatusAccessor<ScalarOutput<int32_t>>::operator=;

    void writeIfDifferent(Status newValue) { ScalarOutput<int32_t>::writeIfDifferent(static_cast<int32_t>(newValue)); };
  };

  /********************************************************************************************************************/

  /** Special StatusPushInput which reads from a StatusOutput and also handles the type conversion */
  struct StatusPushInput : StatusAccessor<ScalarPushInput<int32_t>> {
    using StatusAccessor<ScalarPushInput<int32_t>>::StatusAccessor;
    using StatusAccessor<ScalarPushInput<int32_t>>::operator=;
  };

  /********************************************************************************************************************/

  /** Special StatusPollInput which reads from a StatusOutput and also handles the type conversion */
  struct StatusPollInput : StatusAccessor<ScalarPollInput<int32_t>> {
    using StatusAccessor<ScalarPollInput<int32_t>>::StatusAccessor;
    using StatusAccessor<ScalarPollInput<int32_t>>::operator=;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
