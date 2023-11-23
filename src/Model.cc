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

  Proxy::Proxy(std::shared_ptr<ProxyData> data) : _d(std::move(data)) {}

  /********************************************************************************************************************/

  std::string Proxy::getFullyQualifiedPath() const {
    return _d->impl->getFullyQualifiedPath(_d->vertex);
  }

  /********************************************************************************************************************/

  bool Proxy::isValid() const {
    return _d != nullptr && _d->impl != nullptr;
  }

  /********************************************************************************************************************/

  bool Proxy::operator==(const Proxy& other) const {
    if(_d == other._d) {
      return true;
    }
    if(_d == nullptr || other._d == nullptr) {
      return false;
    }
    return _d->impl == other._d->impl && (other._d->impl == nullptr || _d->vertex == other._d->vertex);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of RootProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  RootProxy::RootProxy(ModuleGroup& app) {
    // create ProxyData object
    _d = std::make_shared<ProxyData>();
    // create the graph
    _d->impl = std::make_shared<Impl>();
    // create root vertex
    _d->vertex = boost::add_vertex(_d->impl->_graph);
    VertexProperties::RootProperties props{app};
    _d->impl->_graph[_d->vertex].type = VertexProperties::Type::root;
    _d->impl->_graph[_d->vertex].p.emplace<VertexProperties::RootProperties>(props);

    // create neighbourhood edge to itself: the root represents both the Application ModuleGroup and the directory.
    auto [neighbourhoodEdge, success] = boost::add_edge(_d->vertex, _d->vertex, _d->impl->_graph);
    assert(success);
    _d->impl->_graph[neighbourhoodEdge].type = EdgeProperties::Type::neighbourhood;
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
    // The analyzer assumes that m_Size can be modified in boost::is_any_of so that there could be a mismatch between
    // creating a dynamic entry and not freeing it later if m_size suddenly is different from what it was
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    return _d->impl->addDirectoryRecursive(_d->vertex, name);
  }

  /********************************************************************************************************************/

  ProcessVariableProxy RootProxy::addVariable(const std::string& name) {
    return _d->impl->addVariable(_d->vertex, name);
  }

  /********************************************************************************************************************/

  void RootProxy::remove(ModuleGroup& module) {
    _d->impl->remove(module);
  }

  /********************************************************************************************************************/

  void RootProxy::remove(ApplicationModule& module) {
    _d->impl->remove(module);
  }

  /********************************************************************************************************************/

  RootProxy::operator Model::ModuleGroupProxy() {
    return _d->impl->_graph[_d->vertex].makeProxy<ModuleGroupProxy>(_d->vertex, _d->impl);
  }

  /********************************************************************************************************************/

  RootProxy::operator Model::DirectoryProxy() {
    return _d->impl->_graph[_d->vertex].makeProxy<DirectoryProxy>(_d->vertex, _d->impl);
  }

  /********************************************************************************************************************/

  RootProxy RootProxy::makeRootProxy(const std::shared_ptr<Impl>& impl) {
    auto graph = impl->_graph;
    auto* vertex = *(boost::vertices(graph).first);

    return graph[vertex].visitProxy(
        [](auto proxy) {
          if constexpr(isRoot(proxy)) {
            return proxy;
          }
          else {
            assert(false);
            return RootProxy();
          }
        },
        vertex, impl);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  ModuleGroupProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& ModuleGroupProxy::getName() const {
    return std::get<VertexProperties::ModuleGroupProperties>(_d->impl->_graph[_d->vertex].p).name;
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

  void ModuleGroupProxy::remove(ModuleGroup& module) {
    _d->impl->remove(module);
  }

  /********************************************************************************************************************/

  void ModuleGroupProxy::remove(ApplicationModule& module) {
    _d->impl->remove(module);
  }

  /********************************************************************************************************************/

  ModuleGroup& ModuleGroupProxy::getModuleGroup() const {
    return std::get<VertexProperties::ModuleGroupProperties>(_d->impl->_graph[_d->vertex].p).module;
  }

  /********************************************************************************************************************/

  void ModuleGroupProxy::informMove(ModuleGroup& module) {
    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::ModuleGroupProperties>(_d->impl->_graph[_d->vertex].p);

    _d->impl->_graph[_d->vertex].p.emplace<VertexProperties::ModuleGroupProperties>(
        VertexProperties::ModuleGroupProperties{std::move(currentProps.name), module});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  ApplicationModuleProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& ApplicationModuleProxy::getName() const {
    return std::get<VertexProperties::ApplicationModuleProperties>(_d->impl->_graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  VariableGroupProxy ApplicationModuleProxy::add(VariableGroup& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  void ApplicationModuleProxy::addVariable(ProcessVariableProxy& variable, VariableNetworkNode& node) {
    return _d->impl->addVariableNode(*this, variable, node);
  }

  /********************************************************************************************************************/

  void ApplicationModuleProxy::remove(VariableGroup& module) {
    _d->impl->remove(module);
  }

  /********************************************************************************************************************/

  ApplicationModuleProxy::operator Model::VariableGroupProxy() {
    return _d->impl->_graph[_d->vertex].makeProxy<VariableGroupProxy>(_d->vertex, _d->impl);
  }

  /********************************************************************************************************************/

  ApplicationModule& ApplicationModuleProxy::getApplicationModule() const {
    return std::get<VertexProperties::ApplicationModuleProperties>(_d->impl->_graph[_d->vertex].p).module;
  }

  /********************************************************************************************************************/

  void ApplicationModuleProxy::informMove(ApplicationModule& module) {
    assert(_d->impl->_graph[_d->vertex].type == VertexProperties::Type::applicationModule);

    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::ApplicationModuleProperties>(_d->impl->_graph[_d->vertex].p);

    _d->impl->_graph[_d->vertex].p.emplace<VertexProperties::ApplicationModuleProperties>(
        VertexProperties::ApplicationModuleProperties{std::move(currentProps.name), module});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  VariableGroupProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& VariableGroupProxy::getName() const {
    return std::get<VertexProperties::VariableGroupProperties>(_d->impl->_graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  VariableGroupProxy VariableGroupProxy::add(VariableGroup& module) {
    return _d->impl->add(_d->vertex, module);
  }

  /********************************************************************************************************************/

  void VariableGroupProxy::addVariable(ProcessVariableProxy& variable, VariableNetworkNode& node) {
    return _d->impl->addVariableNode(*this, variable, node);
  }

  /********************************************************************************************************************/

  void VariableGroupProxy::remove(VariableGroup& module) {
    _d->impl->remove(module);
  }

  /********************************************************************************************************************/

  ApplicationModuleProxy VariableGroupProxy::getOwningModule() const {
    Vertex currentVertex{_d->vertex};

    do {
      // there must be exactly one owner
      assert(boost::in_degree(currentVertex, _d->impl->_ownershipView) == 1);

      // Update currentVertex to the owner of the previous currentVertex.
      currentVertex =
          boost::source(*boost::in_edges(currentVertex, _d->impl->_ownershipView).first, _d->impl->_ownershipView);

      // repeat until the type of the currentVertex is no longer a VariablGroup
    } while(_d->impl->_ownershipView[currentVertex].type == VertexProperties::Type::variableGroup);

    // The type must now be a ApplicationModule
    assert(_d->impl->_ownershipView[currentVertex].type == VertexProperties::Type::applicationModule);

    return _d->impl->_graph[currentVertex].makeProxy<ApplicationModuleProxy>(currentVertex, _d->impl);
  }

  /********************************************************************************************************************/

  VariableGroup& VariableGroupProxy::getVariableGroup() const {
    return std::get<VertexProperties::VariableGroupProperties>(_d->impl->_graph[_d->vertex].p).module;
  }

  /********************************************************************************************************************/

  void VariableGroupProxy::informMove(VariableGroup& group) {
    assert(_d->impl->_graph[_d->vertex].type == VertexProperties::Type::variableGroup);

    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::VariableGroupProperties>(_d->impl->_graph[_d->vertex].p);

    _d->impl->_graph[_d->vertex].p.emplace<VertexProperties::VariableGroupProperties>(
        VertexProperties::VariableGroupProperties{std::move(currentProps.name), group});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /**  DeviceModuleProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& DeviceModuleProxy::getAliasOrCdd() const {
    return std::get<VertexProperties::DeviceModuleProperties>(_d->impl->_graph[_d->vertex].p).aliasOrCdd;
  }

  /********************************************************************************************************************/

  ProcessVariableProxy DeviceModuleProxy::getTrigger() const {
    return std::get<VertexProperties::DeviceModuleProperties>(_d->impl->_graph[_d->vertex].p).trigger;
  }

  /********************************************************************************************************************/

  void DeviceModuleProxy::addVariable(ProcessVariableProxy& variable, VariableNetworkNode& node) {
    return _d->impl->addVariableNode(*this, variable, node);
  }

  /********************************************************************************************************************/

  void DeviceModuleProxy::informMove(DeviceModule& module) {
    // Updating the reference works only through the constructor, hence we have to get all other data members and
    // construct a new one in the std::variant.

    auto currentProps = std::get<VertexProperties::DeviceModuleProperties>(_d->impl->_graph[_d->vertex].p);

    _d->impl->_graph[_d->vertex].p.emplace<VertexProperties::DeviceModuleProperties>(
        VertexProperties::DeviceModuleProperties{
            std::move(currentProps.aliasOrCdd), std::move(currentProps.trigger), module});
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** ProcessVariableProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& ProcessVariableProxy::getName() const {
    return std::get<VertexProperties::ProcessVariableProperties>(_d->impl->_graph[_d->vertex].p).name;
  }

  /********************************************************************************************************************/

  const std::vector<VariableNetworkNode>& ProcessVariableProxy::getNodes() const {
    return std::get<VertexProperties::ProcessVariableProperties>(_d->impl->_graph[_d->vertex].p).nodes;
  }

  /********************************************************************************************************************/

  const std::unordered_set<std::string>& ProcessVariableProxy::getTags() const {
    return std::get<VertexProperties::ProcessVariableProperties>(_d->impl->_graph[_d->vertex].p).tags;
  }

  /********************************************************************************************************************/

  void ProcessVariableProxy::addTag(const std::string& tag) {
    std::get<VertexProperties::ProcessVariableProperties>(_d->impl->_graph[_d->vertex].p).tags.insert(tag);
  }

  /********************************************************************************************************************/

  void ProcessVariableProxy::removeNode(const VariableNetworkNode& node) {
    assert(node.getType() == NodeType::Application || node.getType() == NodeType::Device);
    assert(node.getModel().isValid());
    assert(isValid());
    assert(node.getModel()._d->vertex == _d->vertex);
    assert(node.getModel()._d->impl == _d->impl);

    // remove node from the list of nodes in the PV's vertex properties
    try {
      auto& nodes = std::get<VertexProperties::ProcessVariableProperties>(_d->impl->_graph[_d->vertex].p).nodes;
      auto it = std::find(nodes.begin(), nodes.end(), node);
      if(it == nodes.end()) {
        return;
      }
      nodes.erase(it);

      // remove model relationships between PV and module
      bool ownershipDeleted{false};
      if(node.getType() == NodeType::Application) {
        auto* vg = dynamic_cast<VariableGroup*>(node.getOwningModule());
        assert(vg != nullptr);

        // remove ownership edge to variable group (if any)
        auto vgm = vg->getModel();
        if(vgm.isValid()) {
          for(const auto& edge :
              boost::make_iterator_range(boost::edge_range(vgm._d->vertex, _d->vertex, _d->impl->_graph))) {
            if(_d->impl->_graph[edge].type == EdgeProperties::Type::ownership) {
              boost::remove_edge(edge, _d->impl->_graph);
              ownershipDeleted = true;
              break;
            }
          }
        }

        // obtain the accessing module
        auto* am = dynamic_cast<ApplicationModule*>(vg->findApplicationModule());
        assert(am != nullptr);
        auto amm = am->getModel();
        // The owning ApplicationModule might be no longer in the model. This happens when the owning ApplicationModule
        // has been removed from the model already. In this case, the direct ownership relation as well as the PV access
        // relation have been removed already when removing the ApplicationModule.
        if(amm.isValid()) {
          // remove ownership edge to application module group (if directly owned)
          if(!ownershipDeleted) {
            for(const auto& edge :
                boost::make_iterator_range(boost::edge_range(amm._d->vertex, _d->vertex, _d->impl->_graph))) {
              if(_d->impl->_graph[edge].type == EdgeProperties::Type::ownership) {
                boost::remove_edge(edge, _d->impl->_graph);
                ownershipDeleted = true;
                break;
              }
            }
          }

          // the ownership must be removed now
          assert(ownershipDeleted);

          // remove pv access edge
#ifndef NDEBUG
          bool pvAccessDeleted{false};
#endif
          if(node.getDirection().dir == VariableDirection::consuming) {
            for(const auto& edge :
                boost::make_iterator_range(boost::edge_range(_d->vertex, amm._d->vertex, _d->impl->_graph))) {
              if(_d->impl->_graph[edge].type == EdgeProperties::Type::pvAccess) {
                boost::remove_edge(edge, _d->impl->_graph);
#ifndef NDEBUG
                pvAccessDeleted = true;
#endif
                break;
              }
            }
          }
          else {
            assert(node.getDirection().dir == VariableDirection::feeding);
            for(const auto& edge :
                boost::make_iterator_range(boost::edge_range(amm._d->vertex, _d->vertex, _d->impl->_graph))) {
              if(_d->impl->_graph[edge].type == EdgeProperties::Type::pvAccess) {
                boost::remove_edge(edge, _d->impl->_graph);
#ifndef NDEBUG
                pvAccessDeleted = true;
#endif
                break;
              }
            }
          }
          assert(pvAccessDeleted);
        }
      }
      else if(node.getType() == NodeType::Device) {
        auto* dm = dynamic_cast<DeviceModule*>(node.getOwningModule());
        assert(dm != nullptr);

        // remove ownership and pv access edges to device module
        auto dmm = dm->getModel();
        if(dmm.isValid()) {
          boost::remove_edge(_d->vertex, dmm._d->vertex, _d->impl->_graph);
          boost::remove_edge(dmm._d->vertex, _d->vertex, _d->impl->_graph);
        }
      }

      // if only one incoming edge exists any more, remove the entire variable. the one incoming edge is the parenthood
      // relation. ownership relations are also incoming, of which we must have zero.
      if(boost::in_degree(_d->vertex, _d->impl->_graph) <= 1 && boost::out_degree(_d->vertex, _d->impl->_graph) == 0) {
        assert(boost::in_degree(_d->vertex, _d->impl->_graph) == 0 ||
            _d->impl->_graph[*boost::in_edges(_d->vertex, _d->impl->_graph).first].type ==
                EdgeProperties::Type::parenthood);

        assert(nodes.empty());

        boost::clear_vertex(_d->vertex, _d->impl->_graph);
        boost::remove_vertex(_d->vertex, _d->impl->_graph);

        _d->vertex = Model::Vertex();
        _d->impl.reset();
      }
    }
    catch(std::bad_variant_access&) {
      assert(false);
    }
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** DirectoryProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  const std::string& DirectoryProxy::getName() const {
    return std::get<VertexProperties::DirectoryProperties>(_d->impl->_graph[_d->vertex].p).name;
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
    return genericAdd<ModuleGroupProxy, ModuleGroup, VertexProperties::ModuleGroupProperties,
        VertexProperties::Type::moduleGroup>(owner, module);
  }

  /********************************************************************************************************************/

  ApplicationModuleProxy Impl::add(Vertex owner, ApplicationModule& module) {
    return genericAdd<ApplicationModuleProxy, ApplicationModule, VertexProperties::ApplicationModuleProperties,
        VertexProperties::Type::applicationModule>(owner, module);
  }

  /********************************************************************************************************************/

  VariableGroupProxy Impl::add(Vertex owner, VariableGroup& module) {
    return genericAdd<VariableGroupProxy, VariableGroup, VertexProperties::VariableGroupProperties,
        VertexProperties::Type::variableGroup>(owner, module);
  }

  /********************************************************************************************************************/

  DeviceModuleProxy Impl::add(Vertex owner, DeviceModule& module) {
    return genericAdd<DeviceModuleProxy, DeviceModule, VertexProperties::DeviceModuleProperties,
        VertexProperties::Type::deviceModule>(owner, module);
  }

  /********************************************************************************************************************/

  template<typename PROXY, typename MODULE, typename PROPS, VertexProperties::Type TYPE>
  PROXY Impl::genericAdd(Vertex owner, MODULE& module) {
    auto parentDirectory = visit(owner, returnDirectory, getNeighbourDirectory, returnFirstHit(DirectoryProxy{}));

    // create plain vertex first
    auto* newVertex = boost::add_vertex(_graph);

    // set vertex type and type-dependent properties
    _graph[newVertex].type = TYPE;
    if constexpr(!std::is_same<MODULE, DeviceModule>::value) {
      _graph[newVertex].p.emplace<PROPS>(PROPS{module.getName(), module});
    }
    else {
      auto alias = module.getDeviceManager().getDeviceAliasOrURI();
      auto triggerPath = module.getTriggerPath();

      ProcessVariableProxy trigger; // initially invalid, stays like that if triggerPath.empty()

      if(!triggerPath.empty()) {
        auto dir = parentDirectory.addDirectoryRecursive(Utilities::getPathName(triggerPath));
        trigger = dir.addVariable(Utilities::getUnqualifiedName(triggerPath));

        // connect trigger vertex with trigger edge
        auto [triggerEdge, triggerSuccess] = boost::add_edge(trigger._d->vertex, newVertex, _graph);
        assert(triggerSuccess);
        _graph[triggerEdge].type = EdgeProperties::Type::trigger;
      }

      _graph[newVertex].p.emplace<PROPS>(PROPS{alias, trigger, module});
    }

    // connect the vertex with an ownership edge
    auto [ownershipEdge, ownershipSuccess] = boost::add_edge(owner, newVertex, _graph);
    assert(ownershipSuccess);
    _graph[ownershipEdge].type = EdgeProperties::Type::ownership;

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
    auto [neighbourhoodEdge, neighbourhoodSuccess] = boost::add_edge(newVertex, directory._d->vertex, _graph);
    assert(neighbourhoodSuccess);
    _graph[neighbourhoodEdge].type = EdgeProperties::Type::neighbourhood;

    return _graph[newVertex].makeProxy<PROXY>(newVertex, shared_from_this());
  }

  /********************************************************************************************************************/

  void Impl::remove(ModuleGroup& module) {
    genericRemove(module);
  }

  /********************************************************************************************************************/

  void Impl::remove(ApplicationModule& module) {
    genericRemove(module);
  }

  /********************************************************************************************************************/

  void Impl::remove(VariableGroup& module) {
    genericRemove(module);
  }

  /********************************************************************************************************************/

  void Impl::remove(DeviceModule& module) {
    genericRemove(module);
  }

  /********************************************************************************************************************/

  template<typename MODULE>
  void Impl::genericRemove(MODULE& module) {
    auto modelToRemove = module.getModel();

    // The model may be invalid e.g. in case of calls to unregisterModule() in move assignment operations. Nothing to be
    // removed from the model in that case.
    if(!modelToRemove.isValid()) {
      return;
    }

    // Remove the vertex representing this module with all edges (ownership, PV access etc.)
    auto vertexToRemove = modelToRemove._d->vertex;
    boost::clear_vertex(vertexToRemove, _graph);
    boost::remove_vertex(vertexToRemove, _graph);

    module._model = {};
  }

  /********************************************************************************************************************/

  template<typename PROXY>
  void Impl::addVariableNode(PROXY module, ProcessVariableProxy& variable, VariableNetworkNode& node) {
    node.setModel(variable);

    // get vertex of for variable
    auto* vertex = variable._d->vertex;

    // get owning vertex (VariableGroup or ApplicationModule)
    auto owningVertex = module._d->vertex;

    // create ownership edge
    auto [newOwnershipEdge, success] = boost::add_edge(owningVertex, vertex, _graph);
    assert(success);
    _graph[newOwnershipEdge].type = EdgeProperties::Type::ownership;

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
    auto& props = std::get<VertexProperties::ProcessVariableProperties>(_graph[vertex].p);
    props.nodes.emplace_back(node);

    // add tags
    const auto& tags = node.getTags();
    props.tags.insert(tags.begin(), tags.end());

    // connect the variable vertex with the accessing module, direction depends on access (read/write)
    Edge newEdge;
    if(node.getDirection().dir == VariableDirection::feeding) {
      std::tie(newEdge, success) = boost::add_edge(accessingVertex, vertex, _graph);
    }
    else {
      std::tie(newEdge, success) = boost::add_edge(vertex, accessingVertex, _graph);
    }
    assert(success);
    _graph[newEdge].type = EdgeProperties::Type::pvAccess;
    _graph[newEdge].pvAccessWithReturnChannel = node.getDirection().withReturn;
  }

  /********************************************************************************************************************/

  ProcessVariableProxy Impl::addVariable(Vertex parent, const std::string& name) {
    if(!Utilities::checkName(name, false)) {
      throw ChimeraTK::logic_error("Variable name '" + name + "' contains illegal characters.");
    }

    // first check if variable already exists
    auto existing = visit(parent, returnProcessVariable, adjacentOutSearch, keepParenthood,
        keepProcessVariables && keepName(name), returnFirstHit(ProcessVariableProxy{}));
    if(existing.isValid()) {
      return existing;
    }

    // create plain vertex first
    auto* newVertex = boost::add_vertex(_graph);

    // set vertex type and type-dependent properties
    _graph[newVertex].type = VertexProperties::Type::processVariable;
    VertexProperties::ProcessVariableProperties props{name, {}, {}};
    _graph[newVertex].p.emplace<VertexProperties::ProcessVariableProperties>(props);

    // connect the vertex with a parenthood edge
    auto [newEdge, success] = boost::add_edge(parent, newVertex, _graph);
    assert(success);
    _graph[newEdge].type = EdgeProperties::Type::parenthood;

    return _graph[newVertex].makeProxy<ProcessVariableProxy>(newVertex, shared_from_this());
  }

  /********************************************************************************************************************/

  DirectoryProxy Impl::addDirectory(Vertex parent, const std::string& name) {
    if(!Utilities::checkName(name, false)) {
      throw ChimeraTK::logic_error("Variable name '" + name + "' contains illegal characters.");
    }

    // first check if directory already exists
    auto existing = visit(parent, returnDirectory, adjacentOutSearch, keepParenthood, keepDirectories && keepName(name),
        returnFirstHit(DirectoryProxy{}));
    if(existing.isValid()) {
      return existing;
    }

    // create plain vertex first
    auto* newVertex = boost::add_vertex(_graph);

    // set vertex type and type-dependent properties
    _graph[newVertex].type = VertexProperties::Type::directory;
    VertexProperties::DirectoryProperties props{name};
    _graph[newVertex].p.emplace<VertexProperties::DirectoryProperties>(props);

    // connect the vertex with an hierarchy edge
    auto [newEdge, success] = boost::add_edge(parent, newVertex, _graph);
    assert(success);
    _graph[newEdge].type = EdgeProperties::Type::parenthood;

    return _graph[newVertex].makeProxy<DirectoryProxy>(newVertex, shared_from_this());
  }

  /********************************************************************************************************************/

  DirectoryProxy Impl::addDirectoryRecursive(Vertex parent, const std::string& qualifiedPath) {
    // separate the path into components
    std::vector<std::string> components;
    // The analyzer assumes that m_Size can be modified in boost::is_any_of so that there could be a mismatch between
    // creating a dynamic entry and not freeing it later if m_size suddenly is different from what it was
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    boost::split(components, qualifiedPath, boost::is_any_of("/"));

    // Start at the parent
    auto currentDirectory = _graph[parent].makeProxy<DirectoryProxy>(parent, shared_from_this());

    for(const auto& component : components) {
      // special treatment for "."
      if(component == ".") {
        continue;
      }

      // special treatment for ".."
      if(component == "..") {
        if(currentDirectory._d->vertex == *boost::vertices(currentDirectory._d->impl->_graph).first) {
          throw ChimeraTK::logic_error("Path component '..' at root directory level found.");
        }
        currentDirectory = currentDirectory.visit(returnDirectory, getParent, returnFirstHit(DirectoryProxy{}));
        assert(currentDirectory.isValid());
        continue;
      }

      // Special treatment for an extra slash e.g. at the beginning or two consequtive slashes.
      // Since the slash is the separator, the path component is just empty.
      if(component.empty()) {
        currentDirectory = Model::DirectoryProxy(RootProxy::makeRootProxy(shared_from_this())); // root directory
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
    return _graph[vertex].visit([&](auto props) -> std::string {
      constexpr bool useDirectoryHierarchy = isDirectory(props) || isVariable(props);

      Vertex currentVertex = vertex;

      RegisterPath path;

      // Add name of current vertex (unless it's a DeviceModule or the Root)
      if constexpr(hasName(props)) {
        path = props.name;
      }

      // Loop until we have found the root vertex, add all names to the path
      while(currentVertex != *boost::vertices(_graph).first) {
        // define vistor to be executed for the owner of currentVertex
        bool found = false;
        auto vis = [&](auto proxy) {
          if constexpr(hasName(proxy)) {
            // prefix name of current object to path, since we are iterating towards the root
            path = proxy.getName() / path;
          }
          currentVertex = proxy._d->vertex;
          found = true;
        };

        // just call the lambda for the owner/parent of currentVertex
        if constexpr(!useDirectoryHierarchy) {
          visit(currentVertex, vis, getOwner);
        }
        else {
          visit(currentVertex, vis, getParent);
        }

        // abort condition in case we try to get the qualified path for something not connected with the root any more
        if(!found) {
          path = "<disjunct>" / path;
          break;
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
