/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Declarations of different versions of my_strnxfrm_simple for
  performance testing. They cannot be defined in the test file,
  because gcc -O3 maybe smart enough to optimize them entirely away.
  So we put them in a separate compilation unit.
*/

#include "my_global.h"
#include "m_ctype.h"

namespace strnxfrm_unittest {

extern "C"
size_t
strnxfrm_orig(const CHARSET_INFO *cs,
              uchar *dst, size_t dstlen, uint nweights,
              const uchar *src, size_t srclen, uint flags);
extern "C"
size_t
strnxfrm_orig_unrolled(const CHARSET_INFO *cs,
                       uchar *dst, size_t dstlen, uint nweights,
                       const uchar *src, size_t srclen, uint flags);
extern "C"
size_t
strnxfrm_new(const CHARSET_INFO *cs,
             uchar *dst, size_t dstlen, uint nweights,
             const uchar *src, size_t srclen, uint flags);
extern "C"
size_t
strnxfrm_new_unrolled(const CHARSET_INFO *cs,
                      uchar *dst, size_t dstlen, uint nweights,
                      const uchar *src, size_t srclen, uint flags);

}
