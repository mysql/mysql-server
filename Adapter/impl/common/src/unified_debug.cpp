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
int uni_debug;   // global static visible to DEBUG_XXX() macros
int udeb_mode;    // initially UDEB_ALL, i.e. zero
int debug_fd = STDERR_FILENO;
unsigned char bit_index[UDEB_SOURCE_FILE_BITMASK_BYTES];

int udeb_print_messages(const char *);

void udeb_print(const char *src_file, const char *fmt, ...) {
  va_list args;
  int sz = 0;
  char message[UDEB_MSG_BUF];

  if(udeb_print_messages(src_file)) {    
    va_start(args, fmt);
    sz += vsnprintf(message + sz, UDEB_MSG_BUF - sz, fmt, args);
    va_end(args);
    sprintf(message + sz++, "\n");
    write(debug_fd, message, sz);
  }
}


void udeb_enter(const char *src_file, const char *function, int line) {
  udeb_print(src_file, "Enter: %25s - line %d - %s", function, line, src_file);
}


void udeb_trace(const char *src_file, int line) {
  udeb_print(src_file, "  Trace: %25s line %d", ".....", line);
}


void unified_debug_destination(const char * out_file) {
  int fd = open(out_file, O_APPEND | O_CREAT, 0644);
  
  if(fd < 0) {
    fd = STDERR_FILENO;     /* Print to previous destination: */
    udeb_print(NULL, "Unified Debug: failed to open \"%s\" for output: %s",
               out_file, strerror(errno));
  }
  debug_fd = fd;
}


inline int udeb_hash(const char *name) {
  const unsigned char *p;
  unsigned int h = 0;
  
  for (p = (const unsigned char *) name ; *p != '\0' ; p++) 
      h = 27 * h + *p;
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
  if(file) {
    if(cmd == UDEB_ADD)       index_set(udeb_hash(file));
    else if(cmd == UDEB_DROP) index_clear(udeb_hash(file));
    else abort();
  }
  else udeb_mode = cmd;
 
  return; 
}

inline int udeb_print_messages(const char *file) {
  int response;
  
  if(file == 0) return true;  // special internal case

#ifdef METADEBUG
fprintf(stderr, "udeb_print? %s\n", file);
#endif
  
  switch(udeb_mode) {
    case UDEB_NONE: 
      response = false;
      break;
    case UDEB_ALL_BUT_SELECTED:
      response =  ! (index_read(udeb_hash(file)));
      break;
    case UDEB_NONE_BUT_SELECTED:
      response =  index_read(udeb_hash(file));
      break;
    default:
      response = true;
      break;
  }
  return response;
}

