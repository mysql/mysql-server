/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include <iomanip>
#include <thread>

#include "rest_metadata_client.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"

std::ostream &operator<<(std::ostream &os,
                         const std::chrono::milliseconds &duration) {
  return os << duration.count() << "ms";
}

std::ostream &operator<<(std::ostream &os,
                         const decltype(std::chrono::steady_clock::now()) &tp) {
  std::chrono::milliseconds ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          tp.time_since_epoch());

  std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ms);
  std::time_t t = s.count();
  auto frac = ms.count() % 1000;  // just to be sure

  return os << t << "." << std::setfill('0') << std::setw(3) << frac;
}

const char *FetchErrorCategory::name() const noexcept { return "fetch"; }

std::string FetchErrorCategory::message(int ev) const {
  switch (static_cast<FetchErrorc>(ev)) {
    case FetchErrorc::request_failed:
      return "HTTP request failed without an error-code";
    case FetchErrorc::authentication_required:
      return "Authentication Required";
    case FetchErrorc::not_ok:
      return "REST request failed permanently";
    case FetchErrorc::content_empty:
      return "HTTP response is empty";
    case FetchErrorc::unexpected_content_type:
      return "unexpected content-type";
    case FetchErrorc::parse_error:
      return "document failed to parse";
    case FetchErrorc::not_ready_yet:
      return "service not available yet";
    default:
      return std::to_string(ev);
  }
}

const FetchErrorCategory theFetchErrorCategory{};

std::error_code make_error_code(FetchErrorc e) {
  return {static_cast<int>(e), theFetchErrorCategory};
}

std::error_code RestMetadataClient::fetch(
    RestMetadataClient::MetadataStatus &status) {
  IOContext io_ctx;
  const std::string url =
      std::string("/api/") + kRestAPIVersion + "/metadata/test/status";

  RestClient rest_client(io_ctx, hostname_, port_, username_, password_);

  HttpRequest get_req = rest_client.request_sync(HttpMethod::Get, url);

  if (!get_req) {
    return make_error_code(FetchErrorc::request_failed);
  }

  switch (get_req.get_response_code()) {
    case 200u:
      break;
    case 404u:
    case 503u:
      return make_error_code(FetchErrorc::not_ready_yet);
    case 401u:
      return make_error_code(FetchErrorc::authentication_required);
    default:
      return make_error_code(FetchErrorc::not_ok);
  }

  if (get_req.get_input_headers().get("Content-Type") !=
      std::string("application/json")) {
    return make_error_code(FetchErrorc::unexpected_content_type);
  }

  auto resp_buffer = get_req.get_input_buffer();

  size_t content_length = resp_buffer.length();
  if (content_length == 0u) {
    return make_error_code(FetchErrorc::content_empty);
  }

  auto json_content = resp_buffer.pop_front(content_length);

  std::string json_doc_content{json_content.begin(), json_content.end()};

  rapidjson::Document doc;
  if (doc.Parse(json_doc_content.c_str()).HasParseError()) {
    return make_error_code(FetchErrorc::parse_error);
  }

  // validate server response against json-schema to allow simple
  // access later
  const char metadata_schema[] =
      "{\n"
      "  \"type\": \"object\",\n"
      "  \"properties\": {\n"
      "     \"refreshSucceeded\": {\n"
      "       \"type\": \"integer\"\n"
      "     },\n"
      "     \"refreshFailed\": {\n"
      "       \"type\": \"integer\"\n"
      "     }\n"
      "  }, \"required\": [\"refreshFailed\", \"refreshSucceeded\"]\n"
      "}\n";
  rapidjson::Document schema_doc;
  if (schema_doc.Parse(metadata_schema).HasParseError()) {
    return make_error_code(FetchErrorc::parse_error);
  }

  rapidjson::SchemaDocument schema(schema_doc);
  rapidjson::SchemaValidator validator(schema);

  if (!doc.Accept(validator)) {
    return make_error_code(FetchErrorc::parse_error);
  }

  // input is all valid, fields can be accessed safely
  status = RestMetadataClient::MetadataStatus{
      doc["refreshFailed"].GetUint64(), doc["refreshSucceeded"].GetUint64()};

  return {};  // no error
}

std::error_code RestMetadataClient::wait_until_cache_fetched(
    std::chrono::steady_clock::time_point end_tp, MetadataStatus &status,
    std::function<bool(const MetadataStatus &)> pred) {
  while (std::chrono::steady_clock::now() < end_tp) {
    MetadataStatus cur_status;
    std::error_code ec = fetch(cur_status);
    if (ec) {
      if (ec != make_error_code(FetchErrorc::not_ready_yet)) {
        // not_ready_yet is hopefully a temporary error, retry
        return ec;
      }
    } else if (pred(cur_status)) {
      status = cur_status;

      return {};
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return make_error_code(std::errc::timed_out);
}

std::error_code RestMetadataClient::wait_for_cache_fetched(
    std::chrono::milliseconds timeout, MetadataStatus &status,
    std::function<bool(const MetadataStatus &)> pred) {
  return wait_until_cache_fetched(std::chrono::steady_clock::now() + timeout,
                                  status, pred);
}

std::error_code RestMetadataClient::wait_for_cache_ready(
    std::chrono::milliseconds timeout, MetadataStatus &status) {
  return wait_for_cache_fetched(timeout, status,
                                [](const MetadataStatus &cur_status) {
                                  return cur_status.refresh_succeeded > 0;
                                });
}

std::error_code RestMetadataClient::wait_for_cache_changed(
    std::chrono::milliseconds timeout, MetadataStatus &status) {
  MetadataStatus before;

  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  auto end_tp = std::chrono::steady_clock::now() + timeout;

  std::error_code ec = wait_until_cache_fetched(
      end_tp, before, [](const MetadataStatus &) { return true; });

  if (ec) return ec;

  MetadataStatus after;

  ec = wait_until_cache_fetched(
      end_tp, after, [before](const MetadataStatus &cur_status) {
        return (before.refresh_succeeded != cur_status.refresh_succeeded) ||
               (before.refresh_failed != cur_status.refresh_failed);
      });

  if (ec) return ec;

  status = after;
  return {};
}

std::error_code RestMetadataClient::wait_for_cache_updated(
    std::chrono::milliseconds timeout, MetadataStatus &status) {
  MetadataStatus before;

  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  auto end_tp = std::chrono::steady_clock::now() + timeout;

  std::error_code ec = wait_until_cache_fetched(
      end_tp, before, [](const MetadataStatus &) { return true; });

  if (ec) return ec;

  MetadataStatus after;

  ec = wait_until_cache_fetched(
      end_tp, after, [before](const MetadataStatus &cur_status) {
        return before.refresh_succeeded != cur_status.refresh_succeeded;
      });

  if (ec) return ec;

  status = after;

  return {};
}
