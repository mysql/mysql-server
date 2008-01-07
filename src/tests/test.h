#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <db.h>

#ifndef DB_YESOVERWRITE
#define DB_YESOVERWRITE 0
#endif
#ifndef DB_DELETE_ANY
#define DB_DELETE_ANY 0
#endif

int verbose=0;

#define CKERR(r) ({ if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0); })
#define CKERR2(r,r2) ({ if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, db_strerror(r), r2); assert(r==r2); })

void parse_args (int argc, const char *argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-h]\n", argv0);
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

// Simle LCG random number generator.  Not high quality, but good enough.
static uint32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
}
static inline uint32_t myrandom (void) {
    rstate = (279470275ull*(uint64_t)rstate)%4294967291ull;
    return rstate;
}
