/* We are going to test whether create and close properly check their input. */

#include "test.h"

toku_range_tree* __toku_lt_ifexist_selfwrite(toku_lock_tree* tree, DB_TXN* txn);
toku_range_tree* __toku_lt_ifexist_selfread(toku_lock_tree* tree, DB_TXN* txn);

int r;
toku_lock_tree* lt  = NULL;
DB*             db  = (DB*)1;
DB_TXN*         txn = (DB_TXN*)1;
size_t mem = 4096 * 1000;
BOOL duplicates = FALSE;
int  nums[100];

DBT _key_left[2];
DBT _key_right[2];
DBT _data_left[2];
DBT _data_right[2];
DBT* key_left[2]   ;
DBT* key_right[2]  ;
DBT* data_left [2] ;
DBT* data_right[2] ;

toku_point qleft, qright;
toku_range query;
toku_range* buf;
unsigned buflen;
unsigned numfound;

void setup_tree(BOOL dups) {
    r = toku_lt_create(&lt, db, dups, dbpanic, mem, dbcmp, dbcmp,
                       toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lt);
}

void close_tree(void) {
    assert(lt);
    r = toku_lt_close(lt);
    CKERR(r);
    lt = NULL;
}

typedef enum { null = -1, infinite = -2, neg_infinite = -3 } lt_infty;

DBT* set_to_infty(DBT *dbt, lt_infty value) {
    if (value == infinite) return (DBT*)toku_lt_infinity;
    if (value == neg_infinite) return (DBT*)toku_lt_neg_infinity;
    if (value == null) return dbt_init(dbt, NULL, 0);
    assert(value >= 0);
    return                    dbt_init(dbt, &nums[value], sizeof(nums[0]));
}


void lt_insert(BOOL dups, int key_l, int data_l, int key_r, int data_r) {
    DBT _key_left;
    DBT _key_right;
    DBT _data_left;
    DBT _data_right;
    DBT* key_left   = &_key_left;
    DBT* key_right  = &_key_right;
    DBT* data_left  = dups ? &_data_left : NULL;
    DBT* data_right = dups ? &_data_right: NULL;

    key_left  = set_to_infty(key_left,  key_l);
    key_right = set_to_infty(key_right, key_r);
    if (dups) {
        if (key_left != &_key_left) data_left = key_left;
        else data_left = set_to_infty(data_left,  data_l);
        if (key_right != &_key_right) data_right = key_right;
        else data_right = set_to_infty(data_right,  data_r);
        assert(key_left  && data_left);
        assert(key_right && data_right);
    } else {
        data_left = data_right = NULL;
        assert(key_left  && !data_left);
        assert(key_right && !data_right);
    }

    r = toku_lt_acquire_range_read_lock(lt, txn, key_left,  data_left,
                                                 key_right, data_right);
    CKERR(r);
}

