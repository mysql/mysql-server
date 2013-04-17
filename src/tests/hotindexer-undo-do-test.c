// test the hotindexer undo do function
// read a description of the live transactions and a leafentry from a test file, run the undo do function,
// and print out the actions taken by the undo do function while processing the leafentry

#include "test.h"
#include <stdbool.h>

#include "tokuconst.h"
#include "brttypes.h"
#include "omt.h"
#include "mempool.h"
#include "leafentry.h"
#include "ule.h"
#include "ule-internal.h"
#include "le-cursor.h"
#include "indexer-internal.h"
#include "xids-internal.h"

typedef enum {
    TOKUTXN_NOT_LIVE, TOKUTXN_LIVE, TOKUTXN_COMMIT, TOKUTXN_ABORT,
} TOKUTXN_STATE;

struct txn {
    TXNID xid;
    TOKUTXN_STATE state;
};

struct live {
    int n;
    int o;
    struct txn *txns;
};

static void
live_init(struct live *live) {
    live->n = live->o = 0;
    live->txns = NULL;
}

static void
live_destroy(struct live *live) {
    toku_free(live->txns);
}

static void
live_add(struct live *live, TXNID xid, TOKUTXN_STATE state) {
    if (live->o >= live->n) {
        int newn = live->n == 0 ? 1 : live->n * 2;
        live->txns = (struct txn *) toku_realloc(live->txns, newn * sizeof (struct txn));
        resource_assert(live->txns);
        live->n = newn;
    }
    live->txns[live->o++] = (struct txn ) { xid, state };
}

static int
txn_state(struct live *live, TXNID xid) {
    int r = TOKUTXN_NOT_LIVE;
    for (int i = 0; i < live->o; i++) {
        if (live->txns[i].xid == xid) {
            r = live->txns[i].state;
            break;
        }
    }
    return r;
}

// live transaction ID set
struct live live_xids;

static void
uxr_init(UXR uxr, uint8_t type, void *val, uint32_t vallen, TXNID xid) {
    uxr->type = type;
    uxr->valp = toku_malloc(vallen); resource_assert(uxr->valp);
    memcpy(uxr->valp, val, vallen);
    uxr->vallen = vallen;
    uxr->xid = xid;
}

static void
uxr_destroy(UXR uxr) {
    toku_free(uxr->valp);
    uxr->valp = NULL;
}

static ULE
ule_init(ULE ule) {
    ule->num_puxrs = 0;
    ule->num_cuxrs = 0;
    ule->keyp = NULL;
    ule->keylen = 0;
    ule->uxrs = ule->uxrs_static;
    return ule;
}

static void
ule_set_key(ULE ule, void *key, uint32_t keylen) {
    ule->keyp = toku_realloc(ule->keyp, keylen);
    memcpy(ule->keyp, key, keylen);
    ule->keylen = keylen;
}

static void
ule_destroy(ULE ule) {
    for (unsigned int i = 0; i < ule->num_cuxrs + ule->num_puxrs; i++) 
        uxr_destroy(&ule->uxrs[i]);
    toku_free(ule->keyp);
    ule->keyp = NULL;
}

static void
ule_add_provisional(ULE ule, UXR uxr) {
    invariant(ule->num_cuxrs + ule->num_puxrs + 1 <= MAX_TRANSACTION_RECORDS*2);
    ule->uxrs[ule->num_cuxrs + ule->num_puxrs] = *uxr;
    ule->num_puxrs++;
}

static void
ule_add_committed(ULE ule, UXR uxr) {
    lazy_assert(ule->num_puxrs == 0);
    invariant(ule->num_cuxrs + 1 <= MAX_TRANSACTION_RECORDS*2);
    ule->uxrs[ule->num_cuxrs] = *uxr;
    ule->num_cuxrs++;
}

static ULE
ule_create(void) {
    ULE ule = (ULE) toku_calloc(1, sizeof (ULE_S)); resource_assert(ule);
    if (ule)
        ule_init(ule);
    return ule;
}

static void
ule_free(ULE ule) {
    ule_destroy(ule);
    toku_free(ule);
}

static void
print_xids(XIDS xids) {
    printf("[");
    if (xids->num_xids == 0)
        printf("0");
    else {
        for (int i = 0; i < xids->num_xids; i++) {
            printf("%lu", xids->ids[i]);
            if (i+1 < xids->num_xids)
                printf(",");
        }
    }
    printf("] ");
}

static void
print_dbt(DBT *dbt) {
    printf("%.*s ", dbt->size, (char *) dbt->data);
}

