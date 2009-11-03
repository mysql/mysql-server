/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
/* LICENSE:  This file is licensed under the GPL or from Tokutek. */

/* Make a db.h that will be link-time compatible with Sleepycat's Berkeley DB. */

#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define VISIBLE "__attribute__((__visibility__(\"default\")))"

void print_dbtype(void) {
    /* DBTYPE is mentioned by db_open.html */
    printf("typedef enum {\n");
    printf(" DB_BTREE=%d,\n", DB_BTREE);
    printf(" DB_UNKNOWN=%d\n", DB_UNKNOWN);
    printf("} DBTYPE;\n");
}
#if 0
void print_db_notices (void) {
    printf("typedef enum { /* This appears to be a mysql-specific addition to the api. */ \n");
    printf(" DB_NOTICE_LOGFILE_CHANGED=%d\n", DB_NOTICE_LOGFILE_CHANGED);
    printf("} db_notices;\n");
}
#endif

#define dodefine(name) printf("#define %s %d\n", #name, name)

enum {
	TOKUDB_OUT_OF_LOCKS         = -100000,
        TOKUDB_SUCCEEDED_EARLY      = -100001,
        TOKUDB_FOUND_BUT_REJECTED   = -100002,
        TOKUDB_USER_CALLBACK_ERROR  = -100003,
        TOKUDB_DICTIONARY_TOO_OLD   = -100004,
        TOKUDB_DICTIONARY_TOO_NEW   = -100005,
        TOKUDB_DICTIONARY_NO_HEADER = -100006
};

void print_defines (void) {
    printf("#ifndef _TOKUDB_WRAP_H\n");
    dodefine(DB_VERB_DEADLOCK);
    dodefine(DB_VERB_RECOVERY);
    dodefine(DB_VERB_REPLICATION);
    dodefine(DB_VERB_WAITSFOR);

    dodefine(DB_DBT_MALLOC);
    dodefine(DB_DBT_REALLOC);
    dodefine(DB_DBT_USERMEM);
    dodefine(DB_DBT_DUPOK);

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

    dodefine(DB_LOCK_DEFAULT);
    dodefine(DB_LOCK_OLDEST);
    dodefine(DB_LOCK_RANDOM);

    dodefine(DB_DUP);
    dodefine(DB_DUPSORT);

    dodefine(DB_KEYFIRST);
    dodefine(DB_KEYLAST);
    dodefine(DB_NODUPDATA);
    dodefine(DB_NOOVERWRITE);
    printf("#define DB_YESOVERWRITE 254\n"); // tokudb
    dodefine(DB_OPFLAGS_MASK);

    dodefine(DB_AUTO_COMMIT);

    dodefine(DB_INIT_LOCK);
    dodefine(DB_INIT_LOG);
    dodefine(DB_INIT_MPOOL);
    printf("#define DB_CLOSE_DONT_TRIM_LOG 1048576\n"); // tokudb
    dodefine(DB_INIT_TXN);

#ifdef DB_READ_UNCOMMITTED
    dodefine(DB_READ_UNCOMMITTED);
#endif
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
    printf("#define DB_TRUNCATE_WITHCURSORS %d\n", 1<<17); // private tokudb

    dodefine(DB_FIRST);
    dodefine(DB_GET_BOTH);
    dodefine(DB_GET_BOTH_RANGE);
    dodefine(DB_LAST);
    dodefine(DB_CURRENT);
    dodefine(DB_NEXT);
    dodefine(DB_NEXT_DUP);
    dodefine(DB_NEXT_NODUP);
    dodefine(DB_PREV);
#if defined(DB_PREV_DUP)
    dodefine(DB_PREV_DUP);
#endif
    dodefine(DB_PREV_NODUP);
    dodefine(DB_SET);
    dodefine(DB_SET_RANGE);
    printf("#define DB_CURRENT_BINDING 253\n"); // private tokudb
    printf("#define DB_SET_RANGE_REVERSE 252\n"); // private tokudb
    printf("#define DB_GET_BOTH_RANGE_REVERSE 251\n"); // private tokudb
    dodefine(DB_RMW);
    printf("#define DB_PRELOCKED 0x00800000\n"); // private tokudb
    printf("#define DB_PRELOCKED_WRITE 0x00400000\n"); // private tokudb

    dodefine(DB_DBT_APPMALLOC);
#ifdef DB_DBT_MULTIPLE
    dodefine(DB_DBT_MULTIPLE);
#endif

    // flags for the env->set_flags function
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    dodefine(DB_LOG_AUTOREMOVE);
#endif
    dodefine(DB_TXN_WRITE_NOSYNC);
    dodefine(DB_TXN_NOWAIT);
    dodefine(DB_TXN_SYNC);

    
    printf("#endif\n");
    
    /* TOKUDB specific error codes*/
    printf("/* TOKUDB specific error codes */\n");
    dodefine(TOKUDB_OUT_OF_LOCKS);
    dodefine(TOKUDB_SUCCEEDED_EARLY);
    dodefine(TOKUDB_DICTIONARY_TOO_OLD);
    dodefine(TOKUDB_DICTIONARY_TOO_NEW);
    dodefine(TOKUDB_DICTIONARY_NO_HEADER);
    dodefine(TOKUDB_FOUND_BUT_REJECTED);
    dodefine(TOKUDB_USER_CALLBACK_ERROR);
}

