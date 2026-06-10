/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_FILE_SYSTEM_FILE_SYSTEM_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_FILE_SYSTEM_FILE_SYSTEM_H_

#include <map>
#include <memory>
#include <string>

#include "collector/mysql_cache_manager.h"
#include "mrs/rest/response_cache.h"
#include "mysqlrouter/polyglot_file_system.h"

namespace mrs {

namespace endpoint {
class DbServiceEndpoint;
class ContentFileEndpoint;
}  // namespace endpoint

namespace file_system {

using ContentFilePtr = std::shared_ptr<mrs::endpoint::ContentFileEndpoint>;
using ContentFileWeakPtr = std::weak_ptr<mrs::endpoint::ContentFileEndpoint>;

class DbServiceFileSystem : public shcore::polyglot::IFile_system {
 public:
  DbServiceFileSystem(endpoint::DbServiceEndpoint *endpoint);

  std::string parse_uri_path(const std::string &uri) override;
  std::string parse_string_path(const std::string &path) override;
  void check_access(const std::string &path, int64_t flags) override;
  void create_directory(const std::string &path) override;
  void remove(const std::string &path) override;
  std::shared_ptr<shcore::polyglot::ISeekable_channel> new_byte_channel(
      const std::string &path) override;
  std::shared_ptr<shcore::polyglot::IDirectory_stream> new_directory_stream(
      const std::string &path) override;
  std::string to_absolute_path(const std::string &path) override;
  std::string to_real_path(const std::string &path) override;

  ~DbServiceFileSystem() override = default;

 private:
  endpoint::DbServiceEndpoint *m_service_endpoint;
  void traverse_files(std::function<bool(const ContentFilePtr &)> callback);
  ContentFilePtr lookup_file(const std::string &request_path);
};

}  // namespace file_system
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_FILE_SYSTEM_FILE_SYSTEM_H_
