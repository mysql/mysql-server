/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_CURSOR_H)
#define TOKU_YDB_CURSOR_H


typedef enum {
    YDB_C_LAYER_STATUS_NUM_ROWS = 0             /* number of rows in this status array */
} ydb_c_lock_layer_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_C_LAYER_STATUS_NUM_ROWS];
} YDB_C_LAYER_STATUS_S, *YDB_C_LAYER_STATUS;

void ydb_c_layer_get_status(YDB_C_LAYER_STATUS statp);

int toku_c_get(DBC * c, DBT * key, DBT * data, uint32_t flag);
int toku_c_getf_set(DBC *c, uint32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
int toku_c_close(DBC * c);
int toku_db_cursor_internal(DB *db, DB_TXN * txn, DBC **c, uint32_t flags, int is_temporary_cursor);
int toku_db_cursor(DB *db, DB_TXN *txn, DBC **c, uint32_t flags);



#endif
