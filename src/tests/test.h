/* -*- mode: C; c-basic-offset: 4 -*- */

#ifndef __TEST_H
#define __TEST_H

#if defined(__cilkplusplus) || defined(__cplusplus)
extern "C" {
#endif

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include <toku_portability.h>

#include <string.h>
#include <stdlib.h>
#include <toku_stdint.h>
#include <stdio.h>
#include <db.h>
#include <limits.h>
#include <errno.h>
#include "toku_htonl.h"
#include "toku_assert.h"
#include <signal.h>
#include <time.h>
#if defined(USE_TDB)
#include "ydb.h"
//TDB uses DB_NOTFOUND for c_del and DB_CURRENT errors.
#ifdef DB_KEYEMPTY
#error
#endif
#define DB_KEYEMPTY DB_NOTFOUND
#endif
#ifndef DB_DELETE_ANY
#define DB_DELETE_ANY 0
#endif

int verbose=0;

#define UU(x) x __attribute__((__unused__))

/*
 * Note that these evaluate the argument 'r' multiple times, so you cannot
 * do CKERR(function_call(args)).  I've added CHK macros below that are
 * safer and allow this usage.
 */
#define CKERR(r) do { if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0); } while (0)
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, db_strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, db_strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

/*
 * Helpers for defining pseudo-hygienic macros using a (gensym)-like
 * technique.
 */
#define _CONCAT(x, y) x ## y
#define CONCAT(x, y) _CONCAT(x, y)
#define GS(symbol) CONCAT(CONCAT(__gensym_, __LINE__), CONCAT(_, symbol))

/*
 * Check macros which use CKERR* underneath, but evaluate their arguments
 * only once.  They return the result of `expr' so they can be used like a
 * transparent function call like `r = CHK(db->get(db, ...))'.
 */
#define CHK(expr) ({                            \
            int (GS(r)) = (expr);               \
            CKERR(GS(r));                       \
            (GS(r));                            \
        })
#define CHK2(expr, expected) ({                 \
            int (GS(r)) = (expr);               \
            int (GS(r2)) = (expected);          \
            CKERR2((GS(r)), (GS(r2)));          \
            (GS(r));                            \
        })
#define CHK2s(expr, expected1, expected2) ({            \
            int (GS(r)) = (expr);                       \
            int (GS(r2)) = (expected1);                 \
            int (GS(r3)) = (expected2);                 \
            CKERR2((GS(r)), (GS(r2)), (GS(r3)));        \
            (GS(r));                                    \
        })

