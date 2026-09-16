/*
 * Copyright (c) 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_HELPER_SQL_LITERAL_ESCAPING_H_
#define PLUGIN_X_SRC_HELPER_SQL_LITERAL_ESCAPING_H_

#include <cassert>
#include <cstddef>

#include "mysql/strings/m_ctype.h"
#include "plugin/x/src/interface/sql_session.h"

namespace xpl {

inline bool is_no_backslash_escapes(iface::Sql_session *session) {
  assert(session != nullptr);
  return session->is_sql_mode_set("NO_BACKSLASH_ESCAPES");
}

inline std::size_t escape_sql_quote(const CHARSET_INFO *charset_info, char *to,
                                    std::size_t to_length, const char *from,
                                    std::size_t length, char quote) {
  const char *to_start = to;
  const char *to_end = to_start + (to_length != 0 ? to_length - 1 : 2 * length);
  const char *end = from + length;
  const bool use_mb_flag = use_mb(charset_info);
  bool overflow = false;

  for (; from < end; ++from) {
    unsigned mb_length = 0;
    if (use_mb_flag &&
        (mb_length = my_ismbchar(charset_info, from, end)) != 0) {
      if (to + mb_length > to_end) {
        overflow = true;
        break;
      }

      while (mb_length-- != 0) *to++ = *from++;
      --from;
      continue;
    }

    if (*from == quote) {
      if (to + 2 > to_end) {
        overflow = true;
        break;
      }
      *to++ = quote;
      *to++ = quote;
    } else {
      if (to + 1 > to_end) {
        overflow = true;
        break;
      }
      *to++ = *from;
    }
  }

  *to = '\0';
  return overflow ? static_cast<std::size_t>(-1)
                  : static_cast<std::size_t>(to - to_start);
}

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_SQL_LITERAL_ESCAPING_H_
