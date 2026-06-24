// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyLogger.h"

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  void PyLogger::bind(py::module& mod) {
    // Logger::Severity
    py::class_<Logger> mPythonLogger(mod, "Logger", py::module_local());
    // Register the inner enum Value first, so Severity can reference it
    py::enum_<Logger::Severity::Value>(mPythonLogger, "_SeverityValue", py::module_local(), py::arithmetic())
        .value("trace", Logger::Severity::Value::trace)
        .value("debug", Logger::Severity::Value::debug)
        .value("info", Logger::Severity::Value::info)
        .value("warning", Logger::Severity::Value::warning)
        .value("error", Logger::Severity::Value::error)
        .export_values();

    py::class_<Logger::Severity> mSeverity(mPythonLogger, "Severity", py::module_local());
    mSeverity
        .def_property_readonly_static(
            "trace", [](py::object) { return Logger::Severity(Logger::Severity::Value::trace); })
        .def_property_readonly_static(
            "debug", [](py::object) { return Logger::Severity(Logger::Severity::Value::debug); })
        .def_property_readonly_static(
            "info", [](py::object) { return Logger::Severity(Logger::Severity::Value::info); })
        .def_property_readonly_static(
            "warning", [](py::object) { return Logger::Severity(Logger::Severity::Value::warning); })
        .def_property_readonly_static(
            "error", [](py::object) { return Logger::Severity(Logger::Severity::Value::error); })
        .def("__repr__", [](const Logger::Severity& s) { return std::string(s.toString()); })
        .def("__eq__", [](const Logger::Severity& a, const Logger::Severity& b) { return a == b; })
        .def("__ne__", [](const Logger::Severity& a, const Logger::Severity& b) { return a != b; });

    /**
     * Logger::StreamProxy
     *
     * Since the StreamProxy is a C++ stream, we have a local wrapper
     * object that provides a log() function instead
     */
    py::class_<PyLoggerStreamProxy>(mPythonLogger, "StreamProxy", py::module_local())
        .def("log", &PyLoggerStreamProxy::log);

    /**
     * Global logger helper function
     */
    mod.def("logger",
        [](Logger::Severity severity, const std::string& context) { return PyLoggerStreamProxy(severity, context); });
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
