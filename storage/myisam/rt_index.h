/* Copyright (c) 2002, 2024, Oracle and/or its affiliates.

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

/**
  @file storage/myisam/rt_index.h
*/

#ifndef _rt_index_h
#define _rt_index_h

#include <sys/types.h>

#include "my_inttypes.h"
#include "myisam.h"

#define rt_PAGE_FIRST_KEY(page, nod_flag) (page + 2 + nod_flag)
#define rt_PAGE_NEXT_KEY(key, key_length, nod_flag) \
  (key + key_length + (nod_flag ? nod_flag : info->s->base.rec_reflength))
#define rt_PAGE_END(page) (page + mi_getint(page))

#define rt_PAGE_MIN_SIZE(block_length) ((uint)(block_length) / 3)

int rtree_insert(MI_INFO *info, uint keynr, uchar *key, uint key_length);
int rtree_delete(MI_INFO *info, uint keynr, uchar *key, uint key_length);

int rtree_find_first(MI_INFO *info, uint keynr, uchar *key, uint key_length,
                     uint search_flag);
int rtree_find_next(MI_INFO *info, uint keynr, uint search_flag);

int rtree_get_first(MI_INFO *info, uint keynr, uint key_length);
int rtree_get_next(MI_INFO *info, uint keynr, uint key_length);

ha_rows rtree_estimate(MI_INFO *info, uint keynr, uchar *key, uint key_length,
                       uint flag);

int rtree_split_page(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page, uchar *key,
                     uint key_length, my_off_t *new_page_offs);

#endif /* _rt_index_h */
