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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "test.h"

#include <util/omt.h>

static void
parse_args (int argc, const char *argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
        int resultcode=0;
        if (strcmp(argv[1], "-v")==0) {
            verbose++;
        } else if (strcmp(argv[1], "-q")==0) {
            verbose = 0;
        } else if (strcmp(argv[1], "-h")==0) {
        do_usage:
            fprintf(stderr, "Usage:\n%s [-v|-h]\n", argv0);
            exit(resultcode);
        } else {
            resultcode=1;
            goto do_usage;
        }
        argc--;
        argv++;
    }
}
/* End ".h like" stuff. */

struct value {
    uint32_t number;
};
#define V(x) ((struct value *)(x))

enum rand_type {
    TEST_RANDOM,
    TEST_SORTED,
    TEST_IDENTITY
};
enum close_when_done {
    CLOSE_WHEN_DONE,
    KEEP_WHEN_DONE
};
enum create_type {
    STEAL_ARRAY,
    BATCH_INSERT,
    INSERT_AT,
    INSERT_AT_ALMOST_RANDOM,
};

/* Globals */
typedef void *OMTVALUE;
toku::omt<OMTVALUE> *global_omt;
OMTVALUE*       global_values = NULL;
struct value*   global_nums   = NULL;
uint32_t       global_length;

static void
cleanup_globals (void) {
    assert(global_values);
    toku_free(global_values);
    global_values = NULL;
    assert(global_nums);
    toku_free(global_nums);
    global_nums = NULL;
}

/* Some test wrappers */
struct functor {
    int (*f)(OMTVALUE, uint32_t, void *);
    void *v;
};
int call_functor(const OMTVALUE &v, uint32_t idx, functor *const ftor);
int call_functor(const OMTVALUE &v, uint32_t idx, functor *const ftor) {
    return ftor->f(const_cast<OMTVALUE>(v), idx, ftor->v);
}
static int omt_iterate(toku::omt<void *> *omt, int (*f)(OMTVALUE, uint32_t, void*), void*v) {
    struct functor ftor = { .f = f, .v = v };
    return omt->iterate<functor, call_functor>(&ftor);
}

struct heftor {
    int (*h)(OMTVALUE, void *v);
    void *v;
};
int call_heftor(const OMTVALUE &v, const heftor &htor);
int call_heftor(const OMTVALUE &v, const heftor &htor) {
    return htor.h(const_cast<OMTVALUE>(v), htor.v);
}
static int omt_insert(toku::omt<void *> *omt, OMTVALUE value, int(*h)(OMTVALUE, void*v), void *v, uint32_t *index) {
    struct heftor htor = { .h = h, .v = v };
    return omt->insert<heftor, call_heftor>(value, htor, index);
}
static int omt_find_zero(toku::omt<void *> *V, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    return V->find_zero<heftor, call_heftor>(htor, value, index);
}
static int omt_find(toku::omt<void *> *V, int (*h)(OMTVALUE, void*extra), void*extra, int direction, OMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    return V->find<heftor, call_heftor>(htor, direction, value, index);
}
static int omt_split_at(toku::omt<void *> *omt, toku::omt<void *> **newomtp, uint32_t index) {
    toku::omt<void *> *XMALLOC(newomt);
    int r = omt->split_at(newomt, index);
    if (r != 0) {
        toku_free(newomt);
    } else {
        *newomtp = newomt;
    }
    return r;
}
static int omt_merge(toku::omt<void *> *leftomt, toku::omt<void *> *rightomt, toku::omt<void *> **newomtp) {
    toku::omt<void *> *XMALLOC(newomt);
    newomt->merge(leftomt, rightomt);
    toku_free(leftomt);
    toku_free(rightomt);
    *newomtp = newomt;
    return 0;
}

const unsigned int random_seed = 0xFEADACBA;

static void
init_init_values (unsigned int seed, uint32_t num_elements) {
    srandom(seed);

    cleanup_globals();

    XMALLOC_N(num_elements, global_values);
    XMALLOC_N(num_elements, global_nums);
    global_length = num_elements;
}

static void
init_identity_values (unsigned int seed, uint32_t num_elements) {
    uint32_t   i;

    init_init_values(seed, num_elements);

    for (i = 0; i < global_length; i++) {
        global_nums[i].number   = i;
        global_values[i]        = (OMTVALUE)&global_nums[i];
    }
}

