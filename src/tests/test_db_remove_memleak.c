/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Does removing a database free the DB structure's memory? */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <db.h>
#include <string.h>

// DIR is defined in the Makefile

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
    r=env->open(env, DIR, DB_INIT_MPOOL|DB_PRIVATE|DB_CREATE, 0777); assert(r==0);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->open(db, NULL, "master.db", NULL, DB_BTREE, DB_CREATE, 0666); assert(r==0);
    data.size = sizeof("first.db");
    data.data = "first.db";
    db->put(db, NULL, &key, &data, 0);
    r=db->close(db, 0);         assert(r==0);

    r=db_create(&db, env, 0);   assert(r==0);
    r=db->remove(db, "master.db", NULL, 0); assert(r==0);

    r=env->close(env, 0);     assert(r==0);
    return 0;
}
