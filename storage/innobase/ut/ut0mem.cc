/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file ut/ut0mem.cc
 Memory primitives

 Created 5/11/1994 Heikki Tuuri
 *************************************************************************/

#include "ut0mem.h"
#include <stdlib.h>
#include "os0thread.h"
#include "srv0srv.h"

/** Copies up to size - 1 characters from the NUL-terminated string src to
 dst, NUL-terminating the result. Returns strlen(src), so truncation
 occurred if the return value >= size.
 @return strlen(src) */
ulint ut_strlcpy(char *dst,       /*!< in: destination buffer */
                 const char *src, /*!< in: source buffer */
                 ulint size)      /*!< in: size of destination buffer */
{
  ulint src_size = strlen(src);

  if (size != 0) {
    ulint n = std::min(src_size, size - 1);

    memcpy(dst, src, n);
    dst[n] = '\0';
  }

  return (src_size);
}

/********************************************************************
Concatenate 3 strings.*/
char *ut_str3cat(
    /* out, own: concatenated string, must be
    freed with ut::free() */
    const char *s1, /* in: string 1 */
    const char *s2, /* in: string 2 */
    const char *s3) /* in: string 3 */
{
  char *s;
  ulint s1_len = strlen(s1);
  ulint s2_len = strlen(s2);
  ulint s3_len = strlen(s3);

  s = static_cast<char *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                             s1_len + s2_len + s3_len + 1));

  memcpy(s, s1, s1_len);
  memcpy(s + s1_len, s2, s2_len);
  memcpy(s + s1_len + s2_len, s3, s3_len);

  s[s1_len + s2_len + s3_len] = '\0';

  return (s);
}
