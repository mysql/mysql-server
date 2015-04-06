/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"
/* LICENSE:  This file is licensed under the GPL or from Tokutek. */

/* Make a db.h that will be link-time compatible with Sleepycat's Berkeley DB. */


#include <stdio.h>
#include <stdlib.h>
// Don't include toku_assert.h.   Just use assert.h
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#define VISIBLE "__attribute__((__visibility__(\"default\")))"

#define FIELD_LIMIT 100
struct fieldinfo {
    const char *decl_format_string;
    const char *name;
    size_t offset;
} fields[FIELD_LIMIT];
static int field_counter=0;

static int compare_fields (const void *av, const void *bv) {
    const struct fieldinfo *a = (const struct fieldinfo *) av;
    const struct fieldinfo *b = (const struct fieldinfo *) bv;
    if (a->offset< b->offset) return -1;
    if (a->offset==b->offset) return  0;
    return +1;
}				      

#define STRUCT_SETUP(typ, fname, fstring) ({            \
    assert(field_counter<FIELD_LIMIT);                  \
    fields[field_counter].decl_format_string = fstring; \
    fields[field_counter].name               = #fname;  \
    fields[field_counter].offset             = __builtin_offsetof(typ, fname); \
    field_counter++; })

static void sort_and_dump_fields (const char *structname, bool has_internal, const char *extra_decls[]) {
    int i;
    qsort(fields, field_counter, sizeof(fields[0]), compare_fields);
    printf("struct __toku_%s {\n", structname);
    if (has_internal) {
	printf("  struct __toku_%s_internal *i;\n", structname);
	printf("#define %s_struct_i(x) ((x)->i)\n", structname);
    }
    if (extra_decls) {
	while (*extra_decls) {
	    printf("  %s;\n", *extra_decls);
	    extra_decls++;
	}
    }
    for (i=0; i<field_counter; i++) {
	printf("  ");
	printf(fields[i].decl_format_string, fields[i].name);
	printf(";\n");
    }
    printf("};\n");
}

#include "db-4.6.19.h"

static void print_dbtype(void) {
    /* DBTYPE is mentioned by db_open.html */
    printf("typedef enum {\n");
    printf(" DB_BTREE=%d,\n", DB_BTREE);
    printf(" DB_UNKNOWN=%d\n", DB_UNKNOWN);
    printf("} DBTYPE;\n");
}


#define dodefine(name) printf("#define %s %d\n", #name, name)
#define dodefine_track(flags, name) ({ assert((flags & name) != name);	\
                                       flags |= (name);                \
                                       printf("#define %s %d\n", #name, name); })
#define dodefine_from_track(flags, name) ({\
    uint32_t which;                        \
    uint32_t bit;                          \
    for (which = 0; which < 32; which++) { \
        bit = 1U << which;                 \
        if (!(flags & bit)) break;         \
    }                                      \
    assert(which < 32);                    \
    printf("#define %s %u\n", #name, bit); \
    flags |= bit;                          \
    })

#define dodefine_track_enum(flags, name) ({ assert(name>=0 && name<256); \
                                            assert(!(flags[name])); \
                                            flags[name] = 1;        \
                                            printf("#define %s %d\n", #name, (int)(name)); })
#define dodefine_from_track_enum(flags, name) ({\
    uint32_t which;                             \
    /* don't use 0 */                           \
    for (which = 1; which < 256; which++) {     \
        if (!(flags[which])) break;             \
    }                                           \
    assert(which < 256);                        \
    flags[which] = 1;                           \
    printf("#define %s %u\n", #name, which);    \
    })

enum {
        TOKUDB_OUT_OF_LOCKS            = -100000,
        TOKUDB_SUCCEEDED_EARLY         = -100001,
        TOKUDB_FOUND_BUT_REJECTED      = -100002,
        TOKUDB_USER_CALLBACK_ERROR     = -100003,
        TOKUDB_DICTIONARY_TOO_OLD      = -100004,
        TOKUDB_DICTIONARY_TOO_NEW      = -100005,
        TOKUDB_DICTIONARY_NO_HEADER    = -100006,
        TOKUDB_CANCELED                = -100007,
        TOKUDB_NO_DATA                 = -100008,
        TOKUDB_ACCEPT                  = -100009,
        TOKUDB_MVCC_DICTIONARY_TOO_NEW = -100010,
        TOKUDB_UPGRADE_FAILURE         = -100011,
        TOKUDB_TRY_AGAIN               = -100012,
        TOKUDB_NEEDS_REPAIR            = -100013,
        TOKUDB_CURSOR_CONTINUE         = -100014,
        TOKUDB_BAD_CHECKSUM            = -100015,
        TOKUDB_HUGE_PAGES_ENABLED      = -100016,
        TOKUDB_OUT_OF_RANGE            = -100017,
        TOKUDB_INTERRUPTED             = -100018,
        DONTUSE_I_JUST_PUT_THIS_HERE_SO_I_COULD_HAVE_A_COMMA_AFTER_EACH_ITEM
};

