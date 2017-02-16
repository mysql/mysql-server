#ifndef SQL_RECORDS_H
#define SQL_RECORDS_H 
/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                      /* gcc class implementation */
#endif
#include <my_global.h>                /* for uint typedefs */

struct st_join_table;
class handler;
struct TABLE;
class THD;
class SQL_SELECT;
class Copy_field;

/**
  A context for reading through a single table using a chosen access method:
  index read, scan, etc, use of cache, etc.

  Use by:
  READ_RECORD read_record;
  init_read_record(&read_record, ...);
  while (read_record.read_record())
  {
    ...
  }
  end_read_record();
*/

struct READ_RECORD
{
  typedef int (*Read_func)(READ_RECORD*);
  typedef void (*Unlock_row_func)(st_join_table *);
  typedef int (*Setup_func)(struct st_join_table*);

  TABLE *table;                                 /* Head-form */
  //handler *file;
  TABLE **forms;                                /* head and ref forms */
  Unlock_row_func unlock_row;
  Read_func read_record;
  THD *thd;
  SQL_SELECT *select;
  uint cache_records;
  uint ref_length,struct_length,reclength,rec_cache_size,error_offset;
  uint index;
  uchar *ref_pos;				/* pointer to form->refpos */
  uchar *record;
  uchar *rec_buf;                /* to read field values  after filesort */
  uchar	*cache,*cache_pos,*cache_end,*read_positions;
  struct st_io_cache *io_cache;
  bool print_error, ignore_not_found_rows;

  /* 
    SJ-Materialization runtime may need to read fields from the materialized
    table and unpack them into original table fields:
  */
  Copy_field *copy_field;
  Copy_field *copy_field_end;
public:
  READ_RECORD() {}
};

bool init_read_record(READ_RECORD *info, THD *thd, TABLE *reg_form,
		      SQL_SELECT *select, int use_record_cache,
                      bool print_errors, bool disable_rr_cache);
void init_read_record_idx(READ_RECORD *info, THD *thd, TABLE *table,
                          bool print_error, uint idx, bool reverse);
void end_read_record(READ_RECORD *info);

void rr_unlock_row(st_join_table *tab);

#endif /* SQL_RECORDS_H */