//#define DECL_LIMIT 100
struct fieldinfo {
    char *decl;
    unsigned int off;
    unsigned int size;
};

#if USE_MAJOR==4 && USE_MINOR==1
#include "sample_offsets_32_4_1.h"
#include "sample_offsets_64_4_1.h"
#elif USE_MAJOR==4 && USE_MINOR==3
#include "sample_offsets_32_4_3.h"
#include "sample_offsets_64_4_3.h"
#elif USE_MAJOR==4 && USE_MINOR==4
#include "sample_offsets_32_4_4.h"
#include "sample_offsets_64_4_4.h"
#elif USE_MAJOR==4 && USE_MINOR==5
#include "sample_offsets_32_4_5.h"
#include "sample_offsets_64_4_5.h"
#elif USE_MAJOR==4 && USE_MINOR==6
#include "sample_offsets_32_4_6.h"
#include "sample_offsets_64_4_6.h"
#else
#error
#endif

void print_struct (const char *structname, int need_internal, struct fieldinfo *fields32, struct fieldinfo *fields64, unsigned int N, const char *extra_decls[]) {
    unsigned int i;
    unsigned int current_32 = 0;
    unsigned int current_64 = 0;
    int dummy_counter=0;
    int did_toku_internal=0;
//    int total32 = fields32[N-1].size;
//    int total64 = fields32[N-1].size;
    printf("struct __toku_%s {\n", structname);
    for (i=0; i<N-1; i++) {
	unsigned int this_32 = fields32[i].off;
	unsigned int this_64 = fields64[i].off;
	//fprintf(stderr, "this32=%d current32=%d this64=%d current64=%d\n", this_32, current_32, this_64, current_64);
	if (this_32 > current_32 || this_64 > current_64) {
	    unsigned int diff32 = this_32-current_32;
	    unsigned int diff64 = this_64-current_64;
	    assert(this_32 > current_32 && this_64 > current_64);
	    if (diff32!=diff64) {
		unsigned int diff = diff64-diff32;
		unsigned int n_dummys = diff/4;
		if (need_internal && !did_toku_internal) {
		    if (TDB_NATIVE &&
			(strcmp(structname, "dbc")==0 ||
			 strcmp(structname, "db_txn")==0)) {
			printf("  struct __toku_%s_internal ii;\n", structname);
			printf("#define %s_struct_i(x) (&(x)->ii)\n", structname);
		    } else {
			printf("  struct __toku_%s_internal *i;\n", structname);
			printf("#define %s_struct_i(x) ((x)->i)\n", structname);
		    }
		    n_dummys--;
		    did_toku_internal=1;
		}
		while (n_dummys>0 && extra_decls && *extra_decls) {
		    printf("  %s;\n", *extra_decls);
		    extra_decls++;
		    n_dummys--;
		}
		if (n_dummys>0) {
		    if (!TDB_NATIVE)
			printf("  void* __toku_dummy%d[%d];\n", dummy_counter, n_dummys);
		    dummy_counter++;
		}
		diff64-=diff*2;
		diff32-=diff;
		
	    }
	    assert(diff32==diff64);
	    if (diff32>0) {
		if (!TDB_NATIVE)
		    printf("  char __toku_dummy%d[%d];\n", dummy_counter, diff32);
		dummy_counter++;
	    }
	    current_32 = this_32;
	    current_64 = this_64;
	}
	if (this_32<current_32 || this_64<current_64) {
	    printf("Whoops this_32=%d this_64=%d\n", this_32, this_64);
	}
	if (i+1<N) {
	    assert(strcmp(fields32[i].decl, fields64[i].decl)==0);
	    printf("  %s;", fields32[i].decl);
	    if (!TDB_NATIVE)
		printf(" /* 32-bit offset=%d size=%d, 64=bit offset=%d size=%d */", fields32[i].off, fields32[i].size, fields64[i].off, fields64[i].size);
	    printf("\n");
	} else {
	    assert(fields32[i].decl==0);
	    assert(fields64[i].decl==0);
	}
	current_32 += fields32[i].size;
	current_64 += fields64[i].size;
    }
    if (extra_decls) assert(NULL==*extra_decls); // make sure that the extra decls all got used up.
    {
	unsigned int this_32 = fields32[N-1].off;
	unsigned int this_64 = fields64[N-1].off;
	unsigned int diff32  = this_32-current_32;
	unsigned int diff64  = this_64-current_64;
	if (diff32>0 && diff32<diff64) {
	    unsigned int diff = diff64-diff32;
	    if (!TDB_NATIVE)
		printf("  void* __toku_dummy%d[%d]; /* Padding at the end */ \n", dummy_counter, diff/4);
	    dummy_counter++;
	    diff64-=diff*2;
	    diff32-=diff;
	}
	if (diff32>0) {
	    if (!TDB_NATIVE)
		printf("  char __toku_dummy%d[%d];  /* Padding at the end */ \n", dummy_counter, diff32);
	    dummy_counter++;
	    diff64-=diff32;
	    diff32=0;
	}
	if (diff64>0)
	    if (!TDB_NATIVE)
		printf("  /* %d more bytes of alignment in the 64-bit case. */\n", diff64);
	assert(diff64<8); /* there could be a few left from alignment. */ 
    }
    printf("};\n");
    assert(did_toku_internal || !need_internal);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    printf("#ifndef _DB_H\n");
    printf("#define _DB_H\n");
    printf("/* This code generated by make_db_h.   Copyright (c) 2007, 2008 Tokutek */\n");
    printf("#ident \"Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved.\"\n");
    printf("#include <sys/types.h>\n");
    printf("/*stdio is needed for the FILE* in db->verify*/\n");
    printf("#include <stdio.h>\n");
    //printf("#include <inttypes.h>\n");
    printf("#if defined(__cplusplus)\nextern \"C\" {\n#endif\n");

    assert(DB_VERSION_MAJOR==DB_VERSION_MAJOR_32);
    assert(DB_VERSION_MINOR==DB_VERSION_MINOR_32);
    printf("#define TOKUDB 1\n");
    printf("#define TOKUDB_NATIVE_H %d\n", TDB_NATIVE);
    dodefine(DB_VERSION_MAJOR);
    dodefine(DB_VERSION_MINOR);
    dodefine(DB_VERSION_PATCH);
    printf("#ifndef _TOKUDB_WRAP_H\n");
    printf("#define DB_VERSION_STRING \"Tokutek: TokuDB %d.%d.%d\"\n", DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH);
    printf("#else\n");
    printf("#define DB_VERSION_STRING_ydb \"Tokutek: TokuDB (wrapped bdb)\"\n");
    printf("#endif\n");

    if (0) {
	printf("#ifndef __BIT_TYPES_DEFINED__\n");
	printf("/* Define some int types if not provided by the system.  BIND does this, so we do it too. */\n");
	printf("typedef unsigned int u_int32_t;\n");
	printf("#endif\n");
    }

    //Typedef toku_off_t
    printf("#ifndef TOKU_OFF_T_DEFINED\n"
           "#define TOKU_OFF_T_DEFINED\n"
           "typedef int64_t toku_off_t;\n"
           "#endif\n");

    //printf("typedef struct __toku_db_btree_stat DB_BTREE_STAT;\n");
    printf("typedef struct __toku_db_env DB_ENV;\n");
    printf("typedef struct __toku_db_key_range DB_KEY_RANGE;\n");
    printf("typedef struct __toku_db_lsn DB_LSN;\n");
    printf("typedef struct __toku_db DB;\n");
    printf("typedef struct __toku_db_txn DB_TXN;\n");
    printf("typedef struct __toku_db_txn_active DB_TXN_ACTIVE;\n");
    printf("typedef struct __toku_db_txn_stat DB_TXN_STAT;\n");
    printf("typedef struct __toku_dbc DBC;\n");
    printf("typedef struct __toku_dbt DBT;\n");
    printf("typedef u_int32_t db_recno_t;\n");
    printf("typedef int(*YDB_CALLBACK_FUNCTION)(DBT const*, DBT const*, void*);\n");
    printf("typedef int(*YDB_HEAVISIDE_CALLBACK_FUNCTION)(DBT const *key, DBT const *value, void *extra_f, int r_h);\n");
    printf("typedef int(*YDB_HEAVISIDE_FUNCTION)(const DBT *key, const DBT *value, void *extra_h);\n");

    printf("#include <tdb-internal.h>\n");
    
    //stat64
    printf("typedef struct __toku_db_btree_stat64 {\n");
    printf("  u_int64_t bt_nkeys; /* how many unique keys (guaranteed only to be an estimate, even when flattened)          */\n");
    printf("  u_int64_t bt_ndata; /* how many key-value pairs (an estimate, but exact when flattened)                       */\n");
    printf("  u_int64_t bt_dsize; /* how big are the keys+values (not counting the lengths) (an estimate, unless flattened) */\n");
    printf("  u_int64_t bt_fsize; /* how big is the underlying file                                                         */\n");
    printf("} DB_BTREE_STAT64;\n");

    //engine status info
    printf("typedef struct __toku_engine_status {\n");
    printf("  char             now[26];                 /* time of engine status query (i.e. now)  */ \n");
    printf("  u_int64_t        ydb_lock_ctr;            /* how many times has ydb lock been taken/released */ \n");
    printf("  u_int64_t        max_possible_sleep;      /* max possible sleep time for ydb lock scheduling (constant) */ \n");
    printf("  u_int64_t        processor_freq_mhz;      /* clock frequency in MHz */ \n");
    printf("  u_int64_t        max_requested_sleep;     /* max sleep time requested, can be larger than max possible */ \n");
    printf("  u_int64_t        times_max_sleep_used;    /* number of times the max_possible_sleep was used to sleep */ \n");
    printf("  u_int64_t        total_sleepers;          /* total number of times a client slept for ydb lock scheduling */ \n");
    printf("  u_int64_t        total_sleep_time;        /* total time spent sleeping for ydb lock scheduling */ \n");
    printf("  u_int64_t        max_waiters;             /* max number of simultaneous client threads kept waiting for ydb lock  */ \n");
    printf("  u_int64_t        total_waiters;           /* total number of times a client thread waited for ydb lock  */ \n");
    printf("  u_int64_t        total_clients;           /* total number of separate client threads that use ydb lock  */ \n");
    printf("  u_int64_t        time_ydb_lock_held_unavailable;  /* number of times a thread migrated and theld is unavailable */ \n");
    printf("  u_int64_t        max_time_ydb_lock_held;  /* max time a client thread held the ydb lock  */ \n");
    printf("  u_int64_t        total_time_ydb_lock_held;/* total time client threads held the ydb lock  */ \n");
    printf("  u_int64_t        logger_lock_ctr;         /* how many times has logger lock been taken/released */ \n");
    printf("  u_int32_t        checkpoint_period;       /* delay between automatic checkpoints  */ \n");
    printf("  u_int32_t        checkpoint_footprint;    /* state of checkpoint procedure        */ \n");
    printf("  char             checkpoint_time_begin[26]; /* time of last checkpoint begin      */ \n");
    printf("  char             checkpoint_time_begin_complete[26]; /* time of last complete checkpoint begin      */ \n");
    printf("  char             checkpoint_time_end[26]; /* time of last checkpoint end      */ \n");
    printf("  u_int64_t        cachetable_lock_taken;   /* how many times has cachetable lock been taken */ \n");
    printf("  u_int64_t        cachetable_lock_released;/* how many times has cachetable lock been released */ \n");
    printf("  u_int64_t        cachetable_hit;          /* how many cache hits   */ \n");
    printf("  u_int64_t        cachetable_miss;         /* how many cache misses */ \n");
    printf("  u_int64_t        cachetable_misstime;     /* how many usec spent waiting for disk read because of cache miss */ \n");
    printf("  u_int64_t        cachetable_waittime;     /* how many usec spent waiting for another thread to release cache line */ \n");
    printf("  u_int64_t        cachetable_wait_reading; /* how many times get_and_pin waits for a node to be read */ \n");
    printf("  u_int64_t        cachetable_wait_writing; /* how many times get_and_pin waits for a node to be written */ \n");
    printf("  u_int64_t        puts;                    /* how many times has a newly created node been put into the cachetable */ \n");
    printf("  u_int64_t        prefetches;              /* how many times has a block been prefetched into the cachetable */ \n");
    printf("  u_int64_t        maybe_get_and_pins;      /* how many times has maybe_get_and_pin(_clean) been called */ \n");
    printf("  u_int64_t        maybe_get_and_pin_hits;  /* how many times has get_and_pin(_clean) returned with a node */ \n");
    printf("  int64_t          cachetable_size_current; /* sum of the sizes of the nodes represented in the cachetable */ \n");
    printf("  int64_t          cachetable_size_limit;   /* the limit to the sum of the node sizes */ \n");
    printf("  int64_t          cachetable_size_writing; /* the sum of the sizes of the nodes being written */ \n");
    printf("  int64_t          get_and_pin_footprint;   /* state of get_and_pin procedure */ \n");
    printf("  u_int32_t        range_locks_max;         /* max total number of range locks */ \n");
    printf("  u_int32_t        range_locks_max_per_db;  /* max range locks per dictionary */ \n");
    printf("  u_int32_t        range_locks_curr;        /* total range locks currently in use */ \n");
    printf("  u_int64_t        inserts;                 /* ydb row insert operations            */ \n");
    printf("  u_int64_t        deletes;                 /* ydb row delete operations            */ \n");
    printf("  u_int64_t        commits;                 /* ydb txn commit operations            */ \n");
    printf("  u_int64_t        aborts;                  /* ydb txn abort operations             */ \n");
    printf("  u_int64_t        point_queries;           /* ydb point queries                    */ \n");
    printf("  u_int64_t        sequential_queries;      /* ydb sequential queries               */ \n");
    printf("} ENGINE_STATUS;\n");


    print_dbtype();
//    print_db_notices();
    print_defines();

    printf("/* in wrap mode, top-level function txn_begin is renamed, but the field isn't renamed, so we have to hack it here.*/\n");
    printf("#ifdef _TOKUDB_WRAP_H\n#undef txn_begin\n#endif\n");
    assert(sizeof(db_btree_stat_fields32)==sizeof(db_btree_stat_fields64));
    // Don't produce db_btree_stat records.
    //print_struct("db_btree_stat", 0, db_btree_stat_fields32, db_btree_stat_fields64, sizeof(db_btree_stat_fields32)/sizeof(db_btree_stat_fields32[0]), 0);
    assert(sizeof(db_env_fields32)==sizeof(db_env_fields64));
    {
	const char *extra[]={
                             "int (*checkpointing_set_period)             (DB_ENV*, u_int32_t) /* Change the delay between automatic checkpoints.  0 means disabled. */",
                             "int (*checkpointing_get_period)             (DB_ENV*, u_int32_t*) /* Retrieve the delay between automatic checkpoints.  0 means disabled. */",
                             "int (*checkpointing_postpone)               (DB_ENV*) /* Use for 'rename table' or any other operation that must be disjoint from a checkpoint */",
                             "int (*checkpointing_resume)                 (DB_ENV*) /* Alert tokudb 'postpone' is no longer necessary */",
                             "int (*checkpointing_begin_atomic_operation) (DB_ENV*) /* Begin a set of operations (that must be atomic as far as checkpoints are concerned). i.e. inserting into every index in one table */",
                             "int (*checkpointing_end_atomic_operation)   (DB_ENV*) /* End   a set of operations (that must be atomic as far as checkpoints are concerned). */",
                             "int (*set_default_bt_compare)  (DB_ENV*,int (*bt_compare) (DB *, const DBT *, const DBT *)) /* Set default (key) comparison function for all DBs in this environment.  Required for RECOVERY since you cannot open the DBs manually. */",
                             "int (*set_default_dup_compare) (DB_ENV*,int (*bt_compare) (DB *, const DBT *, const DBT *)) /* Set default (val) comparison function for all DBs in this environment.  Required for RECOVERY since you cannot open the DBs manually. */",
			     "int (*get_engine_status)                    (DB_ENV*, ENGINE_STATUS*) /* Fill in status struct */",
			     "int (*get_engine_status_text)               (DB_ENV*, char*, int)     /* Fill in status text */",
			     "int (*get_iname)                            (DB_ENV* env, DBT* dname_dbt, DBT* iname_dbt) /* lookup existing iname */",
			     NULL};
        print_struct("db_env", 1, db_env_fields32, db_env_fields64, sizeof(db_env_fields32)/sizeof(db_env_fields32[0]), extra);
    }

    assert(sizeof(db_key_range_fields32)==sizeof(db_key_range_fields64));
    print_struct("db_key_range", 0, db_key_range_fields32, db_key_range_fields64, sizeof(db_key_range_fields32)/sizeof(db_key_range_fields32[0]), 0);

    assert(sizeof(db_lsn_fields32)==sizeof(db_lsn_fields64));
    {
	//const char *extra[] = {"u_int64_t lsn", NULL};
	print_struct("db_lsn", 0, db_lsn_fields32, db_lsn_fields64, sizeof(db_lsn_fields32)/sizeof(db_lsn_fields32[0]), 0);
    }

    assert(sizeof(dbt_fields32)==sizeof(dbt_fields64));
    print_struct("dbt", 0, dbt_fields32, dbt_fields64, sizeof(dbt_fields32)/sizeof(dbt_fields32[0]), 0);

    printf("typedef int (*toku_dbt_upgradef)(DB*,\n");
    printf("                                 u_int32_t old_version, const DBT *old_descriptor, const DBT *old_key, const DBT *old_val,\n");
    printf("                                 u_int32_t new_version, const DBT *new_descriptor, const DBT *new_key, const DBT *new_val);\n");

    assert(sizeof(db_fields32)==sizeof(db_fields64));
    {
	const char *extra[]={"int (*key_range64)(DB*, DB_TXN *, DBT *, u_int64_t *less, u_int64_t *equal, u_int64_t *greater, int *is_exact)",
			     "int (*stat64)(DB *, DB_TXN *, DB_BTREE_STAT64 *)",
			     "int (*pre_acquire_read_lock)(DB*, DB_TXN*, const DBT*, const DBT*, const DBT*, const DBT*)",
			     "int (*pre_acquire_table_lock)(DB*, DB_TXN*)",
			     "const DBT* (*dbt_pos_infty)(void) /* Return the special DBT that refers to positive infinity in the lock table.*/",
			     "const DBT* (*dbt_neg_infty)(void)/* Return the special DBT that refers to negative infinity in the lock table.*/",
                             "int (*delboth) (DB*, DB_TXN*, DBT*, DBT*, u_int32_t) /* Delete the key/value pair. */",
                             "int (*row_size_supported) (DB*, u_int32_t) /* Test whether a row size is supported. */",
                             "const DBT *descriptor /* saved row/dictionary descriptor for aiding in comparisons */",
                             "int (*set_descriptor) (DB*, u_int32_t version, const DBT* descriptor, toku_dbt_upgradef dbt_userformat_upgrade) /* set row/dictionary descriptor for a db.  Available only while db is open */",
			     "int (*getf_set)(DB*, DB_TXN*, u_int32_t, DBT*, YDB_CALLBACK_FUNCTION, void*) /* same as DBC->c_getf_set without a persistent cursor) */",
			     "int (*getf_get_both)(DB*, DB_TXN*, u_int32_t, DBT*, DBT*, YDB_CALLBACK_FUNCTION, void*) /* same as DBC->c_getf_get_both without a persistent cursor) */",
                             "int (*flatten)(DB*, DB_TXN*) /* Flatten a dictionary, similar to (but faster than) a table scan */",
			     NULL};
	print_struct("db", 1, db_fields32, db_fields64, sizeof(db_fields32)/sizeof(db_fields32[0]), extra);
    }

    assert(sizeof(db_txn_active_fields32)==sizeof(db_txn_active_fields64));
    print_struct("db_txn_active", 0, db_txn_active_fields32, db_txn_active_fields64, sizeof(db_txn_active_fields32)/sizeof(db_txn_active_fields32[0]), 0);
    assert(sizeof(db_txn_fields32)==sizeof(db_txn_fields64));
    {
	printf("struct txn_stat {\n  u_int64_t rolltmp_raw_count;\n};\n");
	const char *extra[] = {
            "int (*txn_stat)(DB_TXN *, struct txn_stat **)", 
            "struct { void *next, *prev; } open_txns",
            NULL,
        };
	print_struct("db_txn", 1, db_txn_fields32, db_txn_fields64, sizeof(db_txn_fields32)/sizeof(db_txn_fields32[0]), extra);
    }

    assert(sizeof(db_txn_stat_fields32)==sizeof(db_txn_stat_fields64));
    print_struct("db_txn_stat", 0, db_txn_stat_fields32, db_txn_stat_fields64, sizeof(db_txn_stat_fields32)/sizeof(db_txn_stat_fields32[0]), 0);

    {
	const char *extra[]={
			     "int (*c_getf_first)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_last)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
                             "int (*c_getf_next)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_next_dup)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_next_nodup)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_prev)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_prev_dup)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_prev_nodup)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_current)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_current_binding)(DBC *, u_int32_t, YDB_CALLBACK_FUNCTION, void *)",
                             "int (*c_getf_heaviside)(DBC *, u_int32_t, "
                                 "YDB_HEAVISIDE_CALLBACK_FUNCTION f, void *extra_f, "
                                 "YDB_HEAVISIDE_FUNCTION h, void *extra_h, int direction)",
			     "int (*c_getf_set)(DBC *, u_int32_t, DBT *, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_set_range)(DBC *, u_int32_t, DBT *, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_set_range_reverse)(DBC *, u_int32_t, DBT *, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_get_both)(DBC *, u_int32_t, DBT *, DBT *, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_get_both_range)(DBC *, u_int32_t, DBT *, DBT *, YDB_CALLBACK_FUNCTION, void *)",
			     "int (*c_getf_get_both_range_reverse)(DBC *, u_int32_t, DBT *, DBT *, YDB_CALLBACK_FUNCTION, void *)",
			     NULL};
	assert(sizeof(dbc_fields32)==sizeof(dbc_fields64));
	print_struct("dbc", 1, dbc_fields32, dbc_fields64, sizeof(dbc_fields32)/sizeof(dbc_fields32[0]), extra);
    }

    printf("#ifdef _TOKUDB_WRAP_H\n#define txn_begin txn_begin_tokudb\n#endif\n");
    printf("int db_env_create(DB_ENV **, u_int32_t) %s;\n", VISIBLE);
    printf("int db_create(DB **, DB_ENV *, u_int32_t) %s;\n", VISIBLE);
    printf("char *db_strerror(int) %s;\n", VISIBLE);
    printf("const char *db_version(int*,int *,int *) %s;\n", VISIBLE);
    printf("int log_compare (const DB_LSN*, const DB_LSN *) %s;\n", VISIBLE);
    printf("int db_env_set_func_fsync (int (*)(int)) %s;\n", VISIBLE);
    printf("int toku_set_trace_file (char *fname) %s;\n", VISIBLE);
    printf("int toku_close_trace_file (void) %s;\n", VISIBLE);
    printf("int db_env_set_func_free (void (*)(void*)) %s;\n", VISIBLE);
    printf("int db_env_set_func_malloc (void *(*)(size_t)) %s;\n", VISIBLE);
    printf("int db_env_set_func_pwrite (ssize_t (*)(int, const void *, size_t, toku_off_t)) %s;\n", VISIBLE);
    printf("int db_env_set_func_write (ssize_t (*)(int, const void *, size_t)) %s;\n", VISIBLE);
    printf("int db_env_set_func_realloc (void *(*)(void*, size_t)) %s;\n", VISIBLE);
    printf("void db_env_set_checkpoint_callback (void (*)(void*), void*) %s;\n", VISIBLE);
    printf("void db_env_set_checkpoint_callback2 (void (*)(void*), void*) %s;\n", VISIBLE);
    printf("#if defined(__cplusplus)\n}\n#endif\n");
    printf("#endif\n");
    return 0;
}

