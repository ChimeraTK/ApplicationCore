// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pybind11/pybind11.h>
// pybind11.h must come first

#include "PyArrayAccessor.h"
#include "PyOwnershipManagement.h"
#include "PyScalarAccessor.h"

#include <FanIn.h>
#include <ChimeraTK/PyConvert.h>

#include <map>
#include <memory>
#include <vector>

namespace py = pybind11;

namespace ChimeraTK {

  class PyFanIn : public PyOwnedObject {
    class Concept {
     public:
      virtual ~Concept() = default;
      virtual py::object get() const = 0;
      virtual std::string getName() const = 0;
    };

    template<template<typename> class FanInType, typename UserType>
    class Model : public Concept {
     public:
      using FanIn = FanInType<UserType>;
      using Abstractor = typename FanIn::AbstractorType;

      Model(VariableGroup* owner, std::string name, std::vector<std::string> additionalNames, std::string unit,
          std::string description, py::function aggregator, const std::unordered_set<std::string>& tags)
      : _fanIn(owner, std::move(name), additionalNames, std::move(unit), std::move(description),
            [this, aggregator = std::move(aggregator)](TransferElementID id, const auto& inputs) {
              return aggregate(aggregator, id, inputs);
            },
            tags) {}

      FanIn& get() { return _fanIn; }
      py::object get() const { return py::cast(_fanIn.get()); }
      std::string getName() const { return _fanIn.getName(); }

     private:
      template<typename Accessor>
      auto wrap(const Accessor& accessor) {
        if constexpr(array_accessor<Accessor>) {
          return std::make_unique<PyArrayAccessor>(PyArrayAccessor::fromAccessor(accessor));
        }
        else {
          return std::make_unique<PyScalarAccessor>(PyScalarAccessor::fromAccessor(accessor));
        }
      }

      template<typename AccessorMap>
      UserType aggregate(py::function& aggregator, const TransferElementID& id, const AccessorMap& inputs) {
        py::gil_scoped_acquire acquire;
        py::list wrappedInputs;
        PyTransferElementBase* changed = nullptr;

        for(const auto& [inputId, input] : inputs) {
          auto [it, inserted] = _wrappedInputs.try_emplace(inputId);
          if(inserted) {
            it->second = wrap(input);
          }
          auto* accessor = it->second.get();
          wrappedInputs.append(py::cast(accessor, py::return_value_policy::reference));
          if(inputId == id) {
            changed = accessor;
          }
        }

        if(changed == nullptr) {
          throw ChimeraTK::logic_error("FanIn aggregator received an unknown input ID.");
        }
        auto result = convertPyObject<UserType>(
            aggregator(py::cast(changed, py::return_value_policy::reference), wrappedInputs), true, false);
        if constexpr(array_accessor<AccessorMap::mapped_type>) {
          return result;
        }
        else {
          return result[0];
        }
      }

      FanIn _fanIn;
      std::map<TransferElementID, std::unique_ptr<PyTransferElementBase>> _wrappedInputs;
    };

   public:
    template<template<typename> class FanInType>
    static void initialize(PyFanIn& target, DataType type, VariableGroup& owner, const std::string& name,
        const std::vector<std::string>& additionalNames, const std::string& unit, const std::string& description,
        py::function aggregator, const std::unordered_set<std::string>& tags);

    static void bind(py::module& mod);

    py::object get() const { return _impl->get(); }
    std::string getName() const { return _impl->getName(); }

    PyFanIn() = default;

   private:
    std::unique_ptr<Concept> _impl;
  };

} // namespace ChimeraTK
