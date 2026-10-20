/* Copyright (c) 2016, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/check_stack.h"

#include "my_config.h"

#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <atomic>
#include <new>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/derror.h"
#include "sql/sql_class.h"

/****************************************************************************
        Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/

namespace {
int stack_direction = 0;
}

/**
  Determine whether the stack grows upward or downward.

  The function is deliberately not inlined, so @p a belongs to its
  caller's frame while @c b_marker belongs to this frame. In ordinary
  builds, resolve_stack_pointer() returns those local markers unchanged
  and their relative addresses reveal the stack-growth direction.

  In ASan builds, local variables may be allocated on ASan's fake stack.
  resolve_stack_pointer() therefore replaces both markers with physical
  frame addresses before comparing them. This keeps stack-direction
  detection in the same address space as later stack-overrun checks.

  @param a Stack marker from initialize_stack_direction().
  @returns 1 if the stack grows upward, otherwise 0.
*/
NO_INLINE
static int stack_direction_f(const char *a) {
  char b_marker = 0;
  const char *b = resolve_stack_pointer(&b_marker);
  if (b > a) {
    // printf("Stack grows upward\n");
    return 1;
  }
  // printf("Stack grows downward\n");
  return 0;
}

/*
 Prevent compiler optimizations by calling function
 through pointer.
*/
static int (*ptr_f)(const char *) = stack_direction_f;

NO_INLINE
int initialize_stack_direction() {
  int a = 0;
  const int retval = ptr_f(resolve_stack_pointer(&a));
  if (retval == 0) {
    stack_direction = -1;
  } else {
    stack_direction = 1;
  }
  return retval;
}

namespace {
long used_stack(const char *a_ptr, const char *b_ptr) {
  if (stack_direction < 0) return a_ptr - b_ptr;
  return b_ptr - a_ptr;
}
}  // namespace

#ifndef NDEBUG
static std::atomic<long> max_stack_used;
#endif

/**
  Check stack for a overrun

  @param thd            Thread handler.
  @param margin         Minimal acceptable unused space in the stack.
  @param buf            See a note below.

  @returns false if success, true if error (reported).

  @note
  Note: The 'buf' parameter is necessary, and we must have code which uses it.
  - Some of the fix_fields functions have a "dummy" buffer large enough for the
    corresponding execution. (Thus we only have to check in fix_fields.)
  - Passing the buffer to check_stack_overrun() prevents the compiler from
    removing it.
  - For -flto builds, the dummy buffer may be optimized away,
    so we need to write something into it.
*/
bool check_stack_overrun(const THD *thd, long margin, unsigned char *buf) {
  assert(thd == current_thd);
  assert(stack_direction == -1 || stack_direction == 1);

  if (!my_thread_equal(thd->real_id, my_thread_self())) return false;

  /*
    Compare the current stack location with the base captured when
    this THD was attached to the OS thread. In ordinary builds,
    stack_marker provides the conventional local-variable stack address.
    Under ASan, resolve_stack_pointer() replaces that potentially
    fake-stack address with a physical frame address, keeping both
    operands in the same address space.
  */
  char stack_marker = 0;
  const char *const resolved_stack_pointer =
      resolve_stack_pointer(&stack_marker);
  const long stack_used = used_stack(thd->stack_base(), resolved_stack_pointer);
  if (stack_used >= static_cast<long>(my_thread_stack_size - margin) ||
      DBUG_EVALUATE_IF("simulate_stack_overrun", true, false)) {
    // Touch the buffer, so that it is not optimized away by -flto.
    if (buf != nullptr) buf[0] = '\0';

    /*
      Do not use stack for the message buffer to ensure correct
      behaviour in cases we have close to no stack left.
    */
    char *ebuff = new (std::nothrow) char[MYSQL_ERRMSG_SIZE];
    if (ebuff) {
      snprintf(ebuff, MYSQL_ERRMSG_SIZE,
               ER_THD(thd, ER_STACK_OVERRUN_NEED_MORE), stack_used,
               my_thread_stack_size, margin);
      my_message(ER_STACK_OVERRUN_NEED_MORE, ebuff, MYF(ME_FATALERROR));
      delete[] ebuff;
    }
    return true;
  }
#ifndef NDEBUG
  max_stack_used = std::max(max_stack_used.load(), stack_used);
#endif
  return false;
}
