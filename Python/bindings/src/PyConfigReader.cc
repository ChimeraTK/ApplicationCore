// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <pybind11/embed.h>
// pybind11 includes should come first

#include "PyApplicationModule.h"
#include "PyConfigReader.h"

#include <ChimeraTK/cppext/ranges.hpp>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  UserTypeVariantNoVoid PyConfigReader::get(
      ChimeraTK::DataType dt, const std::string& path, std::optional<UserTypeVariantNoVoid> defaultValue) {
    std::optional<UserTypeVariantNoVoid> rv;
    ChimeraTK::callForTypeNoVoid(dt.getAsTypeInfo(), [&](auto t) {
      using UserType = decltype(t);

      if(defaultValue) {
        UserType valAsUserType;
        std::visit(
            [&](auto value) { valAsUserType = ChimeraTK::userTypeToUserType<UserType>(value); }, defaultValue.value());
        rv.emplace(_reader.get().get<UserType>(path, valAsUserType));
      }
      else {
        rv.emplace(_reader.get().get<UserType>(path));
      }
    });

    return std::move(rv.value());
  }

  /********************************************************************************************************************/

  UserTypeTemplateVariantNoVoid<std::vector> PyConfigReader::getArray(ChimeraTK::DataType dt, const std::string& path,
      std::optional<UserTypeTemplateVariantNoVoid<std::vector>> defaultValue) {
    std::optional<UserTypeTemplateVariantNoVoid<std::vector>> rv;
    ChimeraTK::callForTypeNoVoid(dt.getAsTypeInfo(), [&](auto t) {
      using UserType = decltype(t);

      if(defaultValue) {
        std::vector<UserType> valAsUserType = std::visit(
            [&](auto value) {
              return value | std::views::transform([](auto& v) { return ChimeraTK::userTypeToUserType<UserType>(v); }) |
                  cppext::ranges::to<std::vector>();
            },
            defaultValue.value());
        rv.emplace(_reader.get().get<std::vector<UserType>>(path, valAsUserType));
      }
      else {
        rv.emplace(_reader.get().get<std::vector<UserType>>(path));
      }
    });

    return std::move(rv.value());
  }

  /********************************************************************************************************************/

  void PyConfigReader::bind(py::module& m) {
    // Global access to appConfig(), mimicking something like Application::appConfig() in C++
    m.def("appConfig", []() { return PyConfigReader(PyApplicationModule::appConfig()); });
    py::class_<PyConfigReader>(m, "ConfigReader")
        .def("get", &PyConfigReader::get,
            "Get value for given configuration variable.\n\nThis is already accessible right after construction of "
            "this object. Throws ChimeraTK::logic_error if variable doesn't exist. To obtain the value of an array, "
            "use an std::vector<T> as template argument.",
            py::arg(), py::arg("variableName"), py::arg("defaultValue") = std::nullopt)
        .def("getArray", &PyConfigReader::getArray,
            "Get value for given configuration variable.\n\nThis is already accessible right after construction of "
            "this object. Throws ChimeraTK::logic_error if variable doesn't exist. ",
            py::arg(), py::arg("variableName"), py::arg("defaultValue") = std::nullopt)
        .def("getModules", &PyConfigReader::getModules, py::arg("path") = "");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
