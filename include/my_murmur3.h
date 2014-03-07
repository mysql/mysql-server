#ifndef MY_MURMUR3_INCLUDED
#define MY_MURMUR3_INCLUDED

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <my_global.h>

C_MODE_START

uint32 murmur3_32(const uchar * key, size_t len, uint32 seed);

C_MODE_END

#endif /* MY_MURMUR3_INCLUDED */
