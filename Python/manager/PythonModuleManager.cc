// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <pybind11/embed.h>
// pybind11.h must be included first

#include "Application.h"
#include "ConfigReader.h"
#include "PyModuleGroup.h"
#include "PythonModuleManager.h"
#include "VersionInfo.h"

#include <ChimeraTK/SupportedUserTypes.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace py = pybind11;
using namespace py::literals;

namespace ChimeraTK {

  /********************************************************************************************************************/

  namespace detail {
    struct __attribute__((visibility("hidden"))) PythonModuleManagerStatics {
      py::scoped_interpreter pyint{false}; // "false" = do not register signal handlers
      py::exception<boost::thread_interrupted> exceptionObject;
      std::function<void(const std::unique_ptr<PyModuleGroup>&)> onMainGroupChangeCallback;
      PythonModuleManagerStatics();
    };

    /******************************************************************************************************************/

    /** Information about a single loaded Python module for file monitoring purposes. */
    struct __attribute__((visibility("hidden"))) MonitoredModule {
      std::string moduleName;              ///< The Python module name (e.g. "my_module")
      std::string filePath;                ///< Resolved source file path
      std::size_t fileHash;                ///< Hash of last known file content
      std::vector<py::object> pyInstances; ///< Python wrapper instances created by this module
    };

    /******************************************************************************************************************/

    struct __attribute__((visibility("hidden"))) PythonModuleManagerImpl {
      static std::unique_ptr<PythonModuleManagerStatics> statics;
      std::unique_ptr<PyModuleGroup> mainGroup; // this ModuleGroup is presented to Python as "app"
      std::list<py::object> modules; // Python modules loaded (the language construct, *not* ChimeraTK::Module)
      std::unique_ptr<py::gil_scoped_release> release;
      std::vector<MonitoredModule> monitoredModules;  // file monitoring info for modules with monitoring enabled
      std::unique_ptr<std::thread> fileMonitorThread; // thread for periodic file monitoring
    };

    /// \cond suppress_doxygen_warning
    std::unique_ptr<detail::PythonModuleManagerStatics> PythonModuleManagerImpl::statics;
    /// \endcond

    /******************************************************************************************************************/

    __attribute__((visibility("hidden"))) PythonModuleManagerStatics::PythonModuleManagerStatics() {
      py::gil_scoped_acquire gil;

      auto locals = py::dict("so_version"_a = ChimeraTK::VersionInfo::soVersion);
      py::exec(R"(
        import sys
        import os

        new_paths = []
        for p in sys.path:
          new_paths.append(os.path.join(p, 'ChimeraTK', 'ApplicationCore'+so_version))

        sys.path = new_paths + sys.path # prepend so old system libraries are not found first
      )",
          py::globals(), locals);
    }
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

