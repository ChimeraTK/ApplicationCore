// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyScalarAccessor.h"

#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  template<template<typename> class AccessorType>
  UserTypeTemplateVariantNoVoid<ScalarAccessor> PyScalarAccessor::createAccessor(ChimeraTK::DataType type,
      Module* owner, const std::string& name, const std::string& unit, const std::string& description,
      const std::unordered_set<std::string>& tags) {
    std::optional<UserTypeTemplateVariantNoVoid<ScalarAccessor>> rv;
    ChimeraTK::callForTypeNoVoid(type, [&](auto t) {
      using UserType = decltype(t);
      AccessorType<UserType> acc(owner, name, unit, description, tags);
      rv.emplace(std::in_place_type<ScalarAccessor<UserType>>, std::move(acc));
    });
    return std::move(rv.value());
  }

  /********************************************************************************************************************/

  PyScalarAccessor::PyScalarAccessor() : _accessor(ScalarOutput<int>()) {}

  /********************************************************************************************************************/

  template<template<typename> class AccessorType>
  PyScalarAccessor::PyScalarAccessor(AccessorTypeTag<AccessorType>, ChimeraTK::DataType type, Module* owner,
      const std::string& name, const std::string& unit, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : _accessor(createAccessor<AccessorType>(type, owner, name, unit, description, tags)) {}

  /********************************************************************************************************************/

  PyScalarAccessor::~PyScalarAccessor() = default;

  /********************************************************************************************************************/

  UserTypeVariantNoVoid PyScalarAccessor::readAndGet() {
    std::optional<UserTypeVariantNoVoid> rv;
    py::gil_scoped_release release;
    std::visit([&](auto& acc) { rv = acc.readAndGet(); }, _accessor);
    return rv.value();
  }

  /********************************************************************************************************************/

  UserTypeVariantNoVoid PyScalarAccessor::get() const {
    std::optional<UserTypeVariantNoVoid> rv;
    std::visit(
        [&](auto& acc) {
          using ACC = typename std::remove_reference<decltype(acc)>::type;
          using expectedUserType = typename ACC::value_type;
          rv = expectedUserType(
              boost::dynamic_pointer_cast<NDRegisterAccessor<expectedUserType>>(acc.getHighLevelImplElement())
                  ->accessData(0));
        },
        _accessor);
    return rv.value();
  }

  /********************************************************************************************************************/

  void PyScalarAccessor::writeIfDifferent(UserTypeVariantNoVoid val) {
    std::visit(
        [&](auto& acc) {
          using ACC = typename std::remove_reference<decltype(acc)>::type;
          using expectedUserType = typename ACC::value_type;
          std::visit(
              [&](auto value) { acc.writeIfDifferent(ChimeraTK::userTypeToUserType<expectedUserType>(value)); }, val);
        },
        _accessor);
  }

  /********************************************************************************************************************/

  void PyScalarAccessor::setAndWrite(UserTypeVariantNoVoid val) {
    std::visit(
        [&](auto& acc) {
          using ACC = typename std::remove_reference<decltype(acc)>::type;
          using expectedUserType = typename ACC::value_type;
          std::visit([&](auto value) { acc.setAndWrite(ChimeraTK::userTypeToUserType<expectedUserType>(value)); }, val);
        },
        _accessor);
  }

  /********************************************************************************************************************/

  void PyScalarAccessor::set(UserTypeVariantNoVoid val) {
    std::visit(
        [&](auto& acc) {
          using ACC = typename std::remove_reference<decltype(acc)>::type;
          using expectedUserType = typename ACC::value_type;
          std::visit([&](auto value) { acc = ChimeraTK::userTypeToUserType<expectedUserType>(value); }, val);
        },
        _accessor);
  }

  /********************************************************************************************************************/

  std::string PyScalarAccessor::repr(py::object& acc) {
    if(not acc.cast<PyScalarAccessor&>().getTE().isInitialised()) {
      return "<ScalarAccessor(not initialized)>";
    }

    std::string rep{"<ScalarAccessor(type="};
    rep.append(py::cast<py::object>(py::cast(&acc).attr("getValueType")()).attr("__repr__")().cast<std::string>());
    rep.append(", name=");
    rep.append(py::cast(&acc).attr("getName")().cast<std::string>());
    rep.append(", data=");
    rep.append(py::cast<py::object>(py::cast(&acc).attr("__str__")()).cast<std::string>());
    rep.append(", versionNumber=");
    rep.append(py::cast<py::object>(py::cast(&acc).attr("getVersionNumber")()).attr("__repr__")().cast<std::string>());
    rep.append(", dataValidity=");
    rep.append(py::cast<py::object>(py::cast(&acc).attr("dataValidity")()).attr("__repr__")().cast<std::string>());
    rep.append(")>");
    return rep;
  }

  /********************************************************************************************************************/

  void PyScalarAccessor::bind(py::module& m) {
    // strictly speaking, py::nodelete is not necessary since we hand out instances only via factory function,
    // but leave it here for consistency
    py::class_<PyScalarAccessor, PyTransferElementBase, std::unique_ptr<PyScalarAccessor, py::nodelete>> scalaracc(
        m, "ScalarAccessor", py::buffer_protocol());
    scalaracc.def(py::init<>())
        .def("read", &PyScalarAccessor::read,
            "Read the data from the device.\n\nIf AccessMode::wait_for_new_data was set, this function will block "
            "until new data has arrived. Otherwise it still might block for a short time until the data transfer was "
            "complete.")
        .def("readNonBlocking", &PyScalarAccessor::readNonBlocking,
            "Read the next value, if available in the input buffer.\n\nIf AccessMode::wait_for_new_data was set, this "
            "function returns immediately and the return value indicated if a new value was available (true) or not "
            "(false).\n\nIf AccessMode::wait_for_new_data was not set, this function is identical to read(), which "
            "will still return quickly. Depending on the actual transfer implementation, the backend might need to "
            "transfer data to obtain the current value before returning. Also this function is not guaranteed to be "
            "lock free. The return value will be always true in this mode.")
        .def("readLatest", &PyScalarAccessor::readLatest,
            "Read the latest value, discarding any other update since the last read if present.\n\nOtherwise this "
            "function is identical to readNonBlocking(), i.e. it will never wait for new values and it will return "
            "whether a new value was available if AccessMode::wait_for_new_data is set.")
        .def("getName", &PyScalarAccessor::getName, "Returns the name that identifies the process variable.")
        .def("getUnit", &PyScalarAccessor::getUnit,
            "Returns the engineering unit.\n\nIf none was specified, it will default to ' n./ a.'")
        .def("getDescription", &PyScalarAccessor::getDescription, "Returns the description of this variable/register.")
        .def("getValueType", &PyScalarAccessor::getValueType,
            "Returns the std::type_info for the value type of this transfer element.\n\nThis can be used to determine "
            "the type at runtime.")
        .def("getVersionNumber", &PyScalarAccessor::getVersionNumber,
            "Returns the version number that is associated with the last transfer (i.e. last read or write)")
        .def("isReadOnly", &PyScalarAccessor::isReadOnly,
            "Check if transfer element is read only, i.e. it is readable but not writeable.")
        .def("isReadable", &PyScalarAccessor::isReadable, "Check if transfer element is readable.")
        .def("isWriteable", &PyScalarAccessor::isWriteable, "Check if transfer element is writeable.")
        .def("getId", &PyScalarAccessor::getId,
            "Obtain unique ID for the actual implementation of this TransferElement.\n\nThis means that e.g. two "
            "instances of ScalarRegisterAccessor created by the same call to Device::getScalarRegisterAccessor() (e.g. "
            "by copying the accessor to another using NDRegisterAccessorBridge::replace()) will have the same ID, "
            "while two instances obtained by to difference calls to Device::getScalarRegisterAccessor() will have a "
            "different ID even when accessing the very same register.")
        .def("dataValidity", &PyScalarAccessor::dataValidity,
            "Return current validity of the data.\n\nWill always return DataValidity.ok if the backend does not "
            "support it")
        .def("get", &PyScalarAccessor::get, "Return a value of UserType (without a previous read).")
        .def(
            "readAndGet", &PyScalarAccessor::readAndGet, "Convenience function to read and return a value of UserType.")
        .def("set", &PyScalarAccessor::set, "Set the value of UserType.", py::arg("val"))
        .def("write", &PyScalarAccessor::write,
            "Write the data to device.\n\nThe return value is true, old data was lost on the write transfer (e.g. due "
            "to an buffer overflow). In case of an unbuffered write transfer, the return value will always be false.")
        .def("writeDestructively", &PyScalarAccessor::writeDestructively,
            "Just like write(), but allows the implementation to destroy the content of the user buffer in the "
            "process.\n\nThis is an optional optimisation, hence there is a default implementation which just calls "
            "the normal doWriteTransfer(). In any case, the application must expect the user buffer of the "
            "TransferElement to contain undefined data after calling this function.")
        .def("writeIfDifferent", &PyScalarAccessor::writeIfDifferent,
            "Convenience function to set and write new value if it differes from the current value.\n\nThe given "
            "version number is only used in case the value differs. If versionNumber == {nullptr}, a new version "
            "number is generated only if the write actually takes place.",
            py::arg("val"))
        .def("setAndWrite", &PyScalarAccessor::setAndWrite,
            "Convenience function to set and write new value.\n\nThe given version number. If versionNumber == {}, a "
            "new version number is generated.",
            py::arg("val"))
        .def("__repr__", &PyScalarAccessor::repr);

    for(const auto& fn : PyTransferElementBase::specialFunctionsToEmulateNumeric) {
      scalaracc.def(fn.c_str(), [fn](PyScalarAccessor& acc, PyScalarAccessor& other) {
        return py::cast<py::object>(py::cast(acc).attr("get")()).attr(fn.c_str())(py::cast(other).attr("get")());
      });
      scalaracc.def(fn.c_str(), [fn](PyScalarAccessor& acc, py::object& other) {
        return py::cast<py::object>(py::cast(acc).attr("get")()).attr(fn.c_str())(other);
      });
    }

    for(const auto& fn : PyTransferElementBase::specialUnaryFunctionsToEmulateNumeric) {
      scalaracc.def(fn.c_str(),
          [fn](PyScalarAccessor& acc) { return py::cast<py::object>(py::cast(acc).attr("get")()).attr(fn.c_str())(); });
    }

    /**
     *  ScalarPushInput
     */
    m.def(
        "ScalarPushInput",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyScalarAccessor>(
              AccessorTypeTag<ScalarPushInput>{}, type, &owner, name, unit, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ScalarPushInputWB
     */
    m.def(
        "ScalarPushInputWB",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyScalarAccessor>(
              AccessorTypeTag<ScalarPushInputWB>{}, type, &owner, name, unit, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ScalarPollInput
     */
    m.def(
        "ScalarPollInput",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyScalarAccessor>(
              AccessorTypeTag<ScalarPollInput>{}, type, &owner, name, unit, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ScalarOutput
     */
    m.def(
        "ScalarOutput",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyScalarAccessor>(
              AccessorTypeTag<ScalarOutput>{}, type, &owner, name, unit, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ScalarOutputPushRB
     */
    m.def(
        "ScalarOutputPushRB",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyScalarAccessor>(
              AccessorTypeTag<ScalarOutputPushRB>{}, type, &owner, name, unit, description);
        },
        py::return_value_policy::reference, "");

    /**
     * ScalarOutputReverseRecovery
     */
    m.def(
        "ScalarOutputReverseRecovery",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyScalarAccessor>(
              AccessorTypeTag<ScalarOutputReverseRecovery>{}, type, &owner, name, unit, description);
        },
        py::return_value_policy::reference, "");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
