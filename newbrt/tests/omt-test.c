#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <errno.h>
#include <sys/types.h>

typedef struct value *OMTVALUE;
typedef OMTVALUE TESTVALUE;
#include "omt.h"
#include "../newbrt/memory.h"
#include "../newbrt/toku_assert.h"
#include "../include/db.h"
#include "../newbrt/brttypes.h"
#include <stdlib.h>
#include <stdint.h>

/* Things that would go in a omt-tests.h if we split to multiple files later. */
int verbose=0;

#define CKERR(r) ({ if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); })
#define CKERR2(r,r2) ({ if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); })
#define CKERR2s(r,r2,r3) ({ if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, strerror(r), r2,r3); assert(r==r2||r==r3); })

#include <string.h>
void parse_args (int argc, const char *argv[]) {
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
    u_int32_t number;
};

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
OMT omt;
TESTVALUE*       values = NULL;
struct value*   nums   = NULL;
u_int32_t       length;

void cleanup_globals(void) {
    assert(values);
    toku_free(values);
    values = NULL;
    assert(nums);
    toku_free(nums);
    nums = NULL;
}

const unsigned int random_seed = 0xFEADACBA;

void init_init_values(unsigned int seed, u_int32_t num_elements) {
    srandom(seed);

    cleanup_globals();

    MALLOC_N(num_elements, values);
    assert(values);
    MALLOC_N(num_elements, nums);
    assert(nums);
    length = num_elements;
}

void init_identity_values(unsigned int seed, u_int32_t num_elements) {
    u_int32_t   i;

    init_init_values(seed, num_elements);

    for (i = 0; i < length; i++) {
        nums[i].number   = i;
        values[i]        = (TESTVALUE)&nums[i];
    }
}

void init_distinct_sorted_values(unsigned int seed, u_int32_t num_elements) {
    u_int32_t   i;

    init_init_values(seed, num_elements);

    u_int32_t number = 0;

    for (i = 0; i < length; i++) {
        number          += (u_int32_t)(random() % 32) + 1;
        nums[i].number   = number;
        values[i]        = (TESTVALUE)&nums[i];
    }
}

void init_distinct_random_values(unsigned int seed, u_int32_t num_elements) {
    init_distinct_sorted_values(seed, num_elements);

    u_int32_t   i;
    u_int32_t   choice;
    u_int32_t   choices;
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

void init_globals(void) {
    MALLOC_N(1, values);
    assert(values);
    MALLOC_N(1, nums);
    assert(nums);
    length = 1;
}

void test_close(enum close_when_done close) {
    if (close == KEEP_WHEN_DONE) return;
    assert(close == CLOSE_WHEN_DONE);
    toku_omt_destroy(&omt);
    assert(omt==NULL);
}

void test_create(enum close_when_done close) {
    int r;
    omt = NULL;

    r = toku_omt_create(&omt);
    CKERR(r);
    assert(omt!=NULL);
    test_close(close);
}

void test_create_size(enum close_when_done close) {
    test_create(KEEP_WHEN_DONE);
    assert(toku_omt_size(omt) == 0);
    test_close(close);
}

void test_create_insert_at_almost_random(enum close_when_done close) {
    u_int32_t i;
    int r;
    u_int32_t size = 0;

    test_create(KEEP_WHEN_DONE);
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+1);
    CKERR2(r, EINVAL);
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+2);
    CKERR2(r, EINVAL);
    for (i = 0; i < length/2; i++) {
        assert(size==toku_omt_size(omt));
        r = toku_omt_insert_at(omt, values[i], i);
        CKERR(r);
        assert(++size==toku_omt_size(omt));
        r = toku_omt_insert_at(omt, values[length-1-i], i+1);
        CKERR(r);
        assert(++size==toku_omt_size(omt));
    }
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+1);
    CKERR2(r, EINVAL);
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+2);
    CKERR2(r, EINVAL);
    assert(size==toku_omt_size(omt));
    test_close(close);
}

