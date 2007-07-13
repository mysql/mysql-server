#include "memory.h"
#include "cachetable.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct item {
    CACHEKEY key;
    char *something;
};

int expect_n_flushes=0;
CACHEKEY flushes[100];

static void expect1(CACHEKEY key) {
    expect_n_flushes=1;
    flushes[0]=key;
}
static void expectN(CACHEKEY key) {
    flushes[expect_n_flushes++]=key;
}

CACHEFILE expect_f;

static void flush (CACHEFILE f, CACHEKEY key, void*value, int write_me __attribute__((__unused__)), int keep_mee __attribute__((__unused__))) {
    struct item *it = value;
    int i;

    printf("Flushing %lld (it=>key=%lld)\n", key, it->key);

    assert(expect_f==f);
    assert(strcmp(it->something,"something")==0);
    assert(it->key==key);

    /* Verify that we expected the flush. */
    for (i=0; i<expect_n_flushes; i++) {
	if (key==flushes[i]) {
	    flushes[i] = flushes[expect_n_flushes-1];
	    expect_n_flushes--;
	    goto found_flush;
	}
    }
    printf("%lld was flushed, but I didn't expect it\n", key);
    abort();
 found_flush:
    my_free(value);
}

struct item *make_item (CACHEKEY key) {
    struct item *MALLOC(it);
    it->key=key;
    it->something="something";
    return it;
}

CACHEKEY did_fetch=-1;
int fetch (CACHEFILE f, CACHEKEY key, void**value, void*extraargs) {
    printf("Fetch %lld\n", key);
    assert (expect_f==f);
    assert((long)extraargs==23);
    *value = make_item(key);
    did_fetch=key;
    return 0;
}


void test0 (void) {
    void* t3=(void*)23;
    CACHETABLE t;
    CACHEFILE f;
    int r;
    char fname[] = "test.dat";
    r=create_cachetable(&t, 5);
    assert(r==0);
    unlink(fname);
    r = cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r==0);
    expect_f = f;

    expect_n_flushes=0;
    r=cachetable_put(f, 1, make_item(1), flush, fetch, t3);   /* 1P */        /* this is the lru list.  1 is pinned. */
    assert(r==0);
    assert(expect_n_flushes==0);

    expect_n_flushes=0;
    r=cachetable_put(f, 2, make_item(2), flush, fetch, t3);
    assert(r==0);
    r=cachetable_unpin(f, 2, 1);           /* 2U 1P */
    assert(expect_n_flushes==0);

    expect_n_flushes=0;
    r=cachetable_put(f, 3, make_item(3), flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 3P 2U 1P */   /* 3 is most recently used (pinned), 2 is next (unpinned), 1 is least recent (pinned) */

    expect_n_flushes=0;
    r=cachetable_put(f, 4, make_item(4), flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 4P 3P 2U 1P */

    expect_n_flushes=0;
    r=cachetable_put(f, 5, make_item(5), flush, fetch, t3);
    assert(r==0);
    r=cachetable_unpin(f, 5, 1);
    assert(r==0);
    r=cachetable_unpin(f, 3, 1);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 5U 4P 3U 2U 1P */

    expect1(2); /* 2 is the oldest unpinned item. */
    r=cachetable_put(f, 6, make_item(6), flush, fetch, t3);   /* 6P 5U 4P 3U 1P */
    assert(r==0);
    assert(expect_n_flushes==0);


    expect1(3);
    r=cachetable_put(f, 7, make_item(7), flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);
    r=cachetable_unpin(f, 7, 1);           /* 7U 6P 5U 4P 1P */
    assert(r==0);

    {
	void *item_v=0;
	expect_n_flushes=0;
	r=cachetable_get_and_pin(f, 5, &item_v, flush, fetch, t3);  /* 5P 7U 6P 4P 1P */
	assert(r==0);
	assert(((struct item *)item_v)->key==5);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
	assert(expect_n_flushes==0);
    }

    {
	void *item_v=0;
	r=cachetable_unpin(f, 4, 1);
	assert(r==0);
	expect1(4);
	did_fetch=-1;
	r=cachetable_get_and_pin(f, 2, &item_v, flush, fetch, t3);  /* 2p 5P 7U 6P 1P */
	assert(r==0);
	assert(did_fetch==2); /* Expect that 2 is fetched in. */
	assert(((struct item *)item_v)->key==2);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
	assert(expect_n_flushes==0);
    }
	
    r=cachetable_unpin(f, 2, 1);
    assert(r==0);
    r=cachetable_unpin(f ,5, 1);
    assert(r==0);
    r=cachetable_unpin(f, 6, 1);
    assert(r==0);
    r=cachetable_unpin(f, 1, 1);
    assert(r==0);
    r=cachetable_assert_all_unpinned(t);
    assert(r==0);

    printf("Closing\n");
    expect1(2);
    expectN(5);
    expectN(7);
    expectN(6);
    expectN(1);
    r=cachefile_close(f);
    assert(r==0);
    r=cachetable_close(t);
    assert(r==0);
    assert(expect_n_flushes==0);
    expect_f = 0; 
    memory_check_all_free();
}

static void flush_n (CACHEFILE f __attribute__((__unused__)), CACHEKEY key __attribute__((__unused__)), void *value, int write_me __attribute__((__unused__)), int keep_me __attribute__((__unused__))) {
    int *v = value;
    assert(*v==0);
}
static int fetch_n (CACHEFILE f __attribute__((__unused__)), CACHEKEY key __attribute__((__unused__)), void**value, void*extraargs) {
    assert((long)extraargs==42);
    *value=0;
    return 0;
}


void test_nested_pin (void) {
    void *f2=(void*)42;
    CACHETABLE t;
    CACHEFILE f;
    int i0, i1;
    int r;
    void *vv;
    char fname[] = "test.dat";
    r = create_cachetable(&t, 1);
    assert(r==0);
    unlink(fname);
    r = cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r==0);
    expect_f = f;

    i0=0; i1=0;
    r = cachetable_put(f, 1, &i0, flush_n, fetch_n, f2);
    assert(r==0);
    r = cachetable_get_and_pin(f, 1, &vv, flush_n, fetch_n, f2);
    assert(r==0);
    assert(vv==&i0);
    assert(i0==0);
    r = cachetable_unpin(f, 1, 0);
    assert(r==0);
    r = cachetable_put(f, 2, &i1, flush_n, fetch_n, f2);
    assert(r!=0); // previously pinned, we shouldn't be able to put.
    r = cachetable_unpin(f, 1, 0);
    assert(r==0);
    r = cachetable_put(f, 2, &i1, flush_n, fetch_n, f2);
    assert(r==0); // now it is unpinned, we can put it.
    
}


