/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>
#include "tokuconst.h"
#define MAX_NEST MAX_TRANSACTION_RECORDS
#define MAX_SIZE (MAX_TRANSACTION_RECORDS + 1)

/*********************
 *
 * Purpose of this test is to verify insert ignore (DB_NOOVERWRITE_NO_ERROR) with nested transactions,
 * including support for implicit promotion
 * in the presence of placeholders and branched trees of transactions.
 *
create empty db
for test = 1 to MAX
   for nesting level 0
     - randomly insert or not
   for nesting_level = 1 to MAX
     - begin txn
     - randomly perform four operations, each of which is one of (insert, delete, do nothing)
     -  if insert, use a value/len unique to this txn
     - query to verify
   for nesting level = MAX to 1
     - randomly abort or commit each transaction or
      - insert or delete at same level (followed by either abort/commit)
      - branch (add more child txns similar to above)
     - query to verify
delete db
 *
 */


enum { TYPE_DELETE = 1, TYPE_INSERT, TYPE_PLACEHOLDER };

BOOL top_is_delete;
u_int8_t junkvalbuf[MAX_SIZE];
DBT junkval;
u_int8_t valbufs[MAX_NEST][MAX_SIZE];
DBT vals        [MAX_NEST];
u_int8_t keybuf [MAX_SIZE];
DBT key;
u_int8_t types  [MAX_NEST];
u_int8_t currval[MAX_NEST];
DB_TXN   *txns   [MAX_NEST];
DB_TXN   *txn_query;
DB_TXN   *patient_txn;
int which_expected;

static void
fillrandom(u_int8_t buf[MAX_SIZE], u_int32_t length) {
    assert(length < MAX_SIZE);
    u_int32_t i;
    for (i = 0; i < length; i++) {
        buf[i] = random() & 0xFF;
    } 
}

static void
initialize_values (void) {
    int nest_level;
    for (nest_level = 0; nest_level < MAX_NEST; nest_level++) {
        fillrandom(valbufs[nest_level], nest_level);
        dbt_init(&vals[nest_level], &valbufs[nest_level][0], nest_level);
    }
    u_int32_t len = random() % MAX_SIZE;
    fillrandom(keybuf, len);
    dbt_init(&vals[nest_level], &keybuf[0], len);

    fillrandom(junkvalbuf, MAX_SIZE-1);
    dbt_init(&junkval, &junkvalbuf[0], MAX_SIZE-1);
}


static DB *db;
static DB_ENV *env;

static void
setup_db (void) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN | DB_PRIVATE | DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    CKERR(r);

    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0); CKERR(r);
    }
    r = env->txn_begin(env, NULL, &txn_query, DB_READ_UNCOMMITTED);
        CKERR(r);
}


