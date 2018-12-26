#ifndef SQL_RECORDS_H
#define SQL_RECORDS_H 
/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>                /* for uint typedefs */
#include "my_base.h"

class QEP_TAB;
class handler;
struct TABLE;
class THD;

/**
  A context for reading through a single table using a chosen access method:
  index read, scan, etc, use of cache, etc.

  Use by:
@code
  READ_RECORD read_record;
  if (init_read_record(&read_record, ...))
    return TRUE;
  while (read_record.read_record())
  {
    ...
  }
  end_read_record();
@endcode
*/

class QUICK_SELECT_I;

struct READ_RECORD
{
  typedef int (*Read_func)(READ_RECORD*);
  typedef void (*Unlock_row_func)(QEP_TAB *);
  typedef int (*Setup_func)(QEP_TAB*);

  TABLE *table;                                 /* Head-form */
  TABLE **forms;                                /* head and ref forms */
  Unlock_row_func unlock_row;
  Read_func read_record;
  THD *thd;
  QUICK_SELECT_I *quick;
  uint cache_records;
  uint ref_length,struct_length,reclength,rec_cache_size,error_offset;

  /**
    Counting records when reading result from filesort().
    Used when filesort leaves the result in the filesort buffer.
   */
  ha_rows unpack_counter;

  uchar *ref_pos;				/* pointer to form->refpos */
  uchar *record;
  uchar *rec_buf;                /* to read field values  after filesort */
  uchar	*cache,*cache_pos,*cache_end,*read_positions;
  struct st_io_cache *io_cache;
  bool print_error, ignore_not_found_rows;

public:
  READ_RECORD() { memset(this, 0, sizeof(*this)); }
};

bool init_read_record(READ_RECORD *info, THD *thd,
                      TABLE *table, QEP_TAB *qep_tab,
		      int use_record_cache,
                      bool print_errors, bool disable_rr_cache);
bool init_read_record_idx(READ_RECORD *info, THD *thd, TABLE *table,
                          bool print_error, uint idx, bool reverse);
void end_read_record(READ_RECORD *info);

void rr_unlock_row(QEP_TAB *tab);
int rr_sequential(READ_RECORD *info);

#endif /* SQL_RECORDS_H */
