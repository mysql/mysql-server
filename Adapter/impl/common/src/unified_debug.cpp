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
#include <libgen.h>

#include "unified_debug.h"

/* These static variables are initialized to zero */
int uni_debug;
int udeb_mode;    // initially UDEB_ALL, i.e. zero
int udeb_level = UDEB_DEBUG;
int debug_fd = STDERR_FILENO;
unsigned char bit_index[UDEB_SOURCE_FILE_BITMASK_BYTES];

int udeb_lookup(const char *);

void udeb_switch(int i) {
  switch(i) {
    case 0:
    case 1:
      uni_debug = i;
      break;
    
    case UDEB_INFO:
    case UDEB_DEBUG:
    case UDEB_DETAIL:
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

void udeb_print(const char *src_file, int level, const char *fmt, ...) {
  va_list args;
  int sz = 0;
  char message[UDEB_MSG_BUF];

  if(udeb_level >= level && udeb_lookup(src_file)) {
    va_start(args, fmt);
    sz += vsnprintf(message + sz, UDEB_MSG_BUF - sz, fmt, args);
    va_end(args);
    sprintf(message + sz++, "\n");
    write(debug_fd, message, sz);
  }
}


void udeb_enter(const char *src_file, const char *function, int line) {
  udeb_print(src_file, UDEB_DEBUG,
             "Enter: %27s - line %d - %s", function, line, src_file);
}


void udeb_trace(const char *src_file, int line) {
  udeb_print(src_file, UDEB_DETAIL,
             "  Trace: %27s line %d", ".....", line);
}


void udeb_leave(const char *src_file, const char *function) {
  udeb_print(src_file, UDEB_DEBUG,
             "  Leave: %25s", function);
}


void unified_debug_destination(const char * out_file) {
  int fd = open(out_file, O_APPEND | O_CREAT, 0644);
  
  if(fd < 0) {
    fd = STDERR_FILENO;     /* Print to previous destination: */
    udeb_print(NULL, UDEB_INFO, 
               "Unified Debug: failed to open \"%s\" for output: %s",
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
  return h % UDEB_SOURCE_FILE_BITMASK_BITS;
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
#ifdef METADEBUG
  udeb_print(0, UDEB_INFO, "udeb_select: %s %d", file, cmd);
#endif
  if(file) {
    if(cmd == UDEB_ADD)       index_set(udeb_hash(file));
    else if(cmd == UDEB_DROP) index_clear(udeb_hash(file));
    else abort();
  }
  else udeb_mode = cmd;
 
  return; 
}

inline int udeb_lookup(const char *key) {
  int response;
  
  if(key == 0) return true;  // special internal case

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

#ifdef METADEBUG
  fprintf(stderr, "udeb_lookup: %s: %s\n", key, response ? "T" : "F");
#endif

  return response;
}