static void
init_distinct_sorted_values (unsigned int seed, uint32_t num_elements) {
    uint32_t   i;

    init_init_values(seed, num_elements);

    uint32_t number = 0;

    for (i = 0; i < global_length; i++) {
        number          += (uint32_t)(random() % 32) + 1;
        global_nums[i].number   = number;
        global_values[i]        = (OMTVALUE)&global_nums[i];
    }
}

static void
init_distinct_random_values (unsigned int seed, uint32_t num_elements) {
    init_distinct_sorted_values(seed, num_elements);

    uint32_t   i;
    uint32_t   choice;
    uint32_t   choices;
    struct value temp;
    for (i = 0; i < global_length - 1; i++) {
        choices = global_length - i;
        choice  = random() % choices;
        if (choice != i) {
            temp         = global_nums[i];
            global_nums[i]      = global_nums[choice];
            global_nums[choice] = temp;
        }
    }
}

static void
init_globals (void) {
    XMALLOC_N(1, global_values);
    XMALLOC_N(1, global_nums);
    global_length = 1;
}

static void
test_close (enum close_when_done do_close) {
    if (do_close == KEEP_WHEN_DONE) {
        return;
    }
    assert(do_close == CLOSE_WHEN_DONE);
    global_omt->destroy();
    toku_free(global_omt);
}

static void
test_create (enum close_when_done do_close) {
    XMALLOC(global_omt);
    global_omt->create();
    test_close(do_close);
}

static void
test_create_size (enum close_when_done do_close) {
    test_create(KEEP_WHEN_DONE);
    assert(global_omt->size() == 0);
    test_close(do_close);
}

static void
test_create_insert_at_almost_random (enum close_when_done do_close) {
    uint32_t i;
    int r;
    uint32_t size = 0;

    test_create(KEEP_WHEN_DONE);
    r = global_omt->insert_at(global_values[0], global_omt->size()+1);
    CKERR2(r, EINVAL);
    r = global_omt->insert_at(global_values[0], global_omt->size()+2);
    CKERR2(r, EINVAL);
    for (i = 0; i < global_length/2; i++) {
        assert(size==global_omt->size());
        r = global_omt->insert_at(global_values[i], i);
        CKERR(r);
        assert(++size==global_omt->size());
        r = global_omt->insert_at(global_values[global_length-1-i], i+1);
        CKERR(r);
        assert(++size==global_omt->size());
    }
    r = global_omt->insert_at(global_values[0], global_omt->size()+1);
    CKERR2(r, EINVAL);
    r = global_omt->insert_at(global_values[0], global_omt->size()+2);
    CKERR2(r, EINVAL);
    assert(size==global_omt->size());
    test_close(do_close);
}

static void
test_create_insert_at_sequential (enum close_when_done do_close) {
    uint32_t i;
    int r;
    uint32_t size = 0;

    test_create(KEEP_WHEN_DONE);
    r = global_omt->insert_at(global_values[0], global_omt->size()+1);
    CKERR2(r, EINVAL);
    r = global_omt->insert_at(global_values[0], global_omt->size()+2);
    CKERR2(r, EINVAL);
    for (i = 0; i < global_length; i++) {
        assert(size==global_omt->size());
        r = global_omt->insert_at(global_values[i], i);
        CKERR(r);
        assert(++size==global_omt->size());
    }
    r = global_omt->insert_at(global_values[0], global_omt->size()+1);
    CKERR2(r, EINVAL);
    r = global_omt->insert_at(global_values[0], global_omt->size()+2);
    CKERR2(r, EINVAL);
    assert(size==global_omt->size());
    test_close(do_close);
}

static void
test_create_from_sorted_array (enum create_type create_choice, enum close_when_done do_close) {
    global_omt = NULL;

    if (create_choice == BATCH_INSERT) {
        XMALLOC(global_omt);
        global_omt->create_from_sorted_array(global_values, global_length);
    }
    else if (create_choice == STEAL_ARRAY) {
        XMALLOC(global_omt);
        OMTVALUE* XMALLOC_N(global_length, values_copy);
        memcpy(values_copy, global_values, global_length*sizeof(*global_values));
        global_omt->create_steal_sorted_array(&values_copy, global_length, global_length);
        assert(values_copy==NULL);
    }
    else if (create_choice == INSERT_AT) {
        test_create_insert_at_sequential(KEEP_WHEN_DONE);
    }
    else if (create_choice == INSERT_AT_ALMOST_RANDOM) {
        test_create_insert_at_almost_random(KEEP_WHEN_DONE);
    }
    else {
        assert(false);
    }

    assert(global_omt!=NULL);
    test_close(do_close);
}

