// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Model.h"
#include "Module.h"

#include <boost/thread.hpp>

#include <list>

namespace ChimeraTK {

  /********************************************************************************************************************/

  class ApplicationModule;
  class ModuleGroup;
  struct ConfigReader;

  /********************************************************************************************************************/

  class VariableGroup : public Module {
   public:
    /**
     * Constructor: Create VariableGroup register it with its owner.
     *
     * @param owner The owning VariableGroup or ApplicationModule.
     * @param name Name of this group. The name may be qualified (e.g. start with "../" or "/").
     * @param description A short description of this group.
     * @param tags List of tags to be attached to all owned variables (directly or indirectly).
     */
    VariableGroup(VariableGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    /**
     * Deprecated constructor with HierarchyModifier for backwards compatibility. Use constructor without
     * HierarchyModifier and if necessary qualified names instead.
     */
    [[deprecated]] VariableGroup(VariableGroup* owner, const std::string& name, const std::string& description,
        HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags = {});

    /**
     * Default constructor: Allows late initialisation of VariableGroups (e.g. when creating arrays of VariableGroups).
     */
    VariableGroup() = default;

    /** Move constructor */
    VariableGroup(VariableGroup&& other) noexcept { operator=(std::move(other)); }
    VariableGroup(const VariableGroup& other) noexcept = delete;

    /** Move assignment */
    VariableGroup& operator=(VariableGroup&& other) noexcept;
    VariableGroup& operator=(const VariableGroup& other) noexcept = delete;

    ModuleType getModuleType() const override { return ModuleType::VariableGroup; }

    /** Return the application model proxy representing this module */
    ChimeraTK::Model::VariableGroupProxy getModel() { return _model; }

    std::string getVirtualQualifiedName() const override;

   protected:
    ChimeraTK::Model::VariableGroupProxy _model;

   private:
    friend class ApplicationModule;
    /** Constructor: Create ModuleGroup by the given name with the given description and register it with its
     *  owner. The hierarchy will be modified according to the hierarchyModifier (when VirtualModules are created e.g.
     *  in findTag()). The specified list of tags will be added to all elements directly or indirectly owned by this
     *  instance.
     *
     *  Note: VariableGroups may only be owned by ApplicationModules or other VariableGroups. */
    VariableGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {});
  };

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
