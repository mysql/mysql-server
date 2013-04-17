/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
// measure the lock tree memory footprint while acquiring point write locks.
// this test assumes that the lock tree lock limits and lock memory limites are big enough to store the locks
// without the need for lock escalation.
//
// example: ./test_footprint_point_write.tlog --max_locks 1000000 --max_lock_memory 1000000000 --nrows 1000000

#include <config.h>
#ifndef TOKU_ALLOW_DEPRECATED
#define TOKU_ALLOW_DEPRECATED
#endif
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif
#include "test.h"
#include <toku_byteswap.h>
#include <dlfcn.h>

static uint64_t htonl64(uint64_t x) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return bswap_64(x);
#else
#error
#endif
}

typedef size_t (*malloc_usable_size_fun_t)(void *p);
#if defined(HAVE_MALLOC_USABLE_SIZE)
size_t malloc_usable_size(void *p);
static malloc_usable_size_fun_t malloc_usable_size_f = malloc_usable_size;
#elif defined(HAVE_MALLOC_SIZE)
size_t malloc_size(void *p);
static malloc_usable_size_fun_t malloc_usable_size_f = malloc_size;
#endif

struct my_ltm_status {
    uint32_t max_locks, curr_locks;
    uint64_t max_lock_memory, curr_lock_memory;
};

static void my_ltm_get_status(toku_ltm *ltm, struct my_ltm_status *my_status) {
    LTM_STATUS_S status;
    toku_ltm_get_status(ltm, &status);
    my_status->max_locks        = status.status[LTM_LOCKS_LIMIT].value.num;
    my_status->curr_locks       = status.status[LTM_LOCKS_CURR].value.num;
    my_status->max_lock_memory  = status.status[LTM_LOCK_MEMORY_LIMIT].value.num;
    my_status->curr_lock_memory = status.status[LTM_LOCK_MEMORY_CURR].value.num;
}

static void *my_malloc(size_t s) {
    void * p = malloc(s);
    if (verbose) 
        printf("%s %lu %lu\n", __FUNCTION__, s, malloc_usable_size_f(p));
    return p;
}

static void *my_realloc(void *p, size_t s) {
    if (verbose)
        printf("%s %p %lu\n", __FUNCTION__, p, s);
    return realloc(p, s);
}

static void my_free(void *p) {
    if (verbose) 
        printf("%s %p %lu\n", __FUNCTION__, p, malloc_usable_size_f(p));
    free(p);
}

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 2;
    uint64_t max_lock_memory = 4096;
    uint64_t nrows = 1;
    bool do_malloc_trace = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoi(argv[++i]);
            continue;
        }        
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--malloc") == 0) {
            do_malloc_trace = true;
            continue;
        }
        assert(0);
    }

    if (do_malloc_trace) {
        toku_set_func_malloc(my_malloc);
        toku_set_func_free(my_free);
        toku_set_func_realloc(my_realloc);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic);
    assert(r == 0 && ltm);
    
    struct my_ltm_status s;
    my_ltm_get_status(ltm, &s);
    assert(s.max_locks == max_locks);
    assert(s.curr_locks == 0);
    assert(s.max_lock_memory == max_lock_memory);
    assert(s.curr_lock_memory == 0);

    toku_lock_tree *lt = NULL;
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, NULL, dbcmp, NULL, NULL, NULL);
    assert(r == 0 && lt);

    TXNID txn_a = 1;

    // acquire the locks on keys 1 .. nrows
    for (uint64_t i = 1; i <= nrows; i++) {
        uint64_t k = htonl64(i);
        DBT key = { .data = &k, .size = sizeof k };
        r = toku_lt_acquire_write_lock(lt, txn_a, &key);
        if (r != 0) {
            assert(r == TOKUDB_OUT_OF_LOCKS);
            break;
        }

        struct my_ltm_status t;
        my_ltm_get_status(ltm, &t);
        assert(t.max_locks == max_locks);
        assert(t.curr_locks == i);
        assert(t.max_lock_memory == max_lock_memory);
        assert(t.curr_lock_memory > s.curr_lock_memory);
        
        if (verbose)
            printf("%"PRIu64" %"PRIu64"\n", i, t.curr_lock_memory - s.curr_lock_memory);
        
        s = t;
    }
    

    // release the locks
    r = toku_lt_unlock_txn(lt, txn_a);  assert(r == 0);

    my_ltm_get_status(ltm, &s);
    assert(s.curr_locks == 0);

    // shutdown 
    toku_lt_remove_db_ref(lt);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
