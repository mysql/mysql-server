// test tokudb cardinality in status dictionary
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>
typedef unsigned long long ulonglong;
#include <tokudb_status.h>
#include <tokudb_buffer.h>

// Provide some mimimal MySQL classes just to compile the tokudb cardinality functions
class KEY_INFO {
public:
    uint flags;
    uint64_t *rec_per_key;
};
#define HA_NOSAME 1
class TABLE_SHARE {
public:
    uint primary_key;
    uint keys;
};
class TABLE {
public:
    TABLE_SHARE *s;
    KEY_INFO *key_info;
};
uint get_key_parts(KEY_INFO *key_info) {
    assert(key_info);
    return 0;
}
#include <tokudb_card.h>

// verify that we can create and close a status dictionary
static void test_create(DB_ENV *env) {
    int error;

    DB_TXN *txn = NULL;
    error = env->txn_begin(env, NULL, &txn, 0);
    assert(error == 0);

    DB *status_db = NULL;
    error = tokudb::create_status(env, &status_db, "status.db", txn);
    assert(error == 0);

    error = txn->commit(txn, 0);
    assert(error == 0);

    error = tokudb::close_status(&status_db);
    assert(error == 0);
}

// verify that no card row in status works
static void test_no_card(DB_ENV *env) {
    int error;

    DB_TXN *txn = NULL;
    error = env->txn_begin(env, NULL, &txn, 0);
    assert(error == 0);

    DB *status_db = NULL;
    error = tokudb::open_status(env, &status_db, "status.db", txn);
    assert(error == 0);

    error = tokudb::get_card_from_status(status_db, txn, 0, NULL);
    assert(error == DB_NOTFOUND);

    error = txn->commit(txn, 0);
    assert(error == 0);

    error = tokudb::close_status(&status_db);
    assert(error == 0);
}

// verify that a card row with 0 array elements works
static void test_0(DB_ENV *env) {
    int error;

    DB_TXN *txn = NULL;
    error = env->txn_begin(env, NULL, &txn, 0);
    assert(error == 0);

    DB *status_db = NULL;
    error = tokudb::open_status(env, &status_db, "status.db", txn);
    assert(error == 0);

    tokudb::set_card_in_status(status_db, txn, 0, NULL);

    error = tokudb::get_card_from_status(status_db, txn, 0, NULL);
    assert(error == 0);

    error = txn->commit(txn, 0);
    assert(error == 0);

    error = tokudb::close_status(&status_db);
    assert(error == 0);
}

// verify that writing and reading card info works for several sized card arrays
static void test_10(DB_ENV *env) {
    int error;

    for (uint64_t i = 0; i < 20; i++) {

        uint64_t rec_per_key[i];
        for (uint64_t j = 0; j < i; j++) 
            rec_per_key[j] = j == 0 ? 10+i : 10 * rec_per_key[j-1];

        DB_TXN *txn = NULL;
        error = env->txn_begin(env, NULL, &txn, 0);
        assert(error == 0);

        DB *status_db = NULL;
        error = tokudb::open_status(env, &status_db, "status.db", txn);
        assert(error == 0);

        tokudb::set_card_in_status(status_db, txn, i, rec_per_key);

        uint64_t stored_rec_per_key[i];
        error = tokudb::get_card_from_status(status_db, txn, i, stored_rec_per_key);
        assert(error == 0);

        for (uint64_t j = 0; j < i; j++) 
            assert(rec_per_key[j] == stored_rec_per_key[j]);
        
        error = txn->commit(txn, 0);
        assert(error == 0);
        
        error = tokudb::close_status(&status_db);
        assert(error == 0);

        error = env->txn_begin(env, NULL, &txn, 0);
        assert(error == 0);

        error = tokudb::open_status(env, &status_db, "status.db", txn);
        assert(error == 0);
        
        tokudb::set_card_in_status(status_db, txn, i, rec_per_key);

        error = tokudb::get_card_from_status(status_db, txn, i, stored_rec_per_key);
        assert(error == 0);

        for (uint64_t j = 0; j < i; j++) 
            assert(rec_per_key[j] == stored_rec_per_key[j]);
        
        error = txn->commit(txn, 0);
        assert(error == 0);
        
        error = tokudb::close_status(&status_db);
        assert(error == 0);
    }
}

int main() {
    int error;

    error = system("rm -rf " __FILE__ ".testdir");
    assert(error == 0);

    error = mkdir(__FILE__ ".testdir", S_IRWXU+S_IRWXG+S_IRWXO);
    assert(error == 0);

    DB_ENV *env = NULL;
    error = db_env_create(&env, 0);
    assert(error == 0);

    error = env->open(env, __FILE__ ".testdir", DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    assert(error == 0);

    test_create(env);
    test_no_card(env);
    test_0(env);
    test_10(env);

    error = env->close(env, 0);
    assert(error == 0);
    
    return 0;
}
