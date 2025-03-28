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
    explicit PyDataConsistencyGroup(DataConsistencyGroup&& other) : _impl(std::move(other)) {}

    explicit PyDataConsistencyGroup(const py::args& args, DataConsistencyGroup::MatchingMode mode, unsigned histLen)
    : _impl(mode) {
      for(const auto& acc : args) {
        add(acc.cast<PyTransferElementBase&>(), histLen);
      }
    }

    void add(PyTransferElementBase& acc, unsigned histLen = DataConsistencyGroup::defaultHistLen) {
      _impl.add(acc.getTE(), histLen);
    }

    bool update(const TransferElementID& transferelementid) { return _impl.update(transferelementid); }

    DataConsistencyGroup::MatchingMode getMatchingMode() const { return _impl.getMatchingMode(); };

    static void bind(py::module& mod);

   protected:
    DataConsistencyGroup _impl;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
