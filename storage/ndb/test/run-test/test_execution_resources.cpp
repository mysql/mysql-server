/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include "test_execution_resources.hpp"

#include <string>
#include <utility>
#include <vector>
#include <util/File.hpp>

bool TestExecutionResources::registerExecutable(std::string exe,
                                                bool isRequired) {
  if (m_resources.find(exe) != m_resources.end()) {
    return false;
  }

  Resource executable = {exe, Resource::Type::Exe, isRequired, {"", ""}};
  m_resources[executable.name] = executable;

  return true;
}

bool TestExecutionResources::registerLibrary(std::string lib, bool isRequired) {
  if (m_resources.find(lib) != m_resources.end()) {
    return false;
  }

  Resource library = {lib, Resource::Type::Lib, isRequired, {"", ""}};
  m_resources[library.name] = library;

  return true;
}

bool TestExecutionResources::setRequired(std::string resource) {
  auto it = m_resources.find(resource);
  if (it == m_resources.end()) {
    return false;
  }

  it->second.isRequired = true;
  return true;
}

bool TestExecutionResources::loadPaths(const char *prefix0, const char *prefix1,
                                       std::vector<std::string> *error,
                                       std::vector<std::string> *info) {
  require(prefix0 != nullptr);

  m_prefixes = {prefix0, prefix1};

  bool ok = true;
  for (auto &e : m_resources) {
    Resource &resource = e.second;

    ok &= setPath(resource, 0, error, info);

    if (prefix1 != nullptr) {
      ok &= setPath(resource, 1, error, info);
    }
  }

  return ok;
}

bool TestExecutionResources::setPath(Resource &resource, int prefix,
                                     std::vector<std::string> *error,
                                     std::vector<std::string> *info) {
  require(isValidPrefix(prefix));

  const bool setFolder = (resource.type == Resource::Type::Lib);
  std::string path = find_path(resource.name, prefix, setFolder);
  if (path != "") {
    resource.paths[prefix] = path;
    return true;
  }

  bool ok = true;
  if (resource.isRequired) {
    reportRequired(resource.name, m_prefixes[prefix], error);
    ok = false;
  } else {
    reportNonRequired(resource.name, m_prefixes[prefix], info);
  }

  return ok;
}

std::string TestExecutionResources::find_path(std::string name, int prefix,
                                              bool returnFolder) {
  for (auto &folder : m_search_path) {
    std::string path = std::string(m_prefixes[prefix]) + "/" + folder;
    std::string fullResourcePath = path + "/" + name;
    if (File_class::exists(fullResourcePath.c_str())) {
      return returnFolder ? path : fullResourcePath;
    }
  }

  return {};
}

void TestExecutionResources::reportNonRequired(std::string exe,
                                               const char *prefix,
                                               std::vector<std::string> *msgs) {
  if (msgs == nullptr) return;

  std::string msg = "Missing non-required '" + exe + "' in '" + prefix + "'";
  msgs->push_back(std::move(msg));
}

void TestExecutionResources::reportRequired(std::string exe, const char *prefix,
                                            std::vector<std::string> *msgs) {
  if (msgs == nullptr) return;

  std::string msg = "Failure to locate '" + exe + "' in '" + prefix + "'";
  msgs->push_back(std::move(msg));
}

std::string TestExecutionResources::getPath(std::string name, int prefix) {
  require(isValidPrefix(prefix));

  const auto resource = m_resources.find(name);
  if (resource == m_resources.end()) {
    return {};
  }

  return resource->second.paths[prefix];
}
