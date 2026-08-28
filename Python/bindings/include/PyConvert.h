// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * This header collects some conversion routines which should be useful both for ApplicationCore Python bindings
 * and DeviceAccess Python bindings.
 *
 * Our goal is to support differtly packed user data as input, and without data loss convert to right ChimeraTK UserType
 * scalar or vector. We support:
 * - Numpy arrays of the usual types
 * - Python lists of native Python scalars (bool, int, float, string) or numpy scalars (e.g. np.float32)
 *
 * The naive approch of defining std::variant<list of C++ user types or vector or those> does not work since it
 * results in forceful type coercion, e.g. np.float32 is truncated to integers.
 * Pybind11 has a 2 pass algorithm for type conversion:
 * Pass 1 checks for a perfect match, of the included types in the std::variant.
 * If a perfect match is not found, it falls back to Pass 2, which looks at implicit type conversions.
 * When we defined std::variant<int, float, double> and pass in a np.float32 value, the latter is not deteced as perfect
 * match for C++-float in Pass 1, and then implicitly truncated to an integer value in Pass 2.
 *
 * Since we finally anyway make use of our custom type conversion function userTypeToUserType, we must avoid as far as
 * possible, automatic type conversions of the input.
 */

#include <pybind11/numpy.h>

#include <iostream>
#include <variant>

namespace ChimeraTK {

  namespace py = pybind11;

  /********************************************************************************************************************/

  /// the python scalar primities
  using Scalar = std::variant<bool, int64_t, double, std::string>;

  template<typename expectedUserType>
  expectedUserType convertPyScalar(const py::object& input) {
    // check for numpy scalars (e.g., np.int32, np.float64)
    // NumPy scalars have an `.item()` method that returns standard Python primitives
    py::object clean_input = input;
    if(py::hasattr(input, "item") && !py::isinstance<py::str>(input)) {
      try {
        clean_input = input.attr("item")(); // Convert np.int32(5) -> Python int(5)
      }
      catch(const py::error_already_set&) {
        // Not a numpy scalar (but could be numpy array with more than one elements)
      }
    }

    // try casting to scalar (Handles standard primitives & converted NumPy scalars)
    // this may throw py::cast_error
    auto scalar = clean_input.cast<Scalar>();
    expectedUserType ret;
    std::visit(
        [&ret](const auto& arg) {
          std::cout << "convertPyScalar v = " << std::boolalpha << arg << std::endl;
          ret = userTypeToUserType<expectedUserType>(arg);
        },
        scalar);
    std::cout << "convertPyScalar ret = " << std::boolalpha << ret << std::endl;
    return ret;
  }

  /********************************************************************************************************************/

  /// helper for array conversion when data types are know at compile time
  template<typename expectedUserType, typename FROM>
  std::vector<expectedUserType> convertTypedPyArray(const py::array& arr) {
    std::vector<expectedUserType> converted(arr.size());
    // 1. Convert reference handle to typed wrapper (Zero-copy, O(1) operation)
    auto typed_arr = arr.cast<py::array_t<FROM>>();

    // 2. Request an unchecked proxy view (NDIM=1 for 1D, 2 for 2D, etc.)
    //    Disables GIL acquisition and runtime bounds-checking overhead.
    auto r = typed_arr.template unchecked<1>();

    // 3. Directly access elements
    for(ssize_t i = 0; i < r.shape(0); ++i) {
      // Zero-copy direct read
      // std::cout << "received value: " << in << " of size " << sizeof(FROM) << " bytes.\n";
      converted[i] = userTypeToUserType<expectedUserType>(r(i));
    }
    return converted;
  }

  /********************************************************************************************************************/

  /**
   * This takes numpy arrays.
   * Advantage of py::array over std::vector is, it directly maps memory of numpy arrays, eliminating the copy and
   * convert altogether where possible. When conversion must happen, the stricter numpy rules for conversion apply,
   * which forbid information loss.
   */
  template<typename expectedUserType>
  std::vector<expectedUserType> convertPyArray(const py::array& arr) {
    std::cout << "convertPyArray for numpy array [" << arr.dtype().kind() << arr.itemsize()
              << "] Shape: " << arr.shape(0) << "\n";

    py::dtype dt = arr.dtype();

    if(dt.is(py::dtype::of<int8_t>())) {
      return convertTypedPyArray<expectedUserType, int8_t>(arr);
    }
    if(dt.is(py::dtype::of<int16_t>())) {
      return convertTypedPyArray<expectedUserType, int16_t>(arr);
    }
    if(dt.is(py::dtype::of<int32_t>())) {
      return convertTypedPyArray<expectedUserType, int32_t>(arr);
    }
    if(dt.is(py::dtype::of<int64_t>())) {
      return convertTypedPyArray<expectedUserType, int64_t>(arr);
    }
    if(dt.is(py::dtype::of<uint8_t>())) {
      return convertTypedPyArray<expectedUserType, uint8_t>(arr);
    }
    if(dt.is(py::dtype::of<uint16_t>())) {
      return convertTypedPyArray<expectedUserType, uint16_t>(arr);
    }
    if(dt.is(py::dtype::of<uint32_t>())) {
      return convertTypedPyArray<expectedUserType, uint32_t>(arr);
    }
    if(dt.is(py::dtype::of<uint64_t>())) {
      return convertTypedPyArray<expectedUserType, uint64_t>(arr);
    }
    if(dt.is(py::dtype::of<float>())) {
      return convertTypedPyArray<expectedUserType, float>(arr);
    }
    if(dt.is(py::dtype::of<double>())) {
      return convertTypedPyArray<expectedUserType, double>(arr);
    }
    if(dt.is(py::dtype::of<bool>())) {
      return convertTypedPyArray<expectedUserType, bool>(arr);
    }

    throw std::runtime_error("Unsupported NumPy array dtype!");
  }

  /********************************************************************************************************************/

  /// this takes native python lists, content possibly of mixed types
  /// convertToPyArr should be preferred when input is a numpy-array
  template<typename expectedUserType>
  std::vector<expectedUserType> convertPyList(const py::object& input) {
    // Note, the following does not work because the cast often chooses a wrong type when doing auto conversion:
    // auto vec = input.cast<std::vector<Scalar>>();
    // Instead, we must manually go over the sequence and convert every item

    auto seq = input.cast<py::sequence>();
    std::cout << "convertPyList of size " << seq.size() << ":\n";
    std::vector<expectedUserType> converted(seq.size());

    std::transform(seq.begin(), seq.end(), converted.begin(), [](const auto& itemHandle) {
      auto v = py::reinterpret_borrow<py::object>(itemHandle);
      return convertPyScalar<expectedUserType>(v);
    });
    return converted;
  }

  /********************************************************************************************************************/

  /// input may be numpy arrays, numpy scalars, python primitives, and lists of numpy scalars or python primitives
  template<typename expectedUserType>
  std::vector<expectedUserType> convertPyObject(const py::object& input, bool allowScalar, bool allowArray) {
    if(allowArray) {
      // CHECK NUMPY ARRAYS FIRST (Zero-copy, preserves multi-dim metadata)
      if(py::isinstance<py::array>(input)) {
        auto arr = input.cast<py::array>();
        return convertPyArray<expectedUserType>(arr);
      }
    }

    if(allowScalar) {
      return {convertPyScalar<expectedUserType>(input)};
    }

    if(allowArray) {
      // try casting to vector (Handles Python lists/tuples)
      return convertPyList<expectedUserType>(input);
    }

    assert(false);
    throw py::value_error("unsupported parameters for convertPyObject");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
