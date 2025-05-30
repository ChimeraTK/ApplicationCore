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
#include "ScalarAccessor.h"
#include "StatusAccessor.h"

namespace ChimeraTK {

  /********************************************************************************************************************/
  /* Declaration of MonitorBase **************************************************************************************/
  /********************************************************************************************************************/
  struct MonitorBase : ApplicationModule {
    // make constructors protected not to allow of instantiancion of this object - this is just a base class for other monitors
   protected:
    MonitorBase(ModuleGroup* owner, const std::string& description, const std::string& outputPath,
        const std::string& disablePath, const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    MonitorBase() = default;

   public:
    /** Disable/enable the entire status monitor */
    ScalarPushInput<ChimeraTK::Boolean> disable;
    /** Result of the monitor */
    StatusOutput status;

   protected:
    DataValidity _lastStatusValidity = DataValidity::ok;
    void setStatus(StatusOutput::Status newStatus);
  };

  /********************************************************************************************************************/
  /* Declaration of MaxMonitor ***************************************************************************************/
  /********************************************************************************************************************/

  /** Module for status monitoring depending on a maximum threshold value*/
  template<typename T>
  struct MaxMonitor : MonitorBase {
    MaxMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& parameterPath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    MaxMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& warningThresholdPath, const std::string& faultThresholdPath, const std::string& disablePath,
        const std::string& description, const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    MaxMonitor() = default;

    /** Variable to monitor */
    ScalarPushInput<T> watch;

    /** WARNING state to be reported if threshold is reached or exceeded*/
    ScalarPushInput<T> warningThreshold;

    /** FAULT state to be reported if threshold is reached or exceeded*/
    ScalarPushInput<T> faultThreshold;

    /** Disable the monitor. The status will always be OFF. You don't have to connect this input.
     *  When there is no feeder, ApplicationCore will connect it to a constant feeder with value 0, hence the monitor is
     * always enabled.
     */

    /** This is where state evaluation is done */
    void mainLoop() override;
  };

  /********************************************************************************************************************/
  /* Declaration of MinMonitor ***************************************************************************************/
  /********************************************************************************************************************/

  /** Module for status monitoring depending on a minimum threshold value*/
  template<typename T>
  struct MinMonitor : MonitorBase {
    MinMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& parameterPath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    MinMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& warningThresholdPath, const std::string& faultThresholdPath, const std::string& disablePath,
        const std::string& description, const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    MinMonitor() = default;

    /** Variable to monitor */
    ScalarPushInput<T> watch;

    /** WARNING state to be reported if threshold is reached or exceeded*/
    ScalarPushInput<T> warningThreshold;

    /** FAULT state to be reported if threshold is reached or exceeded*/
    ScalarPushInput<T> faultThreshold;

    /** This is where state evaluation is done */
    void mainLoop() override;
  };

  /********************************************************************************************************************/
  /* Declaration of RangeMonitor *************************************************************************************/
  /********************************************************************************************************************/

  /** Module for status monitoring depending on range of threshold values.
   * As long as a monitored value is in the range defined by user it goes
   * to fault or warning state. If the monitored value exceeds the upper limmit
   * or goes under the lower threshold the state reported will be always OK.
   * IMPORTANT: This module does not check for ill logic, so make sure to
   * set the ranges correctly to issue warning or fault.
   */
  template<typename T>
  struct RangeMonitor : MonitorBase {
    RangeMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& parameterPath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    RangeMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& warningLowerThresholdPath, const std::string& warningUpperThresholdPath,
        const std::string& faultLowerThresholdPath, const std::string& faultUpperThresholdPath,
        const std::string& disablePath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    RangeMonitor() = default;

    /** Variable to monitor */
    ScalarPushInput<T> watch;

    /** WARNING state to be reported if value is in between the upper and
     * lower threshold including the start and end of thresholds.
     */
    ScalarPushInput<T> warningLowerThreshold;
    ScalarPushInput<T> warningUpperThreshold;

    /** FAULT state to be reported if value is in between the upper and
     * lower threshold including the start and end of thresholds.
     */
    ScalarPushInput<T> faultLowerThreshold;
    ScalarPushInput<T> faultUpperThreshold;

    /** This is where state evaluation is done */
    void mainLoop() override;
  };

