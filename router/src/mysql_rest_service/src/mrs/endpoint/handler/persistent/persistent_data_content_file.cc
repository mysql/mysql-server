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

#include "mrs/endpoint/handler/persistent/persistent_data_content_file.h"

#include "mrs/database/query_entry_content_file.h"
#include "mrs/http/error.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/string_utils.h"

namespace mrs {
namespace endpoint {
namespace handler {

using FetchedFile = PersistentDataContentFile::FetchedFile;
using MySQLSession = collector::MysqlCacheManager::Object;
using CachedObject = collector::MysqlCacheManager::CachedObject;
using MysqlCacheManager = collector::MysqlCacheManager;
using MySQLConnection = collector::MySQLConnection;

static CachedObject get_session(
    MySQLSession session, MysqlCacheManager *cache_manager,
    MySQLConnection type = MySQLConnection::kMySQLConnectionMetadataRO) {
  if (session) return CachedObject(nullptr, true, session);

  return cache_manager->get_instance(type, false);
}

PersistentDataContentFile::PersistentDataContentFile(
    ContentFilePtr entry_file, collector::MysqlCacheManager *cache,
    mrs::ResponseCache *response_cache, const OptionalIndexNames &index_names)
    : entry_{entry_file}, cache_{cache}, index_names_{index_names} {
  if (response_cache) {
    response_cache_ =
        std::make_shared<FileEndpointResponseCache>(response_cache);
  }
}

FetchedFile PersistentDataContentFile::fetch_file(
    MySQLSession ctxts_sql_session) {
  if (response_cache_) {
    auto cached_entry = response_cache_->lookup_file(entry_->id);
    if (cached_entry) {
      return {cached_entry->data, cached_entry->media_type.value()};
    }
  }

  auto session = get_session(ctxts_sql_session, cache_,
                             MySQLConnection::kMySQLConnectionMetadataRO);
  if (nullptr == session.get())
    throw http::Error(HttpStatusCode::InternalError);

  mysql_harness::Path path{entry_->request_path};
  auto result_type = helper::get_media_type_from_extension(
      mysql_harness::make_lower(path.extension()).c_str());

  mrs::database::QueryEntryContentFile query_content_file;
  query_content_file.query_file(session.get(), entry_->id);

  if (response_cache_) {
    response_cache_->create_file_entry(entry_->id, query_content_file.result,
                                       result_type);
  }

  return {query_content_file.result, result_type};
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
