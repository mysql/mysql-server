/* Simple test of logging.  Can I start a TokuDB with logging enabled? */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <db.h>

// DIR is defined in the Makefile

DB_ENV *env;
DB *db;

int main (int argc, char *argv[]) {
    int r;
    system("rm -rf " DIR);
    r=mkdir(DIR, 0777);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, DIR, DB_PRIVATE|DB_CREATE, 0777); assert(r==0);
    r=db_create(&db, env, 0); assert(r==0);
    r=db->open(db, NULL, "doesnotexist.db", "testdb", DB_BTREE, 0, 0666); assert(r==ENOENT);
    r=db->open(db, NULL, "doesnotexist.db", "testdb", DB_BTREE, DB_CREATE, 0666); assert(r==0);
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
    return 0;
}
