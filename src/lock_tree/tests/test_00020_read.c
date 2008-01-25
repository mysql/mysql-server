/* We are going to test whether create and close properly check their input. */

#include "test.h"

int r;
toku_lock_tree* lt  = NULL;
DB*             db  = (DB*)1;
DB_TXN*         txn = (DB_TXN*)1;
BOOL duplicates = FALSE;
int  nums[100];

void setup_tree(BOOL dups) {
    r = toku_lt_create(&lt, db, dups, dbcmp, dbcmp,
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

void runtest(BOOL dups) {
    int i;
    const DBT* choices[3];
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
}

int main(int argc, const char *argv[]) {
    int i;
    for (i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) nums[i] = i;

    runtest(FALSE);
    runtest(TRUE);
    return 0;
}
