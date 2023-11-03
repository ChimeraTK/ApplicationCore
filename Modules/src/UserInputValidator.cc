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

    std::cout << _module->getName() << " Accepting from this " << std::endl;
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

  struct TopologicalOrderVisitorImpl {
    explicit TopologicalOrderVisitorImpl(std::shared_ptr<Model::Impl> impl) : _impl(std::move(impl)) {}

    // FIXME: Temporary, just to check where the empty list came from
    TopologicalOrderVisitorImpl(TopologicalOrderVisitorImpl&& Other) = delete;
    TopologicalOrderVisitorImpl(TopologicalOrderVisitorImpl& Other) = delete;
    TopologicalOrderVisitorImpl& operator=(TopologicalOrderVisitorImpl& other) = delete;

    template<class Vertex, class Graph>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void finishVertex(Vertex v, Graph& g) {
      if(g[v].type == Model::VertexProperties::Type::applicationModule) {
        stack.push_front(v);
        distances.try_emplace(v, std::numeric_limits<int>::min());
      }
    }

    std::shared_ptr<Model::Impl> _impl;
    std::deque<Model::Vertex> stack{};
    std::map<Model::Vertex, int> distances{};
  };

  struct TopologicalOrderVisitor : boost::dfs_visitor<> {
    explicit TopologicalOrderVisitor(std::shared_ptr<Model::Impl> impl)
    : _impl(std::make_shared<TopologicalOrderVisitorImpl>(std::move(impl))) {}

#if 0
    template<class Vertex, class Graph>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void discover_vertex(Vertex v, Graph& g) {
      _impl->discoverVertex(v, g);
    }
#endif

    template<class Vertex, class Graph>
    // NOLINTNEXTLINE(readability-identifier-naming)
    void finish_vertex(Vertex v, Graph& g) {
      _impl->finishVertex(v, g);
    }

    std::deque<Model::Vertex>& stack() { return _impl->stack; }
    std::map<Model::Vertex, int>& distances() { return _impl->distances; }
    std::shared_ptr<TopologicalOrderVisitorImpl> _impl;
  };

  void UserInputValidator::finalise() {
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

    // Find longest path that is validated in our model, starting at this module
    // Making a DFS, using a color map so that we do not walk in cylces.

    std::map<Model::Vertex, boost::default_color_type> colors;
    boost::associative_property_map color_map(colors);

    auto extendedVertexFilter = Model::EdgeFilter([](const Model::EdgeProperties& edge) -> bool {
      return edge.type == Model::EdgeProperties::Type::pvAccess && edge.pvAccessWithReturnChannel;
    });

    // Filter that keeps our tagged variable nodes and application modules
    auto extendedTagFilter = Model::VertexFilter([](const Model::VertexProperties& vertex) -> bool {
      return vertex.visit([](auto props) -> bool {
        if constexpr(Model::isVariable(props)) {
          return props.tags.count(std::string(UserInputValidator::tagValidatedVariable)) > 0;
        }

        if constexpr(Model::isApplicationModule(props)) {
          return true;
        }

        return false;
      });
    });

    auto filteredGraph = _module->getModel()._d->impl->getFilteredGraph(extendedVertexFilter, extendedTagFilter);
    auto visitor = TopologicalOrderVisitor(_module->getModel()._d->impl);
    auto* ourVertex = _module->getModel()._d->vertex;

    boost::depth_first_visit(filteredGraph, ourVertex, visitor, color_map);

    auto& distances = visitor.distances();

    distances[ourVertex] = 0;

    for(auto* stackEntry : visitor.stack()) {
      auto [start, end] = boost::out_edges(stackEntry, filteredGraph);
      std::set<Model::Vertex> downstream{};
      for(auto edgeIterator = start; edgeIterator != end; ++edgeIterator) {
        auto* vtx = target(*edgeIterator, filteredGraph);
        auto [s, e] = boost::out_edges(vtx, filteredGraph);
        for(auto ei = s; ei != e; ++ei) {
          downstream.insert(target(*ei, filteredGraph));
        }
      }

      for(auto* vtx : downstream) {
        assert(filteredGraph[vtx].type == Model::VertexProperties::Type::applicationModule);

        auto downstreamName = std::get<Model::VertexProperties::ApplicationModuleProperties>(filteredGraph[vtx].p).name;
        distances[vtx] = std::max(distances[vtx], distances[stackEntry] + 1);
      }
    }

    _validationDepth = 1 + std::max_element(distances.begin(), distances.end(), [](auto& a, auto& b) -> bool {
      return a.second < b.second;
    })->second;

    for(auto& v : _variableMap) {
      v.second->setHistorySize(_validationDepth);
    }
  }

  /*********************************************************************************************************************/

  UserInputValidator::Validator::Validator(std::function<bool(void)> isValidTest, std::string initialErrorMessage)
  : isValidFunction(std::move(isValidTest)), errorMessage(std::move(initialErrorMessage)) {}

  /*********************************************************************************************************************/

} // namespace ChimeraTK
