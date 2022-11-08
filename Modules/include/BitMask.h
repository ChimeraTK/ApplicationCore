// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

namespace ChimeraTK {

  template<size_t NBITS>
  struct WriteBitMask : public ApplicationModule {
    using ApplicationModule::ApplicationModule;

    /// individual inputs for each bit
    struct Input : public VariableGroup {
      Input() {}
      explicit Input(VariableGroup* owner) : VariableGroup(owner, ".", "The input bits") {
        for(size_t i = 0; i < NBITS; ++i) {
          bit[i].replace(ScalarPushInput<int>(
              this, "bit" + std::to_string(i), "", "The bit " + std::to_string(i) + " of the bit mask"));
        }
      }

      ScalarPushInput<int> bit[NBITS];
    };
    Input input{this};

    ScalarOutput<int32_t> bitmask{this, "bitmask", "", "Output bit mask."};

    void mainLoop() {
      auto readGroup = input.readAnyGroup();

      while(true) {
        // create bit mask
        bitmask = 0;
        for(size_t i = 0; i < NBITS; ++i) {
          if(input.bit[i] != 0) {
            bitmask |= 1 << i;
          }
        }
        bitmask.write();

        // wait for new input values (at the end, since we want to process the
        // initial values first)
        readGroup.readAny();
      }
    }
  };

  /*********************************************************************************************************************/

  template<size_t NBITS>
  struct ReadBitMask : public ApplicationModule {
    ReadBitMask(EntityOwner* owner, const std::string& name, const std::string& description,
        bool eliminateHierarchy = false, const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, eliminateHierarchy, tags) {}

    ReadBitMask() {}

    ReadBitMask(EntityOwner* owner, const std::string& inputName, const std::string& inputDescription,
        const std::unordered_set<std::string>& inputTags, const std::array<std::string, NBITS>& outputNames,
        const std::array<std::string, NBITS>& outputDescriptions, const std::unordered_set<std::string>& outputTags)
    : ApplicationModule(owner, inputName, inputDescription, true) {
      bitmask.setMetaData(inputName, "", inputDescription, inputTags);
      output.setEliminateHierarchy();
      for(size_t i = 0; i < NBITS; ++i) {
        output.bit[i].setMetaData(outputNames[i], "", outputDescriptions[i], outputTags);
      }
    }

    /// individual outputs for each bit
    struct Output : public VariableGroup {
      Output() {}
      explicit Output(VariableGroup* owner) : VariableGroup(owner, ".", "The extracted output bits") {
        for(size_t i = 0; i < NBITS; ++i) {
          bit[i].replace(ScalarOutput<int>(
              this, "bit" + std::to_string(i), "", "The bit " + std::to_string(i) + " of the bit mask"));
        }
      }

      ScalarOutput<int> bit[NBITS];
    };
    Output output{this};

    ScalarPushInput<int32_t> bitmask{this, "bitmask", "", "Input bit mask."};

    void mainLoop() {
      while(true) {
        // decode bit mask
        for(size_t i = 0; i < NBITS; ++i) {
          output.bit[i] = (bitmask & (1 << i)) != 0;
          output.bit[i].write(); /// @todo TODO better make a writeAll() for VariableGroups
        }

        // wait for new input values (at the end, since we want to process the
        // initial values first)
        bitmask.read();
      }
    }
  };

} // namespace ChimeraTK
