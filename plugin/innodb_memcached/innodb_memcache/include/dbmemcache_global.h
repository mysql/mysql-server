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

/* A memcached constant; also defined in default_engine.h */
#define POWER_LARGEST 200

/* Operation Verb Enums
   --------------------
   These are used in addition to the ENGINE_STORE_OPERATION constants defined
   in memcached/types.h.  OP_READ must be greater than the highest OPERATION_x
   defined there, and the largest OP_x constant defined here must fit inside
   of workitem.base.verb (currently 4 bits).
*/
enum { OP_READ = 8, OP_DELETE, OP_ARITHMETIC, OP_SCAN };

#endif
