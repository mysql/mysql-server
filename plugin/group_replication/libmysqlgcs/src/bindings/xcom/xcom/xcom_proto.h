/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef XCOM_PROTO_H
#define XCOM_PROTO_H

#include "x_platform.h"

static inline unsigned int get_32(unsigned char const *p) {
  return ((unsigned int)p[3] | (((unsigned int)p[2]) << 8) |
          (((unsigned int)p[1]) << 16) | (((unsigned int)p[0]) << 24));
}

static inline void put_32(unsigned char *p, unsigned int v) {
  p[3] = (unsigned char)((v)&0xff);
  p[2] = (unsigned char)(((v) >> 8) & 0xff);
  p[1] = (unsigned char)(((v) >> 16) & 0xff);
  p[0] = (unsigned char)(((v) >> 24) & 0xff);
}

static inline unsigned int get_16(unsigned char const *p) {
  return ((unsigned int)p[1] | (((unsigned int)p[0]) << 8));
}

static inline void put_16(unsigned char *p, unsigned int v) {
  p[1] = (unsigned char)((v)&0xff);
  p[0] = (unsigned char)(((v) >> 8) & 0xff);
}

#endif
