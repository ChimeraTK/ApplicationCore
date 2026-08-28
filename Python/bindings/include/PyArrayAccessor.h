// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "AccessorVariant.h"
#include "ArrayAccessor.h"
#include "PyOwnershipManagement.h"
#include "PyTransferElement.h"

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
    static ArrayAccessorVariant createAccessor(ChimeraTK::DataType type, Module* owner, const std::string& name,
        std::string unit, size_t nElements, const std::string& description,
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

    /*
     *
     * The problem with std::vector is that conversion of python containers like lists or numpy arrays relies on
     * automatic conversion functions, rather than identical types.
     * You see that wen you add .noconvert() to the value argument (e.g. for the set method), then you get
     * TypeError: set(): incompatible function arguments when you pass in a numpy array of a supported type.
     * On the other hand, if you do allow automatic conversion, the order inside std::variant is relevant. There,
     * we list the wider types last, but that means automatic conversion discards information, which is not acceptable
     * (e.g. truncate numpy.float32 to C-int).
     *
     * Advantage of py::array is, it directly maps memory of numpy arrays, eliminating the copy and convert altogether
     * where possible. When conversion must happen, the stricter numpy rules for conversion apply, which forbid
     * information loss.
     * The flag c_style guarantees that array elements are contiguous in memory.
     * The flag forcecast allowes conversion from non-numpy objects like plain python lists.
     */

    // UserTypeTemplateVariantNoVoid expects a single template argument, std::vector has multiple (with defaults)...
    template<typename T>
    using Vector = std::vector<T>;

    // mapping of Python lists and iterables
    template<typename T>
    using SVector = std::vector<T>;
    template<typename T>
    // mapping of numpy arrays.
    // eliminate default flag py::array::forcecast, since it causes trouble (Pass 2 will select lossy conversion)
    // for same reason, pass value arguments with flat .noconvert()
    // We can live without the Pass 2 conversion since we have our own userTypeToUserType conversion in addition
    using PVector = py::array_t<T, py::array::c_style>;

    // TODO check - should we include PVector<bool> ?
    using UserTypeArrVariantNoVoid = std::variant<SVector<ChimeraTK::Boolean>, PVector<int8_t>, PVector<uint8_t>,
        PVector<int16_t>, PVector<uint16_t>, PVector<int32_t>, PVector<uint32_t>, PVector<int64_t>, PVector<uint64_t>,
        PVector<float>, PVector<double>,
        // TODO so we also need this? PVector<std::string>,
        // add SVector for types double and int64 for conversion from Python lists of native Python int,float.
        SVector<float>, SVector<double>, SVector<int64_t>, SVector<std::string>>;

    // TODO want to use numpy_scalar but requires newer pybind11!
    // TODO also consider DeviceAccess - PythonBindings
    // check whether we have same bug - I suspect so!
    /*
    py::short a; py::int b; py::long c;

    using UserTypeScalarVariantNoVoid = std::variant<py::numpy_scalar<int8_t>,
        py::numpy_scalar<uint8_t>, py::numpy_scalar<int16_t>, py::numpy_scalar<uint16_t>, py::numpy_scalar<int32_t>,
    py::numpy_scalar<uint32_t>, py::numpy_scalar<int64_t>, py::numpy_scalar<uint64_t>, py::numpy_scalar<float>,
    py::numpy_scalar<double>, py::numpy_scalar<std::string>, bool, double, std::string
        >;
*/

    py::object readAndGet();

    void setAndWrite(const UserTypeArrVariantNoVoid& vec);

    size_t getNElements();

    void set(const UserTypeArrVariantNoVoid& vec);

    py::object get() const;

    py::object getitem(size_t index) const;

    void setitem(size_t index, const UserTypeVariantNoVoid& val);

    void setslice(const py::slice& slice, const UserTypeVariantNoVoid& val);

    static std::string repr(py::object& acc);

    py::buffer_info getBufferInfo();

    py::object getattr(const std::string& name) const { return get().attr(name.c_str()); }

    static void bind(py::module& mod);

    mutable ArrayAccessorVariant _accessor;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
