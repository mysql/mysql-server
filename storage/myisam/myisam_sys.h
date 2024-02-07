/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#ifndef MYISAM_SYS_INCLUDED
#define MYISAM_SYS_INCLUDED

#include "my_inttypes.h"
#include "my_io.h"

extern int my_lock(File fd, int op, myf MyFlags);

#define MY_REDEL_MAKE_BACKUP 256
#define MY_REDEL_NO_COPY_STAT 512 /* my_redel() doesn't call my_copystat() */

extern int my_redel(const char *from, const char *to, int MyFlags);

extern int my_copystat(const char *from, const char *to, int MyFlags);

#endif /* MYISAM_SYS_INCLUDED */
