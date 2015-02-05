/* Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _sql_cursor_h_
#define _sql_cursor_h_

#include "sql_class.h"                          /* Query_arena */

class JOIN;

/**
  @file

  Declarations for implementation of server side cursors. Only
  read-only non-scrollable cursors are currently implemented.
*/

/**
  Server_side_cursor -- an interface for materialized
  implementation of cursors. All cursors are self-contained
  (created in their own memory root).  For that reason they must
  be deleted only using a pointer to Server_side_cursor, not to
  its base class.
*/

class Server_side_cursor: protected Query_arena, public Sql_alloc
{
protected:
  /** Row destination used for fetch */
  Query_result *result;
public:
  Server_side_cursor(MEM_ROOT *mem_root_arg, Query_result *result_arg)
    :Query_arena(mem_root_arg, STMT_INITIALIZED), result(result_arg)
  {}

  virtual bool is_open() const= 0;

  virtual int open(JOIN *top_level_join)= 0;
  virtual bool fetch(ulong num_rows)= 0;
  virtual void close()= 0;
  virtual ~Server_side_cursor();

  static void operator delete(void *ptr, size_t size);
};


bool mysql_open_cursor(THD *thd, Query_result *result,
                       Server_side_cursor **res);

#endif /* _sql_cusor_h_ */
