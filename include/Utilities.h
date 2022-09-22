// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <string>

namespace ChimeraTK::Utilities {

  /**
   *  Return the last component of the given qualified path name.
   *  Example: "/some/deep/hierarchy/levels" would return "levels"
   *
   *  This function is useful together with getPathName(), when a qualified variable name is given, and a
   *  HierarchyModifyingGroup with the variable inside needs to be created.
   */
  std::string getUnqualifiedName(const std::string& qualifiedName);

  /**
   *  Return all but the last components of the given qualified name.
   *  Example: "/some/deep/hierarchy/levels" would return "/some/deep/hierarchy"
   *
   *  This function is useful together with getUnqualifiedName(), when a qualified variable name is given, and a
   *  HierarchyModifyingGroup with the variable inside needs to be created.
   */
  std::string getPathName(const std::string& qualifiedName);

  /**
   * Convert all characters which are not allowed in variable or module names into underscores.
   * If allowDotsAndSlashes is true, dots and slashes will not be converted into underscores, so the resulting name can
   * be a qualified name.
   */
  std::string stripName(const std::string& name, bool allowDotsAndSlashes);

  /**
   * Check given name for characters which are not allowed in variable or module names.
   * If allowDotsAndSlashes is true, dots and slashes are allowed, so the resulting name can be a qualified name.
   */
  bool checkName(const std::string& name, bool allowDotsAndSlashes);

} // namespace ChimeraTK::Utilities
