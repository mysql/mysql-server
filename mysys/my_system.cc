/* Copyright (c) 2024, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_system.cc Functions to retrieve system information like total
  Physical memory
*/

#include <cassert>
#include "my_config.h"
#include "my_dbug.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

unsigned long long my_physical_memory() {
  unsigned long long mem = 0ULL;
#ifdef _WIN32
  MEMORYSTATUSEX ms;
  ms.dwLength = sizeof(ms);
  GlobalMemoryStatusEx(&ms);
  mem = ms.ullTotalPhys;
#elif defined(HAVE_UNISTD_H) /* _WIN32 */
  long pages = sysconf(_SC_PHYS_PAGES);
  long pagesize = sysconf(_SC_PAGESIZE);
  if (pages > 0 && pagesize > 0) {
    mem = static_cast<unsigned long long>(pages * pagesize);
  }
#else
#error "Missing implementation of sysconf or GlobalMemoryStatusEx"
#endif /* HAVE_UNISTD_H */
  assert(mem != 0);
  return mem;
}
