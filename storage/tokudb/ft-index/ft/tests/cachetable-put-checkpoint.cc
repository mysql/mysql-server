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
#include "cachetable-test.h"

//
// This test ensures that get_and_pin with dependent nodes works
// as intended with checkpoints, by having multiple threads changing
// values on elements in data, and ensure that checkpoints always get snapshots 
// such that the sum of all the elements in data are 0.
//

// The arrays

// must be power of 2 minus 1
#define NUM_ELEMENTS 127
// must be (NUM_ELEMENTS +1)/2 - 1
#define NUM_INTERNAL 63
#define NUM_MOVER_THREADS 4

int64_t data[NUM_ELEMENTS];
int64_t checkpointed_data[NUM_ELEMENTS];
PAIR data_pair[NUM_ELEMENTS];

uint32_t time_of_test;
bool run_test;

static void
put_callback_pair(
    CACHEKEY key,
    void *UU(v),
    PAIR p) 
{
    int64_t data_index = key.b;
    data_pair[data_index] = p;
}

static void 
clone_callback(
    void* value_data, 
    void** cloned_value_data, 
    long* clone_size,
    PAIR_ATTR* new_attr, 
    bool UU(for_checkpoint), 
    void* UU(write_extraargs)
    )
{
    new_attr->is_valid = false;
    int64_t* XMALLOC(data_val);
    *data_val = *(int64_t *)value_data;
    *cloned_value_data = data_val;
    *new_attr = make_pair_attr(8);
    *clone_size = 8;
}

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size,
       bool write_me,
       bool keep_me,
       bool checkpoint_me,
        bool UU(is_clone)
       ) {
    int64_t val_to_write = *(int64_t *)v;
    size_t data_index = (size_t)k.b;
    if (write_me) {
        usleep(10);
        *new_size = make_pair_attr(8);
        data[data_index] = val_to_write;
        if (checkpoint_me) checkpointed_data[data_index] = val_to_write;
    }
    if (!keep_me) {
        toku_free(v);
    }
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR p,
       int UU(fd),
       CACHEKEY k,
       uint32_t fullhash __attribute__((__unused__)),
       void **value,
       void** UU(dd),
       PAIR_ATTR *sizep,
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
    *dirtyp = 0;
    size_t data_index = (size_t)k.b;
    // assert that data_index is valid
    // if it is INT64_MAX, then that means
    // the block is not supposed to be in the cachetable
    assert(data[data_index] != INT64_MAX);
    
    int64_t* XMALLOC(data_val);
    usleep(10);
    *data_val = data[data_index];
    data_pair[data_index] = p;
    *value = data_val;
    *sizep = make_pair_attr(8);
    return 0;
}

static void *test_time(void *arg) {
    //
    // if num_Seconds is set to 0, run indefinitely
    //
    if (time_of_test != 0) {
        usleep(time_of_test*1000*1000);
        if (verbose) printf("should now end test\n");
        run_test = false;
    }
    if (verbose) printf("should be ending test now\n");
    return arg;
}

CACHETABLE ct;
CACHEFILE f1;

static void move_number_to_child(
    int parent, 
    int64_t* parent_val, 
    enum cachetable_dirty parent_dirty
    ) 
{
    int child = 0;
    int r;
    child = ((random() % 2) == 0) ? (2*parent + 1) : (2*parent + 2); 
    
    void* v1;
    long s1;
    CACHEKEY parent_key;
    parent_key.b = parent;
    uint32_t parent_fullhash = toku_cachetable_hash(f1, parent_key);

    
    CACHEKEY child_key;
    child_key.b = child;
    uint32_t child_fullhash = toku_cachetable_hash(f1, child_key);
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    wc.clone_callback = clone_callback;
    PAIR dep_pair = data_pair[parent];
    r = toku_cachetable_get_and_pin_with_dep_pairs(
        f1,
        child_key,
        child_fullhash,
        &v1,
        &s1,
        wc, fetch, def_pf_req_callback, def_pf_callback,
        PL_WRITE_CHEAP,
        NULL,
        1, //num_dependent_pairs
        &dep_pair,
        &parent_dirty
        );
    assert(r==0);
    
    int64_t* child_val = (int64_t *)v1;
    assert(child_val != parent_val); // sanity check that we are messing with different vals
    assert(*parent_val != INT64_MAX);
    assert(*child_val != INT64_MAX);
    usleep(10);
    (*parent_val)++;
    (*child_val)--;
    r = toku_test_cachetable_unpin(f1, parent_key, parent_fullhash, CACHETABLE_DIRTY, make_pair_attr(8));
    assert_zero(r);
    if (child < NUM_INTERNAL) {
        move_number_to_child(child, child_val, CACHETABLE_DIRTY);
    }
    else {
        r = toku_test_cachetable_unpin(f1, child_key, child_fullhash, CACHETABLE_DIRTY, make_pair_attr(8));
        assert_zero(r);
    }
}

