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
#ifndef NDBMEMCACHE_GLOBAL_H
#define NDBMEMCACHE_GLOBAL_H

/* Convenience macros */
#define LOG_INFO EXTENSION_LOG_INFO
#define LOG_WARNING EXTENSION_LOG_WARNING

/* C-linkage macros */
#ifdef __cplusplus
#define DECLARE_FUNCTIONS_WITH_C_LINKAGE extern "C" { 
#define END_FUNCTIONS_WITH_C_LINKAGE }
#else
#define DECLARE_FUNCTIONS_WITH_C_LINKAGE
#define END_FUNCTIONS_WITH_C_LINKAGE
#endif

/* CPU cache line size; a constant 64 for now.  */
#define CACHE_LINE_SIZE 64

/* A memcached constant; also defined in default_engine.h */
#define POWER_LARGEST  200


/* Operation Verb Enums  
   --------------------
   These are used in addition to the ENGINE_STORE_OPERATION constants defined 
   in memcached/types.h.  OP_READ must be greater than the highest OPERATION_x
   defined there, and the largest OP_x constant defined here must fit inside 
   of workitem.base.verb (currently 4 bits). 
*/
enum {  
  OP_READ = 8,    
  OP_DELETE,
  OP_ARITHMETIC,
  OP_SCAN,
  OP_FLUSH
};

/* Operation Status enums */
typedef enum {
  op_not_supported,
  op_failed,
  op_bad_key,
  op_overflow,
  op_prepared,
} op_status_t;


#endif