void test_create_insert_at_sequential(enum close_when_done close) {
    u_int32_t i;
    int r;
    u_int32_t size = 0;

    test_create(KEEP_WHEN_DONE);
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+1);
    CKERR2(r, EINVAL);
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+2);
    CKERR2(r, EINVAL);
    for (i = 0; i < length; i++) {
        assert(size==toku_omt_size(omt));
        r = toku_omt_insert_at(omt, values[i], i);
        CKERR(r);
        assert(++size==toku_omt_size(omt));
    }
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+1);
    CKERR2(r, EINVAL);
    r = toku_omt_insert_at(omt, values[0], toku_omt_size(omt)+2);
    CKERR2(r, EINVAL);
    assert(size==toku_omt_size(omt));
    test_close(close);
}

void test_create_from_sorted_array(enum create_type create_choice, enum close_when_done close) {
    int r;
    omt = NULL;

    if (create_choice == BATCH_INSERT) {
        r = toku_omt_create_from_sorted_array(&omt, values, length);
        CKERR(r);
    }
    else if (create_choice == INSERT_AT) {
        test_create_insert_at_sequential(KEEP_WHEN_DONE);
    }
    else if (create_choice == INSERT_AT_ALMOST_RANDOM) {
        test_create_insert_at_almost_random(KEEP_WHEN_DONE);
    }
    else assert(FALSE);

    assert(omt!=NULL);
    test_close(close);
}

void test_create_from_sorted_array_size(enum create_type create_choice, enum close_when_done close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    assert(toku_omt_size(omt)==length);
    test_close(close);
}    

void test_fetch_verify (OMT omtree, TESTVALUE* val, u_int32_t len ) {
    u_int32_t i;
    int j;
    int r;
    TESTVALUE v = (TESTVALUE)&i;
    TESTVALUE oldv = v;

    OMTCURSOR c;
    r = toku_omt_cursor_create(&c);
    CKERR(r);

    assert(len == toku_omt_size(omtree));
    for (i = 0; i < len; i++) {
        assert(oldv!=val[i]);
        v = NULL;
        r = toku_omt_fetch(omtree, i, &v, NULL);
        CKERR(r);
        assert(v != NULL);
        assert(v != oldv);
        assert(v == val[i]);
        assert(v->number == val[i]->number);

        v = oldv;
        r = toku_omt_fetch(omtree, i, &v, c);
        CKERR(r);
        assert(v != NULL);
        assert(v != oldv);
        assert(v == val[i]);
        assert(v->number == val[i]->number);
        assert(toku_omt_cursor_is_valid(c));
 
        v = oldv;
        r = toku_omt_cursor_current(c, &v);
        CKERR(r);
        assert(v != NULL);
        assert(v != oldv);
        assert(v == val[i]);
        assert(v->number == val[i]->number);
        assert(toku_omt_cursor_is_valid(c));

        v = oldv;
        j = i + 1;
        while ((r = toku_omt_cursor_next(c, &v)) == 0) {
            assert(toku_omt_cursor_is_valid(c));
            assert(v != NULL);
            assert(v != oldv);
            assert(v == val[j]);
            assert(v->number == val[j]->number);
            j++;
            v = oldv;
        }
        CKERR2(r, EINVAL);
        assert(j == (int) len);

        assert(oldv!=val[i]);
        v = NULL;
        r = toku_omt_fetch(omtree, i, &v, c);
        CKERR(r);
        assert(v != NULL);
        assert(v != oldv);
        assert(v == val[i]);
        assert(v->number == val[i]->number);

        v = oldv;
        j = i - 1;
        while ((r = toku_omt_cursor_prev(c, &v)) == 0) {
            assert(toku_omt_cursor_is_valid(c));
            assert(v != NULL);
            assert(v != oldv);
            assert(v == val[j]);
            assert(v->number == val[j]->number);
            j--;
            v = oldv;
        }
        CKERR2(r, EINVAL);
        assert(j == -1);

    }

    for (i = len; i < len*2; i++) {
        v = oldv;
        r = toku_omt_fetch(omtree, i, &v, NULL);
        CKERR2(r, EINVAL);
        assert(v == oldv);
        v = NULL;
        r = toku_omt_fetch(omtree, i, &v, c);
        CKERR2(r, EINVAL);
        assert(v == NULL);
    }

    toku_omt_cursor_destroy(&c);
}

