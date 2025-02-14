// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/cppext/finally.hpp>
#include <ChimeraTK/RegisterPath.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/transpose_graph.hpp>

#include <atomic>
#include <map>
#include <memory>
#include <utility>

namespace ChimeraTK {
  // Forward declarations
  class ModuleGroup;
  class ApplicationModule;
  class VariableGroup;
  class VariableNetworkNode;
  class DeviceModule;
  class Module;

  template<typename T>
  class InversionOfControlAccessor;

} // namespace ChimeraTK

namespace ChimeraTK::Model {

  /********************************************************************************************************************/

  // Forward declarations

  class RootProxy;
  class ModuleGroupProxy;
  class ApplicationModuleProxy;
  class VariableGroupProxy;
  class DeviceModuleProxy;
  class ProcessVariableProxy;
  class DirectoryProxy;
  class Impl;

  template<typename ProxyType>
  class NonOwningProxy;

  namespace detail {
    // Define how the boost::adjacency_list is supposed to store edges and vertices internally
    using OutEdgeListType = boost::multisetS;
    using VertexListType = boost::listS;
  } // namespace detail

  // Note: This is effectively a forward-declaration. The type must match the type Graph::vertex_descriptor for the
  // used Graph type which is only known below. It will be re-aliased below so we get an error if the type mismatches.
  // This trick is necessary to break the otherwise circular dependency of the VertexProperties class using the Vertex
  // definition, while the VertexProperties class is necessary to define the Graph bringing the vertex_descriptor.
  // Since the Vertex type will not depend on the VertexProperties, this is not expected to go wrong.
  using Vertex =
      boost::adjacency_list<detail::OutEdgeListType, detail::VertexListType, boost::bidirectionalS>::vertex_descriptor;

  /********************************************************************************************************************/

  /**
   * Base class for the proxies representing objects in the model.
   */
  class Proxy {
   public:
    /**
     * The default constructor creates a dysfunctional, empty proxy. Calling any functions on the resulting object
     * will cause undefined behaviour (null pointer dereferencing or shared_ptr assertion failure).
     */
    Proxy() = default;

    /**
     * Traverse the model using the specified filter and call the visitor functor for each ModuleGroup,
     * ApplicationModule, VariableGroup, DeviceModule, ProcessVariable and Directory found during traversal. The
     * (templated) functor must accept a single argument of any of the proxy types.
     *
     * The optional additional arguments can be used to control how the search is performed. One can specify filters
     * for filtering objects (i.e. vertices) and relations (i.e. edges), and one can specify a search type and some
     * search options.
     *
     * Filters can either be defined freely (see the EdgeFilter and VertexFilter classes), or the following
     * pre-defined filters can be used:
     * - Relationship filters (filter edges by type, cf. EdgeProperties::Type):
     *   - keepPvAccess
     *   - keepPvAccessWithReturnChannel
     *   - keepOwnership
     *   - keepParenthood
     *   - keepNeighbourhood
     * - Object type filters:
     *   - keepModuleGroups
     *   - keepApplicationModules
     *   - keepVariableGroups
     *   - keepDeviceModules
     *   - keepProcessVariables
     *   - keepDirectories
     * - Other filters on objects:
     *   - keepName("objectName") - filter by given object name
     *   - keepTag("tagName") - filter by given tag name (currently applies to ProcessVariables only; recommended to
     *                          combine with keepProcessVariables)
     *
     * Note: The relationship filters are applied before the search is executed, while the object type filters only
     *       filter for which objects the visitor function is called.
     *
     * The following search types can be specified:
     * - adjacentSearch: Find everything directly connected/related to the current object. Note that objects will be
     *                   found multiple times if there are multiple relations between the same objects (e.g. ownership
     *                   and pvAccess).
     * - adjacentInSearch: Like AdjacentSearch, but limit to inbound relationships (e.g. inbound ownership means the
     *                     owner of the current object is found)
     * - adjacentOutSearch: Like AdjacentSearch, but limit to outgoing relationships (e.g. outgoing ownership means
     *                      the owned submodules/variables of the current object are found)
     * - depthFirstSearch: Traverse the entire model from the current object downwards with the depth-first algorithm
     * - breadthFirstSearch: Traverse the entire model from the current object downwards with the breadth-first
     *                       algorithm
     *
     * The following search options can be specified:
     * - returnFirstHit: Terminate the search after the first hit. If specified, the visitor may return a non-void value
     *                   which will be returned by the visit() function.
     *
     * In case of depthFirstSearch, the following search option can be specified:
     * - continueSearchDisjunctTrees: Continue search even when starting object/vertex is finished. This allows to
     *                                traverse also disjunct parts of the model.
     *
     * There are also combined search configurations which can be used to specify filters and search types at the same
     * time:
     * - ownedModuleGroups: equivalent of adjacentOutSearch, keepOwnership, keepModuleGroups
     * - ownedApplicationModules: equivalent of adjacentOutSearch, keepOwnership, keepApplicationModules
     * - ownedVariableGroups: equivalent of adjacentOutSearch, keepOwnership, keepVariableGroups
     * - ownedVariables: equivalent of adjacentOutSearch, keepParenthood, keepProcessVariables
     * - childDirectories: equivalent of adjacentOutSearch, parenthoodFilter, keepDirectories
     * - childVariables: equivalent of adjacentOutSearch, keepParenthood, keepProcessVariables
     * - children: equivalent of adjacentOutSearch, keepParenthood
     * - getOwner: equivalent of adjacentInSearch, keepOwnership
     * - getParent: equivalent of adjacentInSearch, keepParenthood
     * - getNeighbourDirectory: equivalent of adjacentOutSearch, keepNeighbourhood
     * - neighbourModules: equivalent of adjacentInSearch, keepNeighbourhood
     *
     * The order of these additional arguments does not matter. If conflicting arguments are specified (e.g. two
     * different search types), the first one is chosen.
     *
     * To simplify getting a single search result, the following pre-defined visitor functors can be used, typically in
     * combination with returnFirstHit and appropriate filters:
     * - returnModuleGroup
     * - returnApplicationModule
     * - returnVariableGroup
     * - returnProcessVariable
     * - returnDirectory
     */
    template<typename VISITOR, typename... Args>
    auto visit(VISITOR visitor, Args... args) const;

    /**
     * Return the fully qualified path.
     *
     * For modules, this is the name of all owners and the module itself, concatenated with slashes as separators.
     * Path components which modify the hierarchy (e.g. starting with ".." or "/", or the ".") will be resolved before
     * returning the result.
     *
     * For process variables and directories, this is the name of all parents and the object itself, concatenated with
     * slashes as separators. (Path components modifying the hierarchy cannot occur here.)
     */
    [[nodiscard]] std::string getFullyQualifiedPath() const;

    /**
     * Check if the model is valid. Default-constructed modules and their sub-modules will not have a valid model. If
     * the model is not valid, no functions other than isValid() may be called.
     */
    [[nodiscard]] bool isValid() const;

    [[nodiscard]] bool operator==(const Proxy& other) const;

   protected:
    friend class Impl;
    friend class RootProxy;
    friend class ModuleGroupProxy;
    friend class ApplicationModuleProxy;
    friend class VariableGroupProxy;
    friend class DeviceModuleProxy;
    friend class ProcessVariableProxy;
    friend class DirectoryProxy;
    friend struct VertexProperties;

    template<typename ProxyType>
    friend class NonOwningProxy;

    /// Struct holding the data for the proxy classes
    struct ProxyData;

    // Proxy(Model::Vertex vertex, const std::shared_ptr<Impl>& impl);
    explicit Proxy(std::shared_ptr<ProxyData> data);

    std::shared_ptr<ProxyData> _d;
  };

  /********************************************************************************************************************/

  /**
   * Proxy representing the root of the application model.
   *
   * This acts as the main entry point for the model.
   */
  class RootProxy : public Proxy {
   public:
    /// This constructor creates a new, empty model with the given ModuleGroup as application root.
    explicit RootProxy(ModuleGroup& app);

    ModuleGroupProxy add(ModuleGroup& module);
    ApplicationModuleProxy add(ApplicationModule& module);
    DeviceModuleProxy add(DeviceModule& module);

    DirectoryProxy addDirectory(const std::string& name);
    ProcessVariableProxy addVariable(const std::string& name);
    DirectoryProxy addDirectoryRecursive(const std::string& name);

    void remove(ApplicationModule& module);
    void remove(ModuleGroup& module);

    /**
     * Resolve the given path and call the visitor for the found object. If the path does not exist, the visitor is
     * not called and false is returned.
     *
     * \param path can be the name of a direct subdirectory or process variable, it can point to a deeper subdirectory
     * hierarchy, or it can also refer to higher directory levels by starting with "../" or "/". The name always
     * refers to the process variable directory structure and never to the object ownership hierarchy.
     *
     * \param visitor must accept ProcessVariableProxy and DirectoryProxy objects as its sole argument.
     *
     * \return indicates whether the path has been found.
     */
    template<typename VISITOR>
    bool visitByPath(std::string_view path, VISITOR visitor) const;

    template<typename... Args>
    void writeGraphViz(const std::string& filename, Args... args) const;

