#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <db.h>
#include "test.h"

void test_abort_create(void) {

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    int r;
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_data_dir(env, DIR);
    r = env->set_lg_dir(env, DIR);
    env->set_errfile(env, stdout);
    r = env->open(env, 0, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, 0777); 
    if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
    assert(r == 0);

    DB_TXN *txn = 0;
    r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);

    {
	struct stat statbuf;
	r = stat(DIR "/test.db", &statbuf);
	assert(r==0);
    }

    r = txn->abort(txn); assert(r == 0);
    r = db->close(db, 0);

    r = env->close(env, 0); assert(r == 0);

    {
	struct stat statbuf;
	r = stat(DIR "/test.db", &statbuf);
	assert(r!=0);
    }


}

int main(int argc, char *argv[]) {
    test_abort_create();
    return 0;
}
