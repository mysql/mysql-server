/*
  Copyright (c) 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with the
  program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "rest_sync_point_handler.h"

#include <rapidjson/document.h>

#include "mysqlrouter/rest_api_utils.h"

constexpr const char RestSyncPointStatus::path_regex[];
constexpr const char RestSyncPointRelease::path_regex[];

std::shared_ptr<RestSyncPointRegistry> sync_points() {
  static auto registry = std::make_shared<RestSyncPointRegistry>();
  return registry;
}

std::shared_ptr<mysql_harness::SyncPoint> RestSyncPointRegistry::get(
    const std::string &name) {
  return get_rest(name);
}

std::shared_ptr<RestSyncPoint> RestSyncPointRegistry::get_rest(
    const std::string &name) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto &point = sync_points_[name];
  if (!point) point = std::make_shared<RestSyncPoint>();
  return point;
}

void RestSyncPoint::wait() {
  std::unique_lock<std::mutex> lock(mutex_);
  reached_ = true;
  released_cv_.wait(lock, [this] { return released_; });
}

bool RestSyncPoint::reached() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return reached_;
}

void RestSyncPoint::release() {
  std::lock_guard<std::mutex> lock(mutex_);
  released_ = true;
  released_cv_.notify_all();
}

bool RestSyncPointStatus::on_handle_request(
    http::base::Request &req, const std::string &,
    const std::vector<std::string> &path_matches) {
  if (!ensure_no_params(req)) return true;

  rapidjson::Document doc;
  auto &allocator = doc.GetAllocator();
  const auto point = sync_points()->get_rest(path_matches[1]);
  doc.SetObject().AddMember("reached", point->reached(), allocator);
  send_json_document(req, HttpStatusCode::Ok, doc);
  return true;
}

bool RestSyncPointRelease::on_handle_request(
    http::base::Request &req, const std::string &,
    const std::vector<std::string> &path_matches) {
  if (!ensure_no_params(req)) return true;

  sync_points()->get_rest(path_matches[1])->release();
  rapidjson::Document doc;
  doc.SetObject();
  send_json_document(req, HttpStatusCode::Ok, doc);
  return true;
}
