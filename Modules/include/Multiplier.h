/*
 *  Generic module to multiply one value with another
 */

#ifndef CHIMERATK_APPLICATION_CORE_MULTIPLIER_H
#define CHIMERATK_APPLICATION_CORE_MULTIPLIER_H

#include <cmath>
#include <limits>

#include "ApplicationCore.h"

namespace ChimeraTK {

  template<typename InputType, typename OutputType = InputType, size_t NELEMS = 1>
  struct ConstMultiplier : public ApplicationModule {
    ConstMultiplier(EntityOwner* owner, const std::string& name, const std::string& description, double factor)
    : ApplicationModule(owner, name, ""), input(this, "input", "", NELEMS, description),
      output(this, "output", "", NELEMS, description), _factor(factor) {
      setEliminateHierarchy();
    }

    ArrayPushInput<InputType> input;
    ArrayOutput<OutputType> output;

    double _factor;

    void mainLoop() {
      while(true) {
        // scale value (with rounding, if integral type)
        if(!std::numeric_limits<OutputType>::is_integer) {
          for(size_t i = 0; i < NELEMS; ++i) output[i] = input[i] * _factor;
        }
        else {
          for(size_t i = 0; i < NELEMS; ++i) output[i] = std::round(input[i] * _factor);
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

    Multiplier(EntityOwner* owner, const std::string& inputPath, const std::string& inputUnit,
          const std::string& factorPath, const std::string& outputPath,
          const std::string& outputUnit, const std::string& description,
          HierarchyModifier hierarchyModifier = HierarchyModifier::hideThis,
          const std::unordered_set<std::string>& inputTags = {},
          const std::unordered_set<std::string>& factorTags = {},
          const std::unordered_set<std::string>& outputTags = {})
    : ApplicationModule(owner, "Multiplier", "", hierarchyModifier),
      ig{this, HierarchyModifyingGroup::getPathName(inputPath), ""},
      fg{this, HierarchyModifyingGroup::getPathName(factorPath), ""},
      og{this, HierarchyModifyingGroup::getPathName(outputPath), ""}
    {
      std::string factorUnit =  "(" + outputUnit + ")/(" + inputUnit + ")";
      ig.input.replace(ArrayPushInput<InputType>(&ig, HierarchyModifyingGroup::getUnqualifiedName(inputPath),
                                                 inputUnit, NELEMS, description, inputTags));
      fg.factor.replace(ScalarPushInput<InputType>(&fg, HierarchyModifyingGroup::getUnqualifiedName(factorPath),
                                                  factorUnit, description, factorTags));
      og.output.replace(ArrayOutput<InputType>(&og, HierarchyModifyingGroup::getUnqualifiedName(outputPath),
                                                  outputUnit, NELEMS, description, outputTags));
    }

    Multiplier(EntityOwner* owner, const std::string& name, const std::string& factorName, const std::string& unitInput,
        const std::string& unitOutput, const std::string& description,
        const std::unordered_set<std::string>& tagsInput = {}, const std::unordered_set<std::string>& tagsOutput = {},
        const std::unordered_set<std::string>& tagsFactor = {})
    : ApplicationModule(owner, name, "", HierarchyModifier::hideThis),
      ig{this, ".", ""},
      fg{this, ".", ""},
      og{this, ".", ""}
    {
      ig.input.replace(ArrayPushInput<InputType>(&ig, name, unitInput, NELEMS, description, tagsInput));
      fg.factor.replace(ScalarPushInput<double>(
          &fg, factorName, "(" + unitOutput + ")/(" + unitInput + ")", description, tagsFactor));
      og.output.replace(ArrayOutput<OutputType>(&og, name, unitOutput, NELEMS, description, tagsOutput));
    }

    /** Note: This constructor is deprectated! */
    [[deprecated]] Multiplier(EntityOwner* owner, const std::string& name, const std::string& description)
      : ApplicationModule(owner, name, "", HierarchyModifier::hideThis),
        ig{this, ".", ""},
        fg{this, ".", ""},
        og{this, ".", ""}
    {
      ig.input.replace(ArrayPushInput<InputType>(&ig, "input", "", NELEMS, description));
      fg.factor.replace(ScalarPushInput<double>(&fg, "factor", "", description));
      og.output.replace(ArrayOutput<OutputType>(&og, "factor", "", NELEMS, description));
    }

    struct : HierarchyModifyingGroup {
      using HierarchyModifyingGroup::HierarchyModifyingGroup;
      ArrayPushInput<InputType> input;
    } ig;
    struct : HierarchyModifyingGroup {
      using HierarchyModifyingGroup::HierarchyModifyingGroup;
      ScalarPushInput<double> factor;
    } fg;
    struct : HierarchyModifyingGroup {
      using HierarchyModifyingGroup::HierarchyModifyingGroup;
      ArrayOutput<OutputType> output;
    } og;

    void mainLoop() {
      ReadAnyGroup group{ig.input, fg.factor};
      while(true) {
        // scale value (with rounding, if integral type)
        if(!std::numeric_limits<OutputType>::is_integer) {
          for(size_t i = 0; i < NELEMS; ++i) og.output[i] = ig.input[i] * fg.factor;
        }
        else {
          for(size_t i = 0; i < NELEMS; ++i) og.output[i] = std::round(ig.input[i] * fg.factor);
        }

        // write scaled value
        og.output.write();

        // wait for new input value (at the end, since we want to process the
        // initial values first)
        group.readAny();
      }
    }
  };

  template<typename InputType, typename OutputType = InputType, size_t NELEMS = 1>
  struct Divider : public ApplicationModule {
    using ApplicationModule::ApplicationModule;
    Divider(EntityOwner* owner, const std::string& name, const std::string& description)
    : ApplicationModule(owner, name, ""), input(this, "input", "", NELEMS, description),
      divider(this, "divider", "", "Divider to scale the input value with"),
      output(this, "output", "", NELEMS, description) {
      setEliminateHierarchy();
    }

    ArrayPushInput<InputType> input;
    ScalarPushInput<double> divider;
    ArrayOutput<OutputType> output;

    void mainLoop() {
      ReadAnyGroup group{input, divider};
      while(true) {
        // scale value (with rounding, if integral type)
        if(!std::numeric_limits<OutputType>::is_integer) {
          for(size_t i = 0; i < NELEMS; ++i) output[i] = input[i] / divider;
        }
        else {
          for(size_t i = 0; i < NELEMS; ++i) output[i] = std::round(input[i] / divider);
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

#endif /* CHIMERATK_APPLICATION_CORE_MULTIPLIER_H */