void test_create_fetch_verify(enum create_type create_choice, enum close_when_done close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    test_fetch_verify(omt, values, length);
    test_close(close);
}

static int iterate_helper_error_return = 1;

int iterate_helper(TESTVALUE v, u_int32_t idx, void* extra) {
    if (extra == NULL) return iterate_helper_error_return;
    TESTVALUE* vals = (TESTVALUE *)extra;
    assert(v != NULL);
    assert(v == vals[idx]);
    assert(v->number == vals[idx]->number);
    return 0;
}

void test_iterate_verify(OMT omtree, TESTVALUE* vals, u_int32_t len) {
    int r;
    iterate_helper_error_return = 0;
    r = toku_omt_iterate(omtree, iterate_helper, (void*)vals);
    CKERR(r);
    iterate_helper_error_return = 0xFEEDABBA;
    r = toku_omt_iterate(omtree, iterate_helper, NULL);
    if (!len) {
        CKERR2(r, 0);
    }
    else {
        CKERR2(r, iterate_helper_error_return);
    }
}

void test_create_iterate_verify(enum create_type create_choice, enum close_when_done close) {
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    test_iterate_verify(omt, values, length);
    test_close(close);
}


void permute_array(u_int32_t* arr, u_int32_t len) {
    //
    // create a permutation of 0...size-1
    //
    u_int32_t i = 0;
    for (i = 0; i < len; i++) {
        arr[i] = i;
    }
    for (i = 0; i < len - 1; i++) {
        u_int32_t choices = len - i;
        u_int32_t choice  = random() % choices;
        if (choice != i) {
            u_int32_t temp = arr[i];
            arr[i]      = arr[choice];
            arr[choice] = temp;
        }
    }
}

void test_create_set_at(enum create_type create_choice, enum close_when_done close) {
    u_int32_t i = 0;

    struct value*   old_nums   = NULL;
    MALLOC_N(length, old_nums);
    assert(nums);

    u_int32_t* perm = NULL;
    MALLOC_N(length, perm);
    assert(perm);

    TESTVALUE* old_values = NULL;
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
    r = toku_omt_set_at (omt, values[0], length);
    CKERR2(r,EINVAL);    
    r = toku_omt_set_at (omt, values[0], length+1);
    CKERR2(r,EINVAL);    
    for (i = 0; i < length; i++) {
        u_int32_t choice = perm[i];
        values[choice] = &nums[choice];
        nums[choice].number = (u_int32_t)random();
        r = toku_omt_set_at (omt, values[choice], choice);
        CKERR(r);
        test_iterate_verify(omt, values, length);
        test_fetch_verify(omt, values, length);
    }
    r = toku_omt_set_at (omt, values[0], length);
    CKERR2(r,EINVAL);    
    r = toku_omt_set_at (omt, values[0], length+1);
    CKERR2(r,EINVAL);    

    toku_free(perm);
    toku_free(old_values);
    toku_free(old_nums);

    test_close(close);
}

int insert_helper(TESTVALUE value, void* extra_insert) {
    TESTVALUE to_insert = (OMTVALUE)extra_insert;
    assert(to_insert);

    if (value->number < to_insert->number) return -1;
    if (value->number > to_insert->number) return +1;
    return 0;
}

