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
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "my_compiler.h"
#include "ndb_pipeline.h"
#include "thread_identifier.h"

static pthread_key_t tlskey;
static int is_initialized;


void initialize_thread_id_key() {
  if(! pthread_key_create(& tlskey, NULL))
    is_initialized = 1;
}


void set_thread_id(thread_identifier *t) {
  pthread_setspecific(tlskey, t);
}


const thread_identifier * get_thread_id() {
  if(is_initialized) 
    return (const thread_identifier *) pthread_getspecific(tlskey);
  else
    return NULL;
}

void set_child_thread_id(thread_identifier *parent, const char * fmt, ...)
  MY_ATTRIBUTE((format(printf, 2, 3)));

void set_child_thread_id(thread_identifier *parent, const char * fmt, ...) {
  va_list args;
  thread_identifier *tid;

  assert(parent->pipeline);

  tid = (thread_identifier *) memory_pool_alloc(parent->pipeline->pool,
                                                sizeof(thread_identifier));
  tid->pipeline = parent->pipeline;
 
  va_start(args, fmt);
  vsnprintf(tid->name, THD_ID_NAME_LEN, fmt, args);
  va_end(args);
  
  set_thread_id(tid);
}

