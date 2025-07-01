// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "AccessorConcepts.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"

namespace ChimeraTK {

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  namespace detail::FanIn {
    template<class AccessorType>
    struct AccessorTypeHelper {};

    template<scalar_accessor AccessorType>
    struct AccessorTypeHelper<AccessorType> {
      using type = ScalarRegisterAccessor<typename AccessorType::value_type>;
      using out_type = ScalarOutput<typename AccessorType::value_type>;
    };
    template<array_accessor AccessorType>
    struct AccessorTypeHelper<AccessorType> {
      using type = OneDRegisterAccessor<typename AccessorType::value_type>;
      using out_type = ArrayOutput<typename AccessorType::value_type>;
    };
  } // namespace detail::FanIn

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  /**
   * Special accessor allows multiple incoming connections to the same logical process variable.
   *
   * The FanIn is meant to be used with a ReadAnyGroup, hence its read functions are not available for the user. It will
   * create internally one input for each incoming connection and alter the name of that internal process variable into
   * something unique. The user must provide an aggergator function which decides how to map the incoming data onto a
   * single value. The single value will then be made available through an internal output to other ApplicationModules,
   * the control system and/or devices. It can also be accessed by the owing ApplicationModule code as if it were an
   * ordinary ScalarPushInput or ArrayPushInput.
   *
   * For convenience, it is recommended to use the ScalarFanIn resp. ArrayFanIn type aliases instead of this class
   * directly.
   */
  template<push_input AccessorType>
  class FanIn : public detail::FanIn::AccessorTypeHelper<AccessorType>::out_type {
   public:
    using AbstractorType = detail::FanIn::AccessorTypeHelper<AccessorType>::type;
    using value_type = typename AccessorType::value_type;
    using AggregatorType =
        std::function<value_type(TransferElementID, const std::map<TransferElementID, AbstractorType>&)>;
    using out_type = detail::FanIn::AccessorTypeHelper<AccessorType>::out_type;

    /**
     * Construct FanIn.

     * @param owner See normel accessors.
     * @param name See normel accessors.
     * @param unit See normel accessors.
     * @param description See normel accessors.
     * @param tags See normel accessors.
     * @param aggregator Functor which is called for each incoming change. It must accept two arguments:
     *                   - TransferElementID - id of the internal accessor receiving the change
     *                   - std::map<TransferElementID, AbstractorType>) - map of all internal input accessors
     *                   This functor can either just deal with the arriving value, or it can iterate all inputs and so
     *                   somthing more complex with them.
     *
     * If the user is just interested in the most recent value, regardles of its source, simply pass keepLastValue as an
     * aggregator.
     */
    FanIn(VariableGroup* owner, std::string name, std::string unit, const std::string& description,
        AggregatorType aggregator, const std::unordered_set<std::string>& tags = {});

    static constexpr auto keepLastValue = [](auto id, auto map) { return map[id]; };

   protected:
    // Change read/write functions etc. to protected, because they are not for the user. Using them might be
    // confusing since the FanIn is actually an output.
    using out_type::write;
    using out_type::writeDestructively;
    using out_type::writeIfDifferent;
    using out_type::setAndWrite;
    using out_type::read;
    using out_type::readNonBlocking;
    using out_type::readLatest;
    using out_type::isReadOnly;
    using out_type::isReadable;
    using out_type::isWriteable;

    class Inputs : public VariableGroup {
     public:
      using VariableGroup::VariableGroup;

      Inputs(VariableGroup* owner, FanIn& output, std::string name, std::string unit, const std::string& description,
          FanIn::AggregatorType aggregator, const std::unordered_set<std::string>& tags = {});

      const AbstractorType& get() const;

      const AbstractorType& get(const TransferElementID& id) const;

      bool has(const TransferElementID& id) const;

     protected:
      void postConstruct() override;
      void prepare() override;

      void processUpdate(const TransferElementID& change);

      std::vector<AccessorType> _inputs;
      std::map<TransferElementID, AbstractorType> _inputMap;

      std::string _name;
      std::string _unit;

      TransferElementID _lastUpdate;

      AggregatorType _aggregator;

      out_type& _output;