    /**
     * Convert into a proxy for the ModuleGroup part/aspect of the Application
     */
    explicit operator Model::ModuleGroupProxy();

    /**
     * Convert into a proxy for the Directory part/aspect of the Application
     */
    explicit operator Model::DirectoryProxy();

    /**
     * Create RootProxy assuming the application has been created already (i.e. the root vertex already exists and the
     * RootProxy has been created before).
     */
    static RootProxy makeRootProxy(const std::shared_ptr<Impl>& impl);

   private:
    using Proxy::Proxy;
    friend class Proxy;
    friend class ModuleGroupProxy;
    friend class ApplicationModuleProxy;
    friend class VariableGroupProxy;
    friend class DeviceModuleProxy;
    friend class ProcessVariableProxy;
    friend class DirectoryProxy;
    friend struct VertexProperties;
  };

  /********************************************************************************************************************/

  class ModuleGroupProxy : public Proxy {
   public:
    /// Get the name of the ModuleGroup
    [[nodiscard]] const std::string& getName() const;

    /// Return the actual ModuleGroup
    [[nodiscard]] ModuleGroup& getModuleGroup() const;

    ModuleGroupProxy add(ModuleGroup& module);
    ApplicationModuleProxy add(ApplicationModule& module);
    DeviceModuleProxy add(DeviceModule& module);

    void remove(ApplicationModule& module);
    void remove(ModuleGroup& module);

   private:
    using Proxy::Proxy;
    friend struct VertexProperties;

    /// Update ModuleGroup reference after move operation
    void informMove(ModuleGroup& module);
    friend class ChimeraTK::ModuleGroup;
  };

  /********************************************************************************************************************/

  class ApplicationModuleProxy : public Proxy {
   public:
    /// Get the name of the ApplicationModule
    [[nodiscard]] const std::string& getName() const;

    /// Return the actual ApplicationModule
    [[nodiscard]] ApplicationModule& getApplicationModule() const;

    VariableGroupProxy add(VariableGroup& module);
    void addVariable(ProcessVariableProxy& variable, VariableNetworkNode& node);
    void remove(VariableGroup& module);

    /**
     * Convert into a proxy for the VariableGroup part/aspect of the ApplicationModule
     */
    explicit operator Model::VariableGroupProxy();

   private:
    using Proxy::Proxy;
    friend struct VertexProperties;

    /// Update ApplicationModule reference after move operation
    void informMove(ApplicationModule& module);
    friend class ChimeraTK::ApplicationModule;
  };

  /********************************************************************************************************************/

  class VariableGroupProxy : public Proxy {
   public:
    /// Get the name of the VariableGroup
    [[nodiscard]] const std::string& getName() const;

    /// Return the actual VariableGroup
    [[nodiscard]] VariableGroup& getVariableGroup() const;

    /// Return the owning ApplicationModule (may be indirectly owned in case of nested VariableGroups).
    [[nodiscard]] ApplicationModuleProxy getOwningModule() const;

    VariableGroupProxy add(VariableGroup& module);
    void addVariable(ProcessVariableProxy& variable, VariableNetworkNode& node);
    void remove(VariableGroup& module);

   private:
    using Proxy::Proxy;
    friend struct VertexProperties;

    /// Update VariableGroup reference after move operation
    void informMove(VariableGroup& group);
    friend class ChimeraTK::VariableGroup;
  };

  /********************************************************************************************************************/

  class DeviceModuleProxy : public Proxy {
   public:
    /// Get the alias or CDD of the device
    [[nodiscard]] const std::string& getAliasOrCdd() const;

    /// Get the ProcessVariableProxy for the trigger. If no trigger was specified, the returned proxy will be invalid,
    /// i.e. Proxy::isValid() will return false.
    [[nodiscard]] ProcessVariableProxy getTrigger() const;

    void addVariable(ProcessVariableProxy& variable, VariableNetworkNode& node);

    [[nodiscard]] DeviceModule& getDeviceModule() const;

   private:
    using Proxy::Proxy;
    friend struct VertexProperties;

    /// Update DeviceModule reference after move operation
    void informMove(DeviceModule& module);
    friend class ChimeraTK::DeviceModule;
  };

  /********************************************************************************************************************/

  class ProcessVariableProxy : public Proxy {
   public:
    /// Get the name of the ProcessVariable
    [[nodiscard]] const std::string& getName() const;

    /// Return all VariableNetworkNodes for this variable
    [[nodiscard]] const std::vector<std::shared_ptr<VariableNetworkNode>>& getNodes() const;

    /// Return all tags attached to this variable
    [[nodiscard]] const std::unordered_set<std::string>& getTags() const;

    /**
     * Resolve the given path and call the visitor for the found object. If the path does not exist, the visitor is
     * not called and false is returned.
     *
     * \param path can be the name of a direct subdirectory or process variable, it can point to a deeper subdirectory
     * hierarchy, or it can also refer to higher directory levels by starting with "../" or "/".
     *
     * \param visitor must accept ProcessVariableProxy and DirectoryProxy objects as its sole argument.
     *
     * \return indicates whether the path has been found.
     *
     * Hint: For process variables, the path must start with ".." or "/" to make any sense.
     */
    template<typename VISITOR>
    bool visitByPath(std::string_view path, VISITOR visitor) const;

   protected:
    using Proxy::Proxy;
    friend struct VertexProperties;

    /// Add tag to this PV. Used by VariableNetworkNode to update the model when tags are added to PVs.
    void addTag(const std::string& tag);

    /// Remove VariableNetworkNode from the list of nodes. Note: Will invalidate return value of getNodes()!
    void removeNode(const VariableNetworkNode& node);

    friend class ChimeraTK::VariableNetworkNode;
    friend class ChimeraTK::DeviceModule;
    friend class ChimeraTK::Model::Impl;
    friend class ChimeraTK::Module;

    template<typename T>
    friend class ChimeraTK::InversionOfControlAccessor;
  };

  /********************************************************************************************************************/

  class DirectoryProxy : public Proxy {
   public:
    /// Get the name of the Directory
    [[nodiscard]] const std::string& getName() const;

    /**
     * Resolve the given path and call the visitor for the found object. If the path does not exist, the visitor is
     * not called and false is returned.
     *
     * \param path can be the name of a direct subdirectory or process variable, it can point to a deeper subdirectory
     * hierarchy, or it can also refer to higher directory levels by starting with "../" or "/".
     *
     * \param visitor must accept ProcessVariableProxy and DirectoryProxy objects as its sole argument.
     *
     * \return indicates whether the path has been found.
     */
    template<typename VISITOR>
    bool visitByPath(std::string_view path, VISITOR visitor) const;

    ProcessVariableProxy addVariable(const std::string& name);
    DirectoryProxy addDirectory(const std::string& name);
    DirectoryProxy addDirectoryRecursive(const std::string& name);

   private:
    using Proxy::Proxy;
    friend struct VertexProperties;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  /**
   * Proxy class which does not keep the ownership of the model.
   *
   * The normal Proxy-based types like ProcessVariableProxy keep internally a shared pointer to the model. In place
   * where the model is supposed to keep a Proxy (e.g. in the VertexProperties), this would create a shared-pointer loop
   * resulting in a memory leak. To break this circle, a NonOwningProxy templated to the respective proxy-type
   * (e.g. NonOwningProxy<ProcessVariableProxy>) can be used instead.
   */
  template<typename ProxyType>
  class NonOwningProxy {
   public:
    /// Default constructor creates "empty" non-owning proxy which does not contain a valid proxy
    NonOwningProxy() = default;

    /// Construct non-owning proxy from (owning) proxy
    // NOLINTNEXTLINE(google-explicit-constructor)
    NonOwningProxy(const ProxyType& owningProxy)
    : _vertex(owningProxy.isValid() ? owningProxy._d->vertex : nullptr),
      _impl(owningProxy.isValid() ? owningProxy._d->impl : nullptr) {}

    /// Assign non-owning proxy from (owning) proxy
    NonOwningProxy& operator=(const ProxyType& owningProxy) {
      if(owningProxy.isValid()) {
        _vertex = owningProxy._d->vertex;
        _impl = owningProxy._d->impl;
      }
      else {
        _vertex = nullptr;
        _impl.reset();
      }
      return *this;
    }

    /// Return owning proxy. If the model targeted by this non-owning proxy has gone away, the returned proxy will
    /// not be valid.
    ProxyType lock() {
      ProxyType p;
      p._d = std::make_shared<Proxy::ProxyData>(_vertex, _impl.lock());
      return p;
    }

   private:
    Model::Vertex _vertex{};
    std::weak_ptr<Impl> _impl;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  /**
   * Information to be stored with each vertex
   */
  struct VertexProperties {
    enum class Type {
      invalid,
      root,
      moduleGroup,
      applicationModule,
      variableGroup,
      deviceModule,
      processVariable,
      directory
    };
    Type type{Type::invalid};

