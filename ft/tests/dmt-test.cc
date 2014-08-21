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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "test.h"

#include <util/dmt.h>

typedef void *DMTVALUE;

class dmtvalue_writer {
public:
    size_t get_size(void) const {
        return sizeof(DMTVALUE);
    }
    void write_to(DMTVALUE *const dest) const {
        *dest = value;
    }

    dmtvalue_writer(DMTVALUE _value)
        : value(_value) {
    }
    dmtvalue_writer(const uint32_t size UU(), DMTVALUE *const src)
        : value(*src) {
        paranoid_invariant(size == sizeof(DMTVALUE));
    }
private:
    const DMTVALUE value;
};

typedef toku::dmt<DMTVALUE, DMTVALUE, dmtvalue_writer> *DMT;

static int dmt_insert_at(DMT dmt, DMTVALUE value, uint32_t index) {
    dmtvalue_writer functor(value);
    return dmt->insert_at(functor, index);
}

static DMT dmt_create_from_sorted_array(DMTVALUE *values, uint32_t numvalues) {
    DMT XMALLOC(dmt);
    dmt->create();
    for (uint32_t i = 0; i < numvalues; i++) {
        dmt_insert_at(dmt, values[i], i);
    }
    return dmt;
}

struct heftor {
    int (*h)(DMTVALUE, void *v);
    void *v;
};

int call_heftor(const uint32_t size, const DMTVALUE &v, const heftor &htor);
int call_heftor(const uint32_t size, const DMTVALUE &v, const heftor &htor) {
    invariant(size == sizeof(DMTVALUE));
    return htor.h(const_cast<DMTVALUE>(v), htor.v);
}

static int dmt_insert(DMT dmt, DMTVALUE value, int(*h)(DMTVALUE, void*v), void *v, uint32_t *index) {
    struct heftor htor = { .h = h, .v = v };
    dmtvalue_writer functor(value);
    return dmt->insert<heftor, call_heftor>(functor, htor, index);
}

static int dmt_find_zero(DMT V, int (*h)(DMTVALUE, void*extra), void*extra, DMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    uint32_t ignore;
    return V->find_zero<heftor, call_heftor>(htor, &ignore, value, index);
}

static int dmt_find(DMT V, int (*h)(DMTVALUE, void*extra), void*extra, int direction, DMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    uint32_t ignore;
    return V->find<heftor, call_heftor>(htor, direction, &ignore, value, index);
}

static int dmt_split_at(DMT dmt, DMT *newdmtp, uint32_t index) {
    if (index > dmt->size()) { return EINVAL; }
    DMT XMALLOC(newdmt);
    newdmt->create();
    int r;

    for (uint32_t i = index; i < dmt->size(); i++) {
        DMTVALUE v;
        r = dmt->fetch(i, nullptr, &v);
        invariant_zero(r);
        r = dmt_insert_at(newdmt, v, i-index);
        invariant_zero(r);
    }
    if (dmt->size() > 0) {
        for (uint32_t i = dmt->size(); i > index; i--) {
            r = dmt->delete_at(i - 1);
            invariant_zero(r);
        }
    }
    r = 0;

    if (r != 0) {
        toku_free(newdmt);
    } else {
        *newdmtp = newdmt;
    }
    return r;
}

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
    BATCH_INSERT,
    INSERT_AT,
    INSERT_AT_ALMOST_RANDOM,
};

/* Globals */
DMT global_dmt;
DMTVALUE*       values = nullptr;
struct value*   nums   = nullptr;
uint32_t       length;

static void
cleanup_globals (void) {
    assert(values);
    toku_free(values);
    values = nullptr;
    assert(nums);
    toku_free(nums);
    nums = nullptr;
}

const unsigned int random_seed = 0xFEADACBA;

static void
init_init_values (unsigned int seed, uint32_t num_elements) {
    srandom(seed);

    cleanup_globals();

    MALLOC_N(num_elements, values);
    assert(values);
    MALLOC_N(num_elements, nums);
    assert(nums);
    length = num_elements;
}

static void
init_identity_values (unsigned int seed, uint32_t num_elements) {
    uint32_t   i;

    init_init_values(seed, num_elements);

    for (i = 0; i < length; i++) {
        nums[i].number   = i;
        values[i]        = (DMTVALUE)&nums[i];
    }
}

static void
init_distinct_sorted_values (unsigned int seed, uint32_t num_elements) {
    uint32_t   i;

    init_init_values(seed, num_elements);

    uint32_t number = 0;

    for (i = 0; i < length; i++) {
        number          += (uint32_t)(random() % 32) + 1;
        nums[i].number   = number;
        values[i]        = (DMTVALUE)&nums[i];
    }
}

