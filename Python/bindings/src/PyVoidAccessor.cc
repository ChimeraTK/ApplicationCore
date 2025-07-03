// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyVoidAccessor.h"

#include <pybind11/stl.h>

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  PyVoidAccessor::PyVoidAccessor() : _accessor(VoidOutput()) {}

  /********************************************************************************************************************/

  template<class AccessorType>
  PyVoidAccessor::PyVoidAccessor(VoidTypeTag<AccessorType>, Module* owner, const std::string& name,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : _accessor(std::move(AccessorType(owner, name, description, tags))) {}

  /********************************************************************************************************************/

  PyVoidAccessor::~PyVoidAccessor() = default;

  /********************************************************************************************************************/

  std::string PyVoidAccessor::repr(py::object& acc) {
    std::string rep{"<VoidAccessor(name="};
    rep.append(py::cast(&acc).attr("getName")().cast<std::string>());
    rep.append(", versionNumber=");
    rep.append(py::cast<py::object>(py::cast(&acc).attr("getVersionNumber")()).attr("__repr__")().cast<std::string>());
    rep.append(", dataValidity=");
    rep.append(py::cast<py::object>(py::cast(&acc).attr("dataValidity")()).attr("__repr__")().cast<std::string>());
    rep.append(")>");
    return rep;
  }

  /********************************************************************************************************************/

  void PyVoidAccessor::bind(py::module& m) {
    // strictly speaking, py::nodelete is not necessary since we hand out instances only via factory function,
    // but leave it here for consistentcy
    py::class_<PyVoidAccessor, PyTransferElementBase, std::unique_ptr<PyVoidAccessor, py::nodelete>> scalaracc(
        m, "VoidAccessor", py::buffer_protocol());
    scalaracc.def(py::init<>())
        .def("read", &PyVoidAccessor::read,
            "Read the data from the device.\n\nIf AccessMode::wait_for_new_data was set, this function will block "
            "until new data has arrived. Otherwise it still might block for a short time until the data transfer was "
            "complete.")
        .def("readNonBlocking", &PyVoidAccessor::readNonBlocking,
            "Read the next value, if available in the input buffer.\n\nIf AccessMode::wait_for_new_data was set, this "
            "function returns immediately and the return value indicated if a new value was available (true) or not "
            "(false).\n\nIf AccessMode::wait_for_new_data was not set, this function is identical to read(), which "
            "will still return quickly. Depending on the actual transfer implementation, the backend might need to "
            "transfer data to obtain the current value before returning. Also this function is not guaranteed to be "
            "lock free. The return value will be always true in this mode.")
        .def("readLatest", &PyVoidAccessor::readLatest,
            "Read the latest value, discarding any other update since the last read if present.\n\nOtherwise this "
            "function is identical to readNonBlocking(), i.e. it will never wait for new values and it will return "
            "whether a new value was available if AccessMode::wait_for_new_data is set.")
        .def("write", &PyVoidAccessor::write,
            "Write the data to device.\n\nThe return value is true, old data was lost on the write transfer (e.g. due "
            "to an buffer overflow). In case of an unbuffered write transfer, the return value will always be false.")
        .def("writeDestructively", &PyVoidAccessor::writeDestructively,
            "Just like write(), but allows the implementation to destroy the content of the user buffer in the "
            "process.\n\nThis is an optional optimisation, hence there is a default implementation which just calls "
            "the normal doWriteTransfer(). In any case, the application must expect the user buffer of the "
            "TransferElement to contain undefined data after calling this function.")
        .def("getName", &PyVoidAccessor::getName, "Returns the name that identifies the process variable.")
        .def("getUnit", &PyVoidAccessor::getUnit,
            "Returns the engineering unit.\n\nIf none was specified, it will default to ' n./ a.'")
        .def("getDescription", &PyVoidAccessor::getDescription, "Returns the description of this variable/register.")
        .def("getValueType", &PyVoidAccessor::getValueType,
            "Returns the std::type_info for the value type of this transfer element.\n\nThis can be used to determine "
            "the type at runtime.")
        .def("getVersionNumber", &PyVoidAccessor::getVersionNumber,
            "Returns the version number that is associated with the last transfer (i.e. last read or write)")
        .def("isReadOnly", &PyVoidAccessor::isReadOnly,
            "Check if transfer element is read only, i.e. it is readable but not writeable.")
        .def("isReadable", &PyVoidAccessor::isReadable, "Check if transfer element is readable.")
        .def("isWriteable", &PyVoidAccessor::isWriteable, "Check if transfer element is writeable.")
        .def("getId", &PyVoidAccessor::getId,
            "Obtain unique ID for the actual implementation of this TransferElement.\n\nThis means that e.g. two "
            "instances of VoidRegisterAccessor created by the same call to Device::getVoidRegisterAccessor() (e.g. "
            "by copying the accessor to another using NDRegisterAccessorBridge::replace()) will have the same ID, "
            "while two instances obtained by to difference calls to Device::getVoidRegisterAccessor() will have a "
            "different ID even when accessing the very same register.")
        .def("dataValidity", &PyVoidAccessor::dataValidity,
            "Return current validity of the data.\n\nWill always return DataValidity.ok if the backend does not "
            "support it")
        .def("__repr__", &PyVoidAccessor::repr);

    /**
     *  VoidPushInput
     */
    m.def(
        "VoidInput",
        [](VariableGroup& owner, const std::string& name, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyVoidAccessor>(
              VoidTypeTag<VoidInput>(), &owner, name, description);
        },
        py::return_value_policy::reference, "");

    /**
     *  VoidOutput
     */
    m.def(
        "VoidOutput",
        [](VariableGroup& owner, const std::string& name, const std::string& description) {
          return dynamic_cast<PyOwningObject&>(owner).make_child<PyVoidAccessor>(
              VoidTypeTag<VoidOutput>(), &owner, name, description);
        },
        py::return_value_policy::reference, "");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
