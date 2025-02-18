// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "PyModuleGroup.h"

#include <functional>
#include <memory>

namespace ChimeraTK {

  namespace detail {
    struct PythonModuleManagerImpl;
  }

  /**
   * This class loads and unloads the Python modules as specified in the ConfigReader XML file, and creates the Python
   * interpreter instance if necessary.
   *
   * The Application keeps an instance of the PythonModuleManager and calls createModules().
   */
  class PythonModuleManager {
   public:
    /// need non-default constructor due to incomplete type detail::PythonModuleManagerImpl
    PythonModuleManager();

    /// clean up detail::PythonModuleManagerImpl, in particular py::gil_scoped_release
    void deinit();

    /// need non-default destructor due to incomplete type detail::PythonModuleManagerImpl
    ~PythonModuleManager();

    /// called by Application to load all Python modules specified in the ConfigReader XML file
    void createModules(Application& app);

    /**
     * Register callback function to get informed about the main PyModuleGroup which is created by the
     * PythonModuleManager. This function is called by the Python bindings module (i.e. when loading the first Python
     * module in createModules()). The callback is immediately called, but also later again in subsequent instances
     * of the PythonModuleManager (in case multiple Applications run after each other in the same process, as done in
     * tests). This trick is needed since the call crosses from the PyApplicationCore.cpython....so into the
     * libChimeraTK-ApplicationCore.so and the visibility of all pybind11 classes is hidden.
     */
    void setOnMainGroupChange(std::function<void(const std::unique_ptr<PyModuleGroup>&)> callback);

    /**
     * Initialise the python interpreter without registering modules.
     * This function must only be called while the application is still single threaded (i.e.
     * LifeCycleState == initialisation).
     * It will throw a logig error if this is not the case.
     */
    void init();

   private:
    std::unique_ptr<detail::PythonModuleManagerImpl> _impl;
  };

} // namespace ChimeraTK
