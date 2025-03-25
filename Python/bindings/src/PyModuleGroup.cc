// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <pybind11/embed.h>
// pybind11 includes should come first

#include "PyModuleGroup.h"

#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  void PyModuleGroup::bind(py::module& m) {
    /**
     * The actual ModuleGroup is only used as a base class, on the Python side we give out PyModuleGroups
     * as "ModuleGroup".
     */
    py::class_<ModuleGroup>(m, "ModuleGroupBase")
        .def("getName", &ModuleGroup::getName, R"(Get the name of the module instance.)");

    /**
     * PyModuleGroup as "ModuleGroup"
     */
    // return_value_policy::reference is not enough in constructor factory function.
    // in order to turn off memory management by python, we also need to adapt custom holder type and remove deleter.
    py::class_<PyModuleGroup, ModuleGroup, PyOwningObject, std::unique_ptr<PyModuleGroup, py::nodelete>> mg(
        m, "ModuleGroup", py::dynamic_attr(), py::multiple_inheritance());
    mg.def(py::init([](ModuleGroup& owner, const std::string& name, const std::string& description,
                        const std::unordered_set<std::string>& tags) {
      return dynamic_cast<PyOwningObject&>(owner).make_child<PyModuleGroup>(&owner, name, description, tags);
    }),
        py::return_value_policy::reference,
        // doc and default args:
        "", py::arg("owner"), py::arg("name"), py::arg("description"),
        py::arg("tags") = std::unordered_set<std::string>{});
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
