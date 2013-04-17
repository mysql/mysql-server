/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include "includes.h"
#include "brtloader-internal.h"
#include "pqueue.h"

int found_dup = -1;

// simple compare func
static int test_compare(DB * UU(db), const DBT *dbta, const DBT *dbtb)
{
    int a = *((int*)dbta->data);
    int b = *((int*)dbtb->data);
    if ( a<b ) return -1;
    if ( a>b ) return 1;
    return 0;
}

static inline DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

static void err_cb(DB *db, int which_db, int err, DBT *key, DBT *val, void *extra) {
    db = db; which_db = which_db; err = err; extra = extra;
    val = val;
    found_dup = *(int *)key->data;
    if (verbose) printf("err_cb : key <%d> val <%d>\n", *(int *)key->data, *(int *)val->data);
}

static int run_test(void) 
{
    const int n_sources=10;
    pqueue_t      *pq;
    pqueue_node_t *pq_nodes = (pqueue_node_t *) toku_malloc( n_sources * sizeof(pqueue_node_t));
    pqueue_node_t *node;
    DB *dest_db = NULL;
    brt_compare_func compare = test_compare;
    int r;
    struct error_callback_s error_callback;
    brt_loader_init_error_callback(&error_callback);
    brt_loader_set_error_function(&error_callback, err_cb, NULL);

    r = pqueue_init(&pq, n_sources, 0, dest_db, compare, &error_callback);
    if (r) return r;

    DBT keys[n_sources];
    DBT vals[n_sources];
    DBT zero = {.data=0, .flags=DB_DBT_REALLOC, .size=0, .ulen=0};
    int key_data[10] = {0, 4, 8, 9, 5, 1, 2, 6, 7, 3};

    for (int i=0;i<n_sources; i++) {
        if (verbose) printf("%d ", key_data[i]);
	keys[i] = zero; 
	vals[i] = zero; 
        dbt_init(&keys[i], &key_data[i], sizeof(int));
    }
    if (verbose) printf("\n");

    // test 1 : fill it up, then empty it out
    for (int i=0; i<n_sources; i++) {
        pq_nodes[i].key = &keys[i];
        pq_nodes[i].val = &vals[i];
        pq_nodes[i].i   = i;
        pqueue_insert(pq, &pq_nodes[i]);
    }

    for (int i=0; i<n_sources; i++) {
        r = pqueue_pop(pq, &node);   assert(r==0);
        if (verbose) printf("%d : %d\n", i, *(int*)(node->key->data));
        if ( *(int*)(node->key->data) != i ) { 
            if (verbose) printf("FAIL\n"); return -1; 
        }
    }
    pqueue_free(pq);
    if (verbose) printf("test1 : PASS\n");

    // test 2 : fill it, then empty and reload, then empty
    {
	r = pqueue_init(&pq, n_sources, 0, dest_db, compare, &error_callback);
	if (r) return r;
    }

    DBT more_keys[20];
    DBT more_vals[20];
    int more_key_data[20] = {0, 4, 8, 9, 5, 1, 2, 6, 7, 3, 10, 11, 14, 13, 12, 17, 19, 15, 18, 16};
    for (int i=0; i<20; i++) {
	more_keys[i] = zero; 
	more_vals[i] = zero; 
        dbt_init(&more_keys[i], &more_key_data[i], sizeof(int));
    }
    
    for (int i=0; i<10; i++) {
        pq_nodes[i].key = &more_keys[i];
        pq_nodes[i].val = &more_vals[i];
        pq_nodes[i].i   = i;
        pqueue_insert(pq, &pq_nodes[i]);
    }

    for (int i=0; i<5; i++) {
        r = pqueue_pop(pq, &node);   assert(r==0);
        if ( *(int *)node->key->data != i ) { printf("FAIL\n"); return -1; }
        if (verbose) printf("%d : %d\n", i, *(int*)node->key->data);
    }

    int n;
    for (int i=5; i<15; i++) {
        r = pqueue_pop(pq, &node);   assert(r==0);
        if ( *(int *)node->key->data != i ) { printf("FAIL\n"); return -1; }
        if (verbose) printf("%d : %d\n", i, *(int*)node->key->data);
        n = node->i;
        pq_nodes[n].key = &more_keys[i+5];
        pq_nodes[n].val = &more_vals[i+5];
        pqueue_insert(pq, &pq_nodes[n]);
    }

    for (int i=15; i<20; i++) {
        r = pqueue_pop(pq, &node);   assert(r==0);
        if ( *(int*)node->key->data != i ) { printf("FAIL\n"); return -1; }
        if (verbose) printf("%d : %d\n", i, *(int*)node->key->data);
    }
    if (verbose) printf("test2 : PASS\n");
    pqueue_free(pq);

    // test 3 : put in a dup
    {
	r = pqueue_init(&pq, 10, 0, dest_db, compare, &error_callback);
	if (r) return r;
    }

    DBT keys3[10];
    DBT vals3[10];
    int key_data3[10] = {0, 1, 2, 3, 4, 5, 6, 6, 8, 9}; // dup is 6
    int val_data3[10];
    
    for (int i=0; i<10; i++) {
	keys3[i] = zero; 
        vals3[i] = zero;
        val_data3[i] = i;
        dbt_init(&keys3[i], &key_data3[i], sizeof(int));
        dbt_init(&vals3[i], &val_data3[i], sizeof(int));
    }
    int ii;
    for (ii=0; ii<10; ii++) {
        pq_nodes[ii].key = &keys3[ii];
        pq_nodes[ii].val = &vals3[ii];
        pq_nodes[ii].i   = ii;
        r = pqueue_insert(pq, &pq_nodes[ii]);
        if ( r != 0 ) goto found_duplicate6;
    }
    for (ii=0; ii<10; ii++) {
        r = pqueue_pop(pq, &node);
//        if (verbose) printf("%d : %d\n", ii, *(int*)node->key->data);
        if ( r != 0 ) goto found_duplicate6;
    }
found_duplicate6:
//    if (verbose) printf("%d : %d\n", ii, *(int*)node->key->data);
    if ( found_dup != 6 ) { printf("FAIL\n"); return -1; }
    if (verbose) printf("test3 : PASS\n");
    pqueue_free(pq);
    brt_loader_destroy_error_callback(&error_callback);

    // test 4 - find duplicate when inserting
    brt_loader_init_error_callback(&error_callback);
    brt_loader_set_error_function(&error_callback, err_cb, NULL);
    r = pqueue_init(&pq, 10, 0, dest_db, compare, &error_callback);        if (r) return r;

    found_dup = -1;
    DBT keys4[10];
    DBT vals4[10];
    int key_data4[10] = {0, 0, 2, 3, 4, 5, 6, 7, 8, 9}; // dup is 0
    int val_data4[10];
    
    for (int i=0; i<10; i++) {
	keys4[i] = zero; 
        vals4[i] = zero;
        val_data4[i] = i;
        dbt_init(&keys4[i], &key_data4[i], sizeof(int));
        dbt_init(&vals4[i], &val_data4[i], sizeof(int));
    }

    for (ii=0; ii<10; ii++) {
        pq_nodes[ii].key = &keys4[ii];
        pq_nodes[ii].val = &vals4[ii];
        pq_nodes[ii].i   = ii;
        r = pqueue_insert(pq, &pq_nodes[ii]);
        if ( r != 0 ) { 
//            if (verbose) printf("%d : %d\n", ii, *(int*)pq_nodes[ii].key->data);
            goto found_duplicate0;
        }
    }
    for (ii=0; ii<10; ii++) {
        r = pqueue_pop(pq, &node);
//        if (verbose) printf("%d : %d\n", ii, *(int*)node->key->data);
        if ( r != 0 ) goto found_duplicate0;
    }
found_duplicate0:
    if ( found_dup != 0 ) { printf("FAIL - found_dup : %d\n", found_dup); return -1; }
    if (verbose) printf("test4 : PASS\n");
    if (verbose) printf("PASS\n");
    pqueue_free(pq);
    toku_free(pq_nodes);
    brt_loader_destroy_error_callback(&error_callback);

    return 0;
}



int
test_main (int argc, const char *argv[]) {
    argc--; argv++;
    while (argc>0) {
    	if (strcmp(argv[0], "-v")==0) {
            verbose++;
        }
	argc--;
	argv++;
    }
    return run_test();
}
