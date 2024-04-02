/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_REQUESTCONTEXT_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_REQUESTCONTEXT_H_

#include "collector/mysql_cache_manager.h"
#include "helper/http/url.h"
#include "http/base/request.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/http/cookie.h"
#include "mrs/http/header_accept.h"
#include "mrs/interface/authorize_handler.h"
#include "mrs/interface/authorize_manager.h"

namespace mrs {
namespace rest {

struct RequestContext {
  using SqlSessionCached = collector::MysqlCacheManager::CachedObject;
  using AuthUser = mrs::database::entry::AuthUser;
  using Url = helper::http::Url;
  using HeaderAccept = mrs::http::HeaderAccept;
  using Request = ::http::base::Request;
  using Headers = ::http::base::Headers;

  RequestContext(interface::AuthorizeManager *auth_manager = nullptr)
      : auth_manager_{auth_manager} {}
  RequestContext(Request *r,
                 interface::AuthorizeManager *auth_manager = nullptr)
      : request{r},
        auth_manager_{auth_manager},
        accepts{get_in_headers().find_cstr("Accept")} {}

  Request *request{nullptr};
  http::Cookie cookies{request};
  SqlSessionCached sql_session_cache;
  interface::AuthorizeManager *auth_manager_;
  std::shared_ptr<interface::AuthorizeHandler> selected_handler;

  HeaderAccept accepts;
  AuthUser user;
  bool post_authentication{false};

  Url get_http_url() { return Url{request->get_uri()}; }
  const Headers &get_in_headers() { return request->get_input_headers(); }
};

}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_REQUESTCONTEXT_H_
