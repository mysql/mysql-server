/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file os/os0proc.cc
 The interface to the operating system
 process control primitives

 Created 9/30/1995 Heikki Tuuri
 *******************************************************/

#include "my_config.h"

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include "ha_prototypes.h"
#include "os0proc.h"
#include "srv0srv.h"
#include "ut0byte.h"
#include "ut0mem.h"

/** The total amount of memory currently allocated from the operating
system with os_mem_alloc_large(). */
std::atomic<ulint> os_total_large_mem_allocated{0};

/** Whether to use large pages in the buffer pool */
bool os_use_large_pages;

/** Large page size. This may be a boot-time option on some platforms */
uint os_large_page_size;

/** Converts the current process id to a number.
@return process id as a number */
ulint os_proc_get_number(void) {
#ifdef _WIN32
  return (static_cast<ulint>(GetCurrentProcessId()));
#else
  return (static_cast<ulint>(getpid()));
#endif
}
