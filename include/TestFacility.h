// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"

#include <ChimeraTK/ControlSystemAdapter/ControlSystemPVManager.h>
#include <ChimeraTK/OneDRegisterAccessor.h>
#include <ChimeraTK/ScalarRegisterAccessor.h>
#include <ChimeraTK/VariantUserTypes.h>
#include <ChimeraTK/VoidRegisterAccessor.h>

#include <boost/fusion/include/at_key.hpp>

namespace ChimeraTK {

  /********************************************************************************************************************/

  namespace detail {

    template<typename T>
    struct BoolTypeHelper {
      using type = T;
    };

    template<>
    struct BoolTypeHelper<bool> {
      using type = ChimeraTK::Boolean;
    };

  } // namespace detail

  /********************************************************************************************************************/

  /** Helper class to facilitate tests of applications based on ApplicationCore */
  class TestFacility {
   public:
    /** The passed application will
     *  automatically be put into the testable mode and initialised. */
    explicit TestFacility(Application& app, bool enableTestableMode = true);

    /** Start the application in testable mode. */
    void runApplication() const;

    /**
     * Check whether data has been sent to the application so stepApplication() can be called.
     */
    bool canStepApplication() const;

    /** Perform a "step" of the application. This runs the application until all
     * input provided to it has been processed and all application modules wait
     * for new data in blocking read calls. This function returns only after the
     * application has reached that stated and was paused again. After returning
     * from this function, the result can be checked and new data can be provided
     * to the application. The new data will not be
     *  processed until the next call to step(). */
    void stepApplication(bool waitForDeviceInitialisation = true) const;

    /** Obtain a void process variable from the application, which is published
     * to the control system. */
    ChimeraTK::VoidRegisterAccessor getVoid(const ChimeraTK::RegisterPath& name) const;

    /** Obtain a scalar process variable from the application, which is published
     * to the control system. */
    template<typename T>
    ChimeraTK::ScalarRegisterAccessor<T> getScalar(const ChimeraTK::RegisterPath& name) const;

    /** Obtain an array-type process variable from the application, which is
     * published to the control system. */
    template<typename T>
    ChimeraTK::OneDRegisterAccessor<T> getArray(const ChimeraTK::RegisterPath& name) const;

    /** Convenience function to write a scalar process variable in a single call
     */
    template<typename TYPE>
    void writeScalar(const std::string& name, TYPE value);

    /** Convenience function to write an array process variable in a single call
     */
    template<typename TYPE>
    void writeArray(const std::string& name, const std::vector<TYPE>& value);

    /** Convenience function to read the latest value of a scalar process variable
     * in a single call */
    template<typename TYPE>
    TYPE readScalar(const std::string& name);

    /** Convenience function to read the latest value of an array process variable
     * in a single call */
    template<typename TYPE>
    std::vector<TYPE> readArray(const std::string& name);

    /** Set default value for scalar process variable. */
    template<typename T>
    void setScalarDefault(const ChimeraTK::RegisterPath& name, const T& value);

    /** Set default value for array process variable. */
    template<typename T>
    void setArrayDefault(const ChimeraTK::RegisterPath& name, const std::vector<T>& value);

    /** Function to obtain a process variable from the control system. For conveniently creating arrays and scalars,
     see getArray, getScalar and getVoid */
    template<typename T>
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<T>> getAccessor(const ChimeraTK::RegisterPath& name) const;

    /** */
    [[nodiscard]] boost::shared_ptr<ControlSystemPVManager> getPvManager() const { return _pvManager; }

    /**
     * Set ConfigReader scalar variable for the next instantiated Applicaton
     */
    template<typename T>
    static void setConfigScalar(const ChimeraTK::RegisterPath& name, const T& value);

    /**
     * Set ConfigReader array variable for the next instantiated Applicaton
     */
    template<typename T>
    static void setConfigArray(const ChimeraTK::RegisterPath& name, const std::vector<T>& value);

   protected:
    boost::shared_ptr<ControlSystemPVManager> _pvManager;

    // Cache (possible decorated) accessors to avoid the need to create accessors multiple times. This would not work
    // if the accessor is decorated, since the buffer would be lost and thus the current value could no longer be
    // obtained. This has to be done in dependence of the user type. Since this is a cache and does not change the
    // logical behaviour of the class, the maps are defined mutable.
    template<typename UserType>
    using AccessorMap = std::map<std::string, boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>;
    mutable ChimeraTK::TemplateUserTypeMap<AccessorMap> _accessorMap;

    // default values for process variables
    template<typename UserType>
    using Defaults = std::map<std::string, std::vector<UserType>>;
    ChimeraTK::TemplateUserTypeMap<Defaults> _defaults;

    Application& _app;

    // static storage of values set via setConfigScalar
    static std::map<ChimeraTK::RegisterPath, ChimeraTK::UserTypeVariantNoVoid> _configScalars;

    template<typename UserType>
    using Vector = std::vector<UserType>;

    // static storage of values set via setConfigArray
    static std::map<ChimeraTK::RegisterPath, ChimeraTK::UserTypeTemplateVariantNoVoid<Vector>> _configArrays;

    friend class ConfigReader;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename T>
  ChimeraTK::ScalarRegisterAccessor<T> TestFacility::getScalar(const ChimeraTK::RegisterPath& name) const {
    return getAccessor<T>(name);
  }

