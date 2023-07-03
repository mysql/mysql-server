/*
 Copyright (c) 2012, 2022, Oracle and/or its affiliates.
 
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

#ifndef NODEJS_ADAPTER_INCLUDE_UNIFIED_DEBUG_H
#define NODEJS_ADAPTER_INCLUDE_UNIFIED_DEBUG_H

#include <assert.h>

/* Unified debugging library for C++ and JavaScript. 
   JavaScript code can control debugging output. 
   C++ and JavaScript can both create messages.
   Debugging can be enabled or disabled for individual source files. 
   (The implementation takes a hash of the filename and looks up a single
    bit in a bitmask indexes.  Hash collisions are possible.)
*/

enum {
  UDEB_OFF      = 0,
  UDEB_URGENT   = 1,
  UDEB_NOTICE   = 2,
  UDEB_INFO     = 3,
  UDEB_DEBUG    = 4,
  UDEB_DETAIL   = 5
};

#define UDEB_SOURCE_FILE_BITMASK_BYTES 2048
#define UDEB_SOURCE_FILE_BITMASK_BITS (8 * 2048)

#ifdef __cplusplus
#define DECLARE_FUNCTIONS_WITH_C_LINKAGE extern "C" { 
#define END_FUNCTIONS_WITH_C_LINKAGE }
#else
#define DECLARE_FUNCTIONS_WITH_C_LINKAGE
#define END_FUNCTIONS_WITH_C_LINKAGE
#endif

DECLARE_FUNCTIONS_WITH_C_LINKAGE


/* Macros in the Public API:
 *
 * DEBUG_PRINT(level, fmt, ...) : print message at level
 * DEBUG_ENTER()                : enter a function (DEBUG level)
 * DEBUG_TRACE()                : print a line number trace (DETAIL level)
 * DEBUG_LEAVE()                : leave a function (DEBUG level)
 * DEBUG_MARKER()               : automatic enter & leave for C++ code
 *
*/


void udeb_print(const char *, int level, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 3, 4)))
#endif
    ;

inline void udeb_trace(const char *src_path, int line) {
  udeb_print(src_path, UDEB_DETAIL, "  Trace: %27s line %d", ".....", line);
}

inline void udeb_leave(int level, const char *src_path, const char *function) {
  udeb_print(src_path, level, "  Leave: %25s", function);
}

void udeb_enter(int, const char *, const char *, int);


END_FUNCTIONS_WITH_C_LINKAGE


/* The C-style API uses macros. DEBUG_ENTER(), DEBUG_PRINT(), DEBUG_TRACE().
   There is also a macro wrapper for the C++ marker.
*/   
#ifdef UNIFIED_DEBUG
extern int uni_debug;

#define DEBUG_ENTER()    if(uni_debug) udeb_enter(UDEB_DEBUG, __FILE__, __func__, __LINE__)
#define DEBUG_PRINT(...) if(uni_debug) udeb_print(__FILE__, UDEB_DEBUG, __VA_ARGS__)
#define DEBUG_PRINT_DETAIL(...) if(uni_debug) udeb_print(__FILE__, UDEB_DETAIL, __VA_ARGS__)
#define DEBUG_PRINT_INFO(...) if(uni_debug) udeb_print(__FILE__, UDEB_INFO, __VA_ARGS__)
#define DEBUG_TRACE()    if(uni_debug) udeb_trace(__FILE__, __LINE__)
#define DEBUG_LEAVE()    if(uni_debug) udeb_leave(UDEB_DEBUG, __FILE__, __func__)
#define DEBUG_MARKER(lvl)  u_DebugMarker _dm( __FILE__, __func__, __LINE__, lvl)
#define DEBUG_ASSERT(x) assert(x)

/* For a C++ API, you can declare a DEBUG_MARKER() on the stack in any scope.
   Its constructor will write a message when the scope is entered, 
   and its destructor will write a message when the scope is exited.
*/
#ifdef __cplusplus
class u_DebugMarker {
public:
  const char *sfile, *sfunc;
  int level;
  u_DebugMarker(const char *sfl, const char * sfn, int ln, int l=UDEB_DEBUG) : 
    sfile(sfl), sfunc(sfn), level(l)  { 
      if(uni_debug) udeb_enter(level, sfile, sfunc, ln); 
    }
  ~u_DebugMarker()          { if(uni_debug) udeb_leave(level, sfile, sfunc); }
};
#endif

#else

#define DEBUG_PRINT(...)
#define DEBUG_PRINT_INFO(...)
#define DEBUG_PRINT_DETAIL(...)
#define DEBUG_ENTER()
#define DEBUG_TRACE()
#define DEBUG_LEAVE()
#define DEBUG_MARKER(lvl)
#define DEBUG_ASSERT(x)

#endif

/* Maximum size of a debug message */
#define UDEB_MSG_BUF 8000

#endif


