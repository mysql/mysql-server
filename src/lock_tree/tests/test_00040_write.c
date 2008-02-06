/* We are going to test whether create and close properly check their input. */

#include "test.h"

toku_range_tree* __toku_lt_ifexist_selfwrite(toku_lock_tree* tree, DB_TXN* txn);
toku_range_tree* __toku_lt_ifexist_selfread(toku_lock_tree* tree, DB_TXN* txn);

int r;
toku_lock_tree* lt  = NULL;
DB*             db  = (DB*)1;
u_int32_t mem = 1000;
u_int32_t memcnt = 0;
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

void init_query(BOOL dups) {  
    init_point(&qleft,  lt);
    init_point(&qright, lt);
    
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

void setup_tree(BOOL dups) {
    memcnt = 0;
    r = toku_lt_create(&lt, db, dups, dbpanic, mem, &memcnt, dbcmp, dbcmp,
                       toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lt);
    init_query(dups);
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


void lt_insert(BOOL dups, int r_expect, char txn, int key_l, int data_l, 
               int key_r, int data_r, BOOL read_flag) {
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
        assert(!read_flag || (key_right && data_right));
    } else {
        data_left = data_right = NULL;
        assert(key_left  && !data_left);
        assert(!read_flag || (key_right && !data_right));
    }

    DB_TXN* local_txn = (DB_TXN*) (size_t) txn;

    if (read_flag)
        r = toku_lt_acquire_range_read_lock(lt, local_txn, key_left,  data_left,
                                            key_right, data_right);
    else
        r = toku_lt_acquire_write_lock(lt, local_txn, key_left,  data_left);
    CKERR2(r, r_expect);
}

void lt_insert_read(BOOL dups, int r_expect, char txn, int key_l, int data_l, 
                    int key_r, int data_r) {
    lt_insert(dups, r_expect, txn, key_l, data_l, key_r, data_r, TRUE);
}

void lt_insert_write(BOOL dups, int r_expect, char txn, int key_l, int data_l) {
    lt_insert(dups, r_expect, txn, key_l, data_l, 0, 0, FALSE);
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
                                    char char_txn) {

    r = toku_rt_find(rt, &query, 0, &buf, &buflen, &numfound);
    CKERR(r);
    assert(numfound==k);

    DB_TXN* find_txn = (DB_TXN *) (size_t) char_txn;

    toku_point left, right;
    init_point(&left, lt);
    setup_payload_len(&left.key_payload, &left.key_len, key_l);
    if (dups) {
        if (key_l < null) left.data_payload = left.key_payload;
        else setup_payload_len(&left.data_payload, &left.data_len, data_l);
    }
    init_point(&right, lt);
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

void lt_unlock(char ctxn) {
  int r;
  r = toku_lt_unlock(lt, (DB_TXN *) (size_t) ctxn);
  CKERR(r);
}
              
void runtest(BOOL dups) {
    
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_read (dups, 0, 'a', 1, 1, 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_read (dups, DB_LOCK_DEADLOCK, 'b', 1, 1, 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_read (dups, 0, 'b', 1, 1, 1, 1);
    lt_insert_write(dups, DB_LOCK_DEADLOCK, 'a', 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_read (dups, DB_LOCK_DEADLOCK, 'b', 2, 1, 4, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_write (dups, DB_LOCK_DEADLOCK, 'b', 2, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_read (dups, 0, 'b', 3, 1, 3, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_read (dups, 0, 'b', 3, 1, 3, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'b', 1, 1);
    lt_insert_write(dups, 0, 'b', 2, 1);
    lt_insert_write(dups, 0, 'b', 3, 1);
    lt_insert_write(dups, 0, 'b', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_write(dups, 0, 'a', 6, 1);
    lt_insert_write(dups, 0, 'a', 7, 1);
    lt_insert_write(dups, 0, 'a', 8, 1);
    lt_insert_write(dups, 0, 'a', 9, 1);
    lt_insert_read (dups, DB_LOCK_DEADLOCK, 'a', 3, 1, 7, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'b', 1, 1);
    lt_insert_write(dups, 0, 'b', 2, 1);
    lt_insert_write(dups, 0, 'b', 3, 1);
    lt_insert_write(dups, 0, 'b', 4, 1);
    lt_insert_write(dups, 0, 'b', 5, 1);
    lt_insert_write(dups, 0, 'b', 6, 1);
    lt_insert_write(dups, 0, 'b', 7, 1);
    lt_insert_write(dups, 0, 'b', 8, 1);
    lt_insert_write(dups, 0, 'b', 9, 1);
    lt_insert_read (dups, DB_LOCK_DEADLOCK, 'a', 3, 1, 7, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_read (dups, 0, 'a', 3, 1, 7, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'b', 1, 1);
    lt_insert_write(dups, 0, 'b', 2, 1);
    lt_insert_write(dups, 0, 'b', 3, 1);
    lt_insert_write(dups, 0, 'b', 4, 1);
    lt_insert_read (dups, DB_LOCK_DEADLOCK, 'a', 3, 1, 7, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'b', 4, 1);
    lt_insert_write(dups, 0, 'b', 5, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_read (dups, DB_LOCK_DEADLOCK, 'b', 3, 1, 3, 1);
    lt_unlock('a');
    lt_insert_write(dups, 0, 'b', 3, 1);
    lt_insert_read (dups, DB_LOCK_DEADLOCK, 'a', 3, 1, 3, 1);
    lt_unlock('b');
    lt_insert_read (dups, 0, 'a', 3, 1, 3, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    lt_insert_write(dups, 0, 'b', 2, 1);
    lt_unlock('b');
    close_tree();
    /* ********************* */
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