static void *move_numbers(void *arg) {
    while (run_test) {
        int parent = 0;
        int r;
        void* v1;
        long s1;
        CACHEKEY parent_key;
        parent_key.b = parent;
        uint32_t parent_fullhash = toku_cachetable_hash(f1, parent_key);
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = flush;
        wc.clone_callback = clone_callback;
        r = toku_cachetable_get_and_pin_with_dep_pairs(
            f1,
            parent_key,
            parent_fullhash,
            &v1,
            &s1,
            wc, fetch, def_pf_req_callback, def_pf_callback,
            PL_WRITE_CHEAP,
            NULL,
            0, //num_dependent_pairs
            NULL,
            NULL
            );
        assert(r==0);
        int64_t* parent_val = (int64_t *)v1;
        move_number_to_child(parent, parent_val, CACHETABLE_CLEAN);
    }
    return arg;
}

static void remove_data(CACHEKEY* cachekey, bool for_checkpoint, void* UU(extra)) {
    assert(cachekey->b < NUM_ELEMENTS);
    data[cachekey->b] = INT64_MAX;
    if (for_checkpoint) {
        checkpointed_data[cachekey->b] = INT64_MAX;
    }
}


static void get_data(CACHEKEY* cachekey, uint32_t* fullhash, void* extra) {
    int* CAST_FROM_VOIDP(key, extra);
    cachekey->b = *key;
    *fullhash = toku_cachetable_hash(f1, *cachekey);
    data[*key] = INT64_MAX - 1;
}

static void merge_and_split_child(
    int parent, 
    int64_t* parent_val, 
    enum cachetable_dirty parent_dirty
    ) 
{
    int child = 0;
    int other_child = 0;
    int r;
    bool even = (random() % 2) == 0;
    child = (even) ? (2*parent + 1) : (2*parent + 2); 
    other_child = (!even) ? (2*parent + 1) : (2*parent + 2);
    assert(child != other_child);
    
    void* v1;
    long s1;

    CACHEKEY parent_key;
    parent_key.b = parent;
    uint32_t parent_fullhash = toku_cachetable_hash(f1, parent_key);

    CACHEKEY child_key;
    child_key.b = child;
    uint32_t child_fullhash = toku_cachetable_hash(f1, child_key);
    enum cachetable_dirty child_dirty = CACHETABLE_CLEAN;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    wc.clone_callback = clone_callback;
    PAIR dep_pair = data_pair[parent];
    r = toku_cachetable_get_and_pin_with_dep_pairs(
        f1,
        child_key,
        child_fullhash,
        &v1,
        &s1,
        wc, fetch, def_pf_req_callback, def_pf_callback,
        PL_WRITE_CHEAP,
        NULL,
        1, //num_dependent_pairs
        &dep_pair,
        &parent_dirty
        );
    assert(r==0);
    int64_t* child_val = (int64_t *)v1;
    
    CACHEKEY other_child_key;
    other_child_key.b = other_child;
    uint32_t other_child_fullhash = toku_cachetable_hash(f1, other_child_key);
    enum cachetable_dirty dirties[2];
    dirties[0] = parent_dirty;
    dirties[1] = child_dirty;
    PAIR dep_pairs[2];
    dep_pairs[0] = data_pair[parent];
    dep_pairs[1] = data_pair[child];
    
    r = toku_cachetable_get_and_pin_with_dep_pairs(
        f1,
        other_child_key,
        other_child_fullhash,
        &v1,
        &s1,
        wc, fetch, def_pf_req_callback, def_pf_callback,
        PL_WRITE_CHEAP,
        NULL,
        2, //num_dependent_pairs
        dep_pairs,
        dirties
        );
    assert(r==0);
    int64_t* other_child_val = (int64_t *)v1;
    assert(*parent_val != INT64_MAX);
    assert(*child_val != INT64_MAX);
    assert(*other_child_val != INT64_MAX);
    
    // lets get rid of other_child_val with a merge
    *child_val += *other_child_val;
    *other_child_val = INT64_MAX;        
    toku_test_cachetable_unpin_and_remove(f1, other_child_key, remove_data, NULL);
    dirties[1] = CACHETABLE_DIRTY;
    child_dirty = CACHETABLE_DIRTY;
    
    // now do a split
    CACHEKEY new_key;
    uint32_t new_fullhash;
    int64_t* XMALLOC(data_val);
    toku_cachetable_put_with_dep_pairs(
          f1,
          get_data,
          data_val,
          make_pair_attr(8),
          wc,
          &other_child,
          2, // number of dependent pairs that we may need to checkpoint
          dep_pairs,
          dirties,
          &new_key,
          &new_fullhash,
          put_callback_pair
          );
    assert(new_key.b == other_child);
    assert(new_fullhash == other_child_fullhash);
    *data_val = 5000;
    *child_val -= 5000;
    
    r = toku_test_cachetable_unpin(f1, parent_key, parent_fullhash, CACHETABLE_DIRTY, make_pair_attr(8));
    assert_zero(r);
    r = toku_test_cachetable_unpin(f1, other_child_key, other_child_fullhash, CACHETABLE_DIRTY, make_pair_attr(8));
    assert_zero(r);
    if (child < NUM_INTERNAL) {
        merge_and_split_child(child, child_val, CACHETABLE_DIRTY);
    }
    else {
        r = toku_test_cachetable_unpin(f1, child_key, child_fullhash, CACHETABLE_DIRTY, make_pair_attr(8));
        assert_zero(r);
    }
}