#define DEBUG_LINE do { \
    fprintf(stderr, "%s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); \
    fflush(stderr); \
} while (0)

// If the error code depends on BDB vs TDB use this
#ifdef USE_TDB
#define CKERR_depending(r,tdbexpect,bdbexpect) CKERR2(r,tdbexpect)
#else
#define CKERR_depending(r,tdbexpect,bdbexpect) CKERR2(r,bdbexpect)
#endif

static __attribute__((__unused__)) void
parse_args (int argc, char * const argv[]) {
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

static __attribute__((__unused__)) void 
print_engine_status(DB_ENV * UU(env)) {
#ifdef USE_TDB
    if (verbose) {  // verbose declared statically in this file
      int buffsize = 1024 * 32;
      char buff[buffsize];
      env->get_engine_status_text(env, buff, buffsize);
      printf("Engine status:\n");
      printf("%s", buff);
    }
#endif
}


static __attribute__((__unused__)) DBT *
dbt_init(DBT *dbt, const void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = (void*)data;
    dbt->size = size;
    return dbt;
}

static __attribute__((__unused__)) DBT *
dbt_init_malloc (DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_MALLOC;
    return dbt;
}

static __attribute__((__unused__)) DBT *
dbt_init_realloc (DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_REALLOC;
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

static __attribute__((__unused__)) int
int64_dbt_cmp (DB *db UU(), const DBT *a, const DBT *b) {
//    assert(db && a && b);
    assert(a);
    assert(b);
//    assert(db);

    assert(a->size == sizeof(int64_t));
    assert(b->size == sizeof(int64_t));

    int64_t x = *(int64_t *) a->data;
    int64_t y = *(int64_t *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static __attribute__((__unused__)) int
int_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
  assert(db && a && b);
  assert(a->size == sizeof(int));
  assert(b->size == sizeof(int));

  int x = *(int *) a->data;
  int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static __attribute__((__unused__)) int
uint_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
  assert(db && a && b);
  assert(a->size == sizeof(unsigned int));
  assert(b->size == sizeof(unsigned int));

  unsigned int x = *(unsigned int *) a->data;
  unsigned int y = *(unsigned int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

#include <stdbool.h>
// typedef enum __toku_bool { FALSE=0, TRUE=1} BOOL;
#define TRUE true
#define FALSE false
typedef bool BOOL;


#ifdef USE_TDB
#define SET_TRACE_FILE(x) toku_set_trace_file(x)
#define CLOSE_TRACE_FILE(x) toku_close_trace_file()
#else
#define SET_TRACE_FILE(x) ((void)0)
#define CLOSE_TRACE_FILE(x) ((void)0)
#endif

#include <memory.h>

unsigned int seed = 0xFEEDFACE;

static u_int64_t __attribute__((__unused__))
random64(void) {
    static int seeded = 0;
    if (!seeded) {
        seeded = 1;
        srandom(seed);
    }
    //random() generates 31 bits of randomness (low order)
    u_int64_t low     = random();
    u_int64_t high    = random();
    u_int64_t twobits = random();
    u_int64_t ret     = low | (high<<31) | (twobits<<62); 
    return ret;
}

static __attribute__((__unused__))
double get_tdiff(void) {
    static struct timeval prev={0,0};
    if (prev.tv_sec==0) {
	gettimeofday(&prev, 0);
	return 0.0;
    } else {
	struct timeval now;
	gettimeofday(&now, 0);
	double diff = now.tv_sec - prev.tv_sec + 1e-6*(now.tv_usec - prev.tv_usec);
	prev = now;
	return diff;
    }
}

static __attribute__((__unused__))
void format_time(const time_t *timer, char *buf) {
    ctime_r(timer, buf);
    size_t len = strlen(buf);
    assert(len < 26);
    char end;

    assert(len>=1);
    end = buf[len-1];
    while (end == '\n' || end == '\r') {
        buf[len-1] = '\0';
        len--;
        assert(len>=1);
        end = buf[len-1];
    }
}

static __attribute__((__unused__))
void print_time_now(void) {
    char timestr[80];
    time_t now = time(NULL);
    format_time(&now, timestr);
    printf("%s", timestr);
}

//Simulate as hard a crash as possible.
//Choices:
//  raise(SIGABRT)
//  kill -SIGKILL $pid
//  divide by 0
//  null dereference
//  abort()
//  assert(FALSE) (from <assert.h>)
//  assert(FALSE) (from <toku_assert.h>)
//
//Linux:
//  abort() and both assert(FALSE) cause FILE buffers to be flushed and written to disk: Unacceptable
//Windows:
//  None of them cause file buffers to be flushed/written to disk, however
//  abort(), assert(FALSE) <assert.h>, null dereference, and divide by 0 cause popups requiring user intervention during tests: Unacceptable
//
//kill -SIGKILL $pid is annoying (and so far untested)
//
//raise(SIGABRT) has the downside that perhaps it could be caught?
//I'm choosing raise(SIGABRT), followed by divide by 0, followed by null dereference, followed by all the others just in case one gets caught.
static void UU()
toku_hard_crash_on_purpose(void) {
#if TOKU_WINDOWS
    TerminateProcess(GetCurrentProcess(), 137);
#else
    raise(SIGKILL); //Does not flush buffers on linux; cannot be caught.
#endif
    {
        int zero = 0;
        int infinity = 1/zero;
        fprintf(stderr, "Force use of %d\n", infinity);
        fflush(stderr); //Make certain the string is calculated.
    }
    {
        void * intothevoid = NULL;
        (*(int*)intothevoid)++;
        fprintf(stderr, "Force use of *(%p) = %d\n", intothevoid, *(int*)intothevoid);
        fflush(stderr);
    }
    abort();
    fprintf(stderr, "This line should never be printed\n");
    fflush(stderr);
}

static void UU()
multiply_locks_for_n_dbs(DB_ENV *env, int num_dbs) {
    int r;
    uint32_t current_max_locks;
    r = env->get_lk_max_locks(env, &current_max_locks);
    CKERR(r);
    r = env->set_lk_max_locks(env, current_max_locks * num_dbs);
    CKERR(r);
#if defined(USE_TDB)
    uint64_t current_max_lock_memory;
    r = env->get_lk_max_memory(env, &current_max_lock_memory);
    CKERR(r);
    r = env->set_lk_max_memory(env, current_max_lock_memory * num_dbs);
    CKERR(r);
#endif
}

static inline void
default_parse_args (int argc, char * const argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

/* Some macros for evaluating blocks or functions within the scope of a
 * transaction. */
#define IN_TXN_COMMIT(env, parent, txn, flags, expr) ({                 \
            DB_TXN *(txn);                                              \
            CHK((env)->txn_begin((env), (parent), &(txn), (flags)));    \
            (expr);                                                     \
            CHK((txn)->commit((txn), 0));                               \
        })

#define IN_TXN_ABORT(env, parent, txn, flags, expr) ({                  \
            DB_TXN *(txn);                                              \
            CHK((env)->txn_begin((env), (parent), &(txn), (flags)));    \
            (expr);                                                     \
            CHK((txn)->abort(txn));                                     \
        })

#if defined(__cilkplusplus) || defined(__cplusplus)
}
#endif

int test_main (int argc, char * const argv[]);
int
#if defined(__cilkplusplus)
cilk_main(int argc, char *argv[]) 
#else
main(int argc, char * const argv[]) 
#endif
{
    int r;
#if IS_TDB && TOKU_WINDOWS
    int rinit = toku_ydb_init();
    CKERR(rinit);
#endif
#if !IS_TDB && DB_VERSION_MINOR==4 && DB_VERSION_MINOR == 7
    r = db_env_set_func_malloc(toku_malloc);   assert(r==0);
    r = db_env_set_func_free(toku_free);      assert(r==0);
    r = db_env_set_func_realloc(toku_realloc);   assert(r==0);
#endif
    toku_os_initialize_settings(1);
    r = test_main(argc, argv);
#if IS_TDB && TOKU_WINDOWS
    int rdestroy = toku_ydb_destroy();
    CKERR(rdestroy);
#endif
    return r;
}

#endif // __TEST_H
