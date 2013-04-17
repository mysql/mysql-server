// measure the performance of a simulated "insert on duplicate key update" operation
// the table schema is t(a int, b int, c int, d int, primary key(a, b))
// a and b are random
// c is the sum of the observations
// d is the first observation

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include "db.h"

#if defined(BDB)
#define DB_YESOVERWRITE 0
#endif

static int verbose = 0;

static int get_int(void *p) {
    int v; 
    memcpy(&v, p, sizeof v);
    return htonl(v);
}

#if !defined(BDB)
static int my_update_callback(DB *db, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra) {
    if (old_val == NULL) {
        // new_val = extra
        set_val(extra, set_extra);
    } else {
        if (verbose) {
            printf("u"); fflush(stdout);
        }
        // new_val = old_val + extra
        assert(old_val->size == 8 && extra->size == 8);
        char new_val_buffer[8];
        memcpy(new_val_buffer, old_val->data, sizeof new_val_buffer);
        int newc = htonl(get_int(old_val->data) + get_int(extra->data)); // newc = oldc + newc
        memcpy(new_val_buffer, &newc, sizeof newc);
        DBT new_val = { .data = new_val_buffer, .size = sizeof new_val_buffer };
        set_val(&new_val, set_extra);
    }
    return 0;
}
#endif

static void insert_and_update(DB_ENV *db_env, DB *db, DB_TXN *txn, int a, int b, int c, int d, bool do_update_callback) {
    int r;

    // generate the key
    char key_buffer[8];
    int newa = htonl(a);
    memcpy(key_buffer, &newa, sizeof newa);
    int newb = htonl(b);
    memcpy(key_buffer+4, &newb, sizeof newb);

    // generate the value
    char val_buffer[8];
    int newc = htonl(c);
    memcpy(val_buffer, &newc, sizeof newc);
    int newd = htonl(d);
    memcpy(val_buffer+4, &newd, sizeof newd);

#if !defined(BDB)
    if (do_update_callback) {
        // extra = value_buffer, implicit combine column c update function
        DBT key = { .data = key_buffer, .size = sizeof key_buffer };
        DBT extra = { .data = val_buffer, .size = sizeof val_buffer };
        r = db->update(db, txn, &key, &extra, 0); assert(r == 0);
    } else
#endif
    {
        DBT key = { .data = key_buffer, .size = sizeof key_buffer };
        DBT value = { .data = val_buffer, .size = sizeof val_buffer };
        r = db->put(db, txn, &key, &value, DB_NOOVERWRITE);

        // if key exists then update and put overwrite
        if (r == DB_KEYEXIST) {
            if (verbose) {
                printf("k"); fflush(stdout);
            }

            DBT oldkey = { .data = key_buffer, .size = sizeof key_buffer };
            DBT oldvalue; memset(&oldvalue, 0, sizeof oldvalue);
            r = db->get(db, txn, &oldkey, &oldvalue, 0);
            assert(r == 0);

            // update it
            int oldc = get_int(oldvalue.data);
            newc = htonl(oldc + c); // newc = oldc + newc
            memcpy(val_buffer, &newc, sizeof newc);
            r = db->put(db, txn, &key, &value, DB_YESOVERWRITE);
        }
        assert(r == 0);
    }
}

static inline float tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

static void insert_and_update_all(DB_ENV *db_env, DB *db, long nrows, long max_rows_per_txn, int key_range, long rows_per_report, bool do_update_callback, bool do_txn) {
    int r;
    struct timeval tstart;
    r = gettimeofday(&tstart, NULL); assert(r == 0);
    struct timeval tlast = tstart;
    DB_TXN *txn = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &txn, 0); assert(r == 0);
    }
    long n_rows_per_txn = 0;
    long rowi;
    for (rowi = 0; rowi < nrows; rowi++) {
        int a = random() % key_range;
        int b = random() % key_range;
        int c = 1;
        int d = 0; // timestamp
        insert_and_update(db_env, db, txn, a, b, c, d, do_update_callback);
        n_rows_per_txn++;
        
        // maybe commit
        if (do_txn && n_rows_per_txn == max_rows_per_txn) {
            r = txn->commit(txn, 0); assert(r == 0);
            r = db_env->txn_begin(db_env, NULL, &txn, 0); assert(r == 0);
            n_rows_per_txn = 0;
        }

        // maybe report performance
        if (((rowi + 1) % rows_per_report) == 0) {
            struct timeval tnow;
            r = gettimeofday(&tnow, NULL); assert(r == 0);
            printf("%.3f %.0f/s %.0f/s\n", tdiff(&tnow, &tlast), rows_per_report/tdiff(&tnow, &tlast), rowi/tdiff(&tnow, &tstart)); fflush(stdout);
            tlast = tnow;
        }
    }

    if (do_txn) {
        r = txn->commit(txn, 0); assert(r == 0);
    }
    struct timeval tnow;
    r = gettimeofday(&tnow, NULL); assert(r == 0);
    printf("total %.3f %.0f/s\n", tdiff(&tnow, &tstart), nrows/tdiff(&tnow, &tstart)); fflush(stdout);
}

int main(int argc, char *argv[]) {
    char *db_env_dir = "update.env";
    int db_env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    char *db_filename = "update.db";
    long rows = 100000000;
    long rows_per_txn = 1000;
    long rows_per_report = 100000;
    int key_range = 100000;
    bool do_update_callback = false;
    bool do_txn = true;

    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "--rows") == 0 && i+1 < argc) {
            rows = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows_per_txn") == 0 && i+1 < argc) {
            rows_per_txn = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows_per_report") == 0 && i+1 < argc) {
            rows_per_report = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--key_range") == 0 && i+1 < argc) {
            key_range = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--txn") == 0 && i+1 < argc) {
            do_txn = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--update_callback") == 0) {
            do_update_callback = true;
            continue;
        }

        assert(0);
    }

    int r;
    char rm_cmd[strlen(db_env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", db_env_dir);
    r = system(rm_cmd); assert(r == 0);

    r = mkdir(db_env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert(r == 0);

    // create and open the env
    DB_ENV *db_env = NULL;
    r = db_env_create(&db_env, 0); assert(r == 0);
#if !defined(BDB)
    db_env->set_update(db_env, my_update_callback);
#endif
    if (!do_txn)
        db_env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    r = db_env->open(db_env, db_env_dir, db_env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);

    // create the db
    DB *db = NULL;
    r = db_create(&db, db_env, 0); assert(r == 0);
    DB_TXN *create_txn = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &create_txn, 0); assert(r == 0);
    }
    r = db->open(db, create_txn, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
    if (do_txn) {
        r = create_txn->commit(create_txn, 0); assert(r == 0);
    }

    // insert on duplicate key update
    insert_and_update_all(db_env, db, rows, rows_per_txn, key_range, rows_per_report, do_update_callback, do_txn);

    // shutdown
    r = db->close(db, 0); assert(r == 0); db = NULL;
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
