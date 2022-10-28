// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Model.h"

#include "ApplicationModule.h"
#include "DeviceManager.h"
#include "DeviceModule.h"
#include "ModuleGroup.h"
#include "Utilities.h"
#include "VariableGroup.h"

namespace ChimeraTK::Model {

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of Proxy base class */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  Proxy::Proxy(Vertex vertex, const std::shared_ptr<Impl>& impl)
  : _d(std::make_shared<ProxyData>(ProxyData{vertex, impl})) {}

  /********************************************************************************************************************/

  std::string Proxy::getFullyQualifiedPath() const {
    return _d->impl->getFullyQualifiedPath(_d->vertex);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of RootProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  RootProxy::RootProxy(ModuleGroup& app) : Proxy(0, std::make_shared<Impl>()) {
    // create root vertex
    _d->vertex = boost::add_vertex(_d->impl->graph);
    VertexProperties::RootProperties props{app};
    _d->impl->graph[_d->vertex].type = VertexProperties::Type::root;
    _d->impl->graph[_d->vertex].p.emplace<VertexProperties::RootProperties>(props);

    // create neighbourhood edge to itself: the root represents both the Application ModuleGroup and the directory.
    auto neighbourhoodEdge = boost::add_edge(_d->vertex, _d->vertex, _d->impl->graph).first;
    _d->impl->graph[neighbourhoodEdge].type = EdgeProperties::Type::neighbourhood;
  }

  /********************************************************************************************************************/

  ModuleGroupProxy RootProxy::add(ModuleGroup& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  ApplicationModuleProxy RootProxy::add(ApplicationModule& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  DeviceModuleProxy RootProxy::add(DeviceModule& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  DirectoryProxy RootProxy::addDirectory(const std::string& name) {
    return _d->impl->addDirectory(_d->vertex, name);
  }

  /********************************************************************************************************************/

  DirectoryProxy RootProxy::addDirectoryRecursive(const std::string& name) {
    return _d->impl->addDirectoryRecursive(_d->vertex, name);
  }

  /********************************************************************************************************************/

  ProcessVariableProxy RootProxy::addVariable(const std::string& name) {
    return _d->impl->addVariable(_d->vertex, name);
  }

  /********************************************************************************************************************/

  RootProxy::operator Model::ModuleGroupProxy() {
    return {_d->vertex, _d->impl};
  }

  /********************************************************************************************************************/

  RootProxy::operator Model::DirectoryProxy() {
    return {_d->vertex, _d->impl};
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  ModuleGroupProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& ModuleGroupProxy::getName() const {
    return std::get<VertexProperties::ModuleGroupProperties>(_d->impl->graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  ModuleGroupProxy ModuleGroupProxy::add(ModuleGroup& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  ApplicationModuleProxy ModuleGroupProxy::add(ApplicationModule& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  DeviceModuleProxy ModuleGroupProxy::add(DeviceModule& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  ModuleGroup& ModuleGroupProxy::getModuleGroup() const {
    return std::get<VertexProperties::ModuleGroupProperties>(_d->impl->graph[_d->vertex].p).module;
  }

  /********************************************************************************************************************/

  void ModuleGroupProxy::informMove(ModuleGroup& module) {
    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::ModuleGroupProperties>(_d->impl->graph[_d->vertex].p);

    _d->impl->graph[_d->vertex].p.emplace<VertexProperties::ModuleGroupProperties>(
        VertexProperties::ModuleGroupProperties{std::move(currentProps.name), module});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  ApplicationModuleProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& ApplicationModuleProxy::getName() const {
    return std::get<VertexProperties::ApplicationModuleProperties>(_d->impl->graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  VariableGroupProxy ApplicationModuleProxy::add(VariableGroup& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  void ApplicationModuleProxy::addVariable(const ProcessVariableProxy& variable, const VariableNetworkNode& node) {
    return _d->impl->addVariableNode(*this, variable, node);
  }

  /********************************************************************************************************************/

  ApplicationModuleProxy::operator Model::VariableGroupProxy() {
    return {_d->vertex, _d->impl};
  }

  /********************************************************************************************************************/

  ApplicationModule& ApplicationModuleProxy::getApplicationModule() const {
    return std::get<VertexProperties::ApplicationModuleProperties>(_d->impl->graph[_d->vertex].p).module;
  }

  /********************************************************************************************************************/

  void ApplicationModuleProxy::informMove(ApplicationModule& module) {
    assert(_d->impl->graph[_d->vertex].type == VertexProperties::Type::applicationModule);

    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::ApplicationModuleProperties>(_d->impl->graph[_d->vertex].p);

    _d->impl->graph[_d->vertex].p.emplace<VertexProperties::ApplicationModuleProperties>(
        VertexProperties::ApplicationModuleProperties{std::move(currentProps.name), module});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  VariableGroupProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& VariableGroupProxy::getName() const {
    return std::get<VertexProperties::VariableGroupProperties>(_d->impl->graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  VariableGroupProxy VariableGroupProxy::add(VariableGroup& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  void VariableGroupProxy::addVariable(const ProcessVariableProxy& variable, const VariableNetworkNode& node) {
    return _d->impl->addVariableNode(*this, variable, node);
  }
  /********************************************************************************************************************/

  ApplicationModuleProxy VariableGroupProxy::getOwningModule() const {
    Vertex currentVertex{_d->vertex};

    do {
      // there must be exactly one owner
      assert(boost::in_degree(currentVertex, _d->impl->ownershipView) == 1);

      // Update currentVertex to the owner of the previous currentVertex.
      currentVertex =
          boost::source(*boost::in_edges(currentVertex, _d->impl->ownershipView).first, _d->impl->ownershipView);

      // repeat until the type of the currentVertex is no longer a VariablGroup
    } while(_d->impl->ownershipView[currentVertex].type == VertexProperties::Type::variableGroup);

    // The type must now be a ApplicationModule
    assert(_d->impl->ownershipView[currentVertex].type == VertexProperties::Type::applicationModule);

    return {currentVertex, _d->impl};
  }

  /********************************************************************************************************************/

  VariableGroup& VariableGroupProxy::getVariableGroup() const {
    return std::get<VertexProperties::VariableGroupProperties>(_d->impl->graph[_d->vertex].p).module;
  }

  /********************************************************************************************************************/

  void VariableGroupProxy::informMove(VariableGroup& group) {
    assert(_d->impl->graph[_d->vertex].type == VertexProperties::Type::variableGroup);

    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::VariableGroupProperties>(_d->impl->graph[_d->vertex].p);

    _d->impl->graph[_d->vertex].p.emplace<VertexProperties::VariableGroupProperties>(
        VertexProperties::VariableGroupProperties{std::move(currentProps.name), group});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  DeviceModuleProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& DeviceModuleProxy::getAliasOrCdd() const {
    return std::get<VertexProperties::DeviceModuleProperties>(_d->impl->graph[_d->vertex].p).aliasOrCdd;
  }

  /********************************************************************************************************************/

  ProcessVariableProxy DeviceModuleProxy::getTrigger() const {
    return std::get<VertexProperties::DeviceModuleProperties>(_d->impl->graph[_d->vertex].p).trigger;
  }

  /********************************************************************************************************************/

  void DeviceModuleProxy::addVariable(const ProcessVariableProxy& variable, const VariableNetworkNode& node) {
    return _d->impl->addVariableNode(*this, variable, node);
  }

  /********************************************************************************************************************/

  void DeviceModuleProxy::informMove(DeviceModule& module) {
    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::DeviceModuleProperties>(_d->impl->graph[_d->vertex].p);

    _d->impl->graph[_d->vertex].p.emplace<VertexProperties::DeviceModuleProperties>(
        VertexProperties::DeviceModuleProperties{
            std::move(currentProps.aliasOrCdd), std::move(currentProps.trigger), module});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** ProcessVariableProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& ProcessVariableProxy::getName() const {
    return std::get<VertexProperties::ProcessVariableProperties>(_d->impl->graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  const std::vector<VariableNetworkNode>& ProcessVariableProxy::getNodes() const {
    return std::get<VertexProperties::ProcessVariableProperties>(_d->impl->graph[_d->vertex].p).nodes;
  }

  /********************************************************************************************************************/

  const std::unordered_set<std::string>& ProcessVariableProxy::getTags() const {
    return std::get<VertexProperties::ProcessVariableProperties>(_d->impl->graph[_d->vertex].p).tags;
  }

  /********************************************************************************************************************/

  void ProcessVariableProxy::addTag(const std::string& tag) {
    std::get<VertexProperties::ProcessVariableProperties>(_d->impl->graph[_d->vertex].p).tags.insert(tag);
  }

  /********************************************************************************************************************/

  void ProcessVariableProxy::removeNode(const VariableNetworkNode& node) {
    assert(node.getType() == NodeType::Application || node.getType() == NodeType::Device);

    // remove node from the list of nodes in the PV's vertex properties
    auto& nodes = std::get<VertexProperties::ProcessVariableProperties>(_d->impl->graph[_d->vertex].p).nodes;
    auto it = std::find(nodes.begin(), nodes.end(), node);
    if(it == nodes.end()) {
      return;
    }
    nodes.erase(it);

    // remove model relationships between PV and module
    if(node.getType() == NodeType::Application) {
      auto* vg = dynamic_cast<VariableGroup*>(node.getOwningModule());
      assert(vg != nullptr);

      // remove ownership edge to variable group (if any)
      auto vgm = vg->getModel();
      if(vgm.isValid()) {
        boost::remove_edge(vgm._d->vertex, _d->vertex, _d->impl->graph);
      }

      // remove pv access edge, and along side the ownership edge if directly owned by application module
      auto* am = dynamic_cast<ApplicationModule*>(vg->findApplicationModule());
      assert(am != nullptr);
      auto amm = am->getModel();

      assert(amm.isValid());
      boost::remove_edge(amm._d->vertex, _d->vertex, _d->impl->graph);
      boost::remove_edge(_d->vertex, amm._d->vertex, _d->impl->graph);
    }
    else if(node.getType() == NodeType::Device) {
      auto* dm = dynamic_cast<DeviceModule*>(node.getOwningModule());
      assert(dm != nullptr);

      // remove ownership edge to variable group (if any)
      auto dmm = dm->getModel();
      if(dmm.isValid()) {
        boost::remove_edge(_d->vertex, dmm._d->vertex, _d->impl->graph);
        boost::remove_edge(dmm._d->vertex, _d->vertex, _d->impl->graph);
      }
    }

    // if only one incoming edge exists any more, remove the entire variable. the one incoming edge is the parenthood
    // relation. ownership relations are also incoming, of which we must have zero.
    if(boost::in_degree(_d->vertex, _d->impl->graph) <= 1) {
      // Note: We cannot really remove the vertex for the variable, since boost::remove_vertex() invalidates all vertex
      // descriptors, which are the only link between the "real world" and the model.
      // Instead we completely disconnect it from the rest of the model (in particular the parent directory), so it
      // usually is no longer found.
      clear_vertex(_d->vertex, _d->impl->graph);
    }
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** DirectoryProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& DirectoryProxy::getName() const {
    return std::get<VertexProperties::DirectoryProperties>(_d->impl->graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  ProcessVariableProxy DirectoryProxy::addVariable(const std::string& name) {
    return _d->impl->addVariable(_d->vertex, name);
  }

  /********************************************************************************************************************/

  DirectoryProxy DirectoryProxy::addDirectory(const std::string& name) {
    return _d->impl->addDirectory(_d->vertex, name);
  }

  /********************************************************************************************************************/

  DirectoryProxy DirectoryProxy::addDirectoryRecursive(const std::string& name) {
    return _d->impl->addDirectoryRecursive(_d->vertex, name);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of Impl */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  ModuleGroupProxy Impl::add(Vertex owner, ModuleGroup& module) {
    return generic_add<ModuleGroupProxy, ModuleGroup, VertexProperties::ModuleGroupProperties,
        VertexProperties::Type::moduleGroup>(owner, module);
  }

  /********************************************************************************************************************/

  ApplicationModuleProxy Impl::add(Vertex owner, ApplicationModule& module) {
    return generic_add<ApplicationModuleProxy, ApplicationModule, VertexProperties::ApplicationModuleProperties,
        VertexProperties::Type::applicationModule>(owner, module);
  }

  /********************************************************************************************************************/

  VariableGroupProxy Impl::add(Vertex owner, VariableGroup& module) {
    return generic_add<VariableGroupProxy, VariableGroup, VertexProperties::VariableGroupProperties,
        VertexProperties::Type::variableGroup>(owner, module);
  }

  /********************************************************************************************************************/

  DeviceModuleProxy Impl::add(Vertex owner, DeviceModule& module) {
    return generic_add<DeviceModuleProxy, DeviceModule, VertexProperties::DeviceModuleProperties,
        VertexProperties::Type::deviceModule>(owner, module);
  }

  /********************************************************************************************************************/

  template<typename PROXY, typename MODULE, typename PROPS, VertexProperties::Type TYPE>
  PROXY Impl::generic_add(Vertex owner, MODULE& module) {
    auto parentDirectory = visit(owner, returnDirectory, getNeighbourDirectory, returnFirstHit(DirectoryProxy{}));

    // create plain vertex first
    auto newVertex = boost::add_vertex(graph);

    // set vertex type and type-dependent properties
    graph[newVertex].type = TYPE;
    if constexpr(!std::is_same<MODULE, DeviceModule>::value) {
      graph[newVertex].p.emplace<PROPS>(PROPS{module.getName(), module});
    }
    else {
      auto alias = module.getDeviceManager().getDeviceAliasOrURI();
      auto triggerPath = module.getTriggerPath();

      ProcessVariableProxy trigger; // initially invalid, stays like that if triggerPath.empty()

      if(!triggerPath.empty()) {
        auto dir = parentDirectory.addDirectoryRecursive(Utilities::getPathName(triggerPath));
        trigger = dir.addVariable(Utilities::getUnqualifiedName(triggerPath));
      }

      graph[newVertex].p.emplace<PROPS>(PROPS{alias, trigger, module});
    }

    // connect the vertex with an ownership edge
    auto ownershipEdge = boost::add_edge(owner, newVertex, graph).first; // .second is ignored as it cannot fail
    graph[ownershipEdge].type = EdgeProperties::Type::ownership;

    // obtain/create directory corresponding to the fully qualified path of the module
    DirectoryProxy directory;
    if constexpr(!std::is_same<MODULE, DeviceModule>::value) {
      directory = addDirectoryRecursive(parentDirectory._d->vertex, module.getName());
    }
    else {
      // the register content of DeviceModules is connected directly to the parent directory, since the alias/URI shall
      // not be visible as a sub-directory.
      directory = parentDirectory;
    }

    // connect the vertex with the directory with a neighbourhood edge
    auto neighbourhoodEdge = boost::add_edge(newVertex, directory._d->vertex, graph).first; // dito
    graph[neighbourhoodEdge].type = EdgeProperties::Type::neighbourhood;

    return {newVertex, shared_from_this()};
  }

  /********************************************************************************************************************/

  template<typename PROXY>
  void Impl::addVariableNode(PROXY module, const ProcessVariableProxy& variable, const VariableNetworkNode& node) {
    // get vertex of for variable
    auto vertex = variable._d->vertex;

    // get owning vertex (VariableGroup or ApplicationModule)
    auto owningVertex = module._d->vertex;

    // create ownership edge
    auto newOwnershipEdge = boost::add_edge(owningVertex, vertex, graph).first; // see above
    graph[newOwnershipEdge].type = EdgeProperties::Type::ownership;

    // get accessing ApplicationModule or DeviceModule vertex
    Vertex accessingVertex;
    if constexpr(std::is_same<PROXY, ApplicationModuleProxy>::value || std::is_same<PROXY, DeviceModuleProxy>::value) {
      accessingVertex = module._d->vertex;
    }
    else {
      static_assert(std::is_same<PROXY, VariableGroupProxy>::value, "Proxy type cannot have variable nodes.");
      accessingVertex = module.getOwningModule()._d->vertex;
    }

    // add node to variable
    auto& props = std::get<VertexProperties::ProcessVariableProperties>(graph[vertex].p);
    props.nodes.emplace_back(node);

    // add tags
    const auto& tags = node.getTags();
    props.tags.insert(tags.begin(), tags.end());

    // connect the variable vertex with the accessing module, direction depends on access (read/write)
    Edge newEdge;
    if(node.getDirection().dir == VariableDirection::feeding) {
      newEdge = boost::add_edge(accessingVertex, vertex, graph)
                    .first; // .second is ignored: silently reject duplicates CHECKME/FIXME!
    }
    else {
      newEdge = boost::add_edge(vertex, accessingVertex, graph).first; // see above
    }
    graph[newEdge].type = EdgeProperties::Type::pvAccess;
  }

  /********************************************************************************************************************/

  ProcessVariableProxy Impl::addVariable(Vertex parent, const std::string& name) {
    /// @todo FIXME add checks on the name: must not contain dots or slashes (must be unqualified name)!

    // first check if variable already exists
    auto existing = visit(parent, returnProcessVariable, adjacentOutSearch, keepParenthood,
        keepProcessVariables && keepName(name), returnFirstHit(ProcessVariableProxy{}));
    if(existing.isValid()) {
      return existing;
    }

    // create plain vertex first
    auto newVertex = boost::add_vertex(graph);

    // set vertex type and type-dependent properties
    graph[newVertex].type = VertexProperties::Type::processVariable;
    VertexProperties::ProcessVariableProperties props{name, {}, {}};
    graph[newVertex].p.emplace<VertexProperties::ProcessVariableProperties>(props);

    // connect the vertex with a parenthood edge
    auto newEdge = boost::add_edge(parent, newVertex, graph).first; // .second is ignored as it cannot fail
    graph[newEdge].type = EdgeProperties::Type::parenthood;

    return {newVertex, shared_from_this()};
  }

  /********************************************************************************************************************/

  DirectoryProxy Impl::addDirectory(Vertex parent, const std::string& name) { // create plain vertex first
    /// @todo FIXME add checks on the name: must not contain dots or slashes (must be unqualified name)!

    // first check if directory already exists
    auto existing = visit(parent, returnDirectory, adjacentOutSearch, keepParenthood, keepDirectories && keepName(name),
        returnFirstHit(DirectoryProxy{}));
    if(existing.isValid()) {
      return existing;
    }

    // create plain vertex first
    auto newVertex = boost::add_vertex(graph);

    // set vertex type and type-dependent properties
    graph[newVertex].type = VertexProperties::Type::directory;
    VertexProperties::DirectoryProperties props{name};
    graph[newVertex].p.emplace<VertexProperties::DirectoryProperties>(props);

    // connect the vertex with an hierarchy edge
    auto newEdge = boost::add_edge(parent, newVertex, graph).first; // .second is ignored as it cannot fail
    graph[newEdge].type = EdgeProperties::Type::parenthood;

    return {newVertex, shared_from_this()};
  }

  /********************************************************************************************************************/

  DirectoryProxy Impl::addDirectoryRecursive(Vertex parent, const std::string& qualifiedPath) {
    // separate the path into components
    std::vector<std::string> components;
    boost::split(components, qualifiedPath, boost::is_any_of("/"));

    // Start at the parent
    DirectoryProxy currentDirectory(parent, shared_from_this());

    for(const auto& component : components) {
      // special treatment for "."
      if(component == ".") {
        continue;
      }

      // special treatment for ".."
      if(component == "..") {
        if(currentDirectory._d->vertex == *boost::vertices(currentDirectory._d->impl->graph).first) {
          throw ChimeraTK::logic_error("Path component '..' at root directory level found.");
        }
        currentDirectory = currentDirectory.visit(returnDirectory, getParent, returnFirstHit(DirectoryProxy{}));
        assert(currentDirectory.isValid());
        continue;
      }

      // Special treatment for an extra slash e.g. at the beginning or two consequtive slashes.
      // Since the slash is the separator, the path component is just empty.
      if(component.empty()) {
        currentDirectory = {*(boost::vertices(graph).first), shared_from_this()}; // root directory
        continue;
      }

      // no special treatment necessary: just add the new level (if not yet existing)
      currentDirectory = currentDirectory.addDirectory(component);
    }

    // return the directory (either newly created or previously existing)
    return currentDirectory;
  }

  /********************************************************************************************************************/

  [[nodiscard]] std::string Impl::getFullyQualifiedPath(Vertex vertex) {
    return graph[vertex].visit([&](auto props) -> std::string {
      constexpr bool useDirectoryHierarchy = isDirectory(props) || isVariable(props);

      Vertex currentVertex = vertex;

      RegisterPath path;

      // Add name of current vertex (unless it's a DeviceModule or the Root)
      if constexpr(hasName(props)) {
        path = props.name;
      }

      // Loop until we have found the root vertex, add all names to the path
      while(currentVertex != *boost::vertices(graph).first) {
        // define vistor to be executed for the owner of currentVertex
        auto vis = [&](auto proxy) {
          if constexpr(hasName(proxy)) {
            // prefix name of current object to path, since we are iterating towards the root
            path = proxy.getName() / path;
          }
          currentVertex = proxy._d->vertex;
        };

        // just call the lambda for the owner/parent of currentVertex
        if constexpr(!useDirectoryHierarchy) {
          visit(currentVertex, vis, getOwner);
        }
        else {
          visit(currentVertex, vis, getParent);
        }
      }

      // resolve "/", ".." and "."
      auto components = path.getComponents();
      path = ""; // build path from components
      for(const auto& component : components) {
        if(component == ".") {
          continue;
        }
        if(component == "..") {
          path--; // remove last component
          continue;
        }
        if(component == "/") {
          path = ""; // clear entire path
          continue;
        }
        path /= component;
      }

      return path;
    });
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK::Model
