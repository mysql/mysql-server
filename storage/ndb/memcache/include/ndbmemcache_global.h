/*
Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
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