    struct InvalidProperties {}; /// This is used to allow default construction of the std::variant
    struct RootProperties {
      ModuleGroup& module;
    };
    struct ModuleGroupProperties {
      std::string name;
      ModuleGroup& module;
    };
    struct ApplicationModuleProperties {
      std::string name;
      ApplicationModule& module;
    };
    struct VariableGroupProperties {
      std::string name;
      VariableGroup& module;
    };
    struct DeviceModuleProperties {
      std::string aliasOrCdd;
      NonOwningProxy<ProcessVariableProxy> trigger;
      DeviceModule& module;
    };
    struct ProcessVariableProperties {
      std::string name;
      std::vector<std::shared_ptr<VariableNetworkNode>> nodes;
      std::unordered_set<std::string> tags;
    };
    struct DirectoryProperties {
      std::string name;
    };

    // The actual properties struct is stored in a std::variant as it depends on the vertex type
    std::variant<InvalidProperties, RootProperties, ModuleGroupProperties, ApplicationModuleProperties,
        VariableGroupProperties, DeviceModuleProperties, ProcessVariableProperties, DirectoryProperties>
        p;

    // Call the visitor and pass the Properties struct matching the current vertex type. The return value of the
    // visitor will be passed through.
    template<typename VISITOR>
    [[nodiscard]] typename std::invoke_result<VISITOR, ApplicationModuleProperties&>::type visit(VISITOR visitor) const;

    // Call the visitor and pass a Proxy matching the current vertex type.The return value of the visitor
    // will be passed through.
    template<typename VISITOR>
    typename std::invoke_result<VISITOR, ApplicationModuleProxy>::type visitProxy(
        VISITOR visitor, Vertex vertex, const std::shared_ptr<Impl>& impl) const;

    VertexProperties() = default;
    VertexProperties(const VertexProperties& other) = default;
    VertexProperties& operator=(const VertexProperties& other) {
      // in-place destroy and construct copy, since std::variant does not have a copy assignment operator.
      this->~VertexProperties();
      new(this) VertexProperties(other);
      return *this;
    }

    // Make proxy object for the given vertex or return existing proxy from weak pointer.
    // Note: No checks are made that the given vertex is the right one. Also it is not checked whether the proxy type
    // is correct, which is used in cases where the vertex can have multiple roles (e.g. root vertex can be RootProxy,
    // ModuleGroupProxy and DirectoryProxy).
    template<typename PROXY>
    PROXY makeProxy(Vertex vertex, const std::shared_ptr<Impl>& impl) const;

   private:
    // (Weak) pointer to Proxy, stored here (with the vertex) to make sure we use the same Proxy instance whenever
    // we refer to this vertex. Since the VertexProperties are stored inside the Graph and the Proxy contains a
    // shared pointer to the Graph, the pointer has to be weak to avoid a shared pointer loop.
    // This pointer is basically used as a cache for visitProxy() resp. makeProxy(), which needs to be a const
    // function. Hence this member variable must be made mutable.
    mutable std::weak_ptr<Proxy::ProxyData> _proxy;
  };

  /******************************************************************************************************************/

  /**
   * Information to be stored with each edge
   */
  struct EdgeProperties {
    enum class Type {
      invalid,
      pvAccess,      /// Edge represents access of an module to a PV. Arrow shows data flow direction (read/write).
      ownership,     /// Edge represents (C++) ownership. Arrow points towards the sub-module or PV.
      parenthood,    /// Edge represents the PV directory hierarchy. Arrow points towards the sub-directory or PV.
      neighbourhood, /// Edge points from a module to the directory where its PVs appear without hierarchy modification
      trigger        /// Edge represents trigger access. Arrow points from PV to device module.
    };
    Type type{Type::invalid};

    /// Can be true only for Type::pvAccess, in which case it indicates the presence of a return channel.
    bool pvAccessWithReturnChannel{false};
  };

  /******************************************************************************************************************/
  /**
   * Predicates to identify the type of a proxy or a properties struct. All predicates can be used in constexpr if
   * conditions to simplify type-specific access to members.
   */
  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool isRoot(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::RootProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, RootProxy>::value;
  }

  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool isModuleGroup(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::ModuleGroupProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, ModuleGroupProxy>::value;
  }

  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool isApplicationModule(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::ApplicationModuleProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, ApplicationModuleProxy>::value;
  }

  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool isVariableGroup(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::VariableGroupProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, VariableGroupProxy>::value;
  }

  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool isDeviceModule(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::DeviceModuleProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, DeviceModuleProxy>::value;
  }

  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool isVariable(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::ProcessVariableProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, ProcessVariableProxy>::value;
  }

  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool isDirectory(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::DirectoryProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, DirectoryProxy>::value;
  }

  /******************************************************************************************************************/

  template<typename PROPERTY_OR_PROXY>
  constexpr bool hasName(const PROPERTY_OR_PROXY&) {
    return std::is_same<PROPERTY_OR_PROXY, VertexProperties::ModuleGroupProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, ModuleGroupProxy>::value ||
        std::is_same<PROPERTY_OR_PROXY, VertexProperties::ApplicationModuleProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, ApplicationModuleProxy>::value ||
        std::is_same<PROPERTY_OR_PROXY, VertexProperties::VariableGroupProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, VariableGroupProxy>::value ||
        std::is_same<PROPERTY_OR_PROXY, VertexProperties::ProcessVariableProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, ProcessVariableProxy>::value ||
        std::is_same<PROPERTY_OR_PROXY, VertexProperties::DirectoryProperties>::value ||
        std::is_same<PROPERTY_OR_PROXY, DirectoryProxy>::value;
  }

  /******************************************************************************************************************/

  /**
   * Graph type for the model
   */
  using Graph = boost::adjacency_list<detail::OutEdgeListType, detail::VertexListType, boost::bidirectionalS,
      VertexProperties, EdgeProperties>;

  using Vertex = Graph::vertex_descriptor;

  using Edge = Graph::edge_descriptor;

  using EdgeFilteredView = boost::filtered_graph<Graph, std::function<bool(const Edge&)>, boost::keep_all>;

  /******************************************************************************************************************/

  /// Do not use these class definitions, instead use the static instances below.
  struct SearchType {};
  struct AdjacentSearch : SearchType {};
  struct AdjacentInSearch : SearchType {};
  struct AdjacentOutSearch : SearchType {};
  struct DepthFirstSearch : SearchType {};
  struct BreadthFirstSearch : SearchType {};

  /// Perform search of all adjacent objects in the model graph, independend of the relationship direction
  static constexpr AdjacentSearch adjacentSearch;

  /// Perform search of all adjacent objects in the model graph related in the incoming direction. That means the
  /// found objects are either the owner of the current object, its directory parent or the data source.
  static constexpr AdjacentInSearch adjacentInSearch;

  /// Perform search of all adjacent objects in the model graph related in the outgoing direction. That means the
  /// found objects are either owned by the current object, its directory member or the data sink.
  static constexpr AdjacentOutSearch adjacentOutSearch;

  /// Perform a depth first search on the model graph, starting at the current object.
  static constexpr DepthFirstSearch depthFirstSearch;

  /// Perform a breadth first search on the model graph, starting at the current object.
  static constexpr BreadthFirstSearch breadthFirstSearch;

  /******************************************************************************************************************/

  namespace detail {
    /**
     * Helper to hold values of an arbitrary type including void (in which case no value is held, but no error occurs)
     */
    template<typename T>
    struct ValueHolder {
      T value;
      T get() { return value; }
      using type = T;
    };

    template<>
    struct ValueHolder<void> {
      void get() {}
      using type = void;
    };
  } // namespace detail

  /******************************************************************************************************************/

  /// Do not use these class definitions, instead use the static instances below.
  struct SearchOption {};

  struct ContinueSearchDisjunctTrees : SearchOption {};

  struct ReturnFirstHit : SearchOption {};

  struct VisitOrder : SearchOption {
    enum class VisitOrderType { in, post };
    constexpr VisitOrder() = default;
    constexpr explicit VisitOrder(VisitOrderType t) : SearchOption(), type(t) {}

    VisitOrderType type{VisitOrderType::in};
  };

  template<typename T>
  struct ReturnFirstHitWithValue : ReturnFirstHit {
    detail::ValueHolder<T> notFoundValue; /// Value to be returned when no match was found
  };

  /// Use in combination with DepthFirstSearch to extend the search to disjunct parts of the tree.
  /// Disjuct tree parts can occur e.g. when filtering the model such that the connecting elements are gone, or when
  /// starting the search at a vertex from which there is no (forward) path to the root. If this search type is not
  /// specified, the search will terminate after exploring the subtree the current object is part of.
  static constexpr ContinueSearchDisjunctTrees continueSearchDisjunctTrees;

  /// Stop the search after the first hit and return. If the visitor returns a value, it will be passed on to the caller
  /// of the visit() function. If no match is found, the provided notFoundValue will be returned.
  template<typename T>
  static constexpr ReturnFirstHitWithValue<T> returnFirstHit(T notFoundValue) {
    ReturnFirstHitWithValue<T> rv;
    rv.notFoundValue.value = notFoundValue;
    return rv;
  }

  /// Stop the search after the first hit and return. If the visitor returns a value, it will be passed on to the caller
  /// of the visit() function. If no match is found, the provided notFoundValue will be returned.
  inline constexpr ReturnFirstHitWithValue<void> returnFirstHit() {
    ReturnFirstHitWithValue<void> rv{};
    return rv;
  }

  // Calls the visitor before walking along its edges. Default visiting order.
  static constexpr VisitOrder visitOrderIn{VisitOrder::VisitOrderType::in};

