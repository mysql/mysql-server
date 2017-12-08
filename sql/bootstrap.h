/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include "mysql/thread_type.h"              // enum_thread_type

struct MYSQL_FILE;

class THD;

namespace bootstrap {

/* Bootstrap handler functor */
typedef bool (*bootstrap_functor)(THD *thd);

/**
  Create a thread to execute all commands from the submitted file.
  By providing an explicit bootstrap handler functor, the default
  behavior of reading and executing SQL commands from the submitted
  file may be customized.

  @param file         File providing SQL statements, if non-NULL
  @param boot_handler Optional functor for customized handling
  @param thread_type  Bootstrap thread type.

  @return             Operation outcome, 0 if no errors
*/
bool run_bootstrap_thread(MYSQL_FILE *file, bootstrap_functor boot_handler,
                          enum_thread_type thread_type);
}

#endif // BOOTSTRAP_H
