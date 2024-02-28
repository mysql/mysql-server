/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

// enable using Rapidjson library with std::string
#define RAPIDJSON_HAS_STDSTRING 1

#include "my_rapidjson_size_t.h"

#include "mysqlrouter/mock_server_rest_client.h"

#include <rapidjson/document.h>
#include "mysqlrouter/rest_client.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <thread>

MockServerRestClient::MockServerRestClient(const uint16_t http_port,
                                           const std::string &http_hostname)
    : http_hostname_(http_hostname), http_port_(http_port) {}

void MockServerRestClient::set_globals(const std::string &globals_json) {
  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname_, http_port_);
  auto put_req = rest_client.request_sync(
      HttpMethod::Put, kMockServerGlobalsRestUri, globals_json);

  if (!put_req) {
    throw std::runtime_error(std::string("HTTP Request to ") + http_hostname_ +
                             ":" + std::to_string(http_port_) +
                             " failed (early): " + put_req.error_msg());
  }

  if (put_req.get_response_code() <= 0) {
    throw std::runtime_error(std::string("HTTP Request to ") + http_hostname_ +
                             ":" + std::to_string(http_port_) +
                             " failed: " + put_req.error_msg());
  }

  if (put_req.get_response_code() != 204) {
    throw std::runtime_error(
        std::string("Invalid response code from HTTP PUT request: ") +
        std::to_string(put_req.get_response_code()));
  }

  auto &put_resp_body = put_req.get_input_buffer();
  if ((put_resp_body.length() != 0u)) {
    throw std::runtime_error(
        std::string("Invalid response body length from HTTP PUT request: ") +
        std::to_string(put_resp_body.length()));
  }
}

std::string MockServerRestClient::get_globals_as_json_string() {
  IOContext io_ctx;
  auto req = RestClient(io_ctx, "127.0.0.1", http_port_)
                 .request_sync(HttpMethod::Get, kMockServerGlobalsRestUri);
  if (!req) {
    throw std::runtime_error(std::string("GET ") + kMockServerGlobalsRestUri +
                             " @ " + http_hostname_ + ":" +
                             std::to_string(http_port_) +
                             " failed (early): " + req.error_msg());
  }

  if (req.get_response_code() != 200) {
    throw std::runtime_error(
        std::string("Invalid response code from HTTP PUT request: ") +
        std::to_string(req.get_response_code()));
  }

  auto pvalue = req.get_input_headers().find("Content-Type");
  if (pvalue == nullptr || strcmp(pvalue->c_str(), "application/json") != 0) {
    throw std::runtime_error(std::string("Invalid response Conten-Type: ") +
                             (pvalue ? *pvalue : ""));
  }

  auto &resp_body = req.get_input_buffer();
  if (!(resp_body.length() > 0u)) {
    throw std::runtime_error(std::string("Invalid response buffer size: ") +
                             std::to_string(resp_body.length()));
  }
  auto resp_body_content = resp_body.pop_front(resp_body.length());

  std::string json_payload(resp_body_content.begin(), resp_body_content.end());
  return json_payload;
}

int MockServerRestClient::get_int_global(const std::string &global_name) {
  const auto json_payload = get_globals_as_json_string();

  rapidjson::Document json_doc;
  json_doc.Parse(json_payload);

  const auto it = json_doc.FindMember(global_name);

  if (it == json_doc.MemberEnd()) {
    throw std::runtime_error(std::string("Json payload does not have value: ") +
                             global_name + " payload: " + json_payload);
  }

  if (!it->value.IsInt()) {
    throw std::runtime_error(std::string("Invalid global type: ") +
                             std::to_string(it->value.GetType()) +
                             ", expected Int");
  }
  return it->value.GetInt();
}

bool MockServerRestClient::get_bool_global(const std::string &global_name) {
  const auto json_payload = get_globals_as_json_string();

  rapidjson::Document json_doc;
  json_doc.Parse(json_payload);

  const auto it = json_doc.FindMember(global_name);

  if (it == json_doc.MemberEnd()) {
    throw std::runtime_error(std::string("Json payload does not have value: ") +
                             global_name + " payload: " + json_payload);
  }

  if (!(it->value.IsBool())) {
    throw std::runtime_error(std::string("Invalid global type: ") +
                             std::to_string(it->value.GetType()) +
                             ", expected Bool");
  }
  return it->value.GetBool();
}

void MockServerRestClient::send_delete(const std::string &uri) {
  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname_, http_port_);
  auto kill_req = rest_client.request_sync(HttpMethod::Delete,
                                           "/api/v1/mock_server/connections/");

  if (!kill_req) {
    throw std::runtime_error(std::string("HTTP Delete Request on ") + uri +
                             " failed (early): " + kill_req.error_msg());
  }

  if (kill_req.get_response_code() != 200) {
    throw std::runtime_error(std::string("HTTP Delete Request on ") + uri +
                             " failed (invalid response code): " +
                             std::to_string(kill_req.get_response_code()));
  }

  if (kill_req.get_input_buffer().length() != 0u) {
    throw std::runtime_error(
        std::string("HTTP Delete Request on ") + uri +
        " failed (invalid buffer length): " +
        std::to_string(kill_req.get_input_buffer().length()));
  }
}

bool MockServerRestClient::wait_for_rest_endpoint_ready(
    std::chrono::milliseconds max_wait_time) const noexcept {
  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname_, http_port_);

  while (max_wait_time.count() > 0) {
    auto req =
        rest_client.request_sync(HttpMethod::Get, kMockServerGlobalsRestUri);

    if (req && req.get_response_code() != 0 && req.get_response_code() != 404)
      return true;

    auto wait_time =
        std::min(kMockServerMaxRestEndpointStepTime, max_wait_time);
    std::this_thread::sleep_for(wait_time);

    max_wait_time -= wait_time;
  }

  return false;
}
