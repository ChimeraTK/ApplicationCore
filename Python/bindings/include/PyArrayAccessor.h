// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "ArrayAccessor.h"
#include "PyOwnershipManagement.h"
#include "PyTransferElement.h"
#include "PyVariantTypeDefs.h"
#include <pybind11/numpy.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   * Helper class acting as a ArrayAccessor with a variant UserType.
   */
  class PyArrayAccessor : public PyTransferElement<PyArrayAccessor>, public PyOwnedObject {
    // Helper for constructor - note: we can move templates to the .cc file if we use them only in the same .cc file
    template<template<typename> class AccessorType>
    static userTypeTemplateVariantNoVoid<ArrayAccessor> createAccessor(ChimeraTK::DataType type, Module* owner,
        const std::string& name, std::string unit, size_t nElements, const std::string& description,
        const std::unordered_set<std::string>& tags);

   public:
    PyArrayAccessor() : _accessor(ArrayOutput<int>()) {}
    PyArrayAccessor(PyArrayAccessor&&) = default;
    ~PyArrayAccessor();

    template<template<typename> class AccessorType>
    PyArrayAccessor(AccessorTypeTag<AccessorType>, ChimeraTK::DataType type, Module* owner, const std::string& name,
        std::string unit, size_t nElements, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : _accessor(createAccessor<AccessorType>(type, owner, name, unit, nElements, description, tags)) {}

    // userTypeTemplateVariantNoVoid expects a single template argument, std::vector has multiple (with defaults)...
    template<typename T>
    using Vector = std::vector<T>;

    py::object readAndGet();

    void setAndWrite(const userTypeTemplateVariantNoVoid<Vector>& vec);

    size_t getNElements();

    void set(const userTypeTemplateVariantNoVoid<Vector>& vec);

    py::object get() const;

    py::object getitem(size_t index) const;

    void setitem(size_t index, const userTypeVariantNoVoid& val);

    std::string repr(py::object& acc) const;

    py::buffer_info getBufferInfo();

    py::object getattr(const std::string& name) const { return get().attr(name.c_str()); }

    static void bind(py::module& mod);

    mutable userTypeTemplateVariantNoVoid<ArrayAccessor> _accessor;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK