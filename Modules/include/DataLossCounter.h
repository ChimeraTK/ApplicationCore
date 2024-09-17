// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "ApplicationModule.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

#include <ChimeraTK/SupportedUserTypes.h>

#include <map>

namespace ChimeraTK {

  /**
   *  Module which gathers statistics on data loss inside the application. It will
   * read the data loss counter once per trigger and update the output statistics
   * variables.
   */
  template<typename TRIGGERTYPE = int32_t>
  struct DataLossCounter : ApplicationModule {
    DataLossCounter(ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::string& pathToTrigger, const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, tags), directTrigger(this, pathToTrigger, "", "Trigger Input"),
      trigger(directTrigger) {}

    DataLossCounter() {}

    ScalarPushInput<TRIGGERTYPE> directTrigger;

    struct TriggerGroup_compat : VariableGroup {
      using VariableGroup::VariableGroup;
      ScalarPushInput<TRIGGERTYPE> trigger{this, "trigger", "", "Trigger input"};
    } triggerGroup_compat;

    // This is for backwards compatibility!
    ScalarPushInput<TRIGGERTYPE>& trigger;

    ScalarOutput<uint64_t> lostDataInLastTrigger{this, "lostDataInLastTrigger", "",
        "Number of data transfers during "
        "the last trigger which resulted in data loss."};
    ScalarOutput<uint64_t> triggersWithDataLoss{this, "triggersWithDataLoss", "",
        "Number of trigger periods during "
        "which at least on data transfer resulted in data loss."};

    void mainLoop() override {
      while(true) {
        trigger.read();
        uint64_t counter = Application::getAndResetDataLossCounter();
        lostDataInLastTrigger = counter;
        if(counter > 0) ++triggersWithDataLoss;
        writeAll();
      }
    }
  };

} // namespace ChimeraTK