    // Stop the file monitoring thread first
    _stopFileMonitoring = true;
    if(_impl->fileMonitorThread && _impl->fileMonitorThread->joinable()) {
      _impl->fileMonitorThread->join();
    }
    _impl->fileMonitorThread.reset();

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
    _impl->monitoredModules.clear();
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
    bool anyModuleMonitored = false;
    const auto modules = config.getModules("PythonModules");
    for(auto& module : modules) {
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

      // Check if file monitoring is enabled for this module
      bool fileMonitoringEnabled =
          config.get<ChimeraTK::Boolean>("PythonModules/" + module + "/fileMonitoring", ChimeraTK::Boolean{false});

      if(fileMonitoringEnabled) {
        anyModuleMonitored = true;

        // Resolve the source file path of the loaded module using Python's inspect module
        py::gil_scoped_acquire gilForInspect;
        try {
          py::object inspect = py::module::import("inspect");
          py::object pyModuleObj = _impl->modules.back();
          std::string filePath = py::cast<std::string>(inspect.attr("getfile")(pyModuleObj));

          detail::MonitoredModule monitored;
          monitored.moduleName = name;
          monitored.filePath = filePath;

          // Compute hash of file content
          std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
          std::streamsize size = ifs.tellg();
          ifs.seekg(0, std::ios::beg);
          std::string content(size, '\0');
          ifs.read(&content[0], size);
          monitored.fileHash = std::hash<std::string>{}(content);

          // Find all Python ApplicationModule instances created by this module.
          // Since pybind11 types are module_local, we cannot use py::cast from this library.
          // Instead, we execute Python code to find instances by scanning vars(app) and
          // the module's own namespace for objects that have terminate() and run() methods
          // and whose type's __module__ matches the module name.
          {
            py::dict locals;
            locals["mod_name"] = py::cast(name);
            py::exec(R"(
import sys

app = sys.modules['PyApplicationCore'].app
ApplicationModule = getattr(sys.modules['PyApplicationCore'], 'ApplicationModule', None)

found = []

for attr_name in dir(app):
    try:
        obj = getattr(app, attr_name)
        # Check using isinstance and then verify __module__ matches
        if isinstance(obj, ApplicationModule):
            obj_type = type(obj)
            mod_of_type = getattr(obj_type, '__module__', None)
            if mod_of_type == mod_name:
                found.append(obj)
    except Exception:
        pass

if found:
    sys.modules[mod_name]._chimeraTk_monitored_instances = found
)",
                py::globals(), locals);
          }

          _impl->monitoredModules.emplace_back(std::move(monitored));
        }
        catch(py::error_already_set& err) {
          std::cerr << "Warning: could not resolve source file for Python module " << name << ": " << err.what()
                    << std::endl;
        }
      }
    }

    // Start the file monitoring thread if any module has monitoring enabled
    if(anyModuleMonitored && !_impl->fileMonitorThread) {
      _stopFileMonitoring = false;
      _impl->fileMonitorThread = std::make_unique<std::thread>(&PythonModuleManager::fileMonitoringThread, this);
    }
  }

  /********************************************************************************************************************/

  /********************************************************************************************************************/

  // No detail namespace helpers needed currently - reload logic is entirely in Python-side exec.

  /********************************************************************************************************************/

  void PythonModuleManager::fileMonitoringThread() {
    while(!_stopFileMonitoring) {
      std::this_thread::sleep_for(_fileMonitoringCheckInterval);

      if(_stopFileMonitoring) {
        break;
      }

      // Check each monitored module for changes
      bool reloadedThisIteration = false;

      {
        py::gil_scoped_acquire gil;

        for(size_t i = 0; i < _impl->monitoredModules.size(); ++i) {
          auto& monitored = _impl->monitoredModules[i];

          // Check if the source file has been modified by comparing content hash
          std::error_code ec;
          std::ifstream ifs(monitored.filePath, std::ios::binary | std::ios::ate);
          if(!ifs) {
            // File might have been deleted or become inaccessible - skip
            continue;
          }
          std::streamsize size = ifs.tellg();
          ifs.seekg(0, std::ios::beg);
          std::string content(size, '\0');
          ifs.read(&content[0], size);
          auto currentHash = std::hash<std::string>{}(content);

          if(currentHash == monitored.fileHash) {
            continue; // No change
          }

          // File was modified - reload the module.
          //
          // Strategy: Extract the mainLoop method from the source file using Python's AST,
          // compile it as a standalone function, and attach it as a bound method to the
          // existing Python wrapper objects. This avoids re-executing __init__ or creating
          // new C++ wrappers (which would fail because the PVs / model entries already exist).
          //
          // The existing C++ ApplicationModule wrappers and their accessors are kept alive
          // and reused.

          std::cout << "PythonModuleManager: Reloading module " << monitored.moduleName << std::endl;

          try {
            std::string modName = monitored.moduleName;

            // Execute the entire reload logic from Python to avoid cross-library
            // pybind11 type lookup issues (all C++ bindings are module_local).
            //
            // Strategy:
            //   1. Retrieve the stored Python ApplicationModule instances from the
            //      module's _chimeraTk_monitored_instances attribute (stored at load time).
            //   2. Stop each instance's module thread by interrupting accessors and joining.
            //   3. Extract the mainLoop method from the source file using AST,
            //      compile it, and attach as a bound method to each existing instance.
            //   4. Start a new Python thread directly (bypassing C++ run() to avoid
            //      testable mode lock contention from a background thread).
            //
            // Note: We do NOT call inst.terminate() / inst.run() because those methods
            // acquire/release the testable mode lock in ways that conflict when called
            // from a background thread (the file monitoring thread) while the main test
            // thread is in stepApplication(). Instead, we manage the Python threads
            // directly from this exec block.
            {
              py::dict locals;
              locals["mod_name"] = py::cast(modName);
              locals["filepath"] = py::cast(monitored.filePath);
              py::exec(R"(
import ast, types, sys, threading, time, functools

# Step 1: Retrieve the stored instances
instances = []
if mod_name in sys.modules:
    mod_obj = sys.modules[mod_name]
    if hasattr(mod_obj, '_chimeraTk_monitored_instances'):
        instances = list(mod_obj._chimeraTk_monitored_instances)

if not instances:
    # Fallback: scan PyApplicationCore.app for instances using isinstance
    ApplicationModule = getattr(sys.modules['PyApplicationCore'], 'ApplicationModule', None)
    if ApplicationModule:
        app = sys.modules['PyApplicationCore'].app
        for attr_name in dir(app):
            try:
                obj = getattr(app, attr_name)
                if isinstance(obj, ApplicationModule):
                    obj_type = type(obj)
                    mod = getattr(obj_type, '__module__', None)
                    if mod == mod_name:
                        instances.append(obj)
            except:
                pass

# Step 2: Stop all existing module threads
for inst in instances:
    thread = getattr(inst, '_chimeraTk_thread', None)
    if thread is not None and thread.is_alive():
        # stopAndDrainThread() handles: interrupt -> retry-join -> drain stale exceptions
        inst.stopAndDrainThread()

# Step 3: Extract mainLoop from source file
with open(filepath, 'r') as f:
    source = f.read()

tree = ast.parse(source, filepath)

# Find the first class with a 'mainLoop' method
mainloop_node = None
for node in ast.walk(tree):
    if isinstance(node, ast.ClassDef):
        for item in node.body:
            if isinstance(item, ast.FunctionDef) and item.name == 'mainLoop':
                mainloop_node = item
                break
        if mainloop_node:
            break

if mainloop_node is None:
    raise RuntimeError('No mainLoop method found in ' + filepath)

# Rebuild the function
new_func_node = ast.FunctionDef(
    name='mainLoop',
    args=mainloop_node.args,
    body=mainloop_node.body,
    decorator_list=mainloop_node.decorator_list,
    returns=mainloop_node.returns,
)
ast.copy_location(new_func_node, mainloop_node)

mod_ast = ast.Module(body=[new_func_node], type_ignores=[])
ast.fix_missing_locations(mod_ast)

code = compile(mod_ast, filepath, 'exec')
ns = {}
exec(code, ns)
new_mainloop_func = ns['mainLoop']

# Step 4: Define a wrapper function that handles lock/thread management
# and calls the new mainLoop.
def _run_new_mainloop(module, mainloop_func):
    import sys, traceback, threading, time
    # Register thread name (same pattern as C++ mainLoopWrapper)
    current = threading.current_thread()
    current.name = 'AM_' + type(module).__name__

    try:
        # Acquire shared testable mode lock
        module._obtainTestableModeLock()
        # Call the new mainLoop directly
        mainloop_func(module)
    except ThreadInterrupted:
        pass
    except BaseException as e:
        sys.stderr.write('Exception in module ' + module.getName() + ':\\n')
        traceback.print_exception(e)
    finally:
        try:
            module._releaseTestableModeLock()
        except:
            pass

# Step 5: Start new module threads directly (bypassing C++ run()).
for inst in instances:
    t = threading.Thread(
        target=functools.partial(_run_new_mainloop, inst, new_mainloop_func),
        name='AM_' + type(inst).__name__,
    )
    t.start()
    inst._chimeraTk_thread = t
)",
                  py::globals(), locals);
            }

            std::cout << "PythonModuleManager: Reloaded module " << monitored.moduleName << " successfully"
                      << std::endl;

            // Update the stored file hash
            monitored.fileHash = currentHash;
            reloadedThisIteration = true;
          }
          catch(py::error_already_set& err) {
            std::cerr << "Error reloading Python module " << monitored.moduleName << ": " << err.what() << std::endl;
          }
        }
      }

      // GIL is released here (py::gil_scoped_acquire went out of scope).
      // If we reloaded any module, give the new Python threads time to start
      // and reach their blocking reads before continuing. Without this delay,
      // a test thread calling stepApplication() might find no module thread
      // consuming data, causing stepApplication to wait forever for _counter
      // to reach zero.
      if(reloadedThisIteration) {
        // Give the new module threads time to start and reach their blocking reads.
        // Without this delay, a test thread calling stepApplication() might find no
        // module thread consuming data, causing stepApplication to wait forever for
        // _counter to reach zero.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }
  }

  /********************************************************************************************************************/

  void PythonModuleManager::setOnMainGroupChange(std::function<void(const std::unique_ptr<PyModuleGroup>&)> callback) {
    _impl->statics->onMainGroupChangeCallback = std::move(callback);
    _impl->statics->onMainGroupChangeCallback(_impl->mainGroup);
  }

  /********************************************************************************************************************/

  void PythonModuleManager::setFileMonitoringCheckInterval(std::chrono::milliseconds interval) {
    _fileMonitoringCheckInterval = interval;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