  // Calls the visitor after all edges have been visited. Use together with DFS or BFS to modify the visiting order.
  static constexpr VisitOrder visitOrderPost{VisitOrder::VisitOrderType::post};

  /******************************************************************************************************************/

  template<typename PROPERTIES>
  struct PropertyFilterTag {
    using PropertiesType = PROPERTIES;
  };

  /******************************************************************************************************************/

  // Forward declaration
  template<typename FILTER_LHS, typename FILTER_RHS>
  struct AndSet;

  /******************************************************************************************************************/

  template<typename FILTER_LHS, typename FILTER_RHS>
  struct OrSet : FILTER_LHS, FILTER_RHS {
    explicit constexpr OrSet(FILTER_LHS lhs, FILTER_RHS rhs) : FILTER_LHS(lhs), FILTER_RHS(rhs) {}

    [[nodiscard]] bool evalVertexFilter(const typename FILTER_LHS::PropertiesType& e) const {
      return FILTER_LHS::evalVertexFilter(e) || FILTER_RHS::evalVertexFilter(e);
    };

    [[nodiscard]] bool evalEdgeFilter(const typename FILTER_LHS::PropertiesType& e) const {
      return FILTER_LHS::evalEdgeFilter(e) || FILTER_RHS::evalEdgeFilter(e);
    };

    template<typename FILTER_NEXT_RHS>
    constexpr auto operator||(const FILTER_NEXT_RHS& rhs) const {
      return OrSet<OrSet<FILTER_LHS, FILTER_RHS>, FILTER_NEXT_RHS>(*this, rhs);
    }

    template<typename FILTER_NEXT_RHS>
    constexpr auto operator&&(const FILTER_NEXT_RHS& rhs) const {
      return AndSet<OrSet<FILTER_LHS, FILTER_RHS>, FILTER_NEXT_RHS>(*this, rhs);
    }

    template<typename PROXY>
    [[nodiscard]] constexpr bool constevalObjecttype() const {
      return FILTER_LHS::template constevalObjecttype<PROXY>() || FILTER_RHS::template constevalObjecttype<PROXY>();
    }
  };

  /******************************************************************************************************************/

  template<typename FILTER_LHS, typename FILTER_RHS>
  struct AndSet : FILTER_LHS, FILTER_RHS {
    explicit constexpr AndSet(FILTER_LHS lhs, FILTER_RHS rhs) : FILTER_LHS(lhs), FILTER_RHS(rhs) {}

    [[nodiscard]] bool evalVertexFilter(const typename FILTER_LHS::PropertiesType& e) const {
      return FILTER_LHS::evalVertexFilter(e) && FILTER_RHS::evalVertexFilter(e);
    };

    [[nodiscard]] bool evalEdgeFilter(const typename FILTER_LHS::PropertiesType& e) const {
      return FILTER_LHS::evalEdgeFilter(e) && FILTER_RHS::evalEdgeFilter(e);
    };

    template<typename FILTER_NEXT_RHS>
    constexpr auto operator||(const FILTER_NEXT_RHS& rhs) const {
      return OrSet<AndSet<FILTER_LHS, FILTER_RHS>, FILTER_NEXT_RHS>(*this, rhs);
    }

    template<typename FILTER_NEXT_RHS>
    constexpr auto operator&&(const FILTER_NEXT_RHS& rhs) const {
      return AndSet<AndSet<FILTER_LHS, FILTER_RHS>, FILTER_NEXT_RHS>(*this, rhs);
    }

    template<typename PROXY>
    [[nodiscard]] constexpr bool constevalObjecttype() const {
      return FILTER_LHS::template constevalObjecttype<PROXY>() && FILTER_RHS::template constevalObjecttype<PROXY>();
    }
  };

  /******************************************************************************************************************/

  template<typename FILTER>
  struct EdgeFilter : PropertyFilterTag<EdgeProperties> {
    explicit constexpr EdgeFilter(FILTER filter) : _filter(filter) {}
    constexpr EdgeFilter(const EdgeFilter<FILTER>& rhs) noexcept = default;
    constexpr EdgeFilter(EdgeFilter<FILTER>&& rhs) noexcept = default;

    template<typename FILTER_RHS>
    constexpr auto operator||(const FILTER_RHS& rhs) const {
      static_assert(std::is_base_of<PropertyFilterTag<EdgeProperties>, FILTER_RHS>::value,
          "Logical OR operator || cannot be used on different filter types.");
      return OrSet(*this, rhs);
    }

    template<typename FILTER_RHS>
    constexpr auto operator&&(const FILTER_RHS& rhs) const {
      static_assert(std::is_base_of<PropertyFilterTag<EdgeProperties>, FILTER_RHS>::value,
          "Logical AND operator && cannot be used on different filter types.");
      return AndSet(*this, rhs);
    }

    [[nodiscard]] bool evalEdgeFilter(const EdgeProperties& e) const { return _filter(e); };

   private:
    template<typename FILTER_RHS>
    friend struct EdgeFilter;

    FILTER _filter;
  };

  /******************************************************************************************************************/

  template<typename FILTER>
  struct VertexFilter : PropertyFilterTag<VertexProperties> {
    explicit constexpr VertexFilter(FILTER filter) : _filter(std::move(filter)) {}
    constexpr VertexFilter(const VertexFilter<FILTER>& rhs) = default;
    constexpr VertexFilter(VertexFilter<FILTER>&& rhs) noexcept = default;

    template<typename FILTER_RHS>
    constexpr auto operator||(const FILTER_RHS& rhs) const {
      static_assert(std::is_base_of<PropertyFilterTag<VertexProperties>, FILTER_RHS>::value,
          "Logical OR operator || cannot be used on different filter types.");
      return OrSet(*this, rhs);
    }

    template<typename FILTER_RHS>
    constexpr auto operator&&(const FILTER_RHS& rhs) const {
      static_assert(std::is_base_of<PropertyFilterTag<VertexProperties>, FILTER_RHS>::value,
          "Logical AND operator && cannot be used on different filter types.");
      return AndSet(*this, rhs);
    }

    [[nodiscard]] bool evalVertexFilter(const VertexProperties& e) const { return _filter(e); };

    // Filter on object type in constexpr condition. Will be overridden by ObjecttypeFilter.
    template<typename PROXY>
    [[nodiscard]] constexpr bool constevalObjecttype() const {
      return true;
    }

   private:
    template<typename FILTER_RHS>
    friend struct VertexFilter;

    FILTER _filter;
  };

  /******************************************************************************************************************/

  [[maybe_unused]] static auto keepAll = [](const auto&) -> bool { return true; };
  [[maybe_unused]] static auto keepAllEdges = EdgeFilter(keepAll);
  [[maybe_unused]] static auto keepAllVertices = VertexFilter(keepAll);

  /******************************************************************************************************************/
  /******************************************************************************************************************/

  template<EdgeProperties::Type RELATIONSHIP>
  [[maybe_unused]] static constexpr auto relationshipFilter =
      EdgeFilter([](const EdgeProperties& e) -> bool { return e.type == RELATIONSHIP; });

  /******************************************************************************************************************/

  static constexpr auto keepPvAccess = relationshipFilter<EdgeProperties::Type::pvAccess>;
  static constexpr auto keepPvAccesWithReturnChannel = Model::EdgeFilter([](const Model::EdgeProperties& edge) -> bool {
    return edge.type == Model::EdgeProperties::Type::pvAccess && edge.pvAccessWithReturnChannel;
  });
  static constexpr auto keepOwnership = relationshipFilter<EdgeProperties::Type::ownership>;
  static constexpr auto keepParenthood = relationshipFilter<EdgeProperties::Type::parenthood>;
  static constexpr auto keepNeighbourhood = relationshipFilter<EdgeProperties::Type::neighbourhood>;

  /******************************************************************************************************************/
  /******************************************************************************************************************/

  namespace detail {
    template<VertexProperties::Type OBJECTTYPE>
    [[maybe_unused]] static constexpr auto objecttypeFilterFunctor =
        [](const VertexProperties& e) -> bool { return e.type == OBJECTTYPE; };
  } // namespace detail

  /******************************************************************************************************************/

  // FIXME: This _should_ be fine since it is not possible to use those templates
  // in different compilation units. This is the recommended work-around from the GCC manpage
  // GCC will complain about base class using internal linkage because of the base class depending
  // on the type of lambdas which get a different symbol for every compilation unit they are used in
  // NOLINTNEXTLINE(google-build-namespaces)
  namespace {
    template<VertexProperties::Type OBJECTTYPE, typename PROXYTYPE>
    struct ObjecttypeFilter : VertexFilter<decltype(detail::objecttypeFilterFunctor<OBJECTTYPE>)> {
      constexpr ObjecttypeFilter()
      : VertexFilter<decltype(detail::objecttypeFilterFunctor<OBJECTTYPE>)>(
            detail::objecttypeFilterFunctor<OBJECTTYPE>) {}

      template<typename FILTER_RHS>
      constexpr auto operator||(const FILTER_RHS& rhs) const {
        static_assert(std::is_base_of<PropertyFilterTag<VertexProperties>, FILTER_RHS>::value,
            "Logical OR operator || cannot be used on different filter types.");
        return OrSet(*this, rhs);
      }

