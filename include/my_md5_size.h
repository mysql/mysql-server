#ifndef MY_MD5_SIZE_INCLUDED
#define MY_MD5_SIZE_INCLUDED
/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file my_md5_size.h

  This is not part of md5.h, so that it can be included using C linkage,
  unlike that file.
*/

#define MD5_HASH_SIZE 16 /* Hash size in bytes */

#endif // MY_MD5_SIZE_INCLUDED
