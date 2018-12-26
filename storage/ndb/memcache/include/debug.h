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

/* Sometimes __func__ is not available in C++ */
#if (defined(__cplusplus) && ! defined(HAVE_FUNC_IN_CXX))
  #define __func__ "?"
#endif

/* Some Sun compilers also do not have __func__ */
#if defined(__SUNPRO_C) && ! ((__STDC_VERSION__ >= 199901L) || defined(__C99FEATURES__))
#define __func__ "?"
#endif


extern int do_debug;

/* There's no if(do_debug) check on DEBUG_INIT or DEBUG_ASSERT */
#define DEBUG_INIT(OUTFILE, LEVEL) ndbmc_debug_init(OUTFILE, LEVEL)
#define DEBUG_ASSERT(X) assert(X)
#define DEBUG_ENTER() if(do_debug) ndbmc_debug_enter(__func__)
#define DEBUG_ENTER_DETAIL() if(do_debug > 1) ndbmc_debug_enter(__func__)
#define DEBUG_ENTER_METHOD(name) if(do_debug) ndbmc_debug_enter(name)
#define DEBUG_PRINT(...) if(do_debug) ndbmc_debug_print(__func__, __VA_ARGS__)
#define DEBUG_PRINT_DETAIL(...) if(do_debug > 1) ndbmc_debug_print(__func__, __VA_ARGS__)

#else
#define DEBUG_INIT(...) 
#define DEBUG_ASSERT(...)
#define DEBUG_ENTER()
#define DEBUG_ENTER_DETAIL()
#define DEBUG_ENTER_METHOD(...)
#define DEBUG_PRINT(...) 
#define DEBUG_PRINT_DETAIL(...)

#endif

/* internal prototypes for debug functions */
DECLARE_FUNCTIONS_WITH_C_LINKAGE
void ndbmc_debug_init(const char *file, int enable);
void ndbmc_debug_print(const char *, const char *, ...);
void ndbmc_debug_enter(const char *);
void ndbmc_debug_flush();
END_FUNCTIONS_WITH_C_LINKAGE

#endif
