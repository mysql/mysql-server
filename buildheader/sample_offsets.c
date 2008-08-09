/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
/* LICENSE:  This file is licensed under the GPL or from Tokutek. */

/* Make a db.h that will be link-time compatible with Sleepycat's Berkeley DB. */


#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define DECL_LIMIT 100
#define FIELD_LIMIT 100
struct fieldinfo {
    char decl[DECL_LIMIT];
    unsigned int off;
    unsigned int size;
} fields[FIELD_LIMIT];
int field_counter=0;

int compare_fields (const void *av, const void *bv) {
    const struct fieldinfo *a = av;
    const struct fieldinfo *b = bv;
    if (a->off < b->off) return -1;
    if (a->off > b->off) return 1;
    return 0;
}				      

#define STRUCT_SETUP(typ, name, fstring) ({ snprintf(fields[field_counter].decl, DECL_LIMIT, fstring, #name); \
	    fields[field_counter].off = __builtin_offsetof(typ, name);       \
            { typ dummy;                                           \
		fields[field_counter].size = sizeof(dummy.name); } \
	    field_counter++; })

FILE *outf;
void open_file (void) {
    char fname[100];
#ifdef LOCAL
    snprintf(fname, 100, "sample_offsets_local.h");
#else
    snprintf(fname, 100, "sample_offsets_%d_%d_%d.h", __WORDSIZE, DB_VERSION_MAJOR, DB_VERSION_MINOR);
#endif
    outf = fopen(fname, "w");
    assert(outf);

}

void sort_and_dump_fields (const char *structname, unsigned int sizeofstruct) {
    int i;
    qsort(fields, field_counter, sizeof(fields[0]), compare_fields);
    fprintf(outf, "struct fieldinfo %s_fields%d[] = {\n", structname, __WORDSIZE);
    for (i=0; i<field_counter; i++) {
	fprintf(outf, "  {\"%s\", %d, %d},\n", fields[i].decl, fields[i].off, fields[i].size);
    }
    fprintf(outf, "  {0, %d, %d} /* size of whole struct */\n", sizeofstruct, sizeofstruct);
    fprintf(outf, "};\n");
}

void sample_db_btree_stat_offsets (void) {
    field_counter=0;
    STRUCT_SETUP(DB_BTREE_STAT, bt_ndata, "u_int32_t %s");
    STRUCT_SETUP(DB_BTREE_STAT, bt_nkeys, "u_int32_t %s");
    sort_and_dump_fields("db_btree_stat", sizeof(DB_BTREE_STAT));
}

void sample_db_env_offsets (void) {
    field_counter=0;
    STRUCT_SETUP(DB_ENV, api1_internal,   "void *%s"); /* Used for C++ hacking. */
    STRUCT_SETUP(DB_ENV, app_private, "void *%s");
    STRUCT_SETUP(DB_ENV, close, "int  (*%s) (DB_ENV *, u_int32_t)");
    STRUCT_SETUP(DB_ENV, err, "void (*%s) (const DB_ENV *, int, const char *, ...)");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    STRUCT_SETUP(DB_ENV, get_cachesize, "int (*%s) (DB_ENV *, u_int32_t *, u_int32_t *, int *)");
    STRUCT_SETUP(DB_ENV, get_flags, "int (*%s) (DB_ENV *, u_int32_t *)");
    STRUCT_SETUP(DB_ENV, get_lk_max_locks, "int  (*%s) (DB_ENV *, u_int32_t *)");    
    STRUCT_SETUP(DB_ENV, get_lg_max, "int  (*%s) (DB_ENV *, u_int32_t*)");
#endif
    STRUCT_SETUP(DB_ENV, log_archive, "int  (*%s) (DB_ENV *, char **[], u_int32_t)");
    STRUCT_SETUP(DB_ENV, log_flush, "int  (*%s) (DB_ENV *, const DB_LSN *)");
    STRUCT_SETUP(DB_ENV, open, "int  (*%s) (DB_ENV *, const char *, u_int32_t, int)");
    STRUCT_SETUP(DB_ENV, set_cachesize, "int  (*%s) (DB_ENV *, u_int32_t, u_int32_t, int)");
    STRUCT_SETUP(DB_ENV, set_data_dir, "int  (*%s) (DB_ENV *, const char *)");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
    STRUCT_SETUP(DB_ENV, set_errcall, "void (*%s) (DB_ENV *, void (*)(const char *, char *))");
#endif
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    STRUCT_SETUP(DB_ENV, set_errcall, "void (*%s) (DB_ENV *, void (*)(const DB_ENV *, const char *, const char *))");
#endif
    STRUCT_SETUP(DB_ENV, set_errfile, "void (*%s) (DB_ENV *, FILE*)");
    STRUCT_SETUP(DB_ENV, set_errpfx, "void (*%s) (DB_ENV *, const char *)");
    STRUCT_SETUP(DB_ENV, set_flags, "int  (*%s) (DB_ENV *, u_int32_t, int)");
    STRUCT_SETUP(DB_ENV, set_lg_bsize, "int  (*%s) (DB_ENV *, u_int32_t)");
    STRUCT_SETUP(DB_ENV, set_lg_dir, "int  (*%s) (DB_ENV *, const char *)");
    STRUCT_SETUP(DB_ENV, set_lg_max, "int  (*%s) (DB_ENV *, u_int32_t)");
    STRUCT_SETUP(DB_ENV, set_lk_detect, "int  (*%s) (DB_ENV *, u_int32_t)");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    STRUCT_SETUP(DB_ENV, set_lk_max, "int  (*%s) (DB_ENV *, u_int32_t)");
#endif
    STRUCT_SETUP(DB_ENV, set_lk_max_locks, "int  (*%s) (DB_ENV *, u_int32_t)");
    //STRUCT_SETUP(DB_ENV, set_noticecall, "void (*%s) (DB_ENV *, void (*)(DB_ENV *, db_notices))");
    STRUCT_SETUP(DB_ENV, set_tmp_dir, "int  (*%s) (DB_ENV *, const char *)");
    STRUCT_SETUP(DB_ENV, set_verbose, "int  (*%s) (DB_ENV *, u_int32_t, int)");
    STRUCT_SETUP(DB_ENV, txn_checkpoint, "int  (*%s) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)");
    STRUCT_SETUP(DB_ENV, txn_stat, "int  (*%s) (DB_ENV *, DB_TXN_STAT **, u_int32_t)");
    STRUCT_SETUP(DB_ENV, txn_begin, "int  (*%s) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t)");
    sort_and_dump_fields("db_env", sizeof(DB_ENV));
}

