/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/rest/handler_request_context.h"

namespace mrs {
namespace authentication {

bool WwwAuthenticationHandler::is_authorized(rest::RequestContext *ctxt) {
  if (!authorize(ctxt)) {
    ctxt->user.has_user_id = false;
    return false;
  }
  return true;
}

bool WwwAuthenticationHandler::authorize(rest::RequestContext *ctxt) {
  auto authorization = ctxt->request->get_input_headers().get(kAuthorization);
  auto args = mysql_harness::split_string(authorization, ' ', false);

  return www_authorize(args[1], &ctxt->sql_session_cache, &ctxt->user);
}

bool WwwAuthenticationHandler::unauthorize(rest::RequestContext *) {
  return true;
}

}  // namespace authentication
}  // namespace mrs
