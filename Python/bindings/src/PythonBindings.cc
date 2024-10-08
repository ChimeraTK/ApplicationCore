// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "Application.h"
#include "PyApplicationModule.h"
#include "PyArrayAccessor.h"
#include "PyConfigReader.h"
#include "PyDataConsistencyGroup.h"
#include "PyLogger.h"
#include "PyModuleGroup.h"
#include "PyOwnershipManagement.h"
#include "PyReadAnyGroup.h"
#include "PyScalarAccessor.h"
#include "PyVariableGroup.h"
#include "PyVariantTypeDefs.h"
#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include <ChimeraTK/DataConsistencyGroup.h>
#include <ChimeraTK/SupportedUserTypes.h>

#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/container/map.hpp>

#include <iostream>
#include <map>
#include <string>
#include <variant>

namespace py = pybind11;
using namespace py::literals;

/**
 * General notes:
 *
 * - Class docstrings do not appear in the stub, see: https://github.com/python/mypy/issues/16543
 */

namespace ChimeraTK {

  struct PyApplicationCoreUnload {
    ~PyApplicationCoreUnload() {
      if(Application::hasInstance()) { // hasInstance check required for stubgen tool (it doesn't instantiate an app)
        // We assume here that the reason for PyApplicationCore lib unload is application shutdown.
        // We need to deinitialize PythonModuleManager early in order to get back global interpreter lock.
        // Some static library deinitialsers (in particular exception object used by pybind11 register_exception)
        // assume that we own the GIL.
        // The PythonModuleManager will also shut down all PyApplicationModules (i.e. their internal threads) while
        // keeping the rest of the Application intact for now.
        Application::getInstance().getPythonModuleManager().deinit();
      }
    }
  };
  /**
   * In order to register an unload hook (a struct with non-trivial destructor), this function should be
   * called after any other static instance has been initialized.
   * The intention is that the unload hook executes before any other static deinitializer.
   * Note, we assume here that static destructors are called in reverse order of initialization.
   * This is not guaranteed by the C++ standard but should hold on most platforms.
   */
  template<class UnloadHook>
  static void registerUnloadHook() {
    static UnloadHook hook;
  }

  /********************************************************************************************************************/

  PYBIND11_MODULE(PyApplicationCore, m) {
    /**
     * logic_error exception
     */
    py::register_exception<ChimeraTK::logic_error>(m, "LogicError", PyExc_RuntimeError);

    /**
     *  DataType (with internal enum)
     */
    py::class_<DataType> mDataType(m, "DataType");
    mDataType.def(py::init<DataType::TheType>())
        .def("__str__", &DataType::getAsString)
        .def(py::self == py::self)
        .def("__repr__", [](const DataType& type) { return "DataType." + type.getAsString(); });
    py::enum_<DataType::TheType>(mDataType, "TheType")
        .value("none", DataType::none)
        .value("int8", DataType::int8)
        .value("uint8", DataType::uint8)
        .value("int16", DataType::int16)
        .value("uint16", DataType::uint16)
        .value("int32", DataType::int32)
        .value("uint32", DataType::uint32)
        .value("int64", DataType::int64)
        .value("uint64", DataType::uint64)
        .value("float32", DataType::float32)
        .value("float64", DataType::float64)
        .value("string", DataType::string)
        .value("Boolean", DataType::Boolean)
        .value("Void", DataType::Void)
        .export_values();
    py::implicitly_convertible<DataType::TheType, DataType>();
    py::implicitly_convertible<DataType, DataType::TheType>();

    /**
     * DataValidity
     */
    py::enum_<DataValidity>(m, "DataValidity")
        .value("ok", DataValidity::ok)
        .value("faulty", DataValidity::faulty)
        .export_values();

    /**
     * VersionNumber
     */
    py::class_<VersionNumber>(m, "VersionNumber")
        .def(py::init<>())
        .def(py::init<std::chrono::system_clock::time_point>())
        .def(py::init<std::nullptr_t>(), py::arg("version").none(true))
        // NOLINTBEGIN(misc-redundant-expression)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self < py::self)
        .def(py::self <= py::self)
        .def(py::self > py::self)
        .def(py::self >= py::self)
        // NOLINTEND(misc-redundant-expression)
        .def("getTime", &VersionNumber::getTime, R"(Return the time stamp associated with this version number.)")
        .def("__str__", [](VersionNumber& v) { return std::string(v); })
        .def("__repr__", [](VersionNumber& v) { return "VersionNumber(" + std::string(v) + ")"; });

    /**
     * TransferElementID
     */
    py::class_<TransferElementID>(m, "TransferElementID")
        .def(py::init<>())
        // NOLINTBEGIN(misc-redundant-expression)
        .def(py::self == py::self)
        .def(py::self != py::self)
        // NOLINTEND(misc-redundant-expression)
        .def("isValid", &TransferElementID::isValid, R"(Check whether the ID is valid.)")
        .def("__str__",
            [](TransferElementID& id) {
              std::stringstream ss;
              ss << id;
              return ss.str();
            })
        .def("__repr__", [](TransferElementID& id) {
          std::stringstream ss;
          ss << id;
          return "TransferElementID(" + ss.str() + ")";
        });

    /**
     * Define base classes so we can specify them in the derived class definitions, otherwise pybind11 seems to
     * complain about not mentioning all base classes
     */
    py::class_<PyOwnedObject> pwnedObject(m, "PyOwnedObject");
    py::class_<PyOwningObject> pwningObject(m, "PyOwningObject");

    /**
     *
     */

    /**
     * PyTransferElementBase - a common base class for PyScalarAccessor and PyArrayAccessor
     */
    PyTransferElementBase::bind(m);

    /**
     * ReadAnyGroup
     */
    PyReadAnyGroup::bind(m);

    /**
     * DataConsistencyGroup and DataConsistencyGroup::MatchingMode
     */
    PyDataConsistencyGroup::bind(m);

    /**
     * Logger
     */
    PyLogger::bind(m);

    /**
     * Scalar accessors
     */
    PyScalarAccessor::bind(m);

    /**
     * Array accessors
     */
    PyArrayAccessor::bind(m);

    /**
     * VariableGroup
     */
    PyVariableGroup::bind(m);

    /**
     *  ApplicationModule
     */
    PyApplicationModule::bind(m);

    /**
     *  PyModuleGroup
     */
    PyModuleGroup::bind(m);

    /**
     * ConfigReader
     */
    PyConfigReader::bind(m);

    /*
     * Set the main ModuleGroup as an attribute of the bindings module. This is done with a callback to avoid
     * problems with the symbol visibility (all pybind11 classes are hidden, so we cannot pass them directly between
     * the bindings .so and the main library .so).
     */
    if(Application::hasInstance()) {
      Application::getInstance().getPythonModuleManager().setOnMainGroupChange(
          [m](const std::unique_ptr<PyModuleGroup>& mainGroup) {
            m.attr("app") = py::cast(mainGroup.get(), py::return_value_policy::reference);
          });
    }

    registerUnloadHook<PyApplicationCoreUnload>();
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
