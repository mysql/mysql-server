#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <db_cxx.h>

int verbose;

#ifndef DB_YESOVERWRITE
#define BDB 1
#define DB_YESOVERWRITE 0
#else
#define TDB 1
#endif

int keyeq(Dbt *a, Dbt *b) {
    if (a->get_size() != b->get_size()) return 0;
    return memcmp(a->get_data(), b->get_data(), a->get_size()) == 0;
}

void load(Db *db, int n) {
    if (verbose) printf("load\n");
    int i;
    for (i=0; i<n; i++) {
        if (i == n/2) continue;
        int k = htonl(i);
        Dbt key(&k, sizeof k);
        int v = i;
        Dbt val(&v, sizeof v);
        int r = db->put(0, &key, &val, DB_YESOVERWRITE); assert(r == 0);
    }

    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = i;
        Dbt key(&k, sizeof k);
        Dbt val(&v, sizeof v);
        int r = db->put(0, &key, &val, DB_YESOVERWRITE); assert(r == 0);
    }
}

int my_cursor_count(Dbc *cursor, db_recno_t *count, Db *db) {
    int r;
    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&key, &val, DB_CURRENT); assert(r == 0);
    
    Dbc *count_cursor;
    r = db->cursor(0, &count_cursor, 0); assert(r == 0);
    r = count_cursor->get(&key, &val, DB_SET); assert(r == 0);
    *count = 0;

    Dbt nkey, nval;
    for (;; ) {
        *count += 1;
        nkey.set_flags(DB_DBT_REALLOC);
        nval.set_flags(DB_DBT_REALLOC);
        r = count_cursor->get(&nkey, &nval, DB_NEXT);
        if (r != 0) break;
        if (!keyeq(&key, &nkey)) break;
    }
    r = 0;
    if (nkey.get_data()) free(nkey.get_data());
    if (nval.get_data()) free(nval.get_data());
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());
    int rr = count_cursor->close(); assert(rr == 0);
    return r;
}

void walk(Db *db, int n) {
    if (verbose) printf("walk\n");
    Dbc *cursor;
    int r = db->cursor(0, &cursor, 0); assert(r == 0);
    Dbt key, val;
    int i;
    for (i=0;;i++) {
        key.set_flags(DB_DBT_REALLOC);
        val.set_flags(DB_DBT_REALLOC);
        r = cursor->get(&key, &val, DB_NEXT);
        if (r != 0) break;
        int k;
        assert(key.get_size() == sizeof k);
        memcpy(&k, key.get_data(), key.get_size());
        k = htonl(k);
        int v;
        assert(val.get_size() == sizeof v);
        memcpy(&v, val.get_data(), val.get_size());
        db_recno_t count;
        r = cursor->count(&count, 0); assert(r == 0);
        if (verbose) printf("%d %d %d\n", k, v, count);
        db_recno_t mycount;
        r = my_cursor_count(cursor, &mycount, db); assert(r == 0);
        assert(mycount == count);
        if (k == n/2) assert((int)count == n); else assert(count == 1);
    }
    assert(i == 2*n-1);
    free(key.get_data());
    free(val.get_data());
    r = cursor->close(); assert(r == 0);
}

int cursor_set(Dbc *cursor, int k) {
    Dbt key(&k, sizeof k);
    Dbt val;
    int r = cursor->get(&key, &val, DB_SET);
    return r;
}

void test_zero_count(Db *db, int n) {
    if (verbose) printf("test_zero_count\n");
    Dbc *cursor;
    int r = db->cursor(0, &cursor, 0); assert(r == 0);

    r = cursor_set(cursor, htonl(n/2)); assert(r == 0);
    db_recno_t count;
    r = cursor->count(&count, 0); assert(r == 0);
    assert((int)count == n);

    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    int i;
    for (i=1; count > 0; i++) {
        r = cursor->del(0); assert(r == 0);
        db_recno_t newcount;
        r = cursor->count(&newcount, 0);
        if (r != 0) 
            break;
        assert(newcount == count - 1);
        count = newcount;
        r = cursor->get(&key, &val, DB_NEXT_DUP); 
        if (r != 0) break;
    }
    assert(i == n);
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());
    r = cursor->close(); assert(r == 0);
}

int my_next_nodup(Dbc *cursor, Dbt *key, Dbt *val) {
    int r;
    Dbt currentkey; currentkey.set_flags(DB_DBT_REALLOC);
    Dbt currentval; currentval.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&currentkey, &currentval, DB_CURRENT); assert(r == 0);
    Dbt nkey; nkey.set_flags(DB_DBT_REALLOC);
    Dbt nval; nval.set_flags(DB_DBT_REALLOC);
    for (;;) {
        r = cursor->get(&nkey, &nval, DB_NEXT);
        if (r != 0) break;
        if (!keyeq(&currentkey, &nkey)) break;
    }
    if (nkey.get_data()) free(nkey.get_data());
    if (nval.get_data()) free(nval.get_data());
    if (currentkey.get_data()) free(currentkey.get_data());
    if (currentval.get_data()) free(currentval.get_data());
    if (r == 0) r = cursor->get(key, val, DB_CURRENT);
    return r;
}

