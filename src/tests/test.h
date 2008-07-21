#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <db.h>
#include <assert.h>

#ifndef DB_YESOVERWRITE
#define DB_YESOVERWRITE 0
#endif
#ifndef DB_DELETE_ANY
#define DB_DELETE_ANY 0
#endif

int verbose=0;

#define UU(x) x __attribute__((__unused__))

#define CKERR(r) ({ if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0); })
#define CKERR2(r,r2) ({ if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, db_strerror(r), r2); assert(r==r2); })
#define CKERR2s(r,r2,r3) ({ if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, db_strerror(r), r2,r3); assert(r==r2||r==r3); })

// If the error code depends on BDB vs TDB use this
#ifdef USE_TDB
#define CKERR_depending(r,tdbexpect,bdbexpect) CKERR2(r,tdbexpect)
#else
#define CKERR_depending(r,tdbexpect,bdbexpect) CKERR2(r,bdbexpect)
#endif

void parse_args (int argc, const char *argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q] [-h]\n", argv0);
	    exit(resultcode);
	} else {
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

DBT *dbt_init_malloc(DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_MALLOC;
    return dbt;
}

// Simple LCG random number generator.  Not high quality, but good enough.
static u_int32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
}
static inline u_int32_t myrandom (void) {
    rstate = (279470275ull*(u_int64_t)rstate)%4294967291ull;
    return rstate;
}

int int_dbt_cmp(DB *db, const DBT *a, const DBT *b) {
  assert(db && a && b);
  assert(a->size == sizeof(int));
  assert(b->size == sizeof(int));

  int x = *(int *) a->data;
  int y = *(int *) b->data;

  return x - y;
}

typedef enum __toku_bool { FALSE=0, TRUE=1} BOOL;

#ifdef USE_TDB
#define SET_TRACE_FILE(x) toku_set_trace_file(x)
#define CLOSE_TRACE_FILE(x) toku_close_trace_file()
#else
#define SET_TRACE_FILE(x) ((void)0)
#define CLOSE_TRACE_FILE(x) ((void)0)
#endif