static void
test_create_from_sorted_array_size (enum create_type create_choice, enum close_when_done do_close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    assert(global_omt->size()==global_length);
    test_close(do_close);
}    

static void
test_fetch_verify (toku::omt<void *> *omtree, OMTVALUE* val, uint32_t len ) {
    uint32_t i;
    int r;
    OMTVALUE v = (OMTVALUE)&i;
    OMTVALUE oldv = v;

    assert(len == omtree->size());
    for (i = 0; i < len; i++) {
        assert(oldv!=val[i]);
        v = NULL;
        r = omtree->fetch(i, &v);
        CKERR(r);
        assert(v != NULL);
        assert(v != oldv);
        assert(v == val[i]);
        assert(V(v)->number == V(val[i])->number);
        v = oldv;
    }

    for (i = len; i < len*2; i++) {
        v = oldv;
        r = omtree->fetch(i, &v);
        CKERR2(r, EINVAL);
        assert(v == oldv);
    }

}

static void
test_create_fetch_verify (enum create_type create_choice, enum close_when_done do_close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    test_fetch_verify(global_omt, global_values, global_length);
    test_close(do_close);
}

static int iterate_helper_error_return = 1;

static int
iterate_helper (OMTVALUE v, uint32_t idx, void* extra) {
    if (extra == NULL) return iterate_helper_error_return;
    OMTVALUE* vals = (OMTVALUE *)extra;
    assert(v != NULL);
    assert(v == vals[idx]);
    assert(V(v)->number == V(vals[idx])->number);
    return 0;
}

static void
test_iterate_verify (toku::omt<void *> *omtree, OMTVALUE* vals, uint32_t len) {
    int r;
    iterate_helper_error_return = 0;
    r = omt_iterate(omtree, iterate_helper, (void*)vals);
    CKERR(r);
    iterate_helper_error_return = 0xFEEDABBA;
    r = omt_iterate(omtree, iterate_helper, NULL);
    if (!len) {
        CKERR2(r, 0);
    }
    else {
        CKERR2(r, iterate_helper_error_return);
    }
}

static void
test_create_iterate_verify (enum create_type create_choice, enum close_when_done do_close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    test_iterate_verify(global_omt, global_values, global_length);
    test_close(do_close);
}


static void
permute_array (uint32_t* arr, uint32_t len) {
    //
    // create a permutation of 0...size-1
    //
    uint32_t i = 0;
    for (i = 0; i < len; i++) {
        arr[i] = i;
    }
    for (i = 0; i < len - 1; i++) {
        uint32_t choices = len - i;
        uint32_t choice  = random() % choices;
        if (choice != i) {
            uint32_t temp = arr[i];
            arr[i]      = arr[choice];
            arr[choice] = temp;
        }
    }
}

static void
test_create_set_at (enum create_type create_choice, enum close_when_done do_close) {
    uint32_t i = 0;

    struct value*   old_nums   = NULL;
    XMALLOC_N(global_length, old_nums);

    uint32_t* perm = NULL;
    XMALLOC_N(global_length, perm);

    OMTVALUE* old_values = NULL;
    XMALLOC_N(global_length, old_values);
    
    permute_array(perm, global_length);

    //
    // These are going to be the new global_values
    //
    for (i = 0; i < global_length; i++) {
        old_nums[i] = global_nums[i];
        old_values[i] = &old_nums[i];        
        global_values[i] = &old_nums[i];
    }
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    int r;
    r = global_omt->set_at(global_values[0], global_length);
    CKERR2(r,EINVAL);    
    r = global_omt->set_at(global_values[0], global_length+1);
    CKERR2(r,EINVAL);    
    for (i = 0; i < global_length; i++) {
        uint32_t choice = perm[i];
        global_values[choice] = &global_nums[choice];
        global_nums[choice].number = (uint32_t)random();
        r = global_omt->set_at(global_values[choice], choice);
        CKERR(r);
        test_iterate_verify(global_omt, global_values, global_length);
        test_fetch_verify(global_omt, global_values, global_length);
    }
    r = global_omt->set_at(global_values[0], global_length);
    CKERR2(r,EINVAL);    
    r = global_omt->set_at(global_values[0], global_length+1);
    CKERR2(r,EINVAL);    

    toku_free(perm);
    toku_free(old_values);
    toku_free(old_nums);

    test_close(do_close);
}

