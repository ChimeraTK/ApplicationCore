// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

namespace ChimeraTK {

  /********************************************************************************************************************/

  /**
   * Struct to define the direction of variables. The main direction is defined with an enum. In addition the presence
   * of a return channel is specified.
   */
  struct VariableDirection {
    /**
     * Enum to define directions of variables. The direction is always defined from the point-of-view of the owner, i.e.
     * the application module owning the instance of the accessor in this context.
     */
    enum { consuming, feeding, invalid } dir;

    /** Presence of return channel */
    bool withReturn;

    /** Comparison */
    bool operator==(const VariableDirection& other) const { return dir == other.dir && withReturn == other.withReturn; }
    bool operator!=(const VariableDirection& other) const { return !operator==(other); }
  };

  /********************************************************************************************************************/

  /** Enum to define the update mode of variables. */
  enum class UpdateMode { poll, push, invalid };

  /********************************************************************************************************************/

  /** Enum to define types of VariableNetworkNode */
  enum class NodeType { Device, ControlSystem, Application, TriggerReceiver, TriggerProvider, Constant, invalid };

  /********************************************************************************************************************/

  /** Enum to define the life-cycle states of an Application. */
  enum class LifeCycleState {
    initialisation, ///< Initialisation phase including ApplicationModule::prepare(). Single threaded operation. All
    ///< devices are closed.
    run, ///< Actual run phase with full multi threading. The state is entered right before the threads are launched, so
    ///< there is no guarantee that the application threads have already reached a certain point. Devices will be opened
    ///< after this point.
    shutdown ///< The application is in the process of shutting down.
  };

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