static int
put_callback(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_data = dest_data; src_key = src_key; src_data = src_data;

    lazy_assert(src_db != NULL && dest_db != NULL);

    switch (dest_key->flags) {
    case 0:
        dest_key->data = src_data->data;
        dest_key->size = src_data->size;
        break;
    case DB_DBT_REALLOC:
        dest_key->data = toku_realloc(dest_key->data, src_data->size);
        memcpy(dest_key->data, src_data->data, src_data->size);
        dest_key->size = src_data->size;
        break;
    default:
        lazy_assert(0);
    }

    if (dest_data)
        switch (dest_data->flags) {
        case 0:
            lazy_assert(0);
            break;
        case DB_DBT_REALLOC:
            dest_data->data = toku_realloc(dest_data->data, src_key->size);
            memcpy(dest_data->data, src_key->data, src_key->size);
            dest_data->size = src_key->size;
            break;
        default:
            lazy_assert(0);
        }

    return 0;
}

static DB_INDEXER *test_indexer = NULL;
static DB *test_hotdb = NULL;

static int
test_xid_state(DB_INDEXER *indexer, TXNID xid) {
    invariant(indexer == test_indexer);
    int r = txn_state(&live_xids, xid);
    return r;
}

static int 
test_lock_key(DB_INDEXER *indexer, TXNID xid, DB *hotdb, DBT *key) {
    invariant(indexer == test_indexer);
    invariant(hotdb == test_hotdb);
    invariant(test_xid_state(indexer, xid) == TOKUTXN_LIVE);
    printf("lock [%lu] ", xid);
    print_dbt(key);
    printf("\n");
    return 0;
}

static int 
test_delete_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    invariant(indexer == test_indexer);
    invariant(hotdb == test_hotdb);
    printf("delete_provisional ");
    print_xids(xids);
    print_dbt(hotkey);
    printf("\n");
    return 0;
}

static int
test_delete_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    invariant(indexer == test_indexer);
    invariant(hotdb == test_hotdb);
    printf("delete_committed ");
    print_xids(xids);
    print_dbt(hotkey);
    printf("\n");
    return 0;
}

static int 
test_insert_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids) {
    invariant(indexer == test_indexer);
    invariant(hotdb == test_hotdb);
    printf("insert_provisional ");
    print_xids(xids);
    print_dbt(hotkey);
    print_dbt(hotval);
    printf("\n");
    return 0;
}

static int 
test_insert_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids) {
    invariant(indexer == test_indexer);
    invariant(hotdb == test_hotdb);
    printf("insert_committed ");
    print_xids(xids);
    print_dbt(hotkey);
    print_dbt(hotval);
    printf("\n");
    return 0;
}

static int
test_commit_any(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    invariant(indexer == test_indexer);
    invariant(hotdb == test_hotdb);
    printf("commit_any ");
    print_xids(xids);
    print_dbt(hotkey);
    printf("\n");
    return 0;
}

static int
split_fields(char *line, char *fields[], int maxfields) {
    int i;
    for (i = 0; i < maxfields; i++, line = NULL) {
        fields[i] = strtok(line, " ");
        if (fields[i] == NULL) 
            break;
    }
    return i;
}

static int
read_line(char **line_ptr, size_t *len_ptr, FILE *f) {
    char *line = *line_ptr;
    size_t len = 0;
    bool in_comment = false;
    while (1) {
        int c = fgetc(f);
        if (c == EOF)
            break;
        else if (c == '\n') {
            in_comment = false;
            if (len > 0)
                break;
        } else {
            if (c == '#')
                in_comment = true;
            if (!in_comment) {
                line = toku_realloc(line, len+1);
                line[len++] = c;
            }
        }
    }
    if (len > 0) {
        line = toku_realloc(line, len+1);
        line[len] = '\0';
    }
    *line_ptr = line;
    *len_ptr = len;
    return len == 0 ? -1 : 0;
}

