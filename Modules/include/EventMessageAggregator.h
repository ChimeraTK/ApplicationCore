// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ConfigReader.h"
#include "PeriodicTrigger.h"
#include "StatusWithMessage.h"
#include "VoidAccessor.h"

#include <ChimeraTK/ScalarRegisterAccessor.h>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   * @brief Specialised ScalarOutput for sending event messages which can be aggregated by the EventMessageAggregator.
   *
   * The ApplicationModule should send a message string whenever a corresponding event occurs. It should not clear the
   * message on its own, so the process variable will always contain the latest message string. Clearing old messages
   * is done in the EventMessageAggregator.
   */
  class AggregatableMessage : public ScalarOutput<std::string> {
   public:
    AggregatableMessage(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    using ScalarOutput::ScalarOutput;
    using ScalarOutput::operator=;

    static constexpr std::string_view aggregatableMessageTag{"_ChimeraTK_AggregatableMessage"};
  };

  /********************************************************************************************************************/

  /**
   * @brief Module to aggregate messages from AggregatableMessage outputs.
   *
   * The EventMessageAggregator will search the entire application for all AggregatableMessage outputs and subscribe to
   * them. It will show the latest message in its StatusWithMessage output as a warning. The message can be cleared by
   * writing to the "clear" input. It will automatically clear after a configurable number of seconds from the
   * ConfigReader variable "/Configuration/autoClearEventMessage", which defaults to 300 seconds. A value of 0 seconds
   * will disable the auto-clear functionality.
   *
   * The EventMessageAggregator must be instantiated after all AggregatableMessage outputs have been instantiated.
   *
   * Note: There should be only one EventMessageAggregator per application. In contrast to the StatusAggregator, there
   * is no hierarchical aggregation, and the aggregation is not limited to specific tags. All AggregatableMessage
   * outputs found in the application will be aggregated.
   */
  class EventMessageAggregator : public ModuleGroup {
   public:
    using ModuleGroup::ModuleGroup;

    EventMessageAggregator() : _impl() {}

   private:
    class Impl : public ApplicationModule {
     public:
      Impl(ModuleGroup* owner, const std::string& name, const std::string& description,
          const std::unordered_set<std::string>& tags = {});

      using ApplicationModule::ApplicationModule;

      void mainLoop() override;

     protected:
      std::vector<ScalarPushInput<std::string>> _inputs;

      StatusWithMessage _output{this, "event", ""};

      VoidInput _clear{this, "clear", ""};
      ScalarPushInput<uint64_t> _autoClearTimer{this, "AutoClearTimer/tick", "", ""};

      uint32_t _autoClearSeconds{appConfig().get<uint32_t>("Configuration/autoClearEventMessage", 300)};
    };

    Impl _impl{this, ".", ""};

    PeriodicTrigger _autoClearTimer{this, "AutoClearTimer", "1 Hz trigger for the auto clear timer"};
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