void sample_db_key_range_offsets (void) {
    field_counter=0;
    STRUCT_SETUP(DB_KEY_RANGE, less, "double %s");
    STRUCT_SETUP(DB_KEY_RANGE, equal, "double %s");
    STRUCT_SETUP(DB_KEY_RANGE, greater, "double %s");
    sort_and_dump_fields("db_key_range", sizeof(DB_ENV));
}

void sample_db_lsn_offsets (void) {
    field_counter=0;
    sort_and_dump_fields("db_lsn", sizeof(DB_LSN));
}

void sample_db_offsets (void) {
    /* Do these in alphabetical order. */
    field_counter=0;
    STRUCT_SETUP(DB, api_internal,   "void *%s"); /* Used for C++ hacking. */
    STRUCT_SETUP(DB, app_private,    "void *%s");
    STRUCT_SETUP(DB, associate,      "int (*%s) (DB*, DB_TXN*, DB*, int(*)(DB*, const DBT*, const DBT*, DBT*), u_int32_t)");
    STRUCT_SETUP(DB, close,          "int (*%s) (DB*, u_int32_t)");
    STRUCT_SETUP(DB, cursor,         "int (*%s) (DB *, DB_TXN *, DBC **, u_int32_t)");
    STRUCT_SETUP(DB, dbenv,          "DB_ENV *%s");
    STRUCT_SETUP(DB, del,            "int (*%s) (DB *, DB_TXN *, DBT *, u_int32_t)");
    STRUCT_SETUP(DB, fd,             "int (*%s) (DB *, int *)");
    STRUCT_SETUP(DB, get,            "int (*%s) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    STRUCT_SETUP(DB, get_flags,      "int (*%s) (DB *, u_int32_t *)");
    STRUCT_SETUP(DB, get_pagesize,   "int (*%s) (DB *, u_int32_t *)");
#endif
    STRUCT_SETUP(DB, key_range,      "int (*%s) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)");
    STRUCT_SETUP(DB, open,           "int (*%s) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)");
    STRUCT_SETUP(DB, pget,           "int (*%s) (DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t)");
    STRUCT_SETUP(DB, put,            "int (*%s) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)");
    STRUCT_SETUP(DB, remove,         "int (*%s) (DB *, const char *, const char *, u_int32_t)");
    STRUCT_SETUP(DB, rename,         "int (*%s) (DB *, const char *, const char *, const char *, u_int32_t)");
    STRUCT_SETUP(DB, set_bt_compare, "int (*%s) (DB *, int (*)(DB *, const DBT *, const DBT *))");
    STRUCT_SETUP(DB, set_dup_compare, "int (*%s) (DB *, int (*)(DB *, const DBT *, const DBT *))");
    STRUCT_SETUP(DB, set_errfile,    "void (*%s) (DB *, FILE*)");
    STRUCT_SETUP(DB, set_flags,      "int (*%s) (DB *, u_int32_t)");
    STRUCT_SETUP(DB, set_pagesize,   "int (*%s) (DB *, u_int32_t)");
    STRUCT_SETUP(DB, stat,           "int (*%s) (DB *, void *, u_int32_t)");
    STRUCT_SETUP(DB, truncate,       "int (*%s) (DB *, DB_TXN *, u_int32_t *, u_int32_t)");
    STRUCT_SETUP(DB, verify,         "int (*%s) (DB *, const char *, const char *, FILE *, u_int32_t)");
    sort_and_dump_fields("db", sizeof(DB));
}

void sample_db_txn_active_offsets (void) {
    field_counter=0;
    STRUCT_SETUP(DB_TXN_ACTIVE, lsn, "DB_LSN %s");
    STRUCT_SETUP(DB_TXN_ACTIVE, txnid, "u_int32_t %s");
    sort_and_dump_fields("db_txn_active", sizeof(DB_TXN_ACTIVE));
}

void sample_db_txn_offsets (void) {
    field_counter=0;
    STRUCT_SETUP(DB_TXN, abort,       "int (*%s) (DB_TXN *)");
    STRUCT_SETUP(DB_TXN, api_internal,"void *%s");
    STRUCT_SETUP(DB_TXN, commit,      "int (*%s) (DB_TXN*, u_int32_t)");
    STRUCT_SETUP(DB_TXN, id,          "u_int32_t (*%s) (DB_TXN *)");
    STRUCT_SETUP(DB_TXN, mgrp,        "DB_ENV *%s /*In TokuDB, mgrp is a DB_ENV not a DB_TXNMGR*/");
    STRUCT_SETUP(DB_TXN, parent,      "DB_TXN *%s");
    sort_and_dump_fields("db_txn", sizeof(DB_TXN));
}

void sample_db_txn_stat_offsets (void) {
    field_counter=0;
    STRUCT_SETUP(DB_TXN_STAT, st_nactive, "u_int32_t %s");
    STRUCT_SETUP(DB_TXN_STAT, st_txnarray, "DB_TXN_ACTIVE *%s");
    sort_and_dump_fields("db_txn_stat", sizeof(DB_TXN_STAT));
}

void sample_dbc_offsets (void) {
    field_counter=0;
    STRUCT_SETUP(DBC, c_close, "int (*%s) (DBC *)");
    STRUCT_SETUP(DBC, c_count, "int (*%s) (DBC *, db_recno_t *, u_int32_t)");
    STRUCT_SETUP(DBC, c_del,   "int (*%s) (DBC *, u_int32_t)");
    STRUCT_SETUP(DBC, c_get,   "int (*%s) (DBC *, DBT *, DBT *, u_int32_t)");
    STRUCT_SETUP(DBC, c_pget,  "int (*%s) (DBC *, DBT *, DBT *, DBT *, u_int32_t)");
    STRUCT_SETUP(DBC, c_put,   "int (*%s) (DBC *, DBT *, DBT *, u_int32_t)");
    STRUCT_SETUP(DBC, dbp,     "DB *%s");
    sort_and_dump_fields("dbc", sizeof(DBC));
}

void sample_dbt_offsets (void) {
    field_counter=0;
#if 0 && DB_VERSION_MAJOR==4 && DB_VERSION_MINOR==1
    STRUCT_SETUP(DBT, app_private, "void*%s");
#endif
    STRUCT_SETUP(DBT, data,        "void*%s");
    STRUCT_SETUP(DBT, flags,       "u_int32_t %s");
    STRUCT_SETUP(DBT, size,        "u_int32_t %s");
    STRUCT_SETUP(DBT, ulen,        "u_int32_t %s");
    sort_and_dump_fields("dbt", sizeof(DBT));
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    open_file();
    fprintf(outf, "/* BDB offsets on a %d-bit machine */\n", __WORDSIZE);
    fprintf(outf, "#define DB_VERSION_MAJOR_%d %d\n", __WORDSIZE, DB_VERSION_MAJOR);
    fprintf(outf, "#define DB_VERSION_MINOR_%d %d\n", __WORDSIZE, DB_VERSION_MINOR);
    fprintf(outf, "#define DB_VERSION_STRING_%d \"Berkeley DB Compatability Header %d.%d\"\n", __WORDSIZE, DB_VERSION_MAJOR, DB_VERSION_MINOR);
    sample_db_btree_stat_offsets();
    sample_db_env_offsets();
    sample_db_key_range_offsets();
    sample_db_lsn_offsets();
    sample_db_offsets();
    sample_db_txn_active_offsets();
    sample_db_txn_offsets();
    sample_db_txn_stat_offsets();
    sample_dbc_offsets();
    sample_dbt_offsets();
    return 0;
}
