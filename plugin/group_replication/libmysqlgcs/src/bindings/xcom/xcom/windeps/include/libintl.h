/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
  @file windeps/include/libintl.h
  Include only the necessary part of Sun RPC for Windows builds.
*/

#ifndef _LIBINTL_H
#define _LIBINTL_H 1

/* This is a placeholder to make SUN RPC compile */

#if defined(_WIN32)
#include <winsock2.h>
#endif

#include <malloc.h>
#include <stdlib.h>

/* Need interantionallizaton */
#include "win_i18n.h"

#define __fxprintf(n, p, f, s) printf(p, f, s)

#endif /* _LIBINTL_H */