void test_create_insert(enum close_when_done close) {
    u_int32_t i = 0;

    u_int32_t* perm = NULL;
    MALLOC_N(length, perm);
    assert(perm);

    permute_array(perm, length);

    test_create(KEEP_WHEN_DONE);
    int r;
    u_int32_t size = length;
    length = 0;
    while (length < size) {
        u_int32_t choice = perm[length];
        TESTVALUE to_insert = &nums[choice];
        u_int32_t idx = UINT32_MAX;

        assert(length==toku_omt_size(omt));
        r = toku_omt_insert(omt, to_insert, insert_helper, to_insert, &idx);
        CKERR(r);
        assert(idx <= length);
        if (idx > 0) {
            assert(to_insert->number > values[idx-1]->number);
        }
        if (idx < length) {
            assert(to_insert->number < values[idx]->number);
        }
        length++;
        assert(length==toku_omt_size(omt));
        /* Make room */
        for (i = length-1; i > idx; i--) {
            values[i] = values[i-1];
        }
        values[idx] = to_insert;
        test_fetch_verify(omt, values, length);
        test_iterate_verify(omt, values, length);

        idx = UINT32_MAX;
        r = toku_omt_insert(omt, to_insert, insert_helper, to_insert, &idx);
        CKERR2(r, DB_KEYEXIST);
        assert(idx < length);
        assert(values[idx]->number == to_insert->number);
        assert(length==toku_omt_size(omt));

        test_iterate_verify(omt, values, length);
        test_fetch_verify(omt, values, length);
    }

    toku_free(perm);

    test_close(close);
}

void test_create_delete_at(enum create_type create_choice, enum close_when_done close) {
    u_int32_t i = 0;
    int r = ENOSYS;
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

    assert(length == toku_omt_size(omt));
    r = toku_omt_delete_at(omt, length);
    CKERR2(r,EINVAL);
    assert(length == toku_omt_size(omt));
    r = toku_omt_delete_at(omt, length+1);
    CKERR2(r,EINVAL);
    while (length > 0) {
        assert(length == toku_omt_size(omt));
        u_int32_t index_to_delete = random()%length;
        r = toku_omt_delete_at(omt, index_to_delete);
        CKERR(r);
        for (i = index_to_delete+1; i < length; i++) {
            values[i-1] = values[i];
        }
        length--;
        test_fetch_verify(omt, values, length);
        test_iterate_verify(omt, values, length);
    }
    assert(length == 0);
    assert(length == toku_omt_size(omt));
    r = toku_omt_delete_at(omt, length);
    CKERR2(r, EINVAL);
    assert(length == toku_omt_size(omt));
    r = toku_omt_delete_at(omt, length+1);
    CKERR2(r, EINVAL);
    test_close(close);
}

void test_split_merge(enum create_type create_choice, enum close_when_done close) {
    int r = ENOSYS;
    u_int32_t i = 0;
    OMT left_split = NULL;
    OMT right_split = NULL;
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

    for (i = 0; i <= length; i++) {
        r = toku_omt_split_at(omt, &right_split, length+1);
        CKERR2(r,EINVAL);
        r = toku_omt_split_at(omt, &right_split, length+2);
        CKERR2(r,EINVAL);

        //
        // test successful split
        //
        r = toku_omt_split_at(omt, &right_split, i);
        CKERR(r);
        left_split = omt;
        omt = NULL;
        assert(toku_omt_size(left_split) == i);
        assert(toku_omt_size(right_split) == length - i);
        test_fetch_verify(left_split, values, i);
        test_iterate_verify(left_split, values, i);
        test_fetch_verify(right_split, &values[i], length - i);
        test_iterate_verify(right_split, &values[i], length - i);
        //
        // verify that new OMT's cannot do bad splits
        //
        r = toku_omt_split_at(left_split, &omt, i+1);
        CKERR2(r,EINVAL);
        assert(toku_omt_size(left_split) == i);
        assert(toku_omt_size(right_split) == length - i);
        r = toku_omt_split_at(left_split, &omt, i+2);
        CKERR2(r,EINVAL);
        assert(toku_omt_size(left_split) == i);
        assert(toku_omt_size(right_split) == length - i);
        r = toku_omt_split_at(right_split, &omt, length - i + 1);
        CKERR2(r,EINVAL);
        assert(toku_omt_size(left_split) == i);
        assert(toku_omt_size(right_split) == length - i);
        r = toku_omt_split_at(right_split, &omt, length - i + 1);
        CKERR2(r,EINVAL);
        assert(toku_omt_size(left_split) == i);
        assert(toku_omt_size(right_split) == length - i);

        //
        // test merge
        //
        r = toku_omt_merge(left_split,right_split,&omt);
        CKERR(r);
        left_split = NULL;
        right_split = NULL;
        assert(toku_omt_size(omt) == length);
        test_fetch_verify(omt, values, length);
        test_iterate_verify(omt, values, length);
    }
    test_close(close);
}


