/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file windeps/include/libintl.h
  Include only the necessary part of Sun RPC for Windows builds.
*/

#ifndef _LIBINTL_H
#define _LIBINTL_H	1

/* This is a placeholder to make SUN RPC compile */

#if defined(WIN32) || defined(WIN64)
#include <winsock2.h>
#endif

#include <stdlib.h>
#include <malloc.h>

/* Need interantionallizaton */
#include "win_i18n.h"

#define __fxprintf(p, f, s) printf(p, f, s)

#endif /* _LIBINTL_H */

