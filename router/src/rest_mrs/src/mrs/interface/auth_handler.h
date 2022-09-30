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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HANDLER_H_

#include "collector/mysql_cache_manager.h"
#include "mrs/database/entry/auth_user.h"

#include "mysqlrouter/http_request.h"

namespace mrs {
namespace rest {
struct RequestContext;
}

namespace interface {

class AuthHandler {
 public:
  using Cached = collector::MysqlCacheManager::CachedObject;
  using AuthUser = mrs::database::entry::AuthUser;
  using RequestContext = rest::RequestContext;

 public:
  virtual ~AuthHandler() = default;

  virtual uint64_t get_service_id() const = 0;
  virtual uint64_t get_id() const = 0;

  virtual bool can_process(HttpRequest *request) = 0;
  virtual void mark_response(HttpRequest *request) = 0;

  virtual bool is_authorized(rest::RequestContext *ctxt) = 0;
  virtual bool authorize(rest::RequestContext *ctxt) = 0;
  virtual bool unauthorize(rest::RequestContext *ctxt) = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HANDLER_H_