int my_prev_nodup(Dbc *cursor, Dbt *key, Dbt *val) {
    int r;
    Dbt currentkey; currentkey.set_flags(DB_DBT_REALLOC);
    Dbt currentval; currentval.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&currentkey, &currentval, DB_CURRENT); assert(r == 0);
    Dbt nkey; nkey.set_flags(DB_DBT_REALLOC);
    Dbt nval; nval.set_flags(DB_DBT_REALLOC);
    for (;;) {
        r = cursor->get(&nkey, &nval, DB_PREV);
        if (r != 0) break;
        if (!keyeq(&currentkey, &nkey)) break;
    }
    if (nkey.get_data()) free(nkey.get_data());
    if (nval.get_data()) free(nval.get_data());
    if (currentkey.get_data()) free(currentkey.get_data());
    if (currentval.get_data()) free(currentval.get_data());
    if (r == 0) r = cursor->get(key, val, DB_CURRENT);
    return r;
}

void test_next_nodup(Db *db, int n) {
    if (verbose) printf("test_next_nodup\n");
    int r;
    Dbc *cursor;
    r = db->cursor(0, &cursor, 0); assert(r == 0);
    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&key, &val, DB_FIRST); assert(r == 0);
    int i = 0;
    while (r == 0) {
        int k = htonl(*(int*)key.get_data());
        int v = *(int*)val.get_data();
        if (verbose) printf("%d %d\n", k, v);
        assert(k == i);
        if (k != n/2) assert(v == i); else assert(v == 0);
        i += 1;
        r = my_next_nodup(cursor, &key, &val);
    }
    assert(i == n);
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());
    r = cursor->close(); assert(r == 0);
}

int my_next_dup(Dbc *cursor, Dbt *key, Dbt *val) {
    int r;
    Dbt currentkey; currentkey.set_flags(DB_DBT_REALLOC);
    Dbt currentval; currentval.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&currentkey, &currentval, DB_CURRENT); assert(r == 0);
    Dbt nkey; nkey.set_flags(DB_DBT_REALLOC);
    Dbt nval; nval.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&nkey, &nval, DB_NEXT);
    if (r == 0 && !keyeq(&currentkey, &nkey)) r = DB_NOTFOUND;
    if (nkey.get_data()) free(nkey.get_data());
    if (nval.get_data()) free(nval.get_data());
    if (currentkey.get_data()) free(currentkey.get_data());
    if (currentval.get_data()) free(currentval.get_data());
    if (r == 0) r = cursor->get(key, val, DB_CURRENT);
    return r;
}

int my_prev_dup(Dbc *cursor, Dbt *key, Dbt *val) {
    int r;
    Dbt currentkey; currentkey.set_flags(DB_DBT_REALLOC);
    Dbt currentval; currentval.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&currentkey, &currentval, DB_CURRENT); assert(r == 0);
    Dbt nkey; nkey.set_flags(DB_DBT_REALLOC);
    Dbt nval; nval.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&nkey, &nval, DB_PREV);
    if (r == 0 && !keyeq(&currentkey, &nkey)) r = DB_NOTFOUND;
    if (nkey.get_data()) free(nkey.get_data());
    if (nval.get_data()) free(nval.get_data());
    if (currentkey.get_data()) free(currentkey.get_data());
    if (currentval.get_data()) free(currentval.get_data());
    if (r == 0) r = cursor->get(key, val, DB_CURRENT);
    return r;
}

void test_next_dup(Db *db, int n) {
    if (verbose) printf("test_next_dup\n");
    int r;
    Dbc *cursor;
    r = db->cursor(0, &cursor, 0); assert(r == 0);
    int k = htonl(n/2);
    Dbt setkey(&k, sizeof k);
    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&setkey, &val, DB_SET); assert(r == 0);
    r = cursor->get(&key, &val, DB_CURRENT); assert(r == 0);
    int i = 0;
    while (r == 0) {
        int k = htonl(*(int*)key.get_data());
        int v = *(int*)val.get_data();
        if (verbose) printf("%d %d\n", k, v);
        assert(k == n/2); assert(v == i);
        i += 1;
        r = my_next_dup(cursor, &key, &val);
    }
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());
    r = cursor->close(); assert(r == 0);
}

int main(int argc, char *argv[]) {
    for (int i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose = 1;
    }

    int r;

    Db db(0, DB_CXX_NO_EXCEPTIONS);
    r = db.set_flags(DB_DUP + DB_DUPSORT); assert(r == 0);
    unlink("test.db");
    r = db.open(0, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    
    load(&db, 10);
    walk(&db, 10);
    test_next_nodup(&db, 10);
    test_next_dup(&db, 10);
    test_zero_count(&db, 10);

    return 0;
}
