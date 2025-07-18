// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "AccessorConcepts.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"
#include "UserInputValidator.h"
#include "VariableGroup.h"

#include <boost/range/join.hpp>

#include <ranges>

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
      using acc_type = ScalarAccessor<typename AccessorType::value_type>;
    };
    template<array_accessor AccessorType>
    struct AccessorTypeHelper<AccessorType> {
      using type = OneDRegisterAccessor<typename AccessorType::value_type>;
      using out_type = ArrayOutput<typename AccessorType::value_type>;
      using acc_type = ArrayAccessor<typename AccessorType::value_type>;
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
   * For convenience, it is recommended to use the type aliases ScalarFanIn, ArrayFanIn etc. instead of this class
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
     *
     * @param owner See normal accessors.
     * @param name See normal accessors.
     * @param unit See normal accessors.
     * @param description See normal accessors.
     * @param tags See normal accessors.
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

    /**
     *  Construct FanIn with additional inputs.
     *
     * @param owner See normal accessors.
     * @param name See normal accessors.
     * @param additionalNames Names (relative or absolute) of additional PVs feeding into the FanIn. These can be also
     *                        control system inputs. Note that each of the additional inputs can have only one feeder.
     *                        Automatic faning in only works with the PV name defined by the name parameter.
     * @param unit See normal accessors.
     * @param description See normal accessors.
     * @param tags See normal accessors.
     * @param aggregator See other constructor signature.
     */
    FanIn(VariableGroup* owner, std::string name, std::initializer_list<std::string> additionalNames, std::string unit,
        const std::string& description, AggregatorType aggregator, const std::unordered_set<std::string>& tags = {});

    FanIn(FanIn&& other) noexcept { *this = std::move(other); }
    FanIn& operator=(FanIn&& other) noexcept;
    FanIn(const FanIn& other) = delete;
    FanIn& operator=(const FanIn& other) = delete;
    FanIn() = default;

    /**
     * Return the internal input accessor for the given TransferElementID.
     */
    const AbstractorType& input(const TransferElementID& id) const { return _inputs.get(id); }

    /**
     * Check whether the given TransferElementID identifies an internal input.
     */
    bool hasInput(const TransferElementID& id) const { return _inputs.has(id); }

    /**
     * Return iterable range of all internal input accessors
     */
    auto inputs() const;

    /**
     * Return iterable range of all internal input accessors
     */
    auto inputs();

    void replace(FanIn&& other) { *this = std::move(other); }

   protected:
    // Change read/write functions etc. to protected, because they are not for the user. Using them might be
    // confusing since the FanIn is technically an output.
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

      Inputs(VariableGroup* owner, FanIn& output, std::string name, auto additionalNames, std::string unit,
          const std::string& description, FanIn::AggregatorType aggregator,
          const std::unordered_set<std::string>& tags = {});

      const AccessorType& get(const TransferElementID& id) const;

      bool has(const TransferElementID& id) const;

     protected:
      void postConstruct() override;
      void prepare() override;

      enum class UpdateType { POST_READ, ACCEPT, REJECT };

      void processUpdate(const TransferElementID& change, UpdateType type);

      std::string _name;
      std::string _unit;
      std::vector<std::string> _additionalNames;

      AggregatorType _aggregator;

      out_type* _output{nullptr};

      std::vector<AccessorType> _inputs;
      std::map<TransferElementID, AbstractorType> _abstractorMap;
      std::map<TransferElementID, AccessorType*> _accessorMap;
      TransferElementID _lastUpdate;
      bool _hasValidator{false};
      size_t _nInitialValuesValidated{0};

      friend class FanIn;

      /**
       * Helper decorator which keeps track of the last update received by the FanIn. This is needed because the
       * ReadAnyGroup is created by user code and we do not want to require the user code to pass us the last
       * changed TransferElementID.
       */
      template<user_type U>
      class TrackingDecorator : public NDRegisterAccessorDecorator<U>, public UserInputValidator::AccessorHook {
       public:
        TrackingDecorator(const boost::shared_ptr<ChimeraTK::NDRegisterAccessor<U>>& target, FanIn::Inputs& fanIn)
        : NDRegisterAccessorDecorator<U>(target), _fanIn(fanIn) {}

        void doPostRead(TransferType type, bool updateDataBuffer) override;

        void onReject() override;

        void onAccept() override;

        void onAddValidator(UserInputValidator&) override;

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

  template<user_type UserType>
  using ScalarFanInWB = FanIn<ScalarPushInputWB<UserType>>;

  template<user_type UserType>
  using ArrayFanInWB = FanIn<ArrayPushInputWB<UserType>>;

  static constexpr auto fanInKeepLastValue = [](auto id, const auto& map) { return map.at(id); };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<push_input AccessorType>
  FanIn<AccessorType>::FanIn(VariableGroup* owner, std::string name, std::string unit, const std::string& description,
      AggregatorType aggregator, const std::unordered_set<std::string>& tags)
  : detail::FanIn::AccessorTypeHelper<AccessorType>::out_type(owner, name, unit, description, tags),
    _inputs(owner, *this, name, std::span<std::string>{}, unit, description, aggregator, tags) {}

  /********************************************************************************************************************/

  template<push_input AccessorType>
  FanIn<AccessorType>::FanIn(VariableGroup* owner, std::string name, std::initializer_list<std::string> additionalNames,
      std::string unit, const std::string& description, AggregatorType aggregator,
      const std::unordered_set<std::string>& tags)
  : detail::FanIn::AccessorTypeHelper<AccessorType>::out_type(owner, name, unit, description, tags),
    _inputs(owner, *this, name, additionalNames, unit, description, aggregator, tags) {}

  /********************************************************************************************************************/

  template<push_input AccessorType>
  FanIn<AccessorType>& FanIn<AccessorType>::operator=(FanIn<AccessorType>&& other) noexcept {
    // we have to re-create the inputs VariableGroup so it is properly owned by the new owner
    _inputs = Inputs{static_cast<VariableGroup*>(other.getOwner()), *this, other._inputs._name,
        other._inputs._additionalNames, other._inputs._unit, "", other._inputs._aggregator};

    this->InversionOfControlAccessor<typename detail::FanIn::AccessorTypeHelper<AccessorType>::acc_type>::replace(
        std::move(other));
    return *this;
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  auto FanIn<AccessorType>::inputs() const {
    _inputs.prepare(); // make sure map is filled, noop if it already is
    return _inputs._accessorMap | std::views::values |
        std::views::transform([](const auto* p) -> const auto& { return *p; });
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  auto FanIn<AccessorType>::inputs() {
    _inputs.prepare(); // make sure map is filled, noop if it already is
    return _inputs._accessorMap | std::views::values | std::views::transform([](auto* p) -> auto& { return *p; });
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<push_input AccessorType>
  FanIn<AccessorType>::Inputs::Inputs(VariableGroup* owner, FanIn& output, std::string name, auto additionalNames,
      std::string unit, const std::string& description, AggregatorType aggregator,
      const std::unordered_set<std::string>& tags)
  : VariableGroup(owner, ".", description, tags), _name(std::move(name)), _unit(std::move(unit)),
    _additionalNames(additionalNames.begin(), additionalNames.end()), _aggregator(aggregator), _output(&output) {}

  /********************************************************************************************************************/

  template<push_input AccessorType>
  void FanIn<AccessorType>::Inputs::postConstruct() {
    assert(_output != nullptr);

    std::list<std::string> inputNames;

    auto nodes = _output->getModel().getNodes();
    size_t index{0};

    // add one input for each incoming connection
    for(auto& node : nodes) {
      if(node->getDirection().dir != VariableDirection::feeding) {
        continue;
      }
      if(*node == VariableNetworkNode(*_output)) {
        continue;
      }
      inputNames.emplace_back(node->getName() + "/__FanInNode_" + std::to_string(index) + "__");
      node->setMetaData(inputNames.back());
      ++index;
    }

    for(auto& name : boost::join(inputNames, _additionalNames)) {
      _inputs.emplace_back(this, name, "", "");
    }
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  void FanIn<AccessorType>::Inputs::prepare() {
    if(!_accessorMap.empty()) {
      // prepare() is also called by inputs() to make sure the map is already filled, e.g. when needed by the owning
      // module's prepare() function, which might be called first.
      return;
    }

    for(auto& input : _inputs) {
      auto deco = boost::make_shared<TrackingDecorator<typename AccessorType::value_type>>(input.getImpl(), *this);
      input.NDRegisterAccessorAbstractor<typename AccessorType::value_type>::replace(deco);

      _accessorMap[input.getId()] = &input;
      _abstractorMap[input.getId()].replace(input);
    }
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  const AccessorType& FanIn<AccessorType>::Inputs::get(const TransferElementID& id) const {
    if(_accessorMap.empty()) {
      throw ChimeraTK::logic_error("FanIn::get() called too early, prepare() has not yet been called.");
    }

    return *(_accessorMap.at(id));
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  bool FanIn<AccessorType>::Inputs::has(const TransferElementID& id) const {
    if(_accessorMap.empty()) {
      throw ChimeraTK::logic_error("FanIn::get() called too early, prepare() has not yet been called.");
    }

    return _accessorMap.contains(id);
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  void FanIn<AccessorType>::Inputs::processUpdate(const TransferElementID& change, UpdateType type) {
    assert(_output != nullptr);

    _lastUpdate = change;

    // Only send initial value once all inputs have seen their initial value, so we send out only one single initial
    // value with the aggregator having access to all initial values.
    if(_output->getVersionNumber() == VersionNumber{nullptr}) {
      for(const auto& [id, inp] : _abstractorMap) {
        if(inp.getVersionNumber() == VersionNumber{nullptr}) {
          return;
        }
      }
    }

    if(type != UpdateType::ACCEPT) {
      *_output = _aggregator(change, _abstractorMap);
    }

    if(_output->getVersionNumber() == VersionNumber{nullptr} && _hasValidator && type != UpdateType::POST_READ) {
      ++_nInitialValuesValidated;
      if(_nInitialValuesValidated < _inputs.size()) {
        return;
      }
    }

    // If a UserInputValidator is added, delay writing the output until after the validation took place (see the hook
    // functions onAccept() and onReject() of the TrackingDecorator).
    if(!_hasValidator || type != UpdateType::POST_READ) {
      _output->write();
    }
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<push_input AccessorType>
  template<user_type U>
  void FanIn<AccessorType>::Inputs::TrackingDecorator<U>::doPostRead(TransferType type, bool updateDataBuffer) {
    NDRegisterAccessorDecorator<U>::doPostRead(type, updateDataBuffer);
    if(updateDataBuffer) {
      _fanIn.processUpdate(this->getId(), UpdateType::POST_READ);
    }
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  template<user_type U>
  void FanIn<AccessorType>::Inputs::TrackingDecorator<U>::onReject() {
    assert(_fanIn._hasValidator);
    _fanIn.processUpdate(this->getId(), UpdateType::REJECT);
  }

  /********************************************************************************************************************/

  template<push_input AccessorType>
  template<user_type U>
  void FanIn<AccessorType>::Inputs::TrackingDecorator<U>::onAccept() {
    assert(_fanIn._hasValidator);
    _fanIn.processUpdate(this->getId(), UpdateType::ACCEPT);
  }
  /********************************************************************************************************************/

  template<push_input AccessorType>
  template<user_type U>
  void FanIn<AccessorType>::Inputs::TrackingDecorator<U>::onAddValidator(UserInputValidator&) {
    _fanIn._hasValidator = true;
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
