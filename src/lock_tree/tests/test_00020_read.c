/* We are going to test whether create and close properly check their input. */

#include "test.h"

int r;
toku_lock_tree* lt  = NULL;
toku_ltm*       ltm = NULL;
DB*             db  = (DB*)1;
TXNID           txn = (TXNID)1;
enum { MAX_LT_LOCKS = 1000 };
uint32_t max_locks = MAX_LT_LOCKS;
uint64_t max_lock_memory = MAX_LT_LOCKS*64;
bool duplicates = false;
int  nums[100];

DBT _keys_left[2];
DBT _keys_right[2];
DBT _datas_left[2];
DBT _datas_right[2];
DBT* keys_left[2]   ;
DBT* keys_right[2]  ;
DBT* datas_left[2] ;
DBT* datas_right[2] ;

toku_point qleft, qright;
toku_interval query;
toku_range* buf;
unsigned buflen;
unsigned numfound;

static void init_query(void) {  
    init_point(&qleft,  lt);
    init_point(&qright, lt);
    
    qleft.key_payload  = (void *) toku_lt_neg_infinity;
    qright.key_payload = (void *) toku_lt_infinity;

    memset(&query,0,sizeof(query));
    query.left  = &qleft;
    query.right = &qright;
}

static void setup_tree(void) {
    assert(!lt && !ltm);
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db);
    CKERR(r);
    assert(ltm);
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db);
    CKERR(r);
    assert(lt);
    init_query();
}

static void close_tree(void) {
    r = toku_lt_unlock(lt, txn); CKERR(r);
    assert(lt && ltm);
    r = toku_lt_close(lt); CKERR(r);
    r = toku_ltm_close(ltm); CKERR(r);
    lt = NULL;
    ltm = NULL;
}

typedef enum { null = -1, infinite = -2, neg_infinite = -3 } lt_infty;

static DBT* set_to_infty(DBT *dbt, int value) {
    if (value == infinite) return (DBT*)toku_lt_infinity;
    if (value == neg_infinite) return (DBT*)toku_lt_neg_infinity;
    if (value == null) return dbt_init(dbt, NULL, 0);
    assert(value >= 0);
    return                    dbt_init(dbt, &nums[value], sizeof(nums[0]));
}


static void lt_insert(int key_l, int key_r) {
    DBT _key_left;
    DBT _key_right;
    DBT* key_left   = &_key_left;
    DBT* key_right  = &_key_right;

    key_left  = set_to_infty(key_left,  key_l);
    key_right = set_to_infty(key_right, key_r);
    assert(key_left);
    assert(key_right);

    r = toku_lt_acquire_range_read_lock(lt, db, txn, key_left, key_right);
    CKERR(r);
    toku_lt_verify(lt, db);
}

static void setup_payload_len(void** payload, uint32_t* len, int val) {
    assert(payload && len);

    DBT temp;

    *payload = set_to_infty(&temp, val);
    
    if (val < 0) {
        *len = 0;
    }
    else {
        *len = sizeof(nums[0]);
        *payload = temp.data;
    }
}

static void temporarily_fake_comparison_functions(void) {
    assert(!lt->db && !lt->compare_fun);
    lt->db = db;
    lt->compare_fun = get_compare_fun_from_db(db);
}

static void stop_fake_comparison_functions(void) {
    assert(lt->db && lt->compare_fun);
    lt->db = NULL;
    lt->compare_fun = NULL;
}

static void lt_find(toku_range_tree* rt,
		    unsigned k, int key_l, int key_r,
		    TXNID find_txn) {
temporarily_fake_comparison_functions();
    r = toku_rt_find(rt, &query, 0, &buf, &buflen, &numfound);
    CKERR(r);
    assert(numfound==k);

    toku_point left, right;
    init_point(&left, lt);
    setup_payload_len(&left.key_payload, &left.key_len, key_l);
    init_point(&right, lt);
    setup_payload_len(&right.key_payload, &right.key_len, key_r);
    unsigned i;
    for (i = 0; i < numfound; i++) {
        if (toku_lt_point_cmp(buf[i].ends.left,  &left ) == 0 &&
            toku_lt_point_cmp(buf[i].ends.right, &right) == 0 &&
            buf[i].data == find_txn) { goto cleanup; }
    }
    assert(false);  //Crash since we didn't find it.
cleanup:
    stop_fake_comparison_functions();
}
              

static void insert_1(int key_l, int key_r,
		     const void* kl, const void* kr) {
    DBT _key_left;
    DBT _key_right;
    DBT* key_left   = &_key_left;
    DBT* key_right  = &_key_right;

    dbt_init    (key_left,  &nums[key_l], sizeof(nums[key_l]));
    dbt_init    (key_right, &nums[key_r], sizeof(nums[key_r]));
    if (kl) key_left   = (DBT*)kl;
    if (kr) key_right  = (DBT*)kr;
    

    setup_tree();
    r = toku_lt_acquire_range_read_lock(lt, db, txn, key_left, key_right); CKERR(r);
    close_tree();

    setup_tree();
    r = toku_lt_acquire_read_lock(lt, db, txn, key_left); CKERR(r);
    close_tree();
}

