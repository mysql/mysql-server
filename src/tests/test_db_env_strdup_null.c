/* Do I return EINVAL when passing in NULL for something that would otherwise be strdup'd? */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <db.h>

#define DIR "dir.test_db_open_notexist_reopen"
DB_ENV *env;
DB *db;

int main (int argc, char *argv[]) {
    int r;
    r=system("rm -rf " DIR);                    assert(r==0);
    r=mkdir(DIR, 0777);                         assert(r==0);
    r=db_env_create(&env, 0);                   assert(r==0);
    r=env->set_data_dir(env, NULL);             assert(r==EINVAL);
    r=env->open(env, NULL, DB_PRIVATE, 0777);   assert(r==EINVAL);
    r=env->open(env, DIR, DB_PRIVATE, 0777);    assert(r==0);
    env->set_errpfx(env, NULL);                 assert(1); //Did not crash.
    r=env->set_tmp_dir(env, NULL);              assert(r==EINVAL);
    r=env->close(env, 0);                       assert(r==0);
    return 0;
}

