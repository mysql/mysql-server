/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_ROUTE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_ROUTE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "collector/mysql_cache_manager.h"
#include "helper/mysql_column.h"
#include "mrs/database/entry/field.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/entry/row_group_ownership.h"
#include "mrs/database/entry/row_user_ownership.h"
#include "mrs/interface/object_schema.h"
#include "mrs/interface/rest_handler.h"
#include "mrs/interface/universal_id.h"

namespace mrs {
namespace interface {

class Object {
 public:
  using Handler = mrs::interface::RestHandler;
  using RouteSchema = mrs::interface::ObjectSchema;
  using RouteSchemaPtr = std::shared_ptr<RouteSchema>;
  using EntryObject = std::shared_ptr<database::entry::Object>;
  using Column = helper::Column;
  using RowUserOwnership = database::entry::RowUserOwnership;
  using VectorOfRowGroupOwnership =
      std::vector<database::entry::RowGroupOwnership>;
  using Fields = database::entry::ResultSets;

  enum Access { kCreate = 1, kRead = 2, kUpdate = 4, kDelete = 8 };
  enum Format { kFeed = 1, kItem = 2, kMedia = 3 };

  struct Media {
    bool auto_detect;
    std::optional<std::string> force_type;
  };

 public:
  virtual ~Object() = default;

  virtual void turn(const State state) = 0;
  virtual bool update(const void *pe, RouteSchemaPtr schema) = 0;

  virtual const std::string &get_rest_canonical_url() = 0;
  virtual const std::string &get_rest_url() = 0;
  virtual const std::string &get_json_description() = 0;
  virtual const std::vector<std::string> get_rest_path() = 0;
  virtual const std::string &get_rest_path_raw() = 0;
  virtual const std::string &get_rest_canonical_path() = 0;
  virtual const std::string &get_object_path() = 0;
  virtual const std::string &get_schema_name() = 0;
  virtual const std::string &get_object_name() = 0;
  virtual const std::string &get_options() = 0;
  virtual const std::string &get_version() {
    static std::string empty;
    return empty;
  }
  virtual const std::string *get_default_content() = 0;
  virtual const std::string *get_redirection() = 0;
  virtual const Fields &get_parameters() = 0;
  virtual EntryObject get_cached_object() = 0;
  virtual const std::vector<Column> &get_cached_columnes() = 0;
  virtual uint32_t get_on_page() = 0;

  virtual bool requires_authentication() const = 0;
  virtual UniversalId get_service_id() const = 0;
  virtual UniversalId get_id() const = 0;
  virtual bool has_access(const Access access) const = 0;
  virtual Format get_format() const = 0;
  virtual Media get_media_type() const = 0;
  virtual uint32_t get_access() const = 0;

  virtual const RowUserOwnership &get_user_row_ownership() const = 0;
  virtual const VectorOfRowGroupOwnership &get_group_row_ownership() const = 0;

  virtual RouteSchema *get_schema() = 0;
  virtual collector::MysqlCacheManager *get_cache() = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_ROUTE_H_
