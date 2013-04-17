/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

static void
cleanup_and_free(struct simple_dbt *v) {
    if (v->data) toku_free(v->data);
    v->data = NULL;
    v->len = 0;
}

static void
cleanup(struct simple_dbt *v) {
    v->data = NULL;
    v->len = 0;
}

static void ybt_test0 (void) {
    struct simple_dbt v0 = {0,0}, v1 = {0,0};
    DBT  t0,t1;
    toku_init_dbt(&t0);
    toku_init_dbt(&t1);
    {
	bytevec temp1 = "hello";
        toku_dbt_set(6, temp1, &t0, &v0);
    }
    {
        bytevec temp2 = "foo";
	toku_dbt_set(  4, temp2, &t1, &v1);
    }
    assert(t0.size==6);
    assert(strcmp(t0.data, "hello")==0); 
    assert(t1.size==4);
    assert(strcmp(t1.data, "foo")==0);

    {
        bytevec temp3 = "byebye";
	toku_dbt_set(7, temp3, &t1, &v0);      /* Use v0, not v1 */
    }
    // This assertion would be wrong, since v0 may have been realloc'd, and t0.data may now point
    // at the wrong place
    //assert(strcmp(t0.data, "byebye")==0);     /* t0's data should be changed too, since it used v0 */
    assert(strcmp(t1.data, "byebye")==0);

    cleanup_and_free(&v0);
    cleanup_and_free(&v1);
    

    /* See if we can probe to find out how big something is by setting ulen=0 with YBT_USERMEM */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_USERMEM;
    t0.ulen  = 0;
    {
        bytevec temp4 = "hello";
	toku_dbt_set(6, temp4, &t0, 0);
    }
    assert(t0.data==0);
    assert(t0.size==6);

    /* Check realloc. */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_REALLOC;
    cleanup(&v0);
    {
        bytevec temp5 = "internationalization";
	toku_dbt_set(21, temp5, &t0, &v0);
    }
    assert(v0.data==0); /* Didn't change v0 */
    assert(t0.size==21);
    assert(strcmp(t0.data, "internationalization")==0);

    {
        bytevec temp6 = "provincial";
	toku_dbt_set(11, temp6, &t0, &v0);
    }
    assert(t0.size==11);
    assert(strcmp(t0.data, "provincial")==0);
    
    toku_free(t0.data);
    
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    ybt_test0();
    return 0;
}
