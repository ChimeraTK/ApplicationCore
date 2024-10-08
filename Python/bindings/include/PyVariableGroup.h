// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "PyOwnershipManagement.h"
#include "VariableGroup.h"

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyVariableGroup : public VariableGroup, public PyOwningObject {
   public:
    using VariableGroup::VariableGroup;
    PyVariableGroup(PyVariableGroup&&) = default;

    static void bind(py::module& m);
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK