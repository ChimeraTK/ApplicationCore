// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PyReadAnyGroup.h"

namespace py = pybind11;

namespace ChimeraTK {

  /********************************************************************************************************************/

  PyReadAnyGroup::PyReadAnyGroup(ReadAnyGroup&& other) : _impl(std::move(other)) {}

  /********************************************************************************************************************/

  PyReadAnyGroup::PyReadAnyGroup(py::args args) {
    for(auto& acc : args) add(acc.cast<PyTransferElementBase&>());
  }

  /********************************************************************************************************************/

  void PyReadAnyGroup::readUntil(const TransferElementID& tid) {
    py::gil_scoped_release release;
    _impl.readUntil(tid);
  }

  /********************************************************************************************************************/

  void PyReadAnyGroup::add(PyTransferElementBase& acc) {
    auto teAbstractor = acc.getTE();
    _impl.add(teAbstractor);
  }

  /********************************************************************************************************************/

  void PyReadAnyGroup::readUntilAccessor(PyTransferElementBase& acc) {
    auto te = acc.getTE();
    py::gil_scoped_release release;
    _impl.readUntil(te);
  }

  /********************************************************************************************************************/

  void PyReadAnyGroup::readUntilAll(py::args args) {
    std::vector<TransferElementID> ids(args.size());
    std::transform(args.begin(), args.end(), ids.begin(),
        [](const py::handle& acc) { return acc.cast<PyTransferElementBase&>().getTE().getId(); });
    py::gil_scoped_release release;
    _impl.readUntilAll(ids);
  }

  /********************************************************************************************************************/

  TransferElementID PyReadAnyGroup::readAny() {
    py::gil_scoped_release release;
    return _impl.readAny();
  }

  /********************************************************************************************************************/

  TransferElementID PyReadAnyGroup::readAnyNonBlocking() {
    return _impl.readAnyNonBlocking();
  }

  /********************************************************************************************************************/

  void PyReadAnyGroup::bind(py::module& mod) {
    py::class_<PyReadAnyGroup>(mod, "ReadAnyGroup")
        .def(py::init<>())
        .def(py::init<py::args>())
        .def("add", &PyReadAnyGroup::add,
            "Add register to group.\n\n Note that calling this function is only allowed before finalise() has been "
            "called. The given register may not yet be part of a ReadAnyGroup or a TransferGroup, otherwise an "
            "exception is thrown.\n\nThe register must be must be readable.",
            py::arg("element"))
        .def("readAny", &PyReadAnyGroup::readAny,
            "Wait until one of the elements in this group has received an update.\n\nThe function will return the "
            "TransferElementID of the element which has received the update. If multiple updates are received at the "
            "same time or if multiple updates were already present before the call to this function, the ID of the "
            "first element receiving an update will be returned.\n\nOnly elements with AccessMode::wait_for_new_data "
            "are used for waiting. Once an update has been received for one of these elements, the function will call "
            "readLatest() on all elements without AccessMode::wait_for_new_data (this is equivalent to calling "
            "processPolled()).\n\nBefore returning, the postRead action will be called on the TransferElement whose ID "
            "is returned, so the read data will already be present in the user buffer. All other TransferElements in "
            "this group will not be altered.\n\nBefore calling this function, finalise() must have been called, "
            "otherwise the behaviour is undefined.")
        .def("readAnyNonBlocking", &PyReadAnyGroup::readAnyNonBlocking,
            "Read the next available update in the group, but do not block if no update is available.\n\nIf no update "
            "is available, a default-constructed TransferElementID is returned after all poll-type elements in the "
            "group have been updated.\n\nBefore calling this function, finalise() must have been called, otherwise the "
            "behaviour is undefined.")
        .def("readUntil", py::overload_cast<const TransferElementID&>(&PyReadAnyGroup::readUntil), py::arg("id"))
        .def("readUntil", &PyReadAnyGroup::readUntilAccessor,
            "Wait until the given TransferElement has received an update and store it to its user buffer.\n\nAll "
            "updates of other elements which are received before the update of the given element will be processed and "
            "are thus visible in the user buffers when this function returns.\n\nThe specified TransferElement must be "
            "part of this ReadAnyGroup, otherwise the behaviour is undefined.\n\nThis is merely a convenience function "
            "calling waitAny() in a loop until the ID of the given element is returned.\n\nBefore calling this "
            "function, finalise() must have been called, otherwise the behaviour is undefined.",
            py::arg("element"))
        .def("readUntilAll", &PyReadAnyGroup::readUntilAll,
            "Wait until all of the given TransferElements has received an update and store it to its user "
            "buffer.\n\nAll updates of other elements which are received before the update of the given element will "
            "be processed and are thus visible in the user buffers when this function returns.\n\nThe specified "
            "TransferElement must be part of this ReadAnyGroup, otherwise the behaviour is undefined.\n\nThis is "
            "merely a convenience function calling waitAny() in a loop until the ID of the given element is "
            "returned.\n\nBefore calling this function, finalise() must have been called, otherwise the behaviour is "
            "undefined.")
        .def("finalise", &PyReadAnyGroup::finalise,
            "Finalise the group.\n\nFrom this point on, add() may no longer be called. Only after the group has been "
            "finalised the read functions of this group may be called. Also, after the group has been finalised, read "
            "functions may no longer be called directly on the participating elements (including other copies of the "
            "same element).\n\nThe order of update notifications will only be well-defined for updates which happen "
            "after the call to finalise(). Any unread values which are present in the TransferElements when this "
            "function is called will not be processed in the correct sequence. Only the sequence within each "
            "TransferElement can be guaranteed. For any updates which arrive after the call to finalise() the correct "
            "sequence will be guaranteed even accross TransferElements.\n\nThis function will call readAsync() on all "
            "elements with AccessMode::wait_for_new_data in the group. There must be at least one transfer element "
            "with AccessMode::wait_for_new_data in the group, otherwise an exception is thrown.")
        .def("interrupt", &PyReadAnyGroup::interrupt,
            "Convenience function to interrupt any running readAny/waitAny by calling interrupt on one of the "
            "push-type TransferElements in the group.");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
