/*
 * VariableGroup.h
 *
 *  Created on: Nov 8, 2016
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_VARIABLE_GROUP_H
#define CHIMERATK_VARIABLE_GROUP_H

#include <list>

#include <boost/thread.hpp>

#include "ModuleImpl.h"

namespace ChimeraTK {

  class ApplicationModule;
  struct ConfigReader;

  class VariableGroup : public ModuleImpl {
   public:
    /** Constructor: Create ModuleGroup by the given name with the given description and register it with its
     *  owner. The hierarchy will be modified according to the hierarchyModifier (when VirtualModules are created e.g.
     *  in findTag()). The specified list of tags will be added to all elements directly or indirectly owned by this
     *  instance.
     *
     *  Note: VariableGroups may only be owned by ApplicationModules or other VariableGroups. */
    VariableGroup(EntityOwner* owner, const std::string& name, const std::string& description,
        HierarchyModifier hierarchyModifier = HierarchyModifier::none,
        const std::unordered_set<std::string>& tags = {});

    /** Deprecated form of the constructor. Use the new signature instead. */
    [[deprecated]] VariableGroup(EntityOwner* owner, const std::string& name, const std::string& description,
        bool eliminateHierarchy, const std::unordered_set<std::string>& tags = {});

    /** Default constructor: Allows late initialisation of VariableGroups (e.g.
     * when creating arrays of VariableGroups).
     *
     *  This construtor also has to be here to mitigate a bug in gcc. It is needed
     * to allow constructor inheritance of modules owning other modules. This
     * constructor will not actually be called then. See this bug report:
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67054 */
    VariableGroup() {}

    /** Destructor */
    virtual ~VariableGroup(){};

    /** Move constructor */
    VariableGroup(VariableGroup&& other) { operator=(std::move(other)); }

    /** Move assignment */
    VariableGroup& operator=(VariableGroup&& other) {
      ModuleImpl::operator=(std::move(other));
      return *this;
    }

    ModuleType getModuleType() const override { return ModuleType::VariableGroup; }
  };

} /* namespace ChimeraTK */

#endif /* CHIMERATK_VARIABLE_GROUP_H */
