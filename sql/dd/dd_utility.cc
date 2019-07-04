/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/dd_utility.h"

#include "m_ctype.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

size_t normalize_string(const CHARSET_INFO *cs, const String_type &src,
                        char *normalized_str_buf,
                        size_t normalized_str_buf_length) {
  /*
    Count exact number of characters in the string and adjust output buffer
    size according to it to avoid my_strnxfrm() padding result buffer with
    spaces(sort weights corresponding to spaces).
  */
  size_t len = cs->coll->strnxfrmlen(
      cs, (cs->mbmaxlen *
           cs->cset->numchars(cs, src.c_str(), src.c_str() + src.length())));
  if (len > normalized_str_buf_length) return 0;

  /*
    Normalize the src.
    Store weights corresponding to each character of "src" in the result
    buffer.
  */
  my_strnxfrm(cs, reinterpret_cast<uchar *>(normalized_str_buf), len,
              reinterpret_cast<const uchar *>(src.c_str()), src.length());

  return len;
}

///////////////////////////////////////////////////////////////////////////

}  // namespace dd
