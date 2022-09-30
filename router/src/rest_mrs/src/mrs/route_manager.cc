/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mrs/route_manager.h"

#include <vector>

#include "mysql/harness/logging/logging.h"

#include "mrs/route_factory.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {

RouteManager::RouteManager(
    collector::MysqlCacheManager *cache, const bool is_ssl,
    mrs::interface::AuthManager *auth_manager,
    std::shared_ptr<::mrs::interface::RouteFactory> factory)
    : cache_{cache},
      is_ssl_{is_ssl},
      auth_manager_{auth_manager},
      factory_{factory} {}

RouteManager::RouteManager(collector::MysqlCacheManager *cache,
                           const bool is_ssl,
                           mrs::interface::AuthManager *auth_manager)
    : RouteManager(cache, is_ssl, auth_manager,
                   std::make_shared<mrs::RouteFactory>()) {}

RouteManager::~RouteManager() {
  std::vector<EntryKey> route_ids;
  for (auto &r : routes_) {
    route_ids.emplace_back(r.first);
  }

  for (const auto id : route_ids) {
    handle_delete_route(id);
  }
}

void RouteManager::turn(const State state) {
  // TODO(lkotula): Mutex required (Shouldn't be in review)
  for (auto &pair : routes_) {
    pair.second->turn(state);
  }

  for (auto &pair : schemas_) {
    pair.second->turn(state);
  }

  state_ = state;
}

void RouteManager::update(const std::vector<DbObject> &paths) {
  if (paths.size()) {
    log_debug("route-rest: Number of updated entries:%i", (int)paths.size());
  }

  for (const auto &p : paths) {
    log_debug("route-rest: Processing update id=%i", (int)p.id);
    if (routes_.count({EntryType::key_rest, p.id})) {
      handle_existing_route(p);
    } else {
      handle_new_route(p);
    }
  }
}

void RouteManager::update(const std::vector<ContentFile> &contents) {
  if (contents.size()) {
    log_debug("route-rest-static: Number of updated entries:%i",
              (int)contents.size());
  }

  for (const auto &p : contents) {
    log_debug("route-rest-static: Processing update id=%i", (int)p.id);
    if (routes_.count(p.get_key())) {
      handle_existing_route(p);
    } else {
      handle_new_route(p);
    }
  }
}

void RouteManager::handle_new_route(const ContentFile &pe) {
  if (pe.deleted) return;

  auto schema = handle_schema(pe);
  auto route = factory_->create_router_static_object(pe, schema, cache_,
                                                     is_ssl_, auth_manager_);
  route->turn(state_);

  routes_.emplace(pe.get_key(), route);
}

void RouteManager::handle_existing_route(const ContentFile &pe) {
  if (pe.deleted) {
    handle_delete_route({EntryType::key_static, pe.id});
    return;
  }

  log_debug("Updating static-file:%i", (int)pe.id);

  auto schema = handle_schema(pe);
  auto &route = routes_[{EntryType::key_static, pe.id}];

  route->update(&pe, schema);
  route->turn(state_);
}

RouteManager::RouteSchemaPtr RouteManager::handle_schema(
    const ContentFile &pe) {
  auto s = schemas_.find(pe.schema_path);
  if (s != schemas_.end()) {
    return s->second;
  }

  auto value = factory_->create_router_schema(
      this, cache_, pe.service_path, pe.schema_path, is_ssl_, pe.host,
      pe.requires_authentication, pe.service_id, pe.content_set_id,
      auth_manager_);

  value->turn(state_);

  schemas_[pe.schema_path] = value;

  return value;
}

void RouteManager::handle_existing_route(const DbObject &pe) {
  if (pe.deleted) {
    handle_delete_route({EntryType::key_rest, pe.id});
    return;
  }

  log_debug("Updating rest-route:%i", (int)pe.id);

  auto schema = handle_schema(pe);
  auto &route = routes_[{EntryType::key_rest, pe.id}];

  route->update(&pe, schema);
  route->turn(state_);
}

void RouteManager::handle_delete_route(const EntryKey &pe_id) {
  routes_.erase(pe_id);
}

void RouteManager::handle_new_route(const DbObject &pe) {
  if (pe.deleted) return;
  auto schema = handle_schema(pe);
  auto route = factory_->create_router_object(pe, schema, cache_, is_ssl_,
                                              auth_manager_);

  route->turn(state_);

  routes_.emplace(pe.get_key(), route);
}

// TODO(lkotula): `schemas_` is not cleaned up from not referenced objects
// (Shouldn't be in review)
RouteManager::RouteSchemaPtr RouteManager::handle_schema(const DbObject &pe) {
  auto schema_full_path = pe.service_path + pe.schema_path;
  auto s = schemas_.find(schema_full_path);
  if (s != schemas_.end()) {
    return s->second;
  }

  auto value = factory_->create_router_schema(
      this, cache_, pe.service_path, pe.schema_path, is_ssl_, pe.host,
      pe.schema_requires_authentication, pe.service_id, pe.schema_id,
      auth_manager_);

  value->turn(state_);

  schemas_[schema_full_path] = value;

  return value;
}

void RouteManager::schema_not_used(RouteSchema *route) {
  auto i = schemas_.find(route->get_full_path());

  if (i == schemas_.end()) return;

  schemas_.erase(i);
}

}  // namespace mrs