      template<typename FILTER_RHS>
      constexpr auto operator&&(const FILTER_RHS& rhs) const {
        static_assert(std::is_base_of<PropertyFilterTag<VertexProperties>, FILTER_RHS>::value,
            "Logical AND operator && cannot be used on different filter types.");
        return AndSet(*this, rhs);
      }

      template<typename PROXY>
      [[nodiscard]] constexpr bool constevalObjecttype() const {
        return std::is_same<PROXY, PROXYTYPE>::value;
      }
    };

    /******************************************************************************************************************/
  } // namespace

  /******************************************************************************************************************/
  constexpr static auto keepModuleGroups = ObjecttypeFilter<VertexProperties::Type::moduleGroup, ModuleGroupProxy>();
  constexpr static auto keepApplicationModules =
      ObjecttypeFilter<VertexProperties::Type::applicationModule, ApplicationModuleProxy>();
  constexpr static auto keepVariableGroups =
      ObjecttypeFilter<VertexProperties::Type::variableGroup, VariableGroupProxy>();
  constexpr static auto keepDeviceModules = ObjecttypeFilter<VertexProperties::Type::deviceModule, DeviceModuleProxy>();
  constexpr static auto keepProcessVariables =
      ObjecttypeFilter<VertexProperties::Type::processVariable, ProcessVariableProxy>();
  constexpr static auto keepDirectories = ObjecttypeFilter<VertexProperties::Type::directory, DirectoryProxy>();

  namespace detail {
    /**
     * Helper function to find one of the Proxy types which will be let through by the given vertex filter.
     */
    template<typename VERTEX_FILTER>
    constexpr auto findVertexFilterAcceptedProxyType(VERTEX_FILTER vertexFilter) {
      if constexpr(vertexFilter.template constevalObjecttype<ModuleGroupProxy>()) {
        return ModuleGroupProxy();
      }
      else if constexpr(vertexFilter.template constevalObjecttype<ApplicationModuleProxy>()) {
        return ApplicationModuleProxy();
      }
      else if constexpr(vertexFilter.template constevalObjecttype<VariableGroupProxy>()) {
        return VariableGroupProxy();
      }
      else if constexpr(vertexFilter.template constevalObjecttype<DeviceModuleProxy>()) {
        return DeviceModuleProxy();
      }
      else if constexpr(vertexFilter.template constevalObjecttype<ProcessVariableProxy>()) {
        return ProcessVariableProxy();
      }
      else if constexpr(vertexFilter.template constevalObjecttype<DirectoryProxy>()) {
        return DirectoryProxy();
      }
      else {
        return RootProxy();
      }
    }

    /**
     * Helper to get the return type of a visitor functor, which accepts any proxy type which can pass the given vertex
     * filter.
     */
    template<typename VISITOR, typename VERTEX_FILTER>
    using VisitorReturnType = typename std::invoke_result<VISITOR,
        typename std::invoke_result<decltype(detail::findVertexFilterAcceptedProxyType<VERTEX_FILTER>),
            VERTEX_FILTER>::type>::type;

  } // namespace detail

  /******************************************************************************************************************/
  /******************************************************************************************************************/

  [[maybe_unused]] static auto keepName(const std::string& name) {
    return VertexFilter([name](const VertexProperties& e) -> bool {
      return e.visit([name](auto props) -> bool {
        if constexpr(hasName(props)) {
          return props.name == name;
        }
        else {
          return false;
        }
      });
    });
  }

  /******************************************************************************************************************/

  [[maybe_unused]] static auto keepTag(std::string name) {
    return VertexFilter([name](const VertexProperties& e) -> bool {
      return e.visit([name](auto props) -> bool {
        if constexpr(isVariable(props)) {
          return props.tags.count(name);
        }
        else {
          return false;
        }
      });
    });
  }

  /******************************************************************************************************************/
  /******************************************************************************************************************/

  template<typename A, typename B>
  struct CombinedSearchConfig : A, B {
    constexpr CombinedSearchConfig(A a, B b) : A(a), B(b) {}
  };

  /******************************************************************************************************************/

  template<typename FIRST, typename... MORE>
  constexpr auto combinedSearchConfig(FIRST first, MORE... more) {
    if constexpr(sizeof...(more) > 0) {
      return CombinedSearchConfig(first, combinedSearchConfig(more...));
    }
    else {
      return first;
    }
  }

  /******************************************************************************************************************/

  static constexpr auto ownedModuleGroups = combinedSearchConfig(
      ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepOwnership, ChimeraTK::Model::keepModuleGroups);

  static constexpr auto ownedApplicationModules = combinedSearchConfig(
      ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepOwnership, ChimeraTK::Model::keepApplicationModules);

  static constexpr auto ownedVariableGroups = combinedSearchConfig(
      ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepOwnership, ChimeraTK::Model::keepVariableGroups);

  static constexpr auto ownedVariables = combinedSearchConfig(
      ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepOwnership, ChimeraTK::Model::keepProcessVariables);

  static constexpr auto childDirectories = combinedSearchConfig(
      ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepParenthood, ChimeraTK::Model::keepDirectories);

  static constexpr auto childVariables = combinedSearchConfig(
      ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepParenthood, ChimeraTK::Model::keepProcessVariables);

  static constexpr auto children =
      combinedSearchConfig(ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepParenthood);

  static constexpr auto getOwner =
      combinedSearchConfig(ChimeraTK::Model::adjacentInSearch, ChimeraTK::Model::keepOwnership);

  static constexpr auto getParent =
      combinedSearchConfig(ChimeraTK::Model::adjacentInSearch, ChimeraTK::Model::keepParenthood);

  static constexpr auto getNeighbourDirectory =
      combinedSearchConfig(ChimeraTK::Model::adjacentOutSearch, ChimeraTK::Model::keepNeighbourhood);

  static constexpr auto neighbourModules =
      combinedSearchConfig(ChimeraTK::Model::adjacentInSearch, ChimeraTK::Model::keepNeighbourhood);

  /******************************************************************************************************************/
  /******************************************************************************************************************/

  /**
   * Visitor function for use with Proxy::visit() to return a found ModuleGroupProxy.
   *
   * Use together with Model::returnFirstHit(ModuleGroupProxy{}) and some search criteria which search for
   * module groups (typically exactly one).
   */
  static constexpr auto returnModuleGroup = [](auto am) -> ModuleGroupProxy {
    if constexpr(isModuleGroup(am)) {
      return am;
    }
    else {
      throw ChimeraTK::logic_error("Model: ModuleGroupProxy expected, something else found.");
    }
  };

  /******************************************************************************************************************/

  /**
   * Visitor function for use with Proxy::visit() to return a found ApplicationModuleProxy.
   *
   * Use together with Model::returnFirstHit(ApplicationModuleProxy{}) and some search criteria which search for
   * application modules (typically exactly one).
   */
  static constexpr auto returnApplicationModule = [](auto am) -> ApplicationModuleProxy {
    if constexpr(isApplicationModule(am)) {
      return am;
    }
    else {
      throw ChimeraTK::logic_error("Model: ApplicationModuleProxy expected, something else found.");
    }
  };

  /******************************************************************************************************************/

  /**
   * Visitor function for use with Proxy::visit() to return a found VariableGroupProxy.
   *
   * Use together with Model::returnFirstHit(VariableGroupProxy{}) and some search criteria which search for
   * variable groups (typically exactly one).
   */
  static constexpr auto returnVariableGroup = [](auto am) -> VariableGroupProxy {
    if constexpr(isVariableGroup(am)) {
      return am;
    }
    else {
      throw ChimeraTK::logic_error("Model: VariableGroupProxy expected, something else found.");
    }
  };

  /******************************************************************************************************************/

  /**
   * Visitor function for use with Proxy::visit() to return a found ProcessVariableProxy.
   *
   * Use together with Model::returnFirstHit(ProcessVariableProxy{}) and some search criteria which search for
   * process variables (typically exactly one).
   */
  static constexpr auto returnProcessVariable = [](auto am) -> ProcessVariableProxy {
    if constexpr(isVariable(am)) {
      return am;
    }
    else {
      throw ChimeraTK::logic_error("Model: ProcessVariableProxy expected, something else found.");
    }
  };

  /******************************************************************************************************************/

  /**
   * Visitor function for use with Proxy::visit() to return a found DirectoryProxy.
   *
   * Use together with Model::returnFirstHit(DirectoryProxy()) and some search criteria which search for directories
   * (typically exactly one).
   */
  static constexpr auto returnDirectory = [](auto dir) -> DirectoryProxy {
    if constexpr(isDirectory(dir) || isRoot(dir)) {
      return DirectoryProxy(dir);
    }
    else {
      throw ChimeraTK::logic_error("Model: DirectoryProxy expected, something else found.");
    }
  };

  /******************************************************************************************************************/
  /******************************************************************************************************************/

  template<typename FIRST, typename... ARGS>
  constexpr auto getEdgeFilter([[maybe_unused]] FIRST first, ARGS... args) {
    if constexpr(std::is_base_of<PropertyFilterTag<EdgeProperties>, FIRST>::value) {
      return first;
    }
    else if constexpr(sizeof...(args) == 0) {
      return keepAllEdges;
    }
    else {
      return getEdgeFilter(args...);
    }
  }