void setup_payload_len(void** payload, u_int32_t* len, int val) {
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

void lt_find(BOOL dups, toku_range_tree* rt,
                        unsigned k, int key_l, int data_l,
                                    int key_r, int data_r,
                                    DB_TXN* find_txn) {

    r = toku_rt_find(rt, &query, 0, &buf, &buflen, &numfound);
    CKERR(r);
    assert(numfound==k);

    toku_point left, right;
    memset(&left,0,sizeof(left));
    setup_payload_len(&left.key_payload, &left.key_len, key_l);
    if (dups) {
        if (key_l < null) left.data_payload = left.key_payload;
        else setup_payload_len(&left.data_payload, &left.data_len, data_l);
    }
    memset(&right,0,sizeof(right));
    setup_payload_len(&right.key_payload, &right.key_len, key_r);
    if (dups) {
        if (key_r < null) right.data_payload = right.key_payload;
        else setup_payload_len(&right.data_payload, &right.data_len, data_r);
    }
    unsigned i;
    for (i = 0; i < numfound; i++) {
        if (__toku_lt_point_cmp(buf[i].left,  &left ) == 0 &&
            __toku_lt_point_cmp(buf[i].right, &right) == 0 &&
            buf[i].data == find_txn) return;
    }
    assert(FALSE);  //Crash since we didn't find it.
}
              

void insert_1(BOOL dups, int key_l, int key_r, int data_l, int data_r,
              const void* kl, const void* dl, const void* kr, const void* dr) {
    DBT _key_left;
    DBT _key_right;
    DBT _data_left;
    DBT _data_right;
    DBT* key_left   = &_key_left;
    DBT* key_right  = &_key_right;
    DBT* data_left  = dups ? &_data_left : NULL;
    DBT* data_right = dups ? &_data_right: NULL;

    dbt_init    (key_left,  &nums[key_l], sizeof(nums[key_l]));
    dbt_init    (key_right, &nums[key_r], sizeof(nums[key_r]));
    if (dups) {
        dbt_init(data_left,  &nums[data_l], sizeof(nums[data_l]));
        dbt_init(data_right, &nums[data_r], sizeof(nums[data_r]));
        if (dl) data_left  = (DBT*)dl;
        if (dr) data_right = (DBT*)dr;
    }
    if (kl) key_left   = (DBT*)kl;
    if (kr) key_right  = (DBT*)kr;
    

    setup_tree(dups);
    r = toku_lt_acquire_range_read_lock(lt, txn, key_left,  data_left,
                                                 key_right, data_right);
    CKERR(r);
    close_tree();

    setup_tree(dups);
    r = toku_lt_acquire_read_lock(lt, txn, key_left, data_left);
    CKERR(r);
    close_tree();
}

void insert_2_noclose(BOOL dups, int key_l[2], int key_r[2], 
              int data_l[2], int data_r[2],
              const void* kl[2], const void* dl[2], 
              const void* kr[2], const void* dr[2]) {
    int i;

    setup_tree(dups);

    for (i = 0; i < 2; i++) {
    	key_left[i]   = &_key_left[i];
    	key_right[i]  = &_key_right[i];
    	data_left [i] = dups ? &_data_left[i] : NULL;
    	data_right[i] = dups ? &_data_right[i] : NULL;

    	dbt_init    (key_left[i],  &nums[key_l[i]], sizeof(nums[key_l[i]]));
    	dbt_init    (key_right[i], &nums[key_r[i]], sizeof(nums[key_r[i]]));
    	if (dups) {
        	dbt_init(data_left[i],  &nums[data_l[i]], 
                         sizeof(nums[data_l[i]]));
        	dbt_init(data_right[i], &nums[data_r[i]], 
                         sizeof(nums[data_r[i]]));
        	if (dl[i]) data_left[i]  = (DBT*)dl[i];
        	if (dr[i]) data_right[i] = (DBT*)dr[i];
    	}

    	if (kl[i]) key_left[i]   = (DBT*)kl[i];
    	if (kr[i]) key_right[i]  = (DBT*)kr[i];
    
    	r = toku_lt_acquire_range_read_lock(lt, txn, key_left[i],  data_left[i],
                                            key_right[i], data_right[i]);
        CKERR(r);

    }

}

void init_query(BOOL dups) {  

    memset(&qleft, 0,sizeof(qleft));
    memset(&qright,0,sizeof(qright));
    
    qleft.key_payload  = (void *) toku_lt_neg_infinity;
    qright.key_payload = (void *) toku_lt_infinity;

    if (dups) {
        qleft.data_payload  = qleft.key_payload;
        qright.data_payload = qright.key_payload;
    }

    memset(&query,0,sizeof(query));
    query.left  = &qleft;
    query.right = &qright;
}


void runtest(BOOL dups) {
    int i;
    const DBT* choices[3];

    init_query(dups);

    choices[0] = toku_lt_neg_infinity;
    choices[1] = NULL;
    choices[2] = toku_lt_infinity;
    for (i = 0; i < 9; i++) {
        int a = i / 3;
        int b = i % 3;
        if (a > b) continue;

        insert_1(dups, 3, 3, 7, 7, choices[a], choices[a],
                                   choices[b], choices[b]);
    }
    
    toku_range_tree *rt;
    /* ************************************** */
    setup_tree(dups);

    /////BUG HERE MAYBE NOT CONSOLIDATING.
    /*nodups:
        [(3, 3),  (7,7)] and [(4,4), (5,5)]
      dups:
        [(3, 3),  (3,7)] and [(3,4), (3,5)]
    */
    
    lt_insert(dups,
              3,            3,
              dups ? 3 : 7, 7);
    lt_insert(dups,
              dups ? 3 : 4, 4,
              dups ? 3 : 5, 5);
    
    rt = __toku_lt_ifexist_selfread(lt, txn);
    assert(rt);

    lt_find(dups, rt, 1,
            3,            3,
            dups ? 3 : 7, 7,
            txn);

    rt = lt->mainread;
    assert(rt);

    lt_find(dups, rt, 1,
            3,            3,
            dups ? 3 : 7, 7,
            txn);

    close_tree();
    /* ************************************** */
    setup_tree(dups);

    /*nodups:
        [(3, 3),  (7,7)] and [(4,4), (5,5)]
      dups:
        [(3, 3),  (3,7)] and [(3,4), (3,5)]
    */
    lt_insert(dups,
              dups ? 3 : 4, 4,
              dups ? 3 : 5, 5);
    lt_insert(dups,
              3,            3,
              dups ? 3 : 7, 7);
    
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);

    lt_find(dups, rt, 1,
            3,            3,
            dups ? 3 : 7, 7,
            txn);

    rt = lt->mainread;                          assert(rt);

    lt_find(dups, rt, 1,
            3,            3,
            dups ? 3 : 7, 7,
            txn);
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree(dups);
    lt_insert(dups, 3, 3, 3, 3);
    lt_insert(dups, 4, 4, 4, 4);
    lt_insert(dups, 3, 3, 3, 3);
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 2, 3, 3, 3, 3, txn);
    lt_find(dups, rt, 2, 4, 4, 4, 4, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 2, 3, 3, 3, 3, txn);
    lt_find(dups, rt, 2, 4, 4, 4, 4, txn);
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree(dups);
    for (i = 0; i < 20; i += 2) {
        lt_insert(dups,       i, 5, i + 1, 10);
    }
    rt = __toku_lt_ifexist_selfread(lt, txn);
    assert(rt);
    for (i = 0; i < 20; i += 2) {
        lt_find(dups, rt, 10, i, 5, i + 1, 10, txn);
    }
    rt = lt->mainread; assert(rt);
    for (i = 0; i < 20; i += 2) {
        lt_find(dups, rt, 10, i, 5, i + 1, 10, txn);
    }
    lt_insert(dups,        0, neg_infinite, 20, infinite);
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(  dups, rt, 1, 0, neg_infinite, 20, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(  dups, rt, 1, 0, neg_infinite, 20, infinite, txn);
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree(dups);
    lt_insert(dups,        0, neg_infinite, 1, infinite);
    lt_insert(dups,        1, neg_infinite, 2, infinite);

    lt_insert(dups,        4, neg_infinite, 5, infinite);
    lt_insert(dups,        3, neg_infinite, 4, infinite);
    
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 2,   0, neg_infinite, 2, infinite, txn);
    lt_find(dups, rt, 2,   3, neg_infinite, 5, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 2,   0, neg_infinite, 2, infinite, txn);
    lt_find(dups, rt, 2,   3, neg_infinite, 5, infinite, txn);

    lt_insert(dups,        2, neg_infinite, 3, infinite);

    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 1,   0, neg_infinite, 5, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 1,   0, neg_infinite, 5, infinite, txn);
    rt = NULL;
    close_tree();
    /* ************************************** */
    setup_tree(dups);
    lt_insert(dups,        1, neg_infinite, 3, infinite);
    lt_insert(dups,        4, neg_infinite, 6, infinite);
    lt_insert(dups,        2, neg_infinite, 5, infinite);
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, 6, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, 6, infinite, txn);
    close_tree();

    setup_tree(dups);
    lt_insert(dups, neg_infinite, neg_infinite, 3, infinite);
    lt_insert(dups,        4, neg_infinite, 5, infinite);
    lt_insert(dups,        6, neg_infinite, 8, infinite);
    lt_insert(dups,        2, neg_infinite, 7, infinite);
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 1,   neg_infinite, neg_infinite, 8, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 1,   neg_infinite, neg_infinite, 8, infinite, txn);
    close_tree();

    setup_tree(dups);
    lt_insert(dups,        1, neg_infinite, 2, infinite);
    lt_insert(dups,        3, neg_infinite, infinite, infinite);
    lt_insert(dups,        2, neg_infinite, 3, infinite);
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, infinite, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, infinite, infinite, txn);
    close_tree();

    setup_tree(dups);
    lt_insert(dups,        1, neg_infinite, 2, infinite);
    lt_insert(dups,        3, neg_infinite, 4, infinite);
    lt_insert(dups,        5, neg_infinite, 6, infinite);
    lt_insert(dups,        2, neg_infinite, 5, infinite);
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, 6, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, 6, infinite, txn);
    close_tree();

    setup_tree(dups);
    lt_insert(dups,        1, neg_infinite, 2, infinite);
    lt_insert(dups,        3, neg_infinite, 5, infinite);
    lt_insert(dups,        2, neg_infinite, 4, infinite);
    rt = __toku_lt_ifexist_selfread(lt, txn);   assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, 5, infinite, txn);
    rt = lt->mainread;                          assert(rt);
    lt_find(dups, rt, 1,   1, neg_infinite, 5, infinite, txn);
    close_tree();

    /* ************************************** */
}


void init_test(void) {
    unsigned i;
    for (i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) nums[i] = i;

    buflen = 64;
    buf = (toku_range*) toku_malloc(buflen*sizeof(toku_range));
}





int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    init_test();

    runtest(FALSE);
    runtest(TRUE);

    return 0;
}
