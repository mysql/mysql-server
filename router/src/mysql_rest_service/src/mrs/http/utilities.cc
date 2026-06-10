/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "mrs/http/utilities.h"

#include <cstring>

#include "mysql/harness/logging/logging.h"

#include "mrs/http/error.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace http {

HttpStatusCode::key_type redirect(HttpRequest *request, const char *url) {
  bool has_token = strstr(url, "?accessToken=") || strstr(url, "&accessToken=");
  log_debug("Redirection to '%s'", has_token ? "*****" : url);
  request->get_output_headers().add("Location", url);
  return HttpStatusCode::TemporaryRedirect;
}

void redirect_and_throw(HttpRequest *request, const char *url) {
  throw http::Error(redirect(request, url));
}

void redirect_and_throw(HttpRequest *request, const std::string &url) {
  redirect_and_throw(request, url.c_str());
}

}  // namespace http
}  // namespace mrs