static int
read_test(char *testname, ULE ule) {
    int r = 0;
    FILE *f = fopen(testname, "r");
    if (f) {
        char *line = NULL;
        size_t len = 0;
        while (read_line(&line, &len, f) != -1) {
            // printf("%s", line);

            const int maxfields = 8;
            char *fields[maxfields];
            int nfields = split_fields(line, fields, maxfields);
            // for (int i = 0; i < nfields; i++); printf("%s ", fields[i]); printf("\n");

            if (nfields < 1)
                continue;
            // live xid...
            if (strcmp(fields[0], "live") == 0) {
                for (int i = 1; i < nfields; i++)
                    live_add(&live_xids, atoll(fields[i]), TOKUTXN_LIVE);
                continue;
            }
            // xid <XID> [live|committing|aborting]
            if (strcmp(fields[0], "xid") == 0 && nfields == 3) {
                TXNID xid = atoll(fields[1]);
                TOKUTXN_STATE state = TOKUTXN_NOT_LIVE;
                if (strcmp(fields[2], "live") == 0)
                    state = TOKUTXN_LIVE;
                else if (strcmp(fields[2], "committing") == 0)
                    state = TOKUTXN_COMMIT;
                else if (strcmp(fields[2], "aborting") == 0)
                    state = TOKUTXN_ABORT;
                else
                    assert(0);
                live_add(&live_xids, xid, state);
                continue;
            }
            // key KEY
            if (strcmp(fields[0], "key") == 0 && nfields == 2) {
                ule_set_key(ule, fields[1], strlen(fields[1]));
                continue;
            }
            // insert committed|provisional XID DATA
            if (strcmp(fields[0], "insert") == 0 && nfields == 4) {
                UXR_S uxr_s; 
                uxr_init(&uxr_s, XR_INSERT, fields[3], strlen(fields[3]), atoll(fields[2]));
                if (fields[1][0] == 'p')
                    ule_add_provisional(ule, &uxr_s);
                if (fields[1][0] == 'c')
                    ule_add_committed(ule, &uxr_s);
                continue;
            }
            // delete committed|provisional XID
            if (strcmp(fields[0], "delete") == 0 && nfields == 3) {
                UXR_S uxr_s; 
                uxr_init(&uxr_s, XR_DELETE, NULL, 0, atoll(fields[2]));
                if (fields[1][0] == 'p')
                    ule_add_provisional(ule, &uxr_s);
                if (fields[1][0] == 'c')
                    ule_add_committed(ule, &uxr_s);
                continue;
            }
            // placeholder XID
            if (strcmp(fields[0], "placeholder") == 0 && nfields == 2) {
                UXR_S uxr_s; 
                uxr_init(&uxr_s, XR_PLACEHOLDER, NULL, 0, atoll(fields[1]));
                ule_add_provisional(ule, &uxr_s);
                continue;
            }
            // placeholder provisional XID
            if (strcmp(fields[0], "placeholder") == 0 && nfields == 3 && fields[1][0] == 'p') {
                UXR_S uxr_s; 
                uxr_init(&uxr_s, XR_PLACEHOLDER, NULL, 0, atoll(fields[2]));
                ule_add_provisional(ule, &uxr_s);
                continue;
            }
            fprintf(stderr, "%s???\n", line);
            r = EINVAL;
        }
        toku_free(line);
        fclose(f);
    } else {
        r = errno;
        fprintf(stderr, "fopen %s errno=%d\n", testname, errno);
    }
    return r;
 }

static int
run_test(char *envdir, char *testname) {
    if (verbose)
        printf("%s\n", testname);

    live_init(&live_xids);

    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);
    r = env->set_redzone(env, 0); assert_zero(r);

    r = env->set_generate_row_callback_for_put(env, put_callback); assert_zero(r);

    r = env->open(env, envdir, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *src_db = NULL;
    r = db_create(&src_db, env, 0); assert_zero(r);
    r = src_db->open(src_db, NULL, "0.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *dest_db = NULL;
    r = db_create(&dest_db, env, 0); assert_zero(r);
    r = dest_db->open(dest_db, NULL, "1.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DB_INDEXER *indexer = NULL;
    r = env->create_indexer(env, txn, &indexer, src_db, 1, &dest_db, NULL, 0); assert_zero(r);

    // set test callbacks
    indexer->i->test_xid_state = test_xid_state;
    indexer->i->test_lock_key = test_lock_key;
    indexer->i->test_delete_provisional = test_delete_provisional;
    indexer->i->test_delete_committed = test_delete_committed;
    indexer->i->test_insert_provisional = test_insert_provisional;
    indexer->i->test_insert_committed = test_insert_committed;
    indexer->i->test_commit_any = test_commit_any;

    // verify indexer and hotdb in the callbacks
    test_indexer = indexer;
    test_hotdb = dest_db;

    // create a ule
    ULE ule = ule_create(); 
    ule_init(ule);

    // read the test
    r = read_test(testname, ule);
    if (r != 0)
        return r;

    r = indexer->i->undo_do(indexer, dest_db, ule); assert_zero(r);

    ule_free(ule);

    r = indexer->close(indexer); assert_zero(r);

    r = txn->abort(txn); assert_zero(r);

    r = src_db->close(src_db, 0); assert_zero(r);
    r = dest_db->close(dest_db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);

    live_destroy(&live_xids);
    
    return r;
}

int
test_main(int argc, char * const argv[]) {
    int r;

    // parse_args(argc, argv);
    int i;
    for (i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        
        break;
    }

    for (r = 0 ; r == 0 && i < argc; i++) {
        char *testname = argv[i];
        char envdir[strlen(ENVDIR) + 1 + 32 + 1];
        sprintf(envdir, "%s.%d", ENVDIR, toku_os_getpid());

        char syscmd[32 + strlen(envdir)];
        sprintf(syscmd, "rm -rf %s", envdir);
        r = system(syscmd); assert_zero(r);
        r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

        r = run_test(envdir, testname);
    }

    return r;
}

