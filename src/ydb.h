/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_INTERFACE_H)
#define TOKU_YDB_INTERFACE_H

#if defined(__cplusplus)
extern "C" {
#endif

// Initialize the ydb library globals.  
// Called when the ydb library is loaded.
int toku_ydb_init(void);

// Called when the ydb library is unloaded.
void toku_ydb_destroy(void);

// db_env_create for the trace library
int db_env_create_toku10(DB_ENV **, u_int32_t) __attribute__((__visibility__("default")));

// db_create for the trace library
int db_create_toku10(DB **, DB_ENV *, u_int32_t) __attribute__((__visibility__("default")));

// test only function
int toku_test_db_redirect_dictionary(DB * db, const char * dname_of_new_file, DB_TXN *dbtxn) __attribute__((__visibility__("default")));

uint64_t toku_test_get_latest_lsn(DB_ENV *env) __attribute__((__visibility__("default")));

// test-only function
extern int toku_test_get_checkpointing_user_data_status(void) __attribute__((__visibility__("default")));

#if defined(__cplusplus)
}
#endif

#endif
