/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_REST_METADATA_CLIENT_INCLUDED
#define MYSQLROUTER_REST_METADATA_CLIENT_INCLUDED

#include <chrono>
#include <functional>
#include <string>
#include <system_error>

enum class FetchErrorc {
  request_failed = 1,
  not_ok,
  unexpected_content_type,
  content_empty,
  parse_error,
  not_ready_yet,
  authentication_required
};

namespace std {
template <>
struct is_error_code_enum<FetchErrorc> : true_type {};
}  // namespace std

std::error_code make_error_code(FetchErrorc e);

struct FetchErrorCategory : std::error_category {
  const char *name() const noexcept override;
  std::string message(int ev) const override;
};

class RestMetadataClient {
 private:
  std::string hostname_;
  unsigned port_;
  std::string username_;
  std::string password_;

 public:
  struct MetadataStatus {
    uint64_t refresh_failed;
    uint64_t refresh_succeeded;
  };

  RestMetadataClient(const std::string &hostname, unsigned port,
                     const std::string &username = "",
                     const std::string &password = "")
      : hostname_{hostname},
        port_{port},
        username_{username},
        password_{password} {}

  std::error_code fetch(MetadataStatus &status);

  std::error_code wait_for_cache_fetched(
      std::chrono::milliseconds timeout, MetadataStatus &status,
      std::function<bool(const MetadataStatus &)> pred);

  std::error_code wait_until_cache_fetched(
      std::chrono::steady_clock::time_point tp, MetadataStatus &status,
      std::function<bool(const MetadataStatus &)> pred);

  std::error_code wait_for_cache_ready(std::chrono::milliseconds timeout,
                                       MetadataStatus &status);

  std::error_code wait_for_cache_changed(std::chrono::milliseconds timeout,
                                         MetadataStatus &status);

  std::error_code wait_for_cache_updated(std::chrono::milliseconds timeout,
                                         MetadataStatus &status);
};

#endif
