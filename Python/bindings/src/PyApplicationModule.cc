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
              print("mainLoop terminated in module "+self.getName())
            except ThreadInterrupted as e:
              print("ThreadInterrupted received in module "+self.getName())
              pass
            except Exception as e:
              print("Exception in module "+self.getName()+":")
              traceback.print_exception(e)
          theModule.mainLoopWrapper2 = mainLoopWrapper2.__get__(theModule)
          theModule._chimeraTk_thread = threading.Thread(target=theModule.mainLoopWrapper2, name='AM_' + type(theModule).__name__)
        )",
          py::globals(), locals);
      _myThread = locals["theModule"].attr("_chimeraTk_thread");
      Application::getInstance().getTestableMode().unlock("releaseForPythonModuleStart");
      _myThread.attr("start")();
    }
    // must release GIL before acquiring testable mode lock, to avoid deadlock
    // lock must be exclusive, since this is executed in the main thread (= test thread). The module thread is launched
    // above in the py::eval() call.
    Application::getInstance().getTestableMode().lock("acquireForPythonModuleStart", false);
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

  void PyApplicationModule::interruptAllAccessors() {
    for(auto& var : getAccessorListRecursive()) {
      auto el{var.getAppAccessorNoType().getHighLevelImplElement()};
      el->interrupt();
    }
  }

  void PyApplicationModule::interruptAndClearAllAccessors() {
    for(auto& var : getAccessorListRecursive()) {
      // Only interrupt inputs that use blocking reads (wait_for_new_data).
      if(var.getDirection().dir == VariableDirection::consuming) {
        auto el{var.getAppAccessorNoType().getHighLevelImplElement()};
        if(el->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
          el->interrupt();
        }
        // Poll-type inputs (without wait_for_new_data) do not block on the queue.
        // Their doReadTransferSynchronously() uses non-blocking pop() after the first read.
        // Pushing an exception to their queue would only cause problems since there is no
        // way to drain it without either blocking (pop_wait) or infinite-looping
        // (readNonBlocking always returns true for poll inputs).
      }
    }
  }

  /********************************************************************************************************************/

  void PyApplicationModule::drainThreadInterrupted() {
    for(auto& var : getAccessorListRecursive()) {
      if(var.getDirection().dir == VariableDirection::consuming) {
        auto el{var.getAppAccessorNoType().getHighLevelImplElement()};

        // Only drain push-type inputs (wait_for_new_data). Poll-type inputs never receive
        // thread_interrupted exceptions (interruptAndClearAllAccessors() skips them), and
        // readNonBlocking() on poll inputs always returns true which would cause an infinite
        // loop.
        if(!el->getAccessModeFlags().has(AccessMode::wait_for_new_data)) {
          continue;
        }

        // Drain any stale thread_interrupted exceptions from the accessor's queue.
        // After interrupt() wakes the old module thread, it consumes one exception and
        // exits. But if the module has multiple consuming accessors, the interrupt() calls
        // on the other accessors leave stale exceptions. The new module thread would throw
        // on its first readNonBlocking()/readLatest() call on those accessors.
        //
        // IMPORTANT: readNonBlocking() goes through the TestableMode decorator chain.
        // For TestableMode-decorated accessors, doPostRead calls obtainLockAndDecrementCounter()
        // which acquires the shared testable mode lock but never releases it (releaseLock()
        // is only called in doPreRead for TransferType::read). Since this function is called
        // from the file monitoring thread (which should NOT hold testable mode locks), we
        // must explicitly release any lock acquired during draining.
        try {
          while(el->readNonBlocking()) {
            // Discard any stale data/exceptions
          }
        }
        catch(...) {
          // Exceptions are expected (thread_interrupted) and should be discarded
        }
      }
    }

    // Release any testable mode shared lock that was acquired during drain operations.
    // drainThreadInterrupted() is called from the file monitoring thread, which should
    // never hold testable mode locks. But readNonBlocking() through the TestableMode
    // decorator's doPostRead acquires the shared lock and doesn't release it.
    auto& tm = Application::getInstance().getTestableMode();
    if(tm.isEnabled() && tm.testLock()) {
      tm.unlock("drainThreadInterrupted");
    }
  }

  /********************************************************************************************************************/

  bool PyApplicationModule::stopAndDrainThread() {
    if(!_myThread) {
      return true;
    }

    py::gil_scoped_acquire gil;
    if(!_myThread.attr("is_alive")().cast<bool>()) {
      return true;
    }

    // Step 1: Interrupt all push-type input accessors to wake the old thread
    interruptAndClearAllAccessors();

    // Step 2: Wait for the old thread to exit with retries
    static constexpr int maxRetries = 100; // 100 * 100ms = 10s total
    for(int i = 0; i < maxRetries; ++i) {
      if(!_myThread.attr("is_alive")().cast<bool>()) {
        break; // Thread exited
      }
      // Re-interrupt in case the thread was blocked on a different read
      interruptAndClearAllAccessors();
      _myThread.attr("join")(0.1); // 100ms timeout
    }

    // Step 3: Check if the thread is still alive
    if(_myThread.attr("is_alive")().cast<bool>()) {
      // Thread did not exit in time
      py::print("Warning: Module thread for '" + getName() + "' did not exit within timeout. Proceeding with drain.");
      return false;
    }

    // Step 4: Drain any stale thread_interrupted exceptions
    drainThreadInterrupted();

    return true;
  }

  /********************************************************************************************************************/

  void PyApplicationModule::_obtainTestableModeLock() {
    Application::getInstance().getTestableMode().lock("python_reload", true);
  }

  /********************************************************************************************************************/

  void PyApplicationModule::_releaseTestableModeLock() {
    Application::getInstance().getTestableMode().unlock("python_reload");
  }

  /********************************************************************************************************************/

  void PyApplicationModule::bind(py::module& m) {
    // on Python side, PythonApplicationModule derives from PyVariableGroup although on C++ side, it does not.
    // So we specify inheritance on Python side by constructor args.
    py::class_<PyApplicationModule, PythonApplicationModuleTrampoline, VariableGroup,
        std::unique_ptr<PyApplicationModule, py::nodelete>>
        cam(m, "ApplicationModule", py::multiple_inheritance(), py::module_local());

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
        .def(
            "disable", &PyApplicationModule::disable, "Disable the module such that it is not part of the Application.")
        .def("run", &PyApplicationModule::run, "Run the module's mainLoop in a separate thread.")
        .def("terminate", &PyApplicationModule::terminate, "Terminate the module's execution thread.")
        .def("interruptAllAccessors", &PyApplicationModule::interruptAllAccessors,
            "Interrupt all accessors to wake the module from blocking reads.")
        .def("interruptAndClearAllAccessors", &PyApplicationModule::interruptAndClearAllAccessors,
            "Interrupt all consuming accessors to wake the module from blocking reads.")
        .def("drainThreadInterrupted", &PyApplicationModule::drainThreadInterrupted,
            "Drain any stale thread_interrupted exceptions from all consuming accessors. Must be called after the "
            "old module thread has exited, before starting a new thread.")
        .def("stopAndDrainThread", &PyApplicationModule::stopAndDrainThread,
            "Interrupt the module thread (if alive), wait for it to exit with retries, and drain any stale "
            "thread_interrupted exceptions. Returns True if the thread was stopped successfully, False if it "
            "timed out.")
        .def("_obtainTestableModeLock", &PyApplicationModule::_obtainTestableModeLock,
            "Obtain the testable mode lock (shared) for the current thread.")
        .def("_releaseTestableModeLock", &PyApplicationModule::_releaseTestableModeLock,
            "Release the testable mode lock for the current thread.");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
