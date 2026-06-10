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

#include "rest_host_cache_entries.h"

#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include "mysqlrouter/host_cache_component.h"
#include "mysqlrouter/rest_api_utils.h"

bool RestHostCacheEntries::on_handle_request(
    http::base::Request &req, const std::string & /* base_path */,
    const std::vector<std::string> & /* path_matches */) {
  if (!ensure_no_params(req)) return true;

  auto &out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

    {
      auto &inst = HostCacheComponent::get_instance();
      auto hc_entries = inst.get_entries();
      auto hc_in_progress = inst.get_temporary_entries();

      rapidjson::Value entries(rapidjson::kObjectType);
      rapidjson::Value in_progress(rapidjson::kObjectType);

      using steady_clock = host_cache::Entry::steady_clock;
      const auto now = steady_clock::now();
      const std::chrono::seconds k_zero_seconds{0};
      for (const auto &e : hc_entries) {
        auto remaining_ttl = std::chrono::duration_cast<std::chrono::seconds>(
            e.ttl_ - (now - e.creation_time_));
        remaining_ttl =
            (remaining_ttl > k_zero_seconds) ? remaining_ttl : k_zero_seconds;
        rapidjson::Value object(rapidjson::kObjectType);

        object.AddMember("secondsRemainingTtl", remaining_ttl.count(),
                         allocator);
        object.AddMember<uint64_t>("ttl", e.ttl_.count(), allocator);
        object.AddMember<uint64_t>("cacheHits", e.cache_hits_.load(),
                                   allocator);
        object.AddMember<uint64_t>("singleFlight",
                                   e.resolve_waiters_peak_.load(), allocator);
        entries.AddMember(rapidjson::Value(e.hostname_.c_str(),
                                           e.hostname_.size(), allocator),
                          object.Move(), allocator);
      }

      for (const auto &e : hc_in_progress) {
        std::chrono::milliseconds age =
            std::chrono::duration_cast<std::chrono::seconds>(now -
                                                             e->creation_time_);
        rapidjson::Value object(rapidjson::kObjectType);

        object.AddMember<uint64_t>("ageMilliseconds", age.count(), allocator);
        object.AddMember<uint64_t>("consumers", e->resolve_waiters_peak_.load(),
                                   allocator);
        in_progress.AddMember(
            rapidjson::Value(e->host_entry_.hostname_.c_str(),
                             e->host_entry_.hostname_.size(), allocator),
            object.Move(), allocator);
      }

      json_doc.SetObject().AddMember("entries", entries.Move(), allocator);
      json_doc.AddMember("inProgress", in_progress.Move(), allocator);
    }
  }
  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
