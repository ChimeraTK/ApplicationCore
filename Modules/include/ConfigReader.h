// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*!
 * \page configreader ConfigReader Module
 *
 * This Module provides the following features:
 * - read values from an xml config file to have them available at server initialization,
 * - expose above values as process variables; these may connect to other ApplicationCore modules if needed.
 *
 * \section usage Example usage
 * - A server application using the config reader may look like:
 *  \code
 *  namespace ctk = ChimeratK
 *
 *  struct Server : public ctk::Application {
 *    Server() : Application("testserver") {}
 *    ~Server() { shutdown(); }
 *
 *    ctk::ConfigReader config{this, "config", "validConfig.xml", {"MyTAG"}};
 *    TestModule testModule{this, "TestModule", "The test module"};
 *
 *    void Server::defineConnections() override;
 *
 *  };
 *  \endcode
 *
 * - Values from validConfig.xml can be accessed at server startup:
 * \code
 * Server::Server() {
 *  auto config_var = config.get<int8_t>("module1/var8");
 *  auto config_arr = config.get<std::vector<int8>>("module1/submodule/intArray");
 *  // ...
 * }
 * \endcode
 *
 * Configuration will be published as process variables, according to the hierarchy constructed in the configuration
 * file.
 *
 * \section xmlstructure XML file structure
 * - A valid configuration file may look like:
 *   \verbatim
     <configuration>
       <variable name="var8" type="int8" value="-123"/>
       <module name="module1">
         <variable name="var8" type="int8" value="-123"/>
         <module name="submodule">
             <variable name="intArray" type="int32">
             <value i="0" v="10"/>
             <value i="1" v="9"/>
             <value i="2" v="8"/>
             <value i="7" v="3"/>
             <value i="8" v="2"/>
             <value i="9" v="1"/>
             <value i="3" v="7"/>
             <value i="4" v="6"/>
             <value i="5" v="5"/>
             <value i="6" v="4"/>
          </variable>
         </module>
       </module>
     </configuration>
     \endverbatim
 *
 * */

#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ScalarAccessor.h"

#include <ChimeraTK/SupportedUserTypes.h>

#include <map>
#include <unordered_map>
#include <utility>

namespace ChimeraTK {

  struct FunctorFill;
  struct ArrayFunctorFill;
  struct FunctorSetValues;
  struct FunctorSetValuesArray;
  class ModuleTree;

  /** Generic module to read an XML config file and provide the defined values as
   *  constant variables. The config file should look like this:
   *  \code{.xml}
  <configuration>
  <variable name="variableName" type="int32" value="42"/>
  <variable name="anotherVariable" type="string" value="Hello world!"/>
  <variable name="someArray" type="string">
    <value i="0" v="StringEntry1" />
    <value i="1" v="StringEntry2" />
    <value i="2" v="StringEntry3" />
    <value i="3" v="StringEntry4" />
    <value i="4" v="StringEntry5" />
    <value i="5" v="StringEntry6" />
  </variable>
  </configuration>
      \endcode
   *
   *  Outputs are created for each variable, so they can be connected to other
  modules. All values will be provided to
   *  the receivers already in the preparation phase, so no read() must be called.
  Updates will never be sent, so any
   *  blocking read operation on the receivers will block forever.
   *
   *  Configuration values can already be accessed during the
  Application::defineConnections() function by using the
   *  ConfigReader::get() function.
   */
  class ConfigReader : public ApplicationModule {
   public:
    ConfigReader(ModuleGroup* owner, const std::string& name, const std::string& fileName,
        const std::unordered_set<std::string>& tags = {});
    ~ConfigReader() override;

    void mainLoop() override {}
    void prepare() override;

    /**
     *  Get value for given configuration variable. This is already accessible right after construction of this object.
     *  Throws ChimeraTK::logic_error if variable doesn't exist. To obtain the value of an array, use an std::vector<T>
     *  as template argument.
     */
    template<typename T>
    const T& get(std::string variableName) const;

    /**
     *  Version of get() which does not throw if the variable does not exist and instead returns the given default
     *  value.
     */
    template<typename T>
    const T& get(std::string variableName, const T& defaultValue) const;

    /**
     * Returns a list of names of modules which are direct children of path.
     * If path does not exist, will return an empty list.
     */
    std::list<std::string> getModules(const std::string& path = {}) const;

   protected:
    /** Helper function to avoid code duplication in constructors **/
    void construct(const std::string& fileName);

    /** File name */
    std::string _fileName;

    /** List to hold VariableNodes corresponding to xml modules **/
    std::unique_ptr<ModuleTree> _moduleTree;

    /** throw a parsing error with more information */
    void parsingError(const std::string& message);

