// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"

namespace ChimeraTK {

  /**
   *  Generic module to pipe through a scalar value without altering it.
   *  @todo Make it more efficient by removing this module entirely in the
   * ApplicationCore connection logic!
   */
  template<typename Type>
  struct ScalarPipe : public ApplicationModule {
    ScalarPipe(ModuleGroup* owner, const std::string& name, const std::string& unit, const std::string& description,
        const std::unordered_set<std::string>& tagsInput = {}, const std::unordered_set<std::string>& tagsOutput = {})
    : ApplicationModule(owner, ".", "") {
      input.replace(ScalarPushInput<Type>(this, name, unit, description, tagsInput));
      output.replace(ScalarOutput<Type>(this, name, unit, description, tagsOutput));
    }

    ScalarPipe(ModuleGroup* owner, const std::string& inputName, const std::string& outputName, const std::string& unit,
        const std::string& description, const std::unordered_set<std::string>& tagsInput = {},
        const std::unordered_set<std::string>& tagsOutput = {})
    : ApplicationModule(owner, ".", "") {
      input.replace(ScalarPushInput<Type>(this, inputName, unit, description, tagsInput));
      output.replace(ScalarOutput<Type>(this, outputName, unit, description, tagsOutput));
    }

    ScalarPipe() = default;

    ScalarPushInput<Type> input;
    ScalarOutput<Type> output;

    void mainLoop() override {
      while(true) {
        output = static_cast<Type>(input);
        output.write();
        input.read();
      }
    }
  };

  /**
   *  Generic module to pipe through a scalar value without altering it.
   *  @todo Make it more efficient by removing this module entirely in the
   * ApplicationCore connection logic!
   */
  template<typename Type>
  struct ArrayPipe : public ApplicationModule {
    ArrayPipe(ModuleGroup* owner, const std::string& name, const std::string& unit, size_t nElements,
        const std::string& description, const std::unordered_set<std::string>& tagsInput = {},
        const std::unordered_set<std::string>& tagsOutput = {})
    : ApplicationModule(owner, ".", description) {
      input.replace(ArrayPushInput<Type>(this, name, unit, nElements, description, tagsInput));
      output.replace(ArrayOutput<Type>(this, name, unit, nElements, description, tagsOutput));
    }

    ArrayPipe(ModuleGroup* owner, const std::string& inputName, const std::string& outputName, const std::string& unit,
        size_t nElements, const std::string& description, const std::unordered_set<std::string>& tagsInput = {},
        const std::unordered_set<std::string>& tagsOutput = {})
    : ApplicationModule(owner, ".", description) {
      input.replace(ArrayPushInput<Type>(this, inputName, unit, nElements, description, tagsInput));
      output.replace(ArrayOutput<Type>(this, outputName, unit, nElements, description, tagsOutput));
    }

    ArrayPipe() = default;

    ArrayPushInput<Type> input;
    ArrayOutput<Type> output;

    void mainLoop() override {
      std::vector<Type> temp(input.getNElements());
      while(true) {
        input.swap(temp);
        output.swap(temp);
        input.swap(temp);
        output.write();
        input.read();
      }
    }
  };

} // namespace ChimeraTK
