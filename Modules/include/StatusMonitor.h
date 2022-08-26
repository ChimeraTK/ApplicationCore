// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*!
 * \page statusmonitordoc Status Monitor
 *
 *
 * To monitor a status of a varaible in an appplicaiton this group of
 * modules provides different possiblites.
 * It includes
 *  - MaxMonitor to monitor a value depending upon two MAX thresholds for warning and fault.
 *  - MinMonitor to monitor a value depending upon two MIN thresholds for warning and fault.
 *  - RangeMonitor to monitor a value depending upon two ranges of thresholds for warning and fault.
 *  - ExactMonitor to monitor a value which should be exactly same as required value.
 *  - StateMonitor to monitor On/Off state.
 * Depending upon the value and condition on of the four states are reported.
 *  -  OFF, OK, WARNING, FAULT.
 *
 * Checkout the status monitor example to see in detail how it works.
 * \include demoStatusMonitor.cc
 */

/**
For more info see \ref statusmonitordoc
\example demoStatusMonitor.cc
*/

/** Generic modules for status monitoring.
 * Each module monitors an input variable and depending upon the
 * conditions reports four different states.
 */
#include "ApplicationModule.h"
#include "HierarchyModifyingGroup.h"
#include "ScalarAccessor.h"
#include "StatusAccessor.h"
#include "VariableGroup.h"

namespace ChimeraTK {

  /** Common base for StatusMonitors
   *
   *  This holds common process variables that are not dependant on the
   *  type of the variable to be monitored. A non-template base class
   *  facilitates checking for the type in the StatusAggregator, which
   *  needs to identify any StatusMonitor.
   *
   *  FIXME This distinction between StatusMonitorImpl and StatusMonitor is no longer required. Merge the classes!
   */
  struct StatusMonitor : ApplicationModule {
    StatusMonitor(EntityOwner* owner, const std::string& name, const std::string& description, const std::string& input,
        const std::string& output, HierarchyModifier modifier, const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {}, const std::unordered_set<std::string>& tags = {})
    : ApplicationModule(owner, name, description, modifier, tags), _parameterTags(parameterTags), _input(input),
      status(this, output, "", outputTags) {}

    StatusMonitor(StatusMonitor&&) = default;

    ~StatusMonitor() override {}
    void prepare() override {}

    /**Tags for parameters. This makes it easier to connect them to e.g, to control system*/
    std::unordered_set<std::string> _parameterTags;

    const std::string _input;

    /** Status to be reported */
    StatusOutput status;

    /** Disable the monitor. The status will always be OFF. You don't have to connect this input.
     *  When there is no feeder, ApplicationCore will connect it to a constant feeder with value 0, hence the monitor is
     * always enabled.
     */
    ScalarPushInput<int> disable{this, "disable", "", "Disable the status monitor"};
  };

  /** Common template base class for StatusMonitors
   *
   *  This provides a ScalarPushInput for the variable to be monitored, which
   *  can be specified by the input parameter of the constructor.
   */
  template<typename T>
  struct StatusMonitorImpl : public StatusMonitor {
    /** Number of convience constructors for ease of use.
     * The input and output variable names can be given by user which
     * should be mapped with the variables of module to be watched.
     */
    StatusMonitorImpl(EntityOwner* owner, const std::string& name, const std::string& description,
        const std::string& input, const std::string& output, HierarchyModifier modifier,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {}, const std::unordered_set<std::string>& tags = {})
    : StatusMonitor(owner, name, description, input, output, modifier, outputTags, parameterTags, tags),
      oneUp(this, input) {}

    StatusMonitorImpl() { throw logic_error("Default constructor unusable. Just exists to work around gcc bug."); }
    StatusMonitorImpl(StatusMonitorImpl&&) = default;

    ~StatusMonitorImpl() override {}
    void prepare() override {}

    /**Input value that should be monitored. It is moved one level up, so it's parallel to this monitor object.*/
    struct OneUp : public VariableGroup {
      OneUp(EntityOwner* owner, const std::string& watchName)
      : VariableGroup(owner, "hidden", "", HierarchyModifier::oneUpAndHide), watch(this, watchName, "", "") {}
      ScalarPushInput<T> watch;
    } oneUp;
  };

