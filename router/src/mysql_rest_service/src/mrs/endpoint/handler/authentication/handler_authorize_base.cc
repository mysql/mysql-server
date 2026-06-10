/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/handler/authentication/handler_authorize_base.h"

#include "http/base/headers.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace endpoint {
namespace handler {

static void append_no_referrer(::http::base::Headers *headers) {
  if (!headers->find("Referrer-Policy")) {
    headers->add("Referrer-Policy", "no-referrer");
  }
}

void HandlerAuthorizeBase::request_end(RequestContext *ctxt) {
  append_no_referrer(&ctxt->request->get_output_headers());
}

bool HandlerAuthorizeBase::request_error(RequestContext *ctxt,
                                         const http::Error &) {
  append_no_referrer(&ctxt->request->get_output_headers());
  return false;
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
