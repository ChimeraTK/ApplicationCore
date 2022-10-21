// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Utilities.h"

namespace ChimeraTK::Utilities {

  /********************************************************************************************************************/

  std::string getUnqualifiedName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of('/');
    if(found == std::string::npos) return qualifiedName;
    return qualifiedName.substr(found + 1);
  }

  /********************************************************************************************************************/

  std::string getPathName(const std::string& qualifiedName) {
    auto found = qualifiedName.find_last_of('/');
    if(found == std::string::npos) return ".";
    return qualifiedName.substr(0, found);
  }

  /********************************************************************************************************************/

  namespace {
    std::string legalChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_";
  } // namespace

  /********************************************************************************************************************/

  std::string stripName(const std::string& name, bool allowDotsAndSlashes) {
    if(allowDotsAndSlashes) {
      legalChars += "./";
    }

    auto name_stripped = name;
    size_t i = 0;
    while((i = name_stripped.find_first_not_of(legalChars, i)) != std::string::npos) {
      name_stripped[i] = '_';
    }

    return name_stripped;
  }

  /********************************************************************************************************************/

  bool checkName(const std::string& name, bool allowDotsAndSlashes) {
    if(allowDotsAndSlashes) {
      legalChars += "./";
    }

    return (name.find_first_not_of(legalChars) == std::string::npos);
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

} // namespace ChimeraTK::Utilities
