/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"

static void ybt_test0 (void) {
    void *v0=0,*v1=0;
    DBT  t0,t1;
    toku_init_dbt(&t0);
    toku_init_dbt(&t1);
    {
	bytevec temp1 = "hello";
	toku_dbt_set_value(&t0, &temp1, 6, &v0, FALSE);
    }
    {
        bytevec temp2 = "foo";
	toku_dbt_set_value(&t1, &temp2,   4, &v1, FALSE);
    }
    assert(t0.size==6);
    assert(strcmp(t0.data, "hello")==0); 
    assert(t1.size==4);
    assert(strcmp(t1.data, "foo")==0);

    {
        bytevec temp3 = "byebye";
	toku_dbt_set_value(&t1, &temp3, 7, &v0, FALSE);      /* Use v0, not v1 */
    }
    // This assertion would be wrong, since v0 may have been realloc'd, and t0.data may now point
    // at the wrong place
    //assert(strcmp(t0.data, "byebye")==0);     /* t0's data should be changed too, since it used v0 */
    assert(strcmp(t1.data, "byebye")==0);

    toku_free(v0); toku_free(v1);
    toku_memory_check_all_free();

    /* See if we can probe to find out how big something is by setting ulen=0 with YBT_USERMEM */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_USERMEM;
    t0.ulen  = 0;
    {
        bytevec temp4 = "hello";
	toku_dbt_set_value(&t0, &temp4, 6, 0, FALSE);
    }
    assert(t0.data==0);
    assert(t0.size==6);

    /* Check realloc. */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_REALLOC;
    v0 = 0;
    {
        bytevec temp5 = "internationalization";
	toku_dbt_set_value(&t0, &temp5, 21, &v0, FALSE);
    }
    assert(v0==0); /* Didn't change v0 */
    assert(t0.size==21);
    assert(strcmp(t0.data, "internationalization")==0);

    {
        bytevec temp6 = "provincial";
	toku_dbt_set_value(&t0, &temp6, 11, &v0, FALSE);
    }
    assert(t0.size==11);
    assert(strcmp(t0.data, "provincial")==0);
    
    toku_free(t0.data);
    toku_memory_check_all_free();
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    ybt_test0();
    return 0;
}
