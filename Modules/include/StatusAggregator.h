#ifndef CHIMERATK_STATUS_AGGREGATOR_H
#define CHIMERATK_STATUS_AGGREGATOR_H

#include "ApplicationModule.h"
#include "ModuleGroup.h"
#include "HierarchyModifyingGroup.h"
#include "ScalarAccessor.h"

#include <list>
#include <vector>
#include <string>

namespace ChimeraTK {

  /** Special ScalarOutput which represents a status which can be aggregated by the StatusAggregator. */
  struct StatusOutput : ScalarOutput<int32_t> {
    StatusOutput(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {})
    : ScalarOutput<int32_t>(owner, name, unit, description, tags) {
      addTag(tagStatusOutput);
    }
    StatusOutput() : ScalarOutput<int32_t>() {}
    using ScalarOutput<int32_t>::operator=;

    /** These are the states which can be reported */
    enum class Status : int32_t { OFF = 0, OK, WARNING, FAULT };

    /** Reserved tag which is used to mark status outputs */
    constexpr static auto tagStatusOutput = "_ChimeraTK_StatusOutput_statusOutput";

    /** This implicit type conversion is not implementable, cannot hand out a reference */
    operator Status&() = delete;

    /** Implicit type conversion to user type T to access the const value. */
    operator Status() const { return Status(get()->accessData(0, 0)); }

    /** Assignment operator, assigns the first element. */
    StatusOutput& operator=(Status rightHandSide) {
      get()->accessData(0, 0) = static_cast<int32_t>(rightHandSide);
      return *this;
    }

    /* Delete increment/decrement operators since they do not make much sense with a Status */
    void operator++() = delete;
    void operator++(int) = delete;
    void operator--() = delete;
    void operator--(int) = delete;
  };

  /**
   * The StatusAggregator collects results of multiple StatusMonitor instances and aggregates them into a single status,
   * which can take the same values as the result of the individual monitors.
   * 
   * It will search for all StatusOutputs from its point in hierarchy downwards, matching ALL tags which have been
   * passed to the constructor. If a StatusOutputs beloging to another StatusAggregator is found (also matching ALL
   * tags) the search is not recursing further down at that branch, since the StatusAggregator already represents the
   * complete status of the branch below it.
   *
   * Note: The aggregated instances are collected on construction. Hence, the StatusAggregator has to be declared after
   * all instances that shall to be included in the scope (ModuleGroup, Application, ...) of interest.
   * 
   * Note: The reason why ALL tags need to match (in contrast to only one of the tags) is that otherwise the assumption
   * that another StatusAggregator represents the complete status of the branch below it does not hold.
   */
  struct StatusAggregator : ApplicationModule {
    StatusAggregator(EntityOwner* owner, const std::string& name, const std::string& description,
        const std::string& output, HierarchyModifier modifier, const std::unordered_set<std::string>& tags = {});

    StatusAggregator() = default;
    StatusAggregator(StatusAggregator&&) = default;

    void mainLoop() override;

   protected:
    /// Recursivly search for StatusMonitors and other StatusAggregators
    void populateStatusInput();

    /// Helper for populateStatusInput
    void scanAndPopulateFromHierarchyLevel(const Module& module, bool firstLevel);

    /** Reserved tag which is used to mark aggregated status outputs (need to stop searching further down the
     *  hierarchy) */
    constexpr static auto tagAggregatedStatus = "_ChimeraTK_StatusAggregator_aggregatedStatus";

    /// The aggregated status output
    StatusOutput status;

    /// Holding group for status inputs to be aggregated
    struct StatusInputGroup : HierarchyModifyingGroup {
      StatusInputGroup(EntityOwner* owner, std::string qualifiedVariableName)
      : HierarchyModifyingGroup(owner, HierarchyModifyingGroup::getPathName(qualifiedVariableName), ""),
        statusInput(this, HierarchyModifyingGroup::getUnqualifiedName(qualifiedVariableName), "", "") {}
      ScalarPushInput<uint16_t> statusInput;
    };

    /// All status inputs to be aggregated
    std::vector<StatusInputGroup> inputs;
  };

} // namespace ChimeraTK
#endif // CHIMERATK_STATUS_AGGREGATOR_H
