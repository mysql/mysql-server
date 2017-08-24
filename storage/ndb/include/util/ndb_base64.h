/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#ifndef NDB_BASE64_H
#define NDB_BASE64_H

/*
  Interface created to be able to use base64 functions
  using function signatures which does not change between
  MySQL version
*/

#include "base64.h"

/*
  Decode a base64 string into data
*/
static inline
int ndb_base64_decode(const char *src, size_t src_len,
                      void *dst, const char **end_ptr)
{

  return base64_decode(src, src_len, dst, end_ptr
  // Signature of base64_decode changed to be extended
  // with a "flags" argument in 5.6.3, no flags needed for
  // vanilla base64_decode so ignore it in this impl.
                       , 0);
}

#endif
