/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
 
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
#ifndef NDBMEMCACHE_NDB_ENGINE_H
#define NDBMEMCACHE_NDB_ENGINE_H

#include "ndbmemcache_config.h"

#include <pthread.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <memcached/engine.h>
#include <memcached/util.h>
#include <memcached/visibility.h>

#include "atomics.h"

/**
 *  The NDB engine global data.
 
    Inside memcached, a pointer to this is treated as simply an ENGINE_HANDLE_V1 
    pointer.  But inside the NDB engine that pointer is cast up to point to the
    whole private structure. 
    
    This structure also contains a pointer to the default engine's 
    private structure, since all caching is delegated to the default 
    engine.  
 */

 
struct ndb_engine {
  ENGINE_HANDLE_V1 engine;
  SERVER_HANDLE_V1 server;
  GET_SERVER_API get_server_api;
  ENGINE_HANDLE * m_default_engine;
   
  struct {
    const char * connectstring;
    const char * server_role;
    const char * scheduler;
    bool debug_enable;
    bool debug_detail;
    bool reconf_enable;
  } startup_options;
  
  struct {
    size_t maxconns;
    size_t nthreads;
    bool cas_enabled;  
    size_t verbose;
  } server_options;
  
  union {
    engine_info info;
    char buffer[sizeof(engine_info) * (LAST_REGISTERED_ENGINE_FEATURE + 1)];
  } info;
  
  atomic_int32_t npipelines;
  void **pipelines;

  bool connected;

  unsigned int cas_hi;
  atomic_int32_t cas_lo;
};

#ifdef __cplusplus
extern "C" {
#endif

extern size_t global_max_item_size;

#ifdef __cplusplus
};
#endif

#endif
