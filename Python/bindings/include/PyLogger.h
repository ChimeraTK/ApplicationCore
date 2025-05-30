// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "Logger.h"

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyLogger {
   public:
    static void bind(py::module& mod);
  };

  /********************************************************************************************************************/

  /**
   * PyLoggerStreamProxy
   *
   * Since the StreamProxy is a C++ stream, we have a local wrapper object that provides a log() function instead
   */
  class PyLoggerStreamProxy {
   public:
    PyLoggerStreamProxy(Logger::Severity severity, std::string context)
    : _severity(severity), _context(std::move(context)) {}

    void log(const std::string& message) { ChimeraTK::logger(_severity, _context) << message; }

   private:
    Logger::Severity _severity;
    std::string _context;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