  /********************************************************************************************************************/
  /* Declaration of ExactMonitor *************************************************************************************/
  /********************************************************************************************************************/

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
  struct ExactMonitor : MonitorBase {
    /**
     *  Constructor for exact monitoring module.
     *
     *  inputPath: qualified path of the variable to monitor
     *  outputPath: qualified path of the status output variable
     *  parameterPath: qualified path of the VariableGroup holding the parameter variables requiredValue and disable
     *
     *  All qualified paths can be either relative or absolute to the given owner.
     */
    ExactMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& parameterPath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    /**
     *  Constructor for exact monitoring module.
     *
     *  inputPath: qualified path of the variable to monitor
     *  outputPath: qualified path of the status output variable
     *  requiredValuePath: qualified path of the parameter variable requiredValue
     *  disablePath: qualified path of the parameter variable disable
     *
     *  All qualified paths can be either relative or absolute to the given owner.
     */
    ExactMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
        const std::string& requiredValuePath, const std::string& disablePath, const std::string& description,
        const std::unordered_set<std::string>& outputTags = {},
        const std::unordered_set<std::string>& parameterTags = {});

    ExactMonitor() = default;

    /** Variable to monitor */
    ScalarPushInput<T> watch;

    /** The required value to compare with */
    ScalarPushInput<T> requiredValue;

    /** This is where state evaluation is done */
    void mainLoop() override;
  };

  /********************************************************************************************************************/
  /* Implementation starts here **************************************************************************************/
  /********************************************************************************************************************/

