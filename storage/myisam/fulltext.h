/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/**
  @file storage/myisam/fulltext.h
  Some definitions for full-text indices
*/

#include "ft_global.h"
#include "mysql/plugin_ftparser.h"
#include "storage/myisam/myisamdef.h"

#define HA_FT_WTYPE  HA_KEYTYPE_FLOAT
#define HA_FT_WLEN   4
#define FT_SEGS      2

/**
  Accessor methods for the weight and the number of subkeys in a buffer.

  The weight is of float type and subkeys number is of integer type. Both
  are stored in the same position of the buffer and the stored object is
  identified by the sign (bit): the weight value is positive whilst the
  number of subkeys is negative.

  In light of C's strict-aliasing rules, which roughly state that an object
  must not be accessed through incompatible types, these methods are used to
  avoid any problems arising from the type duality inside the buffer. The
  values are retrieved using a character type which can access any object.
*/
#define ft_sintXkorr(A)    mi_sint4korr(A)
#define ft_intXstore(T,A)  mi_int4store(T,A)
#define ft_floatXget(V,M)  mi_float4get(V,M)


extern const HA_KEYSEG ft_keysegs[FT_SEGS];

int  _mi_ft_cmp(MI_INFO *, uint, const uchar *, const uchar *);
int  _mi_ft_add(MI_INFO *, uint, uchar *, const uchar *, my_off_t);
int  _mi_ft_del(MI_INFO *, uint, uchar *, const uchar *, my_off_t);

uint _mi_ft_convert_to_ft2(MI_INFO *, uint, uchar *);