    /** Class holding the value and the accessor for one configuration variable */
    template<typename T>
    struct Var {
      Var(Module* owner, const std::string& name, T theValue)
      : accessor(owner, name, "unknown", "Configuration variable"), value(std::move(theValue)) {}

      Var() = default;

      ScalarOutput<T> accessor;
      T value{};
    };

    /** Class holding the values and the accessor for one configuration array */
    template<typename T>
    struct Array {
      Array(Module* owner, const std::string& name, const std::vector<T>& theValue)
      : accessor(owner, name, "unknown", theValue.size(), "Configuration array"), value(theValue) {}

      Array() = default;

      ArrayOutput<T> accessor;
      std::vector<T> value;
    };

    /** Create an instance of Var<T> and place it on the variableMap */
    template<typename T>
    void createVar(const std::string& name, const std::string& value);

    /** Create an instance of Array<T> and place it on the arrayMap */
    template<typename T>
    void createArray(const std::string& name, const std::map<size_t, std::string>& values);

    /** Check if variable exists in the config and if type of var name in the config file matches the given type.
     * Throws logic_errors otherwise. */
    [[nodiscard]] bool checkVariable(std::string const& name, std::string const& type) const;

    /** Check if array exists in the config and if type of array name in the config file matches the given type.
     * Throws logic_errors otherwise. */
    [[nodiscard]] bool checkArray(std::string const& name, std::string const& type) const;

    /** Define type for map of std::string to Var, so we can put it into the
     * TemplateUserTypeMap */
    template<typename T>
    using MapOfVar = std::unordered_map<std::string, Var<T>>;

    /** Type-depending map of vectors of variables */
    ChimeraTK::TemplateUserTypeMapNoVoid<MapOfVar> _variableMap;

    /** Define type for map of std::string to Array, so we can put it into the
     * TemplateUserTypeMap */
    template<typename T>
    using MapOfArray = std::unordered_map<std::string, Array<T>>;

    /** Type-depending map of vectors of arrays */
    ChimeraTK::TemplateUserTypeMapNoVoid<MapOfArray> _arrayMap;

    /** Map assigning string type identifyers to C++ types */
    ChimeraTK::SingleTypeUserTypeMapNoVoid<const char*> _typeMap{"int8", "uint8", "int16", "uint16", "int32", "uint32",
        "int64", "uint64", "float", "double", "string", "boolean"};

    /**
     * Implementation of get() which can be overloaded for scalars and vectors. The second argument is a dummy only to
     * distinguish the two overloaded functions. The return type is an std::optional. In case the variable does not
     * exist, no value is returned. If the variable exists but has a different type, a ChimeraTK::logic_error is thrown.
     */
    template<typename T>
    std::optional<const T*> getImpl(const std::string& variableName, T*) const;

    template<typename T>
    std::optional<const std::vector<T>*> getImpl(const std::string& variableName, std::vector<T>*) const;

    friend struct FunctorFill;
    friend struct ArrayFunctorFill;
    friend struct FunctorSetValues;
    friend struct FunctorSetValuesArray;
    friend struct FunctorGetTypeForName;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename T>
  const T& ConfigReader::get(std::string variableName, const T& defaultValue) const {
    if(variableName.starts_with("/")) {
      variableName = variableName.substr(1);
    }
    auto result = getImpl(variableName, static_cast<T*>(nullptr));
    return *(result.value_or(&defaultValue));
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename T>
  const T& ConfigReader::get(std::string variableName) const {
    if(variableName.starts_with("/")) {
      variableName = variableName.substr(1);
    }
    auto result = getImpl(variableName, static_cast<T*>(nullptr));
    if(!result.has_value()) {
      auto msg = "ConfigReader: Cannot find configuration variable of the name '" + variableName +
          "' in the config file '" + _fileName + "'.";
      std::cerr << msg << std::endl;
      throw(ChimeraTK::logic_error(msg));
    }
    return *(result.value());
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename T>
  std::optional<const T*> ConfigReader::getImpl(const std::string& variableName, T*) const {
    bool exists = checkVariable(variableName, boost::fusion::at_key<T>(_typeMap));
    if(!exists) {
      return {};
    }
    return &(boost::fusion::at_key<T>(_variableMap.table).at(variableName).value);
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename T>
  std::optional<const std::vector<T>*> ConfigReader::getImpl(const std::string& variableName, std::vector<T>*) const {
    bool exists = checkArray(variableName, boost::fusion::at_key<T>(_typeMap));
    if(!exists) {
      return {};
    }
    return &(boost::fusion::at_key<T>(_arrayMap.table).at(variableName).value);
  }

} // namespace ChimeraTK