static void runtest(void) {
    const DBT* choices[3];
    choices[0] = toku_lt_neg_infinity;
    choices[1] = NULL;
    choices[2] = toku_lt_infinity;
    for (int i = 0; i < 9; i++) {
        int a = i / 3;
        int b = i % 3;
        if (a > b) continue;

        insert_1(3, 3, choices[a], choices[b]);
    }
    
    toku_range_tree *rt;
    /* ************************************** */
    setup_tree();

    /////BUG HERE MAYBE NOT CONSOLIDATING.
    /*
        [3,  7] and [4, 5]
    */
    
    lt_insert(3, 7);
    lt_insert(4, 5);
    rt = toku_lt_ifexist_selfread(lt, txn);
    assert(rt);

    lt_find(rt, 1,
            3,
            7,
            txn);

#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;
    assert(rt);

    lt_find(rt, 1,
            3,
            7,
            txn);
#endif

    close_tree();
    /* ************************************** */
    setup_tree();

    /*
        [3, 7)] and [4, 5]
    */
    lt_insert(4, 5);
    lt_insert(3, 7);
    
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);

    lt_find(rt, 1,
            3,
            7,
            txn);

#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);

    lt_find(rt, 1,
            3,
            7,
            txn);
#endif
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree();
    lt_insert(3, 3);
    lt_insert(4, 4);
    lt_insert(3, 3);
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 2, 3, 3, txn);
    lt_find(rt, 2, 4, 4, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 2, 3, 3, txn);
    lt_find(rt, 2, 4, 4, txn);
#endif
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree();
    for (int i = 0; i < 20; i += 2) {
        lt_insert(i, i + 1);
    }
    rt = toku_lt_ifexist_selfread(lt, txn);
    assert(rt);
    for (int i = 0; i < 20; i += 2) {
        lt_find(rt, 10, i, i + 1, txn);
    }
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread; assert(rt);
    for (int i = 0; i < 20; i += 2) {
        lt_find(rt, 10, i, i + 1, txn);
    }
#endif
    lt_insert(0, 20);
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(  rt, 1, 0, 20, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(  rt, 1, 0, 20, txn);
#endif
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree();
    lt_insert(0, 1);
    lt_insert(1, 2);

    lt_insert(4, 5);
    lt_insert(3, 4);
    
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 2,   0, 2, txn);
    lt_find(rt, 2,   3, 5, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 2,   0, 2, txn);
    lt_find(rt, 2,   3, 5, txn);
#endif

    lt_insert(2, 3);

    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 1,   0, 5, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 1,   0, 5, txn);
#endif
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree();
    lt_insert(1, 3);
    lt_insert(4, 6);
    lt_insert(2, 5);
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 1,   1, 6, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 1,   1, 6, txn);
#endif
    close_tree();

    setup_tree();
    lt_insert(neg_infinite, 3);
    lt_insert(           4, 5);
    lt_insert(           6, 8);
    lt_insert(           2, 7);
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 1,   neg_infinite, 8, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 1,   neg_infinite, 8, txn);
#endif
    close_tree();

    setup_tree();
    lt_insert(1, 2);
    lt_insert(3, infinite);
    lt_insert(2, 3);
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 1,   1, infinite, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 1,   1, infinite, txn);
#endif
    close_tree();

    setup_tree();
    lt_insert(1, 2);
    lt_insert(3, 4);
    lt_insert(5, 6);
    lt_insert(2, 5);
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 1,   1, 6, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 1,   1, 6, txn);
#endif
    close_tree();

    setup_tree();
    lt_insert(1, 2);
    lt_insert(3, 5);
    lt_insert(2, 4);
    rt = toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(rt, 1,   1, 5, txn);
#if TOKU_LT_USE_MAINREAD && !defined(TOKU_RT_NOOVERLAPS)
    rt = lt->mainread;                          assert(rt);
    lt_find(rt, 1,   1, 5, txn);
#endif
    close_tree();

    setup_tree();
    lt_insert(1, 1);
    lt_insert(1, 2);
    lt_insert(1, 3);
    close_tree();
}

static void init_test(void) {
    for (unsigned i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) 
        nums[i] = i;

    buflen = 64;
    buf = (toku_range*) toku_malloc(buflen*sizeof(toku_range));
    assert(buf);
}

static void close_test(void) {
    toku_free(buf);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    init_test();

    runtest();

    close_test();
    return 0;
}
