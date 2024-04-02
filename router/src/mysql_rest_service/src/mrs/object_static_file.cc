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

#include "mrs/object_static_file.h"

#include <time.h>

#include "helper/string/contains.h"
#include "mrs/rest/handler_file.h"

namespace mrs {

using Format = ObjectStaticFile::Format;
using Column = helper::Column;

ObjectStaticFile::ObjectStaticFile(
    const ContentFile &pe, RouteSchemaPtr schema,
    collector::MysqlCacheManager *cache, const bool is_ssl,
    mrs::interface::AuthorizeManager *auth_manager,
    std::shared_ptr<HandlerFactory> handler_factory)
    : cse_{pe},
      cache_{cache},
      is_ssl_{is_ssl},
      auth_{auth_manager},
      handler_factory_{handler_factory} {
  log_debug("src.cse_.default_handling_directory_index=%s",
            pe.default_handling_directory_index ? "true" : "false");
  update(&pe, schema);
}

void ObjectStaticFile::turn(const State state) {
  auto active = cse_.active_file && cse_.active_service && cse_.active_set;
  if (stateOff == state || !active) {
    handle_file_.reset();

    return;
  }

  auto handle_file = std::make_unique<rest::HandlerFile>(this, auth_);

  handle_file_ = std::move(handle_file);
}

bool ObjectStaticFile::update(const void *pv, RouteSchemaPtr schema) {
  auto &pe = *reinterpret_cast<const ContentFile *>(pv);
  bool result = false;
  if (schema != schema_) {
    if (schema_) schema_->route_unregister(this);
    if (schema) schema->route_register(this);
    schema_ = schema;
    result = true;
  }

  if ((cse_.service_path != pe.service_path) ||
      (cse_.schema_path != pe.schema_path) || (cse_.file_path != pe.file_path))
    result = true;

  cse_ = pe;

  update_variables();

  return result;
}

const std::string &ObjectStaticFile::get_rest_canonical_url() {
  static std::string empty;
  return empty;
}

const std::string &ObjectStaticFile::get_rest_url() { return rest_url_; }

const std::string &ObjectStaticFile::get_json_description() {
  static std::string empty;
  return empty;
}

const std::vector<std::string> ObjectStaticFile::get_rest_path() {
  log_debug("cse_.default_handling_directory_index=%s",
            cse_.default_handling_directory_index ? "true" : "false");
  const static std::string k_index_html = "/index.html$";
  if (cse_.default_handling_directory_index &&
      helper::ends_with(rest_path_, "/index.html$")) {
    auto rest_path2 =
        rest_path_.substr(0, rest_path_.length() - k_index_html.length() + 1) +
        "$";
    auto rest_path3 =
        rest_path_.substr(0, rest_path_.length() - k_index_html.length()) + "$";
    return {rest_path_, rest_path2, rest_path3};
  } else if (cse_.is_index) {
    using namespace std::string_literals;  // NOLINT(build/namespaces)
    return {rest_path_, "^"s + cse_.service_path + cse_.schema_path + "$",
            "^"s + cse_.service_path + cse_.schema_path + "/$"};
  }
  return {rest_path_};
}

const std::string &ObjectStaticFile::get_rest_path_raw() {
  return rest_path_raw_;
}

const std::string &ObjectStaticFile::get_rest_canonical_path() {
  static std::string empty;
  return empty;
}

const std::string &ObjectStaticFile::get_object_path() {
  return cse_.file_path;
}

const std::string &ObjectStaticFile::get_schema_name() {
  static std::string empty;
  return empty;
}

const std::string &ObjectStaticFile::get_object_name() {
  static std::string empty;
  return empty;
}

const std::string &ObjectStaticFile::get_version() { return version_; }

const std::string &ObjectStaticFile::get_options() {
  if (!cse_.options_json_schema.empty()) return cse_.options_json_schema;
  return cse_.options_json_service;
}

interface::Object::EntryObject ObjectStaticFile::get_cached_object() {
  static EntryObject empty;
  return empty;
}

const std::vector<Column> &ObjectStaticFile::get_cached_columnes() {
  static std::vector<Column> empty;
  return empty;
}

const mrs::interface::Object::Fields &ObjectStaticFile::get_parameters() {
  static mrs::interface::Object::Fields empty;
  return empty;
}

uint32_t ObjectStaticFile::get_on_page() { return 1; }

bool ObjectStaticFile::requires_authentication() const {
  return cse_.requires_authentication || cse_.schema_requires_authentication;
}

UniversalId ObjectStaticFile::get_id() const { return cse_.id; }

UniversalId ObjectStaticFile::get_service_id() const { return cse_.service_id; }

bool ObjectStaticFile::has_access(const Access access) const {
  if (access == kRead) return true;

  return false;
}

uint32_t ObjectStaticFile::get_access() const {
  return static_cast<uint32_t>(kRead);
}

Format ObjectStaticFile::get_format() const { return Object::kMedia; }

ObjectStaticFile::Media ObjectStaticFile::get_media_type() const {
  return {false, {}};
}

ObjectStaticFile::RouteSchema *ObjectStaticFile::get_schema() {
  return schema_.get();
}

collector::MysqlCacheManager *ObjectStaticFile::get_cache() { return cache_; }

void ObjectStaticFile::update_variables() {
  rest_url_ = (is_ssl_ ? "https://" : "http://") + cse_.host +
              cse_.service_path + cse_.schema_path + cse_.file_path;
  rest_path_ =
      "^" + cse_.service_path + cse_.schema_path + cse_.file_path + "$";
  rest_path_raw_ = cse_.service_path + cse_.schema_path + cse_.file_path;
  version_ = "\"" + std::to_string(time(nullptr)) + "-" +
             std::to_string(cse_.size) + "\"";
}

const ObjectStaticFile::RowUserOwnership &
ObjectStaticFile::get_user_row_ownership() const {
  static RowUserOwnership result{false, {}};

  return result;
}
const ObjectStaticFile::VectorOfRowGroupOwnership &
ObjectStaticFile::get_group_row_ownership() const {
  static VectorOfRowGroupOwnership result;

  return result;
}

const std::string *ObjectStaticFile::get_default_content() {
  if (cse_.content) return &cse_.content.value();

  return nullptr;
}

const std::string *ObjectStaticFile::get_redirection() {
  if (cse_.redirect) return &cse_.redirect.value();

  return nullptr;
}

}  // namespace mrs
