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
#ifndef NDBMEMCACHE_DEBUG_H
#define NDBMEMCACHE_DEBUG_H


/* DEBUG macros for NDB Memcache. 

   Debugging is activated by defining DEBUG_OUTPUT at compile-time.
 
   DEBUG_INIT(const char * outfile) 
     Initialize debugging. If outfile is null, STDERR will be used.

   DEBUG_ASSERT 
     An assertion that is compiled only if debugging is enabled.
 
   DEBUG_PRINT():
     These take printf() style parameter lists.  
   
   DEBUG_ENTER():
     Print the name of the function being entered.
*/


#include "ndbmemcache_global.h"
#include "ndbmemcache_config.h"

#ifdef DEBUG_OUTPUT

/* Very old Sun compilers do not have __func__ */
#ifdef __SUNPRO_C
#if __SUNPRO_C < 0x600
#define __func__ "?"
#endif
#endif


extern int do_debug;

/* There's no if(do_debug) check on DEBUG_INIT or DEBUG_ASSERT */
#define DEBUG_INIT(OUTFILE, LEVEL) ndbmc_debug_init(OUTFILE, LEVEL)
#define DEBUG_ASSERT(X) assert(X)
#define DEBUG_ENTER() if(do_debug) ndbmc_debug_enter(__func__)
#define DEBUG_ENTER_METHOD(name) if(do_debug) ndbmc_debug_enter(name)
#define DEBUG_PRINT(...) if(do_debug) ndbmc_debug_print(__func__, __VA_ARGS__)

#else
#define DEBUG_INIT(...) 
#define DEBUG_ASSERT(...)
#define DEBUG_ENTER()
#define DEBUG_ENTER_METHOD(...)
#define DEBUG_PRINT(...) 

#endif

/* internal prototypes for debug functions */
DECLARE_FUNCTIONS_WITH_C_LINKAGE
void ndbmc_debug_init(const char *file, int enable);
void ndbmc_debug_print(const char *, const char *, ...);
void ndbmc_debug_enter(const char *);
END_FUNCTIONS_WITH_C_LINKAGE

#endif
