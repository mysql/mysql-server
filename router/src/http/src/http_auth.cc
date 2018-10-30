/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "http_auth.h"

#include <algorithm>
#include <string>

#include "http_auth_backend.h"
#include "http_auth_method_basic.h"
#include "matcher.h"

std::string HttpQuotedString::quote(const std::string &str) {
  std::string out;

  out.append("\"");
  for (const auto &c : str) {
    if (c == '"') {
      out += '\\';
      out += '"';
    } else if (c == '\\') {
      out += '\\';
      out += '\\';
    } else {
      out += c;
    }
  }
  out.append("\"");

  return out;
}

std::string HttpAuthChallenge::str() const {
  std::string out;

  out.append(scheme_);

  bool is_first = true;
  if (!token_.empty()) {
    out.append(" ");
    out.append(token_);
    is_first = false;
  }

  for (auto &kv : params_) {
    if (!is_first) {
      out.append(",");
    } else {
      out.append(" ");
    }
    out.append(kv.first);
    out.append("=");
    out.append(HttpQuotedString::quote(kv.second));

    is_first = false;
  }

  return out;
}

/**
 * match a TCHAR.
 *
 * @param c character to check
 * @returns success
 * @retval true if c is a TCHAR
 */
static bool is_tchar(char c) {
  return Matcher::Sor<Matcher::One<'!', '#', '$', '%', '&', '\'', '*', '+', '-',
                                   '.', '^', '_', '`', '|', '~'>,
                      Matcher::Alnum>::match(c);
}

/**
 * match a TOKEN68.
 *
 * @param c character to check
 * @returns success
 * @retval true if c is a TOKEN68
 */
static bool is_token68(char c) {
  return Matcher::Sor<Matcher::One<'+', '-', '.', '/', '=', '_', '~'>,
                      Matcher::Alnum>::match(c);
}

HttpAuthCredentials HttpAuthCredentials::from_header(const std::string &hdr,
                                                     std::error_code &errc) {
  if (hdr.empty()) {
    errc = std::make_error_code(std::errc::invalid_argument);
    return {{}, {}, {}};
  }
  // Basic dGVzdDoxMjPCow==
  auto begin_scheme = hdr.begin();
  auto end_scheme = std::find_if_not(hdr.begin(), hdr.end(), is_tchar);
  // stopped too late
  if (begin_scheme == end_scheme) {
    errc = std::make_error_code(std::errc::invalid_argument);
    return {{}, {}, {}};
  }

  std::string scheme(begin_scheme, end_scheme);
  std::string token;

  if (end_scheme != hdr.end()) {
    auto begin_sp = end_scheme;
    auto end_sp =
        std::find_if_not(end_scheme, hdr.end(), Matcher::One<' '>::match);

    if (begin_sp != end_sp) {
      // if there is a SP, we may also see a token
      auto begin_token = end_sp;
      auto end_token = std::find_if_not(begin_token, hdr.end(), is_token68);

      token = std::string(begin_token, end_token);
    }
  }

  // the RFC allows params after or instead of the token.
  // currently they are ignored. They should be added as soon as auth-method
  // is supported that needs them.

  return {scheme, token, {}};
}

std::string HttpAuthCredentials::str() const {
  std::string out;

  out.append(scheme_);
  out.append(" ");
  bool is_first = true;
  if (!token_.empty()) {
    out.append(token_);
    is_first = false;
  }

  for (auto &kv : params_) {
    if (!is_first) {
      out.append(",");
    }
    out.append(kv.first);
    out.append("=");
    out.append(HttpQuotedString::quote(kv.second));

    is_first = false;
  }

  return out;
}

bool HttpAuth::require_auth(HttpRequest &req,
                            std::shared_ptr<HttpAuthRealm> realm) {
  constexpr char kAuthorization[]{"Authorization"};
  constexpr char kWwwAuthenticate[]{"WWW-Authenticate"};
  constexpr char kMethodBasic[]{"Basic"};
  // enforce authentication
  auto authorization = req.get_input_headers().get(kAuthorization);

  auto out_hdrs = req.get_output_headers();

  // no Authorization, tell the client to authenticate
  if (authorization == nullptr) {
    out_hdrs.add(kWwwAuthenticate, HttpAuthChallenge(realm->method(), "",
                                                     {{"realm", realm->name()}})
                                       .str()
                                       .c_str());
    req.send_reply(HttpStatusCode::Unauthorized);
    return true;
  }

  // split Basic <...>
  std::error_code ec;
  auto creds = HttpAuthCredentials::from_header(authorization, ec);
  if (ec) {
    // parsing header failed
    req.send_reply(HttpStatusCode::BadRequest);
    return true;
  }

  if (creds.scheme() == kMethodBasic) {
    std::error_code ec;
    auto auth_data =
        HttpAuthMethodBasic::decode_authorization(creds.token(), ec);
    if (ec) {
      req.send_reply(HttpStatusCode::BadRequest);
      return true;
    }

    // we could log 'ec' with log_debug()
    if (/* auto ec = */ realm->authenticate(auth_data.username,
                                            auth_data.password)) {
      out_hdrs.add(
          kWwwAuthenticate,
          HttpAuthChallenge(realm->method(), "", {{"realm", realm->name()}})
              .str()
              .c_str());
      req.send_reply(HttpStatusCode::Unauthorized);
      return true;
    }
  } else {
    // we never announced something else
    req.send_reply(HttpStatusCode::BadRequest);
    return true;
  }

  return false;
}
