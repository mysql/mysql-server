/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Does removing subdatabases corrupt the db file/other dbs in that file? (when nothing else open) */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <db.h>
#include <string.h>

// DIR is defined in the Makefile
#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

DB_ENV *env;
DB *db;
DBT key;
DBT data;

int main (int argc, char *argv[]) {
    int r;
    system("rm -rf " DIR);
    r=mkdir(DIR, 0777);         assert(r==0);
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.size = sizeof("name");
    key.data = "name";
    
    r=db_env_create(&env, 0);   assert(r==0);
    // Note: without DB_INIT_MPOOL the BDB library will fail on db->open().
    r=env->open(env, DIR, DB_INIT_MPOOL|DB_PRIVATE|DB_CREATE, 0777); assert(r==0);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->remove(db, "DoesNotExist.db", NULL, 0);       assert(r==ENOENT);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->remove(db, "DoesNotExist.db", "SubDb", 0);    assert(r==ENOENT);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->open(db, NULL, "master.db", "first", DB_BTREE, DB_CREATE, 0666); CKERR(r);
    data.size = sizeof("first.db");
    data.data = "first.db";
    db->put(db, NULL, &key, &data, 0);
    r=db->close(db, 0);         assert(r==0);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->remove(db, "master.db", "second", 0); assert(r==ENOENT);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->open(db, NULL, "master.db", "second", DB_BTREE, DB_CREATE, 0666); assert(r==0);
    key.size = sizeof("name");
    key.data = "name";
    data.size = sizeof("second.db");
    data.data = "second.db";
    db->put(db, NULL, &key, &data, 0);
    r=db->close(db, 0);         assert(r==0);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->open(db, NULL, "master.db", "third", DB_BTREE, DB_CREATE, 0666); assert(r==0);
    key.size = sizeof("name");
    key.data = "name";
    data.size = sizeof("third.db");
    data.data = "third.db";
    db->put(db, NULL, &key, &data, 0);
    r=db->close(db, 0);         assert(r==0);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->remove(db, "master.db", "second", 0); assert(r==0);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->remove(db, "master.db", "second", 0); assert(r==ENOENT);

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.size = sizeof("name");
    key.data = "name";

    //Verify data still exists in first/third
    r=db_create(&db, env, 0);   assert(r==0);
    r=db->open(db, NULL, "master.db", "first", DB_BTREE, 0, 0666); assert(r==0);
    r=db->get(db, NULL, &key, &data, 0);    assert(r==0);
    assert(!strcmp(data.data, "first.db"));
    r=db->close(db, 0);         assert(r==0);
    
    r=db_create(&db, env, 0);   assert(r==0);
    r=db->open(db, NULL, "master.db", "third", DB_BTREE, 0, 0666); assert(r==0);
    r=db->get(db, NULL, &key, &data, 0);    assert(r==0);
    assert(!strcmp(data.data, "third.db"));
    r=db->close(db, 0);         assert(r==0);
    
    //Verify second is gone.
    r=db_create(&db, env, 0);   assert(r==0);
    r=db->open(db, NULL, "master.db", "second", DB_BTREE, 0, 0666); assert(r==ENOENT);
    //Create again, verify it does not have its old data.
    r=db->open(db, NULL, "master.db", "second", DB_BTREE, DB_CREATE, 0666); assert(r==0);
    r=db->get(db, NULL, &key, &data, 0);    assert(r==DB_NOTFOUND);
    
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
    return 0;
}