void init_values(enum rand_type rand_choice) {
    const u_int32_t test_size = 100;
    if (rand_choice == TEST_RANDOM) {
        init_distinct_random_values(random_seed, test_size);
    }
    else if (rand_choice == TEST_SORTED) {
        init_distinct_sorted_values(random_seed, test_size);
    }
    else if (rand_choice == TEST_IDENTITY) {
        init_identity_values(       random_seed, test_size);
    }
    else assert(FALSE);
}

void test_create_array(enum create_type create_choice, enum rand_type rand_choice) {
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
    u_int32_t first_zero;
    u_int32_t first_pos;
} h_extra;

int test_heaviside(OMTVALUE v_omt, void* x) {
    TESTVALUE v = (OMTVALUE) v_omt;
    h_extra* extra = (h_extra*)x;
    assert(v && x);
    assert(extra->first_zero <= extra->first_pos);

    u_int32_t value = v->number;
    if (value < extra->first_zero) return -1;
    if (value < extra->first_pos) return 0;
    return 1;
}

void heavy_extra(h_extra* extra, u_int32_t first_zero, u_int32_t first_pos) {
    extra->first_zero = first_zero;
    extra->first_pos  = first_pos;
}

void test_find_dir(int dir, void* extra, int (*h)(OMTVALUE, void*),
                   int r_expect, BOOL idx_will_change, u_int32_t idx_expect,
                   u_int32_t number_expect, BOOL cursor_valid) {
    u_int32_t idx     = UINT32_MAX;
    u_int32_t old_idx = idx;
    TESTVALUE omt_val, omt_val_curs;
    OMTCURSOR c;
    int r;
    BOOL found;

    r = toku_omt_cursor_create(&c);
    CKERR(r);

    omt_val = NULL;
    if (dir == 0) {
        r = toku_omt_find_zero(omt, h, extra,      &omt_val, &idx, c);
    }
    else {
        r = toku_omt_find(     omt, h, extra, dir, &omt_val, &idx, c);
    }
    CKERR2(r, r_expect);
    if (idx_will_change) {
        assert(idx == idx_expect);
    }
    else {
        assert(idx == old_idx);
    }
    if (r == DB_NOTFOUND) {
        assert(omt_val == NULL);
        found = FALSE;
    }
    else {
        assert(omt_val->number == number_expect);
        found = TRUE;
    }
 
    assert(!cursor_valid == !toku_omt_cursor_is_valid(c));
    if (cursor_valid) {
        TESTVALUE tmp;
        assert(idx_will_change);
        omt_val_curs = NULL;
        r = toku_omt_cursor_current(c, &omt_val_curs);
        CKERR(r);
        assert(toku_omt_cursor_is_valid(c));
        r = toku_omt_fetch(omt, idx, &tmp, NULL);
        CKERR(r);
        if (found) assert(tmp==omt_val);
        assert(omt_val_curs != NULL);
        assert(omt_val_curs == tmp);
        assert(omt_val_curs->number == tmp->number);
        if (found) assert(omt_val_curs->number==number_expect);
    }

    toku_omt_cursor_invalidate(c);
    assert(!toku_omt_cursor_is_valid(c));
    toku_omt_cursor_destroy(&c);

    /* Verify we can pass NULL value. */
    omt_val = NULL;
    idx      = old_idx;
    if (dir == 0) {
        r = toku_omt_find_zero(omt, h, extra,      NULL, &idx, NULL);
    }
    else {
        r = toku_omt_find(     omt, h, extra, dir, NULL, &idx, NULL);
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
        r = toku_omt_find_zero(omt, h, extra,      &omt_val, 0, NULL);
    }
    else {
        r = toku_omt_find(     omt, h, extra, dir, &omt_val, 0, NULL);
    }
    CKERR2(r, r_expect);
    assert(idx == old_idx);
    if (r == DB_NOTFOUND) {
        assert(omt_val == NULL);
    }
    else {
        assert(omt_val->number == number_expect);
    }

    /* Verify we can pass NULL both. */
    omt_val  = NULL;
    idx      = old_idx;
    if (dir == 0) {
        r = toku_omt_find_zero(omt, h, extra,      NULL, 0, NULL);
    }
    else {
        r = toku_omt_find(     omt, h, extra, dir, NULL, 0, NULL);
    }
    CKERR2(r, r_expect);
    assert(idx == old_idx);
    assert(omt_val == NULL);
}