      /**
       * Helper decorator which keeps track of the last update received by the FanIn. This is needed because the
       * ReadAnyGroup is created by user code and we do not want to require the user code to pass us the last changed
       * TransferElementID.
       */
      template<user_type U>
      class TrackingDecorator : public NDRegisterAccessorDecorator<U> {
       public:
        TrackingDecorator(const boost::shared_ptr<ChimeraTK::NDRegisterAccessor<U>>& target, FanIn::Inputs& fanIn)
        : NDRegisterAccessorDecorator<U>(target), _fanIn(fanIn) {}

        void doPostRead(TransferType type, bool updateDataBuffer) override;

       private:
        Inputs& _fanIn;
      };
    };

    Inputs _inputs;
  };

  /********************************************************************************************************************/

  template<user_type UserType>
  using ScalarFanIn = FanIn<ScalarPushInput<UserType>>;

  template<user_type UserType>
  using ArrayFanIn = FanIn<ArrayPushInput<UserType>>;

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<push_input AccessorType>
  FanIn<AccessorType>::FanIn(VariableGroup* owner, std::string name, std::string unit, const std::string& description,
      AggregatorType aggregator, const std::unordered_set<std::string>& tags)
  : detail::FanIn::AccessorTypeHelper<AccessorType>::out_type(owner, name, unit, description, tags),
    _inputs(owner, *this, name, unit, description, aggregator, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<push_input AccessorType>
  FanIn<AccessorType>::Inputs::Inputs(VariableGroup* owner, FanIn& output, std::string name, std::string unit,
      const std::string& description, AggregatorType aggregator, const std::unordered_set<std::string>& tags)
  : VariableGroup(owner, ".", description, tags), _name(std::move(name)), _unit(std::move(unit)),
    _aggregator(aggregator), _output(output) {
    // create 'pilot' input, to allow something to connect with us
    _inputs.emplace_back(this, _name, _unit, "");
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  void FanIn<AccessorType>::Inputs::postConstruct() {
    std::list<std::string> inputNames;

    auto nodes = _inputs.front().getModel().getNodes();
    size_t index{0};
    for(auto& node : nodes) {
      if(node->getDirection().dir != VariableDirection::feeding) {
        continue;
      }
      if(*node == VariableNetworkNode(_output)) {
        continue;
      }
      inputNames.emplace_back(node->getName() + "/__FanInNode_" + std::to_string(index) + "__");
      node->setMetaData(inputNames.back());
      ++index;
    }

    // remove the 'pilot' input - it's easier to create the actual inputs all the same way
    _inputs.clear();

    for(auto& name : inputNames) {
      _inputs.emplace_back(this, name, "", "");
    }
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  void FanIn<AccessorType>::Inputs::prepare() {
    for(auto& input : _inputs) {
      auto deco = boost::make_shared<TrackingDecorator<typename AccessorType::value_type>>(input.getImpl(), *this);
      input.NDRegisterAccessorAbstractor<typename AccessorType::value_type>::replace(deco);

      _inputMap[input.getId()].replace(input);
    }
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  const FanIn<AccessorType>::AbstractorType& FanIn<AccessorType>::Inputs::get() const {
    return get(_lastUpdate);
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  const FanIn<AccessorType>::AbstractorType& FanIn<AccessorType>::Inputs::get(const TransferElementID& id) const {
    if(_inputMap.empty()) {
      throw ChimeraTK::logic_error("FanIn::get() called too early, prepare() has not yet been called.");
    }

    return _inputMap.at(id);
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  bool FanIn<AccessorType>::Inputs::has(const TransferElementID& id) const {
    if(_inputMap.empty()) {
      throw ChimeraTK::logic_error("FanIn::get() called too early, prepare() has not yet been called.");
    }

    return _inputMap.contains(id);
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  void FanIn<AccessorType>::Inputs::processUpdate(const TransferElementID& change) {
    _lastUpdate = change;
    _output.setAndWrite(_aggregator(change, _inputMap));
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<push_input AccessorType>
  template<user_type U>
  void FanIn<AccessorType>::Inputs::TrackingDecorator<U>::doPostRead(TransferType type, bool updateDataBuffer) {
    NDRegisterAccessorDecorator<U>::doPostRead(type, updateDataBuffer);
    if(updateDataBuffer) {
      _fanIn.processUpdate(this->getId());
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
