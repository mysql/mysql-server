/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* This header file contains type declarations used by UCA code. */

#ifndef STR_UCA_TYPE_H
#define STR_UCA_TYPE_H

#include "my_inttypes.h"

/*
  So far we have only Croatian collation needs to reorder Latin and
  Cyrillic group of characters. May add more in future.
*/
#define UCA_MAX_CHAR_GRP 4
enum enum_uca_ver
{
  UCA_V400, UCA_V520, UCA_V900
};

enum enum_char_grp
{
  CHARGRP_NONE,
  CHARGRP_CORE,
  CHARGRP_LATIN,
  CHARGRP_CYRILLIC,
  CHARGRP_ARAB,
  CHARGRP_KANA,
  CHARGRP_OTHERS
};

struct Weight_boundary
{
  uint16  begin;
  uint16  end;
};

struct Reorder_wt_rec
{
  struct Weight_boundary old_wt_bdy;
  struct Weight_boundary new_wt_bdy;
};

struct Reorder_param
{
  enum enum_char_grp     reorder_grp[UCA_MAX_CHAR_GRP];
  struct Reorder_wt_rec  wt_rec[2 * UCA_MAX_CHAR_GRP];
  int                    wt_rec_num;
  uint16                 max_weight;
};

enum enum_case_first
{
  CASE_FIRST_OFF,
  CASE_FIRST_UPPER,
  CASE_FIRST_LOWER
};

struct Coll_param
{
  struct Reorder_param *reorder_param;
  bool                  norm_enabled; // false = normalization off, default;
                                      // true = on
  enum enum_case_first  case_first;
};

#endif
