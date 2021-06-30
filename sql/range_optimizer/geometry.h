/* Copyright (c) 2000, 2021, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_GEOMETRY_H_
#define SQL_RANGE_OPTIMIZER_GEOMETRY_H_

#include <sys/types.h>

#include "sql/range_optimizer/range_scan.h"

class THD;
struct MEM_ROOT;
struct TABLE;

class QUICK_RANGE_SELECT_GEOM : public QUICK_RANGE_SELECT {
 public:
  QUICK_RANGE_SELECT_GEOM(TABLE *table, uint index_arg,
                          MEM_ROOT *return_mem_root, uint mrr_flags_arg,
                          uint mrr_buf_size_arg, const KEY_PART *key,
                          Bounds_checked_array<QUICK_RANGE *> ranges_arg,
                          uint used_keyparts_arg)
      : QUICK_RANGE_SELECT(table, index_arg, return_mem_root, mrr_flags_arg,
                           mrr_buf_size_arg, key, ranges_arg,
                           used_keyparts_arg) {}
  int get_next() override;
};

#endif  // SQL_RANGE_OPTIMIZER_GEOMETRY_H_
