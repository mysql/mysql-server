/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "memory.h"
#include "cachetable.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const int test_object_size = 1;

static CACHETABLE ct;

enum { N_PRESENT_LIMIT = 4, TRIALS=200, N_FILES=2 };
static int n_present=0;
static struct present_items {
    CACHEKEY key;
    CACHEFILE cf;
} present_items[N_PRESENT_LIMIT];

static void print_ints(void) __attribute__((__unused__));
static void print_ints(void) {
    int i;
    for (i=0; i<n_present; i++) {
	if (i==0) printf("{"); else printf(",");
	printf("{%lld,%p}", present_items[i].key, present_items[i].cf);
    }
    printf("}\n");
}

static void item_becomes_present(CACHEFILE cf, CACHEKEY key) {
    assert(n_present<N_PRESENT_LIMIT);
    present_items[n_present].cf     = cf;
    present_items[n_present].key    = key;
    n_present++;
}

static void item_becomes_not_present(CACHEFILE cf, CACHEKEY key) {
    int i;
    //printf("Removing {%4lld %16p}: Initially: ", key, cf); print_ints();
    assert(n_present<=N_PRESENT_LIMIT);
    for (i=0; i<n_present; i++) {
	if (present_items[i].cf==cf && present_items[i].key==key) {
	    present_items[i]=present_items[n_present-1];
	    n_present--;
	    //printf("                                    Finally: "); print_ints();
	    return;
	}
    }
    printf("Whoops, %p,%lld was already not present\n", cf ,key);
    abort();
}

static void file_is_not_present(CACHEFILE cf) {
    int i;
    for (i=0; i<n_present; i++) {
	assert(present_items[i].cf!=cf);
    }
}


static void flush_forchain (CACHEFILE f            __attribute__((__unused__)),
			    CACHEKEY  key,
			    void     *value,
			    long      size         __attribute__((__unused__)),
			    BOOL      write_me     __attribute__((__unused__)),
			    BOOL      keep_me      __attribute__((__unused__)),
			    LSN       modified_lsn __attribute__((__unused__)),
			    BOOL      rename_p     __attribute__((__unused__))) {
    int *v = value;
    //toku_cachetable_print_state(ct);
    //printf("Flush %lld %d\n", key, (int)value);
    assert((long)v==(long)key);
    item_becomes_not_present(f, key);
    //print_ints();
}

static int fetch_forchain (CACHEFILE f __attribute__((__unused__)), CACHEKEY key, void**value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    assert((long)extraargs==(long)key);
    *value = (void*)(long)key;
    written_lsn->lsn = 0;
    return 0;
}

static void verify_cachetable_against_present (void) {
    int i;
    for (i=0; i<n_present; i++) {
	void *v;
	int r;
	assert(toku_cachetable_maybe_get_and_pin(present_items[i].cf,
						 present_items[i].key,
						 &v)==0);
	r = toku_cachetable_unpin(present_items[i].cf, present_items[i].key, CACHETABLE_CLEAN, test_object_size);
    }
}


static void test_chaining (void) {
    /* Make sure that the hash chain and the LRU list don't get confused. */
    CACHEFILE f[N_FILES];
    enum { FILENAME_LEN=100 };
    char fname[N_FILES][FILENAME_LEN];
    int r;
    long i, trial;
    r = toku_create_cachetable(&ct, N_PRESENT_LIMIT, ZERO_LSN, NULL_LOGGER);    assert(r==0);
    for (i=0; i<N_FILES; i++) {
	r = snprintf(fname[i], FILENAME_LEN, "cachetabletest2.%ld.dat", i);
	assert(r>0 && r<FILENAME_LEN);
	unlink(fname[i]);
	r = toku_cachetable_openf(&f[i], ct, fname[i], O_RDWR|O_CREAT, 0777);   assert(r==0);
	}
    for (i=0; i<N_PRESENT_LIMIT; i++) {
	int fnum = i%N_FILES;
	//printf("%s:%d Add %d\n", __FILE__, __LINE__, i);
	r = toku_cachetable_put(f[fnum], i, (void*)i, test_object_size, flush_forchain, fetch_forchain, (void*)i); assert(r==0);
	item_becomes_present(f[fnum], i);
	r = toku_cachetable_unpin(f[fnum], i, CACHETABLE_CLEAN, test_object_size);                                                assert(r==0);
	//print_ints();
    }
    for (trial=0; trial<TRIALS; trial++) {
	if (n_present>0) {
	    // First touch some random ones
	    int whichone = random()%n_present;
	    void *value;
	    //printf("Touching %d (%lld, %p)\n", whichone, present_items[whichone].key, present_items[whichone].cf);
	    r = toku_cachetable_get_and_pin(present_items[whichone].cf,
					    present_items[whichone].key,
					    &value,
					    NULL,
					    flush_forchain,
					    fetch_forchain,
					    (void*)(long)present_items[whichone].key
					    );
	    assert(r==0);
	    r = toku_cachetable_unpin(present_items[whichone].cf,
				      present_items[whichone].key,
				      CACHETABLE_CLEAN, test_object_size);
	    assert(r==0);
	}

	i += 1+ random()%100;
	int fnum = i%N_FILES;
	// i is always incrementing, so we need not worry about inserting a duplicate
	//printf("%s:%d Add {%d,%p}\n", __FILE__, __LINE__, i, f[fnum]);
	r = toku_cachetable_put(f[fnum], i, (void*)i, test_object_size, flush_forchain, fetch_forchain, (void*)i); assert(r==0);
	item_becomes_present(f[fnum], i);
	//print_ints();
	//cachetable_print_state(ct);
	r = toku_cachetable_unpin(f[fnum], i, CACHETABLE_CLEAN, test_object_size);                                                assert(r==0);
	verify_cachetable_against_present();

	if (random()%10==0) {
	    i = random()%N_FILES;
	    //printf("Close %d (%p), now n_present=%d\n", i, f[i], n_present);
	    //print_ints();
	    CACHEFILE oldcf=f[i];
	    r = toku_cachefile_close(&f[i]);                            assert(r==0);
	    file_is_not_present(oldcf);
	    r = toku_cachetable_openf(&f[i], ct, fname[i], O_RDWR, 0777); assert(r==0);
	}
    }
    for (i=0; i<N_FILES; i++) {
	r = toku_cachefile_close(&f[i]); assert(r==0);
    }
    r = toku_cachetable_close(&ct); assert(r==0);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test_chaining();
    toku_malloc_cleanup();
    printf("ok\n");
    return 0;
}
