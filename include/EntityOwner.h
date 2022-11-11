// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Flags.h"
#include "VariableNetworkNode.h"

#include <list>
#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  class AccessorBase;
  class Module;

  /********************************************************************************************************************/

  /**
   *  Convenience type definition which can optionally be used as a shortcut for the type which defines a list of
   *  tags.
   */
  using TAGS = const std::unordered_set<std::string>;

  /********************************************************************************************************************/

  /**
   *  Base class for owners of other EntityOwners (e.g. Modules) and Accessors.
   *  FIXME: Unify with Module class (not straight forward!).
   */
  class EntityOwner {
   public:
    /** Constructor: Create EntityOwner by the given name with the given description. The specified list of
     *  tags will be added to all elements directly or indirectly owned by this instance. */
    EntityOwner(std::string name, std::string description, std::unordered_set<std::string> tags = {});

    /** Default constructor just for late initialisation */
    EntityOwner();

    /** Virtual destructor to make the type polymorphic */
    virtual ~EntityOwner() = default;

    /** Move constructor */
    EntityOwner(EntityOwner&& other) noexcept { operator=(std::move(other)); }
    EntityOwner(const EntityOwner& other) = delete;

    /** Move assignment operator */
    EntityOwner& operator=(EntityOwner&& other) noexcept;
    EntityOwner& operator=(const EntityOwner& other) = delete;

    /** Get the name of the module instance */
    const std::string& getName() const { return _name; }

    /** Get the fully qualified name of the module instance, i.e. the name
     * containing all module names further up in the hierarchy. */
    virtual std::string getQualifiedName() const = 0;

    /** Get the description of the module instance */
    const std::string& getDescription() const { return _description; }

    /** Obtain the full description including the full description of the owner.
     */
    virtual std::string getFullDescription() const = 0;

    /** Obtain the list of accessors/variables directly associated with this
     * instance */
    std::list<VariableNetworkNode> getAccessorList() const { return _accessorList; }

    /** Obtain the list of submodules associated with this instance */
    std::list<Module*> getSubmoduleList() const { return _moduleList; }

    /** Obtain the list of accessors/variables associated with this instance and
     * any submodules */
    std::list<VariableNetworkNode> getAccessorListRecursive() const;

    /** Obtain the list of submodules associated with this instance and any
     * submodules */
    std::list<Module*> getSubmoduleListRecursive() const;

    /** Called inside the constructor of Accessor: adds the accessor to the list
     */
    void registerAccessor(VariableNetworkNode accessor);

    /** Called inside the destructor of Accessor: removes the accessor from the
     * list */
    void unregisterAccessor(const VariableNetworkNode& accessor) { _accessorList.remove(accessor); }

    /** Register another module as a sub-module. Will be called automatically by
     * all modules in their constructors. If addTags is set to false, the tags of
     * this EntityOwner will not be set to the module being registered. This is
     * e.g. used in the move-constructor of Module to prevent from altering the
     * tags in the move operation. */
    void registerModule(Module* module, bool addTags = true);

    /** Unregister another module as a sub-module. Will be called automatically by
     * all modules in their destructors. */
    void unregisterModule(Module* module);

    /** Add a tag to all Application-type nodes inside this group. It will recurse
     * into any subgroups. See VariableNetworkNode::addTag() for additional
     * information about tags. */
    void addTag(const std::string& tag);

    /** Print the full hierarchy to stdout. */
    void dump(const std::string& prefix = "") const;

    enum class ModuleType { ApplicationModule, ModuleGroup, VariableGroup, ControlSystem, Device, Invalid };

    /** Return the module type of this module, or in case of a VirtualModule the
     * module type this VirtualModule was derived from. */
    virtual ModuleType getModuleType() const = 0;

    /** Return the current version number which has been received with the last
     * push-type read operation. */
    virtual VersionNumber getCurrentVersionNumber() const = 0;

    /** Set the current version number. This function is called by the push-type
     * input accessors in their read functions. */
    virtual void setCurrentVersionNumber(VersionNumber versionNumber) = 0;

    /** Return the data validity flag. If any This function will be called by all output accessors in their write
     *  functions. */
    virtual DataValidity getDataValidity() const = 0;

    /** Set the data validity flag to fault and increment the fault counter. This function will be called by all input
     *  accessors when receiving the a faulty update if the previous update was ok. The caller of this function must
     *  ensure that calls to this function are paired to a subsequent call to decrementDataFaultCounter(). */
    virtual void incrementDataFaultCounter() = 0;

    /** Decrement the fault counter and set the data validity flag to ok if the counter has reached 0. This function
     *  will be called by all input accessors when receiving the an ok update if the previous update was faulty. The
     *  caller of this function must ensure that calles to this function are paired to a previous call to
     *  incrementDataFaultCounter(). */
    virtual void decrementDataFaultCounter() = 0;

    /** Use pointer to the module as unique identifier.*/
    virtual std::list<EntityOwner*> getInputModulesRecursively(std::list<EntityOwner*> startList) = 0;

    /** Get the ID of the circular dependency network (0 if none). This information is only available after
     *  the Application has finalised all connections.
     */
    virtual size_t getCircularNetworkHash() = 0;

    /** Check whether this module has declared that it reached the testable mode. */
    bool hasReachedTestableMode();

    /**
     * Create a variable name which will be automatically connected with a constant value. This can be used when a
     * constant value.
     */
    template<typename T>
    std::string constant(T value);

    /**
     * Prefix for costants created by constant().
     */
    static const std::string namePrefixConstant;

   protected:
    /** Convert HierarchyModifier into path qualification (for backwards compatibility only!) */
    void applyHierarchyModifierToName(HierarchyModifier hierarchyModifier);

    /** The name of this instance */
    std::string _name;

    /** The description of this instance */
    std::string _description;

    /** List of accessors owned by this instance */
    std::list<VariableNetworkNode> _accessorList;

    /** List of modules owned by this instance */
    std::list<Module*> _moduleList;

    /** List of tags to be added to all accessors and modules inside this module
     */
    std::unordered_set<std::string> _tags;

    /** Flag used by the testable mode to identify whether a thread within the EntityOwner has reached the point where
     *  the testable mode lock is acquired.
     *  @todo This should be moved to a more proper place in the hierarchy (e.g. ModuleImpl) after InternalModule class
     *  has been properly unified with the normal Module class. */
    std::atomic<bool> _testableModeReached{false};
  };

  /********************************************************************************************************************/

  template<typename T>
  std::string EntityOwner::constant(T value) {
    return namePrefixConstant + userTypeToUserType<std::string>(value);
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
