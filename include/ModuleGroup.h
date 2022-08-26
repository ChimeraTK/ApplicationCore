// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ModuleImpl.h"

#include <boost/thread.hpp>

#include <list>

namespace ChimeraTK {

  class ModuleGroup : public ModuleImpl {
   public:
    /** Constructor: Create ModuleGroup by the given name with the given description and register it with its
     *  owner. The hierarchy will be modified according to the hierarchyModifier (when VirtualModules are created e.g.
     *  in findTag()). The specified list of tags will be added to all elements directly or indirectly owned by this
     *  instance.
     *
     *  Note: ModuleGroups may only be owned by the Application or other ModuleGroups. */
    ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
        HierarchyModifier hierarchyModifier = HierarchyModifier::none,
        const std::unordered_set<std::string>& tags = {});

    /** Deprecated form of the constructor. Use the new signature instead. */
    ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description, bool eliminateHierarchy,
        const std::unordered_set<std::string>& tags = {});

    /** Default constructor: Allows late initialisation of ModuleGroups (e.g. when
     * creating arrays of ModuleGroups).
     *
     *  This construtor also has to be here to mitigate a bug in gcc. It is needed
     * to allow constructor inheritance of modules owning other modules. This
     * constructor will not actually be called then. See this bug report:
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67054 */
    ModuleGroup() : ModuleImpl() {}

    /** Move constructor */
    ModuleGroup(ModuleGroup&& other) { operator=(std::move(other)); }

    /** Move assignment */
    ModuleGroup& operator=(ModuleGroup&& other);

    ModuleType getModuleType() const override { return ModuleType::ModuleGroup; }
  };

} /* namespace ChimeraTK */
