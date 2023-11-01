// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "UserInputValidator.h"

#include "Module.h"

#include <utility>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  void UserInputValidator::setErrorFunction(const std::function<void(const std::string&)>& errorFunction) {
    _errorFunction = errorFunction;
  }

  /*********************************************************************************************************************/

  bool UserInputValidator::validate(const ChimeraTK::TransferElementID& change) {
    if(!change.isValid()) {
      return validateAll();
    }

    // We have downstream channels that signalized a change - invalidate all of our
    if(_downstreamInvalidatingReturnChannels.count(change) > 0) {
      for(auto& v : _variableMap) {
        v.second->reject();
      }

      return false;
    }

    if(!_validatorMap.count(change)) {
      return false;
    }

    for(auto* validator : _validatorMap.at(change)) {
      if(!validator->isValidFunction()) {
        _errorFunction(validator->errorMessage);
        _variableMap.at(change)->reject();
        return true;
      }
    }

    _variableMap.at(change)->accept();
    return false;
  }

  /*********************************************************************************************************************/

  bool UserInputValidator::validateAll() {
    bool rejected = false;
    for(auto& v : _variableMap) {
      rejected |= validate(v.first);
    }
    return rejected;
  }

  template<typename VISITOR>
  struct TopologicalOrderVisitorImpl : boost::dfs_visitor<> {
    TopologicalOrderVisitorImpl(VISITOR& visitor, std::shared_ptr<Model::Impl> impl)
    : _visitor(visitor), _impl(std::move(impl)) {}

    TopologicalOrderVisitorImpl(TopologicalOrderVisitorImpl&& Other) = delete;
    TopologicalOrderVisitorImpl(TopologicalOrderVisitorImpl& Other) = delete;
    TopologicalOrderVisitorImpl& operator=(TopologicalOrderVisitorImpl& other) = delete;

    template<class Vertex, class Graph>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void discoverVertex(Vertex v, Graph& g) {
      _nextCallShouldStop = g[v].visitProxy(_visitor, v, _impl);
    }

    template<class Vertex, class Graph>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void finishVertex(Vertex v, Graph& g) {
      if(g[v].type == Model::VertexProperties::Type::applicationModule) {
        stack.push_back(v);
        distances.try_emplace(v, std::numeric_limits<int>::min());
      }
    }

    bool shouldStopVisitingCurrentBranch() {
      bool shouldStop = _nextCallShouldStop;
      _nextCallShouldStop = false;
      return shouldStop;
    }

    VISITOR _visitor;
    std::shared_ptr<Model::Impl> _impl;
    std::vector<Model::Vertex> stack{};
    std::map<Model::Vertex, int> distances{};
    bool _nextCallShouldStop{false};
  };

  template<typename VISITOR>
  struct TopologicalOrderVisitor : boost::dfs_visitor<> {
    TopologicalOrderVisitor(VISITOR& visitor, std::shared_ptr<Model::Impl> impl)
    : _impl(std::make_shared<TopologicalOrderVisitorImpl<VISITOR>>(visitor, std::move(impl))) {}

    template<class Vertex, class Graph>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void discover_vertex(Vertex v, Graph& g) {
      _impl->discoverVertex(v, g);
    }

    template<class Vertex, class Graph>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void finish_vertex(Vertex v, Graph& g) {
      _impl->finishVertex(v, g);
    }

    bool shouldStopVisitingCurrentBranch() { return _impl->shouldStopVisitingCurrentBranch(); }

    std::vector<Model::Vertex>& stack() { return _impl->stack; }
    std::map<Model::Vertex, int>& distances() { return _impl->distances; }
    std::shared_ptr<TopologicalOrderVisitorImpl<VISITOR>> _impl;
  };

  void UserInputValidator::finalise() {
    std::unordered_set<std::shared_ptr<UserInputValidator>> downstreamValidators{};

    auto isAccessorValidated = [](auto& accessor) -> bool {
      return accessor.getDirection().dir == VariableDirection::feeding && accessor.getDirection().withReturn &&
          accessor.getModel().getTags().count(std::string(UserInputValidator::tagValidatedVariable)) > 0;
    };

    // Find out accessors with return
    for(auto& accessor : _module->getAccessorListRecursive()) {
      if(isAccessorValidated(accessor)) {
        _downstreamInvalidatingReturnChannels.emplace(accessor.getAppAccessorNoType().getId());
      }
    }

    _validationDepth = 1;
    auto scanModel = [&](auto proxy) -> bool {
      if constexpr(ChimeraTK::Model::isApplicationModule(proxy)) {
        auto list = proxy.getApplicationModule().getAccessorListRecursive();
        return !std::any_of(list.cbegin(), list.cend(), isAccessorValidated);
      }

      // Not an application module. Continuing anyway
      // TODO: Really or is not having an application module also a reason to stop?
      return false;
    };

    std::map<Model::Vertex, boost::default_color_type> colors;
    boost::associative_property_map color_map(colors);

    // TODO: Apply filter for no outgoing validated nodes here already
    // Then we can use the DFS search
    auto filteredGraph = _module->getModel()._d->impl->getFilteredGraph(
        Model::keepPvAccess, Model::keepApplicationModules, Model::keepProcessVariables);
    auto visitor = TopologicalOrderVisitor(scanModel, _module->getModel()._d->impl);

    boost::depth_first_visit(filteredGraph, _module->getModel()._d->vertex, visitor, color_map,
        [&]([[maybe_unused]] auto a, [[maybe_unused]] auto b) -> bool {
          return visitor.shouldStopVisitingCurrentBranch();
        });

    std::cout << "Iterating stack, starting from " << _module->getName() << std::endl;

    auto& distances = visitor.distances();

    distances[_module->getModel()._d->vertex] = 0;

    for(auto it = std::next(visitor.stack().cbegin()); it != visitor.stack().cend(); ++it) {
      std::cout << std::get<Model::VertexProperties::ApplicationModuleProperties>(filteredGraph[*it].p).name
                << std::endl;
      auto [start, end] = boost::out_edges(*it, filteredGraph);
      for(auto edgeIterator = start; edgeIterator != end; ++edgeIterator) {
        auto* vtx = target(*edgeIterator, filteredGraph);
        if(!filteredGraph[vtx].visitProxy(scanModel, vtx, _module->getModel()._d->impl)) continue;
        distances[vtx] = std::max(distances[vtx], distances[*it] + 1);
      }
    }

    _validationDepth = std::max_element(distances.begin(), distances.end(), [](auto& a, auto& b) -> bool {
      return a.second < b.second;
    })->second;

    std::cout << "Longest validation depth: " << _validationDepth << std::endl;

    for(auto& v : _variableMap) {
      v.second->setHistorySize(_validationDepth);
    }
  }

  /*********************************************************************************************************************/

  UserInputValidator::Validator::Validator(std::function<bool(void)> isValidTest, std::string initialErrorMessage)
  : isValidFunction(std::move(isValidTest)), errorMessage(std::move(initialErrorMessage)) {}

  /*********************************************************************************************************************/

} // namespace ChimeraTK
