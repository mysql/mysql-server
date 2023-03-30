/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_STACKTRACE_H
#define NDB_STACKTRACE_H

#include "my_stacktrace.h"

#include <climits>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <link.h>
#endif

static unsigned long long ndb_get_program_base_address();

void ndb_init_stacktrace()
{
#ifdef HAVE_STACKTRACE
  my_init_stacktrace();
#endif
}

void ndb_print_stacktrace()
{
#ifdef HAVE_STACKTRACE
  my_safe_printf_stderr(
      "For help with below stacktrace consult:\n"
      "https://dev.mysql.com/doc/refman/en/using-stack-trace.html\n"
      "Also note that stack_bottom and thread_stack will always show up as zero.\n");

  unsigned long long base_address = ndb_get_program_base_address();
  if (base_address != 0 && base_address != ULLONG_MAX)
  {
    my_safe_printf_stderr(
        "Base address/slide: 0x%llx\n"
        "With use of addr2line, llvm-symbolizer, or, atos, subtract the "
        "addresses in\n"
        "stacktrace with the base address before passing them to tool.\n"
        "For tools that have options for slide use that, e.g.:\n"
        "llvm-symbolizer --adjust-vma=0x%llx ...\n"
        "atos -s 0x%llx ...\n",
        base_address,
        base_address,
        base_address);
  }

  my_print_stacktrace(nullptr, 0);
#endif
}

#if defined(__APPLE__)
unsigned long long ndb_get_program_base_address()
{
  return static_cast<unsigned long long>(_dyld_get_image_vmaddr_slide(0));
}
#elif defined(_WIN32)
unsigned long long ndb_get_program_base_address()
{
  return ULLONG_MAX;
}
#else
static int
ndb_get_program_base_address_callback(struct dl_phdr_info *info,
                                      size_t /* size */,
                                      void *data)
{
  unsigned long long *base_address = (unsigned long long *)data;
  *base_address = info->dlpi_addr;
  return 1; // End iteration after first module which is program.
}

unsigned long long ndb_get_program_base_address()
{
  unsigned long long base_address = ULLONG_MAX;
  void *callback_data = (void *)&base_address;
  dl_iterate_phdr(&ndb_get_program_base_address_callback, callback_data);
  return base_address;
}
#endif

#endif