static int
insert_helper (OMTVALUE value, void* extra_insert) {
    OMTVALUE to_insert = (OMTVALUE)extra_insert;
    assert(to_insert);

    if (V(value)->number < V(to_insert)->number) return -1;
    if (V(value)->number > V(to_insert)->number) return +1;
    return 0;
}

static void
test_create_insert (enum close_when_done do_close) {
    uint32_t i = 0;

    uint32_t* perm = NULL;
    XMALLOC_N(global_length, perm);

    permute_array(perm, global_length);

    test_create(KEEP_WHEN_DONE);
    int r;
    uint32_t size = global_length;
    global_length = 0;
    while (global_length < size) {
        uint32_t choice = perm[global_length];
        OMTVALUE to_insert = &global_nums[choice];
        uint32_t idx = UINT32_MAX;

        assert(global_length==global_omt->size());
        r = omt_insert(global_omt, to_insert, insert_helper, to_insert, &idx);
        CKERR(r);
        assert(idx <= global_length);
        if (idx > 0) {
            assert(V(to_insert)->number > V(global_values[idx-1])->number);
        }
        if (idx < global_length) {
            assert(V(to_insert)->number < V(global_values[idx])->number);
        }
        global_length++;
        assert(global_length==global_omt->size());
        /* Make room */
        for (i = global_length-1; i > idx; i--) {
            global_values[i] = global_values[i-1];
        }
        global_values[idx] = to_insert;
        test_fetch_verify(global_omt, global_values, global_length);
        test_iterate_verify(global_omt, global_values, global_length);

        idx = UINT32_MAX;
        r = omt_insert(global_omt, to_insert, insert_helper, to_insert, &idx);
        CKERR2(r, DB_KEYEXIST);
        assert(idx < global_length);
        assert(V(global_values[idx])->number == V(to_insert)->number);
        assert(global_length==global_omt->size());

        test_iterate_verify(global_omt, global_values, global_length);
        test_fetch_verify(global_omt, global_values, global_length);
    }

    toku_free(perm);

    test_close(do_close);
}

static void
test_create_delete_at (enum create_type create_choice, enum close_when_done do_close) {
    uint32_t i = 0;
    int r = ENOSYS;
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

    assert(global_length == global_omt->size());
    r = global_omt->delete_at(global_length);
    CKERR2(r,EINVAL);
    assert(global_length == global_omt->size());
    r = global_omt->delete_at(global_length+1);
    CKERR2(r,EINVAL);
    while (global_length > 0) {
        assert(global_length == global_omt->size());
        uint32_t index_to_delete = random()%global_length;
        r = global_omt->delete_at(index_to_delete);
        CKERR(r);
        for (i = index_to_delete+1; i < global_length; i++) {
            global_values[i-1] = global_values[i];
        }
        global_length--;
        test_fetch_verify(global_omt, global_values, global_length);
        test_iterate_verify(global_omt, global_values, global_length);
    }
    assert(global_length == 0);
    assert(global_length == global_omt->size());
    r = global_omt->delete_at(global_length);
    CKERR2(r, EINVAL);
    assert(global_length == global_omt->size());
    r = global_omt->delete_at(global_length+1);
    CKERR2(r, EINVAL);
    test_close(do_close);
}

static void
test_split_merge (enum create_type create_choice, enum close_when_done do_close) {
    int r = ENOSYS;
    uint32_t i = 0;
    toku::omt<void *> *left_split = NULL;
    toku::omt<void *> *right_split = NULL;
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

    for (i = 0; i <= global_length; i++) {
        r = omt_split_at(global_omt, &right_split, global_length+1);
        CKERR2(r,EINVAL);
        r = omt_split_at(global_omt, &right_split, global_length+2);
        CKERR2(r,EINVAL);

        //
        // test successful split
        //
        r = omt_split_at(global_omt, &right_split, i);
        CKERR(r);
        left_split = global_omt;
        global_omt = NULL;
        assert(left_split->size() == i);
        assert(right_split->size() == global_length - i);
        test_fetch_verify(left_split, global_values, i);
        test_iterate_verify(left_split, global_values, i);
        test_fetch_verify(right_split, &global_values[i], global_length - i);
        test_iterate_verify(right_split, &global_values[i], global_length - i);
        //
        // verify that new global_omt's cannot do bad splits
        //
        r = omt_split_at(left_split, &global_omt, i+1);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == global_length - i);
        r = omt_split_at(left_split, &global_omt, i+2);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == global_length - i);
        r = omt_split_at(right_split, &global_omt, global_length - i + 1);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == global_length - i);
        r = omt_split_at(right_split, &global_omt, global_length - i + 1);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == global_length - i);

        //
        // test merge
        //
        r = omt_merge(left_split,right_split,&global_omt);
        CKERR(r);
        left_split = NULL;
        right_split = NULL;
        assert(global_omt->size() == global_length);
        test_fetch_verify(global_omt, global_values, global_length);
        test_iterate_verify(global_omt, global_values, global_length);
    }
    test_close(do_close);
}


