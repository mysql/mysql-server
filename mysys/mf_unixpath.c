/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "mysys_priv.h"
#include <m_string.h>

/**
  Convert filename to unix style filename.

  @remark On Windows, converts '\' to '/'.

  @param to   A pathname.
*/

void to_unix_path(char *to __attribute__((unused)))
{
#if FN_LIBCHAR != '/'
  {
    to--;
    while ((to=strchr(to+1,FN_LIBCHAR)) != 0)
      *to='/';
  }
#endif
}
