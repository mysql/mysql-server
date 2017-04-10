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

#include "sql/check_stack.h"

#include "my_config.h"

#include <algorithm>
#include <new>

#include "current_thd.h"
#include "derror.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_my_snprintf.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql_class.h"

/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/

#if STACK_DIRECTION < 0
#define used_stack(A,B) (long) (A - B)
#else
#define used_stack(A,B) (long) (B - A)
#endif

#ifndef DBUG_OFF
long max_stack_used;
#endif


/**
  Check stack for a overrun

  @param thd            Thread handler.
  @param margin         Minimal acceptable unused space in the stack.
  @param buf            See a note below.

  @returns false if success, true if error (reported).

  @note
  Note: The 'buf' parameter is necessary, even if it is unused here.
  - fix_fields functions has a "dummy" buffer large enough for the
    corresponding exec. (Thus we only have to check in fix_fields.)
  - Passing to check_stack_overrun() prevents the compiler from removing it.
*/
bool check_stack_overrun(const THD *thd, long margin,
			 unsigned char *buf MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(thd == current_thd);
  long stack_used= used_stack(thd->thread_stack,
                              reinterpret_cast<char*>(&stack_used));
  if (stack_used >= static_cast<long>(my_thread_stack_size - margin))
  {
    /*
      Do not use stack for the message buffer to ensure correct
      behaviour in cases we have close to no stack left.
    */
    char* ebuff= new (std::nothrow) char[MYSQL_ERRMSG_SIZE];
    if (ebuff) {
      my_snprintf(ebuff, MYSQL_ERRMSG_SIZE,
                  ER_THD(thd, ER_STACK_OVERRUN_NEED_MORE),
                  stack_used, my_thread_stack_size, margin);
      my_message(ER_STACK_OVERRUN_NEED_MORE, ebuff, MYF(ME_FATALERROR));
      delete [] ebuff;
    }
    return true;
  }
#ifndef DBUG_OFF
  max_stack_used= std::max(max_stack_used, stack_used);
#endif
  return false;
}
