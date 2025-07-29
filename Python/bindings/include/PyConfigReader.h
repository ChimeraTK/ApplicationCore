// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "ConfigReader.h"

#include <ChimeraTK/VariantUserTypes.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  class PyConfigReader {
   public:
    explicit PyConfigReader(ConfigReader& wrappedReader) : _reader(wrappedReader) {}

    // Config reader provides this with two functions, we move it to one single function that has an optional
    // default value, and internally dispatches to either of the two get() functions depending on whether or not that
    // optional was passed in.
    UserTypeVariantNoVoid get(
        ChimeraTK::DataType dt, const std::string& path, std::optional<UserTypeVariantNoVoid> defaultValue);

    template<typename UserType>
    using Vector = std::vector<UserType>;

    // Variant of the above for getting arrays
    UserTypeTemplateVariantNoVoid<Vector> getArray(ChimeraTK::DataType dt, const std::string& path,
        std::optional<UserTypeTemplateVariantNoVoid<Vector>> defaultValue);

    std::list<std::string> getModules(const std::string& path) { return _reader.get().getModules(path); }

    static void bind(py::module& mod);

   private:
    std::reference_wrapper<ConfigReader> _reader;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
