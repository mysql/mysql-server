/* Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _sql_cursor_h_
#define _sql_cursor_h_

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                              /* gcc class interface */
#endif

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
  select_result *result;
public:
  Server_side_cursor(MEM_ROOT *mem_root_arg, select_result *result_arg)
    :Query_arena(mem_root_arg, STMT_INITIALIZED), result(result_arg)
  {}

  virtual bool is_open() const= 0;

  virtual int open(JOIN *top_level_join)= 0;
  virtual void fetch(ulong num_rows)= 0;
  virtual void close()= 0;
  virtual ~Server_side_cursor();

  static void operator delete(void *ptr, size_t size);
};


int mysql_open_cursor(THD *thd, select_result *result,
                      Server_side_cursor **res);

#endif /* _sql_cusor_h_ */
