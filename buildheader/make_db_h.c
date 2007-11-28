/* Make a db.h that will be link-time compatible with Sleepycat's Berkeley DB. */

#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>


void print_dbtype(void) {
    /* DBTYPE is mentioned by db_open.html */
    printf("typedef enum {\n");
    printf(" DB_BTREE=%d\n", DB_BTREE);
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
    dodefine(DB_EXCL);
    dodefine(DB_PRIVATE);
    dodefine(DB_RDONLY);
    dodefine(DB_RECOVER);
    dodefine(DB_THREAD);
    dodefine(DB_TXN_NOSYNC);

    dodefine(DB_LOCK_DEFAULT);
    dodefine(DB_LOCK_OLDEST);
    dodefine(DB_LOCK_RANDOM);

    dodefine(DB_DUP);
    dodefine(DB_DUPSORT);

    dodefine(DB_NOOVERWRITE);

    dodefine(DB_INIT_LOCK);
    dodefine(DB_INIT_LOG);
    dodefine(DB_INIT_MPOOL);
    dodefine(DB_INIT_TXN);
    dodefine(DB_USE_ENVIRON);
    dodefine(DB_USE_ENVIRON_ROOT);

    dodefine(DB_KEYEMPTY);
    dodefine(DB_KEYEXIST);
    dodefine(DB_LOCK_DEADLOCK);
    dodefine(DB_NOTFOUND);

    printf("#define DB_BADFORMAT -30500\n"); // private tokudb
    printf("#define DB_DELETE_ANY %d\n", 1<<16); // private tokudb

    dodefine(DB_FIRST);
    dodefine(DB_GET_BOTH);
    dodefine(DB_LAST);
    dodefine(DB_NEXT);
    dodefine(DB_NEXT_DUP);
    dodefine(DB_PREV);
    dodefine(DB_SET);
    dodefine(DB_SET_RANGE);
    dodefine(DB_RMW);
    printf("#endif\n");
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
#elif USE_MAJOR==4 && USE_MINOR==6
#include "sample_offsets_32_4_6.h"
#include "sample_offsets_64_4_6.h"
#else
#error
#endif

void print_struct (const char *structname, int need_internal, struct fieldinfo *fields32, struct fieldinfo *fields64, unsigned int N) {
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
		    printf("  struct __toku_%s_internal *i;\n", structname);
		    n_dummys--;
		    did_toku_internal=1;
		}
		if (n_dummys>0) printf("  void* __toku_dummy%d[%d];\n", dummy_counter++, n_dummys);
		diff64-=diff*2;
		diff32-=diff;
		
	    }
	    assert(diff32==diff64);
	    if (diff32>0) {
		printf("  char __toku_dummy%d[%d];\n", dummy_counter++, diff32);
	    }
	    current_32 = this_32;
	    current_64 = this_64;
	}
	if (this_32<current_32 || this_64<current_64) {
	    printf("Whoops this_32=%d this_64=%d\n", this_32, this_64);
	}
	if (i+1<N) {
	    assert(strcmp(fields32[i].decl, fields64[i].decl)==0);
	    printf("  %s; /* 32-bit offset=%d size=%d, 64=bit offset=%d size=%d */\n", fields32[i].decl, fields32[i].off, fields32[i].size, fields64[i].off, fields64[i].size);
	} else {
	    assert(fields32[i].decl==0);
	    assert(fields64[i].decl==0);
	}
	current_32 += fields32[i].size;
	current_64 += fields64[i].size;
    }
    {
	unsigned int this_32 = fields32[N-1].off;
	unsigned int this_64 = fields64[N-1].off;
	unsigned int diff32  = this_32-current_32;
	unsigned int diff64  = this_64-current_64;
	if (diff32>0 && diff32<diff64) {
	    unsigned int diff = diff64-diff32;
	    printf("  void* __toku_dummy%d[%d]; /* Padding at the end */ \n", dummy_counter++, diff/4);
	    diff64-=diff*2;
	    diff32-=diff;
	}
	if (diff32>0) {
	    printf("  char __toku_dummy%d[%d];  /* Padding at the end */ \n", dummy_counter++, diff32);
	    diff64-=diff32;
	    diff32=0;
	}
	if (diff64>0) printf("  /* %d more bytes of alignment in the 64-bit case. */\n", diff64);
	assert(diff64<8); /* there could be a few left from alignment. */ 
    }
    printf("};\n");
    assert(did_toku_internal || !need_internal);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    printf("#ifndef _DB_H\n");
    printf("#define _DB_H\n");
    printf("/* This code generated by make_db_h.   Copyright (c) 2007 Tokutek */\n");
    printf("#include <sys/types.h>\n");
    printf("/*stdio is needed for the FILE* in db->verify*/\n");
    printf("#include <stdio.h>\n");
    //printf("#include <inttypes.h>\n");
    printf("#if defined(__cplusplus)\nextern \"C\" {\n#endif\n");

    assert(DB_VERSION_MAJOR==DB_VERSION_MAJOR_32);
    assert(DB_VERSION_MINOR==DB_VERSION_MINOR_32);
    printf("#define TOKUDB 1\n");
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

    printf("typedef struct __toku_db_btree_stat DB_BTREE_STAT;\n");
    printf("typedef struct __toku_db_env DB_ENV;\n");
    printf("typedef struct __toku_db_key_range DB_KEY_RANGE;\n");
    printf("typedef struct __toku_db_lsn DB_LSN;\n");
    printf("typedef struct __toku_db DB;\n");
    printf("typedef struct __toku_db_txn DB_TXN;\n");
    printf("typedef struct __toku_db_txn_active DB_TXN_ACTIVE;\n");
    printf("typedef struct __toku_db_txn_stat DB_TXN_STAT;\n");
    printf("typedef struct __toku_dbc DBC;\n");
    printf("typedef struct __toku_dbt DBT;\n");
    print_dbtype();
