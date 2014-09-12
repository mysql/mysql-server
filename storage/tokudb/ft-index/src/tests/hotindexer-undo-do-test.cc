/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

// test the hotindexer undo do function
// read a description of the live transactions and a leafentry from a test file, run the undo do function,
// and print out the actions taken by the undo do function while processing the leafentry

#include "test.h"

#include <ft/tokuconst.h>
#include <ft/fttypes.h>
#include <ft/omt.h>
#include <ft/leafentry.h>
#include <ft/ule.h>
#include <ft/ule-internal.h>
#include <ft/le-cursor.h>
#include "indexer-internal.h"
#include <ft/xids-internal.h>

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

static TOKUTXN_STATE
lookup_txn_state(struct live *live, TXNID xid) {
    TOKUTXN_STATE r = TOKUTXN_RETIRED;
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
    ule->uxrs = ule->uxrs_static;
    return ule;
}

static void
ule_destroy(ULE ule) {
    for (unsigned int i = 0; i < ule->num_cuxrs + ule->num_puxrs; i++) {
        uxr_destroy(&ule->uxrs[i]);
    }
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
            printf("%" PRIu64, xids->ids[i]);
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
put_callback(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];
    (void) dest_db; (void) src_db; (void) dest_key; (void) dest_val; (void) src_key; (void) src_val;

    lazy_assert(src_db != NULL && dest_db != NULL);

    switch (dest_key->flags) {
    case 0:
        dest_key->data = src_val->data;
        dest_key->size = src_val->size;
        break;
    case DB_DBT_REALLOC:
        dest_key->data = toku_realloc(dest_key->data, src_val->size);
        memcpy(dest_key->data, src_val->data, src_val->size);
        dest_key->size = src_val->size;
        break;
    default:
        lazy_assert(0);
    }

    if (dest_val)
        switch (dest_val->flags) {
        case 0:
            lazy_assert(0);
            break;
        case DB_DBT_REALLOC:
            dest_val->data = toku_realloc(dest_val->data, src_key->size);
            memcpy(dest_val->data, src_key->data, src_key->size);
            dest_val->size = src_key->size;
            break;
        default:
            lazy_assert(0);
        }

    return 0;
}

static int
del_callback(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, const DBT *src_key, const DBT *src_data) {
    toku_dbt_array_resize(dest_keys, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    (void) dest_db; (void) src_db; (void) dest_key; (void) src_key; (void) src_data;

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
    return 0;
}


static DB_INDEXER *test_indexer = NULL;
static DB *test_hotdb = NULL;

static TOKUTXN_STATE
test_xid_state(DB_INDEXER *indexer, TXNID xid) {
    invariant(indexer == test_indexer);
    TOKUTXN_STATE r = lookup_txn_state(&live_xids, xid);
    return r;
}

static void 
test_lock_key(DB_INDEXER *indexer, TXNID xid, DB *hotdb, DBT *key) {
    invariant(indexer == test_indexer);
    invariant(hotdb == test_hotdb);
    TOKUTXN_STATE txn_state = test_xid_state(indexer, xid);
    invariant(txn_state == TOKUTXN_LIVE || txn_state == TOKUTXN_PREPARING);
    printf("lock [%" PRIu64 "] ", xid);
    print_dbt(key);
    printf("\n");
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
                XREALLOC_N(len+1, line);
                line[len++] = c;
            }
        }
    }
    if (len > 0) {
        XREALLOC_N(len+1, line);
        line[len] = '\0';
    }
    *line_ptr = line;
    *len_ptr = len;
    return len == 0 ? -1 : 0;
}

struct saved_lines_t {
    char** savedlines;
    uint32_t capacity;
    uint32_t used;
};

static void
save_line(char** line, saved_lines_t* saved) {
    if (saved->capacity == saved->used) {
        if (saved->capacity == 0) {
            saved->capacity = 1;
        }
        saved->capacity *= 2;
        XREALLOC_N(saved->capacity, saved->savedlines);
    }
    saved->savedlines[saved->used++] = *line;
    *line = nullptr;
}

static int
read_test(char *testname, ULE ule, DBT* key, saved_lines_t* saved) {
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
                TOKUTXN_STATE state = TOKUTXN_RETIRED;
                if (strcmp(fields[2], "live") == 0)
                    state = TOKUTXN_LIVE;
                else if (strcmp(fields[2], "preparing") == 0)
                    state = TOKUTXN_PREPARING;
                else if (strcmp(fields[2], "committing") == 0)
                    state = TOKUTXN_COMMITTING;
                else if (strcmp(fields[2], "aborting") == 0)
                    state = TOKUTXN_ABORTING;
                else
                    assert(0);
                live_add(&live_xids, xid, state);
                continue;
            }
            // key KEY
            if (strcmp(fields[0], "key") == 0 && nfields == 2) {
                save_line(&line, saved);
                dbt_init(key, fields[1], strlen(fields[1]));
                continue;
            }
            // insert committed|provisional XID DATA
            if (strcmp(fields[0], "insert") == 0 && nfields == 4) {
                save_line(&line, saved);
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
    r = env->set_generate_row_callback_for_del(env, del_callback); assert_zero(r);

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

    saved_lines_t saved;
    ZERO_STRUCT(saved);
    // read the test
    DBT key;
    ZERO_STRUCT(key);
    r = read_test(testname, ule, &key, &saved);
    if (r != 0)
        return r;

    r = indexer->i->undo_do(indexer, dest_db, &key, ule); assert_zero(r);

    ule_free(ule);
    key.data = NULL;

    for (uint32_t i = 0; i < saved.used; i++) {
        toku_free(saved.savedlines[i]);
    }
    toku_free(saved.savedlines);

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

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    assert_zero(r);
    for (r = 0 ; r == 0 && i < argc; i++) {
        char *testname = argv[i];
        char pid[10];
        sprintf(pid, "%d", toku_os_getpid());
        char envdir[TOKU_PATH_MAX+1];
        toku_path_join(envdir, 2, TOKU_TEST_FILENAME, pid);

        toku_os_recursive_delete(envdir);
        r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

        r = run_test(envdir, testname);
    }

    return r;
}

