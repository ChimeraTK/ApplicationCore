// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ArrayAccessor.h"
#include "ScalarAccessor.h"

#include <boost/circular_buffer.hpp>
#include <boost/fusion/container.hpp>

#include <iostream>

namespace ChimeraTK {

  // Forward-declarations
  class Module;

  /**
   * Class to realise the validation of user input values.
   *
   * User input values will be checked to fulfill certain conditions upon change. If the conditions are not met, the
   * change is rejected and an error function is called e.g. to report the error to the user.
   *
   * Note, this class is not a module. Instantiate it as a member of any ApplicationModule which needs to perform
   * validation of its inputs, or at the beginning of its mainLoop() function.
   *
   * Also note that as of now only scalar inputs can be validated.
   *
   * Inputs to validate can be added through the add() function. To ensure consistency between the value used by the
   * ApplicationModule and the value visible on the control system side, the input should be of the type
   * ChimeraTK::ScalarPushInputWB. If this is not possible (e.g. the same input is used by multiple ApplicationModules),
   * a ChimeraTK::ScalarPushInput can be used instead and the value will not be changed back to the previous value when
   * being rejected.
   *
   * Fallback values can be specified for each input, which will be used if the validation of the initial values fails
   * already. If no fallback value is specified, an invalid initial value will be changed to the default-constructed
   * value (e.g. 0). Hence a fallback value must be specified if the default-constructed value is not in the range of
   * valid values - otherwise the ApplicationModule might be confronted with this invalid value at runtime.
   *
   * The validation of initial values can be triggered either by calling validateAll() or be calling validate() with
   * a default-constructed ChimeraTK::TransferElementID.
   *
   * Use setErrorFunction() to define a function which reports the error to the user.
   *
   * The class must be used together with a ReadAnyGroup. Each value change reported by the ReadAnyGroup should be
   * passed to the validate() function. This will trigger all relevant validations and ensure all (validated) inputs
   * have valid values when returning.
   *
   * A typical program flow of the mainLoop() looks like this:
   *
   * void MyModule::mainLoop() {
   *   ChimeraTK::UserInputValidator validator;
   *   validator.setErrorFunction([&](const std::string &message) { ... code to report error ...});
   *   validator.add("MyInput must be bigger than 0!", [&] { return myInput > 0; }, myInput);
   *   validator.setFallback(myInput, 1); // necessary, since 0 is not valid
   *
   *   ChimeraTK::TransferElementID change;
   *   auto rag = readAnyGroup();
   *   while(true) {
   *     validator.validate(change); // change is default constructed in first run -> validate all initial values
   *
   *     ... do some computations based on myInput which would fail for myInput <= 0 ...
   *
   *     change = rag.readAny();
   *   }
   * }
   *
   */
  struct UserInputValidator {
    explicit UserInputValidator(ApplicationModule* module) : _module(module) {}
    /**
     * Add new condition to validate the given accessors against.
     *
     * errorMessage is the string to be passed on to the error function (as set via setErrorFunction()) if the condition
     * is not met.
     *
     * isValidFunction is a functor object (typically a lambda) taking no arguments and returning a boolean value. It
     * must return true, if the set of values is valid, and false if the values are invalid. By using a lambda which
     * binds to the accessors by reference, the current accessor values can be directly accessed.
     *
     * The remaining arguments must be all accessors used in the condition. If accessors used in the expression are not
     * listed, the expression will not be evaluated when that accessor changes and hence invalid states may go
     * unnoticed.
     *
     * This function can be called an arbitrary number of times. Also the same accessors may be passed multiple times to
     * different calls of this function. That way the expressions written in the isValidFunction can be kept simple and
     * the provided error messages can be more specific. E.g. the two conditions A > 0 and A < B can be defined in two
     * separate calls to the add() function despite A being part of both conditions. If A changes, both conditions will
     * be checked, since A is specified in the list of accessors in both calls.
     *
     * This function does not yet evaluate anything. It merely stores all information for later use. When validate() is
     * called, all isValidFunctions matching the given change are checked. If any of the checked isValidFunctions
     * returns false, the variable passed to validate() is reverted to its previous value.
     */
    template<typename... ACCESSORTYPES>
    void add(
        const std::string& errorMessage, const std::function<bool(void)>& isValidFunction, ACCESSORTYPES&... accessors);

    /**
     * Provide fallback value for the given accessor. This value is used if the validation of the initial value fails,
     * since there is no previous value to revert to in that case.
     *
     * It is mandatory to call this function for all accessors whose value after construction (usually 0) is outside the
     * range of valid values, as otherwise a failed initial value validation reverts to the (invalid) value after
     * construction and hence the subsequent computations might fail.
     */
    template<typename UserType, template<typename> typename Accessor>
    void setFallback(Accessor<UserType>& accessor, UserType value);