static void
init_distinct_random_values (unsigned int seed, uint32_t num_elements) {
    init_distinct_sorted_values(seed, num_elements);

    uint32_t   i;
    uint32_t   choice;
    uint32_t   choices;
    struct value temp;
    for (i = 0; i < length - 1; i++) {
        choices = length - i;
        choice  = random() % choices;
        if (choice != i) {
            temp         = nums[i];
            nums[i]      = nums[choice];
            nums[choice] = temp;
        }
    }
}

static void
init_globals (void) {
    MALLOC_N(1, values);
    assert(values);
    MALLOC_N(1, nums);
    assert(nums);
    length = 1;
}

static void
test_close (enum close_when_done do_close) {
    if (do_close == KEEP_WHEN_DONE) return;
    assert(do_close == CLOSE_WHEN_DONE);
    global_dmt->destroy();
    toku_free(global_dmt);
    global_dmt = nullptr;
}

static void
test_create (enum close_when_done do_close) {
    XMALLOC(global_dmt);
    global_dmt->create();
    test_close(do_close);
}

static void
test_create_size (enum close_when_done do_close) {
    test_create(KEEP_WHEN_DONE);
    assert(global_dmt->size() == 0);
    test_close(do_close);
}

static void
test_create_insert_at_almost_random (enum close_when_done do_close) {
    uint32_t i;
    int r;
    uint32_t size = 0;

    test_create(KEEP_WHEN_DONE);
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+1);
    CKERR2(r, EINVAL);
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+2);
    CKERR2(r, EINVAL);
    for (i = 0; i < length/2; i++) {
        assert(size==global_dmt->size());
        r = dmt_insert_at(global_dmt, values[i], i);
        CKERR(r);
        assert(++size==global_dmt->size());
        r = dmt_insert_at(global_dmt, values[length-1-i], i+1);
        CKERR(r);
        assert(++size==global_dmt->size());
    }
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+1);
    CKERR2(r, EINVAL);
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+2);
    CKERR2(r, EINVAL);
    assert(size==global_dmt->size());
    test_close(do_close);
}

static void
test_create_insert_at_sequential (enum close_when_done do_close) {
    uint32_t i;
    int r;
    uint32_t size = 0;

    test_create(KEEP_WHEN_DONE);
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+1);
    CKERR2(r, EINVAL);
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+2);
    CKERR2(r, EINVAL);
    for (i = 0; i < length; i++) {
        assert(size==global_dmt->size());
        r = dmt_insert_at(global_dmt, values[i], i);
        CKERR(r);
        assert(++size==global_dmt->size());
    }
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+1);
    CKERR2(r, EINVAL);
    r = dmt_insert_at(global_dmt, values[0], global_dmt->size()+2);
    CKERR2(r, EINVAL);
    assert(size==global_dmt->size());
    test_close(do_close);
}

