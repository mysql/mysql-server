/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_INTERFACE_H)
#define TOKU_YDB_INTERFACE_H


// Initialize the ydb library globals.  
// Called when the ydb library is loaded.
int toku_ydb_init(void);

// Called when the ydb library is unloaded.
void toku_ydb_destroy(void);

// db_env_create for the trace library
int db_env_create_toku10(DB_ENV **, uint32_t) __attribute__((__visibility__("default")));

// db_create for the trace library
int db_create_toku10(DB **, DB_ENV *, uint32_t) __attribute__((__visibility__("default")));

// test only function
extern "C" int toku_test_db_redirect_dictionary(DB * db, const char * dname_of_new_file, DB_TXN *dbtxn) __attribute__((__visibility__("default")));

extern "C" uint64_t toku_test_get_latest_lsn(DB_ENV *env) __attribute__((__visibility__("default")));

// test-only function
extern "C" int toku_test_get_checkpointing_user_data_status(void) __attribute__((__visibility__("default")));


#endif
