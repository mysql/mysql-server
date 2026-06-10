/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_PERSISTENT_PERSISTENT_DATA_CONTENT_FILE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_PERSISTENT_PERSISTENT_DATA_CONTENT_FILE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "collector/mysql_cache_manager.h"
#include "helper/media_type.h"
#include "mrs/database/entry/content_file.h"
#include "mrs/rest/response_cache.h"

namespace mrs {
namespace endpoint {
namespace handler {

class PersistentDataContentFile {
 public:
  using ContentFile = mrs::database::entry::ContentFile;
  using ContentFilePtr = std::shared_ptr<ContentFile>;
  using MySQLSession = collector::MysqlCacheManager::Object;
  using EndpointResponseCachePtr =
      std::shared_ptr<mrs::FileEndpointResponseCache>;
  using OptionalIndexNames = std::optional<std::vector<std::string>>;

  struct FetchedFile {
    std::string content;
    helper::MediaType content_type;
  };

 public:
  PersistentDataContentFile(ContentFilePtr entry_file,
                            collector::MysqlCacheManager *cache,
                            mrs::ResponseCache *response_cache,
                            const OptionalIndexNames &index_names);

  FetchedFile fetch_file(MySQLSession ctxts_sql_session);

 private:
  ContentFilePtr entry_;
  collector::MysqlCacheManager *cache_;
  EndpointResponseCachePtr response_cache_;
  OptionalIndexNames index_names_;
};

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_PERSISTENT_PERSISTENT_DATA_CONTENT_FILE_H_ \
        */
