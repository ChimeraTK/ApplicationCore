// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationModule.h"
#include "ModuleGroup.h"
#include "StatusAccessor.h"
#include "StatusWithMessage.h"
#include "VoidAccessor.h"

#include <list>
#include <string>
#include <vector>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   * The StatusAggregator collects results of multiple StatusMonitor instances and aggregates them into a single status,
   * which can take the same values as the result of the individual monitors.
   *
   * It will search for all StatusOutputs from its point in hierarchy downwards, matching the tagsToAggregate passed to
   * the constructor. If a StatusOutputs beloging to another StatusAggregator is found (also matching the
   * tagsToAggregate) the search is not recursing further down at that branch, since the StatusAggregator already
   * represents the complete status of the branch below it. StatusAggregators created on the same hierarchy level (i.e.
   * sharing the owner) never aggregate each other.
   *
   * Note: The aggregated instances are collected on construction. Hence, the StatusAggregator has to be declared after
   * all instances that shall to be included in the scope (ModuleGroup, Application, ...) of interest.
   */
  struct StatusAggregator : ApplicationModule {
    /**
     *  Possible status priority modes used during aggregation of unequal Status values. The output Status value of the
     *  StatusAggregator will be equal to the current input Status value with the highest priority.
     *
     *  The priorities are listed with the possible values, highest priority first.
     *
     *  Hint for remembering the value names: f = fault, w = warning, o = off, k = ok
     */
    enum class PriorityMode {
      fwok,          ///< fault - warning - off - ok
      fwko,          ///< fault - warning - ok - off
      fw_warn_mixed, ///< fault - warning - ok or off, mixed state of ok or off results in warning
      ofwk           ///< off - fault - warning - ok
    };

    /**
     *  Construct StatusAggregator object.
     *
     *  The StatusAggregator is a module with a single output, the aggregated status. For convenience, the module itself
     *  is always hidden, and the outputName is interpreted as a qualified variable name, which can be relative or
     *  absolute. See the class description of the HierarchyModifyingGroup for more details.
     *
     *  The mode governs how multiple unequal input status values are aggregated into a single status. See the
     *  PriorityMode class description for details.
     *
     *  The tagsToAggregate are the tags which are required to be present at the aggregated StatusOutputs. StatusOutputs
     *  which do not have the specified tags are ignored. If no tag is specified, all StatusOutputs are aggregated. At
     *  the moment, at maximum only one tag may be specified.
     *
     *  outputTags is the list of tags which is attached to the aggregated output. This tag has no influence on the
     *  aggregation. Other StatusAggregators will aggregate the output based on the tagsToAggregate, not based on the
     *  outputTags. Any number of tags can be specified here. Typically no tag is specified (even if tagsToAggregate
     *  contains a tag), unless the output needs special treatment somewhere else (e.g. if it is included in the
     *  MicroDAQ system searching for a particular tag).
     *
     *  Note: The constructor will search for StatusOutputs to be aggregated. It can only find what has been constructed
     *  already. Make sure all StatusOutputs to be aggregated are constructed before this aggregator.
     */
    StatusAggregator(ModuleGroup* owner, const std::string& outputName, const std::string& description,
        PriorityMode mode = PriorityMode::fwok, std::unordered_set<std::string> tagsToAggregate = {},
        const std::unordered_set<std::string>& outputTags = {},
        std::string warnMixedMessage = "warning - StatusAggregator inputs have mixed values");

    StatusAggregator(StatusAggregator&& other) = default;
    StatusAggregator() = default;
    StatusAggregator& operator=(StatusAggregator&& other) = default;

    void mainLoop() override;

    DataValidity getDataValidity() const override;

    /** Set a custom message for the warn mixed state.
     */
    void setWarnMixedMessage(std::string message);

   protected:
    /// Recursivly search for StatusMonitors and other StatusAggregators
    void populateStatusInput();

    /** Reserved tag which is used to mark aggregated status outputs (need to stop searching further down the
     *  hierarchy) */
    constexpr static auto tagAggregatedStatus = "_ChimeraTK_StatusAggregator_aggregatedStatus";

    /** Reserved tag which is used to mark internal variables which should not be visible in the virtual hierachy. */
    constexpr static auto tagInternalVars = "_ChimeraTK_StatusAggregator_internalVars";

    /// The aggregated status output
    StatusWithMessage _output;

    /// All status inputs to be aggregated
    std::vector<StatusWithMessageInput> _inputs;

    /// Priority mode used in aggregation
    PriorityMode _mode{PriorityMode::fwok};

    /// List of tags to aggregate
    std::unordered_set<std::string> _tagsToAggregate;

    /// Convert Status value into a priority (high integer value = high priority), depending on chosen PriorityMode
    /// Return value of -1 has the special meaning that the input Status's must be all equal, otherwise it must result
    /// in a warning Status.
    int getPriority(StatusOutput::Status status) const;

    /// Allow runtime debugging
    VoidInput _debug{this, "/Debug/statusAggregators", "Print debug info for all status aggregators once."};

    /// Error message for the warn_mixed condition
    std::string _warnMixedMessage;
  };

  /********************************************************************************************************************/

} // namespace ChimeraTK
