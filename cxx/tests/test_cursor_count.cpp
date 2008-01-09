#include <stdlib.h>
#include <arpa/inet.h>
#include <assert.h>
#include <db_cxx.h>

#ifndef DB_YESOVERWRITE
#define BDB 1
#define DB_YESOVERWRITE 0
#else
#define TDB 1
typedef u_int32_t db_recno_t;
#endif

int keyeq(Dbt *a, Dbt *b) {
    if (a->get_size() != b->get_size()) return 0;
    return memcmp(a->get_data(), b->get_data(), a->get_size()) == 0;
}

void load(Db *db, int n) {
    printf("load\n");
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

db_recno_t my_cursor_count(Db *db, Dbc *cursor) {
    int r;
    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&key, &val, DB_CURRENT); assert(r == 0);
    
    Dbc *count_cursor;
    r = db->cursor(0, &count_cursor, 0); assert(r == 0);
    r = count_cursor->get(&key, &val, DB_SET); assert(r == 0);
    int nmatch;
    Dbt nkey, nval;
    for (nmatch=1; ; nmatch++) {
        nkey.set_flags(DB_DBT_REALLOC);
        nval.set_flags(DB_DBT_REALLOC);
        r = count_cursor->get(&nkey, &nval, DB_NEXT);
        if (r != 0) break;
        if (!keyeq(&key, &nkey)) break;
    }
    if (nkey.get_data()) free(nkey.get_data());
    if (nval.get_data()) free(nval.get_data());
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());
    r = count_cursor->close(); assert(r == 0);
    return nmatch;
}

void walk(Db *db, int n) {
    printf("walk\n");
    Dbc *cursor;
    int r = db->cursor(0, &cursor, 0); assert(r == 0);
    int i;
    Dbt key, val;
    for (i=0; ; i++) {
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
        db_recno_t nmatch;
#if BDB
        r = cursor->count(&nmatch, 0);
        printf("%d %d %d\n", k, v, nmatch);
        assert(my_cursor_count(db, cursor) == nmatch);
#else
        nmatch = my_cursor_count(db, cursor);
        printf("%d %d %d\n", k, v, nmatch);
        if (k == n/2) assert(nmatch == (db_recno_t) n); else assert(nmatch == 1);
#endif      
    }
    free(key.get_data());
    free(val.get_data());
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
    printf("test_next_nodup\n");
    int r;
    Dbc *cursor;
    r = db->cursor(0, &cursor, 0); assert(r == 0);
    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    r = cursor->get(&key, &val, DB_FIRST); assert(r == 0);
    printf("%d %d\n", htonl(*(int*)key.get_data()), *(int*)val.get_data());
    for (;;) {
        r = my_next_nodup(cursor, &key, &val);
        if (r != 0) break;
        printf("%d %d\n", htonl(*(int*)key.get_data()), *(int*)val.get_data());
    }
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());
    r = cursor->close(); assert(r == 0);
}

int main() {
    int r;

    Db db(0, DB_CXX_NO_EXCEPTIONS);
    r = db.set_flags(DB_DUP + DB_DUPSORT); assert(r == 0);
    r = db.open(0, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    
    load(&db, 10);
    walk(&db, 10);
    test_next_nodup(&db, 10);

    return 0;
}
