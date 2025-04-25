// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <pybind11/embed.h>
// pybind11 includes should come first

#include "Application.h"
#include "PyApplicationModule.h"
#include "PyConfigReader.h"
#include "PyLogger.h"

#include <pybind11/stl.h>

#include <iostream>
#include <map>
#include <string>

namespace py = pybind11;
using namespace py::literals;

namespace ChimeraTK {

  /********************************************************************************************************************/

  void PyApplicationModule::run() {
    {
      py::gil_scoped_acquire gil;

      auto locals = py::dict("theModule"_a = *this);
      py::exec(R"(
          def mainLoopWrapper2(self):
            try:
              self.mainLoopWrapper()
            except ThreadInterrupted as e:
              pass
            except Exception as e:
              print("Exception in module "+self.getName()+":")
              traceback.print_exception(e)
          theModule.mainLoopWrapper2 = mainLoopWrapper2.__get__(theModule)
        )",
          py::globals(), locals);
      _myThread = py::eval("threading.Thread(target=theModule.mainLoopWrapper2)", py::globals(), locals);
      Application::getInstance().getTestableMode().unlock("releaseForPythonModuleStart");
      _myThread.attr("start")();
    }
    // must release GIL before acquiring testable mode lock, to avoid deadlock
    Application::getInstance().getTestableMode().lock("acquireForPythonModuleStart", true);
  }

  /********************************************************************************************************************/

  void PyApplicationModule::terminate() {
    ApplicationModule::terminate();

    // The module was not started
    if(!_myThread) {
      return;
    }

    py::gil_scoped_acquire gil;
    while(_myThread.attr("is_alive")().cast<bool>()) {
      for(auto& var : getAccessorListRecursive()) {
        auto el{var.getAppAccessorNoType().getHighLevelImplElement()};
        el->interrupt();
      }

      _myThread.attr("join")(0.01);
    }
  }

  /********************************************************************************************************************/

  void PyApplicationModule::bind(py::module& m) {
    // on Python side, PythonApplicationModule derives from PyVariableGroup although on C++ side, it does not.
    // So we specify inheritance on Python side by constructor args.
    py::class_<PyApplicationModule, PythonApplicationModuleTrampoline, VariableGroup,
        std::unique_ptr<PyApplicationModule, py::nodelete>>
        cam(m, "ApplicationModule", py::multiple_inheritance());

    cam.def(py::init([](ModuleGroup& owner, const std::string& name, const std::string& description,
                         const std::unordered_set<std::string>& tags) {
      return dynamic_cast<PyOwningObject&>(owner).make_child<PythonApplicationModuleTrampoline>(
          &owner, name, description, tags);
    }),
        py::return_value_policy::reference,
        // doc and default args:
        "", py::arg("owner"), py::arg("name"), py::arg("description"),
        py::arg("tags") = std::unordered_set<std::string>{});

    cam.def("mainLoopWrapper", &PyApplicationModule::mainLoopWrapper, py::call_guard<py::gil_scoped_release>())
        .def("appConfig",
            []([[maybe_unused]] PyApplicationModule& pam) { return PyConfigReader(PyApplicationModule::appConfig()); })
        .def("getDataValidity", &PyApplicationModule::getDataValidity,
            "Return the data validity flag.\n\nIf any This function will be called by all output accessors in their "
            "write functions.")
        .def("getCurrentVersionNumber", &PyApplicationModule::getCurrentVersionNumber,
            "Return the current version number which has been received with the last push-type read operation.")
        .def("setCurrentVersionNumber", &PyApplicationModule::setCurrentVersionNumber,
            "Set the current version number.\n\nThis function is called by the push-type input accessors in their read "
            "functions.",
            py::arg("versionNumber"))
        // TODO: Bind Model .def("getModel", &PythonApplicationModule::getModel)
        .def("incrementDataFaultCounter", &PyApplicationModule::incrementDataFaultCounter,
            "Set the data validity flag to fault and increment the fault counter.\n\nThis function will be called by "
            "all input accessors when receiving the a faulty update if the previous update was ok. The caller of this "
            "function must ensure that calls to this function are paired to a subsequent call to "
            "decrementDataFaultCounter().")
        .def("decrementDataFaultCounter", &PyApplicationModule::decrementDataFaultCounter,
            "Decrement the fault counter and set the data validity flag to ok if the counter has reached 0.\n\nThis "
            "function will be called by all input accessors when receiving the an ok update if the previous update was "
            "faulty. The caller of this function must ensure that calles to this function are paired to a previous "
            "call to incrementDataFaultCounter().")
        .def("getDataFaultCounter", &PyApplicationModule::getDataFaultCounter,
            "Get the Number of inputs which report DataValidity::faulty.")
        .def(
            "logger",
            [](PyApplicationModule& module, Logger::Severity severity) {
              // Get name of python implementation, not the C++ class, which would be PythonApplicationModule
              auto pythonClassName = py::cast(&module).get_type().attr("__name__").cast<std::string>();
              return PyLoggerStreamProxy(severity, pythonClassName);
            },
            "Convenicene function to obtain a logger stream with the given Severity.\n\nThe context string will be "
            "obtained from the class name of the module.")
        .def("disable", &PyApplicationModule::disable,
            "Disable the module such that it is not part of the Application.");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
