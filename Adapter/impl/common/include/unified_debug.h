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

/* Unified debugging library for C++ and JavaScript. 
   JavaScript code can control debugging output. 
   C++ and JavaScript can both create messages.
   Debugging can be enabled or disabled for individual source files. 
   (The implementation takes a hash of the filename and looks up a single
    bit in a bitmask indexes.  Hash collisions are possible.)
*/

extern int uni_debug;

#ifdef __cplusplus
extern "C" {
#endif

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

/* Private */
enum {
  UDEB_ALL                = 0,  /* initial state */
  UDEB_ADD                = 1,
  UDEB_DROP               = 2,
  UDEB_NONE               = 3,
  UDEB_ALL_BUT_SELECTED   = 4,
  UDEB_NONE_BUT_SELECTED  = 5
};
void udeb_select(const char *file_name, int udeb_cmd);
void udeb_print(const char *, const char *, ...);
void udeb_enter(const char *, const char *, int);
void udeb_trace(const char *, int);

inline void unified_debug_on(void)                 { uni_debug = 1;            }
inline void unified_debug_off(void)                { uni_debug = 0;            }
inline void unified_debug_add_file(const char *f)  { udeb_select(f, UDEB_ADD); }
inline void unified_debug_drop_file(const char *f) { udeb_select(f, UDEB_DROP);}
inline void unified_debug_all_files()              { udeb_select(NULL, 0);     }
inline void unified_debug_all_but_selected()       { udeb_select(NULL, 4);     }
inline void unified_debug_none_but_selected()      { udeb_select(NULL, 5);     }
inline void unified_debug_none()                   { udeb_select(NULL, 3);     }

#ifdef __cplusplus
}
#endif


/* For a C++ API, you can declare a debug_marker on the stack in any scope.
   Its constructor will write a message when the scope is entered, 
   and its destructor will write a message when the scope is exited.
*/
class debug_marker {
public:
  debug_marker()   {  if(uni_debug) udeb_enter(__FILE__, __func__, __LINE__); }
  ~debug_marker()  {  if(uni_debug) udeb_trace(__FILE__, __LINE__); }
};


/* The C-style API uses macros. DEBUG_ENTER(), DEBUG_PRINT(), DEBUG_TRACE().
   There is also a macro wrapper for the C++ marker.
*/   


#ifdef UNIFIED_DEBUG

#define DEBUG_ENTER()    if(uni_debug) udeb_enter(__FILE__, __func__, __LINE__)
#define DEBUG_PRINT(...) if(uni_debug) udeb_print(__FILE__, __VA_ARGS__)
#define DEBUG_TRACE()    if(uni_debug) udeb_trace(__FILE__, __LINE__)
#define DEBUG_MARKER()   debug_marker _dm()

#else

#define DEBUG_PRINT(...)
#define DEBUG_ENTER()
#define DEBUG_TRACE()
#define DEBUG_MARKER()

#endif


/* Size of bitmask index on filename hashes */
#define UDEB_SOURCE_FILE_BITMASK_BYTES 2048
#define UDEB_SOURCE_FILE_BITMASK_BITS (8 * 2048)

/* Maximum size of a debug message */
#define UDEB_MSG_BUF 8000
