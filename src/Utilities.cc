// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Utilities.h"

#include "EntityOwner.h"

#include <boost/algorithm/algorithm.hpp>
#include <boost/format.hpp>

#include <pthread.h>

#include <cassert>
#include <iostream>

namespace ChimeraTK::Utilities {

  /********************************************************************************************************************/

  std::string getUnqualifiedName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of('/');
    if(found == std::string::npos) {
      return qualifiedName;
    }
    return qualifiedName.substr(found + 1);
  }

  /********************************************************************************************************************/

  std::string getPathName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of('/');
    if(found == std::string::npos) {
      return ".";
    }
    return qualifiedName.substr(0, found);
  }

  /********************************************************************************************************************/

  namespace {
    constexpr std::string_view legalChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_";
  } // namespace

  /********************************************************************************************************************/

  std::string escapeName(const std::string& name, bool allowDotsAndSlashes) {
    std::string myLegalChars(legalChars);

    // remove underscore from legal chars, as we use it as an escape character
    assert(myLegalChars[myLegalChars.size() - 1] == '_');
    myLegalChars.resize(myLegalChars.size() - 1);

    if(allowDotsAndSlashes) {
      myLegalChars += "./";
    }

    std::string name_stripped;
    size_t i;
    size_t iLast = 0;
    while((i = name.find_first_not_of(myLegalChars, iLast)) != std::string::npos) {
      name_stripped += name.substr(iLast, i - iLast);
      name_stripped += (boost::format("_%03d") % int(name[i])).str();
      iLast = i + 1;
    }
    name_stripped += name.substr(iLast);

    return name_stripped;
  }

  /********************************************************************************************************************/

  std::string unescapeName(const std::string& name_stripped) {
    std::string name;
    size_t i;
    size_t iLast = 0;
    while((i = name_stripped.find('_', iLast)) != std::string::npos) {
      name += name_stripped.substr(iLast, i - iLast);
      char code = char(std::stoi(name_stripped.substr(i + 1, 3)));
      name += code;
      iLast = i + 4;
    }
    name += name_stripped.substr(iLast);

    return name;
  }

  /********************************************************************************************************************/

  bool checkName(const std::string& name, bool allowDotsAndSlashes) {
    if(name == EntityOwner::namePrefixConstant.substr(1)) { // namePrefixConstant starts with /
      // EntityOwner::namePrefixConstant violates the allowed characters, so we need to make an exception here.
      return true;
    }

    std::string myLegalChars(legalChars);

    if(allowDotsAndSlashes) {
      myLegalChars += "./";
    }

    return (name.find_first_not_of(myLegalChars) == std::string::npos);
  }

  /********************************************************************************************************************/

  void setThreadName(const std::string& name) {
#if defined(__linux__)
    pthread_setname_np(pthread_self(), name.substr(0, std::min<std::string::size_type>(name.length(), 15)).c_str());
#elif defined(__APPLE__)
    pthread_setname_np(name.substr(0, std::min<std::string::size_type>(name.length(), 15)).c_str());
#endif
  }

  /********************************************************************************************************************/

  // removes trailing slashes, used for variable names
  // raises for multiple slashes
  std::string raiseIftrailingSlash(const std::string& name, bool isModule) {
    if(isModule && name == "/") {
      return name;
    }
    if(boost::ends_with(name, "/")) {
      throw ChimeraTK::logic_error(name + ": " + (isModule ? "module" : "variable") + " names cannot end with /");
    }
    if(name.find("//") != std::string::npos) {
      throw ChimeraTK::logic_error(name + " variable names cannot contain consecutive slashes");
    }
    return name;
  }

  /********************************************************************************************************************/

  bool isBeingDebugged() {
    try {
      std::ifstream status_file("/proc/self/status");
      std::string line;
      while(std::getline(status_file, line)) {
        if(line.substr(0, 10) == "TracerPid:") {
          int tracer_pid = std::stoi(line.substr(10));
          return tracer_pid != 0;
        }
      }
      return false;
    }
    catch(...) {
      // assume no debugger if anything goes wrong (e.g. on different platforms)
      return false;
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK::Utilities