//    print_db_notices();
    print_defines();

    printf("/* in wrap mode, top-level function txn_begin is renamed, but the field isn't renamed, so we have to hack it here.*/\n");
    printf("#ifdef _TOKUDB_WRAP_H\n#undef txn_begin\n#endif\n");
    assert(sizeof(db_btree_stat_fields32)==sizeof(db_btree_stat_fields64));
    print_struct("db_btree_stat", 0, db_btree_stat_fields32, db_btree_stat_fields64, sizeof(db_btree_stat_fields32)/sizeof(db_btree_stat_fields32[0]));
    assert(sizeof(db_env_fields32)==sizeof(db_env_fields64));
    print_struct("db_env", 1, db_env_fields32, db_env_fields64, sizeof(db_env_fields32)/sizeof(db_env_fields32[0]));

    assert(sizeof(db_key_range_fields32)==sizeof(db_key_range_fields64));
    print_struct("db_key_range", 0, db_key_range_fields32, db_key_range_fields64, sizeof(db_key_range_fields32)/sizeof(db_key_range_fields32[0]));

    assert(sizeof(db_lsn_fields32)==sizeof(db_lsn_fields64));
    print_struct("db_lsn", 0, db_lsn_fields32, db_lsn_fields64, sizeof(db_lsn_fields32)/sizeof(db_lsn_fields32[0]));

    assert(sizeof(db_fields32)==sizeof(db_fields64));
    print_struct("db", 1, db_fields32, db_fields64, sizeof(db_fields32)/sizeof(db_fields32[0]));

    assert(sizeof(db_txn_active_fields32)==sizeof(db_txn_active_fields64));
    print_struct("db_txn_active", 0, db_txn_active_fields32, db_txn_active_fields64, sizeof(db_txn_active_fields32)/sizeof(db_txn_active_fields32[0]));
    assert(sizeof(db_txn_fields32)==sizeof(db_txn_fields64));
    print_struct("db_txn", 1, db_txn_fields32, db_txn_fields64, sizeof(db_txn_fields32)/sizeof(db_txn_fields32[0]));

    assert(sizeof(db_txn_stat_fields32)==sizeof(db_txn_stat_fields64));
    print_struct("db_txn_stat", 0, db_txn_stat_fields32, db_txn_stat_fields64, sizeof(db_txn_stat_fields32)/sizeof(db_txn_stat_fields32[0]));

    assert(sizeof(dbc_fields32)==sizeof(dbc_fields64));
    print_struct("dbc", 1, dbc_fields32, dbc_fields64, sizeof(dbc_fields32)/sizeof(dbc_fields32[0]));

    assert(sizeof(dbt_fields32)==sizeof(dbt_fields64));
    print_struct("dbt", 0, dbt_fields32, dbt_fields64, sizeof(dbt_fields32)/sizeof(dbt_fields32[0]));

    printf("#ifdef _TOKUDB_WRAP_H\n#define txn_begin txn_begin_tokudb\n#endif\n");
    printf("int db_env_create(DB_ENV **, u_int32_t);\n");
    printf("int db_create(DB **, DB_ENV *, u_int32_t);\n");
    printf("char *db_strerror(int);\n");
    printf("const char *db_version(int*,int *,int *);\n");
    printf("int log_compare (const DB_LSN*, const DB_LSN *);\n");
    printf("#if defined(__cplusplus)\n}\n#endif\n");
    printf("#endif\n");
    return 0;
}

