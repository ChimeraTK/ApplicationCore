// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "PyOwnershipManagement.h"
#include "PyTransferElement.h"
#include "VoidAccessor.h"

#include <ChimeraTK/VariantUserTypes.h>

namespace py = pybind11;

namespace ChimeraTK {

  template<class AccessorType>
  class VoidTypeTag {};

  /********************************************************************************************************************/

  class PyVoidAccessor : public PyTransferElement<PyVoidAccessor>, public PyOwnedObject {
   public:
    PyVoidAccessor();

    template<class AccessorType>
    PyVoidAccessor(VoidTypeTag<AccessorType>, Module* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    PyVoidAccessor(PyVoidAccessor&&) = default;

    ~PyVoidAccessor() override;

    static std::string repr(py::object& acc);

    static void bind(py::module& mod);

    // NOLINTNEXTLINE(readability-identifier-naming)
    mutable std::variant<VoidAccessor> _accessor;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
