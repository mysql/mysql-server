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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "unified_debug.h"

/* These static variables are initialized to zero */
int uni_debug;
int udeb_mode;    // initially UDEB_ALL, i.e. zero
int udeb_level = UDEB_DEBUG;
int debug_fd = STDERR_FILENO;
unsigned char bit_index[UDEB_SOURCE_FILE_BITMASK_BYTES];
const char * levelstr[4] = {"INFO", "DEBUG", "DETAIL", "META"};

int udeb_lookup(const char *);
void udeb_internal_print(const char *fmt, va_list ap);
void udeb_private_print(const char *fmt, ...);

void udeb_switch(int i) {
  switch(i) {
    case 0:
    case 1:
      uni_debug = i;
      break;
    
    case UDEB_INFO:
    case UDEB_DEBUG:
    case UDEB_DETAIL:
    case UDEB_META:
      udeb_private_print("Setting debug output level to %s", 
                         levelstr[i - UDEB_INFO]);
      udeb_level = i;
      break;
    
    default:
      break;
  }
}

/* uni_dbg() is used by the dynamically loaded v8 module, 
   which may fail to load if it tries to directly access 
   uni_debug as an int.
*/
int uni_dbg() {
  return uni_debug;
}

/* libc's basename(3) is not thread-safe, so we implement a version here.
   This one is essentially a strlen() function that also remembers the final
   path separator
*/
inline const char * udeb_basename(const char *path) {
  const char * last_sep = 0;
  if(path) {  
    const char * s = path;
    last_sep = s;
  
    for(; *s ; s++) 
      if(*s == '/') 
         last_sep = s;
    if(last_sep > path && last_sep < s) // point just past the separator
      last_sep += 1;
  }
  return last_sep;
}


/* udeb_internal_print is our fprintf() 
*/
inline void udeb_internal_print(const char *fmt, va_list args) {
  int sz = 0;
  char message[UDEB_MSG_BUF];

  sz += vsnprintf(message + sz, UDEB_MSG_BUF - sz, fmt, args);
  sprintf(message + sz++, "\n");
  write(debug_fd, message, sz);
}


inline void udeb_private_print(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  udeb_internal_print(fmt, args);
  va_end(args);
}


/* udeb_print() is used by macros in the public API 
*/ 
void udeb_print(const char *src_path, int level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  
  const char * src_file = udeb_basename(src_path);
  if(udeb_level >= level && udeb_lookup(src_file))
    udeb_internal_print(fmt, args);
  
  va_end(args);  
}


void udeb_enter(int level, const char *src_path, const char *function, int line) {
  if(udeb_level >= level) {
    const char *src_file = udeb_basename(src_path);
    if(udeb_lookup(src_file)) {
      udeb_private_print("Enter: %27s - line %d - %s", function, line, src_file);
    }
  }
}


void udeb_trace(const char *src_path, int line) {
  if(udeb_level >= UDEB_DETAIL) {
    const char *src_file = udeb_basename(src_path);
    if(udeb_lookup(src_file)) {
      udeb_private_print("  Trace: %27s line %d", ".....", line);
    }
  }
}


void udeb_leave(int level, const char *src_path, const char *function) {
  if(udeb_level >= level) {
    const char *src_file = udeb_basename(src_path);
    if(udeb_lookup(src_file)) {
      udeb_private_print("  Leave: %25s", function);
    }
  }
}


void unified_debug_destination(const char * out_file) {
  int fd = open(out_file, O_APPEND | O_CREAT, 0644);
  
  if(fd < 0) {
    fd = STDERR_FILENO;     /* Print to previous destination: */
    udeb_private_print("Unified Debug: failed to open \"%s\" for output: %s",
                        out_file, strerror(errno));
  }
  debug_fd = fd;
}


// Bernstein hash
inline int udeb_hash(const char *name) {
  const unsigned char *p;
  unsigned int h = 5381;
  
  for (p = (const unsigned char *) name ; *p != '\0' ; p++) 
      h = ((h << 5) + h) + *p;

  h = h % UDEB_SOURCE_FILE_BITMASK_BITS;

  if(udeb_level == UDEB_META) 
    udeb_private_print("udeb_hash: %s --> %d", name, h);

  return h;
}


inline int index_read(unsigned int bit_number) {
  unsigned short byte = bit_number / 8;
  unsigned char  mask = 1 << ( bit_number % 8);
  return bit_index[byte] & mask;
}

inline int index_set(unsigned int bit_number) {
  unsigned short byte = bit_number / 8;
  unsigned char  mask = 1 << ( bit_number % 8);
  bit_index[byte] |= mask;
}

inline int index_clear(unsigned int bit_number) {
  unsigned short byte = bit_number / 8;
  unsigned char  mask = ((1 << ( bit_number % 8)) ^ 0xFF);
  bit_index[byte] &= mask;
}

void udeb_select(const char *file, int cmd) {
  if(udeb_level == UDEB_META) 
    udeb_private_print("udeb_select: %s %d", file ? file : "NULL", cmd);

  if(file) {
    if(cmd == UDEB_ADD)       index_set(udeb_hash(udeb_basename(file)));
    else if(cmd == UDEB_DROP) index_clear(udeb_hash(udeb_basename(file)));
    else abort();
  }
  else udeb_mode = cmd;
 
  return; 
}

inline int udeb_lookup(const char *key) {
  int response;
  
  switch(udeb_mode) {
    case UDEB_NONE: 
      response = false;
      break;
    case UDEB_ALL_BUT_SELECTED:
      response = ! (index_read(udeb_hash(key)));
      break;
    case UDEB_NONE_BUT_SELECTED:
      response =  index_read(udeb_hash(key));
      break;
    default:
      response = true;
      break;
  }

  if(udeb_level == UDEB_META) 
    udeb_private_print("udeb_lookup: %s --> %s", key, response ? "T" : "F");

  return response;
}