  /** Module for status monitoring depending on a maximum threshold value*/
  template<typename T>
  struct MaxMonitor : public StatusMonitorImpl<T> {
    using StatusMonitorImpl<T>::StatusMonitorImpl;
    /** WARNING state to be reported if threshold is reached or exceeded*/
    ScalarPushInput<T> warning{this, "upperWarningThreshold", "", "", StatusMonitor::_parameterTags};
    /** FAULT state to be reported if threshold is reached or exceeded*/
    ScalarPushInput<T> fault{this, "upperFaultThreshold", "", "", StatusMonitor::_parameterTags};

    /**This is where state evaluation is done*/
    void mainLoop() {
      /** If there is a change either in value monitored or in thershold values, the status is re-evaluated*/
      ReadAnyGroup group{StatusMonitorImpl<T>::oneUp.watch, StatusMonitorImpl<T>::disable, warning, fault};
      while(true) {
        // evaluate and publish first, then read and wait. This takes care of the publishing the initial variables
        if(StatusMonitorImpl<T>::disable != 0) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::OFF;
        }
        else if(StatusMonitorImpl<T>::oneUp.watch >= fault) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::FAULT;
        }
        else if(StatusMonitorImpl<T>::oneUp.watch >= warning) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::WARNING;
        }
        else {
          StatusMonitorImpl<T>::status = StatusOutput::Status::OK;
        }
        StatusMonitorImpl<T>::status.write();
        group.readAny();
      }
    }
  };

  /** Module for status monitoring depending on a minimum threshold value*/
  template<typename T>
  struct MinMonitor : public StatusMonitorImpl<T> {
    using StatusMonitorImpl<T>::StatusMonitorImpl;

    /** WARNING state to be reported if threshold is crossed*/
    ScalarPushInput<T> warning{this, "lowerWarningThreshold", "", "", StatusMonitor::_parameterTags};
    /** FAULT state to be reported if threshold is crossed*/
    ScalarPushInput<T> fault{this, "lowerFaultThreshold", "", "", StatusMonitor::_parameterTags};

    /**This is where state evaluation is done*/
    void mainLoop() {
      /** If there is a change either in value monitored or in thershold values, the status is re-evaluated*/
      ReadAnyGroup group{StatusMonitorImpl<T>::oneUp.watch, StatusMonitorImpl<T>::disable, warning, fault};
      while(true) {
        if(StatusMonitorImpl<T>::disable != 0) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::OFF;
        }
        else if(StatusMonitorImpl<T>::oneUp.watch <= fault) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::FAULT;
        }
        else if(StatusMonitorImpl<T>::oneUp.watch <= warning) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::WARNING;
        }
        else {
          StatusMonitorImpl<T>::status = StatusOutput::Status::OK;
        }
        StatusMonitorImpl<T>::status.write();
        group.readAny();
      }
    }
  };

  /** Module for status monitoring depending on range of threshold values.
   * As long as a monitored value is in the range defined by user it goes
   * to fault or warning state. If the monitored value exceeds the upper limmit
   * or goes under the lowerthreshold the state reported will be always OK.
   * IMPORTANT: This module does not check for ill logic, so make sure to
   * set the ranges correctly to issue warning or fault.
   */
  template<typename T>
  struct RangeMonitor : public StatusMonitorImpl<T> {
    using StatusMonitorImpl<T>::StatusMonitorImpl;

    /** WARNING state to be reported if value is in between the upper and
     * lower threshold including the start and end of thresholds.
     */
    ScalarPushInput<T> warningUpperThreshold{this, "upperWarningThreshold", "", "", StatusMonitor::_parameterTags};
    ScalarPushInput<T> warningLowerThreshold{this, "lowerWarningThreshold", "", "", StatusMonitor::_parameterTags};
    /** FAULT state to be reported if value is in between the upper and
     * lower threshold including the start and end of thresholds.
     */
    ScalarPushInput<T> faultUpperThreshold{this, "upperFaultThreshold", "", "", StatusMonitor::_parameterTags};
    ScalarPushInput<T> faultLowerThreshold{this, "lowerFaultThreshold", "", "", StatusMonitor::_parameterTags};

    /**This is where state evaluation is done*/
    void mainLoop() {
      /** If there is a change either in value monitored or in thershold values, the status is re-evaluated*/
      ReadAnyGroup group{StatusMonitorImpl<T>::oneUp.watch, StatusMonitorImpl<T>::disable, warningUpperThreshold,
          warningLowerThreshold, faultUpperThreshold, faultLowerThreshold};
      while(true) {
        if(StatusMonitorImpl<T>::disable != 0) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::OFF;
        }
        // Check for fault limits first. Like this they supersede the warning,
        // even if they are stricter then the warning limits (mis-configuration)
        else if(StatusMonitorImpl<T>::oneUp.watch <= faultLowerThreshold ||
            StatusMonitorImpl<T>::oneUp.watch >= faultUpperThreshold) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::FAULT;
        }
        else if(StatusMonitorImpl<T>::oneUp.watch <= warningLowerThreshold ||
            StatusMonitorImpl<T>::oneUp.watch >= warningUpperThreshold) {
          StatusMonitorImpl<T>::status = StatusOutput::Status::WARNING;
        }
        else {
          StatusMonitorImpl<T>::status = StatusOutput::Status::OK;
        }
        StatusMonitorImpl<T>::status.write();
        group.readAny();
      }
    }
  };

  /**
   *  Module for status monitoring of an exact value.
   *
   *  If monitored input value is not exactly the same as the requiredValue, a fault state will be reported. If the
   *  parameter variable "disable" is set to a non-zero value, the monitoring is disabled and the output status is
   *  always OFF.
   *
   *  Note: It is strongly recommended to use this monitor only for integer data types or strings, as floating point
   *  data types should never be compared with exact equality.
   */
  template<typename T>
  struct ExactMonitor : ApplicationModule {
    /**
     *  Constructor for exact monitoring module.
     *
     *  inputPath: qualified path of the variable to monitor
     *  outputPath: qualified path of the status output variable
     *  parameterPath: qualified path of the VariableGroup holding the parameter variables requiredValue and disable
     *
     *  All qualified paths can be either relative or absolute to the given owner. See HierarchyModifyingGroup for
     *  more details.
     */
    ExactMonitor(EntityOwner* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& parameterPath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {})
    : ExactMonitor(owner, inputPath, outputPath, parameterPath + "/requiredValue", parameterPath + "/disable",
          description, outputTags, parameterTags) {}

    /**
     *  Constructor for exact monitoring module.
     *
     *  inputPath: qualified path of the variable to monitor
     *  outputPath: qualified path of the status output variable
     *  requiredValuePath: qualified path of the parameter variable requiredValue
     *  disablePath: qualified path of the parameter variable disable
     *
     *  All qualified paths can be either relative or absolute to the given owner. See HierarchyModifyingGroup for
     *  more details.
     */
    ExactMonitor(EntityOwner* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& requiredValuePath, const std::string& disablePath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {})
    : ApplicationModule(owner, "hidden", description, HierarchyModifier::hideThis),
      watch(this, inputPath, "", "Value to monitor"),
      requiredValue(this, requiredValuePath, "", "Value to compare with", parameterTags),
      disable(this, disablePath, "", "Disable the status monitor", parameterTags),
      status(this, outputPath, "Resulting status", outputTags) {}

    ExactMonitor() = default;

    /** Variable to monitor */
    ModifyHierarchy<ScalarPushInput<T>> watch;

    /** The required value to compare with */
    ModifyHierarchy<ScalarPushInput<T>> requiredValue;

    /** Disable/enable the entire status monitor */
    ModifyHierarchy<ScalarPushInput<int>> disable;

    /** Result of the monitor */
    ModifyHierarchy<StatusOutput> status;

    /** This is where state evaluation is done */
    void mainLoop() {
      // If there is a change either in value monitored or in requiredValue, the status is re-evaluated
      ReadAnyGroup group{watch.value, disable.value, requiredValue.value};

      DataValidity lastStatusValidity = DataValidity::ok;

      while(true) {
        StatusOutput::Status newStatus;
        if(disable.value != 0) {
          newStatus = StatusOutput::Status::OFF;
        }
        else if(watch.value != requiredValue.value) {
          newStatus = StatusOutput::Status::FAULT;
        }
        else {
          newStatus = StatusOutput::Status::OK;
        }

        // update only if status has changed, but always in case of initial value
        if(status.value != newStatus || getDataValidity() != lastStatusValidity ||
            status.value.getVersionNumber() == VersionNumber{nullptr}) {
          status.value = newStatus;
          status.value.write();
          lastStatusValidity = getDataValidity();
        }
        group.readAny();
      }
    }
  };

} // namespace ChimeraTK
