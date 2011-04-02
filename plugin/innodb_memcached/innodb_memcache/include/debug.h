#ifndef NDBMEMCACHE_DEBUG_H
#define NDBMEMCACHE_DEBUG_H


/* DEBUG macros for NDB Memcache. 

   Debugging is activated by defining DEBUG_OUTPUT at compile-time.
 
   In order to use the THREAD_ variants of these macros, the caller must define
   two macros, DEBUG_THD_ID and DEBUG_THD_NAME, in the source file. 
     DEBUG_THD_ID : (int) numeric thread id
     DEBUG_THD_NAME : (const char *) thread name.
 
   DEBUG_INIT(const char * outfile) 
     Initialize debugging. If outfile is null, STDERR will be used.

   DEBUG_ASSERT 
     An assertion that is compiled only if debugging is enabled.
 
   DEBUG_PRINT(), THREAD_DEBUG_PRINT():
     These take printf() style parameter lists.  
   
   DEBUG_ENTER(), THREAD_DEBUG_ENTER:
     Print the name of the function being entered.

   ODD_DEBUG_ENTER(thread_id, thread_name, function_name)
   ODD_DEBUG_PRINT(thread_id, thread_name, fmt, ... ):
     Manual variants which allow the caller to specify the thread name and id.
*/


#include "dbmemcache_global.h"
#include "config.h"

#ifdef DEBUG_OUTPUT

extern int do_debug;

/* There's no if(do_debug) check on DEBUG_INIT or DEBUG_ASSERT */
#define DEBUG_INIT(OUTFILE, LEVEL) ndbmc_debug_init(OUTFILE, LEVEL)
#define DEBUG_ASSERT(X) assert(X)

#define DEBUG_PRINT(...) if(do_debug) ndbmc_debug_print(0, 0, __func__, __VA_ARGS__)
#define THREAD_DEBUG_PRINT(...) if(do_debug) ndbmc_debug_print(DEBUG_THD_ID, DEBUG_THD_NAME, __func__, __VA_ARGS__)

#define DEBUG_ENTER() if(do_debug) ndbmc_debug_enter(0, 0, __func__)
#define THREAD_DEBUG_ENTER() if(do_debug) ndbmc_debug_enter(DEBUG_THD_ID, DEBUG_THD_NAME, __func__)

#define ODD_DEBUG_ENTER(id, name, func) if(do_debug) ndbmc_debug_enter(id, name, func)
#define ODD_DEBUG_PRINT(id, name, ...) if(do_debug) ndbmc_debug_print(id, name, __func__, __VA_ARGS__)

#else
#define DEBUG_INIT(...) 
#define DEBUG_ASSERT(...)
#define DEBUG_PRINT(...) 
#define THREAD_DEBUG_PRINT(...) 
#define DEBUG_ENTER()
#define THREAD_DEBUG_ENTER()
#define ODD_DEBUG_ENTER(...) 
#define ODD_DEBUG_PRINT(...)

#endif

/* internal prototypes for debug functions */
DECLARE_FUNCTIONS_WITH_C_LINKAGE
void ndbmc_debug_init(const char *file, bool enable);
void ndbmc_debug_print(int, const char *, const char *, const char *, ...);
void ndbmc_debug_enter(int, const char *, const char *);
END_FUNCTIONS_WITH_C_LINKAGE

#endif