  constexpr auto getEdgeFilter() {
    return keepAllEdges;
  }

  /******************************************************************************************************************/

  template<typename FIRST, typename... ARGS>
  constexpr auto getVertexFilter([[maybe_unused]] FIRST first, ARGS... args) {
    if constexpr(std::is_base_of<PropertyFilterTag<VertexProperties>, FIRST>::value) {
      return first;
    }
    else if constexpr(sizeof...(args) == 0) {
      return keepAllVertices;
    }
    else {
      return getVertexFilter(args...);
    }
  }

  constexpr auto getVertexFilter() {
    return keepAllVertices;
  }

  /******************************************************************************************************************/

  template<typename SEARCH_TYPE>
  struct SearchTypeHolder {
    using type = SEARCH_TYPE;
  };

  template<typename FIRST, typename... ARGS>
  constexpr auto getSearchType() {
    if constexpr(std::is_base_of<SearchType, FIRST>::value) {
      return SearchTypeHolder<FIRST>();
    }
    else if constexpr(sizeof...(ARGS) == 0) {
      return adjacentOutSearch;
    }
    else {
      return getSearchType<ARGS...>();
    }
  }

  constexpr auto getSearchType() {
    return adjacentOutSearch;
  }

  /******************************************************************************************************************/

  template<typename SEARCH_OPTION_TO_FIND, typename FIRST, typename... ARGS>
  constexpr bool hasSearchOption() {
    if constexpr(std::is_base_of<SEARCH_OPTION_TO_FIND, FIRST>::value) {
      return true;
    }
    else if constexpr(sizeof...(ARGS) == 0) {
      return false;
    }
    else {
      return hasSearchOption<SEARCH_OPTION_TO_FIND, ARGS...>();
    }
  }

  template<typename SEARCH_OPTION_TO_FIND>
  constexpr bool hasSearchOption() {
    return false;
  }

  /******************************************************************************************************************/

  template<typename SEARCH_OPTION_TO_FIND, typename FIRST, typename... ARGS>
  constexpr auto getSearchOption([[maybe_unused]] FIRST first, ARGS... args) {
    if constexpr(std::is_base_of<SEARCH_OPTION_TO_FIND, FIRST>::value) {
      return first;
    }
    else if constexpr(sizeof...(ARGS) == 0) {
      throw ChimeraTK::logic_error("Model::getSearchOption() called but search option not found!");
    }
    else {
      return getSearchOption<SEARCH_OPTION_TO_FIND, ARGS...>(args...);
    }
  }

  template<typename SEARCH_OPTION_TO_FIND>
  constexpr auto getSearchOption() {
    throw ChimeraTK::logic_error("Model::getSearchOption() called but search option not found!");
  }

  /******************************************************************************************************************/

  template<typename FIRST, typename... ARGS>
  constexpr void checkConfigValidity(FIRST, ARGS... args) {
    static_assert(std::is_base_of<PropertyFilterTag<EdgeProperties>, FIRST>::value ||
            std::is_base_of<PropertyFilterTag<VertexProperties>, FIRST>::value ||
            std::is_base_of<SearchType, FIRST>::value || std::is_base_of<SearchOption, FIRST>::value,
        "Wrong type passed in search configuration argument list. Must be ether an EdgeFilter, a VertexFilter, a "
        "SearchType or a SearchOption.");
    if constexpr(sizeof...(args) > 0) {
      checkConfigValidity(args...);
    }
  }

