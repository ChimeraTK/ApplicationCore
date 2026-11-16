// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyFanIn.h"

#include <ChimeraTK/PyConvert.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  template<template<typename> class FanInType>
  void PyFanIn::initialize(PyFanIn& target, DataType type, VariableGroup& owner, const std::string& name,
      const std::vector<std::string>& additionalNames, const std::string& unit, const std::string& description,
      py::function aggregator, const std::unordered_set<std::string>& tags) {
    callForTypeNoVoid(type, [&](auto valueType) {
      using UserType = decltype(valueType);
      target._impl = std::make_unique<Model<FanInType, UserType>>(
          &owner, name, additionalNames, unit, description, std::move(aggregator), tags);
    });
  }

  template<template<typename> class FanInType>
  static auto fanInFactory() {
    return [](DataType type, VariableGroup& owner, const std::string& name, const std::vector<std::string>& names,
               const std::string& unit, const std::string& description, py::function aggregator,
               const std::unordered_set<std::string>& tags, PyFanIn& target) {
      PyFanIn::initialize<FanInType>(
          target, type, owner, name, names, unit, description, std::move(aggregator), tags);
    };
  }

  void PyFanIn::bind(py::module& mod) {
    py::class_<PyFanIn, std::unique_ptr<PyFanIn, py::nodelete>>(mod, "FanIn", py::module_local())
        .def("get", &PyFanIn::get, "Return the current aggregated value.")
        .def("getName", &PyFanIn::getName);

    auto bindFactory = [&](const char* name, auto factory) {
      mod.def(name, [factory](DataType type, VariableGroup& owner, const std::string& pvName,
                   const std::string& unit, const std::string& description, py::function aggregator,
                   const std::vector<std::string>& additionalNames,
                   const std::unordered_set<std::string>& tags) {
        auto* fanIn = dynamic_cast<PyOwningObject&>(owner).make_child<PyFanIn>();
        factory(type, owner, pvName, additionalNames, unit, description, std::move(aggregator), tags, *fanIn);
        return fanIn;
      },
          py::arg("type"), py::arg("owner"), py::arg("name"), py::arg("unit"), py::arg("description"),
          py::arg("aggregator"), py::arg("additionalNames") = std::vector<std::string>{},
          py::arg("tags") = std::unordered_set<std::string>{}, py::return_value_policy::reference);
    };

    bindFactory("ScalarFanIn", fanInFactory<ScalarFanIn>());
    bindFactory("ScalarFanInWB", fanInFactory<ScalarFanInWB>());
    // ArrayFanIn currently has no constructor carrying the required array size;
    // expose it after the core FanIn API supports sized array inputs.
  }

} // namespace ChimeraTK
