// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "PyOwnershipManagement.h"
#include "PyTransferElement.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/VariantUserTypes.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyScalarAccessor : public PyTransferElement<PyScalarAccessor>, public PyOwnedObject {
    // Helper for constructor - note: we can move templates to the .cc file if we use them only in the same .cc file
    template<template<typename> class AccessorType>
    static UserTypeTemplateVariantNoVoid<ScalarAccessor> createAccessor(ChimeraTK::DataType type, Module* owner,
        const std::string& name, const std::string& unit, const std::string& description,
        const std::unordered_set<std::string>& tags);

   public:
    PyScalarAccessor();

    template<template<typename> class AccessorType>
    PyScalarAccessor(AccessorTypeTag<AccessorType>, ChimeraTK::DataType type, Module* owner, const std::string& name,
        const std::string& unit, const std::string& description, const std::unordered_set<std::string>& tags = {});

    PyScalarAccessor(PyScalarAccessor&&) = default;

    ~PyScalarAccessor();

    UserTypeVariantNoVoid readAndGet();

    UserTypeVariantNoVoid get() const;

    void writeIfDifferent(UserTypeVariantNoVoid val);

    void setAndWrite(UserTypeVariantNoVoid val);

    void set(UserTypeVariantNoVoid val);

    static std::string repr(py::object& acc);

    static void bind(py::module& mod);

    // Needs to be mutable to have a const get function, despite a non-const getHighLevelImplElement function
    // get has to be const so it can be used in operator== which is expected by pibind11 to have const arguments.
    mutable UserTypeTemplateVariantNoVoid<ScalarAccessor> _accessor;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