void test_find(enum create_type create_choice, enum close_when_done close) {
    h_extra extra;
    init_identity_values(random_seed, 100);
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

/*
    -...-
        A
*/
    heavy_extra(&extra, length, length);
    test_find_dir(-1, &extra, test_heaviside, 0,           TRUE,  length-1, length-1, TRUE);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, FALSE, 0,        0,        FALSE);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, TRUE,  length,   length,   FALSE);


/*
    +...+
    B
*/
    heavy_extra(&extra, 0, 0);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, FALSE, 0, 0, FALSE);
    test_find_dir(+1, &extra, test_heaviside, 0,           TRUE,  0, 0, TRUE);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, TRUE,  0, 0, FALSE);

/*
    0...0
    C
*/
    heavy_extra(&extra, 0, length);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, FALSE, 0, 0, FALSE);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, FALSE, 0, 0, FALSE);
    test_find_dir(0,  &extra, test_heaviside, 0,           TRUE,  0, 0, TRUE);

/*
    -...-0...0
        AC
*/
    heavy_extra(&extra, length/2, length);
    test_find_dir(-1, &extra, test_heaviside, 0,           TRUE,  length/2-1, length/2-1, TRUE);
    test_find_dir(+1, &extra, test_heaviside, DB_NOTFOUND, FALSE, 0,          0,          FALSE);
    test_find_dir(0,  &extra, test_heaviside, 0,           TRUE,  length/2,   length/2,   TRUE);

/*
    0...0+...+
    C    B
*/
    heavy_extra(&extra, 0, length/2);
    test_find_dir(-1, &extra, test_heaviside, DB_NOTFOUND, FALSE, 0,        0,        FALSE);
    test_find_dir(+1, &extra, test_heaviside, 0,           TRUE,  length/2, length/2, TRUE);
    test_find_dir(0,  &extra, test_heaviside, 0,           TRUE,  0,        0,        TRUE);

/*
    -...-+...+
        AB
*/
    heavy_extra(&extra, length/2, length/2);
    test_find_dir(-1, &extra, test_heaviside, 0,           TRUE, length/2-1, length/2-1, TRUE);
    test_find_dir(+1, &extra, test_heaviside, 0,           TRUE, length/2,   length/2,   TRUE);
    test_find_dir(0,  &extra, test_heaviside, DB_NOTFOUND, TRUE, length/2,   length/2,   FALSE);

/*
    -...-0...0+...+
        AC    B
*/    
    heavy_extra(&extra, length/3, 2*length/3);
    test_find_dir(-1, &extra, test_heaviside, 0, TRUE,   length/3-1,   length/3-1, TRUE);
    test_find_dir(+1, &extra, test_heaviside, 0, TRUE, 2*length/3,   2*length/3,   TRUE);
    test_find_dir(0,  &extra, test_heaviside, 0, TRUE,   length/3,     length/3,   TRUE);

    /* Cleanup */
    test_close(close);
}