void null_flush (CACHEFILE cf  __attribute__((__unused__)),
		 CACHEKEY k    __attribute__((__unused__)),
		 void *v       __attribute__((__unused__)),
		 int write_me  __attribute__((__unused__)),
		 int keep_me   __attribute__((__unused__))) {
}
int add123_fetch (CACHEFILE cf __attribute__((__unused__)), CACHEKEY key, void **value, void*extraargs) {
    assert((long)extraargs==123);
    *value = (void*)((unsigned long)key+123L);
    return 0;
}
int add222_fetch (CACHEFILE cf __attribute__((__unused__)), CACHEKEY key, void **value, void*extraargs) {
    assert((long)extraargs==222);
    *value = (void*)((unsigned long)key+222L);
    return 0;
}


void test_multi_filehandles (void) {
    CACHETABLE t;
    CACHEFILE f1,f2,f3;
    char fname1[]="test.dat";
    char fname2[]="test2.dat";
    char fname3[]="test3.dat";
    int r;
    void *v;
    unlink(fname1);
    unlink(fname2);

    r = create_cachetable(&t, 4);                                 assert(r==0);
    r = cachetable_openf(&f1, t, fname1, O_RDWR|O_CREAT, 0777);   assert(r==0);
    r = link(fname1, fname2);                                     assert(r==0);
    r = cachetable_openf(&f2, t, fname2, O_RDWR|O_CREAT, 0777);   assert(r==0);
    r = cachetable_openf(&f3, t, fname3, O_RDWR|O_CREAT, 0777);   assert(r==0);

    assert(f1==f2);
    assert(f1!=f3);
    
    r = cachetable_put(f1, 1, (void*)124, null_flush, add123_fetch, (void*)123); assert(r==0);
    r = cachetable_get_and_pin(f2, 1, &v, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==124);
    r = cachetable_get_and_pin(f2, 2, &v, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==125);
    r = cachetable_get_and_pin(f3, 2, &v, null_flush, add222_fetch, (void*)222); assert(r==0);
    assert((unsigned long)v==224);
    r = cachetable_maybe_get_and_pin(f1, 2, &v); assert(r==0);
    assert((unsigned long)v==125);
    
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test0();
    test_nested_pin();
    test_multi_filehandles ();
    printf("ok\n");
    return 0;
}
