/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#elif defined(unix) || defined(__unix__) || defined(__unix)
#include <unistd.h>
#endif

#if defined(__linux__)
#include <sys/sysinfo.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace shcore {

uint64_t getPhysicalMemorySize() {
  uint64_t memsize = 0;

#ifdef _WIN32
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  if (GlobalMemoryStatusEx(&memInfo) == 0) {
    return 0;
  }
  memsize = memInfo.ullTotalPhys;
#elif defined(__linux__)
  struct sysinfo info;
  if (sysinfo(&info) != 0) {
    return 0;
  }
  memsize = info.totalram;
#elif defined(__APPLE__)
  int mib[2];
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  u_int namelen = sizeof(mib) / sizeof(mib[0]);
  u_long size = sizeof(memsize);
  if (sysctl(mib, namelen, &memsize, &size, NULL, 0) != 0) {
    return 0;
  }
#endif

  return memsize;
}
}  // namespace shcore