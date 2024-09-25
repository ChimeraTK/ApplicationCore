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
   * Convert all characters which are not allowed in variable or module names into underscores followed by their
   * 3-digit ascii code. An underscore will be escaped that way as well.
   * If allowDotsAndSlashes is true, dots and slashes will not be converted, so the resulting name can be a qualified
   * name.
   */
  std::string escapeName(const std::string& name, bool allowDotsAndSlashes);

  /**
   * Undo the escaping done by escapeName().
   */
  std::string unescapeName(const std::string& name_stripped);

  /**
   * Check given name for characters which are not allowed in variable or module names.
   * If allowDotsAndSlashes is true, dots and slashes are allowed, so the resulting name can be a qualified name.
   */
  bool checkName(const std::string& name, bool allowDotsAndSlashes);

  /**
   * Set name of the current thread.
   *
   * @note: This function contains platform-dependent code and may need adjustment for new platforms. On unsupported
   * platforms, this function does nothing.
   */
  void setThreadName(const std::string& name);

  /**
   * Strips trailing slashes
   *
   */
  std::string stripTrailingSlashes(const std::string& name);

  /**
   * Raises logic error if name ends in a slash
   *
   */
  std::string raiseIftrailingSlash(const std::string& name);

} // namespace ChimeraTK::Utilities
