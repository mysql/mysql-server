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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_REST_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_REST_HANDLER_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "helper/media_type.h"
#include "mrs/http/error.h"

class HttpRequest;

namespace mysqlrouter {
class MySQLSession;
}  // namespace mysqlrouter

namespace mrs {
namespace rest {
struct RequestContext;
}  // namespace rest

namespace interface {

class AuthorizeManager;

class RestHandler {
 public:
  using SqlSession = mysqlrouter::MySQLSession;
  using RequestContext = rest::RequestContext;
  using Parameters = std::map<std::string, std::string>;

  struct Result {
    using Type = helper::MediaType;

    Result() {}
    Result(std::string &&r) : response{r} {}
    Result(std::string &&r, Type t, std::string e = {})
        : response{r}, type{t}, etag{std::move(e)} {}
    Result(HttpStatusCode::key_type s, std::string &&r, Type t,
           std::string e = {})
        : response{r}, status{s}, type{t}, etag{std::move(e)} {}
    Result(const std::string &r, Type t, std::string e = {})
        : response{r}, type{t}, etag{std::move(e)} {}
    Result(std::string &&r, std::string t, std::string e = {})
        : response{r}, type_text{t}, etag{std::move(e)} {}

    std::string response;
    HttpStatusCode::key_type status{HttpStatusCode::Ok};
    Type type{Type::typeDefault};
    std::optional<std::string> type_text;
    std::string etag;
  };

  enum class Authorization { kNotNeeded, kRequires, kCheck };

 public:
  virtual ~RestHandler() = default;

  virtual bool may_check_access() const = 0;
  virtual Authorization requires_authentication() const = 0;

  virtual uint64_t get_service_id() const = 0;
  virtual uint64_t get_db_object_id() const = 0;
  virtual uint64_t get_schema_id() const = 0;
  virtual uint32_t get_access_rights() const = 0;
  virtual const Parameters &get_headers_parameters() const = 0;
  virtual bool may_return_detailed_errors() const = 0;
  virtual bool is_json_response() const { return true; }

  virtual void authorization(RequestContext *ctxt) = 0;
  virtual bool request_begin(RequestContext *ctxt) = 0;
  virtual void request_end(RequestContext *ctxt) = 0;
  virtual bool request_error(RequestContext *ctxt, const http::Error &e) = 0;
  virtual Result handle_get(RequestContext *ctxt) = 0;
  virtual Result handle_post(RequestContext *ctxt,
                             const std::vector<uint8_t> &document) = 0;
  virtual Result handle_delete(RequestContext *ctxt) = 0;
  virtual Result handle_put(RequestContext *ctxt) = 0;
};
}  // namespace interface

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_REST_HANDLER_H_
