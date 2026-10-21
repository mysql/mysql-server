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

#ifndef MYSQLROUTER_REST_SYNC_POINT_HANDLER_INCLUDED
#define MYSQLROUTER_REST_SYNC_POINT_HANDLER_INCLUDED

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include "mysql/harness/sync_point.h"
#include "mysqlrouter/rest_api_component.h"

class RestSyncPoint : public mysql_harness::SyncPoint {
 public:
  void wait() override;
  bool reached() const;
  void release();

 private:
  mutable std::mutex mutex_;
  std::condition_variable released_cv_;
  bool reached_{false};
  bool released_{false};
};

class RestSyncPointRegistry : public mysql_harness::SyncPointRegistry {
 public:
  std::shared_ptr<mysql_harness::SyncPoint> get(
      const std::string &name) override;
  std::shared_ptr<RestSyncPoint> get_rest(const std::string &name);

 private:
  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<RestSyncPoint>> sync_points_;
};

std::shared_ptr<RestSyncPointRegistry> sync_points();

class RestSyncPointStatus : public RestApiHandler {
 public:
  static constexpr const char path_regex[] = "^/test/sync-points/([^/]+)$";

  RestSyncPointStatus() : RestApiHandler({}, HttpMethod::Get) {}

  bool on_handle_request(http::base::Request &req, const std::string &base_path,
                         const std::vector<std::string> &path_matches) override;
};

class RestSyncPointRelease : public RestApiHandler {
 public:
  static constexpr const char path_regex[] =
      "^/test/sync-points/([^/]+)/release$";

  RestSyncPointRelease() : RestApiHandler({}, HttpMethod::Post) {}

  bool on_handle_request(http::base::Request &req, const std::string &base_path,
                         const std::vector<std::string> &path_matches) override;
};

#endif