static void
test_create_from_sorted_array (enum create_type create_choice, enum close_when_done do_close) {
    global_dmt = nullptr;

    if (create_choice == BATCH_INSERT) {
        global_dmt = dmt_create_from_sorted_array(values, length);
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

    assert(global_dmt!=nullptr);
    test_close(do_close);
}

static void
test_create_from_sorted_array_size (enum create_type create_choice, enum close_when_done do_close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    assert(global_dmt->size()==length);
    test_close(do_close);
}    

static void
test_fetch_verify (DMT dmtree, DMTVALUE* val, uint32_t len ) {
    uint32_t i;
    int r;
    DMTVALUE v = (DMTVALUE)&i;
    DMTVALUE oldv = v;

    assert(len == dmtree->size());
    for (i = 0; i < len; i++) {
        assert(oldv!=val[i]);
        v = nullptr;
        r = dmtree->fetch(i, nullptr, &v);
        CKERR(r);
        assert(v != nullptr);
        assert(v != oldv);
        assert(v == val[i]);
        assert(V(v)->number == V(val[i])->number);
        v = oldv;
    }

    for (i = len; i < len*2; i++) {
        v = oldv;
        r = dmtree->fetch(i, nullptr, &v);
        CKERR2(r, EINVAL);
        assert(v == oldv);
    }

}

static void
test_create_fetch_verify (enum create_type create_choice, enum close_when_done do_close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    test_fetch_verify(global_dmt, values, length);
    test_close(do_close);
}

static int iterate_helper_error_return = 1;

static int
iterate_helper (DMTVALUE v, uint32_t idx, void* extra) {
    if (extra == nullptr) return iterate_helper_error_return;
    DMTVALUE* vals = (DMTVALUE *)extra;
    assert(v != nullptr);
    assert(v == vals[idx]);
    assert(V(v)->number == V(vals[idx])->number);
    return 0;
}
struct functor {
    int (*f)(DMTVALUE, uint32_t, void *);
    void *v;
};

int call_functor(const uint32_t size, const DMTVALUE &v, uint32_t idx, functor *const ftor);
int call_functor(const uint32_t size, const DMTVALUE &v, uint32_t idx, functor *const ftor) {
    invariant(size == sizeof(DMTVALUE));
    return ftor->f(const_cast<DMTVALUE>(v), idx, ftor->v);
}

static int dmt_iterate(DMT dmt, int (*f)(DMTVALUE, uint32_t, void*), void*v) {
    struct functor ftor = { .f = f, .v = v };
    return dmt->iterate<functor, call_functor>(&ftor);
}

static void
test_iterate_verify (DMT dmtree, DMTVALUE* vals, uint32_t len) {
    int r;
    iterate_helper_error_return = 0;
    r = dmt_iterate(dmtree, iterate_helper, (void*)vals);
    CKERR(r);
    iterate_helper_error_return = 0xFEEDABBA;
    r = dmt_iterate(dmtree, iterate_helper, nullptr);
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
    test_iterate_verify(global_dmt, values, length);
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

static int
dmt_set_at (DMT dmt, DMTVALUE value, uint32_t index) {
    int r = dmt->delete_at(index);
    if (r!=0) return r;
    return dmt_insert_at(dmt, value, index);
}

static void
test_create_set_at (enum create_type create_choice, enum close_when_done do_close) {
    uint32_t i = 0;

    struct value*   old_nums   = nullptr;
    MALLOC_N(length, old_nums);
    assert(nums);

    uint32_t* perm = nullptr;
    MALLOC_N(length, perm);
    assert(perm);

    DMTVALUE* old_values = nullptr;
    MALLOC_N(length, old_values);
    assert(old_values);
    
    permute_array(perm, length);

    //
    // These are going to be the new values
    //
    for (i = 0; i < length; i++) {
        old_nums[i] = nums[i];
        old_values[i] = &old_nums[i];        
        values[i] = &old_nums[i];
    }
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    int r;
    r = dmt_set_at (global_dmt, values[0], length);
    CKERR2(r,EINVAL);    
    r = dmt_set_at (global_dmt, values[0], length+1);
    CKERR2(r,EINVAL);    
    for (i = 0; i < length; i++) {
        uint32_t choice = perm[i];
        values[choice] = &nums[choice];
        nums[choice].number = (uint32_t)random();
        r = dmt_set_at (global_dmt, values[choice], choice);
        CKERR(r);
        test_iterate_verify(global_dmt, values, length);
        test_fetch_verify(global_dmt, values, length);
    }
    r = dmt_set_at (global_dmt, values[0], length);
    CKERR2(r,EINVAL);    
    r = dmt_set_at (global_dmt, values[0], length+1);
    CKERR2(r,EINVAL);    

    toku_free(perm);
    toku_free(old_values);
    toku_free(old_nums);

    test_close(do_close);
}

static int
insert_helper (DMTVALUE value, void* extra_insert) {
    DMTVALUE to_insert = (DMTVALUE)extra_insert;
    assert(to_insert);

    if (V(value)->number < V(to_insert)->number) return -1;
    if (V(value)->number > V(to_insert)->number) return +1;
    return 0;
}

static void
test_create_insert (enum close_when_done do_close) {
    uint32_t i = 0;

    uint32_t* perm = nullptr;
    MALLOC_N(length, perm);
    assert(perm);

    permute_array(perm, length);

    test_create(KEEP_WHEN_DONE);
    int r;
    uint32_t size = length;
    length = 0;
    while (length < size) {
        uint32_t choice = perm[length];
        DMTVALUE to_insert = &nums[choice];
        uint32_t idx = UINT32_MAX;

        assert(length==global_dmt->size());
        r = dmt_insert(global_dmt, to_insert, insert_helper, to_insert, &idx);
        CKERR(r);
        assert(idx <= length);
        if (idx > 0) {
            assert(V(to_insert)->number > V(values[idx-1])->number);
        }
        if (idx < length) {
            assert(V(to_insert)->number < V(values[idx])->number);
        }
        length++;
        assert(length==global_dmt->size());
        /* Make room */
        for (i = length-1; i > idx; i--) {
            values[i] = values[i-1];
        }
        values[idx] = to_insert;
        test_fetch_verify(global_dmt, values, length);
        test_iterate_verify(global_dmt, values, length);

        idx = UINT32_MAX;
        r = dmt_insert(global_dmt, to_insert, insert_helper, to_insert, &idx);
        CKERR2(r, DB_KEYEXIST);
        assert(idx < length);
        assert(V(values[idx])->number == V(to_insert)->number);
        assert(length==global_dmt->size());

        test_iterate_verify(global_dmt, values, length);
        test_fetch_verify(global_dmt, values, length);
    }

    toku_free(perm);

    test_close(do_close);
}

static void
test_create_delete_at (enum create_type create_choice, enum close_when_done do_close) {
    uint32_t i = 0;
    int r = ENOSYS;
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

    assert(length == global_dmt->size());
    r = global_dmt->delete_at(length);
    CKERR2(r,EINVAL);
    assert(length == global_dmt->size());
    r = global_dmt->delete_at(length+1);
    CKERR2(r,EINVAL);
    while (length > 0) {
        assert(length == global_dmt->size());
        uint32_t index_to_delete = random()%length;
        r = global_dmt->delete_at(index_to_delete);
        CKERR(r);
        for (i = index_to_delete+1; i < length; i++) {
            values[i-1] = values[i];
        }
        length--;
        test_fetch_verify(global_dmt, values, length);
        test_iterate_verify(global_dmt, values, length);
    }
    assert(length == 0);
    assert(length == global_dmt->size());
    r = global_dmt->delete_at(length);
    CKERR2(r, EINVAL);
    assert(length == global_dmt->size());
    r = global_dmt->delete_at(length+1);
    CKERR2(r, EINVAL);
    test_close(do_close);
}

static int dmt_merge(DMT leftdmt, DMT rightdmt, DMT *newdmtp) {
    DMT XMALLOC(newdmt);
    newdmt->create();
    int r;
    for (uint32_t i = 0; i < leftdmt->size(); i++) {
        DMTVALUE v;
        r = leftdmt->fetch(i, nullptr, &v);
        invariant_zero(r);
        r = newdmt->insert_at(v, i);
        invariant_zero(r);
    }
    uint32_t offset = leftdmt->size();
    for (uint32_t i = 0; i < rightdmt->size(); i++) {
        DMTVALUE v;
        r = rightdmt->fetch(i, nullptr, &v);
        invariant_zero(r);
        r = newdmt->insert_at(v, i+offset);
        invariant_zero(r);
    }
    leftdmt->destroy();
    rightdmt->destroy();
    toku_free(leftdmt);
    toku_free(rightdmt);
    *newdmtp = newdmt;
    return 0;
}

static void
test_split_merge (enum create_type create_choice, enum close_when_done do_close) {
    int r = ENOSYS;
    uint32_t i = 0;
    DMT left_split = nullptr;
    DMT right_split = nullptr;
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

    for (i = 0; i <= length; i++) {
        r = dmt_split_at(global_dmt, &right_split, length+1);
        CKERR2(r,EINVAL);
        r = dmt_split_at(global_dmt, &right_split, length+2);
        CKERR2(r,EINVAL);

        //
        // test successful split
        //
        r = dmt_split_at(global_dmt, &right_split, i);
        CKERR(r);
        left_split = global_dmt;
        global_dmt = nullptr;
        assert(left_split->size() == i);
        assert(right_split->size() == length - i);
        test_fetch_verify(left_split, values, i);
        test_iterate_verify(left_split, values, i);
        test_fetch_verify(right_split, &values[i], length - i);
        test_iterate_verify(right_split, &values[i], length - i);
        //
        // verify that new global_dmt's cannot do bad splits
        //
        r = dmt_split_at(left_split, &global_dmt, i+1);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == length - i);
        r = dmt_split_at(left_split, &global_dmt, i+2);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == length - i);
        r = dmt_split_at(right_split, &global_dmt, length - i + 1);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == length - i);
        r = dmt_split_at(right_split, &global_dmt, length - i + 1);
        CKERR2(r,EINVAL);
        assert(left_split->size() == i);
        assert(right_split->size() == length - i);

        //
        // test merge
        //
        r = dmt_merge(left_split,right_split,&global_dmt);
        CKERR(r);
        left_split = nullptr;
        right_split = nullptr;
        assert(global_dmt->size() == length);
        test_fetch_verify(global_dmt, values, length);
        test_iterate_verify(global_dmt, values, length);
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
test_heaviside (DMTVALUE v_dmt, void* x) {
    DMTVALUE v = (DMTVALUE) v_dmt;
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
test_find_dir (int dir, void* extra, int (*h)(DMTVALUE, void*),
	       int r_expect, bool idx_will_change, uint32_t idx_expect,
	       uint32_t number_expect, bool UU(cursor_valid)) {
    uint32_t idx     = UINT32_MAX;
    uint32_t old_idx = idx;
    DMTVALUE dmt_val;
    int r;

    dmt_val = nullptr;

    /* Verify we can pass nullptr value. */
    dmt_val = nullptr;
    idx      = old_idx;
    if (dir == 0) {
        r = dmt_find_zero(global_dmt, h, extra,      nullptr, &idx);
    }
    else {
        r = dmt_find(     global_dmt, h, extra, dir, nullptr, &idx);
    }
    CKERR2(r, r_expect);
    if (idx_will_change) {
        assert(idx == idx_expect);
    }
    else {
        assert(idx == old_idx);
    }
    assert(dmt_val == nullptr);
    
    /* Verify we can pass nullptr idx. */
    dmt_val  = nullptr;
    idx      = old_idx;
    if (dir == 0) {
        r = dmt_find_zero(global_dmt, h, extra,      &dmt_val, 0);
    }
    else {
        r = dmt_find(     global_dmt, h, extra, dir, &dmt_val, 0);
    }
    CKERR2(r, r_expect);
    assert(idx == old_idx);
    if (r == DB_NOTFOUND) {
        assert(dmt_val == nullptr);
    }
    else {
        assert(V(dmt_val)->number == number_expect);
    }

    /* Verify we can pass nullptr both. */
    dmt_val  = nullptr;
    idx      = old_idx;
    if (dir == 0) {
        r = dmt_find_zero(global_dmt, h, extra,      nullptr, 0);
    }
    else {
        r = dmt_find(     global_dmt, h, extra, dir, nullptr, 0);
    }
    CKERR2(r, r_expect);
    assert(idx == old_idx);
    assert(dmt_val == nullptr);
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
    heavy_extra(&extra, length, length);
    test_find_dir(-1, &extra, test_heaviside, 0,           true,  length-1, length-1, true);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, false, 0,        0,        false);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, true,  length,   length,   false);


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
    heavy_extra(&extra, 0, length);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, false, 0, 0, false);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, false, 0, 0, false);
    test_find_dir(0,  &extra, test_heaviside, 0,           true,  0, 0, true);

