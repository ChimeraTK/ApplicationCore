// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "PyTransferElement.h"

#include <ChimeraTK/DataConsistencyGroup.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyDataConsistencyGroup {
   public:
    PyDataConsistencyGroup() = default;

    PyDataConsistencyGroup(DataConsistencyGroup&& other) : _impl(std::move(other)) {}

    PyDataConsistencyGroup(py::args args) {
      for(auto& acc : args) add(acc.cast<PyTransferElementBase&>());
    }

    void add(PyTransferElementBase& acc) { _impl.add(acc.getTE()); }

    bool update(const TransferElementID& transferelementid) { return _impl.update(transferelementid); }

    void setMatchingMode(DataConsistencyGroup::MatchingMode newMode) { _impl.setMatchingMode(newMode); }

    DataConsistencyGroup::MatchingMode getMatchingMode() const { return _impl.getMatchingMode(); };

    static void bind(py::module& mod);

   protected:
    // Helper constructor that takes a std::vector and passes it on to the iterator constructor of
    // ReadAnyGroup
    PyDataConsistencyGroup(std::vector<TransferElementAbstractor> args) : _impl(args.begin(), args.end()) {}

    DataConsistencyGroup _impl;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK