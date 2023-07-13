// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "HierarchyModifyingGroup.h"
#include "ScalarAccessor.h"

#include <chrono>

namespace ChimeraTK {

  /**
   * Simple periodic trigger that fires a variable once per second.
   * After configurable number of seconds it will wrap around
   */
  struct PeriodicTrigger : public ApplicationModule {
    /**
     * Create periodic trigger module.
     *
     * In addition to the usual arguments of an ApplicationModule, the default timeout value is specified. This value
     * is used as a timeout if the timeout value is set to 0. The timeout value is in milliseconds.
     *
     * @param owner Owning ModuleGroup
     * @param name The name of the PeriodicTrigger module
     * @param description The description of the PeriodicTrigger module
     * @param defaultPeriod Trigger period in milliseconds, used when the trigger period input process variable has the
     *        value 0.
     * @param tags List of tags to attach to all variables concerning the location in the virtual hierarchy
     * @param periodName Qualified name for the period input process variable
     * @param tickName Qualified names for the tick output process variable
     *
     * For periodName and tickName, you can just give a variable name, a relative or an absolute path.
     */
    PeriodicTrigger(ModuleGroup* owner, const std::string& name, const std::string& description,
        const uint32_t defaultPeriod = 1000, const std::unordered_set<std::string>& tags = {},
        const std::string& periodName = "period", const std::string& tickName = "tick")
    : ApplicationModule(owner, name, description, tags),
      period(this, periodName, "ms", "period in milliseconds. The trigger is sent once per the specified duration."),
      tick(this, tickName, "", "Timer tick. Counts the trigger number starting from 0."),
      defaultPeriod_(defaultPeriod) {}

    PeriodicTrigger() {}

    [[deprecated("Use PeriodicTrigger without hierarchy modifier and a qualified path "
                 "instead")]] PeriodicTrigger(ModuleGroup* owner, const std::string& name,
        const std::string& description, const uint32_t defaultPeriod, HierarchyModifier hierarchyModifier,
        const std::unordered_set<std::string>& tags = {}, const std::string& periodName = "period",
        const std::string& tickName = "tick")
    : PeriodicTrigger(owner, applyHierarchyModifierToName(name, hierarchyModifier), description, defaultPeriod, tags,
          periodName, tickName) {}

    ScalarPollInput<uint32_t> period;
    ScalarOutput<uint64_t> tick;

    void prepare() override {
      setCurrentVersionNumber({});
      tick.write(); // send initial value
    }

    void sendTrigger() {
      setCurrentVersionNumber({});
      ++tick;
      tick.write();
    }

    void mainLoop() override {
      if(Application::getInstance().getTestableMode().isEnabled()) {
        return;
      }
      tick = 0;
      std::chrono::time_point<std::chrono::steady_clock> t = std::chrono::steady_clock::now();

      while(true) {
        period.read();
        if(period == 0) {
          // set receiving end of timeout. Will only be overwritten if there is
          // new data.
          period = defaultPeriod_;
        }
        t += std::chrono::milliseconds(static_cast<uint32_t>(period));
        boost::this_thread::interruption_point();
        std::this_thread::sleep_until(t);

        sendTrigger();
      }
    }

   private:
    uint32_t defaultPeriod_;
  };
} // namespace ChimeraTK