static void
close_db (void) {
    int r;
    r = txn_query->commit(txn_query, 0);
    CKERR(r);
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

static void
verify_val(u_int8_t nest_level) {
    assert(nest_level < MAX_NEST);
    if (nest_level>0) assert(txns[nest_level]);
    assert(types[nest_level] != TYPE_PLACEHOLDER);
    int r;
    DBT observed_val;
    dbt_init(&observed_val, NULL, 0);
    r = db->get(db, txn_query, &key, &observed_val, 0);
    if (types[nest_level] == TYPE_INSERT) {
        CKERR(r);
        int idx = currval[nest_level];
        assert(observed_val.size == vals[idx].size);
        assert(memcmp(observed_val.data, vals[idx].data, vals[idx].size) == 0);
        top_is_delete = FALSE;
    }
    else {
        assert(types[nest_level] == TYPE_DELETE);
        CKERR2(r, DB_NOTFOUND);
        top_is_delete = TRUE;
    }
}

static u_int8_t
randomize_no_placeholder_type(void) {
    int r;
    r = random() % 2;
    switch (r) {
        case 0:
            return TYPE_INSERT;
        case 1:
            return TYPE_DELETE;
        default:
            assert(FALSE);
	    return 0;
    }
}

static u_int8_t
randomize_type(void) {
    int r;
    r = random() % 4;
    switch (r) {
        case 0:
            return TYPE_INSERT;
        case 1:
            return TYPE_DELETE;
        case 2:
        case 3:
            return TYPE_PLACEHOLDER;
        default:
            assert(FALSE);
	    return 0;
    }
}

static void
maybe_insert_or_delete(uint8_t nest, int type) {
    int r;
    if (nest>0) assert(txns[nest]);
    types[nest] = type;
    currval[nest] = nest;
    switch (types[nest]) {
        case TYPE_INSERT:
            if (top_is_delete) {
                r = db->put(db, txns[nest], &key, &vals[nest], DB_NOOVERWRITE_NO_ERROR);
                    CKERR(r);
            }
            else {
                r = db->put(db, txns[nest], &key, &vals[nest], DB_YESOVERWRITE);
                    CKERR(r);
            }
            r = db->put(db, txns[nest], &key, &junkval, DB_NOOVERWRITE_NO_ERROR);
                CKERR(r);
            top_is_delete = FALSE;
            break;
        case TYPE_DELETE:
            r = db->del(db, txns[nest], &key, DB_DELETE_ANY);
                CKERR(r);
            top_is_delete = TRUE;
            break;
        case TYPE_PLACEHOLDER:
            types[nest] = types[nest - 1];
            currval[nest] = currval[nest-1];
            break;
        default:
            assert(FALSE);
    }
    verify_val(nest);
}

static void
start_txn_and_maybe_insert_or_delete(u_int8_t nest) {
    int iteration;
    int r;
    for (iteration = 0; iteration < 4; iteration++) {
        BOOL skip = FALSE;
        if (nest == 0) {
            types[nest] = randomize_no_placeholder_type();
            assert(types[nest] != TYPE_PLACEHOLDER);
            //Committed entry is autocommitted by not providing the txn
            txns[nest] = NULL;
        }
        else {
            if (iteration == 0) {
                types[nest] = randomize_type();
                r = env->txn_begin(env, txns[nest-1], &txns[nest], 0);
                    CKERR(r);
                if (types[nest] == TYPE_PLACEHOLDER) skip = TRUE;
            }
            else {
                types[nest] = randomize_no_placeholder_type();
                assert(types[nest] != TYPE_PLACEHOLDER);
            }
        }
        maybe_insert_or_delete(nest, types[nest]);
        assert(types[nest] != TYPE_PLACEHOLDER);
        if (skip) break;
    }
}

static void
initialize_db(void) {
    types[0] = TYPE_DELETE; //Not yet inserted
    verify_val(0);
    int i;
    for (i = 0; i < MAX_NEST; i++) {
        start_txn_and_maybe_insert_or_delete(i);
    }
}

static void
test_txn_nested_jumble (int iteration) {
    int r;
    if (verbose) { fprintf(stderr, "%s (%s):%d [iteration # %d]\n", __FILE__, __FUNCTION__, __LINE__, iteration); fflush(stderr); }

    initialize_db();
    r = env->txn_begin(env, NULL, &patient_txn, 0);
        CKERR(r);

    int index_of_expected_value  = MAX_NEST - 1;
    int nest_level               = MAX_NEST - 1;
    int min_allowed_branch_level = MAX_NEST - 2;
futz_with_stack:
    while (nest_level > 0) {
        int operation = random() % 4;
        switch (operation) {
            case 0:
                //abort
                r = txns[nest_level]->abort(txns[nest_level]);
                    CKERR(r);
                index_of_expected_value = nest_level - 1;
                txns[nest_level] = NULL;
                nest_level--;
                verify_val(index_of_expected_value);
                break;
            case 1:
                //commit
                r = txns[nest_level]->commit(txns[nest_level], DB_TXN_NOSYNC);
                    CKERR(r);
                currval[nest_level-1] = currval[index_of_expected_value];
                types[nest_level-1]   = types[index_of_expected_value];
                index_of_expected_value = nest_level - 1;
                txns[nest_level] = NULL;
                nest_level--;
                verify_val(index_of_expected_value);
                break;
            case 2:;
                //do more work with this guy
                int type = randomize_no_placeholder_type();
                maybe_insert_or_delete(nest_level, type);
                index_of_expected_value = nest_level;
                continue; //transaction is still alive
            case 3:
                if (min_allowed_branch_level >= nest_level) {
                    //start new subtree
                    int max = nest_level + 4;
                    if (MAX_NEST - 1 < max) max = MAX_NEST - 1;
                    assert(max > nest_level);
                    int branch_level;
                    for (branch_level = nest_level + 1; branch_level <= max; branch_level++) {
                        start_txn_and_maybe_insert_or_delete(branch_level);
                    }
                    nest_level = max;
                    min_allowed_branch_level--;
                    index_of_expected_value = nest_level;
                }
                continue; //transaction is still alive
            default:
                assert(FALSE);
        }
    }
    //All transactions that have touched this key are finished.
    assert(nest_level == 0);
    if (min_allowed_branch_level >= 0) {
        //start new subtree
        int max = 4;
        assert(patient_txn);
        txns[1] = patient_txn;
        patient_txn = NULL;
        maybe_insert_or_delete(1, randomize_no_placeholder_type());
        int branch_level;
        for (branch_level = 2; branch_level <= max; branch_level++) {
            start_txn_and_maybe_insert_or_delete(branch_level);
        }
        nest_level = max;
        min_allowed_branch_level = -1;
        index_of_expected_value = nest_level;
        goto futz_with_stack;
    }

    //Clean out dictionary

    types[0] = TYPE_DELETE;
    r = db->del(db, NULL, &key, DB_DELETE_ANY);
        CKERR(r);
    verify_val(0);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    initialize_values();
    int i;
    setup_db();
    for (i = 0; i < 64; i++) {
        test_txn_nested_jumble(i);
    }
    close_db();
    return 0;
}

