/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "tls_error.h"

#include <openssl/err.h>
#include <openssl/ssl.h>  // wolfssl defines ERR_get_error() in ssl.h

#include <deque>

static std::string ossl_to_str(const std::string &prefix) {
  std::deque<std::string> sections;

  while (ERR_peek_error()) {
    // workaround a bug in wolfssl 3.14 which returns BAD_STATE_E for
    // ERR_get_error() is the queue is empty by first peeking and then
    // pulling the error-code
    auto err = ERR_get_error();

    std::string section;
#if defined(LIBWOLFSSL_VERSION_HEX)
    // wolfssl has no ERR_func_error_string()
    const char *func_err_str = nullptr;
    char err_buf[120];  // at least 120 bytes long
    ERR_error_string_n(err, err_buf, sizeof(err_buf));
    const char *reason_err_str = err_buf;
#else
    const char *func_err_str = ERR_func_error_string(err);
    const char *reason_err_str = ERR_reason_error_string(err);
#endif

    if (func_err_str || reason_err_str) {
      section.append(func_err_str ? func_err_str : "");
      section.append("::");
      section.append(reason_err_str ? reason_err_str : "");
    } else {
      section.append("errcode=" + std::to_string(err));
    }
    sections.push_front(section);
  }

  std::string out(prefix);
  out += ": ";
  for (auto &section : sections) {
    out.append(section);
    out.append(" -> ");
  }
  return out;
}

TlsError::TlsError(const std::string &what)
    : std::runtime_error(ossl_to_str(what)) {}
