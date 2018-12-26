/* Copyright (c) 2002, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/myisam/rt_mbr.h
*/

#ifndef _rt_mbr_h
#define _rt_mbr_h

#include <sys/types.h>

#include "my_compare.h"
#include "my_inttypes.h"
#include "myisam.h"

int rtree_key_cmp(HA_KEYSEG *keyseg, uchar *a, uchar *b, uint key_length,
                  uint nextflag);
int rtree_combine_rect(HA_KEYSEG *keyseg, uchar *, uchar *, uchar *,
                       uint key_length);
double rtree_rect_volume(HA_KEYSEG *keyseg, uchar *, uint key_length);
int rtree_d_mbr(HA_KEYSEG *keyseg, uchar *a, uint key_length, double *res);
double rtree_overlapping_area(HA_KEYSEG *keyseg, uchar *a, uchar *b,
                              uint key_length);
double rtree_area_increase(HA_KEYSEG *keyseg, uchar *a, uchar *b,
                           uint key_length, double *ab_area);
double rtree_perimeter_increase(HA_KEYSEG *keyseg, uchar *a, uchar *b,
                                uint key_length, double *ab_perim);
int rtree_page_mbr(MI_INFO *info, HA_KEYSEG *keyseg, uchar *page_buf, uchar *c,
                   uint key_length);
#endif /* _rt_mbr_h */
