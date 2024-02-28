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

#include "mysqlrouter/rest_api_utils.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "http/base/request.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/component/http_auth_realm_component.h"
#include "mysqlrouter/component/http_server_auth.h"

void send_json_document(http::base::Request &req,
                        HttpStatusCode::key_type status_code,
                        const rapidjson::Document &json_doc) {
  // serialize json-document into a string
  rapidjson::StringBuffer json_buf;

  {
    rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);

    json_doc.Accept(json_writer);

  }  // free json_doc and json_writer early

  req.send_reply(status_code,
                 HttpStatusCode::get_default_status_text(status_code),
                 {json_buf.GetString(), json_buf.GetSize()});
}

void send_rfc7807_error(http::base::Request &req,
                        HttpStatusCode::key_type status_code,
                        const std::map<std::string, std::string> &fields) {
  auto &out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/problem+json");

  rapidjson::Document json_doc;

  auto &allocator = json_doc.GetAllocator();

  json_doc.SetObject();
  for (const auto &field : fields) {
    json_doc.AddMember(
        rapidjson::Value(field.first.c_str(), field.first.size(), allocator),
        rapidjson::Value(field.second.c_str(), field.second.size(), allocator),
        allocator);
  }

  json_doc.AddMember("status", status_code, allocator);

  send_json_document(req, status_code, json_doc);
}

void send_rfc7807_not_found_error(http::base::Request &req) {
  send_rfc7807_error(req, HttpStatusCode::NotFound,
                     {
                         {"title", "URI not found"},
                         {"instance", req.get_uri().get_path()},
                     });
}

bool ensure_http_method(http::base::Request &req,
                        HttpMethod::Bitset allowed_methods) {
  if ((HttpMethod::Bitset(req.get_method()) & allowed_methods).any())
    return true;

  std::vector<std::string> allowed_method_names;
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Put)).any())
    allowed_method_names.push_back("PUT");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Get)).any())
    allowed_method_names.push_back("GET");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Head)).any())
    allowed_method_names.push_back("HEAD");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Post)).any())
    allowed_method_names.push_back("POST");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Trace)).any())
    allowed_method_names.push_back("TRACE");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Connect)).any())
    allowed_method_names.push_back("CONNECT");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Patch)).any())
    allowed_method_names.push_back("PATCH");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Options)).any())
    allowed_method_names.push_back("OPTIONS");
  if ((allowed_methods & HttpMethod::Bitset(HttpMethod::Delete)).any())
    allowed_method_names.push_back("DELETE");

  auto &out_hdrs = req.get_output_headers();
  out_hdrs.add("Allow", mysql_harness::join(allowed_method_names, ",").c_str());

  send_rfc7807_error(
      req, HttpStatusCode::MethodNotAllowed,
      {
          {"title", "HTTP Method not allowed"},
          {"detail", "only HTTP Methods " +
                         mysql_harness::join(allowed_method_names, ",") +
                         " are supported"},
      });

  return false;
}

bool ensure_auth(http::base::Request &req, const std::string require_realm) {
  if (!require_realm.empty()) {
    if (auto realm =
            HttpAuthRealmComponent::get_instance().get(require_realm)) {
      if (HttpAuth::require_auth(req, realm)) {
        // auth wasn't successful, response already sent
        return false;
      }

      // access granted, fall through
    }
  }

  return true;
}

bool ensure_no_params(http::base::Request &req) {
  if (!req.get_uri().get_query().empty()) {
    send_rfc7807_error(req, HttpStatusCode::BadRequest,
                       {
                           {"title", "validation error"},
                           {"detail", "parameters not allowed"},
                       });
    return false;
  }

  return true;
}

bool ensure_modified_since(http::base::Request &req, time_t last_modified) {
  if (!req.is_modified_since(last_modified)) {
    req.send_reply(HttpStatusCode::NotModified);
    return false;
  }

  req.add_last_modified(last_modified);

  return true;
}
