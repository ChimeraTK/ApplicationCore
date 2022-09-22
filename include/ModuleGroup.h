// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Model.h"
#include "ModuleImpl.h"

#include <boost/thread.hpp>

#include <list>

namespace ChimeraTK {

  class Application;

  class ModuleGroup : public ModuleImpl {
   public:
    /**
     *  Constructor: Create ModuleGroup by the given name with the given description and register it with its
     *  owner. The specified list of tags will be added to all elements directly or indirectly owned by this
     *  instance.
     *
     *  Note: ModuleGroups may only be owned by the Application or other ModuleGroups.
     */
    ModuleGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
        const std::unordered_set<std::string>& tags = {});

    /** Deprecated form of the constructor. Use the new signature without hierarchy modifier and if necessary qualified
     * names instead. */
    [[deprecated]] ModuleGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
        HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags = {});

    /** Deprecated form of the constructor. Use the new signature instead. */
    [[deprecated]] ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
        HierarchyModifier hierarchyModifier = HierarchyModifier::none,
        const std::unordered_set<std::string>& tags = {});

    /** Very Deprecated form of the constructor. Use the new signature instead. */
    [[deprecated]] ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
        bool eliminateHierarchy, const std::unordered_set<std::string>& tags);

    /// Default constructor to allow late initialisation of module groups
    ModuleGroup() = default;

    /** Move constructor */
    ModuleGroup(ModuleGroup&& other) noexcept { operator=(std::move(other)); }

    /** Move assignment */
    ModuleGroup& operator=(ModuleGroup&& other) noexcept;

    ModuleType getModuleType() const override { return ModuleType::ModuleGroup; }

    /** Return the application model proxy representing this module */
    ChimeraTK::Model::ModuleGroupProxy getModel() { return _model; }

    std::string getVirtualQualifiedName() const override;

   private:
    friend class Application;
    friend class DeviceModule;

    /// Constructor used by Application and DeviceModule bypassing model registration
    explicit ModuleGroup(ModuleGroup* owner, const std::string& name);

    /// Application model proxy representing this module
    Model::ModuleGroupProxy _model;
  };

} /* namespace ChimeraTK */
