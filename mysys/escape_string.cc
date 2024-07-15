/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cstddef>

#include "mysql/strings/m_ctype.h"

/*
  Escape string with backslashes (\)

  SYNOPSIS
    escape_string_for_mysql()
    charset_info        Charset of the strings
    to                  Buffer for escaped string
    to_length           Length of destination buffer, or 0
    from                The string to escape
    length              The length of the string to escape

  DESCRIPTION
    This escapes the contents of a string by adding backslashes before special
    characters, and turning others into specific escape sequences, such as
    turning newlines into \n and null bytes into \0.

  NOTE
    To maintain compatibility with the old C API, to_length may be 0 to mean
    "big enough"

  RETURN VALUES
    (size_t) -1 The escaped string did not fit in the to buffer
    #           The length of the escaped string
*/

size_t escape_string_for_mysql(const CHARSET_INFO *charset_info, char *to,
                               size_t to_length, const char *from,
                               size_t length) {
  const char *to_start = to;
  const char *end = nullptr;
  const char *to_end = to_start + (to_length != 0 ? to_length - 1 : 2 * length);
  bool overflow = false;
  const bool use_mb_flag = use_mb(charset_info);
  for (end = from + length; from < end; from++) {
    char escape = 0;
    int tmp_length = 0;
    if (use_mb_flag && (tmp_length = my_ismbchar(charset_info, from, end))) {
      if (to + tmp_length > to_end) {
        overflow = true;
        break;
      }
      while (tmp_length--) *to++ = *from++;
      from--;
      continue;
    }
    /*
     If the next character appears to begin a multi-byte character, we
     escape that first byte of that apparent multi-byte character. (The
     character just looks like a multi-byte character -- if it were actually
     a multi-byte character, it would have been passed through in the test
     above.)

     Without this check, we can create a problem by converting an invalid
     multi-byte character into a valid one. For example, 0xbf27 is not
     a valid GBK character, but 0xbf5c is. (0x27 = ', 0x5c = \)
    */
    tmp_length = use_mb_flag ? my_mbcharlen_ptr(charset_info, from, end) : 0;
    if (tmp_length > 1)
      escape = *from;
    else
      switch (*from) {
        case 0: /* Must be escaped for 'mysql' */
          escape = '0';
          break;
        case '\n': /* Must be escaped for logs */
          escape = 'n';
          break;
        case '\r':
          escape = 'r';
          break;
        case '\\':
          escape = '\\';
          break;
        case '\'':
          escape = '\'';
          break;
        case '"': /* Better safe than sorry */
          escape = '"';
          break;
        case '\032': /* This gives problems on Win32 */
          escape = 'Z';
          break;
      }
    if (escape != 0) {
      if (to + 2 > to_end) {
        overflow = true;
        break;
      }
      *to++ = '\\';
      *to++ = escape;
    } else {
      if (to + 1 > to_end) {
        overflow = true;
        break;
      }
      *to++ = *from;
    }
  }
  *to = 0;
  return overflow ? static_cast<size_t>(-1)
                  : static_cast<size_t>(to - to_start);
}
