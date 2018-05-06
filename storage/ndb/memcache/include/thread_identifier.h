/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 
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
 
#ifndef NDBMEMCACHE_THREAD_IDENTIFIER_H
#define NDBMEMCACHE_THREAD_IDENTIFIER_H

#include <sys/types.h>
#include "ndbmemcache_config.h"
#include "ndbmemcache_global.h"

struct request_pipeline;  /* a forward declaration */

#define THD_ID_NAME_LEN (64 - sizeof(size_t))

typedef struct {
  struct request_pipeline *pipeline;
  char name[THD_ID_NAME_LEN];        /* likely size is 56 or 60 */
} thread_identifier;


DECLARE_FUNCTIONS_WITH_C_LINKAGE

/** call once at startup time, after pthread initilization */
void initialize_thread_id_key(void);           

/** store an identifier for the current thread */
void set_thread_id(thread_identifier *t);      

/** set child's identifier, based on parent, with printf-style thread name */
void set_child_thread_id(thread_identifier *parent, const char * fmt, ...);

/** fetch the identifier for the current thread */
const thread_identifier * get_thread_id(void); 

END_FUNCTIONS_WITH_C_LINKAGE

#endif