    template<typename UserType, template<typename> typename Accessor>
    void setFallback(Accessor<UserType>& accessor, std::vector<UserType> value);

    /**
     * Define how to report error messages to the user. The first argument of the add() function is passed to the given
     * errorFunction when the corresponding validation condition is false. Typically this function will pass this string
     * on to some string output which will display the value to the user/operator.
     */
    void setErrorFunction(const std::function<void(const std::string&)>& errorFunction);

    /**
     * Execute all validations for the given change. The change argument normally is the return value of
     * ReadAnyGroup::readAny(), indicating that this variable has changed. All validation conditions provided through
     * the add() function are searched for this variable. If at least one of the matching isValidFunctions returns
     * false, the new value is considered invalid.
     *
     * The value of the accessor is then changed back to the last known value (resp. the fallback value if no previous
     * valid value exists). If the accessor has a writeback channel, this reverted value is written back. Finally, the
     * errorFunction provided through setErrorFunction() is called with the error string matching the first failed
     * validation condition to inform the user/operator.
     *
     * If change is a default-constructed ChimeraTK::TransferElementID, all validation conditions are evaluated and all
     * invalid values are corrected. This is equivalent to call validateAll(). This functionality is useful to trigger
     * the validation of initial values.
     */
    bool validate(const ChimeraTK::TransferElementID& change);

    /**
     * Evaluate all validation conditions and correct all invalid values. This is equivalent to call validate() with a
     * default-constructed ChimeraTK::TransferElementID. This function is useful to trigger the validation of initial
     * values.
     */
    bool validateAll();

    void finalise();

   protected:
    static constexpr std::string_view tagValidatedVariable{"__UserInputValidator"};

    // Helper function for internal book keeping of accessors (prevent unnecessary overwrite of map entry, which might
    // result in loss of fallback values).
    template<typename UserType, template<typename> typename Accessor>
    void addAccessorIfNeeded(Accessor<UserType>& accessor);

    // Type-independent base class representing a variable passed at least once to add() or setFallback().
    struct VariableBase {
      enum class RejectionType { downstream, self };
      virtual ~VariableBase() = default;
      virtual void reject(RejectionType type) = 0;
      virtual void accept() = 0;
      virtual void setHistorySize(size_t size) = 0;
    };

    // Type-dependent representation of all known variables.
    template<typename UserType, template<typename> typename Accessor>
    struct Variable : VariableBase {
      explicit Variable(Accessor<UserType>& accessor);
      Variable() = delete;
      ~Variable() override = default;

      // called when validation function returned false
      void reject(RejectionType type) override;

      // called when validation function returned true
      void accept() override;

      void setHistorySize(size_t) override;

      // value to revert to if reject() is called. Updated through accept().
      boost::circular_buffer<std::vector<UserType>> lastAcceptedValue{};
      std::vector<UserType> fallbackValue{UserType()};

      // Reference to the accessor.
      Accessor<UserType>& accessor;

      std::size_t historyLength{1};
    };

    // Represents a validation condition
    struct Validator {
      explicit Validator(std::function<bool(void)> isValidTest, std::string errorMessage);
      Validator() = delete;
      Validator(const Validator& other) = default;

      std::function<bool(void)> isValidFunction;
      std::string errorMessage;
    };

    // List of Validator objects
    std::list<Validator> _validators; // must not use std::vector as resizing it invalidates pointers to objects

    // Map to find Variable object for given TransferElementID
    std::map<ChimeraTK::TransferElementID, std::shared_ptr<VariableBase>> _variableMap;

    // Map to find all Validators associated with the given TransferElementID
    std::map<ChimeraTK::TransferElementID, std::vector<Validator*>> _validatorMap;

    // Function to be called for reporting validation errors
    std::function<void(const std::string&)> _errorFunction{[](const std::string& m) { std::cerr << m << std::endl; }};

    ApplicationModule* _module{nullptr};

    std::unordered_set<ChimeraTK::TransferElementID> _downstreamInvalidatingReturnChannels{};
    size_t _validationDepth{0};
  };

  /*********************************************************************************************************************/

  template<typename... ACCESSORTYPES>
  void UserInputValidator::add(
      const std::string& errorMessage, const std::function<bool(void)>& isValidFunction, ACCESSORTYPES&... accessors) {
    boost::fusion::list<ACCESSORTYPES&...> accessorList{accessors...};
    static_assert(boost::fusion::size(accessorList) > 0, "Must specify at least one accessor!");
    assert(isValidFunction != nullptr);

    // create validator and store in list
    _validators.emplace_back(isValidFunction, errorMessage);

    // create map of accessors to validators, also add accessors/variables to list
    boost::fusion::for_each(accessorList, [&](auto& accessor) {
      addAccessorIfNeeded(accessor);
      _validatorMap[accessor.getId()].push_back(&_validators.back());
    });
  }

