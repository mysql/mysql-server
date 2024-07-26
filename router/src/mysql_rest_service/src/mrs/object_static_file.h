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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_STATIC_FILE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_STATIC_FILE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/handler_factory.h"
#include "mrs/interface/object.h"
#include "mrs/interface/query_factory.h"
#include "mrs/interface/rest_handler.h"
#include "mrs/interface/state.h"
#include "mrs/rest/entry/app_content_file.h"

namespace mrs {

class ObjectStaticFile : public mrs::interface::Object {
 public:
  using RouteSchema = mrs::interface::ObjectSchema;
  using ContentFile = rest::entry::AppContentFile;
  using HandlerFactory = mrs::interface::HandlerFactory;
  using AuthManager = mrs::interface::AuthorizeManager;
  using MysqlCacheManager = collector::MysqlCacheManager;
  using QueryFactory = mrs::interface::QueryFactory;

 public:
  ObjectStaticFile(const ContentFile &pe, RouteSchemaPtr schema,
                   MysqlCacheManager *cache, const bool is_ssl,
                   AuthManager *auth_manager, HandlerFactory *handler_factory,
                   QueryFactory *query_factory);

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
  const std::string &get_version() override;
  const std::string &get_options() override;
  EntryObjectPtr get_object() override;
  const Fields &get_parameters() override;
  uint32_t get_on_page() override;
  Media get_media_type() const override;

  bool requires_authentication() const override;
  UniversalId get_service_id() const override;
  EntryKey get_key() const override;
  UniversalId get_id() const override;
  bool has_access(const Access access) const override;
  uint32_t get_access() const override;
  Format get_format() const override;

  RouteSchemaPtr get_schema() override;
  MysqlCacheManager *get_cache() override;

  const RowUserOwnership &get_user_row_ownership() const override;
  const VectorOfRowGroupOwnership &get_group_row_ownership() const override;
  const std::string *get_default_content() override;
  const std::string *get_redirection() override;

  bool get_service_active() const override;
  void set_service_active(const bool active) override;

 private:
  void update_variables();

  State state_{stateOff};
  ContentFile cse_;
  RouteSchemaPtr schema_;
  MysqlCacheManager *cache_;
  bool is_ssl_;
  AuthManager *auth_;
  std::string rest_url_;
  std::string rest_path_;
  std::string rest_path_raw_;
  std::string version_;
  std::unique_ptr<Handler> handle_file_;
  HandlerFactory *handler_factory_;
  QueryFactory *query_factory_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_STATIC_FILE_H_
