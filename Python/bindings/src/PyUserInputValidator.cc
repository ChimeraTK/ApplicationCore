// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyUserInputValidator.h"

#include "PyArrayAccessor.h"
#include "PyScalarAccessor.h"

#include <pybind11/functional.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  void PyUserInputValidator::add(
      const std::string& errorMessage, std::function<bool(void)>& isValidFunction, py::args& args) {
    auto* validator = _impl.addValidator(isValidFunction, errorMessage);
    for(const auto& arg : args) {
      if(py::type::of(arg).is(py::type::of<PyScalarAccessor>())) {
        arg.cast<PyScalarAccessor&>().visit(
            [&](auto& accessor) { _impl.registerAccessorWithValidator(accessor, validator); });
      }
      else if(py::type::of(arg).is(py::type::of<PyArrayAccessor>())) {
        arg.cast<PyArrayAccessor&>().visit(
            [&](auto& accessor) { _impl.registerAccessorWithValidator(accessor, validator); });
      }
      else {
        throw ChimeraTK::logic_error("Invalid accessor type " + py::type::of(arg).attr("__name__").cast<std::string>());
      }
    }
  }

  /********************************************************************************************************************/

  void PyUserInputValidator::bind(py::module& m) {
    py::class_<PyUserInputValidator>(m, "UserInputValidator")
        .def(py::init<>())
        .def("add", &PyUserInputValidator::add)
        .def("setErrorFunction", &PyUserInputValidator::setErrorFunction)
        .def("validate", &PyUserInputValidator::validate)
        .def("validateAll", &PyUserInputValidator::validateAll)
        .def("setFallback",
            py::overload_cast<PyScalarAccessor&, UserTypeVariantNoVoid>(&PyUserInputValidator::setFallback))
        .def("setFallback",
            py::overload_cast<PyArrayAccessor&, const UserTypeTemplateVariantNoVoid<Vector>&>(
                &PyUserInputValidator::setFallback));
  }

  /********************************************************************************************************************/

  void PyUserInputValidator::setFallback(PyScalarAccessor& acc, UserTypeVariantNoVoid inputValue) {
    acc.visit([&](auto& accessor) {
      using Accessor = typename std::remove_reference_t<decltype(accessor)>;
      using UserType = typename Accessor::value_type;
      std::visit(
          [&](auto value) { _impl.setFallback(accessor, ChimeraTK::userTypeToUserType<UserType>(value)); }, inputValue);
    });
  }

  /********************************************************************************************************************/

  void PyUserInputValidator::setFallback(
      PyArrayAccessor& acc, const UserTypeTemplateVariantNoVoid<Vector>& inputValue) {
    acc.visit([&](auto& accessor) {
      using Accessor = typename std::remove_reference_t<decltype(accessor)>;
      using UserType = typename Accessor::value_type;
      std::visit(
          [&](auto vector) {
            std::vector<UserType> converted(vector.size());
            std::transform(vector.begin(), vector.end(), converted.begin(),
                [](auto& value) { return userTypeToUserType<UserType>(value); });
            _impl.setFallback(accessor, converted);
          },
          inputValue);
    });
  }
  /********************************************************************************************************************/

} // namespace ChimeraTK
