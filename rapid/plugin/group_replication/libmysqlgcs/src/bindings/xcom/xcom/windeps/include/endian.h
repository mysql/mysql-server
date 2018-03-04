/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file windeps/include/endian.h
  Include only the necessary part of Sun RPC for Windows builds.
*/

#ifndef _ENDIAN_H
#define _ENDIAN_H   1

#if defined(_WIN32)
#include <winsock2.h>
#endif

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321

/* This is in no way shape or form portable */

#if(LITTLEENDIAN == 0x0001)
#define __BYTE_ORDER __LITTLE_ENDIAN
#define __FLOAT_WORD_ORDER __BYTE_ORDER

#else /* LITTLEENDIAN */

#if(BIGENDIAN == 0x0001)
#define __BYTE_ORDER __BIG_ENDIAN
#define __FLOAT_WORD_ORDER __BYTE_ORDER

#else
#error Can't make out endianness ...
#endif /* BIGENDIAN */

#endif /* LITTLEENDIAN */

#endif /* _ENDIAN_H */

