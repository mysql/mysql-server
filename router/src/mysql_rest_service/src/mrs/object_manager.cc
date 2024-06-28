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

#include "mrs/object_manager.h"

#include <algorithm>
#include <iterator>
#include <vector>
#include "mysql/harness/logging/logging.h"

#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "helper/string/contains.h"
#include "mrs/object_factory.h"
#include "mrs/rest/handler_string.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {

namespace cvt {
using std::to_string;
static const std::string &to_string(const std::string &str) { return str; }
}  // namespace cvt

namespace {
class PluginOptions {
 public:
  std::map<std::string, std::string> default_content;
};

class ParsePluginOptions
    : public helper::json::RapidReaderHandlerToStruct<PluginOptions> {
 public:
  template <typename ValueType>
  void handle_object_value(const std::string &key, const ValueType &vt) {
    //    log_debug("handle_object_value key:%s, v:%s", key.c_str(),
    //              cvt::to_string(vt).c_str());
    static const std::string kHttpContent = "defaultContent.";
    using std::to_string;

    if (helper::starts_with(key, kHttpContent)) {
      result_.default_content[key.substr(kHttpContent.length())] =
          cvt::to_string(vt);
    }
  }

  template <typename ValueType>
  void handle_value(const ValueType &vt) {
    const auto &key = get_current_key();
    if (is_object_path()) {
      handle_object_value(key, vt);
    }
  }

  bool String(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }

  bool RawNumber(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }

  bool Bool(bool v) override {
    handle_value(v);
    return true;
  }
};

PluginOptions parse_json_options(const std::string &options) {
  return helper::json::text_to_handler<ParsePluginOptions>(options);
}
}  // namespace

ObjectManager::ObjectManager(collector::MysqlCacheManager *cache,
                             const bool is_ssl,
                             mrs::interface::AuthorizeManager *auth_manager,
                             mrs::GtidManager *gtid_manager,
                             ::mrs::interface::ObjectFactory *factory)
    : cache_{cache},
      is_ssl_{is_ssl},
      auth_manager_{auth_manager},
      gtid_manager_{gtid_manager},
      factory_{factory} {}

ObjectManager::~ObjectManager() {
  std::vector<EntryKey> route_ids;
  for (auto &r : routes_) {
    route_ids.emplace_back(r.first);
  }

  for (const auto &id : route_ids) {
    handle_delete_route(id);
  }
}

void ObjectManager::turn(const State state, const std::string &options) {
  if (state_ != state) {
    for (auto &pair : routes_) {
      pair.second->turn(state);
    }

    for (auto &pair : schemas_) {
      pair.second->turn(state);
    }
  }

  if (state == State::stateOn) {
    update_options(options);
  }

  state_ = state;
}

void ObjectManager::update(const std::vector<DbObject> &paths) {
  if (paths.size()) {
    log_debug("route-rest: Number of updated entries:%i", (int)paths.size());
  }

  for (const auto &p : paths) {
    log_debug("route-rest: Processing update id=%s", p.id.to_string().c_str());
    if (routes_.count(p.get_key())) {
      handle_existing_route(p);
    } else {
      handle_new_route(p);
    }
  }
}

void ObjectManager::update(const std::vector<ContentFile> &contents) {
  if (contents.size()) {
    log_debug("route-rest-static: Copy updates:%i",
              static_cast<int>(contents.size()));
  }

  std::vector<AppContentFile> copy;

  for (const auto &c : contents) {
    copy.emplace_back(c);
  }
  update(copy);
}

void ObjectManager::update(const std::vector<AppContentFile> &contents) {
  if (contents.size()) {
    log_debug("route-rest-static: Number of updated entries:%i",
              static_cast<int>(contents.size()));
  }

  for (const auto &p : contents) {
    log_debug("route-rest-static: Processing update id=%s",
              p.id.to_string().c_str());
    if (routes_.count(p.get_key())) {
      handle_existing_route(p);
    } else {
      handle_new_route(p);
    }
  }
}

