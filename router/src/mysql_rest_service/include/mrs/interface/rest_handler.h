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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_REST_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_REST_HANDLER_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mrs/interface/http_result.h"
#include "mrs/interface/universal_id.h"

class HttpRequest;

namespace mysqlrouter {
class MySQLSession;
}  // namespace mysqlrouter

namespace mrs {

namespace http {
class Error;
}  // namespace http

namespace rest {
struct RequestContext;
}  // namespace rest

namespace interface {

class AuthorizeManager;

using Parameters = std::map<std::string, std::string>;

class ReqRes {
 public:
  bool header_{false};
  bool body_{false};
};

class Request : public ReqRes {
 public:
};

class Response : public ReqRes {
 public:
  bool detailed_errors_{false};
};

class Options {
 public:
  Options() {}

  Parameters parameters_;

  class AllowedOrigins {
   public:
    enum AllowedOriginsType { AllowNone, AllowAll, AllowSpecified };

    AllowedOriginsType type{AllowNone};
    std::vector<std::string> allowed_origins;
  } allowed_origins;

  class Debug {
   public:
    Debug() {}

    class Http {
     public:
      Http() {}
      Request request;
      Response response;
    } http;
    bool log_exceptions{false};
  } debug;

  class Metadata {
   public:
    Metadata() {}

    bool gtid{true};
  } metadata;

  class Query {
   public:
    uint64_t wait{1};
    bool embed_wait{false};
  } query;

  class Cache {
   public:
    bool gtid_cache{false};
  } cache;

  class Result {
   public:
    bool include_links{true};
    bool stored_procedure_nest_resultsets{false};
  } result;
};

class RestHandler {
 public:
  using SqlSession = mysqlrouter::MySQLSession;
  using RequestContext = mrs::rest::RequestContext;
  using Parameters = mrs::interface::Parameters;
  using HttpResult = mrs::interface::HttpResult;

  enum class Authorization { kNotNeeded, kRequires, kCheck };

 public:
  virtual ~RestHandler() = default;

  virtual bool may_check_access() const = 0;
  virtual Authorization requires_authentication() const = 0;

  virtual UniversalId get_service_id() const = 0;
  virtual UniversalId get_db_object_id() const = 0;
  virtual UniversalId get_schema_id() const = 0;
  virtual uint32_t get_access_rights() const = 0;
  virtual bool is_json_response() const { return true; }
  virtual const Options &get_options() const = 0;

  virtual void authorization(RequestContext *ctxt) = 0;
  virtual bool request_begin(RequestContext *ctxt) = 0;
  virtual void request_end(RequestContext *ctxt) = 0;
  virtual bool request_error(RequestContext *ctxt, const http::Error &e) = 0;
  virtual HttpResult handle_get(RequestContext *ctxt) = 0;
  virtual HttpResult handle_post(RequestContext *ctxt,
                                 const std::vector<uint8_t> &document) = 0;
  virtual HttpResult handle_delete(RequestContext *ctxt) = 0;
  virtual HttpResult handle_put(RequestContext *ctxt) = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_REST_HANDLER_H_
