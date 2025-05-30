// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyLogger.h"

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  void PyLogger::bind(py::module& mod) {
    // Logger::Severity
    py::class_<Logger> mPythonLogger(mod, "Logger");
    py::enum_<Logger::Severity>(mPythonLogger, "Severity")
        .value("trace", Logger::Severity::trace)
        .value("debug", Logger::Severity::debug)
        .value("info", Logger::Severity::info)
        .value("warning", Logger::Severity::warning)
        .value("error", Logger::Severity::error)
        .export_values();

    /**
     * Logger::StreamProxy
     *
     * Since the StreamProxy is a C++ stream, we have a local wrapper
     * object that provides a log() function instead
     */
    py::class_<PyLoggerStreamProxy>(mPythonLogger, "StreamProxy").def("log", &PyLoggerStreamProxy::log);

    /**
     * Global logger helper function
     */
    mod.def("logger",
        [](Logger::Severity severity, const std::string& context) { return PyLoggerStreamProxy(severity, context); });
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