  /********************************************************************************************************************/

  template<typename T>
  ChimeraTK::OneDRegisterAccessor<T> TestFacility::getArray(const ChimeraTK::RegisterPath& name) const {
    return getAccessor<T>(name);
  }

  /********************************************************************************************************************/

  template<typename TYPE>
  void TestFacility::writeScalar(const std::string& name, const TYPE value) {
    auto acc = getScalar<typename detail::BoolTypeHelper<TYPE>::type>(name);
    acc = value;
    acc.write();
  }

  /********************************************************************************************************************/

  template<typename TYPE>
  void TestFacility::writeArray(const std::string& name, const std::vector<TYPE>& value) {
    auto acc = getArray<typename detail::BoolTypeHelper<TYPE>::type>(name);
    if constexpr(!std::is_same<TYPE, bool>::value) {
      acc = value;
    }
    else {
      assert(value.size() == acc.getNElements());
      std::transform(value.begin(), value.end(), acc.begin(), [](const bool& v) -> ChimeraTK::Boolean { return v; });
    }
    acc.write();
  }

  /********************************************************************************************************************/

  template<typename TYPE>
  TYPE TestFacility::readScalar(const std::string& name) {
    auto acc = getScalar<typename detail::BoolTypeHelper<TYPE>::type>(name);
    acc.readLatest();
    return acc;
  }

  /********************************************************************************************************************/

  template<typename TYPE>
  std::vector<TYPE> TestFacility::readArray(const std::string& name) {
    auto acc = getArray<typename detail::BoolTypeHelper<TYPE>::type>(name);
    acc.readLatest();
    return acc;
  }

  /********************************************************************************************************************/

  template<typename T>
  void TestFacility::setScalarDefault(const ChimeraTK::RegisterPath& name, const T& value) {
    if(_app._testFacilityRunApplicationCalled) {
      throw ChimeraTK::logic_error("TestFacility::setScalarDefault() called after runApplication().");
    }
    std::vector<T> vv;
    vv.push_back(value);
    setArrayDefault(name, vv);
  }

  /********************************************************************************************************************/

  template<typename T>
  void TestFacility::setArrayDefault(const ChimeraTK::RegisterPath& name, const std::vector<T>& value) {
    if(_app._testFacilityRunApplicationCalled) {
      throw ChimeraTK::logic_error("TestFacility::setArrayDefault() called after runApplication().");
    }
    // check if PV exists
    if(!_pvManager->hasProcessVariable(name)) {
      throw ChimeraTK::logic_error("Process variable '" + name + "' does not exist.");
    }

    // check if the type is right
    auto pv = _pvManager->getProcessArray<typename detail::BoolTypeHelper<T>::type>(name);
    if(pv == nullptr) {
      auto pvUntyped = _pvManager->getProcessVariable(name);
      throw ChimeraTK::logic_error("Process variable '" + name + "' requested by the wrong type: " + typeid(T).name() +
          " != " + pvUntyped->getValueType().name());
    }

    // store default value in map
    auto& tv = boost::fusion::at_key<typename detail::BoolTypeHelper<T>::type>(_defaults.table)[name];
    if constexpr(!std::is_same<T, bool>::value) {
      tv = value;
    }
    else {
      tv.resize(value.size());
      std::transform(value.begin(), value.end(), tv.begin(), [](const bool& v) -> ChimeraTK::Boolean { return v; });
    }
  }

  /********************************************************************************************************************/

  template<typename T>
  boost::shared_ptr<ChimeraTK::NDRegisterAccessor<T>> TestFacility::getAccessor(
      const ChimeraTK::RegisterPath& name) const {
    // check for existing accessor in cache
    if(boost::fusion::at_key<T>(_accessorMap.table).count(name) > 0) {
      return boost::fusion::at_key<T>(_accessorMap.table)[name];
    }

    // obtain accessor from ControlSystemPVManager
    auto pv = _pvManager->getProcessArray<T>(name);
    if(pv == nullptr) {
      throw ChimeraTK::logic_error("Process variable '" + name + "' does not exist.");
    }

    // decorate with TestableModeAccessorDecorator if variable is sender and
    // receiver is not poll-type (then no entry in pvIdMapp exists), and store it in cache
    if(auto it = _app._pvIdMap.find(pv->getUniqueId()); pv->isWriteable() && it != _app._pvIdMap.end()) {
      boost::fusion::at_key<T>(_accessorMap.table)[name] = _app.getTestableMode().decorate<T>(
          pv, detail::TestableMode::DecoratorType::WRITE, "ControlSystem:" + name, it->second);
    }
    else {
      boost::fusion::at_key<T>(_accessorMap.table)[name] = pv;
    }

    // return the accessor as stored in the cache
    return boost::fusion::at_key<T>(_accessorMap.table)[name];
  }

  /********************************************************************************************************************/

  template<typename T>
  void TestFacility::setConfigScalar(const ChimeraTK::RegisterPath& name, const T& value) {
    _configScalars[name] = value;
  }

  /********************************************************************************************************************/

  template<typename T>
  void TestFacility::setConfigArray(const ChimeraTK::RegisterPath& name, const std::vector<T>& value) {
    _configArrays[name] = value;
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
