/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
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

#include "my_config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include "debug.h"
#include "thread_identifier.h"  


#define DEBUG_MSG_BUF 1022

FILE *debug_outfile;
int do_debug = 0;

void ndbmc_debug_init(const char *filename, int level) {
  do_debug = level;
  if(level) {
    if(filename)
      debug_outfile = fopen(filename, "w");
    else
      debug_outfile = fdopen(STDERR_FILENO, "a");
    assert(debug_outfile);
  }
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


void ndbmc_debug_flush() {
  const thread_identifier *thread = get_thread_id();
  const char * name = thread ? thread->name : "main";
  fprintf(debug_outfile, "thread %s: flushed log file.\n", name);
  fflush(debug_outfile);
}