/*
    -...-0...0
        AC
*/
    heavy_extra(&extra, length/2, length);
    test_find_dir(-1, &extra, test_heaviside, 0,           true,  length/2-1, length/2-1, true);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, false, 0,          0,          false);
    test_find_dir(0,  &extra, test_heaviside, 0,           true,  length/2,   length/2,   true);

/*
    0...0+...+
    C    B
*/
    heavy_extra(&extra, 0, length/2);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, false, 0,        0,        false);
    test_find_dir(+1, &extra, test_heaviside, 0,           true,  length/2, length/2, true);
    test_find_dir(0,  &extra, test_heaviside, 0,           true,  0,        0,        true);

/*
    -...-+...+
        AB
*/
    heavy_extra(&extra, length/2, length/2);
    test_find_dir(-1, &extra, test_heaviside, 0,           true, length/2-1, length/2-1, true);
    test_find_dir(+1, &extra, test_heaviside, 0,           true, length/2,   length/2,   true);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, true, length/2,   length/2,   false);

/*
    -...-0...0+...+
        AC    B
*/    
    heavy_extra(&extra, length/3, 2*length/3);
    test_find_dir(-1, &extra, test_heaviside, 0, true,   length/3-1,   length/3-1, true);
    test_find_dir(+1, &extra, test_heaviside, 0, true, 2*length/3,   2*length/3,   true);
    test_find_dir(0,  &extra, test_heaviside, 0, true,   length/3,     length/3,   true);

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
// zero, also tests that you still get a valid DMT back and that the way
// to deallocate it still works.
{
    DMT src = nullptr, dest = nullptr;
    int r = 0;

    XMALLOC(src);
    src->create();
    for (long i = 0; i < nelts; ++i) {
        r = dmt_insert_at(src, (DMTVALUE) i, i);
        assert_zero(r);
    }

    XMALLOC(dest);
    dest->clone(*src);
    assert(dest->size() == nelts);
    for (long i = 0; i < nelts; ++i) {
        DMTVALUE v;
        long l;
        r = dest->fetch(i, nullptr, &v);
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
    runtests_create_choice(INSERT_AT);
    runtests_create_choice(INSERT_AT_ALMOST_RANDOM);
    test_clone(0);
    test_clone(1);
    test_clone(1000);
    test_clone(10000);
    cleanup_globals();
    return 0;
}

