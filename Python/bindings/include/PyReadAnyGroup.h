// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "PyTransferElement.h"

#include <ChimeraTK/ReadAnyGroup.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyReadAnyGroup {
   public:
    PyReadAnyGroup() = default;

    PyReadAnyGroup(ReadAnyGroup&& other);

    PyReadAnyGroup(py::args args);

    void readUntil(const TransferElementID& tid);

    void add(PyTransferElementBase& acc);

    void readUntilAccessor(PyTransferElementBase& acc);

    void readUntilAll(py::args args);

    TransferElementID readAny();

    TransferElementID readAnyNonBlocking();

    void finalise() { _impl.finalise(); }

    void interrupt() { _impl.interrupt(); }

    static void bind(py::module& mod);

   protected:
    ReadAnyGroup _impl;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
