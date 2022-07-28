/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

/*
  Needed since rpcgen expands macros itself, we cannot put
  this in the xcom_vp.x file directly.
 */

#ifndef XCOM_VP_PLATFORM_H
#define XCOM_VP_PLATFORM_H

/* Avoid warnings from the rpcgen */
#if defined(__GNUC__) || defined(__GNUG__)
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wundef"
#endif
#endif

#ifdef __APPLE__
#if __APPLE__

/* xdr_uint64_t and xdr_uint32_t are not defined on OSX */
#define xdr_uint64_t xdr_u_int64_t
#define xdr_uint32_t xdr_u_int32_t
#endif
#endif

#endif
