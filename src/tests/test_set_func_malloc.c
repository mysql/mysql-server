/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#define DONT_DEPRECATE_MALLOC
#include "test.h"

/* Test to see if setting malloc works, and if dlmalloc works. */

#include <memory.h>
#include <db.h>
#ifdef USE_TDB
#include <dlmalloc.h>
#endif

static int malloc_counter=0;
static int realloc_counter=0;
static int free_counter=0;

static void *
bmalloc (size_t s)
{
    malloc_counter++;
    return malloc(s);
}

static void
bfree (void*p)
{
    free_counter++;
    free(p);
}

static void*
brealloc (void*p, size_t s)
{
    realloc_counter++;
    return realloc(p,s);
}

void
test1 (void)
{
    DB_ENV *env=0;
    int r;
    r = db_env_create(&env, 0);            assert(r==0);
    r = env->close(env, 0);                assert(r==0);
    assert(malloc_counter==0);
    assert(free_counter==0);
    assert(realloc_counter==0);

    r = db_env_set_func_malloc(bmalloc);   assert(r==0);
    r = db_env_create(&env, 0);            assert(r==0);
    r = env->close(env, 0);                assert(r==0);
    assert(malloc_counter>0);
    assert(free_counter==0);
    assert(realloc_counter==0);

    malloc_counter = realloc_counter = free_counter = 0;

    r = db_env_set_func_free(bfree);       assert(r==0);
    r = db_env_set_func_malloc(NULL);      assert(r==0);
    r = db_env_create(&env, 0);            assert(r==0);
    r = env->close(env, 0);                assert(r==0);
    assert(malloc_counter==0);
    assert(free_counter>=0);
    assert(realloc_counter==0);
    
    r = db_env_set_func_malloc(bmalloc);   assert(r==0);
    r = db_env_set_func_realloc(brealloc); assert(r==0);
    r = db_env_set_func_free(bfree);       assert(r==0);
    
#ifdef USE_TDB
    // toku_malloc isn't affected by calling the BDB set_fun_malloc calls.
    malloc_counter = realloc_counter = free_counter = 0;

    {
	void *x = toku_malloc(5); assert(x);	assert(malloc_counter==1 && free_counter==0 && realloc_counter==0);
	x = toku_realloc(x, 6);   assert(x);    assert(malloc_counter==1 && free_counter==0 && realloc_counter==1);
	toku_free(x);	                        assert(malloc_counter==1 && free_counter==1 && realloc_counter==1);
    }

    r = db_env_set_func_malloc(dlmalloc);   assert(r==0);
    r = db_env_set_func_realloc(dlrealloc); assert(r==0);
    r = db_env_set_func_free(dlfree);       assert(r==0);

    {
	char *x = toku_malloc(5);      assert(x);
	x = toku_realloc(x, 6);        assert(x);
	toku_free(x);
    }
#endif
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__)))
{
    test1();
    return 0;
}
