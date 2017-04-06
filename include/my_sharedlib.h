/*
   Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MY_SHAREDLIB_INCLUDED
#define MY_SHAREDLIB_INCLUDED

/**
  @file include/my_sharedlib.h
  Functions related to handling of plugins and other dynamically loaded
  libraries.
*/

#if defined(_WIN32)
#define dlsym(lib, name) (void*)GetProcAddress((HMODULE)lib, name)
#define dlopen(libname, unused) LoadLibraryEx(libname, NULL, 0)
#define dlclose(lib) FreeLibrary((HMODULE)lib)
#define DLERROR_GENERATE(errmsg, error_number) \
  char win_errormsg[2048]; \
  if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, \
                   0, error_number, 0, win_errormsg, 2048, NULL)) \
  { \
    char *ptr; \
    for (ptr= &win_errormsg[0] + strlen(win_errormsg) - 1; \
         ptr >= &win_errormsg[0] && strchr("\r\n\t\0x20", *ptr); \
         ptr--) \
      *ptr= 0; \
    errmsg= win_errormsg; \
  } \
  else \
    errmsg= ""
#define dlerror() ""
#define dlopen_errno GetLastError()

#else /* _WIN32 */

#ifndef MYSQL_ABI_CHECK
#include <dlfcn.h>
#include <errno.h>
#endif

#define DLERROR_GENERATE(errmsg, error_number) errmsg= dlerror()
#define dlopen_errno errno
#endif /* _WIN32 */

/*
  Symbols declared as MYSQL_PLUGIN_API make up the interface between the server
  and plugins. When compiling the server, these are exported; when compiling a
  dynamic plugin, they are imported. When compiling something that is neither,
  they are neither exported nor imported (which is the default), so you should
  set MYSQL_NO_PLUGIN_EXPORT; otherwise, one can get into situations where one
  is including an otherwise unrelated header file that declares a symbol that is
  taken to be exported (and thus can't be pruned away during linking), which
  references a symbol from another module which wasn't meant to be linked in,
  causing a linker error.
*/

#if !defined(MYSQL_NO_PLUGIN_EXPORT) && (defined(MYSQL_DYNAMIC_PLUGIN) || defined(MYSQL_ABI_CHECK))
  #if defined(_WIN32)
    #define MYSQL_PLUGIN_API __declspec(dllimport)
  #else
    #define MYSQL_PLUGIN_API
  #endif
#elif !defined(MYSQL_NO_PLUGIN_EXPORT)
  // Don't export plugin symbols from libmysqlclient and standalone binaries.
  #if defined(_WIN32)
    #define MYSQL_PLUGIN_API __declspec(dllexport)
  #else
    #define MYSQL_PLUGIN_API __attribute__((visibility("default")))
  #endif
#else
  #define MYSQL_PLUGIN_API
#endif

/*
  These are symbols that are grandfathered as part of the API, ie., they were
  used by existing plugins at the time where the API was closed down. New code
  should not rely on using such symbols, but rather use the service and
  component APIs. Their use is strongly discouraged and may break at any time;
  if you want to test that your plugin does not depend on them, compile and
  test it with this #define set to the empty string.
*/
#define MYSQL_PLUGIN_LEGACY_API MYSQL_PLUGIN_API

#endif  // MY_SHAREDLIB_INCLUDED
