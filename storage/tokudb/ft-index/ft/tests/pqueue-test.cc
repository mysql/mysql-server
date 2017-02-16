/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include "loader/loader-internal.h"
#include "loader/pqueue.h"

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

static inline DBT *dbt_init(DBT *dbt, void *data, uint32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

static void err_cb(DB *db, int which_db, int err, DBT *key, DBT *val, void *extra) {
    (void) db; (void) which_db; (void) err; (void) extra;
    (void) val;
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
    ft_compare_func compare = test_compare;
    int r;
    struct error_callback_s error_callback;
    ft_loader_init_error_callback(&error_callback);
    ft_loader_set_error_function(&error_callback, err_cb, NULL);

    r = pqueue_init(&pq, n_sources, 0, dest_db, compare, &error_callback);
    if (r) return r;

    DBT keys[n_sources];
    DBT vals[n_sources];
    DBT zero;
    toku_init_dbt_flags(&zero, DB_DBT_REALLOC);
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
    ft_loader_destroy_error_callback(&error_callback);

    // test 4 - find duplicate when inserting
    ft_loader_init_error_callback(&error_callback);
    ft_loader_set_error_function(&error_callback, err_cb, NULL);
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
    ft_loader_destroy_error_callback(&error_callback);

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
