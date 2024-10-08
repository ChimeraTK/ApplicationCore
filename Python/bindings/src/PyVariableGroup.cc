// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyVariableGroup.h"

#include "PyReadAnyGroup.h"
#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  void PyVariableGroup::bind(py::module& m) {
    /**
     * The actual VariableGroup is only used as a base class, on the Python side we give out PyVariableGroups
     * as "VariableGroup".
     */
    py::class_<VariableGroup>(m, "VariableGroupBase")
        .def("getName", &VariableGroup::getName, "Get the name of the module instance.")
        .def(
            "readAnyGroup", [](VariableGroup& self) { return PyReadAnyGroup(self.readAnyGroup()); },
            "Create a ChimeraTK::ReadAnyGroup for all readable variables in this Module.")
        .def(
            "readAll",
            [](VariableGroup& self, bool includeReturnChannels) {
              py::gil_scoped_release release;
              self.readAll(includeReturnChannels);
            },
            "Read all readable variables in the group.\n\nIf there are push-type variables in the group, this call "
            "will block until all of the variables have received an update. All push-type variables are read first, "
            "the poll-type variables are therefore updated with the latest values upon return. includeReturnChannels "
            "determines whether return channels of *OutputRB accessors are included in the read.",
            py::arg("includeReturnChannels") = false)
        .def(
            "readAllLatest",
            [](VariableGroup& self, bool includeReturnChannels) {
              py::gil_scoped_release release;
              self.readAllLatest(includeReturnChannels);
            },
            "Just call readLatest() on all readable variables in the group.\n\nincludeReturnChannels determines "
            "whether return channels of *OutputRB accessors are included in the read.",
            py::arg("includeReturnChannels") = false)

        .def(
            "readAllNonBlocking",
            [](VariableGroup& self, bool includeReturnChannels) {
              py::gil_scoped_release release;
              self.readAllNonBlocking(includeReturnChannels);
            },
            "Just call readNonBlocking() on all readable variables in the group.\n\nincludeReturnChannels determines "
            "whether return channels of *OutputRB accessors are included in the read.",
            py::arg("includeReturnChannels") = false)
        .def(
            "writeAll",
            [](VariableGroup& self, bool includeReturnChannels) {
              py::gil_scoped_release release;
              self.writeAll(includeReturnChannels);
            },
            "Just call write() on all writable variables in the group.\n\nincludeReturnChannels determines whether "
            "return channels of *InputWB accessors are included in the write.",
            py::arg("includeReturnChannels") = false)
        .def(
            "writeAllDestructively",
            [](VariableGroup& self, bool includeReturnChannels) {
              py::gil_scoped_release release;
              self.writeAllDestructively(includeReturnChannels);
            },
            "Just call writeDestructively() on all writable variables in the group.\n\nincludeReturnChannels "
            "determines whether return channels of *InputWB accessors are included in the write.",
            py::arg("includeReturnChannels") = false);

    /**
     * PyVariableGroup as "VariableGroup"
     */
    // return_value_policy::reference is not enough in constructor factory function.
    // in order to turn off memory management by python, we also need to adapt custom holder type and remove deleter.
    py::class_<PyVariableGroup, VariableGroup, PyOwningObject, std::unique_ptr<PyVariableGroup, py::nodelete>> vg(
        m, "VariableGroup", py::dynamic_attr(), py::multiple_inheritance());

    vg.def(py::init([](VariableGroup& owner, const std::string& name, const std::string& description,
                        const std::unordered_set<std::string>& tags) {
      return dynamic_cast<PyOwningObject&>(owner).make_child<PyVariableGroup>(&owner, name, description, tags);
    }),
        py::return_value_policy::reference,
        // doc and default args:
        "", py::arg("owner"), py::arg("name"), py::arg("description"),
        py::arg("tags") = std::unordered_set<std::string>{});
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
