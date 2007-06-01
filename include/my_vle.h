/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA */

#ifndef VLE_H
#define VLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "my_global.h"

/*
  The size (in bytes) required to store the object ITEM, which can be
  either an expression or a type (since sizeof() is used on the item).
*/
#define my_vle_sizeof(ITEM) (((sizeof(ITEM) * CHAR_BIT) + 6) / 7)

uchar *my_vle_encode(uchar *vle, size_t max, ulong value);
uchar const *my_vle_decode(ulong *value_ptr, uchar const *vle);

#ifdef __cplusplus
}
#endif

#endif
