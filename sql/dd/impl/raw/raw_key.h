/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__RAW_KEY_INCLUDED
#define DD__RAW_KEY_INCLUDED

#include "my_base.h"    // key_part_map
#include "sql/sql_const.h" // MAX_KEY_LENGTH

namespace dd {

///////////////////////////////////////////////////////////////////////////

struct Raw_key
{
  uchar key[MAX_KEY_LENGTH];
  int index_no;
  int key_len;

  key_part_map keypart_map;

  Raw_key(int p_index_no, int p_key_len, key_part_map p_keypart_map)
   :index_no(p_index_no),
    key_len(p_key_len),
    keypart_map(p_keypart_map)
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__RAW_KEY_INCLUDED
