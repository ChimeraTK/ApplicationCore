// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"

#include <cmath>
#include <limits>

namespace ChimeraTK {

  template<typename InputType, typename OutputType = InputType, size_t NELEMS = 1>
  struct ConstMultiplier : public ApplicationModule {
    ConstMultiplier(ModuleGroup* owner, const std::string& name, const std::string& description, double theFactor)
    : ApplicationModule(owner, name, ""), input(this, "input", "", NELEMS, description),
      output(this, "output", "", NELEMS, description), factor(theFactor) {}

    ArrayPushInput<InputType> input;
    ArrayOutput<OutputType> output;

    double factor;

    void mainLoop() override {
      while(true) {
        // scale value (with rounding, if integral type)
        if(!std::numeric_limits<OutputType>::is_integer) {
          for(size_t i = 0; i < NELEMS; ++i) {
            output[i] = input[i] * factor;
          }
        }
        else {
          for(size_t i = 0; i < NELEMS; ++i) {
            output[i] = std::round(input[i] * factor);
          }
        }

        // write scaled value
        output.write();

        // wait for new input value (at the end, since we want to process the
        // initial values first)
        input.read();
      }
    }
  };

  template<typename InputType, typename OutputType = InputType, size_t NELEMS = 1>
  struct Multiplier : public ApplicationModule {
    using ApplicationModule::ApplicationModule;

    Multiplier(ModuleGroup* owner, const std::string& name, const std::string& factorName, const std::string& unitInput,
        const std::string& unitOutput, const std::string& description,
        const std::unordered_set<std::string>& tagsInput = {}, const std::unordered_set<std::string>& tagsOutput = {},
        const std::unordered_set<std::string>& tagsFactor = {})
    : ApplicationModule(owner, ".", "") {
      input.replace(ArrayPushInput<InputType>(input, name, unitInput, NELEMS, description, tagsInput));
      factor.replace(ScalarPushInput<double>(
          input, factorName, "(" + unitOutput + ")/(" + unitInput + ")", description, tagsFactor));
      output.replace(ArrayOutput<OutputType>(input, name, unitOutput, NELEMS, description, tagsOutput));
    }

    Multiplier(ModuleGroup* owner, const std::string& inputPath, const std::string& inputUnit,
        const std::string& factorPath, const std::string& outputPath, const std::string& outputUnit,
        const std::string& description, const std::unordered_set<std::string>& inputTags = {},
        const std::unordered_set<std::string>& factorTags = {}, const std::unordered_set<std::string>& outputTags = {})
    : ApplicationModule(owner, ".", "") {
      std::string factorUnit = "(" + outputUnit + ")/(" + inputUnit + ")";
      input.replace(ArrayPushInput<InputType>(this, inputPath, inputUnit, NELEMS, description, inputTags));
      factor.replace(ScalarPushInput<InputType>(this, factorPath, factorUnit, description, factorTags));
      output.replace(ArrayOutput<InputType>(this, outputPath, outputUnit, NELEMS, description, outputTags));
    }

    ArrayPushInput<InputType> input;
    ScalarPushInput<double> factor;
    ArrayOutput<OutputType> output;

    void mainLoop() override {
      ReadAnyGroup group{input, factor};
      while(true) {
        // scale value (with rounding, if integral type)
        if constexpr(!std::numeric_limits<OutputType>::is_integer) {
          for(size_t i = 0; i < NELEMS; ++i) {
            output[i] = input[i] * factor;
          }
        }
        else {
          for(size_t i = 0; i < NELEMS; ++i) {
            output[i] = std::round(input[i] * factor);
          }
        }

        // write scaled value
        output.write();

        // wait for new input value (at the end, since we want to process the
        // initial values first)
        group.readAny();
      }
    }
  };

  template<typename InputType, typename OutputType = InputType, size_t NELEMS = 1>
  struct Divider : public ApplicationModule {
    using ApplicationModule::ApplicationModule;
    Divider(ModuleGroup* owner, const std::string& name, const std::string& description)
    : ApplicationModule(owner, name, ""), input(this, "input", "", NELEMS, description),
      divider(this, "divider", "", "Divider to scale the input value with"),
      output(this, "output", "", NELEMS, description) {}

    ArrayPushInput<InputType> input;
    ScalarPushInput<double> divider;
    ArrayOutput<OutputType> output;

    void mainLoop() override {
      ReadAnyGroup group{input, divider};
      while(true) {
        // scale value (with rounding, if integral type)
        if(!std::numeric_limits<OutputType>::is_integer) {
          for(size_t i = 0; i < NELEMS; ++i) {
            output[i] = input[i] / divider;
          }
        }
        else {
          for(size_t i = 0; i < NELEMS; ++i) {
            output[i] = std::round(input[i] / divider);
          }
        }

        // write scaled value
        output.write();

        // wait for new input value (at the end, since we want to process the
        // initial values first)
        group.readAny();
      }
    }
  };

} // namespace ChimeraTK
