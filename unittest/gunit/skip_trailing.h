/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Declarations of different versions of skip_trailing_space for
  performance testing. They cannot be defined in the test file,
  because gcc -O3 maybe smart enough to optimize them entirely away.
  So we put them in a separate compilation unit.
*/

#include "my_global.h"
#include "m_ctype.h"

namespace skip_trailing_space_unittest {

extern const uchar *skip_trailing_orig(const uchar *ptr, size_t len);
extern const uchar *skip_trailing_unalgn(const uchar *ptr, size_t len);
extern const uchar *skip_trailing_4byte(const uchar *ptr, size_t len);
extern const uchar *skip_trailing_8byte(const uchar *ptr, size_t len);

}