void invalidate_callback_null(OMTCURSOR c, void *extra) {
    assert(c && !extra);
}

void invalidate_callback_inc(OMTCURSOR c, void *extra) {
    assert(c);
    int *num = extra;
    (*num)++;
}

void test_invalidate(enum create_type create_choice, BOOL set_callback, BOOL invalidate_callback) {
    init_identity_values(random_seed, 100);
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);

    OMTCURSOR c;
    int invalidate_count = 0;
        
    int r = toku_omt_cursor_create(&c);
    if (set_callback || invalidate_callback) {
        toku_omt_cursor_set_invalidate_callback(c, invalidate_callback_inc, &invalidate_count);
    } 
    if (invalidate_callback) {
        toku_omt_cursor_set_invalidate_callback(c, invalidate_callback_null, NULL);
    }
    OMTVALUE val;
    r = toku_omt_fetch(omt, 0, &val, c); CKERR(r);
    assert(toku_omt_cursor_is_valid(c));
    assert(invalidate_count==0);
    r = toku_omt_cursor_prev(c, &val); CKERR2(r, EINVAL);
    assert(!toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==1);
    else                                      assert(invalidate_count==0);
    r = toku_omt_cursor_prev(c, &val); CKERR2(r, EINVAL);
    assert(!toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==1);
    else                                      assert(invalidate_count==0);

    r = toku_omt_fetch(omt, toku_omt_size(omt)-1, &val, c); CKERR(r);
    assert(toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==1);
    else                                      assert(invalidate_count==0);

    r = toku_omt_cursor_prev(c, &val); CKERR(r);
    assert(toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==1);
    else                                      assert(invalidate_count==0);
    r = toku_omt_cursor_next(c, &val); CKERR(r);
    assert(toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==1);
    else                                      assert(invalidate_count==0);
    r = toku_omt_cursor_next(c, &val); CKERR2(r, EINVAL);
    assert(!toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==2);
    else                                      assert(invalidate_count==0);

    r = toku_omt_fetch(omt, toku_omt_size(omt)-1, &val, c); CKERR(r);
    assert(toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==2);
    else                                      assert(invalidate_count==0);

    test_close(CLOSE_WHEN_DONE);
    assert(!toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==3);
    else                                      assert(invalidate_count==0);

    init_identity_values(random_seed, 100);
    test_create_from_sorted_array(create_choice, KEEP_WHEN_DONE);
    r = toku_omt_fetch(omt, toku_omt_size(omt)-1, &val, c); CKERR(r);
    assert(toku_omt_cursor_is_valid(c));
    if (set_callback && !invalidate_callback) assert(invalidate_count==3);
    else                                      assert(invalidate_count==0);

    toku_omt_cursor_destroy(&c);
    if (set_callback && !invalidate_callback) assert(invalidate_count==4);
    else                                      assert(invalidate_count==0);

    test_close(CLOSE_WHEN_DONE);
    if (set_callback && !invalidate_callback) assert(invalidate_count==4);
    else                                      assert(invalidate_count==0);
}

void runtests_create_choice(enum create_type create_choice) {
    test_create_array(create_choice, TEST_SORTED);
    test_create_array(create_choice, TEST_RANDOM);
    test_create_array(create_choice, TEST_IDENTITY);
    test_find(        create_choice, CLOSE_WHEN_DONE);
    test_invalidate(  create_choice, FALSE, FALSE);
    test_invalidate(  create_choice, FALSE, TRUE);
    test_invalidate(  create_choice, TRUE,  FALSE);
    test_invalidate(  create_choice, TRUE,  TRUE);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    init_globals();
    test_create(      CLOSE_WHEN_DONE);
    test_create_size( CLOSE_WHEN_DONE);
    runtests_create_choice(BATCH_INSERT);
    runtests_create_choice(INSERT_AT);
    runtests_create_choice(INSERT_AT_ALMOST_RANDOM);
    cleanup_globals();
    return 0;
}

