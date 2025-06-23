// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "PyArrayAccessor.h"
#include "PyScalarAccessor.h"
#include "UserInputValidator.h"

#include <ChimeraTK/VariantUserTypes.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyUserInputValidator {
   public:
    PyUserInputValidator() = default;
    explicit PyUserInputValidator(UserInputValidator&& other) : _impl(std::move(other)) {}

    void add(const std::string& errorMessage, std::function<bool(void)>&, py::args& args);
    bool validate(ChimeraTK::TransferElementID& change) { return _impl.validate(change); }
    bool validateAll() { return _impl.validateAll(); }
    void setErrorFunction(const std::function<void(const std::string&)>& errorFunction) {
      _impl.setErrorFunction(errorFunction);
    };

    void setFallback(PyScalarAccessor& acc, UserTypeVariantNoVoid value);

    // UserTypeTemplateVariantNoVoid expects a single template argument, std::vector has multiple (with defaults)...
    template<typename T>
    using Vector = std::vector<T>;
    void setFallback(PyArrayAccessor& acc, const UserTypeTemplateVariantNoVoid<Vector>& value);

    static void bind(py::module& mod);

   protected:
    UserInputValidator _impl;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
