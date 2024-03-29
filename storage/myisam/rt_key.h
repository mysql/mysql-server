/* Copyright (c) 2002, 2023, Oracle and/or its affiliates.

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

/* Written by Ramil Kalimullin, who has a shared copyright to this code */

/**
  @file storage/myisam/rt_key.h
*/

#ifndef _rt_key_h
#define _rt_key_h

#include <sys/types.h>

#include "my_inttypes.h"
#include "myisam.h"

int rtree_add_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
                  uint key_length, uchar *page_buf, my_off_t *new_page);
int rtree_delete_key(MI_INFO *info, uchar *page, uchar *key, uint key_length,
                     uint nod_flag);
int rtree_set_key_mbr(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
                      uint key_length, my_off_t child_page);

#endif /* _rt_key_h */
