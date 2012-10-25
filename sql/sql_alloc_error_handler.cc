/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved. 

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

#include "log.h"
#include "sql_class.h"
#include "mysqld.h"

extern "C" void sql_alloc_error_handler(void)
{
  THD *thd= current_thd;
  if (thd && !thd->is_error())
  {
    /*
      This thread is Out Of Memory.

      An OOM condition is a fatal error. It should not be caught by error
      handlers in stored procedures.

      Recording this SQL condition in the condition area could cause more
      memory allocations, which in turn could raise more OOM conditions,
      causing recursion in the error handling code itself. As a result,
      my_error() should not be invoked, and the thread Diagnostics Area is
      set to an error status directly.

      Note that Diagnostics_area::set_error_status() is safe, since it does
      not call any memory allocation routines.

      The visible result for a client application will be:
        - a query fails with an ER_OUT_OF_RESOURCES error, returned in the
          error packet.
        - SHOW ERROR/SHOW WARNINGS may be empty.
    */
    thd->get_stmt_da()->set_error_status(ER_OUT_OF_RESOURCES);
  }

  /* Skip writing to the error log to avoid mtr complaints */
  DBUG_EXECUTE_IF("simulate_out_of_memory", return;);

  sql_print_error("%s", ER(ER_OUT_OF_RESOURCES));
}
