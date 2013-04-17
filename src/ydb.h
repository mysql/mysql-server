// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_INTERFACE_H)
#define TOKU_YDB_INTERFACE_H

// Initialize the ydb library globals.  
// Called when the ydb library is loaded.
void toku_ydb_init(void);

// Called when the ydb library is unloaded.
void toku_ydb_destroy(void);

// db_env_create for the trace library
int db_env_create_toku10(DB_ENV **, u_int32_t) __attribute__((__visibility__("default")));

// db_create for the trace library
int db_create_toku10(DB **, DB_ENV *, u_int32_t) __attribute__((__visibility__("default")));

#endif
