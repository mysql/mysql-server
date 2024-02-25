/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/ngs/thread.h"

#include <stdexcept>

#include "my_sys.h"     // my_thread_stack_size NOLINT(build/include_subdir)
#include "my_thread.h"  // NOLINT(build/include_subdir)

void ngs::thread_create(PSI_thread_key key [[maybe_unused]], Thread_t *thread,
                        Start_routine_t func, void *arg) {
  my_thread_attr_t connection_attrib;

  (void)my_thread_attr_init(&connection_attrib);
  /*
   check_stack_overrun() assumes that stack size is (at least)
   my_thread_stack_size. If it is smaller, we may segfault.
  */
  my_thread_attr_setstacksize(&connection_attrib, my_thread_stack_size);

  if (mysql_thread_create(key, thread, &connection_attrib, func, arg))
    throw std::runtime_error("Could not create a thread");
}

int ngs::thread_join(Thread_t *thread, void **ret) {
  return my_thread_join(thread, ret);
}
