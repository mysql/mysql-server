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

void runtest(BOOL dups) {
    DBT _key_left;
    DBT _key_right;
    DBT _data_left;
    DBT _data_right;
    DBT* key_left   = &_key_left;
    DBT* key_right  = &_key_right;
    DBT* data_left  = dups ? &_data_left : NULL;
    DBT* data_right = dups ? &_data_right: NULL;

    dbt_init    (key_left,  &nums[3], sizeof(nums[3]));
    dbt_init    (key_right, &nums[6], sizeof(nums[6]));
    if (dups) {
        dbt_init(data_left, &nums[3], sizeof(nums[3]));
        dbt_init(data_left, &nums[6], sizeof(nums[6]));
    }
    

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

int main(int argc, const char *argv[]) {
    int i;
    for (i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) nums[i] = i;

    runtest(FALSE);
    runtest(TRUE);
    return 0;
}
