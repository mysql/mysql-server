/* We are going to test whether create and close properly check their input. */

#include "test.h"

int r;
toku_lock_tree* lt  = NULL;
toku_ltm*       ltm = NULL;
DB*             db  = (DB*)1;
u_int32_t max_locks = 1000;
BOOL duplicates = FALSE;
int  nums[100];

DBT _keys_left[2];
DBT _keys_right[2];
DBT _datas_left[2];
DBT _datas_right[2];
DBT* keys_left[2];
DBT* keys_right[2];
DBT* datas_left [2];
DBT* datas_right[2];

toku_point qleft, qright;
toku_interval query;
toku_range* buf;
unsigned buflen;
unsigned numfound;

static void init_query(BOOL dups) {  
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

static void setup_tree(BOOL dups) {
    assert(!lt && !ltm);
    r = toku_ltm_create(&ltm, max_locks, dbpanic,
                        get_compare_fun_from_db, get_dup_compare_from_db,
                        toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(ltm);
    r = toku_lt_create(&lt, dups, dbpanic, ltm,
                       get_compare_fun_from_db, get_dup_compare_from_db,
                       toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lt);
    init_query(dups);
}

static void close_tree(void) {
    assert(lt && ltm);
    r = toku_lt_close(lt);
        CKERR(r);
    r = toku_ltm_close(ltm);
        CKERR(r);
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


static void lt_insert(BOOL dups, int r_expect, char txn, int key_l, int data_l, 
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

    TXNID local_txn = (TXNID) (size_t) txn;

    if (read_flag)
        r = toku_lt_acquire_range_read_lock(lt, db, local_txn,
                                            key_left,  data_left,
                                            key_right, data_right);
    else
        r = toku_lt_acquire_write_lock(lt, db, local_txn, key_left, data_left);
    CKERR2(r, r_expect);
}

static void lt_insert_read(BOOL dups, int r_expect, char txn, int key_l, int data_l, 
                    int key_r, int data_r) {
    lt_insert(dups, r_expect, txn, key_l, data_l, key_r, data_r, TRUE);
}

static void lt_insert_write(BOOL dups, int r_expect, char txn, int key_l, int data_l) {
    lt_insert(dups, r_expect, txn, key_l, data_l, 0, 0, FALSE);
}


static void lt_unlock(char ctxn) {
  int retval;
  retval = toku_lt_unlock(lt, (TXNID) (size_t) ctxn);
  CKERR(retval);
}
              
static void runtest(BOOL dups) {
    
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
    lt_insert_read (dups, DB_LOCK_NOTGRANTED, 'b', 1, 1, 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_read (dups, 0, 'b', 1, 1, 1, 1);
    lt_insert_write(dups, DB_LOCK_NOTGRANTED, 'a', 1, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_read (dups, DB_LOCK_NOTGRANTED, 'b', 2, 1, 4, 1);
    close_tree();
    /* ********************* */
    setup_tree(dups);
    lt_insert_write(dups, 0, 'a', 1, 1);
    lt_insert_write(dups, 0, 'a', 2, 1);
    lt_insert_write(dups, 0, 'a', 3, 1);
    lt_insert_write(dups, 0, 'a', 4, 1);
    lt_insert_write(dups, 0, 'a', 5, 1);
    lt_insert_write (dups, DB_LOCK_NOTGRANTED, 'b', 2, 1);
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
    lt_insert_read (dups, DB_LOCK_NOTGRANTED, 'a', 3, 1, 7, 1);
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
    lt_insert_read (dups, DB_LOCK_NOTGRANTED, 'a', 3, 1, 7, 1);
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
    lt_insert_read (dups, DB_LOCK_NOTGRANTED, 'a', 3, 1, 7, 1);
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
    lt_insert_read (dups, DB_LOCK_NOTGRANTED, 'b', 3, 1, 3, 1);
    lt_unlock('a');
    lt_insert_write(dups, 0, 'b', 3, 1);
    lt_insert_read (dups, DB_LOCK_NOTGRANTED, 'a', 3, 1, 3, 1);
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


static void init_test(void) {
    unsigned i;
    for (i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) nums[i] = i;

    buflen = 64;
    buf = (toku_range*) toku_malloc(buflen*sizeof(toku_range));
}

static void close_test(void) {
    toku_free(buf);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    init_test();

    runtest(FALSE);
    runtest(TRUE);

    close_test();

    return 0;
}
