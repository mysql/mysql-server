/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BUILD_ID_INCLUDED
#define BUILD_ID_INCLUDED

#include "my_config.h"

#ifdef HAVE_BUILD_ID_SUPPORT

/*
  Get 160-bit SHA1 build-id, convert it to a hex string, and store it in dst.
  Assumes we have linked with -Wl,--build-id=sha1
  The dst buffer should have space for at least 41 bytes,
  including the terminating null byte.
 */
bool my_find_build_id(char *dst);

#endif  // HAVE_BUILD_ID_SUPPORT

#endif  // BUILD_ID_INCLUDED
