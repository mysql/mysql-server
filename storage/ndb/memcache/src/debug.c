/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
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
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include "debug.h"
#include "thread_identifier.h"  


#define DEBUG_MSG_BUF 1022

FILE *debug_outfile;
int do_debug = 0;

void ndbmc_debug_init(const char *filename, int enable) {
  if(! enable) return;
  
  do_debug = 1;
  if(filename) 
    debug_outfile = fopen(filename, "w");
  else 
    debug_outfile = fdopen(STDERR_FILENO, "a");
  assert(debug_outfile);
}


void ndbmc_debug_print(const char *function, 
                       const char *fmt, ... ) {
  va_list args;
  char message[DEBUG_MSG_BUF + 2];
  int sz = 0;
  const thread_identifier *thread = get_thread_id();

  if(thread) 
    sz += snprintf(message + sz, DEBUG_MSG_BUF - sz, "%s %s():", 
                   thread->name, function);
  else
    sz += snprintf(message + sz, DEBUG_MSG_BUF - sz, "main %s(): ", function);

  va_start(args, fmt);
  sz += vsnprintf(message + sz, DEBUG_MSG_BUF - sz, fmt, args);
  va_end(args);
  sprintf(message + sz, "\n");
  fputs(message, debug_outfile);
}


void ndbmc_debug_enter(const char *func) {
  const thread_identifier *thread = get_thread_id();
  
  if(thread) 
    fprintf(debug_outfile, "%s --> %s()\n", thread->name, func);
  else
    fprintf(debug_outfile, "main --> %s()\n", func);
}

