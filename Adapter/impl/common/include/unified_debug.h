/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#pragma once

#include <assert.h>

#define UNIFIED_DEBUG 1

/* Unified debugging library for C++ and JavaScript. 
   JavaScript code can control debugging output. 
   C++ and JavaScript can both create messages.
   Debugging can be enabled or disabled for individual source files. 
   (The implementation takes a hash of the filename and looks up a single
    bit in a bitmask indexes.  Hash collisions are possible.)
*/

#ifdef __cplusplus
#define DECLARE_FUNCTIONS_WITH_C_LINKAGE extern "C" { 
#define END_FUNCTIONS_WITH_C_LINKAGE }
#else
#define DECLARE_FUNCTIONS_WITH_C_LINKAGE
#define END_FUNCTIONS_WITH_C_LINKAGE
#endif

DECLARE_FUNCTIONS_WITH_C_LINKAGE

/* Public API */
void unified_debug_on(void);
void unified_debug_off(void);
void unified_debug_add_file(const char *name);
void unified_debug_drop_file(const char *name);
void unified_debug_all_files(void);
void unified_debug_all_but_selected(void);
void unified_debug_none_but_selected(void);
void unified_debug_none();
void unified_debug_destination(const char * file);
void unified_debug_log_level(int);
void unified_debug_close();

/* Macros in the Public API:
 *
 * DEBUG_PRINT(level, fmt, ...) : print message at level
 * DEBUG_ENTER()                : enter a function (DEBUG level)
 * DEBUG_TRACE()                : print a line number trace (DETAIL level)
 * DEBUG_LEAVE()                : leave a function (DEBUG level)
 * DEBUG_MARKER()               : automatic enter & leave for C++ code
 *
 * Constants in the Public API:
 * UDEB_INFO, UDEB_DETAIL, UDEB_TRACE
*/


/* Private */
enum {
  UDEB_ALL                = 0,  /* initial state */
  UDEB_ADD                = 1,
  UDEB_DROP               = 2,
  UDEB_NONE               = 3,
  UDEB_ALL_BUT_SELECTED   = 4,
  UDEB_NONE_BUT_SELECTED  = 5,
  UDEB_INFO               = 6,
  UDEB_DEBUG              = 7,
  UDEB_DETAIL             = 8,
  UDEB_META               = 9
};

void udeb_select(const char *file_name, int udeb_cmd);
void udeb_print(const char *, int level, const char *fmt, ...);
void udeb_enter(int level, const char *, const char *, int);
void udeb_leave(int level, const char *, const char *);
void udeb_trace(const char *, int);
void udeb_switch(int);
int uni_dbg(void);

#ifdef COMPILING_UNIFIED_DEBUG
int uni_debug;
#else
extern int uni_debug;
#endif 

inline void unified_debug_on(void)                 { udeb_switch(1);           }
inline void unified_debug_off(void)                { udeb_switch(0);           }
inline void unified_debug_log_level(int i)         { udeb_switch(i);           }
inline void unified_debug_add_file(const char *f)  { udeb_select(f, UDEB_ADD); }
inline void unified_debug_drop_file(const char *f) { udeb_select(f, UDEB_DROP);}
inline void unified_debug_all_files()              { udeb_select(NULL, 0);     }
inline void unified_debug_all_but_selected()       { udeb_select(NULL, 4);     }
inline void unified_debug_none_but_selected()      { udeb_select(NULL, 5);     }
inline void unified_debug_none()                   { udeb_select(NULL, 3);     }

END_FUNCTIONS_WITH_C_LINKAGE


/* The C-style API uses macros. DEBUG_ENTER(), DEBUG_PRINT(), DEBUG_TRACE().
   There is also a macro wrapper for the C++ marker.
*/   
#ifdef UNIFIED_DEBUG

#define DEBUG_ENTER()    if(uni_debug) udeb_enter(UDEB_DEBUG, __FILE__, __func__, __LINE__)
#define DEBUG_PRINT(...) if(uni_debug) udeb_print(__FILE__, UDEB_DEBUG, __VA_ARGS__)
#define DEBUG_PRINT_DETAIL(...) if(uni_debug) udeb_print(__FILE__, UDEB_DETAIL, __VA_ARGS__)
#define DEBUG_PRINT_INFO(...) if(uni_debug) udeb_print(__FILE__, UDEB_INFO, __VA_ARGS__)
#define DEBUG_TRACE()    if(uni_debug) udeb_trace(__FILE__, __LINE__)
#define DEBUG_LEAVE()    if(uni_debug) udeb_leave(UDEB_DEBUG, __FILE__, __func__)
#define DEBUG_MARKER(l)  u_DebugMarker _dm( __FILE__, __func__, __LINE__, l)
#define DEBUG_ASSERT(x) assert(x)

#else

#define DEBUG_PRINT(...)
#define DEBUG_PRINT_INFO(...)
#define DEBUG_PRINT_DETAIL(...)
#define DEBUG_ENTER()
#define DEBUG_TRACE()
#define DEBUG_LEAVE()
#define DEBUG_MARKER()
#define DEBUG_ASSERT(x)

#endif

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


/* Size of bitmask index on filename hashes */
#define UDEB_SOURCE_FILE_BITMASK_BYTES 2048
#define UDEB_SOURCE_FILE_BITMASK_BITS (8 * 2048)

/* Maximum size of a debug message */
#define UDEB_MSG_BUF 8000
