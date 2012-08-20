/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"


//
// This test ensures that get_and_pin with dependent nodes works
// as intended with checkpoints, by having multiple threads changing
// values on elements in data, and ensure that checkpoints always get snapshots 
// such that the sum of all the elements in data are 0.
//

// The arrays

#define NUM_ELEMENTS 100
#define NUM_MOVER_THREADS 4

int64_t data[NUM_ELEMENTS];
int64_t checkpointed_data[NUM_ELEMENTS];

uint32_t time_of_test;
bool run_test;

static void 
clone_callback(
    void* value_data, 
    void** cloned_value_data, 
    PAIR_ATTR* new_attr, 
    bool UU(for_checkpoint), 
    void* UU(write_extraargs)
    )
{
    new_attr->is_valid = false;
    int64_t* XMALLOC(data_val);
    *data_val = *(int64_t *)value_data;
    *cloned_value_data = data_val;   
}


static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool write_me,
       bool keep_me,
       bool checkpoint_me,
        bool UU(is_clone)
       ) {
    /* Do nothing */
    int64_t val_to_write = *(int64_t *)v;
    size_t data_index = (size_t)k.b;
    assert(val_to_write != INT64_MAX);
    if (write_me) {
        usleep(10);
        data[data_index] = val_to_write;
        if (checkpoint_me) checkpointed_data[data_index] = val_to_write;
    }
    if (!keep_me) {
        toku_free(v);
    }
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
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
    assert(data[data_index] != INT64_MAX);
    
    int64_t* XMALLOC(data_val);
    usleep(10);
    *data_val = data[data_index];
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

static void *move_numbers(void *arg) {
    while (run_test) {
        int rand_key1 = 0;
        int rand_key2 = 0;
        int less;
        int greater;
        int r;
        while (rand_key1 == rand_key2) {
            rand_key1 = random() % NUM_ELEMENTS;
            rand_key2 = random() % NUM_ELEMENTS;
            less = (rand_key1 < rand_key2) ? rand_key1 : rand_key2;
            greater = (rand_key1 > rand_key2) ? rand_key1 : rand_key2;
        }
        assert(less < greater);
        
        /*
        while (rand_key1 == rand_key2) {
            rand_key1 = random() % (NUM_ELEMENTS/2);
            rand_key2 = (NUM_ELEMENTS-1) - rand_key1;
            less = (rand_key1 < rand_key2) ? rand_key1 : rand_key2;
            greater = (rand_key1 > rand_key2) ? rand_key1 : rand_key2;
        }
        assert(less < greater);
        */

        void* v1;
        long s1;
        CACHEKEY less_key;
        less_key.b = less;
        uint32_t less_fullhash = less;
        enum cachetable_dirty less_dirty = CACHETABLE_DIRTY;
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = flush;
        wc.clone_callback = clone_callback;
        r = toku_cachetable_get_and_pin_with_dep_pairs(
            f1,
            less_key,
            less,
            &v1,
            &s1,
            wc, fetch, def_pf_req_callback, def_pf_callback,
            PL_WRITE_EXPENSIVE,
            NULL,
            0, //num_dependent_pairs
            NULL,
            NULL,
            NULL,
            NULL
            );
        assert(r==0);
        int64_t* first_val = (int64_t *)v1;
    
        CACHEKEY greater_key;
        greater_key.b = greater;
        uint32_t greater_fullhash = greater;
        enum cachetable_dirty greater_dirty = CACHETABLE_DIRTY;
        r = toku_cachetable_get_and_pin_with_dep_pairs(
            f1,
            make_blocknum(greater),
            greater,
            &v1,
            &s1,
            wc, fetch, def_pf_req_callback, def_pf_callback, 
            PL_WRITE_EXPENSIVE,
            NULL,
            1, //num_dependent_pairs
            &f1,
            &less_key,
            &less_fullhash,
            &less_dirty
            );
        assert(r==0);
    
        int64_t* second_val = (int64_t *)v1;
        assert(second_val != first_val); // sanity check that we are messing with different vals
        assert(*first_val != INT64_MAX);
        assert(*second_val != INT64_MAX);
        usleep(10);
        (*first_val)++;
        (*second_val)--;
        r = toku_test_cachetable_unpin(f1, less_key, less_fullhash, less_dirty, make_pair_attr(8));

        int third = 0;
        int num_possible_values = (NUM_ELEMENTS-1) - greater;
        if (num_possible_values > 0) {
            third = (random() % (num_possible_values)) + greater + 1;
            CACHEKEY third_key;
            third_key.b = third;
            uint32_t third_fullhash = third;
            enum cachetable_dirty third_dirty = CACHETABLE_DIRTY;
            r = toku_cachetable_get_and_pin_with_dep_pairs(
                f1,
                make_blocknum(third),
                third,
                &v1,
                &s1,
                wc, fetch, def_pf_req_callback, def_pf_callback,
                PL_WRITE_EXPENSIVE,
                NULL,
                1, //num_dependent_pairs
                &f1,
                &greater_key,
                &greater_fullhash,
                &greater_dirty
                );
            assert(r==0);
            
            int64_t* third_val = (int64_t *)v1;
            assert(second_val != third_val); // sanity check that we are messing with different vals
            usleep(10);
            (*second_val)++;
            (*third_val)--;
            r = toku_test_cachetable_unpin(f1, third_key, third_fullhash, third_dirty, make_pair_attr(8));
        }
        r = toku_test_cachetable_unpin(f1, greater_key, greater_fullhash, greater_dirty, make_pair_attr(8));
    }
    return arg;
}

static void *read_random_numbers(void *arg) {
    while(run_test) {
        int rand_key1 = random() % NUM_ELEMENTS;
        void* v1;
        long s1;
        int r1;
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = flush;
        wc.clone_callback = clone_callback;
        r1 = toku_cachetable_get_and_pin_nonblocking(
            f1,
            make_blocknum(rand_key1),
            rand_key1,
            &v1,
            &s1,
            wc, fetch, def_pf_req_callback, def_pf_callback, 
            PL_READ,
            NULL,
            NULL
            );
        if (r1 == 0) {
            r1 = toku_test_cachetable_unpin(f1, make_blocknum(rand_key1), rand_key1, CACHETABLE_CLEAN, make_pair_attr(8));
            assert(r1 == 0);
        }
    }
    if (verbose) printf("leaving\n");
    return arg;
}

static int num_checkpoints = 0;
static void *checkpoints(void *arg) {
    // first verify that checkpointed_data is correct;
    while(run_test) {
        int64_t sum = 0;
        for (int i = 0; i < NUM_ELEMENTS; i++) {
            sum += checkpointed_data[i];
        }
        assert (sum==0);
    
        //
        // now run a checkpoint
        //
        int r;
        CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
        r = toku_cachetable_begin_checkpoint(cp, NULL); assert(r == 0);    
        r = toku_cachetable_end_checkpoint(
            cp, 
            NULL, 
            NULL,
            NULL
            );
        assert(r==0);
        assert (sum==0);
        for (int i = 0; i < NUM_ELEMENTS; i++) {
            sum += checkpointed_data[i];
        }
        assert (sum==0);
        usleep(10*1024);
        num_checkpoints++;
    }
    return arg;
}

static int
test_begin_checkpoint (
    LSN UU(checkpoint_lsn), 
    void* UU(header_v)) 
{
    memcpy(checkpointed_data, data, sizeof(int64_t)*NUM_ELEMENTS);
    return 0;
}

static int
dummy_int_checkpoint_userdata(CACHEFILE UU(cf), int UU(n), void* UU(extra)) {
    return 0;
}

static void sum_vals(void) {
    int64_t sum = 0;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        //printf("actual: i %d val %" PRId64 " \n", i, data[i]);
        sum += data[i];
    }
    if (verbose) printf("actual sum %" PRId64 " \n", sum);
    assert(sum == 0);
    sum = 0;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        //printf("checkpointed: i %d val %" PRId64 " \n", i, checkpointed_data[i]);
        sum += checkpointed_data[i];
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
    time_of_test = 30;

    int r;
    
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    toku_cachefile_set_userdata(
        f1, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        dummy_int_checkpoint_userdata, 
        test_begin_checkpoint, // called in begin_checkpoint
        dummy_int_checkpoint_userdata,
        NULL, 
        NULL
        );
    
    toku_pthread_t time_tid;
    toku_pthread_t checkpoint_tid;
    toku_pthread_t move_tid[NUM_MOVER_THREADS];
    toku_pthread_t read_random_tid[NUM_MOVER_THREADS];
    run_test = true;

    for (int i = 0; i < NUM_MOVER_THREADS; i++) {
        r = toku_pthread_create(&read_random_tid[i], NULL, read_random_numbers, NULL); 
        assert_zero(r);
    }
    for (int i = 0; i < NUM_MOVER_THREADS; i++) {
        r = toku_pthread_create(&move_tid[i], NULL, move_numbers, NULL); 
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
        r = toku_pthread_join(move_tid[i], &ret); 
        assert_zero(r);
    }
    for (int i = 0; i < NUM_MOVER_THREADS; i++) {
        r = toku_pthread_join(read_random_tid[i], &ret); 
        assert_zero(r);
    }

    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, false, ZERO_LSN); assert(r == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
    
    sum_vals();
    if (verbose) printf("num_checkpoints %d\n", num_checkpoints);
    
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
