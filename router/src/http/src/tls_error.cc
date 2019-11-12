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
#include <openssl/ssl.h>

#include <deque>

static std::string ossl_to_str(const std::string &prefix) {
  std::deque<std::string> sections;

  while (ERR_peek_error()) {
    auto err = ERR_get_error();

    std::string section;
    const char *func_err_str = ERR_func_error_string(err);
    const char *reason_err_str = ERR_reason_error_string(err);

    if (func_err_str || reason_err_str) {
      section.append(func_err_str ? func_err_str : "");
      section.append("::");
      section.append(reason_err_str ? reason_err_str : "");
    } else {
      section.append("errcode=" + std::to_string(err) +
                     // comment to make clang-format happy
                     " (lib=" + std::to_string(ERR_GET_LIB(err)) + ")" +
                     " (func=" + std::to_string(ERR_GET_FUNC(err)) + ")" +
                     " (reason=" + std::to_string(ERR_GET_REASON(err)) + ")");
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