  /********************************************************************************************************************/
  /* Implementation of MaxMonitor ************************************************************************************/
  /********************************************************************************************************************/
  template<typename T>
  MaxMonitor<T>::MaxMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& parameterPath, const std::string& description,
      const std::unordered_set<std::string>& outputTags, const std::unordered_set<std::string>& parameterTags)
  : MaxMonitor(owner, inputPath, outputPath, parameterPath + "/upperWarningThreshold",
        parameterPath + "/upperFaultThreshold", parameterPath + "/disable", description, outputTags, parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  MaxMonitor<T>::MaxMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& warningThresholdPath, const std::string& faultThresholdPath, const std::string& disablePath,
      const std::string& description, const std::unordered_set<std::string>& outputTags,
      const std::unordered_set<std::string>& parameterTags)
  : MonitorBase(owner, description, outputPath, disablePath, outputTags, parameterTags),
    watch(this, inputPath, "", "Value to monitor"),
    warningThreshold(this, warningThresholdPath, "", "Warning threshold to compare with", parameterTags),
    faultThreshold(this, faultThresholdPath, "", "Fault threshold to compare with", parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  void MaxMonitor<T>::mainLoop() {
    // If there is a change either in value monitored or in requiredValue, the status is re-evaluated
    ReadAnyGroup group{watch, disable, warningThreshold, faultThreshold};

    while(true) {
      if(disable) {
        setStatus(StatusOutput::Status::OFF);
      }
      else if(watch >= faultThreshold) {
        setStatus(StatusOutput::Status::FAULT);
      }
      else if(watch >= warningThreshold) {
        setStatus(StatusOutput::Status::WARNING);
      }
      else {
        setStatus(StatusOutput::Status::OK);
      }
      group.readAny();
    }
  }

  /********************************************************************************************************************/
  /* Implementation of MinMonitor ************************************************************************************/
  /********************************************************************************************************************/
  template<typename T>
  MinMonitor<T>::MinMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& parameterPath, const std::string& description,
      const std::unordered_set<std::string>& outputTags, const std::unordered_set<std::string>& parameterTags)
  : MinMonitor(owner, inputPath, outputPath, parameterPath + "/lowerWarningThreshold",
        parameterPath + "/lowerFaultThreshold", parameterPath + "/disable", description, outputTags, parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  MinMonitor<T>::MinMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& warningThresholdPath, const std::string& faultThresholdPath, const std::string& disablePath,
      const std::string& description, const std::unordered_set<std::string>& outputTags,
      const std::unordered_set<std::string>& parameterTags)
  : MonitorBase(owner, description, outputPath, disablePath, outputTags, parameterTags),
    watch(this, inputPath, "", "Value to monitor"),
    warningThreshold(this, warningThresholdPath, "", "Warning threshold to compare with", parameterTags),
    faultThreshold(this, faultThresholdPath, "", "Fault threshold to compare with", parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  void MinMonitor<T>::mainLoop() {
    // If there is a change either in value monitored or in requiredValue, the status is re-evaluated
    ReadAnyGroup group{watch, disable, warningThreshold, faultThreshold};

    while(true) {
      if(disable) {
        setStatus(StatusOutput::Status::OFF);
      }
      else if(watch <= faultThreshold) {
        setStatus(StatusOutput::Status::FAULT);
      }
      else if(watch <= warningThreshold) {
        setStatus(StatusOutput::Status::WARNING);
      }
      else {
        setStatus(StatusOutput::Status::OK);
      }
      group.readAny();
    }
  }

  /********************************************************************************************************************/
  /* Implementation of RangeMonitor **********************************************************************************/
  /********************************************************************************************************************/
  template<typename T>
  RangeMonitor<T>::RangeMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& parameterPath, const std::string& description,
      const std::unordered_set<std::string>& outputTags, const std::unordered_set<std::string>& parameterTags)
  : RangeMonitor(owner, inputPath, outputPath, parameterPath + "/lowerWarningThreshold",
        parameterPath + "/upperWarningThreshold", parameterPath + "/lowerFaultThreshold",
        parameterPath + "/upperFaultThreshold", parameterPath + "/disable", description, outputTags, parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  RangeMonitor<T>::RangeMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& warningLowerThresholdPath, const std::string& warningUpperThresholdPath,
      const std::string& faultLowerThresholdPath, const std::string& faultUpperThresholdPath,
      const std::string& disablePath, const std::string& description, const std::unordered_set<std::string>& outputTags,
      const std::unordered_set<std::string>& parameterTags)
  : MonitorBase(owner, description, outputPath, disablePath, outputTags, parameterTags),
    watch(this, inputPath, "", "Value to monitor"), warningLowerThreshold(this, warningLowerThresholdPath, "",
                                                        "Lower warning threshold to compare with", parameterTags),
    warningUpperThreshold(
        this, warningUpperThresholdPath, "", "Upper warning threshold to compare with", parameterTags),
    faultLowerThreshold(this, faultLowerThresholdPath, "", "Lower fault threshold to compare with", parameterTags),
    faultUpperThreshold(this, faultUpperThresholdPath, "", "Upper fault threshold to compare with", parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  void RangeMonitor<T>::mainLoop() {
    // If there is a change either in value monitored or in requiredValue, the status is re-evaluated
    ReadAnyGroup group{
        watch, disable, warningLowerThreshold, warningUpperThreshold, faultLowerThreshold, faultUpperThreshold};

    while(true) {
      if(disable) {
        setStatus(StatusOutput::Status::OFF);
      }
      // Check for fault limits first. Like this they supersede the warning,
      // even if they are stricter then the warning limits (mis-configuration)
      else if(watch <= faultLowerThreshold || watch >= faultUpperThreshold) {
        setStatus(StatusOutput::Status::FAULT);
      }
      else if(watch <= warningLowerThreshold || watch >= warningUpperThreshold) {
        setStatus(StatusOutput::Status::WARNING);
      }
      else {
        setStatus(StatusOutput::Status::OK);
      }
      group.readAny();
    }
  }

  /********************************************************************************************************************/
  /* Implementation of ExactMonitor **********************************************************************************/
  /********************************************************************************************************************/
  template<typename T>
  ExactMonitor<T>::ExactMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& parameterPath, const std::string& description,
      const std::unordered_set<std::string>& outputTags, const std::unordered_set<std::string>& parameterTags)
  : ExactMonitor(owner, inputPath, outputPath, parameterPath + "/requiredValue", parameterPath + "/disable",
        description, outputTags, parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  ExactMonitor<T>::ExactMonitor(ModuleGroup* owner, const std::string& inputPath, const std::string& outputPath,
      const std::string& requiredValuePath, const std::string& disablePath, const std::string& description,
      const std::unordered_set<std::string>& outputTags, const std::unordered_set<std::string>& parameterTags)
  : MonitorBase(owner, description, outputPath, disablePath, outputTags, parameterTags),
    watch(this, inputPath, "", "Value to monitor"),
    requiredValue(this, requiredValuePath, "", "Value to compare with", parameterTags) {}

  /********************************************************************************************************************/
  template<typename T>
  void ExactMonitor<T>::mainLoop() {
    // If there is a change either in value monitored or in requiredValue, the status is re-evaluated
    ReadAnyGroup group{watch, disable, requiredValue};

    while(true) {
      if(disable) {
        setStatus(StatusOutput::Status::OFF);
      }
      else if(watch != requiredValue) {
        setStatus(StatusOutput::Status::FAULT);
      }
      else {
        setStatus(StatusOutput::Status::OK);
      }
      group.readAny();
    }
  }

} // namespace ChimeraTK
