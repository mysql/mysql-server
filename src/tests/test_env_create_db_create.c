#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>

int main (int argc, char *argv[]) {
    DB_ENV *env;
    DB *db;
    int r;
    r = db_env_create(&env, 0);
    assert(r == 0);
    r = db_create(&db, env, 0); 
    assert(r != 0);
    r = env->close(env, 0);       
    assert(r == 0);
    return 0;
}