static void print_defines (void) {
    dodefine(DB_VERB_DEADLOCK);
    dodefine(DB_VERB_RECOVERY);
    dodefine(DB_VERB_REPLICATION);
    dodefine(DB_VERB_WAITSFOR);

    dodefine(DB_ARCH_ABS);
    dodefine(DB_ARCH_LOG);

    dodefine(DB_CREATE);
    dodefine(DB_CXX_NO_EXCEPTIONS);
    dodefine(DB_EXCL);
    dodefine(DB_PRIVATE);
    dodefine(DB_RDONLY);
    dodefine(DB_RECOVER);
    dodefine(DB_RUNRECOVERY);
    dodefine(DB_THREAD);
    dodefine(DB_TXN_NOSYNC);

    /* according to BDB 4.6.19, this is the next unused flag in the set of
     * common flags plus private flags for DB->open */
#define	DB_BLACKHOLE 0x0080000
    dodefine(DB_BLACKHOLE);
#undef DB_BLACKHOLE

    dodefine(DB_LOCK_DEFAULT);
    dodefine(DB_LOCK_OLDEST);
    dodefine(DB_LOCK_RANDOM);

    //dodefine(DB_DUP);      No longer supported #2862
    //dodefine(DB_DUPSORT);  No longer supported #2862

    dodefine(DB_KEYFIRST);
    dodefine(DB_KEYLAST);
    {
        static uint8_t insert_flags[256];
        dodefine_track_enum(insert_flags, DB_NOOVERWRITE);
        dodefine_track_enum(insert_flags, DB_NODUPDATA);
        dodefine_from_track_enum(insert_flags, DB_NOOVERWRITE_NO_ERROR);
    }
    dodefine(DB_OPFLAGS_MASK);

    dodefine(DB_AUTO_COMMIT);

    dodefine(DB_INIT_LOCK);
    dodefine(DB_INIT_LOG);
    dodefine(DB_INIT_MPOOL);
    dodefine(DB_INIT_TXN);

    //dodefine(DB_KEYEMPTY);      /// KEYEMPTY is no longer used.  We just use DB_NOTFOUND
    dodefine(DB_KEYEXIST);
    dodefine(DB_LOCK_DEADLOCK);
    dodefine(DB_LOCK_NOTGRANTED);
    dodefine(DB_NOTFOUND);
    dodefine(DB_SECONDARY_BAD);
    dodefine(DB_DONOTINDEX);
#ifdef DB_BUFFER_SMALL
    dodefine(DB_BUFFER_SMALL);
#endif
    printf("#define DB_BADFORMAT -30500\n"); // private tokudb
    printf("#define DB_DELETE_ANY %d\n", 1<<16); // private tokudb

    dodefine(DB_FIRST);
    dodefine(DB_LAST);
    dodefine(DB_CURRENT);
    dodefine(DB_NEXT);
    dodefine(DB_PREV);
    dodefine(DB_SET);
    dodefine(DB_SET_RANGE);
    printf("#define DB_CURRENT_BINDING 253\n"); // private tokudb
    printf("#define DB_SET_RANGE_REVERSE 252\n"); // private tokudb
    //printf("#define DB_GET_BOTH_RANGE_REVERSE 251\n"); // private tokudb.  No longer supported #2862.
    dodefine(DB_RMW);
    printf("#define DB_IS_RESETTING_OP 0x01000000\n"); // private tokudb
    printf("#define DB_PRELOCKED 0x00800000\n"); // private tokudb
    printf("#define DB_PRELOCKED_WRITE 0x00400000\n"); // private tokudb
    //printf("#define DB_PRELOCKED_FILE_READ 0x00200000\n"); // private tokudb. No longer supported in #4472
    printf("#define DB_IS_HOT_INDEX 0x00100000\n"); // private tokudb
    printf("#define DBC_DISABLE_PREFETCHING 0x20000000\n"); // private tokudb
    printf("#define DB_UPDATE_CMP_DESCRIPTOR 0x40000000\n"); // private tokudb
    printf("#define TOKUFT_DIRTY_SHUTDOWN %x\n", 1<<31);

    {
        //dbt flags
        uint32_t dbt_flags = 0;
        dodefine_track(dbt_flags, DB_DBT_APPMALLOC);
        dodefine_track(dbt_flags, DB_DBT_DUPOK);
        dodefine_track(dbt_flags, DB_DBT_MALLOC);
#ifdef DB_DBT_MULTIPLE
        dodefine_track(dbt_flags, DB_DBT_MULTIPLE);
#endif
        dodefine_track(dbt_flags, DB_DBT_REALLOC);
        dodefine_track(dbt_flags, DB_DBT_USERMEM);
    }

    // flags for the env->set_flags function
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    dodefine(DB_LOG_AUTOREMOVE);
#endif

    {
    //Txn begin/commit flags
        uint32_t txn_flags = 0;
        dodefine_track(txn_flags, DB_TXN_WRITE_NOSYNC);
        dodefine_track(txn_flags, DB_TXN_NOWAIT);
        dodefine_track(txn_flags, DB_TXN_SYNC);
#ifdef DB_TXN_SNAPSHOT
        dodefine_track(txn_flags, DB_TXN_SNAPSHOT);
#endif
#ifdef DB_READ_UNCOMMITTED
        dodefine_track(txn_flags, DB_READ_UNCOMMITTED);
#endif
#ifdef DB_READ_COMMITTED
        dodefine_track(txn_flags, DB_READ_COMMITTED);
#endif
        //Add them if they didn't exist
#ifndef DB_TXN_SNAPSHOT
        dodefine_from_track(txn_flags, DB_TXN_SNAPSHOT);
#endif
#ifndef DB_READ_UNCOMMITTED
        dodefine_from_track(txn_flags, DB_READ_UNCOMMITTED);
#endif
#ifndef DB_READ_COMMITTED
        dodefine_from_track(txn_flags, DB_READ_COMMITTED);
#endif
        dodefine_from_track(txn_flags, DB_INHERIT_ISOLATION);
        dodefine_from_track(txn_flags, DB_SERIALIZABLE);
        dodefine_from_track(txn_flags, DB_TXN_READ_ONLY);
    }
    
    /* TokuFT specific error codes*/
    printf("/* TokuFT specific error codes */\n");
    dodefine(TOKUDB_OUT_OF_LOCKS);
    dodefine(TOKUDB_SUCCEEDED_EARLY);
    dodefine(TOKUDB_FOUND_BUT_REJECTED);
    dodefine(TOKUDB_USER_CALLBACK_ERROR);
    dodefine(TOKUDB_DICTIONARY_TOO_OLD);
    dodefine(TOKUDB_DICTIONARY_TOO_NEW);
    dodefine(TOKUDB_DICTIONARY_NO_HEADER);
    dodefine(TOKUDB_CANCELED);
    dodefine(TOKUDB_NO_DATA);
    dodefine(TOKUDB_ACCEPT);
    dodefine(TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    dodefine(TOKUDB_UPGRADE_FAILURE);
    dodefine(TOKUDB_TRY_AGAIN);
    dodefine(TOKUDB_NEEDS_REPAIR);
    dodefine(TOKUDB_CURSOR_CONTINUE);
    dodefine(TOKUDB_BAD_CHECKSUM);
    dodefine(TOKUDB_HUGE_PAGES_ENABLED);
    dodefine(TOKUDB_OUT_OF_RANGE);
    dodefine(TOKUDB_INTERRUPTED);

    /* LOADER flags */
    printf("/* LOADER flags */\n");
    {
        uint32_t loader_flags = 0;
        dodefine_from_track(loader_flags, LOADER_DISALLOW_PUTS); // Loader is only used for side effects.
        dodefine_from_track(loader_flags, LOADER_COMPRESS_INTERMEDIATES);
    }
}

static void print_db_env_struct (void) {
    field_counter=0;
    STRUCT_SETUP(DB_ENV, api1_internal,   "void *%s"); /* Used for C++ hacking. */
    STRUCT_SETUP(DB_ENV, app_private, "void *%s");
    STRUCT_SETUP(DB_ENV, close, "int  (*%s) (DB_ENV *, uint32_t)");
    STRUCT_SETUP(DB_ENV, err, "void (*%s) (const DB_ENV *, int, const char *, ...) __attribute__ (( format (printf, 3, 4) ))");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    STRUCT_SETUP(DB_ENV, get_cachesize, "int  (*%s) (DB_ENV *, uint32_t *, uint32_t *, int *)");
    STRUCT_SETUP(DB_ENV, get_flags, "int  (*%s) (DB_ENV *, uint32_t *)");
    STRUCT_SETUP(DB_ENV, get_lg_max, "int  (*%s) (DB_ENV *, uint32_t*)");
#endif
    STRUCT_SETUP(DB_ENV, log_archive, "int  (*%s) (DB_ENV *, char **[], uint32_t)");
    STRUCT_SETUP(DB_ENV, log_flush, "int  (*%s) (DB_ENV *, const DB_LSN *)");
    STRUCT_SETUP(DB_ENV, open, "int  (*%s) (DB_ENV *, const char *, uint32_t, int)");
    STRUCT_SETUP(DB_ENV, set_cachesize, "int  (*%s) (DB_ENV *, uint32_t, uint32_t, int)");
    STRUCT_SETUP(DB_ENV, set_data_dir, "int  (*%s) (DB_ENV *, const char *)");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
    STRUCT_SETUP(DB_ENV, set_errcall, "void (*%s) (DB_ENV *, void (*)(const char *, char *))");
#endif
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    STRUCT_SETUP(DB_ENV, set_errcall, "void (*%s) (DB_ENV *, void (*)(const DB_ENV *, const char *, const char *))");
#endif
    STRUCT_SETUP(DB_ENV, set_errfile, "void (*%s) (DB_ENV *, FILE*)");
    STRUCT_SETUP(DB_ENV, set_errpfx, "void (*%s) (DB_ENV *, const char *)");
    STRUCT_SETUP(DB_ENV, set_flags, "int  (*%s) (DB_ENV *, uint32_t, int)");
    STRUCT_SETUP(DB_ENV, set_lg_bsize, "int  (*%s) (DB_ENV *, uint32_t)");
    STRUCT_SETUP(DB_ENV, set_lg_dir, "int  (*%s) (DB_ENV *, const char *)");
    STRUCT_SETUP(DB_ENV, set_lg_max, "int  (*%s) (DB_ENV *, uint32_t)");
    STRUCT_SETUP(DB_ENV, set_lk_detect, "int  (*%s) (DB_ENV *, uint32_t)");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    STRUCT_SETUP(DB_ENV, set_lk_max, "int  (*%s) (DB_ENV *, uint32_t)");
#endif
    //STRUCT_SETUP(DB_ENV, set_noticecall, "void (*%s) (DB_ENV *, void (*)(DB_ENV *, db_notices))");
    STRUCT_SETUP(DB_ENV, set_tmp_dir, "int  (*%s) (DB_ENV *, const char *)");
    STRUCT_SETUP(DB_ENV, set_verbose, "int  (*%s) (DB_ENV *, uint32_t, int)");
    STRUCT_SETUP(DB_ENV, txn_checkpoint, "int  (*%s) (DB_ENV *, uint32_t, uint32_t, uint32_t)");
    STRUCT_SETUP(DB_ENV, txn_stat,    "int  (*%s) (DB_ENV *, DB_TXN_STAT **, uint32_t)");
    STRUCT_SETUP(DB_ENV, txn_begin,   "int  (*%s) (DB_ENV *, DB_TXN *, DB_TXN **, uint32_t)");
    STRUCT_SETUP(DB_ENV, txn_recover, "int  (*%s) (DB_ENV *, DB_PREPLIST preplist[/*count*/], long count, /*out*/ long *retp, uint32_t flags)");
    STRUCT_SETUP(DB_ENV, dbremove,    "int  (*%s) (DB_ENV *, DB_TXN *, const char *, const char *, uint32_t)");
    STRUCT_SETUP(DB_ENV, dbrename,    "int  (*%s) (DB_ENV *, DB_TXN *, const char *, const char *, const char *, uint32_t)");

        const char *extra[]={
                             "int (*checkpointing_set_period)             (DB_ENV*, uint32_t) /* Change the delay between automatic checkpoints.  0 means disabled. */",
                             "int (*checkpointing_get_period)             (DB_ENV*, uint32_t*) /* Retrieve the delay between automatic checkpoints.  0 means disabled. */",
                             "int (*cleaner_set_period)                   (DB_ENV*, uint32_t) /* Change the delay between automatic cleaner attempts.  0 means disabled. */",
                             "int (*cleaner_get_period)                   (DB_ENV*, uint32_t*) /* Retrieve the delay between automatic cleaner attempts.  0 means disabled. */",
                             "int (*cleaner_set_iterations)               (DB_ENV*, uint32_t) /* Change the number of attempts on each cleaner invokation.  0 means disabled. */",
                             "int (*cleaner_get_iterations)               (DB_ENV*, uint32_t*) /* Retrieve the number of attempts on each cleaner invokation.  0 means disabled. */",
                             "int (*checkpointing_postpone)               (DB_ENV*) /* Use for 'rename table' or any other operation that must be disjoint from a checkpoint */",
                             "int (*checkpointing_resume)                 (DB_ENV*) /* Alert tokuft that 'postpone' is no longer necessary */",
                             "int (*checkpointing_begin_atomic_operation) (DB_ENV*) /* Begin a set of operations (that must be atomic as far as checkpoints are concerned). i.e. inserting into every index in one table */",
                             "int (*checkpointing_end_atomic_operation)   (DB_ENV*) /* End   a set of operations (that must be atomic as far as checkpoints are concerned). */",
                             "int (*set_default_bt_compare)               (DB_ENV*,int (*bt_compare) (DB *, const DBT *, const DBT *)) /* Set default (key) comparison function for all DBs in this environment.  Required for RECOVERY since you cannot open the DBs manually. */",
                             "int (*get_engine_status_num_rows)           (DB_ENV*, uint64_t*)  /* return number of rows in engine status */",
                             "int (*get_engine_status)                    (DB_ENV*, TOKU_ENGINE_STATUS_ROW, uint64_t, uint64_t*, fs_redzone_state*, uint64_t*, char*, int, toku_engine_status_include_type) /* Fill in status struct and redzone state, possibly env panic string */",
                             "int (*get_engine_status_text)               (DB_ENV*, char*, int)     /* Fill in status text */",
                             "int (*crash)                                (DB_ENV*, const char*/*expr_as_string*/,const char */*fun*/,const char*/*file*/,int/*line*/, int/*errno*/)",
                             "int (*get_iname)                            (DB_ENV* env, DBT* dname_dbt, DBT* iname_dbt) /* FOR TEST ONLY: lookup existing iname */",
                             "int (*create_loader)                        (DB_ENV *env, DB_TXN *txn, DB_LOADER **blp,    DB *src_db, int N, DB *dbs[/*N*/], uint32_t db_flags[/*N*/], uint32_t dbt_flags[/*N*/], uint32_t loader_flags)",
                             "int (*create_indexer)                       (DB_ENV *env, DB_TXN *txn, DB_INDEXER **idxrp, DB *src_db, int N, DB *dbs[/*N*/], uint32_t db_flags[/*N*/], uint32_t indexer_flags)",
                             "int (*put_multiple)                         (DB_ENV *env, DB *src_db, DB_TXN *txn,\n"
                             "                                               const DBT *src_key, const DBT *src_val,\n"
                             "                                               uint32_t num_dbs, DB **db_array, DBT_ARRAY *keys, DBT_ARRAY *vals, uint32_t *flags_array) /* insert into multiple DBs */",
                             "int (*set_generate_row_callback_for_put)    (DB_ENV *env, generate_row_for_put_func generate_row_for_put)",
                             "int (*del_multiple)                         (DB_ENV *env, DB *src_db, DB_TXN *txn,\n"
                             "                                               const DBT *src_key, const DBT *src_val,\n"
                             "                                               uint32_t num_dbs, DB **db_array, DBT_ARRAY *keys, uint32_t *flags_array) /* delete from multiple DBs */",
                             "int (*set_generate_row_callback_for_del)    (DB_ENV *env, generate_row_for_del_func generate_row_for_del)",
                             "int (*update_multiple)                      (DB_ENV *env, DB *src_db, DB_TXN *txn,\n"
                             "                                               DBT *old_src_key, DBT *old_src_data,\n"
                             "                                               DBT *new_src_key, DBT *new_src_data,\n"
                             "                                               uint32_t num_dbs, DB **db_array, uint32_t *flags_array,\n"
                             "                                               uint32_t num_keys, DBT_ARRAY *keys,\n"
                             "                                               uint32_t num_vals, DBT_ARRAY *vals) /* update multiple DBs */",
                             "int (*get_redzone)                          (DB_ENV *env, int *redzone) /* get the redzone limit */",
                             "int (*set_redzone)                          (DB_ENV *env, int redzone) /* set the redzone limit in percent of total space */",
                             "int (*set_lk_max_memory)                    (DB_ENV *env, uint64_t max)",
                             "int (*get_lk_max_memory)                    (DB_ENV *env, uint64_t *max)",
                             "void (*set_update)                          (DB_ENV *env, int (*update_function)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra))",
                             "int (*set_lock_timeout)                     (DB_ENV *env, uint64_t default_lock_wait_time_msec, uint64_t (*get_lock_wait_time_cb)(uint64_t default_lock_wait_time))",
                             "int (*get_lock_timeout)                     (DB_ENV *env, uint64_t *lock_wait_time_msec)",
                             "int (*set_lock_timeout_callback)            (DB_ENV *env, lock_timeout_callback callback)",
                             "int (*txn_xa_recover)                       (DB_ENV*, TOKU_XA_XID list[/*count*/], long count, /*out*/ long *retp, uint32_t flags)",
                             "int (*get_txn_from_xid)                     (DB_ENV*, /*in*/ TOKU_XA_XID *, /*out*/ DB_TXN **)",
                             "int (*get_cursor_for_directory)             (DB_ENV*, /*in*/ DB_TXN *, /*out*/ DBC **)",
                             "int (*get_cursor_for_persistent_environment)(DB_ENV*, /*in*/ DB_TXN *, /*out*/ DBC **)",
                             "void (*change_fsync_log_period)             (DB_ENV*, uint32_t)",
                             "int (*iterate_live_transactions)            (DB_ENV *env, iterate_transactions_callback callback, void *extra)",
                             "int (*iterate_pending_lock_requests)        (DB_ENV *env, iterate_requests_callback callback, void *extra)",
                             "void (*set_loader_memory_size)(DB_ENV *env, uint64_t (*get_loader_memory_size_callback)(void))",
                             "uint64_t (*get_loader_memory_size)(DB_ENV *env)",
                             "void (*set_killed_callback)(DB_ENV *env, uint64_t default_killed_time_msec, uint64_t (*get_killed_time_callback)(uint64_t default_killed_time_msec), int (*killed_callback)(void))",
                             "void (*do_backtrace)                        (DB_ENV *env)",
                             NULL};

        sort_and_dump_fields("db_env", true, extra);
}

static void print_db_key_range_struct (void) {
    field_counter=0;
    STRUCT_SETUP(DB_KEY_RANGE, less, "double %s");
    STRUCT_SETUP(DB_KEY_RANGE, equal, "double %s");
    STRUCT_SETUP(DB_KEY_RANGE, greater, "double %s");
    sort_and_dump_fields("db_key_range", false, NULL);
}

static void print_db_lsn_struct (void) {
    field_counter=0;
    sort_and_dump_fields("db_lsn", false, NULL);
}

static void print_dbt_struct (void) {
    field_counter=0;
#if 0 && DB_VERSION_MAJOR==4 && DB_VERSION_MINOR==1
    STRUCT_SETUP(DBT, app_private, "void*%s");
#endif
    STRUCT_SETUP(DBT, data,        "void*%s");
    STRUCT_SETUP(DBT, flags,       "uint32_t %s");
    STRUCT_SETUP(DBT, size,        "uint32_t %s");
    STRUCT_SETUP(DBT, ulen,        "uint32_t %s");
    sort_and_dump_fields("dbt", false, NULL);
}

static void print_db_struct (void) {
    /* Do these in alphabetical order. */
    field_counter=0;
    STRUCT_SETUP(DB, api_internal,   "void *%s"); /* Used for C++ hacking. */
    STRUCT_SETUP(DB, app_private,    "void *%s");
    STRUCT_SETUP(DB, close,          "int (*%s) (DB*, uint32_t)");
    STRUCT_SETUP(DB, cursor,         "int (*%s) (DB *, DB_TXN *, DBC **, uint32_t)");
    STRUCT_SETUP(DB, dbenv,          "DB_ENV *%s");
    STRUCT_SETUP(DB, del,            "int (*%s) (DB *, DB_TXN *, DBT *, uint32_t)");
    STRUCT_SETUP(DB, fd,             "int (*%s) (DB *, int *)");
    STRUCT_SETUP(DB, get,            "int (*%s) (DB *, DB_TXN *, DBT *, DBT *, uint32_t)");
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    STRUCT_SETUP(DB, get_flags,      "int (*%s) (DB *, uint32_t *)");
    STRUCT_SETUP(DB, get_pagesize,   "int (*%s) (DB *, uint32_t *)");
#endif
    STRUCT_SETUP(DB, key_range,      "int (*%s) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, uint32_t)");
    STRUCT_SETUP(DB, open,           "int (*%s) (DB *, DB_TXN *, const char *, const char *, DBTYPE, uint32_t, int)");
    STRUCT_SETUP(DB, put,            "int (*%s) (DB *, DB_TXN *, DBT *, DBT *, uint32_t)");
    STRUCT_SETUP(DB, set_errfile,    "void (*%s) (DB *, FILE*)");
    STRUCT_SETUP(DB, set_flags,      "int (*%s) (DB *, uint32_t)");
    STRUCT_SETUP(DB, set_pagesize,   "int (*%s) (DB *, uint32_t)");
    STRUCT_SETUP(DB, stat,           "int (*%s) (DB *, void *, uint32_t)");
    STRUCT_SETUP(DB, verify,         "int (*%s) (DB *, const char *, const char *, FILE *, uint32_t)");
    const char *extra[]={
                         "int (*key_range64)(DB*, DB_TXN *, DBT *, uint64_t *less, uint64_t *equal, uint64_t *greater, int *is_exact)",
                         "int (*get_key_after_bytes)(DB *, DB_TXN *, const DBT *, uint64_t, void (*callback)(const DBT *, uint64_t, void *), void *, uint32_t); /* given start_key and skip_len, find largest end_key such that the elements in [start_key,end_key) sum to <= skip_len bytes */",
                         "int (*keys_range64)(DB*, DB_TXN *, DBT *keyleft, DBT *keyright, uint64_t *less, uint64_t *left, uint64_t *between, uint64_t *right, uint64_t *greater, bool *middle_3_exact)",
			 "int (*stat64)(DB *, DB_TXN *, DB_BTREE_STAT64 *)",
			 "int (*pre_acquire_table_lock)(DB*, DB_TXN*)",
			 "int (*pre_acquire_fileops_lock)(DB*, DB_TXN*)",
			 "const DBT* (*dbt_pos_infty)(void) /* Return the special DBT that refers to positive infinity in the lock table.*/",
			 "const DBT* (*dbt_neg_infty)(void)/* Return the special DBT that refers to negative infinity in the lock table.*/",
			 "void (*get_max_row_size) (DB*, uint32_t *max_key_size, uint32_t *max_row_size)",
			 "DESCRIPTOR descriptor /* saved row/dictionary descriptor for aiding in comparisons */",
			 "DESCRIPTOR cmp_descriptor /* saved row/dictionary descriptor for aiding in comparisons */",
			 "int (*change_descriptor) (DB*, DB_TXN*, const DBT* descriptor, uint32_t) /* change row/dictionary descriptor for a db.  Available only while db is open */",
			 "int (*getf_set)(DB*, DB_TXN*, uint32_t, DBT*, YDB_CALLBACK_FUNCTION, void*) /* same as DBC->c_getf_set without a persistent cursor) */",
			 "int (*optimize)(DB*) /* Run garbage collecion and promote all transactions older than oldest. Amortized (happens during flattening) */",
			 "int (*hot_optimize)(DB*, DBT*, DBT*, int (*progress_callback)(void *progress_extra, float progress), void *progress_extra, uint64_t* loops_run)",
			 "int (*get_fragmentation)(DB*,TOKU_DB_FRAGMENTATION)",
			 "int (*change_pagesize)(DB*,uint32_t)",
			 "int (*change_readpagesize)(DB*,uint32_t)",
			 "int (*get_readpagesize)(DB*,uint32_t*)",
			 "int (*set_readpagesize)(DB*,uint32_t)",
			 "int (*change_compression_method)(DB*,TOKU_COMPRESSION_METHOD)",
			 "int (*get_compression_method)(DB*,TOKU_COMPRESSION_METHOD*)",
			 "int (*set_compression_method)(DB*,TOKU_COMPRESSION_METHOD)",
			 "int (*change_fanout)(DB *db, uint32_t fanout)",
			 "int (*get_fanout)(DB *db, uint32_t *fanout)",
			 "int (*set_fanout)(DB *db, uint32_t fanout)",
			 "int (*set_memcmp_magic)(DB *db, uint8_t magic)",
			 "int (*set_indexer)(DB*, DB_INDEXER*)",
			 "void (*get_indexer)(DB*, DB_INDEXER**)",
			 "int (*verify_with_progress)(DB *, int (*progress_callback)(void *progress_extra, float progress), void *progress_extra, int verbose, int keep_going)",
			 "int (*update)(DB *, DB_TXN*, const DBT *key, const DBT *extra, uint32_t flags)",
			 "int (*update_broadcast)(DB *, DB_TXN*, const DBT *extra, uint32_t flags)",
			 "int (*get_fractal_tree_info64)(DB*,uint64_t*,uint64_t*,uint64_t*,uint64_t*)",
			 "int (*iterate_fractal_tree_block_map)(DB*,int(*)(uint64_t,int64_t,int64_t,int64_t,int64_t,void*),void*)",
                         "const char *(*get_dname)(DB *db)",
                         "int (*get_last_key)(DB *db, YDB_CALLBACK_FUNCTION func, void* extra)",
			 NULL};
    sort_and_dump_fields("db", true, extra);
}

static void print_db_txn_active_struct (void) {
    field_counter=0;
    STRUCT_SETUP(DB_TXN_ACTIVE, lsn, "DB_LSN %s");
    STRUCT_SETUP(DB_TXN_ACTIVE, txnid, "uint32_t %s");
    sort_and_dump_fields("db_txn_active", false, NULL);
}

static void print_db_txn_struct (void) {
    field_counter=0;
    STRUCT_SETUP(DB_TXN, abort,       "int (*%s) (DB_TXN *)");
    STRUCT_SETUP(DB_TXN, api_internal,"void *%s");
    STRUCT_SETUP(DB_TXN, commit,      "int (*%s) (DB_TXN*, uint32_t)");
    STRUCT_SETUP(DB_TXN, prepare,     "int (*%s) (DB_TXN*, uint8_t gid[DB_GID_SIZE], uint32_t flags)");
    STRUCT_SETUP(DB_TXN, discard,     "int (*%s) (DB_TXN*, uint32_t)");
    STRUCT_SETUP(DB_TXN, id,          "uint32_t (*%s) (DB_TXN *)");
    STRUCT_SETUP(DB_TXN, mgrp,        "DB_ENV *%s /* In TokuFT, mgrp is a DB_ENV, not a DB_TXNMGR */");
    STRUCT_SETUP(DB_TXN, parent,      "DB_TXN *%s");
    const char *extra[] = {
	"int (*txn_stat)(DB_TXN *, struct txn_stat **)", 
	"int (*commit_with_progress)(DB_TXN*, uint32_t, TXN_PROGRESS_POLL_FUNCTION, void*)",
	"int (*abort_with_progress)(DB_TXN*, TXN_PROGRESS_POLL_FUNCTION, void*)",
	"int (*xa_prepare) (DB_TXN*, TOKU_XA_XID *, uint32_t flags)",
        "uint64_t (*id64) (DB_TXN*)",
        "void (*set_client_id)(DB_TXN *, uint64_t client_id)",
        "uint64_t (*get_client_id)(DB_TXN *)",
        "bool (*is_prepared)(DB_TXN *)",
        "DB_TXN *(*get_child)(DB_TXN *)",
        "uint64_t (*get_start_time)(DB_TXN *)",
	NULL};
    sort_and_dump_fields("db_txn", false, extra);
}

static void print_db_txn_stat_struct (void) {
    field_counter=0;
    STRUCT_SETUP(DB_TXN_STAT, st_nactive, "uint32_t %s");
    STRUCT_SETUP(DB_TXN_STAT, st_txnarray, "DB_TXN_ACTIVE *%s");
    sort_and_dump_fields("db_txn_stat", false, NULL);
}

static void print_dbc_struct (void) {
    field_counter=0;
    STRUCT_SETUP(DBC, c_close, "int (*%s) (DBC *)");
    //STRUCT_SETUP(DBC, c_del,   "int (*%s) (DBC *, uint32_t)");  // c_del was removed.  See #4576.
    STRUCT_SETUP(DBC, c_get,   "int (*%s) (DBC *, DBT *, DBT *, uint32_t)");
    STRUCT_SETUP(DBC, dbp,     "DB *%s");
    const char *extra[]={
	"int (*c_getf_first)(DBC *, uint32_t, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_last)(DBC *, uint32_t, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_next)(DBC *, uint32_t, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_prev)(DBC *, uint32_t, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_current)(DBC *, uint32_t, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_set)(DBC *, uint32_t, DBT *, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_set_range)(DBC *, uint32_t, DBT *, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_set_range_reverse)(DBC *, uint32_t, DBT *, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_getf_set_range_with_bound)(DBC *, uint32_t, DBT *k, DBT *k_bound, YDB_CALLBACK_FUNCTION, void *)",
	"int (*c_set_bounds)(DBC*, const DBT*, const DBT*, bool pre_acquire, int out_of_range_error)",
        "void (*c_set_check_interrupt_callback)(DBC*, bool (*)(void*, uint64_t deleted_rows), void *)",
	"void (*c_remove_restriction)(DBC*)",
        "char _internal[512]",
	NULL};
    sort_and_dump_fields("dbc", false, extra);
}


int main (int argc, char *const argv[] __attribute__((__unused__))) {
    assert(argc==1);

    printf("#ifndef _DB_H\n");
    printf("#define _DB_H\n");
    printf("/* This code generated by make_db_h.   Copyright (c) 2007-2013 Tokutek */\n");
    printf("#ident \"Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved.\"\n");
    printf("#include <sys/types.h>\n");
    printf("/*stdio is needed for the FILE* in db->verify*/\n");
    printf("#include <stdio.h>\n");
    printf("/*stdbool is needed for the bool in db_env_enable_engine_status*/\n");
    printf("#include <stdbool.h>\n");
    printf("#include <stdint.h>\n");
    //printf("#include <inttypes.h>\n");
    printf("#if defined(__cplusplus) || defined(__cilkplusplus)\nextern \"C\" {\n#endif\n");

    printf("#define DB_VERSION_MAJOR %d\n", DB_VERSION_MAJOR);
    printf("#define DB_VERSION_MINOR %d\n", DB_VERSION_MINOR);
    printf("/* As of r40364 (post TokuFT 5.2.7), the patch version number is 100+ the BDB header patch version number.*/\n");
    printf("#define DB_VERSION_PATCH %d\n", 100+DB_VERSION_PATCH);
    printf("#define DB_VERSION_STRING \"Tokutek: TokuFT %d.%d.%d\"\n", DB_VERSION_MAJOR, DB_VERSION_MINOR, 100+DB_VERSION_PATCH);

#ifndef DB_GID_SIZE
#define DB_GID_SIZE DB_XIDDATASIZE
#endif
    dodefine(DB_GID_SIZE);

    printf("typedef struct toku_xa_xid_s { /* This struct is intended to be binary compatible with the XID in the XA architecture.  See source:/import/opengroup.org/C193.pdf */\n"
           "    long formatID;                  /* format identifier */\n"
           "    long gtrid_length;              /* value from 1 through 64 */\n"
           "    long bqual_length;              /* value from 1 through 64 */\n"
           "    char data[DB_GID_SIZE];\n"
           "} TOKU_XA_XID;\n");

    printf("#ifndef TOKU_OFF_T_DEFINED\n"
           "#define TOKU_OFF_T_DEFINED\n"
           "typedef int64_t toku_off_t;\n"
           "#endif\n");

    printf("typedef struct __toku_db_env DB_ENV;\n");
    printf("typedef struct __toku_db_key_range DB_KEY_RANGE;\n");
    printf("typedef struct __toku_db_lsn DB_LSN;\n");
    printf("typedef struct __toku_db DB;\n");
    printf("typedef struct __toku_db_txn DB_TXN;\n");
    printf("typedef struct __toku_db_txn_active DB_TXN_ACTIVE;\n");
    printf("typedef struct __toku_db_txn_stat DB_TXN_STAT;\n");
    printf("typedef struct __toku_dbc DBC;\n");
    printf("typedef struct __toku_dbt DBT;\n");
    printf("typedef struct __toku_db_preplist { DB_TXN *txn; uint8_t gid[DB_GID_SIZE]; } DB_PREPLIST;\n");
    printf("typedef uint32_t db_recno_t;\n");
    printf("typedef int(*YDB_CALLBACK_FUNCTION)(DBT const*, DBT const*, void*);\n");

    printf("struct simple_dbt {\n");
    printf("    uint32_t len;\n");
    printf("    void     *data;\n");
    printf("};\n");
    
    //stat64
    printf("typedef struct __toku_db_btree_stat64 {\n");
    printf("  uint64_t bt_nkeys; /* how many unique keys (guaranteed only to be an estimate, even when flattened)          */\n");
    printf("  uint64_t bt_ndata; /* how many key-value pairs (an estimate, but exact when flattened)                       */\n");
    printf("  uint64_t bt_dsize; /* how big are the keys+values (not counting the lengths) (an estimate, unless flattened) */\n");
    printf("  uint64_t bt_fsize; /* how big is the underlying file                                                         */\n");
    // 4018
    printf("  uint64_t bt_create_time_sec; /* Creation time, in seconds */\n");
    printf("  uint64_t bt_modify_time_sec; /* Time of last serialization, in seconds */\n");
    printf("  uint64_t bt_verify_time_sec; /* Time of last verification, in seconds */\n");
    printf("} DB_BTREE_STAT64;\n");

    // compression methods
    printf("typedef enum toku_compression_method {\n");
    printf("    TOKU_NO_COMPRESSION = 0,\n");  // "identity" compression
    printf("    TOKU_ZLIB_METHOD    = 8,\n");  // RFC 1950 says use 8 for zlib.  It reserves 15 to allow more bytes.
    printf("    TOKU_QUICKLZ_METHOD = 9,\n");  // We use 9 for QUICKLZ (the QLZ compression level is stored int he high-order nibble).  I couldn't find any standard for any other numbers, so I just use 9. -Bradley
    printf("    TOKU_LZMA_METHOD    = 10,\n");  // We use 10 for LZMA.  (Note the compression level is stored in the high-order nibble).
    printf("    TOKU_ZLIB_WITHOUT_CHECKSUM_METHOD = 11,\n"); // We wrap a zlib without checksumming compression technique in our own checksummed metadata.
    printf("    TOKU_DEFAULT_COMPRESSION_METHOD = 1,\n");  // default is actually quicklz
    printf("    TOKU_FAST_COMPRESSION_METHOD = 2,\n");  // friendlier names
    printf("    TOKU_SMALL_COMPRESSION_METHOD = 3,\n");
    printf("} TOKU_COMPRESSION_METHOD;\n");

    //bulk loader
    printf("typedef struct __toku_loader DB_LOADER;\n");
    printf("struct __toku_loader_internal;\n");
    printf("struct __toku_loader {\n");
    printf("  struct __toku_loader_internal *i;\n");
    printf("  int (*set_error_callback)(DB_LOADER *loader, void (*error_cb)(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra), void *error_extra); /* set the error callback */\n");
    printf("  int (*set_poll_function)(DB_LOADER *loader, int (*poll_func)(void *extra, float progress), void *poll_extra);             /* set the polling function */\n");
    printf("  int (*put)(DB_LOADER *loader, DBT *key, DBT* val);                                                      /* give a row to the loader */\n");
    printf("  int (*close)(DB_LOADER *loader);                                                                        /* finish loading, free memory */\n");
    printf("  int (*abort)(DB_LOADER *loader);                                                                        /* abort loading, free memory */\n");
    printf("};\n");

    //indexer
    printf("typedef struct __toku_indexer DB_INDEXER;\n");
    printf("struct __toku_indexer_internal;\n");
    printf("struct __toku_indexer {\n");
    printf("  struct __toku_indexer_internal *i;\n");
    printf("  int (*set_error_callback)(DB_INDEXER *indexer, void (*error_cb)(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra), void *error_extra); /* set the error callback */\n");
    printf("  int (*set_poll_function)(DB_INDEXER *indexer, int (*poll_func)(void *extra, float progress), void *poll_extra);             /* set the polling function */\n");
    printf("  int (*build)(DB_INDEXER *indexer);  /* build the indexes */\n");
    printf("  int (*close)(DB_INDEXER *indexer);  /* finish indexing, free memory */\n");
    printf("  int (*abort)(DB_INDEXER *indexer);  /* abort  indexing, free memory */\n");
    printf("};\n");

    // Filesystem redzone state
    printf("typedef enum { \n");
    printf("    FS_GREEN = 0,                                   // green zone  (we have lots of space) \n");
    printf("    FS_YELLOW = 1,                                  // yellow zone (issue warning but allow operations) \n");
    printf("    FS_RED = 2,                                     // red zone    (prevent insert operations) \n");
    printf("    FS_BLOCKED = 3                                  // For reporting engine status, completely blocked \n");
    printf("} fs_redzone_state;\n");

    printf("// engine status info\n");
    printf("// engine status is passed to handlerton as an array of TOKU_ENGINE_STATUS_ROW_S[]\n");

    printf("typedef enum {\n");
    printf("   FS_STATE = 0,   // interpret as file system state (redzone) enum \n");
    printf("   UINT64,         // interpret as uint64_t \n");
    printf("   CHARSTR,        // interpret as char * \n");
    printf("   UNIXTIME,       // interpret as time_t \n");
    printf("   TOKUTIME,       // interpret as tokutime_t \n");
    printf("   PARCOUNT,       // interpret as PARTITIONED_COUNTER\n");
    printf("   DOUBLE          // interpret as double\n");
    printf("} toku_engine_status_display_type; \n");

    printf("typedef enum {\n");
    printf("   TOKU_ENGINE_STATUS             = (1ULL<<0),  // Include when asking for engine status\n");
    printf("   TOKU_GLOBAL_STATUS = (1ULL<<1),  // Include when asking for information_schema.global_status\n");
    printf("} toku_engine_status_include_type; \n");

    printf("typedef struct __toku_engine_status_row {\n");
    printf("  const char * keyname;                  // info schema key, should not change across revisions without good reason \n");
    printf("  const char * columnname;               // column for mysql, e.g. information_schema.global_status. TOKUDB_ will automatically be prefixed.\n");
    printf("  const char * legend;                   // the text that will appear at user interface \n");
    printf("  toku_engine_status_display_type type;  // how to interpret the value \n");
    printf("  toku_engine_status_include_type include;  // which kinds of callers should get read this row?\n");
    printf("  union {              \n");
    printf("         double   dnum; \n");
    printf("         uint64_t num; \n");
    printf("         const char *   str; \n");
    printf("         char           datebuf[26]; \n");
    printf("         struct partitioned_counter *parcount;\n");
    printf("  } value;       \n");
    printf("} * TOKU_ENGINE_STATUS_ROW, TOKU_ENGINE_STATUS_ROW_S; \n");

    print_dbtype();
    print_defines();

    printf("typedef struct {\n");
    printf("    uint32_t capacity;\n");
    printf("    uint32_t size;\n");
    printf("    DBT *dbts;\n");
    printf("} DBT_ARRAY;\n\n");
    printf("typedef int (*generate_row_for_put_func)(DB *dest_db, DB *src_db, DBT_ARRAY * dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val);\n");
    printf("typedef int (*generate_row_for_del_func)(DB *dest_db, DB *src_db, DBT_ARRAY * dest_keys, const DBT *src_key, const DBT *src_val);\n");
    printf("DBT_ARRAY * toku_dbt_array_init(DBT_ARRAY *dbts, uint32_t size) %s;\n", VISIBLE);
    printf("void toku_dbt_array_destroy(DBT_ARRAY *dbts) %s;\n", VISIBLE);
    printf("void toku_dbt_array_destroy_shallow(DBT_ARRAY *dbts) %s;\n", VISIBLE);
    printf("void toku_dbt_array_resize(DBT_ARRAY *dbts, uint32_t size) %s;\n", VISIBLE);

    printf("typedef void (*lock_timeout_callback)(DB *db, uint64_t requesting_txnid, const DBT *left_key, const DBT *right_key, uint64_t blocking_txnid);\n");
    printf("typedef int (*iterate_row_locks_callback)(DB **db, DBT *left_key, DBT *right_key, void *extra);\n");
    printf("typedef int (*iterate_transactions_callback)(DB_TXN *dbtxn, iterate_row_locks_callback cb, void *locks_extra, void *extra);\n");
    printf("typedef int (*iterate_requests_callback)(DB *db, uint64_t requesting_txnid, const DBT *left_key, const DBT *right_key, uint64_t blocking_txnid, uint64_t start_time, void *extra);\n");
    print_db_env_struct();
    print_db_key_range_struct();
    print_db_lsn_struct();
    print_dbt_struct();

    printf("typedef struct __toku_descriptor {\n");
    printf("    DBT       dbt;\n");
    printf("} *DESCRIPTOR, DESCRIPTOR_S;\n");

    //file fragmentation info
    //a block is just a contiguous region in a file.
    printf("//One header is included in 'data'\n");
    printf("//One header is included in 'additional for checkpoint'\n");
    printf("typedef struct __toku_db_fragmentation {\n");
    printf("  uint64_t file_size_bytes;               //Total file size in bytes\n");
    printf("  uint64_t data_bytes;                    //Compressed User Data in bytes\n");
    printf("  uint64_t data_blocks;                   //Number of blocks of compressed User Data\n");
    printf("  uint64_t checkpoint_bytes_additional;   //Additional bytes used for checkpoint system\n");
    printf("  uint64_t checkpoint_blocks_additional;  //Additional blocks used for checkpoint system \n");
    printf("  uint64_t unused_bytes;                  //Unused space in file\n");
    printf("  uint64_t unused_blocks;                 //Number of contiguous regions of unused space\n");
    printf("  uint64_t largest_unused_block;          //Size of largest contiguous unused space\n");
    printf("} *TOKU_DB_FRAGMENTATION, TOKU_DB_FRAGMENTATION_S;\n");

    print_db_struct();

    print_db_txn_active_struct();

    printf("typedef struct __toku_txn_progress {\n");
    printf("  uint64_t entries_total;\n");
    printf("  uint64_t entries_processed;\n");
    printf("  uint8_t  is_commit;\n");
    printf("  uint8_t  stalled_on_checkpoint;\n");
    printf("} *TOKU_TXN_PROGRESS, TOKU_TXN_PROGRESS_S;\n");
    printf("typedef void(*TXN_PROGRESS_POLL_FUNCTION)(TOKU_TXN_PROGRESS, void*);\n");
    printf("struct txn_stat {\n  uint64_t rollback_raw_count;\n  uint64_t rollback_num_entries;\n};\n");

    print_db_txn_struct();
    print_db_txn_stat_struct();
    print_dbc_struct();

    printf("int db_env_create(DB_ENV **, uint32_t) %s;\n", VISIBLE);
    printf("int db_create(DB **, DB_ENV *, uint32_t) %s;\n", VISIBLE);
    printf("const char *db_strerror(int) %s;\n", VISIBLE);
    printf("const char *db_version(int*,int *,int *) %s;\n", VISIBLE);
    printf("int log_compare (const DB_LSN*, const DB_LSN *) %s;\n", VISIBLE);
    printf("int toku_set_trace_file (const char *fname) %s;\n", VISIBLE);
    printf("int toku_close_trace_file (void) %s;\n", VISIBLE);
    printf("void db_env_set_direct_io (bool direct_io_on) %s;\n", VISIBLE);
    printf("void db_env_set_compress_buffers_before_eviction (bool compress_buffers) %s;\n", VISIBLE);
    printf("void db_env_set_func_fsync (int (*)(int)) %s;\n", VISIBLE);
    printf("void db_env_set_func_free (void (*)(void*)) %s;\n", VISIBLE);
    printf("void db_env_set_func_malloc (void *(*)(size_t)) %s;\n", VISIBLE);
    printf("void db_env_set_func_realloc (void *(*)(void*, size_t)) %s;\n", VISIBLE);
    printf("void db_env_set_func_pwrite (ssize_t (*)(int, const void *, size_t, toku_off_t)) %s;\n", VISIBLE);
    printf("void db_env_set_func_full_pwrite (ssize_t (*)(int, const void *, size_t, toku_off_t)) %s;\n", VISIBLE);
    printf("void db_env_set_func_write (ssize_t (*)(int, const void *, size_t)) %s;\n", VISIBLE);
    printf("void db_env_set_func_full_write (ssize_t (*)(int, const void *, size_t)) %s;\n", VISIBLE);
    printf("void db_env_set_func_fdopen (FILE* (*)(int, const char *)) %s;\n", VISIBLE);
    printf("void db_env_set_func_fopen (FILE* (*)(const char *, const char *)) %s;\n", VISIBLE);
    printf("void db_env_set_func_open (int (*)(const char *, int, int)) %s;\n", VISIBLE);
    printf("void db_env_set_func_fclose (int (*)(FILE*)) %s;\n", VISIBLE);
    printf("void db_env_set_func_pread (ssize_t (*)(int, void *, size_t, off_t)) %s;\n", VISIBLE);
    printf("void db_env_set_func_loader_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*)) %s;\n", VISIBLE);
    printf("void db_env_set_checkpoint_callback (void (*)(void*), void*) %s;\n", VISIBLE);
    printf("void db_env_set_checkpoint_callback2 (void (*)(void*), void*) %s;\n", VISIBLE);
    printf("void db_env_set_recover_callback (void (*)(void*), void*) %s;\n", VISIBLE);
    printf("void db_env_set_recover_callback2 (void (*)(void*), void*) %s;\n", VISIBLE);
    printf("void db_env_set_loader_size_factor (uint32_t) %s;\n", VISIBLE);
    printf("void db_env_set_mvcc_garbage_collection_verification(uint32_t) %s;\n", VISIBLE);
    printf("void db_env_enable_engine_status(bool) %s;\n", VISIBLE);
    printf("void db_env_set_flusher_thread_callback (void (*)(int, void*), void*) %s;\n", VISIBLE);
    printf("void db_env_set_num_bucket_mutexes(uint32_t) %s;\n", VISIBLE);
    printf("int db_env_set_toku_product_name(const char*) %s;\n", VISIBLE);
    printf("void db_env_try_gdb_stack_trace(const char *gdb_path) %s;\n", VISIBLE);

    printf("#if defined(__cplusplus) || defined(__cilkplusplus)\n}\n#endif\n");
    printf("#endif\n");

    return 0;
}
