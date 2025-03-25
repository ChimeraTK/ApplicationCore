// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ConfigReader.h"

#include "TestFacility.h"
#include "VariableGroup.h"

#include <libxml++/libxml++.h>

#include <filesystem>
#include <iostream>

namespace ChimeraTK {

  template<typename Element>
  static Element prefix(std::string s, Element e) {
    e.name = s + e.name;
    return e;
  }

  static std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& fileName);
  static std::string root(const std::string& flattened_name);
  static std::string branchWithoutRoot(const std::string& flattened_name);
  static std::string branch(const std::string& flattened_name);
  static std::string leaf(const std::string& flattened_name);

  struct Variable {
    std::string name;
    std::string type;
    std::string value;
  };

  struct Array {
    std::string name;
    std::string type;
    std::map<size_t, std::string> values;
  };

  using VariableList = std::vector<Variable>;
  using ArrayList = std::vector<Array>;

  class ConfigParser {
    std::string _fileName;
    std::unique_ptr<xmlpp::DomParser> _parser;
    std::unique_ptr<VariableList> _variableList;
    std::unique_ptr<ArrayList> _arrayList;

   public:
    explicit ConfigParser(const std::string& fileName) : _fileName(fileName), _parser(createDomParser(fileName)) {}

    std::unique_ptr<VariableList> getVariableList();
    std::unique_ptr<ArrayList> getArrayList();

   private:
    std::tuple<std::unique_ptr<VariableList>, std::unique_ptr<ArrayList>> parse();
    xmlpp::Element* getRootNode(xmlpp::DomParser& parser);
    [[noreturn]] void error(const std::string& message);
    bool isVariable(const xmlpp::Element* element);
    bool isArray(const xmlpp::Element* element);
    bool isModule(const xmlpp::Element* element);
    static Variable parseVariable(const xmlpp::Element* element);
    Array parseArray(const xmlpp::Element* element);
    void parseModule(const xmlpp::Element* element, std::string parent_name);

    void validateValueNode(const xmlpp::Element* valueElement);
    std::map<size_t, std::string> gettArrayValues(const xmlpp::Element* element);
  };

  class ModuleTree : public VariableGroup {
   public:
    using VariableGroup::VariableGroup;

    ChimeraTK::Module* lookup(const std::string& flattened_module_name);
    std::list<std::string> getChildList() { return _childrenInOrder; }

    // Prevent any modification of the ModuleTree by lookup(), which otherwise will do a lazy-create
    // Will be called on the top-level module tree once the parsing is done
    void seal();

   private:
    void addChildNode(const std::string& name) {
      if(_children.find(name) == _children.end()) {
        _children[name] = std::make_unique<ModuleTree>(this, name, "");
        _childrenInOrder.push_back(name);
      }
    }

    ChimeraTK::ModuleTree* get(const std::string& flattened_name);

    std::unordered_map<std::string, std::unique_ptr<ModuleTree>> _children;

    // Helper list to be able to return the child modules in the order they were found in the XML file
    std::list<std::string> _childrenInOrder;

    // Whether the ConfigReader will be modified by lookup() or not.
    bool _sealed{false};
  };

  /********************************************************************************************************************/

  /** Functor to fill variableMap */
  struct FunctorFill {
    FunctorFill(ConfigReader* theOwner, const std::string& theType, const std::string& theName,
        const std::string& theValue, bool& isProcessed)
    : owner(theOwner), type(theType), name(theName), value(theValue), processed(isProcessed) {
      processed = false;
    }

    template<typename PAIR>
    void operator()(PAIR&) const {
      // extract the user type from the pair
      using T = typename PAIR::first_type;

      // skip this type, if not matching the type string in the config file
      if(type != boost::fusion::at_key<T>(owner->_typeMap)) {
        return;
      }

      owner->createVar<T>(name, value);
      processed = true;
    }

    ConfigReader* owner;
    const std::string &type, &name, &value;
    bool& processed; // must be a non-const reference, since we want to return
                     // this to the caller
  };

  /********************************************************************************************************************/

  /** Functor to fill variableMap for arrays */
  struct ArrayFunctorFill {
    ArrayFunctorFill(ConfigReader* theOwner, const std::string& theType, const std::string& theName,
        const std::map<size_t, std::string>& theValues, bool& isProcessed)
    : owner(theOwner), type(theType), name(theName), values(theValues), processed(isProcessed) {
      processed = false;
    }

    template<typename PAIR>
    void operator()(PAIR&) const {
      // extract the user type from the pair
      using T = typename PAIR::first_type;

      // skip this type, if not matching the type string in the config file
      if(type != boost::fusion::at_key<T>(owner->_typeMap)) {
        return;
      }

      owner->createArray<T>(name, values);
      processed = true;
    }

    ConfigReader* owner;
    const std::string &type, &name;
    const std::map<size_t, std::string>& values;
    bool& processed; // must be a non-const reference, since we want to return
                     // this to the caller
  };

  /********************************************************************************************************************/

  struct FunctorGetTypeForName {
    FunctorGetTypeForName(const ConfigReader* theOwner, std::string const& theName, std::string& theType)
    : owner(theOwner), name(theName), type(theType) {}

    template<typename PAIR>
    bool operator()(PAIR const& pair) const {
      // extract the user type from the pair
      using T = typename PAIR::first_type;

      size_t numberOfMatches{pair.second.count(name)};
      assert(numberOfMatches <= 1);
      bool hasMatch{numberOfMatches == 1};

      if(hasMatch) {
        type = boost::fusion::at_key<T>(owner->_typeMap);
      }

      return hasMatch;
    }

    const ConfigReader* owner;
    const std::string& name;
    std::string& type;
  };

  /********************************************************************************************************************/

  void ConfigReader::checkVariable(std::string const& name, std::string const& typeOfThis) const {
    std::string typeOfVar;

    bool varExists = boost::fusion::any(_variableMap.table, FunctorGetTypeForName{this, name, typeOfVar});

    if(!varExists) {
      auto msg = "ConfigReader: Cannot find a scalar configuration variable of the name '" + name +
          "' in the config file '" + _fileName + "'.";
      std::cerr << msg << std::endl;
      throw(ChimeraTK::logic_error(msg));
    }

    if(typeOfVar != typeOfThis) {
      auto msg = "ConfigReader: Attempting to read scalar configuration variable '" + name + "' with type '" +
          typeOfThis + "'. This does not match type '" + typeOfVar + "' defined in the config file.";
      std::cerr << msg << std::endl;
      throw(ChimeraTK::logic_error(msg));
    }
  }

  /********************************************************************************************************************/

  void ConfigReader::checkArray(std::string const& name, std::string const& typeOfThis) const {
    std::string typeOfVar;

    bool varExists = boost::fusion::any(_arrayMap.table, FunctorGetTypeForName{this, name, typeOfVar});

    if(!varExists) {
      throw(ChimeraTK::logic_error("ConfigReader: Cannot find a array "
                                   "configuration variable of the name '" +
          name + "' in the config file '" + _fileName + "'."));
    }

    if(typeOfVar != typeOfThis) {
      throw(ChimeraTK::logic_error("ConfigReader: Attempting to read array configuration variable '" + name +
          "' with type '" + typeOfThis + "'. This does not match type '" + typeOfVar +
          "' defined in the config file."));
    }
  }

  /********************************************************************************************************************/

  template<typename T>
  void ConfigReader::createVar(const std::string& name, const std::string& value) {
    T convertedValue = ChimeraTK::userTypeToUserType<T>(value);

    auto moduleName = branch(name);
    auto varName = leaf(name);
    auto* varOwner = _moduleTree->lookup(moduleName);

    // place the variable onto the vector
    std::unordered_map<std::string, ConfigReader::Var<T>>& theMap = boost::fusion::at_key<T>(_variableMap.table);
    theMap.emplace(std::make_pair(name, ConfigReader::Var<T>(varOwner, varName, convertedValue)));
  }

  /********************************************************************************************************************/

  /********************************************************************************************************************/

  template<typename T>
  void ConfigReader::createArray(const std::string& name, const std::map<size_t, std::string>& values) {
    std::vector<T> Tvalues;

    size_t expectedIndex = 0;
    for(const auto& value : values) {
      // check index (std::map should be ordered by the index)
      if(value.first != expectedIndex) {
        parsingError("Array index " + std::to_string(expectedIndex) + " not found, but " + std::to_string(value.first) +
            " was. "
            "Sparse arrays are not supported!");
      }
      ++expectedIndex;

      // convert value into user type
      T convertedValue = ChimeraTK::userTypeToUserType<T>(value.second);

      // store value in vector
      Tvalues.push_back(convertedValue);
    }

    auto moduleName = branch(name);
    auto arrayName = leaf(name);
    auto* arrayOwner = _moduleTree->lookup(moduleName);

    // place the variable onto the vector
    std::unordered_map<std::string, ConfigReader::Array<T>>& theMap = boost::fusion::at_key<T>(_arrayMap.table);
    theMap.emplace(std::make_pair(name, ConfigReader::Array<T>(arrayOwner, arrayName, Tvalues)));
  }

  /********************************************************************************************************************/

  ConfigReader::ConfigReader(ModuleGroup* owner, const std::string& name, const std::string& fileName,
      const std::unordered_set<std::string>& tags)
  : ApplicationModule(owner, name, "Configuration read from file '" + fileName + "'", tags), _fileName(fileName),
    _moduleTree(std::make_unique<ModuleTree>(this, ".", "")) {
    auto* appConfig = Application::getInstance()._defaultConfigReader;

    bool replacingDefaultConfig = false;
    if(appConfig != nullptr) {
      // We have an appconfig (either the default one, or the first one after that to be created) and there is another
      // one we bail out because we do not know what to do. The default one will have disabled itself (which sets the
      // owner to nullptr)
      if(appConfig->getOwner() != nullptr) {
        throw ChimeraTK::logic_error("More than one explicit ConfigReader instances found. Unclear how to continue."
                                     " Please update your application.");
      }
      std::cout << "Using your own ConfigReader module is deprecated. Please use the Application built-in config reader"
                << " by naming your configuration file " << appConfig->_fileName << std::endl;
      replacingDefaultConfig = true;
      Application::getInstance()._defaultConfigReader = this;
    }

    bool doDisable = false;

    try {
      construct(fileName);
    }
    catch(ChimeraTK::runtime_error& ex) {
      if(replacingDefaultConfig) {
        // Re-throw error, backwards compatible
        throw ChimeraTK::logic_error(ex.what());
      }

      doDisable = true;

      std::cout << "Could not load configuration " << fileName << ", assuming no configuration wanted." << std::endl;
    }

    // Create scalar variables set via TestFacility
    for(auto& pairPathnameValue : TestFacility::_configScalars) {
      auto pathname = std::string(pairPathnameValue.first).substr(1); // strip leading slash from RegisterPath
      auto value = pairPathnameValue.second;
      doDisable = false;
      std::visit(
          [&](auto v) {
            using UserType = decltype(v);
            auto moduleName = branch(pathname);
            auto varName = leaf(pathname);
            auto* varOwner = _moduleTree->lookup(moduleName);
            auto& theMap = boost::fusion::at_key<UserType>(_variableMap.table);
            theMap[pathname] = ConfigReader::Var<UserType>(varOwner, varName, v);
          },
          value);
    }
    TestFacility::_configScalars.clear();

    // Create array variables set via TestFacility
    for(auto& pairPathnameValue : TestFacility::_configArrays) {
      auto pathname = std::string(pairPathnameValue.first).substr(1);
      auto& value = pairPathnameValue.second;
      doDisable = false;
      std::visit(
          [&](const auto& v) {
            using UserType = typename std::remove_reference<decltype(v)>::type::value_type;
            auto moduleName = branch(pathname);
            auto arrayName = leaf(pathname);
            auto* arrayOwner = _moduleTree->lookup(moduleName);
            auto& theMap = boost::fusion::at_key<UserType>(_arrayMap.table);
            theMap[pathname] = ConfigReader::Array<UserType>(arrayOwner, arrayName, v);
          },
          value);
    }

    if(doDisable) {
      disable();
    }
  }

  /********************************************************************************************************************/

  void ConfigReader::construct(const std::string& fileName) {
    auto fillVariableMap = [this](const Variable& var) {
      bool processed{false};
      boost::fusion::for_each(_variableMap.table, FunctorFill(this, var.type, var.name, var.value, processed));
      if(!processed) {
        parsingError("Incorrect value '" + var.type + "' for attribute 'type' of the 'variable' tag.");
      }
    };

    auto fillArrayMap = [this](const ChimeraTK::Array& arr) {
      // create accessor and store array value in map using functor
      bool processed{false};
      boost::fusion::for_each(_arrayMap.table, ArrayFunctorFill(this, arr.type, arr.name, arr.values, processed));
      if(!processed) {
        parsingError("Incorrect value '" + arr.type + "' for attribute 'type' of the 'variable' tag.");
      }
    };

    auto parser = ConfigParser(fileName);
    auto v = parser.getVariableList();
    auto a = parser.getArrayList();

    for(const auto& var : *v) {
      fillVariableMap(var);
    }
    for(const auto& arr : *a) {
      fillArrayMap(arr);
    }

    // Stop all modification of moduleTree after reading in the configuration
    _moduleTree->seal();
  }

  // workaround for std::unique_ptr static assert.
  ConfigReader::~ConfigReader() = default;

  /********************************************************************************************************************/

  void ConfigReader::parsingError(const std::string& message) {
    throw ChimeraTK::logic_error("ConfigReader: Error parsing the config file '" + _fileName + "': " + message);
  }

  /********************************************************************************************************************/

  std::list<std::string> ConfigReader::getModules(const std::string& path) const {
    auto* module = _moduleTree->lookup(path);
    if(!module) {
      return {};
    }

    if(module == this) {
      return _moduleTree->getChildList();
    }

    auto* castedModule = dynamic_cast<ModuleTree*>(module);
    return castedModule->getChildList();
  }

  /********************************************************************************************************************/

  /** Functor to set values to the scalar accessors */
  struct FunctorSetValues {
    explicit FunctorSetValues(ConfigReader* theOwner) : owner(theOwner) {}

    template<typename PAIR>
    void operator()(PAIR&) const {
      // get user type and vector
      using T = typename PAIR::first_type;
      std::unordered_map<std::string, ConfigReader::Var<T>>& theMap =
          boost::fusion::at_key<T>(owner->_variableMap.table);

      // iterate vector and set values
      for(auto& pair : theMap) {
        auto& var = pair.second;
        var.accessor = var.value;
        var.accessor.write();
      }
    }

    ConfigReader* owner;
  };

  /********************************************************************************************************************/

  /** Functor to set values to the array accessors */
  struct FunctorSetValuesArray {
    explicit FunctorSetValuesArray(ConfigReader* theOwner) : owner(theOwner) {}

    template<typename PAIR>
    void operator()(PAIR&) const {
      // get user type and vector
      using T = typename PAIR::first_type;
      std::unordered_map<std::string, ConfigReader::Array<T>>& theMap =
          boost::fusion::at_key<T>(owner->_arrayMap.table);

      // iterate vector and set values
      for(auto& pair : theMap) {
        auto& var = pair.second;
        var.accessor = var.value;
        var.accessor.write();
      }
    }

    ConfigReader* owner;
  };

  /********************************************************************************************************************/

  void ConfigReader::prepare() {
    boost::fusion::for_each(_variableMap.table, FunctorSetValues(this));
    boost::fusion::for_each(_arrayMap.table, FunctorSetValuesArray(this));
  }

  /********************************************************************************************************************/

  ChimeraTK::Module* ModuleTree::lookup(const std::string& flattened_module_name) {
    // Root node, return pointer to the ConfigReader
    if(flattened_module_name.empty()) {
      return dynamic_cast<Module*>(_owner);
    }
    // else look up the tree
    return get(flattened_module_name);
  }

  /********************************************************************************************************************/

  void ModuleTree::seal() {
    _sealed = true;
    for(auto& [k, v] : _children) {
      v->seal();
    }
  }

  /********************************************************************************************************************/

  ChimeraTK::ModuleTree* ModuleTree::get(const std::string& flattened_name) {
    auto root_name = root(flattened_name);
    auto remaining_branch_name = branchWithoutRoot(flattened_name);

    ModuleTree* module{nullptr};

    auto r = _children.find(root_name);
    if(r == _children.end() && !_sealed) {
      addChildNode(root_name);
    }

    try {
      if(!remaining_branch_name.empty()) {
        module = _children.at(root_name)->get(remaining_branch_name);
      }
      else {
        module = _children.at(root_name).get();
      }
    }
    catch(std::out_of_range&) {
      assert(_sealed);
    }

    return module;
  }

  /********************************************************************************************************************/

  std::unique_ptr<VariableList> ConfigParser::getVariableList() {
    if(_variableList == nullptr) {
      std::tie(_variableList, _arrayList) = parse();
    }
    return std::move(_variableList);
  }

  /********************************************************************************************************************/

  std::unique_ptr<ArrayList> ConfigParser::getArrayList() {
    if(_arrayList != nullptr) {
      std::tie(_variableList, _arrayList) = parse();
    }
    return std::move(_arrayList);
  }

  /********************************************************************************************************************/

  std::tuple<std::unique_ptr<VariableList>, std::unique_ptr<ArrayList>> ConfigParser::parse() {
    auto* const root = getRootNode(*_parser);
    if(root->get_name() != "configuration") {
      error("Expected 'configuration' tag instead of: " + root->get_name());
    }

    // start with clean lists: parseModule accumulates elements into these.
    _variableList = std::make_unique<VariableList>();
    _arrayList = std::make_unique<ArrayList>();

    const auto* element = dynamic_cast<const xmlpp::Element*>(root);
    std::string parent_module_name;
    parseModule(element, parent_module_name);

    return std::tuple<std::unique_ptr<VariableList>, std::unique_ptr<ArrayList>>{
        std::move(_variableList), std::move(_arrayList)};
  }

  /********************************************************************************************************************/

  void ConfigParser::parseModule(const xmlpp::Element* element, std::string parent_name) {
    auto module_name = (element->get_name() == "configuration") // root node gets special treatment
        ?
        "" :
        element->get_attribute("name")->get_value() + "/";

    parent_name += module_name;

    for(const auto& child : element->get_children()) {
      element = dynamic_cast<const xmlpp::Element*>(child);
      if(!element) {
        continue; // ignore if not an element (e.g. comment)
      }
      if(isVariable(element)) {
        _variableList->emplace_back(prefix(parent_name, parseVariable(element)));
      }
      else if(isArray(element)) {
        _arrayList->emplace_back(prefix(parent_name, parseArray(element)));
      }
      else if(isModule(element)) {
        parseModule(element, parent_name);
      }
      else {
        error("Unknown tag: " + element->get_name());
      }
    }
  }

  /********************************************************************************************************************/

  Variable ConfigParser::parseVariable(const xmlpp::Element* element) {
    auto name = element->get_attribute("name")->get_value();
    auto type = element->get_attribute("type")->get_value();
    auto value = element->get_attribute("value")->get_value();
    return Variable{name, type, value};
  }

  /********************************************************************************************************************/

  Array ConfigParser::parseArray(const xmlpp::Element* element) {
    auto name = element->get_attribute("name")->get_value();
    auto type = element->get_attribute("type")->get_value();
    std::map<size_t, std::string> values = gettArrayValues(element);
    return Array{name, type, values};
  }

  /********************************************************************************************************************/

  xmlpp::Element* ConfigParser::getRootNode(xmlpp::DomParser& parser) {
    auto* root = parser.get_document()->get_root_node();
    if(root->get_name() != "configuration") {
      error("Expected 'configuration' tag instead of: " + root->get_name());
    }
    return root;
  }

  /********************************************************************************************************************/

  void ConfigParser::error(const std::string& message) {
    throw ChimeraTK::logic_error("ConfigReader: Error parsing the config file '" + _fileName + "': " + message);
  }

  /********************************************************************************************************************/

  bool ConfigParser::isVariable(const xmlpp::Element* element) {
    if((element->get_name() == "variable") && element->get_attribute("value")) {
      // validate variable node
      if(!element->get_attribute("name")) {
        error("Missing attribute 'name' for the 'variable' tag.");
      }
      else if(!element->get_attribute("type")) {
        error("Missing attribute 'type' for the 'variable' tag.");
      }
      return true;
    }
    return false;
  }

  /********************************************************************************************************************/

  bool ConfigParser::isArray(const xmlpp::Element* element) {
    if((element->get_name() == "variable") && !element->get_attribute("value")) {
      // validate array node
      if(!element->get_attribute("name")) {
        error("Missing attribute 'name' for the 'variable' tag.");
      }
      else if(!element->get_attribute("type")) {
        error("Missing attribute 'type' for the 'variable' tag.");
      }
      return true;
    }
    return false;
  }

  /********************************************************************************************************************/

  bool ConfigParser::isModule(const xmlpp::Element* element) {
    if(element->get_name() == "module") {
      if(!element->get_attribute("name")) {
        error("Missing attribute 'name' for the 'module' tag.");
      }
      return true;
    }
    return false;
  }

  /********************************************************************************************************************/

  std::map<size_t, std::string> ConfigParser::gettArrayValues(const xmlpp::Element* element) {
    bool valueFound = false;
    std::map<size_t, std::string> values;

    for(const auto& valueChild : element->get_children()) {
      const auto* valueElement = dynamic_cast<const xmlpp::Element*>(valueChild);
      if(!valueElement) {
        continue; // ignore comments etc.
      }
      validateValueNode(valueElement);
      valueFound = true;

      auto* index = valueElement->get_attribute("i");
      auto* value = valueElement->get_attribute("v");

      // get index as number and store value as a string
      size_t intIndex;
      try {
        intIndex = std::stoi(index->get_value());
      }
      catch(std::exception& e) {
        error("Cannot parse string '" + std::string(index->get_value()) + "' as an index number: " + e.what());
      }
      values[intIndex] = value->get_value();
    }
    // make sure there is at least one value
    if(!valueFound) {
      error("Each variable must have a value, either specified as an attribute or as child tags.");
    }
    return values;
  }

  /********************************************************************************************************************/

  void ConfigParser::validateValueNode(const xmlpp::Element* valueElement) {
    if(valueElement->get_name() != "value") {
      error("Expected 'value' tag instead of: " + valueElement->get_name());
    }
    if(!valueElement->get_attribute("i")) {
      error("Missing attribute 'index' for the 'value' tag.");
    }
    if(!valueElement->get_attribute("v")) {
      error("Missing attribute 'value' for the 'value' tag.");
    }
  }

  /********************************************************************************************************************/

  std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& fileName) {
    // Check beforehand to avoid weird unsuppressable "I/O Warning" message from libxml2

    if(!std::filesystem::exists(fileName)) {
      throw ChimeraTK::runtime_error("ConfigReader: " + fileName + " does not exist");
    }

    try {
      return std::make_unique<xmlpp::DomParser>(fileName);
    }
    catch(xmlpp::exception& e) { /// @todo change exception!
      throw ChimeraTK::logic_error("ConfigReader: Error opening the config file '" + fileName + "': " + e.what());
    }
  }

  /********************************************************************************************************************/

  std::string root(const std::string& flattened_name) {
    auto pos = flattened_name.find_first_of('/');
    pos = (pos == std::string::npos) ? flattened_name.size() : pos;
    return flattened_name.substr(0, pos);
  }

  /********************************************************************************************************************/

  std::string branchWithoutRoot(const std::string& flattened_name) {
    auto pos = flattened_name.find_first_of('/');
    pos = (pos == std::string::npos) ? flattened_name.size() : pos + 1;
    return flattened_name.substr(pos, flattened_name.size());
  }

  /********************************************************************************************************************/

  std::string branch(const std::string& flattened_name) {
    auto pos = flattened_name.find_last_of('/');
    pos = (pos == std::string::npos) ? 0 : pos;
    return flattened_name.substr(0, pos);
  }

  /********************************************************************************************************************/

  std::string leaf(const std::string& flattened_name) {
    auto pos = flattened_name.find_last_of('/');
    return flattened_name.substr(pos + 1, flattened_name.size());
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
