/*
   Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <host_info_config.h>

#define KB 1024
#define MB (KB*KB)

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* !WIN32_LEAN_AND_MEAN */
#include <Windows.h>
#endif /* WINDOWS_H */

#ifdef HAVE__SC_PHYS_PAGES
static double get_mem_sysconf() {
  return ((double)sysconf(_SC_PHYS_PAGES)) * ((double)sysconf(_A__SC_PAGESIZE)/MB);
}
#undef get_mem
#define get_mem get_mem_sysconf
#endif /* HAVE__SC_PHYS_PAGES */

#ifdef HAVE_GLOBALMEMORYSTATUS
static double get_mem_GlobalMemoryStatus() {
  MEMORYSTATUS ms;
  GlobalMemoryStatus(&ms);
  return ((double) ms.dwTotalPhys)/MB;
}
#undef get_mem
#define get_mem get_mem_GlobalMemoryStatus
#endif /* HAVE_GLOBALMEMORYSTATUS */

#ifdef HAVE_GLOBALMEMORYSTATUSEX
static double get_mem_GlobalMemoryStatusEx() {
  MEMORYSTATUSEX msx;
  msx.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&msx);
  return ((double)msx.ullTotalPhys)/MB;
}
#undef get_mem
#define get_mem get_mem_GlobalMemoryStatusEx
#endif /* HAVE_GLOBALMEMORYSTATUSEX */

#ifdef HAVE__SC_NPROCESSORS_CONF
static int get_cores_sysconf() {
  return sysconf(_SC_NPROCESSORS_CONF);
}
#undef get_cores
#define get_cores get_cores_sysconf
#endif /* HAVE__SC_N_PROCESSORS_CONF */

#ifdef HAVE_GETSYSTEMINFO
static int get_cores_GetSystemInfo() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);  
  return (int) si.dwNumberOfProcessors; 
}
#undef get_cores
#define get_cores get_cores_GetSystemInfo
#endif /* HAVE_GETSYSTEMINFO */


#include <stdio.h>
#include <string.h>


int main(int argc, char **argv) { 
  int r = 0;

  if (argc == 1) { 
    printf("{ \"ram\": %f, \"cores\": %d }\n",get_mem(),get_cores());
    goto finally;
  }
  if (strcmp(argv[1], "-m") == 0) {
    printf("%f\n", get_mem());
    goto finally;
  }
  if (strcmp(argv[1], "-c") == 0) {
    printf("%d\n", get_cores());
    goto finally;
  }

  /* usage: */
  printf("Usage: %s [-m|-c]\n", argv[0]);
  r = 1;

 finally:
  return r;
}




