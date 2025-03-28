// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyDataConsistencyGroup.h"

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  void PyDataConsistencyGroup::bind(py::module& mod) {
    // DataConsistencyGroup::MatchingMode
    py::enum_<DataConsistencyGroup::MatchingMode>(mod, "MatchingMode")
        .value("none", DataConsistencyGroup::MatchingMode::none)
        .value("exact", DataConsistencyGroup::MatchingMode::exact)
        .value("historized", DataConsistencyGroup::MatchingMode::historized)
        .export_values();

    // DataConsistencyGroup
    py::class_<PyDataConsistencyGroup>(mod, "DataConsistencyGroup")
        .def(py::init<py::args, DataConsistencyGroup::MatchingMode, unsigned>(),
            "Construct DataConsistencyGroup with given list of accessors", py::kw_only(),
            py::arg("mode") = DataConsistencyGroup::MatchingMode::exact,
            py::arg("histLen") = DataConsistencyGroup::defaultHistLen)
        .def("add", &PyDataConsistencyGroup::add,
            "Add register to group.\n\nThe same TransferElement can be part of multiple DataConsistencyGroups. The "
            "register must be must be readable, and it must have AccessMode::wait_for_new_data.",
            py::arg("element"), py::arg("histLen") = DataConsistencyGroup::defaultHistLen)
        .def("update", &PyDataConsistencyGroup::update,
            "This function updates consistentElements, a set of TransferElementID.\n\nIt returns true, if a consistent "
            "state is reached. It returns false if an TransferElementID was updated, that was not added to this Group.",
            py::arg("transferelementid"))
        .def("getMatchingMode", &PyDataConsistencyGroup::getMatchingMode, "Return the current matching mode.");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
