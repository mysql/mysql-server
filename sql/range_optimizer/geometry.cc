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

#include "sql/range_optimizer/geometry.h"

#include <stddef.h>

#include "my_base.h"
#include "my_dbug.h"
#include "sql/handler.h"
#include "sql/range_optimizer/range_optimizer.h"

/* Get next for geometrical indexes */

int QUICK_RANGE_SELECT_GEOM::get_next() {
  DBUG_TRACE;

  for (;;) {
    int result;
    if (last_range) {
      // Already read through key
      result = file->ha_index_next_same(record, last_range->min_key,
                                        last_range->min_length);
      if (result != HA_ERR_END_OF_FILE) return result;
    }

    const size_t count = ranges.size() - (cur_range - ranges.begin());
    if (count == 0) {
      /* Ranges have already been used up before. None is left for read. */
      last_range = nullptr;
      return HA_ERR_END_OF_FILE;
    }
    last_range = *(cur_range++);

    result = file->ha_index_read_map(record, last_range->min_key,
                                     last_range->min_keypart_map,
                                     last_range->rkey_func_flag);
    if (result != HA_ERR_KEY_NOT_FOUND && result != HA_ERR_END_OF_FILE)
      return result;
    last_range = nullptr;  // Not found, to next range
  }
}
