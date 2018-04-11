#ifndef SQL_RECORDS_H
#define SQL_RECORDS_H
/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"

struct IO_CACHE;
class QEP_TAB;
class THD;
struct TABLE;

/**
  A context for reading through a single table using a chosen access method:
  index read, scan, etc, use of cache, etc.

  Use by:
@code
  READ_RECORD read_record;
  if (init_read_record(&read_record, ...))
    return true;
  while (read_record.read_record())
  {
    ...
  }
  end_read_record();
@endcode
*/

class QUICK_SELECT_I;

struct READ_RECORD {
  typedef int (*Read_func)(READ_RECORD *);
  typedef void (*Unlock_row_func)(QEP_TAB *);
  typedef int (*Setup_func)(QEP_TAB *);
  typedef void (*Cleanup_func)(READ_RECORD *);

  TABLE *table{nullptr};  /* Head-form */
  TABLE **forms{nullptr}; /* head and ref forms */
  Unlock_row_func unlock_row{nullptr};
  Read_func read_record{nullptr};
  Cleanup_func cleanup{nullptr};
  THD *thd{nullptr};
  QUICK_SELECT_I *quick{nullptr};
  uint cache_records{0};
  uint ref_length{0}, struct_length{0}, reclength{0}, rec_cache_size{0},
      error_offset{0};

  /**
    Counting records when reading result from filesort().
    Used when filesort leaves the result in the filesort buffer.
   */
  ha_rows unpack_counter{0};

  uchar *ref_pos{nullptr}; /* pointer to form->refpos */
  uchar *record{nullptr};
  uchar *rec_buf{nullptr}; /* to read field values  after filesort */
  uchar *cache{nullptr}, *cache_pos{nullptr}, *cache_end{nullptr},
      *read_positions{nullptr};
  IO_CACHE *io_cache{nullptr};
  bool ignore_not_found_rows{false};

 public:
  READ_RECORD() {}
};

bool init_read_record(READ_RECORD *info, THD *thd, TABLE *table,
                      QEP_TAB *qep_tab, bool disable_rr_cache);
bool init_read_record_idx(READ_RECORD *info, THD *thd, TABLE *table, uint idx,
                          bool reverse);
void end_read_record(READ_RECORD *info);

void rr_unlock_row(QEP_TAB *tab);
int rr_sequential(READ_RECORD *info);

#endif /* SQL_RECORDS_H */
