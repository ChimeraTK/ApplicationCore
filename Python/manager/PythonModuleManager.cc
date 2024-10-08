// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <pybind11/embed.h>
// pybind11.h must be included first

#include "Application.h"
#include "ConfigReader.h"
#include "PyModuleGroup.h"
#include "PythonModuleManager.h"

#include <filesystem>
#include <format>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  namespace detail {
    struct __attribute__((visibility("hidden"))) PythonModuleManagerStatics {
      py::scoped_interpreter pyint{false}; // "false" = do not register signal handlers
      py::exception<boost::thread_interrupted> exceptionObject;
      std::function<void(const std::unique_ptr<PyModuleGroup>&)> onMainGroupChangeCallback;
    };

    /******************************************************************************************************************/

    struct __attribute__((visibility("hidden"))) PythonModuleManagerImpl {
      static std::unique_ptr<PythonModuleManagerStatics> statics;
      std::unique_ptr<PyModuleGroup> mainGroup; // this ModuleGroup is presented to Python as "app"
      std::list<py::object> modules; // Python modules loaded (the language construct, *not* ChimeraTK::Module)
      std::unique_ptr<py::gil_scoped_release> release;
    };

    /// \cond suppress_doxygen_warning
    std::unique_ptr<detail::PythonModuleManagerStatics> PythonModuleManagerImpl::statics;
    /// \endcond

  } // namespace detail

  /********************************************************************************************************************/

  void PythonModuleManager::init() {
    if(_impl) {
      return;
    }

    _impl = std::make_unique<detail::PythonModuleManagerImpl>();
    if(!_impl->statics) {
      // Create impl, with scoped_interpreter etc.

      // The impl object outlives the PythonModuleManager, since we need to keep the interpreter instance alive until
      // the end of the process, even across multiple Application instances (e.g. in tests). Otherwise certain Python
      // modules like numpy and datetime lead to crashes when loaded multiple times.
      _impl->statics = std::make_unique<detail::PythonModuleManagerStatics>();

      // boost::thread_interrupted does not have a what() function, so we cannot use py::register_exception() directly
      // on it. Also py::set_error() seems to be unavailable for our version of pybind11, also there is this bug:
      // https://github.com/pybind/pybind11/issues/4967
      // As a work around, we roughly replicated what register_exception_impl() is doing in pybind11 2.11

      // Create the exception object and store it in a static py::object, so the (state-less) lambda passed to
      // register_exception_translator() below can access it. As long as there is only one PythonModuleManager at a time
      // this is perfectly fine.
      _impl->statics->exceptionObject = {py::module::import("__main__"), "ThreadInterrupted"};

      py::register_exception_translator([](std::exception_ptr p) {
        try {
          if(p) std::rethrow_exception(p);
        }
        catch(const boost::thread_interrupted& e) {
          detail::PythonModuleManagerImpl::statics->exceptionObject("Thread Interrupted");
        }
      });

      // Make sure all Python modules are imported that we need
      py::exec("import threading, traceback, sys, gc");
    }

    // create main group object functioning as "app" object on the Python side
    _impl->mainGroup = std::make_unique<PyModuleGroup>(&Application::getInstance(), ".", "Root for Python Modules");

    // If the bindings have been created already set/replace the "app" object with the newly created main group. This
    // happens when a previous instance of the PythonModuleManager in the same process has already loaded Python
    // modules using the ApplicationCore Python bindings. Otherwise the Python bindings are not yet loaded at this
    // point, so this assignment is done later in PythonModuleManager::setBindings().
    if(_impl->statics->onMainGroupChangeCallback) {
      _impl->statics->onMainGroupChangeCallback(_impl->mainGroup);
    }

    // The scoped_interpreter keeps the GIL by default, so we have to release it here and do the locking explicitly.
    // In the destructor of the PythonModuleManager (hence when the Application is being destroyed) we have to
    // acquire the lock again to make sure static objects created internally by pybind11 can be destroyed while
    // having the GIL (would otherwise lead to an error).
    _impl->release = std::make_unique<py::gil_scoped_release>();
  }

  /********************************************************************************************************************/

  PythonModuleManager::PythonModuleManager() = default;

  /********************************************************************************************************************/

  void PythonModuleManager::deinit() {
    if(!_impl) {
      return;
    }

    // terminate all Python ApplicationModule threads
    for(auto* mod : _impl->mainGroup->getSubmoduleListRecursive()) {
      mod->terminate();
    }

    // destroy the gil_scoped_release object, see comment in constructor when creating it
    _impl->release.reset();

    // de-assign the app object (which points to the root module we are about to destroy)
    if(_impl->statics->onMainGroupChangeCallback) {
      _impl->statics->onMainGroupChangeCallback(std::unique_ptr<PyModuleGroup>{nullptr});
    }

    // unload all Python modules, which will destroy all PythonApplicationModules etc. that have been constructed in
    // Python code.
    for(auto& mod : _impl->modules) {
      auto modname = py::cast<std::string>(mod.attr("__name__"));
      py::exec("sys.modules.pop('" + modname + "')");
    }
    _impl->modules.clear();
    py::exec("gc.collect()");
  }

  /********************************************************************************************************************/

  PythonModuleManager::~PythonModuleManager() {
    if(_impl) {
      deinit();
    }
  }

  /********************************************************************************************************************/

  void PythonModuleManager::createModules(Application& app) {
    auto& config = app.getConfigReader();
    for(auto& module : config.getModules("PythonModules")) {
      init();

      auto name = config.get<std::string>("PythonModules/" + module + "/path");
      py::gil_scoped_acquire gil;

      std::cout << "PythonModuleManager: Loading module " << name << std::endl;

      try {
        py::object themod = py::module::import(name.c_str());
        _impl->modules.emplace_back(std::move(themod));
      }
      catch(py::error_already_set& err) {
        throw ChimeraTK::logic_error("Error loading Python module from " + name + ": " + err.what());
      }
    }
  }

  /********************************************************************************************************************/

  void PythonModuleManager::setOnMainGroupChange(std::function<void(const std::unique_ptr<PyModuleGroup>&)> callback) {
    _impl->statics->onMainGroupChangeCallback = std::move(callback);
    _impl->statics->onMainGroupChangeCallback(_impl->mainGroup);
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