  /*********************************************************************************************************************/

  template<typename UserType, template<typename> typename Accessor>
  void UserInputValidator::setFallback(Accessor<UserType>& accessor, UserType value) {
    addAccessorIfNeeded(accessor);
    auto pv = std::dynamic_pointer_cast<Variable<UserType, Accessor>>(_variableMap.at(accessor.getId()));
    assert(pv != nullptr);
    if(pv->fallbackValue.size() != 1) {
      throw ChimeraTK::logic_error(
          "UserInputValidator::setFallback() with scalar value called for array-typed accessor '" + accessor.getName() +
          "'.");
    }
    pv->fallbackValue[0] = value;
  }

  /*********************************************************************************************************************/

  template<typename UserType, template<typename> typename Accessor>
  void UserInputValidator::setFallback(Accessor<UserType>& accessor, std::vector<UserType> value) {
    addAccessorIfNeeded(accessor);
    auto pv = std::dynamic_pointer_cast<Variable<UserType, Accessor>>(_variableMap.at(accessor.getId()));
    assert(pv != nullptr);
    if(pv->fallbackValue.size() != value.size()) {
      throw ChimeraTK::logic_error(
          "UserInputValidator::setFallback() with called with mismatching array length for accessor '" +
          accessor.getName() + "'.");
    }
    pv->fallbackValue = value;
  }
  /*********************************************************************************************************************/

  template<typename UserType, template<typename> typename Accessor>
  void UserInputValidator::addAccessorIfNeeded(Accessor<UserType>& accessor) {
    if(!_variableMap.count(accessor.getId())) {
      accessor.addTag(std::string(tagValidatedVariable));
      _variableMap[accessor.getId()] = std::make_shared<Variable<UserType, Accessor>>(accessor);
    }
  }
  /*********************************************************************************************************************/

  template<typename UserType, template<typename> typename Accessor>
  UserInputValidator::Variable<UserType, Accessor>::Variable(Accessor<UserType>& validatedAaccessor)
  : accessor(validatedAaccessor) {
    static_assert(std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInputWB<UserType>>::value ||
            std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInput<UserType>>::value ||
            std::is_same<Accessor<UserType>, ChimeraTK::ArrayPushInputWB<UserType>>::value ||
            std::is_same<Accessor<UserType>, ChimeraTK::ArrayPushInput<UserType>>::value,
        "UserInputValidator can only be used with push-type inputs.");
    if constexpr(std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInputWB<UserType>>::value ||
        std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInput<UserType>>::value) {
      fallbackValue.resize(1);
    }
    else {
      fallbackValue.resize(accessor.getNElements());
    }
  }

  /*********************************************************************************************************************/

  template<typename UserType, template<typename> typename Accessor>
  void UserInputValidator::Variable<UserType, Accessor>::reject(RejectionType type) {
    if(type == RejectionType::downstream && !lastAcceptedValue.empty()) {
      lastAcceptedValue.pop_back();
    }
    if constexpr(std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInput<UserType>>::value ||
        std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInputWB<UserType>>::value) {
      if(lastAcceptedValue.empty()) {
        accessor = fallbackValue[0];
      }
      else {
        accessor = lastAcceptedValue.back()[0];
      }
    }
    else {
      if(lastAcceptedValue.empty()) {
        accessor = fallbackValue;
      }
      else {
        accessor = lastAcceptedValue.back();
      }
    }

    if constexpr(std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInputWB<UserType>>::value ||
        std::is_same<Accessor<UserType>, ChimeraTK::ArrayPushInputWB<UserType>>::value) {
      accessor.write();
    }
  }

  /*********************************************************************************************************************/
  template<typename UserType, template<typename> typename Accessor>
  void UserInputValidator::Variable<UserType, Accessor>::accept() {
    if constexpr(std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInput<UserType>>::value ||
        std::is_same<Accessor<UserType>, ChimeraTK::ScalarPushInputWB<UserType>>::value) {
      auto savedValue = std::vector<UserType>(1);
      savedValue[0] = accessor;
      lastAcceptedValue.push_back(savedValue);
    }
    else {
      auto savedValue = std::vector<UserType>(accessor.getNElements());
      savedValue = accessor;
      lastAcceptedValue.push_back(savedValue);
    }
  }
  /*********************************************************************************************************************/

  template<typename UserType, template<typename> typename Accessor>
  void UserInputValidator::Variable<UserType, Accessor>::setHistorySize(std::size_t size) {
    historyLength = 3 * size;
    lastAcceptedValue.set_capacity(historyLength);
  }

  /*********************************************************************************************************************/

} // namespace ChimeraTK
