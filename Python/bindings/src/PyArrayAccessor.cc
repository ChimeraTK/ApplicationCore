// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyArrayAccessor.h"

#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  template<template<typename> class AccessorType>
  UserTypeTemplateVariantNoVoid<ArrayAccessor> PyArrayAccessor::createAccessor(ChimeraTK::DataType type, Module* owner,
      const std::string& name, std::string unit, size_t nElements, const std::string& description,
      const std::unordered_set<std::string>& tags) {
    std::optional<UserTypeTemplateVariantNoVoid<ArrayAccessor>> rv;
    ChimeraTK::callForTypeNoVoid(type, [&](auto t) {
      using UserType = decltype(t);
      AccessorType<UserType> acc(owner, name, unit, nElements, description, tags);
      rv.emplace(std::in_place_type<ArrayAccessor<UserType>>, std::move(acc));
    });
    return std::move(rv.value());
  }

  /********************************************************************************************************************/

  py::object PyArrayAccessor::readAndGet() {
    read();
    return get();
  }

  /********************************************************************************************************************/

  void PyArrayAccessor::setAndWrite(const UserTypeTemplateVariantNoVoid<Vector>& vec) {
    set(vec);
    write();
  }
  /********************************************************************************************************************/

  size_t PyArrayAccessor::getNElements() {
    size_t rv;
    std::visit([&](auto& acc) { rv = acc.getNElements(); }, _accessor);
    return rv;
  }
  /********************************************************************************************************************/

  void PyArrayAccessor::set(const UserTypeTemplateVariantNoVoid<Vector>& vec) {
    std::visit(
        [&](auto& acc) {
          using ACC = typename std::remove_reference<decltype(acc)>::type;
          using expectedUserType = typename ACC::value_type;
          std::visit(
              [&](const auto& vector) {
                std::vector<expectedUserType> converted(vector.size());
                std::transform(vector.begin(), vector.end(), converted.begin(),
                    [](auto v) { return userTypeToUserType<expectedUserType>(v); });
                acc = converted;
              },
              vec);
        },
        _accessor);
  }
  /********************************************************************************************************************/

  py::object PyArrayAccessor::get() const {
    py::object rv;
    std::visit(
        [&](auto& acc) {
          using ACC = typename std::remove_reference<decltype(acc)>::type;
          using userType = typename ACC::value_type;
          auto ndacc = boost::dynamic_pointer_cast<NDRegisterAccessor<userType>>(acc.getHighLevelImplElement());
          if constexpr(std::is_same<userType, std::string>::value) {
            // String arrays are not really supported by numpy, so we return a list instead
            rv = py::cast(ndacc->accessChannel(0));
          }
          else {
            auto ary = py::array(py::dtype::of<userType>(), {acc.getNElements()}, {sizeof(userType)},
                ndacc->accessChannel(0).data(), py::cast(this));
            assert(!ary.owndata()); // numpy must not own our buffers
            rv = ary;
          }
        },
        _accessor);
    return rv;
  }
  /********************************************************************************************************************/

  py::object PyArrayAccessor::getitem(size_t index) const {
    py::object rv;
    std::visit([&](auto& acc) { rv = py::cast(acc[index]); }, _accessor);
    return rv;
  }

  /********************************************************************************************************************/

  void PyArrayAccessor::setitem(size_t index, const UserTypeVariantNoVoid& val) {
    std::visit(
        [&](auto& acc) {
          std::visit(
              [&](auto& v) {
                acc[index] = userTypeToUserType<typename std::remove_reference<decltype(acc)>::type::value_type>(v);
              },
              val);
        },
        _accessor);
  }

  /********************************************************************************************************************/

  void PyArrayAccessor::setslice(const py::slice& slice, const UserTypeVariantNoVoid& val) {
    std::visit(
        [&](auto& acc) {
          std::visit(
              [&](auto& v) {
                size_t start, stop, step, length;
                if(!slice.compute(acc.getNElements(), &start, &stop, &step, &length)) {
                  throw pybind11::error_already_set();
                }

                auto value = userTypeToUserType<typename std::remove_reference<decltype(acc)>::type::value_type>(v);
                for(size_t i = start; i < stop; i += step) {
                  acc[i] = value;
                }
              },
              val);
        },
        _accessor);
  }

  /********************************************************************************************************************/

  std::string PyArrayAccessor::repr(py::object& acc) {
    if(not acc.cast<PyArrayAccessor&>().getTE().isInitialised()) {
      return "<ArrayAccessor(not initialized)>";
    }

    std::string rep{"<ArrayAccessor(type="};
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

  py::buffer_info PyArrayAccessor::getBufferInfo() {
    py::buffer_info info;
    std::visit(
        [&](auto& acc) {
          using ACC = typename std::remove_reference<decltype(acc)>::type;
          using userType = typename ACC::value_type;
          auto ndacc = boost::dynamic_pointer_cast<NDRegisterAccessor<userType>>(acc.getHighLevelImplElement());
          if constexpr(std::is_same<userType, ChimeraTK::Boolean>::value) {
            info.format = py::format_descriptor<bool>::format();
          }
          else if constexpr(std::is_same<userType, std::string>::value) {
            // cannot implement
            return;
          }
          else {
            info.format = py::format_descriptor<userType>::format();
          }
          info.ptr = ndacc->accessChannel(0).data();
          info.itemsize = sizeof(userType);
          info.ndim = 1;
          info.shape = {acc.getNElements()};
          info.strides = {sizeof(userType)};
        },
        _accessor);
    return info;
  }

  /********************************************************************************************************************/

  PyArrayAccessor::~PyArrayAccessor() = default;

  /********************************************************************************************************************/

  void PyArrayAccessor::bind(py::module& m) {
    py::class_<PyArrayAccessor, PyTransferElementBase, std::unique_ptr<PyArrayAccessor, py::nodelete>> arrayacc(
        m, "ArrayAccessor", py::buffer_protocol());
    arrayacc.def(py::init<>())
        .def_buffer(&PyArrayAccessor::getBufferInfo)
        .def("read", &PyArrayAccessor::read,
            "Read the data from the device.\n\nIf AccessMode::wait_for_new_data was set, this function will block "
            "until new data has arrived. Otherwise it still might block for a short time until the data transfer was "
            "complete.")
        .def("readNonBlocking", &PyArrayAccessor::readNonBlocking,
            "Read the next value, if available in the input buffer.\n\nIf AccessMode::wait_for_new_data was set, this "
            "function returns immediately and the return value indicated if a new value was available (true) or not "
            "(false).\n\nIf AccessMode::wait_for_new_data was not set, this function is identical to read(), which "
            "will still return quickly. Depending on the actual transfer implementation, the backend might need to "
            "transfer data to obtain the current value before returning. Also this function is not guaranteed to be "
            "lock free. The return value will be always true in this mode.")
        .def("readLatest", &PyArrayAccessor::readLatest,
            "Read the latest value, discarding any other update since the last read if present.\n\nOtherwise this "
            "function is identical to readNonBlocking(), i.e. it will never wait for new values and it will return "
            "whether a new value was available if AccessMode::wait_for_new_data is set.")
        .def("write", &PyArrayAccessor::write,
            "Write the data to device.\n\nThe return value is true, old data was lost on the write transfer (e.g. due "
            "to an buffer overflow). In case of an unbuffered write transfer, the return value will always be false.")
        .def("writeDestructively", &PyArrayAccessor::writeDestructively,
            "Just like write(), but allows the implementation to destroy the content of the user buffer in the "
            "process.\n\nThis is an optional optimisation, hence there is a default implementation which just calls "
            "the normal doWriteTransfer(). In any case, the application must expect the user buffer of the "
            "TransferElement to contain undefined data after calling this function.")
        .def("getName", &PyArrayAccessor::getName, "Returns the name that identifies the process variable.")
        .def("getUnit", &PyArrayAccessor::getUnit,
            "Returns the engineering unit.\n\nIf none was specified, it will default to ' n./ a.'")
        .def("getDescription", &PyArrayAccessor::getDescription, "Returns the description of this variable/register.")
        .def("getValueType", &PyArrayAccessor::getValueType,
            "Returns the std::type_info for the value type of this transfer element.\n\nThis can be used to determine "
            "the type at runtime.")
        .def("getVersionNumber", &PyArrayAccessor::getVersionNumber,
            "Returns the version number that is associated with the last transfer (i.e. last read or write)")
        .def("isReadOnly", &PyArrayAccessor::isReadOnly,
            "Check if transfer element is read only, i.e. it is readable but not writeable.")
        .def("isReadable", &PyArrayAccessor::isReadable, "Check if transfer element is readable.")
        .def("isWriteable", &PyArrayAccessor::isWriteable, "Check if transfer element is writeable.")
        .def("getId", &PyArrayAccessor::getId,
            "Obtain unique ID for the actual implementation of this TransferElement.\n\nThis means that e.g. two "
            "instances of ScalarRegisterAccessor created by the same call to Device::getScalarRegisterAccessor() (e.g. "
            "by copying the accessor to another using NDRegisterAccessorBridge::replace()) will have the same ID, "
            "while two instances obtained by to difference calls to Device::getScalarRegisterAccessor() will have a "
            "different ID even when accessing the very same register.")
        .def("dataValidity", &PyArrayAccessor::dataValidity,
            "Return current validity of the data.\n\nWill always return DataValidity.ok if the backend does not "
            "support it")
        .def("getNElements", &PyArrayAccessor::getNElements, "Return number of elements/samples in the register.")
        .def("get", &PyArrayAccessor::get, "Return an array of UserType (without a previous read).")
        .def("set", &PyArrayAccessor::set, "Set the values of the array of UserType.", py::arg("newValue"))
        .def("setAndWrite", &PyArrayAccessor::setAndWrite,
            "Convenience function to set and write new value.\n\nThe given version number. If versionNumber == {}, a "
            "new version number is generated.",
            py::arg("newValue"))
        .def(
            "readAndGet", &PyArrayAccessor::readAndGet, "Convenience function to read and return an array of UserType.")
        .def("__repr__", &PyArrayAccessor::repr)
        .def("__getitem__", &PyArrayAccessor::getitem)
        .def("__setitem__", &PyArrayAccessor::setitem)
        .def("__setitem__", &PyArrayAccessor::setslice)
        .def("__getattr__", &PyArrayAccessor::getattr);
    for(const auto& fn : PyTransferElementBase::specialFunctionsToEmulateNumeric) {
      arrayacc.def(fn.c_str(),
          [fn](PyArrayAccessor& acc, PyArrayAccessor& other) { return acc.get().attr(fn.c_str())(other.get()); });
      arrayacc.def(
          fn.c_str(), [fn](PyArrayAccessor& acc, py::object& other) { return acc.get().attr(fn.c_str())(other); });
    }
    for(const auto& fn : PyTransferElementBase::specialUnaryFunctionsToEmulateNumeric) {
      arrayacc.def(fn.c_str(), [fn](PyArrayAccessor& acc) { return acc.get().attr(fn.c_str())(); });
    }

    /**
     *  ArrayPushInput
     */
    m.def(
        "ArrayPushInput",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            size_t nElements, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyArrayAccessor>(
              AccessorTypeTag<ArrayPushInput>{}, type, &owner, name, unit, nElements, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ArrayPushInputWB
     */
    m.def(
        "ArrayPushInputWB",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            size_t nElements, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyArrayAccessor>(
              AccessorTypeTag<ArrayPushInputWB>{}, type, &owner, name, unit, nElements, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ArrayPollInput
     */
    m.def(
        "ArrayPollInput",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            size_t nElements, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyArrayAccessor>(
              AccessorTypeTag<ArrayPollInput>{}, type, &owner, name, unit, nElements, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ArrayOutput
     */
    m.def(
        "ArrayOutput",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            size_t nElements, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyArrayAccessor>(
              AccessorTypeTag<ArrayOutput>{}, type, &owner, name, unit, nElements, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ArrayOutputPushRB
     */
    m.def(
        "ArrayOutputPushRB",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            size_t nElements, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyArrayAccessor>(
              AccessorTypeTag<ArrayOutputPushRB>{}, type, &owner, name, unit, nElements, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  ArrayOutputReverseRecovery
     */
    m.def(
        "ArrayOutputReverseRecovery",
        [](ChimeraTK::DataType type, VariableGroup& owner, const std::string& name, const std::string& unit,
            size_t nElements, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyArrayAccessor>(
              AccessorTypeTag<ArrayOutputReverseRecovery>{}, type, &owner, name, unit, nElements, description);
        },
        py::return_value_policy::reference, "");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
