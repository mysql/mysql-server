// make sure that the borderwrite merge works

#include "test.h"

int r;
toku_lock_tree* lt  = NULL;
toku_ltm*       ltm = NULL;
DB*             db  = (DB*)1;
enum { MAX_LT_LOCKS = 1000 };
uint32_t max_locks = MAX_LT_LOCKS;
uint64_t max_lock_memory = MAX_LT_LOCKS*64;
int  nums[100];

DBT _keys_left[2];
DBT _keys_right[2];
DBT* keys_left[2];
DBT* keys_right[2];

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
    assert(lt && ltm);
    r = toku_lt_close(lt); CKERR(r);
    r = toku_ltm_close(ltm); CKERR(r);
    lt = NULL;
    ltm = NULL;
}

typedef enum { null = -1, infinite = -2, neg_infinite = -3 } lt_infty;

static DBT* set_to_infty(DBT *dbt, int value) {
    if (value == infinite) 
        return (DBT*)toku_lt_infinity;
    if (value == neg_infinite) 
        return (DBT*)toku_lt_neg_infinity;
    if (value == null) 
        return dbt_init(dbt, NULL, 0);
    assert(0 <= value && (int) (sizeof nums / sizeof nums[0]));
    return dbt_init(dbt, &nums[value], sizeof(nums[0]));
}

static void lt_verify(void) {
    toku_lt_verify(lt, NULL);
}

static void lt_insert_write_range(int r_expect, char txn, int key_l, int key_r) {
    DBT _key_left;
    DBT _key_right;
    DBT* key_left   = &_key_left;
    DBT* key_right  = &_key_right;

    key_left  = set_to_infty(key_left,  key_l);
    key_right = set_to_infty(key_right, key_r);

    TXNID local_txn = (TXNID) (size_t) txn;

    r = toku_lt_acquire_range_write_lock(lt, db, local_txn, key_left, key_right);
    CKERR2(r, r_expect);
    lt_verify();
}

static void lt_unlock(TXNID txnid) {
    r= toku_lt_unlock(lt, txnid); CKERR(r);
}

static void runtest(void) {
    setup_tree();
    lt_insert_write_range(0, 'a', 5, 15);
    lt_insert_write_range(0, 'a', 10, 20);
    for (int k = 5; k <= 20; k++)
        lt_insert_write_range(DB_LOCK_NOTGRANTED, 'b', k, k);
    for (int k = 5; k <= 20; k++)
        lt_insert_write_range(0, 'a', k, k);
    lt_unlock('a');
    close_tree();

    setup_tree();
    lt_insert_write_range(0, 'a', 10, 20);
    lt_insert_write_range(0, 'a', 5, 15);
    for (int k = 5; k <= 20; k++)
        lt_insert_write_range(DB_LOCK_NOTGRANTED, 'b', k, k);
    for (int k = 5; k <= 20; k++)
        lt_insert_write_range(0, 'a', k, k);
    lt_unlock('a');
    close_tree();

    setup_tree();
    lt_insert_write_range(0, 'a', 10, 20);
    for (int k = 10; k <= 20; k++)
        lt_insert_write_range(0, 'a', k, k);
    for (int k = 10; k <= 20; k++)
        lt_insert_write_range(DB_LOCK_NOTGRANTED, 'b', k, k);
    lt_unlock('a');
    close_tree();

    setup_tree();
    lt_insert_write_range(0, 'a', 5, 10);
    lt_insert_write_range(0, 'a', 20, 30);
    lt_insert_write_range(0, 'a', 1, 8);
    lt_unlock('a');
    close_tree();

    setup_tree();
    lt_insert_write_range(0, 'a', 5, 10);
    lt_insert_write_range(0, 'a', 20, 30);
    lt_insert_write_range(0, 'a', 25, 35);
    lt_unlock('a');
    close_tree();

    setup_tree();
    lt_insert_write_range(0, 'a', 5, 10);
    lt_insert_write_range(0, 'a', 20, 30);
    lt_insert_write_range(0, 'a', 1, 35);
    lt_unlock('a');
    close_tree();
}

static void init_test(void) {
    for (unsigned i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) 
        nums[i] = i;
    buflen = 64;
    buf = (toku_range*) toku_malloc(buflen*sizeof(toku_range));
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