static void
init_values (enum rand_type rand_choice) {
    const uint32_t test_size = 100;
    if (rand_choice == TEST_RANDOM) {
        init_distinct_random_values(random_seed, test_size);
    }
    else if (rand_choice == TEST_SORTED) {
        init_distinct_sorted_values(random_seed, test_size);
    }
    else if (rand_choice == TEST_IDENTITY) {
        init_identity_values(       random_seed, test_size);
    }
    else assert(false);
}

static void
test_create_array (enum create_type create_choice, enum rand_type rand_choice) {
    /* ********************************************************************** */
    init_values(rand_choice);
    test_create_from_sorted_array(     create_choice, CLOSE_WHEN_DONE);
    test_create_from_sorted_array_size(create_choice, CLOSE_WHEN_DONE);
    /* ********************************************************************** */
    init_values(rand_choice);
    test_create_fetch_verify(          create_choice, CLOSE_WHEN_DONE);
    /* ********************************************************************** */
    init_values(rand_choice);
    test_create_iterate_verify(        create_choice, CLOSE_WHEN_DONE);
    /* ********************************************************************** */
    init_values(rand_choice);
    test_create_set_at(                create_choice, CLOSE_WHEN_DONE);
    /* ********************************************************************** */
    init_values(rand_choice);
    test_create_delete_at(             create_choice, CLOSE_WHEN_DONE);
    /* ********************************************************************** */
    init_values(rand_choice);
    test_create_insert(                               CLOSE_WHEN_DONE);
    /* ********************************************************************** */
    init_values(rand_choice);
    test_split_merge(                  create_choice, CLOSE_WHEN_DONE);
}

typedef struct {
    uint32_t first_zero;
    uint32_t first_pos;
} h_extra;


static int
test_heaviside (OMTVALUE v_omt, void* x) {
    OMTVALUE v = (OMTVALUE) v_omt;
    h_extra* extra = (h_extra*)x;
    assert(v && x);
    assert(extra->first_zero <= extra->first_pos);

    uint32_t value = V(v)->number;
    if (value < extra->first_zero) return -1;
    if (value < extra->first_pos) return 0;
    return 1;
}

static void
heavy_extra (h_extra* extra, uint32_t first_zero, uint32_t first_pos) {
    extra->first_zero = first_zero;
    extra->first_pos  = first_pos;
}

static void
test_find_dir (int dir, void* extra, int (*h)(OMTVALUE, void*),
	       int r_expect, bool idx_will_change, uint32_t idx_expect,
	       uint32_t number_expect, bool UU(cursor_valid)) {
    uint32_t idx     = UINT32_MAX;
    uint32_t old_idx = idx;
    OMTVALUE omt_val;
    int r;

    omt_val = NULL;

    /* Verify we can pass NULL value. */
    omt_val = NULL;
    idx      = old_idx;
    if (dir == 0) {
        r = omt_find_zero(global_omt, h, extra,      NULL, &idx);
    }
    else {
        r = omt_find(     global_omt, h, extra, dir, NULL, &idx);
    }
    CKERR2(r, r_expect);
    if (idx_will_change) {
        assert(idx == idx_expect);
    }
    else {
        assert(idx == old_idx);
    }
    assert(omt_val == NULL);
    
    /* Verify we can pass NULL idx. */
    omt_val  = NULL;
    idx      = old_idx;
    if (dir == 0) {
        r = omt_find_zero(global_omt, h, extra,      &omt_val, 0);
    }
    else {
        r = omt_find(     global_omt, h, extra, dir, &omt_val, 0);
    }
    CKERR2(r, r_expect);
    assert(idx == old_idx);
    if (r == DB_NOTFOUND) {
        assert(omt_val == NULL);
    }
    else {
        assert(V(omt_val)->number == number_expect);
    }

    /* Verify we can pass NULL both. */
    omt_val  = NULL;
    idx      = old_idx;
    if (dir == 0) {
        r = omt_find_zero(global_omt, h, extra,      NULL, 0);
    }
    else {
        r = omt_find(     global_omt, h, extra, dir, NULL, 0);
    }
    CKERR2(r, r_expect);
    assert(idx == old_idx);
    assert(omt_val == NULL);
}