void ObjectManager::handle_new_route(const AppContentFile &pe) {
  if (pe.deleted) return;

  auto schema = handle_schema(pe);
  auto route = factory_->create_router_static_object(pe, schema, cache_,
                                                     is_ssl_, auth_manager_);
  route->turn(state_);

  routes_.emplace(pe.get_key(), route);
}

void ObjectManager::handle_existing_route(const AppContentFile &pe) {
  if (pe.deleted) {
    handle_delete_route(pe.get_key());
    return;
  }

  log_debug("Updating static-file:%s", pe.id.to_string().c_str());

  auto schema = handle_schema(pe);
  auto &route = routes_[pe.get_key()];

  route->update(&pe, schema);
  route->turn(state_);
}

ObjectManager::RouteSchemaPtr ObjectManager::handle_schema(
    const ContentFile &pe) {
  auto s = schemas_.find(pe.schema_path);
  if (s != schemas_.end()) {
    return s->second;
  }

  if (pe.content_set_id == UniversalId()) return {};

  const std::string &option =
      (pe.options_json_schema.empty() ? pe.options_json_service
                                      : pe.options_json_schema);
  auto value = factory_->create_router_schema(
      this, cache_, pe.service_path, pe.schema_path, is_ssl_, pe.host,
      pe.requires_authentication, pe.service_id, pe.content_set_id, option,
      auth_manager_);

  value->turn(state_);

  schemas_[pe.schema_path] = value;

  return value;
}

void ObjectManager::handle_existing_route(const DbObject &pe) {
  if (pe.deleted) {
    handle_delete_route(pe.get_key());
    return;
  }

  log_debug("Updating rest-route:%s", pe.id.to_string().c_str());

  auto schema = handle_schema(pe);
  auto &route = routes_[pe.get_key()];

  route->update(&pe, schema);
  route->turn(state_);
}

void ObjectManager::handle_delete_route(const EntryKey &pe_id) {
  routes_.erase(pe_id);
}

void ObjectManager::handle_new_route(const DbObject &pe) {
  if (pe.deleted) return;
  auto schema = handle_schema(pe);
  auto route = factory_->create_router_object(pe, schema, cache_, is_ssl_,
                                              auth_manager_, gtid_manager_);

  route->turn(state_);

  routes_.emplace(pe.get_key(), route);
}

// TODO(lkotula): `schemas_` is not cleaned up from not referenced objects
// (Shouldn't be in review)
ObjectManager::RouteSchemaPtr ObjectManager::handle_schema(const DbObject &pe) {
  auto schema_full_path = pe.service_path + pe.schema_path;
  auto s = schemas_.find(schema_full_path);
  if (s != schemas_.end()) {
    return s->second;
  }

  const std::string &options =
      (pe.options_json_schema.empty() ? pe.options_json_service
                                      : pe.options_json_schema);

  auto value = factory_->create_router_schema(
      this, cache_, pe.service_path, pe.schema_path, is_ssl_, pe.host,
      pe.schema_requires_authentication, pe.service_id, pe.schema_id, options,
      auth_manager_);

  value->turn(state_);

  schemas_[schema_full_path] = value;

  return value;
}

void ObjectManager::schema_not_used(RouteSchema *route) {
  auto i = schemas_.find(route->get_full_path());

  if (i == schemas_.end()) return;

  schemas_.erase(i);
}

void ObjectManager::update_options(const std::string &options) {
  auto opt = parse_json_options(options);

  custom_paths_.clear();

  for (auto [k, v] : opt.default_content) {
    custom_paths_.push_back(
        std::make_shared<rest::HandlerString>(k, v, auth_manager_));
  }
}

void ObjectManager::clear() {
  routes_.clear();
  schemas_.clear();
  custom_paths_.clear();
}

}  // namespace mrs
