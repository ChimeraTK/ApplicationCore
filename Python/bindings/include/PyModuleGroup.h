// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ModuleGroup.h"
#include "PyOwnershipManagement.h"

/**********************************************************************************************************************/

namespace pybind11 {
  class module_;
  using module = module_;
} // namespace pybind11

/**********************************************************************************************************************/

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyModuleGroup : public ModuleGroup, public PyOwningObject {
   public:
    using ModuleGroup::ModuleGroup;
    PyModuleGroup(PyModuleGroup&&) = default;

    static void bind(pybind11::module& mod);
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