static void
test_find (enum create_type create_choice, enum close_when_done do_close) {
    h_extra extra;
    init_identity_values(random_seed, 100);
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

/*
    -...-
        A
*/
    heavy_extra(&extra, global_length, global_length);
    test_find_dir(-1, &extra, test_heaviside, 0,           true,  global_length-1, global_length-1, true);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, false, 0,        0,        false);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, true,  global_length,   global_length,   false);


/*
    +...+
    B
*/
    heavy_extra(&extra, 0, 0);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, false, 0, 0, false);
    test_find_dir(+1, &extra, test_heaviside, 0,           true,  0, 0, true);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, true,  0, 0, false);

/*
    0...0
    C
*/
    heavy_extra(&extra, 0, global_length);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, false, 0, 0, false);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, false, 0, 0, false);
    test_find_dir(0,  &extra, test_heaviside, 0,           true,  0, 0, true);

/*
    -...-0...0
        AC
*/
    heavy_extra(&extra, global_length/2, global_length);
    test_find_dir(-1, &extra, test_heaviside, 0,           true,  global_length/2-1, global_length/2-1, true);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, false, 0,          0,          false);
    test_find_dir(0,  &extra, test_heaviside, 0,           true,  global_length/2,   global_length/2,   true);

/*
    0...0+...+
    C    B
*/
    heavy_extra(&extra, 0, global_length/2);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, false, 0,        0,        false);
    test_find_dir(+1, &extra, test_heaviside, 0,           true,  global_length/2, global_length/2, true);
    test_find_dir(0,  &extra, test_heaviside, 0,           true,  0,        0,        true);

/*
    -...-+...+
        AB
*/
    heavy_extra(&extra, global_length/2, global_length/2);
    test_find_dir(-1, &extra, test_heaviside, 0,           true, global_length/2-1, global_length/2-1, true);
    test_find_dir(+1, &extra, test_heaviside, 0,           true, global_length/2,   global_length/2,   true);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, true, global_length/2,   global_length/2,   false);

/*
    -...-0...0+...+
        AC    B
*/    
    heavy_extra(&extra, global_length/3, 2*global_length/3);
    test_find_dir(-1, &extra, test_heaviside, 0, true,   global_length/3-1,   global_length/3-1, true);
    test_find_dir(+1, &extra, test_heaviside, 0, true, 2*global_length/3,   2*global_length/3,   true);
    test_find_dir(0,  &extra, test_heaviside, 0, true,   global_length/3,     global_length/3,   true);

    /* Cleanup */
    test_close(do_close);
}

static void
runtests_create_choice (enum create_type create_choice) {
    test_create_array(create_choice, TEST_SORTED);
    test_create_array(create_choice, TEST_RANDOM);
    test_create_array(create_choice, TEST_IDENTITY);
    test_find(        create_choice, CLOSE_WHEN_DONE);
}

static void
test_clone(uint32_t nelts)
// Test that each clone operation gives the right data back.  If nelts is
// zero, also tests that you still get a valid omt back and that the way
// to deallocate it still works.
{
    toku::omt<void *> *src = NULL, *dest = NULL;
    int r;

    XMALLOC(src);
    src->create();
    for (long i = 0; i < nelts; ++i) {
        r = src->insert_at((OMTVALUE) i, i);
        assert_zero(r);
    }

    XMALLOC(dest);
    dest->clone(*src);
    assert(dest != NULL);
    assert(dest->size() == nelts);
    for (long i = 0; i < nelts; ++i) {
        OMTVALUE v;
        long l;
        r = dest->fetch(i, &v);
        assert_zero(r);
        l = (long) v;
        assert(l == i);
    }
    dest->destroy();
    toku_free(dest);
    src->destroy();
    toku_free(src);
}

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    init_globals();
    test_create(      CLOSE_WHEN_DONE);
    test_create_size( CLOSE_WHEN_DONE);
    runtests_create_choice(BATCH_INSERT);
    runtests_create_choice(STEAL_ARRAY);
    runtests_create_choice(INSERT_AT);
    runtests_create_choice(INSERT_AT_ALMOST_RANDOM);
    test_clone(0);
    test_clone(1);
    test_clone(1000);
    test_clone(10000);
    cleanup_globals();
    return 0;
}