static void *merge_and_split(void *arg) {
    while (run_test) {
        int parent = 0;
        int r;        
        void* v1;
        long s1;
        CACHEKEY parent_key;
        parent_key.b = parent;
        uint32_t parent_fullhash = toku_cachetable_hash(f1, parent_key);
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = flush;
        wc.clone_callback = clone_callback;
        r = toku_cachetable_get_and_pin_with_dep_pairs(
            f1,
            parent_key,
            parent_fullhash,
            &v1,
            &s1,
            wc, fetch, def_pf_req_callback, def_pf_callback,
            PL_WRITE_CHEAP,
            NULL,
            0, //num_dependent_pairs
            NULL,
            NULL
            );
        assert(r==0);
        int64_t* parent_val = (int64_t *)v1;
        merge_and_split_child(parent, parent_val, CACHETABLE_CLEAN);
    }
    return arg;
}

static int num_checkpoints = 0;
static void *checkpoints(void *arg) {
    // first verify that checkpointed_data is correct;
    while(run_test) {
        int64_t sum = 0;
        for (int i = 0; i < NUM_ELEMENTS; i++) {
            if (checkpointed_data[i] != INT64_MAX) {
                sum += checkpointed_data[i];
            }
        }
        assert (sum==0);
    
        //
        // now run a checkpoint
        //
        CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
        toku_cachetable_begin_checkpoint(cp, NULL);    
        toku_cachetable_end_checkpoint(
            cp, 
            NULL, 
            NULL,
            NULL
            );
        assert (sum==0);
        for (int i = 0; i < NUM_ELEMENTS; i++) {
            if (checkpointed_data[i] != INT64_MAX) {
                sum += checkpointed_data[i];
            }
        }
        assert (sum==0);
        num_checkpoints++;
    }
    return arg;
}

static void
test_begin_checkpoint (
    LSN UU(checkpoint_lsn), 
    void* UU(header_v)) 
{
    memcpy(checkpointed_data, data, sizeof(int64_t)*NUM_ELEMENTS);
}

static void sum_vals(void) {
    int64_t sum = 0;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        //printf("actual: i %d val %" PRId64 " \n", i, data[i]);
        if (data[i] != INT64_MAX) {
            sum += data[i];
        }
    }
    if (verbose) printf("actual sum %" PRId64 " \n", sum);
    assert(sum == 0);
    sum = 0;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        //printf("checkpointed: i %d val %" PRId64 " \n", i, checkpointed_data[i]);
        if (checkpointed_data[i] != INT64_MAX) {
            sum += checkpointed_data[i];
        }
    }
    if (verbose) printf("checkpointed sum %" PRId64 " \n", sum);
    assert(sum == 0);
}

static void
cachetable_test (void) {
    const int test_limit = NUM_ELEMENTS;

    //
    // let's set up the data
    //
    for (int64_t i = 0; i < NUM_ELEMENTS; i++) {
        data[i] = 0;
        checkpointed_data[i] = 0;
    }
    time_of_test = 60;

    int r;

    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    toku_cachefile_set_userdata(
        f1,
        NULL,
        &dummy_log_fassociate,
        &dummy_close_usr,
        &dummy_free_usr,
        &dummy_chckpnt_usr,
        test_begin_checkpoint, // called in begin_checkpoint
        &dummy_end,
        &dummy_note_pin,
        &dummy_note_unpin
        );

    toku_pthread_t time_tid;
    toku_pthread_t checkpoint_tid;
    toku_pthread_t move_tid[NUM_MOVER_THREADS];
    toku_pthread_t merge_and_split_tid[NUM_MOVER_THREADS];
    run_test = true;

    for (int i = 0; i < NUM_MOVER_THREADS; i++) {
        r = toku_pthread_create(&move_tid[i], NULL, move_numbers, NULL); 
        assert_zero(r);
    }
    for (int i = 0; i < NUM_MOVER_THREADS; i++) {
        r = toku_pthread_create(&merge_and_split_tid[i], NULL, merge_and_split, NULL); 
        assert_zero(r);
    }
    r = toku_pthread_create(&checkpoint_tid, NULL, checkpoints, NULL); 
    assert_zero(r);    
    r = toku_pthread_create(&time_tid, NULL, test_time, NULL); 
    assert_zero(r);

    
    void *ret;
    r = toku_pthread_join(time_tid, &ret); 
    assert_zero(r);
    r = toku_pthread_join(checkpoint_tid, &ret); 
    assert_zero(r);
    for (int i = 0; i < NUM_MOVER_THREADS; i++) {
        r = toku_pthread_join(merge_and_split_tid[i], &ret); 
        assert_zero(r);
    }
    for (int i = 0; i < NUM_MOVER_THREADS; i++) {
        r = toku_pthread_join(move_tid[i], &ret); 
        assert_zero(r);
    }

    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
    
    sum_vals();
    if (verbose) printf("num_checkpoints %d\n", num_checkpoints);
    
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