  constexpr void checkConfigValidity() {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  /**
   * Implementation class for the model. This class is not used directly (hence all members are private), instead the
   * Proxy classes delegate their functions here.
   */
  class Impl : public std::enable_shared_from_this<Impl> {
    friend class Proxy;
    friend class RootProxy;
    friend class ModuleGroupProxy;
    friend class ApplicationModuleProxy;
    friend class VariableGroupProxy;
    friend class DeviceModuleProxy;
    friend class ProcessVariableProxy;
    friend class DirectoryProxy;

   private:
    // convenience functions just redirecting to generic_add.
    ModuleGroupProxy add(Model::Vertex owner, ModuleGroup& module);
    ApplicationModuleProxy add(Model::Vertex owner, ApplicationModule& module);
    VariableGroupProxy add(Model::Vertex owner, VariableGroup& module);
    DeviceModuleProxy add(Model::Vertex owner, DeviceModule& module);

    // common implementation to add any object (ModuleGroup, ApplicationModule, VariableGroup or DeviceModule)
    template<typename PROXY, typename MODULE, typename PROPS, Model::VertexProperties::Type TYPE>
    PROXY genericAdd(Model::Vertex owner, MODULE& module);

    // convenience functions just redirecting to generic_remove.
    void remove(ModuleGroup& module);
    void remove(ApplicationModule& module);
    void remove(VariableGroup& module);
    void remove(DeviceModule& module);

    // Common implementation to remove any object (ModuleGroup, ApplicationModule, VariableGroup or DeviceModule).
    // Do not use for ProcessVariables, remove the node from the ProcessVariable instead.
    template<typename MODULE>
    void genericRemove(MODULE& module);

    // Add variable to parent directory if not yet existing. The corresponding proxy is returned even if the variable
    // existed before.
    ProcessVariableProxy addVariable(Model::Vertex parent, const std::string& name);

    // Add directory to parent directory if not yet existing. The corresponding proxy is returned even if the directory
    // existed before.
    DirectoryProxy addDirectory(Model::Vertex parent, const std::string& name);

    // Add directory tree identified by qualifiedPath and return the lowest-level directory. Any components which
    // currently do not exists will be created. The function will not fail even if the entire path already exists,
    // it will in that case simply return the existing directory.
    DirectoryProxy addDirectoryRecursive(Model::Vertex parent, const std::string& qualifiedPath);

    // module can be either a ApplicationModuleProxy or a VariableGroupProxy. In case of a VariableGroupProxy, the
    // pvAccess-typed edge will connect to the ApplicationModule owning the VariableGroup.
    template<typename PROXY>
    void addVariableNode(PROXY module, ProcessVariableProxy& variable, VariableNetworkNode& node);

    template<typename... Args>
    constexpr auto getFilteredGraph(Args... config) const;

    template<typename VISITOR, typename... Args>
    auto visit(Vertex startVertex, VISITOR visitor, Args... args);

    [[nodiscard]] std::string getFullyQualifiedPath(Vertex vertex);

    template<typename VISITOR, typename PROXY>
    bool visitByPath(std::string_view path, VISITOR visitor, PROXY startProxy);

    Model::Graph _graph;

    // Counter for bookkeeping whether we are currently visiting (i.e. iterating) the _graph or whether modifying
    // the _graph is allowed.
    std::atomic<size_t> _graphVisitingLevel{0};

    Model::EdgeFilteredView _ownershipView{_graph,
        [&](const Model::Edge& edge) -> bool { return _graph[edge].type == Model::EdgeProperties::Type::ownership; },
        boost::keep_all()};
  };

  /********************************************************************************************************************/

  /**
   * The data holding struct for the proxy classes.
   */
  struct Proxy::ProxyData {
    Model::Vertex vertex{};
    std::shared_ptr<Impl> impl;
    ProxyData() = default;
    ProxyData(Model::Vertex v, std::shared_ptr<Impl> i) : vertex(v), impl(std::move(i)) {}
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of Proxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename VISITOR, typename... Args>
  auto Proxy::visit(VISITOR visitor, Args... args) const {
    return _d->impl->visit(_d->vertex, visitor, args...);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of Impl */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename... Args>
  constexpr auto Impl::getFilteredGraph(Args... config) const {
    Model::checkConfigValidity(config...);

    auto edgeFilter = Model::getEdgeFilter(config...);

    // We cannot pass (non-default constructible) lambdas to the filtered_graph directly, since at least the
    // depth_first_search tried to default-construct their types at some point. Also the lifetime of the lambdas needs
    // to go beyond the scope of this function, hence we must not capture by reference!
    [[maybe_unused]] std::function edgeFilterFunctor = [this, edgeFilter](const Model::Edge& e) -> bool {
      Model::EdgeProperties props = _graph[e];
      return edgeFilter.evalEdgeFilter(props);
    };

    // Performance optimisation: Only pass predicates that really do something
    constexpr bool filterEdges = !std::is_same<decltype(edgeFilter), decltype(keepAllEdges)>::value;
    if constexpr(filterEdges) {
      return boost::filtered_graph(_graph, edgeFilterFunctor);
    }
    else {
      return _graph;
    }
  }

  /********************************************************************************************************************/

  namespace detail {
    // Helper class for Impl::visit()
    template<typename BASE, typename VISITOR, typename FILTER, bool RETURN_FIRST_HIT>
    struct VisitorHelper : public BASE {
      // Call given visitor functor for each vertex (at "discover_vertex"), stop when finishing vertex stopAfterVertex
      // (checked in "finish_vertex"). By default, stopAfterVertex is set to an out-of-range index so the search is
      // not stopped. The stopping is realised by throwing a DfsVisitor::StopException.
      explicit VisitorHelper(VISITOR& visitor, std::shared_ptr<Impl> impl, FILTER& filter, Vertex stopAfterVertex,
          ValueHolder<detail::VisitorReturnType<VISITOR, FILTER>>& rv, VisitOrder& visitOrder)
      : _visitor(visitor), _filter(filter), _stopAfterVertex(stopAfterVertex), _impl(std::move(impl)), _rv(rv),
        _visitOrder{visitOrder} {}

      // This is a required function by boost::graph - disable naming check
      template<class Vertex, class Graph>
      // NOLINTNEXTLINE(readability-identifier-naming)
      void discover_vertex(Vertex v, Graph& g) {
        if(_visitOrder.type == VisitOrder::VisitOrderType::in) {
          doVisit(v, g);
        }
      }

      // This is a required function by boost::graph - disable naming check
      template<class Vertex, class Graph>
      // NOLINTNEXTLINE(readability-identifier-naming)
      void finish_vertex(Vertex v, Graph& g) {
        if(_visitOrder.type == VisitOrder::VisitOrderType::post) {
          doVisit(v, g);
        }
        if(v == _stopAfterVertex) {
          throw StopException();
        }
      }

      template<class Vertex, class Graph>
      void doVisit(Vertex v, Graph& g) {
        // apply vertex filter
        if(!_filter.evalVertexFilter(g[v])) {
          return;
        }

        if constexpr(RETURN_FIRST_HIT) {
          if constexpr(!std::is_same<detail::VisitorReturnType<VISITOR, FILTER>, void>::value) {
            _rv.value = g[v].visitProxy(_visitor, v, _impl);
          }
          else {
            g[v].visitProxy(_visitor, v, _impl);
          }
          throw StopException();
        }
        else {
          g[v].visitProxy(_visitor, v, _impl);
        }
      }

      auto getReturnValue() { return _rv.get(); }

      // This exception is just used to trick the boost::depth_first_search() to stop.
      class StopException : public std::exception {};

     protected:
      VISITOR& _visitor;
      FILTER& _filter;
      Vertex _stopAfterVertex;
      std::shared_ptr<Impl> _impl;
      ValueHolder<detail::VisitorReturnType<VISITOR, FILTER>>& _rv;
      VisitOrder _visitOrder;
    };

  } // namespace detail

  /********************************************************************************************************************/

  template<typename VISITOR, typename... Args>
  auto Impl::visit(Vertex startVertex, VISITOR visitor, Args... args) {
    // increase visiting level to prevent modification while iterating, decrease when leaving this function
    _graphVisitingLevel++;
    auto decrementVisitingLevel = cppext::finally([&] { _graphVisitingLevel--; });

    auto filteredGraph = getFilteredGraph(args...);

    auto vertexFilter = Model::getVertexFilter(args...);

    // Execute search operation based on selected type
    auto typeHolder = Model::getSearchType<Args...>();
    using type = typename decltype(typeHolder)::type;
    constexpr bool isAdjacent =
        std::is_same<AdjacentSearch, type>::value || std::is_base_of<AdjacentSearch, type>::value;
    constexpr bool isAdjacentIn =
        std::is_same<AdjacentInSearch, type>::value || std::is_base_of<AdjacentInSearch, type>::value;
    constexpr bool isAdjacentOut =
        std::is_same<AdjacentOutSearch, type>::value || std::is_base_of<AdjacentOutSearch, type>::value;
    constexpr bool isDFS =
        std::is_same<DepthFirstSearch, type>::value || std::is_base_of<DepthFirstSearch, type>::value;
    constexpr bool isBFS =
        std::is_same<BreadthFirstSearch, type>::value || std::is_base_of<BreadthFirstSearch, type>::value;

    constexpr bool continueDisjunct = Model::hasSearchOption<ContinueSearchDisjunctTrees, Args...>();
    constexpr bool returnFirst = Model::hasSearchOption<ReturnFirstHit, Args...>();

    // Wrapper for visitor to filter wrong objecttypes in constexpr condition, so the actual visitor does not have
    // to work with all proxy types, just the ones which can get past the VisitorFilter.
    using VisitorReturnType = detail::VisitorReturnType<VISITOR, decltype(vertexFilter)>;
    auto visitorWrapper = [&](auto proxy) -> VisitorReturnType {
      if constexpr(vertexFilter.template constevalObjecttype<decltype(proxy)>()) {
        return visitor(proxy);
      }
      // The VertexFilter makes sure that this function is never called with the wrong type, hence if we end up here
      // there is a bug in the VertexFilter. Note that this code will get instantiated, just never executed.
      throw ChimeraTK::logic_error("We should never end up here...");
    };

    if constexpr(isAdjacent || isAdjacentOut) {
      auto [start, end] = boost::out_edges(startVertex, filteredGraph);
      for(auto it = start; it != end; ++it) {
        // obtain target vertex of the current outgoing edge (i.e. vertex at the other end of the edge)
        auto vtx = target(*it, _graph);

        // apply vertex filter
        if(!vertexFilter.evalVertexFilter(_graph[vtx])) {
          continue;
        }

        if constexpr(returnFirst) {
          return filteredGraph[vtx].visitProxy(visitorWrapper, vtx, shared_from_this());
        }
        else {
          filteredGraph[vtx].visitProxy(visitorWrapper, vtx, shared_from_this());
        }
      }
    }
    if constexpr(isAdjacent || isAdjacentIn) {
      auto [start, end] = boost::in_edges(startVertex, filteredGraph);
      for(auto it = start; it != end; ++it) {
        // obtain source vertex of the current incoming edge (i.e. vertex at the other end of the edge)
        auto vtx = source(*it, _graph);

        // apply vertex filter
        if(!vertexFilter.evalVertexFilter(_graph[vtx])) {
          continue;
        }

        if constexpr(returnFirst) {
          return filteredGraph[vtx].visitProxy(visitorWrapper, vtx, shared_from_this());
        }
        else {
          filteredGraph[vtx].visitProxy(visitorWrapper, vtx, shared_from_this());
        }
      }
    }
    if constexpr(isDFS || isBFS) {
      // For DFS: By default stop the search when the start vertex is finished, unless configured otherwise
      // For BFS: The search anyway does not continue with disjunct parts, and stopping when the start vertex is
      // finished would stop too early (no recursion).
      Vertex stopVertex;
      if constexpr(continueDisjunct || isBFS) {
        stopVertex = std::numeric_limits<Vertex>::max();
      }
      else {
        stopVertex = startVertex;
      }

      // create visitor object
      detail::ValueHolder<detail::VisitorReturnType<decltype(visitorWrapper), decltype(vertexFilter)>> rv;
      constexpr bool hasVisitOrderOption = Model::hasSearchOption<VisitOrder, Args...>();
      VisitOrder visitOrder;
      if constexpr(hasVisitOrderOption) {
        visitOrder = Model::getSearchOption<VisitOrder, Args...>(args...);
      }
      auto visitorObjectFactory = [&]() {
        if constexpr(isDFS) {
          return detail::VisitorHelper<boost::dfs_visitor<>, decltype(visitorWrapper), decltype(vertexFilter),
              returnFirst>(visitorWrapper, shared_from_this(), vertexFilter, stopVertex, rv, visitOrder);
        }
        else {
          return detail::VisitorHelper<boost::bfs_visitor<>, decltype(visitorWrapper), decltype(vertexFilter),
              returnFirst>(visitorWrapper, shared_from_this(), vertexFilter, stopVertex, rv, visitOrder);
        }
      };
      auto visitorObject = visitorObjectFactory();

      // Prepare a color_map for use with depth_first_search resp. breadth_first_search algorithms.
      //
      // Default color_map works only for boost::vecS for the vertex container, but we are using boost::listS.
      // BOOST documentation is minimalistic in this point. The following solution has been found "experimentally".
      //
      // The color_map is used by the algorithms (boost::depth_first_search and boost::breadth_first_search) to
      // keep track of seen vertices. It is used to store a value of the type boost::default_color_type for each
      // vertex, so the map key needs to be some kind of identifier for the vertex.
      //
      // The default color_map uses the vertex index as a key, which is not possible, because there is no vertex
      // index when using boost::listS. The color_map argument has follow the concept "read/write property map".
      // associative_property_map implements this concept and can use a std::map as a basis.
      //
      // The type of the map key (Vertex) was found out more or less by trial and error.
      std::map<Vertex, boost::default_color_type> colors;
      boost::associative_property_map color_map(colors);

      // The try-catch block is just a trick to terminate the search operation early (when stopVertex hs finished)
      try {
        if constexpr(isDFS) {
          boost::depth_first_search(filteredGraph, visitorObject, color_map, startVertex);
        }
        else {
          boost::queue<Vertex> buffer;
          boost::breadth_first_search(filteredGraph, startVertex, buffer, visitorObject, color_map);
        }
      }
      catch(typename decltype(visitorObject)::StopException&) {
        // Ignore this exception, as it is just to use to trick the boost::depth/breadth_first_search() to stop
      }

      if constexpr(returnFirst) {
        return visitorObject.getReturnValue();
      }
    }

    if constexpr(returnFirst) {
      // Nothing found but ReturnFirstHit option specified: return the not-found value
      return Model::getSearchOption<ReturnFirstHit, Args...>(args...).notFoundValue.get();
    }
  }

  /********************************************************************************************************************/

  template<typename VISITOR, typename PROXY>
  bool Impl::visitByPath(std::string_view path, VISITOR visitor, PROXY startProxy) {
    // Note: the _graphVisitingLevel is not incremented here, since this visitByPath() function does not use any
    // iterators directly. Only when using visit() internally, iterators are used, but visit() increments the counter
    // itself.

    // remove any redundant "./" at the beginning
    while(boost::starts_with(path, "./")) {
      path = path.substr(2);
    }

    // resolve reference to ourself
    if(path.empty() || path == ".") {
      visitor(startProxy);
      return true;
    }

    // first component is one level up
    if(boost::starts_with(path, "../") || path == "..") {
      // remove the component from the path
      path = (path.size() > 2) ? path.substr(3) : "";

      // delegate to our parent proxy
      bool found = false;
      visit(
          startProxy._d->vertex,
          [&](auto parent) {
            if constexpr(isDirectory(parent) || isRoot(parent)) {
              found = parent.visitByPath(path, visitor);
            }
            else {
              assert(false); // directory structure guarantees only directories and the root as parents
            }
          },
          getParent);
      return found;
    }

    // first component refers to root
    if(boost::starts_with(path, "/")) {
      // remove the leading slash
      path = path.substr(1);

      // delegate to root proxy
      return RootProxy::makeRootProxy(shared_from_this()).visitByPath(path, visitor);
    }

    // first component is a child
    // split by first slash
    auto slash = path.find_first_of('/');
    auto childName = path.substr(0, slash);
    path = (slash < path.length() - 1) ? path.substr(slash + 1) : "";

    // delegate to child proxy
    bool found = false;
    visit(
        startProxy._d->vertex,
        [&](auto child) {
          if constexpr(isDirectory(child) || isVariable(child)) {
            if(child.getName() == childName) {
              found = child.visitByPath(path, visitor);
            }
          }
          else {
            assert(false); // search predicate guarantees only directories and variables
          }
        },
        children);
    return found;
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of RootProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename... Args>
  void RootProxy::writeGraphViz(const std::string& filename, Args... args) const {
    auto filteredGraph = _d->impl->getFilteredGraph(args...);

    auto vertexFilter = Model::getVertexFilter(args...);

    std::ofstream of(filename);

    auto vertexPropWriter = [&](std::ostream& out, const auto& vtx) {
      // apply vertex filter
      if(!vertexFilter.evalVertexFilter(_d->impl->_graph[vtx])) {
        return;
      }

      _d->impl->_graph[vtx].visit([&](auto prop) {
        out << "[";

        // Add label depending on type
        if constexpr(std::is_same<decltype(prop), VertexProperties::RootProperties>::value) {
          out << R"(label="/")";
        }
        else if constexpr(std::is_same<decltype(prop), VertexProperties::DeviceModuleProperties>::value) {
          out << R"(label=")" << prop.aliasOrCdd << R"(")";
        }
        else if constexpr(!std::is_same<decltype(prop), VertexProperties::InvalidProperties>::value) {
          out << R"(label=")" << prop.name << R"(")";
        }
        else {
          throw ChimeraTK::logic_error("Unexpected VertexProperties type");
        }

        out << ", ";

        // Set color depending on type
        if constexpr(std::is_same<decltype(prop), VertexProperties::RootProperties>::value) {
          out << "color=grey,style=filled";
        }
        else if constexpr(std::is_same<decltype(prop), VertexProperties::ModuleGroupProperties>::value) {
          out << "color=lightskyblue,style=filled";
        }
        else if constexpr(std::is_same<decltype(prop), VertexProperties::ApplicationModuleProperties>::value) {
          out << "color=cyan,style=filled";
        }
        else if constexpr(std::is_same<decltype(prop), VertexProperties::VariableGroupProperties>::value) {
          out << "color=springgreen,style=filled";
        }
        else if constexpr(std::is_same<decltype(prop), VertexProperties::DeviceModuleProperties>::value) {
          out << "color=yellow,style=filled";
        }
        else if constexpr(std::is_same<decltype(prop), VertexProperties::ProcessVariableProperties>::value) {
          out << "color=black";
        }
        else if constexpr(std::is_same<decltype(prop), VertexProperties::DirectoryProperties>::value) {
          out << "color=peachpuff,style=filled";
        }
        else {
          throw ChimeraTK::logic_error("Unexpected VertexProperties type");
        }
        out << "]";
      });
    };

    auto edgePropWriter = [&](std::ostream& out, const auto& edge) {
      out << "[";
      switch(_d->impl->_graph[edge].type) {
        case EdgeProperties::Type::parenthood: {
          out << "color=red, arrowhead=diamond";
          break;
        }
        case EdgeProperties::Type::ownership: {
          out << "color=blue, arrowhead=odot";
          break;
        }
        case EdgeProperties::Type::pvAccess: {
          out << "color=black";
          break;
        }
        case EdgeProperties::Type::neighbourhood: {
          out << "color=olive, arrowhead=tee";
          break;
        }
        case EdgeProperties::Type::trigger: {
          out << "color=grey, arrowhead=crow";
          break;
        }
        default: {
          throw ChimeraTK::logic_error("Unexpected EdgeProperties type");
        }
      }
      out << "]";
    };

    // Generate a vertex id map. write_graphviz will use those ids as node ids in the generated graphviz code.
    std::map<Vertex, size_t> vertex_ids;
    for(auto u : boost::make_iterator_range(vertices(filteredGraph))) {
      vertex_ids[u] = vertex_ids.size();
    }

    // Note that all 3 writers have to be specified to  pass the vertex id map, otherwise very weired error messages
    // will occur.
    boost::write_graphviz(of, filteredGraph, vertexPropWriter, edgePropWriter, boost::default_writer(),
        boost::make_assoc_property_map(vertex_ids));
  }

  /********************************************************************************************************************/

  template<typename VISITOR>
  bool RootProxy::visitByPath(std::string_view path, VISITOR visitor) const {
    return _d->impl->visitByPath(path, visitor, *this);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of DirectoryProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename VISITOR>
  bool DirectoryProxy::visitByPath(std::string_view path, VISITOR visitor) const {
    return _d->impl->visitByPath(path, visitor, *this);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of ProcessVariableProxy */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename VISITOR>
  bool ProcessVariableProxy::visitByPath(std::string_view path, VISITOR visitor) const {
    return _d->impl->visitByPath(path, visitor, *this);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations of VertexProperties */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename VISITOR>
  typename std::invoke_result<VISITOR, VertexProperties::ApplicationModuleProperties&>::type VertexProperties::visit(
      VISITOR visitor) const {
    switch(type) {
      case Type::root: {
        return visitor(std::get<RootProperties>(p));
        break;
      }
      case Type::moduleGroup: {
        return visitor(std::get<ModuleGroupProperties>(p));
        break;
      }
      case Type::applicationModule: {
        return visitor(std::get<ApplicationModuleProperties>(p));
        break;
      }
      case Type::variableGroup: {
        return visitor(std::get<VariableGroupProperties>(p));
        break;
      }
      case Type::deviceModule: {
        return visitor(std::get<DeviceModuleProperties>(p));
        break;
      }
      case Type::processVariable: {
        return visitor(std::get<ProcessVariableProperties>(p));
        break;
      }
      case Type::directory: {
        return visitor(std::get<DirectoryProperties>(p));
        break;
      }
      default: {
      }
    }
    throw ChimeraTK::logic_error("VertexProperties::visit() called for invalid-typed vertex.");
  }

  /********************************************************************************************************************/

  template<typename PROXY>
  PROXY VertexProperties::makeProxy(Vertex vertex, const std::shared_ptr<Impl>& impl) const {
    auto proxyShared = _proxy.lock();
    if(!proxyShared) {
      proxyShared = std::make_shared<Proxy::ProxyData>(vertex, impl);
      _proxy = proxyShared;
    }
    return PROXY(proxyShared);
  }

  /********************************************************************************************************************/

  template<typename VISITOR>
  typename std::invoke_result<VISITOR, ApplicationModuleProxy>::type VertexProperties::visitProxy(
      VISITOR visitor, Vertex vertex, const std::shared_ptr<Impl>& impl) const {
    switch(type) {
      case Type::root: {
        return visitor(makeProxy<RootProxy>(vertex, impl));
        break;
      }
      case Type::moduleGroup: {
        return visitor(makeProxy<ModuleGroupProxy>(vertex, impl));
        break;
      }
      case Type::applicationModule: {
        return visitor(makeProxy<ApplicationModuleProxy>(vertex, impl));
        break;
      }
      case Type::variableGroup: {
        return visitor(makeProxy<VariableGroupProxy>(vertex, impl));
        break;
      }
      case Type::deviceModule: {
        return visitor(makeProxy<DeviceModuleProxy>(vertex, impl));
        break;
      }
      case Type::processVariable: {
        return visitor(makeProxy<ProcessVariableProxy>(vertex, impl));
        break;
      }
      case Type::directory: {
        return visitor(makeProxy<DirectoryProxy>(vertex, impl));
        break;
      }
      case Type::invalid: {
      }
    }
    throw ChimeraTK::logic_error("VertexProperties::visitProxy() called for invalid-typed vertex.");
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK::Model
