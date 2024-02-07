/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TEST_EXECUTION_RESOURCES_HPP_
#define TEST_EXECUTION_RESOURCES_HPP_

#include <array>
#include <cassert>
#include <map>
#include <string>
#include <vector>

class TestExecutionResources {
 public:
  const char *const NDB_MGMD = "ndb_mgmd";
  const char *const NDBD = "ndbd";
  const char *const NDBMTD = "ndbmtd";
  const char *const MYSQLD = "mysqld";
  const char *const MYSQL_INSTALL_DB = "mysql_install_db";
  const char *const LIBMYSQLCLIENT_DYLIB = "libmysqlclient.dylib";
  const char *const LIBMYSQLCLIENT_SO = "libmysqlclient.so";

  bool registerExecutable(std::string exe, bool isRequired = true);

  bool registerLibrary(std::string lib, bool isRequired = true);

  bool setRequired(std::string resource);

  bool loadPaths(const char *prefix0, const char *prefix1 = nullptr,
                 std::vector<std::string> *error = nullptr,
                 std::vector<std::string> *info = nullptr);

  std::string getExecutableFullPath(const char *exe, int prefix = 0) {
    return getPath(std::string(exe), prefix);
  }

  std::string getLibraryDirectory(const char *lib, int prefix = 0) {
    return getPath(std::string(lib), prefix);
  }

  std::string findExecutableFullPath(const char *exe, int prefix = 0) {
    return find_path(exe, prefix, false);
  }

  std::string findLibraryDirectory(const char *lib, int prefix = 0) {
    return find_path(lib, prefix, true);
  }

 private:
  std::array<const char *, 6> m_search_path = {
      {"bin", "libexec", "sbin", "scripts", "lib", "lib/mysql"}};

  std::array<const char *, 2> m_prefixes;

  bool isValidPrefix(int prefix) const { return (prefix == 0 || prefix == 1); }

  struct Resource {
    std::string name;
    enum class Type { Exe, Lib } type;
    bool isRequired;
    std::string paths[2];
  };

  std::map<std::string, TestExecutionResources::Resource> m_resources = {
      {NDB_MGMD, {NDB_MGMD, Resource::Type::Exe, true, {"", ""}}},
      {NDBD, {NDBD, Resource::Type::Exe, true, {"", ""}}},
      {NDBMTD, {NDBMTD, Resource::Type::Exe, false, {"", ""}}},
      {MYSQLD, {MYSQLD, Resource::Type::Exe, false, {"", ""}}},
      {MYSQL_INSTALL_DB,
       {MYSQL_INSTALL_DB, Resource::Type::Exe, false, {"", ""}}},
#if defined(__MACH__)
      {LIBMYSQLCLIENT_DYLIB,
       {LIBMYSQLCLIENT_DYLIB, Resource::Type::Lib, true, {"", ""}}}
#else
      {LIBMYSQLCLIENT_SO,
       {LIBMYSQLCLIENT_SO, Resource::Type::Lib, true, {"", ""}}}
#endif
  };

  bool setPath(Resource &resource, int prefix, std::vector<std::string> *error,
               std::vector<std::string> *info);

  std::string find_path(std::string name, int prefix, bool returnFolder);

  void reportNonRequired(std::string exe, const char *prefix,
                         std::vector<std::string> *msgs);

  void reportRequired(std::string exe, const char *prefix,
                      std::vector<std::string> *msgs);

  std::string getPath(std::string name, int prefix = 0);
};

#endif  // TEST_EXECUTION_RESOURCES_HPP_
