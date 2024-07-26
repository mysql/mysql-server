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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_H_

#include <memory>
#include <string>

#include "mysqlrouter/component/http_server_component.h"

#include "collector/mysql_cache_manager.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/database/entry/object.h"
#include "mrs/gtid_manager.h"
#include "mrs/interface/handler_factory.h"
#include "mrs/interface/object.h"
#include "mrs/interface/object_schema.h"
#include "mrs/interface/query_factory.h"
#include "mrs/interface/rest_handler.h"
#include "mrs/interface/state.h"

namespace mrs {

class Object : public std::enable_shared_from_this<Object>,
               public mrs::interface::Object {
 public:
  using EntryDbObject = database::entry::DbObject;
  using EntryObjectPtr = std::shared_ptr<database::entry::Object>;
  using HandlerFactory = mrs::interface::HandlerFactory;
  using QueryFactory = mrs::interface::QueryFactory;

 public:
  Object(const EntryDbObject &db_entry, RouteSchemaPtr schema,
         collector::MysqlCacheManager *cache, const bool is_ssl,
         mrs::interface::AuthorizeManager *auth_manager,
         mrs::GtidManager *gtid_manager, HandlerFactory *handler_factory,
         QueryFactory *query_factory);
  ~Object() override;

  void turn(const State state) override;
  bool update(const void *pe, RouteSchemaPtr schema) override;

  const std::string &get_rest_canonical_url() override;
  const std::string &get_rest_url() override;
  const std::string &get_json_description() override;
  const std::vector<std::string> get_rest_path() override;
  const std::string &get_rest_path_raw() override;
  const std::string &get_rest_canonical_path() override;
  const std::string &get_object_path() override;
  const std::string &get_object_name() override;
  const std::string &get_schema_name() override;
  EntryObjectPtr get_object() override;
  const std::string &get_options() override;
  const Fields &get_parameters() override;
  uint32_t get_on_page() override;
  Format get_format() const override;
  Media get_media_type() const override;

  bool requires_authentication() const override;
  /* same as get_id but with type-flags */
  EntryKey get_key() const override;
  UniversalId get_id() const override;
  UniversalId get_service_id() const override;
  bool has_access(const Access access) const override;
  uint32_t get_access() const override;

  RouteSchemaPtr get_schema() override;
  collector::MysqlCacheManager *get_cache() override;

  const RowUserOwnership &get_user_row_ownership() const override;
  const VectorOfRowGroupOwnership &get_group_row_ownership() const override;
  const std::string *get_default_content() override;
  const std::string *get_redirection() override;

  bool get_service_active() const override;
  void set_service_active(const bool active) override;

 private:
  bool is_active() const;
  void handlers_for_table();
  void handlers_for_sp();
  void handlers_for_function();
  void update_variables();
  static std::string extract_first_slash(const std::string &value);

  RouteSchemaPtr schema_;
  EntryDbObject pe_;
  std::string rest_path_;
  std::string rest_canonical_path_;
  std::string rest_path_raw_;
  std::string schema_name_;
  std::string object_name_;
  std::string json_description_;
  collector::MysqlCacheManager *cache_;
  bool is_ssl_;
  std::string url_route_;
  std::string url_rest_canonical_;
  std::unique_ptr<Handler> handle_object_;
  std::unique_ptr<Handler> handle_metadata_;
  uint32_t access_flags_;
  mrs::interface::AuthorizeManager *auth_manager_;
  mrs::GtidManager *gtid_manager_;
  HandlerFactory *handler_factory_;
  QueryFactory *query_factory_;
  Object::RowUserOwnership user_ownership_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_H_
