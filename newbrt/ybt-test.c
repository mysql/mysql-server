/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#define _FILE_OFFSET_BITS 64

#include "ybt.h"
#include "memory.h"
#include <assert.h>
#include <string.h>

static void ybt_test0 (void) {
    void *v0=0,*v1=0;
    DBT  t0,t1;
    toku_init_dbt(&t0);
    toku_init_dbt(&t1);
    toku_dbt_set_value(&t0, "hello", 6, &v0);
    toku_dbt_set_value(&t1, "foo",   4, &v1);
    assert(t0.size==6);
    assert(strcmp(t0.data, "hello")==0); 
    assert(t1.size==4);
    assert(strcmp(t1.data, "foo")==0);

    toku_dbt_set_value(&t1, "byebye", 7, &v0);      /* Use v0, not v1 */
    // This assertion would be wrong, since v0 may have been realloc'd, and t0.data may now point
    // at the wrong place
    //assert(strcmp(t0.data, "byebye")==0);     /* t0's data should be changed too, since it used v0 */
    assert(strcmp(t1.data, "byebye")==0);

    toku_free(v0); toku_free(v1);
    memory_check_all_free();

    /* See if we can probe to find out how big something is by setting ulen=0 with YBT_USERMEM */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_USERMEM;
    t0.ulen  = 0;
    toku_dbt_set_value(&t0, "hello", 6, 0);
    assert(t0.data==0);
    assert(t0.size==6);

    /* Check realloc. */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_REALLOC;
    v0 = 0;
    toku_dbt_set_value(&t0, "internationalization", 21, &v0);
    assert(v0==0); /* Didn't change v0 */
    assert(t0.size==21);
    assert(strcmp(t0.data, "internationalization")==0);

    toku_dbt_set_value(&t0, "provincial", 11, &v0);
    assert(t0.size==11);
    assert(strcmp(t0.data, "provincial")==0);
    
    toku_free(t0.data);
    memory_check_all_free();
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    ybt_test0();
    return 0;
}
