// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "ApplicationModule.h"
#include "PyOwnershipManagement.h"

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyApplicationModule : public ApplicationModule, public PyOwningObject {
   public:
    using ApplicationModule::ApplicationModule;

    void run() override;
    void terminate() override;

    // change visibility since we need to call this from Python
    using ApplicationModule::mainLoopWrapper;

    static void bind(py::module& mod);

   private:
    py::object _myThread;
  };

  /********************************************************************************************************************/

  class PythonApplicationModuleTrampoline : public PyApplicationModule {
   public:
    using PyApplicationModule::PyApplicationModule;

    void mainLoop() override {
      PYBIND11_OVERRIDE_PURE(void, /* Return type */
          PyApplicationModule,     /* Parent class */
          mainLoop                 /* Name of function in C++ (must match Python name) */
      );
    }

    void prepare() override { PYBIND11_OVERRIDE(void, PyApplicationModule, prepare); }
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
