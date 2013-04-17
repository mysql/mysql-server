/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

static void
test_abort_create (void) {

    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    env->set_errfile(env, stdout);
    r = env->open(env, ENVDIR, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
    assert(r == 0);

    DB_TXN *txn = 0;
    r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    {
        char *filename;
#if USE_TDB
        {
            DBT dname;
            DBT iname;
            dbt_init(&dname, "test.db", sizeof("test.db"));
            dbt_init(&iname, NULL, 0);
            iname.flags |= DB_DBT_MALLOC;
            r = env->get_iname(env, &dname, &iname);
            CKERR(r);
            filename = iname.data;
            assert(filename);
        }
#else
        filename = toku_xstrdup("test.db");
#endif
	toku_struct_stat statbuf;
        char fullfile[strlen(filename) + sizeof(ENVDIR "/")];
        snprintf(fullfile, sizeof(fullfile), ENVDIR "/%s", filename);
        toku_free(filename);
	r = toku_stat(fullfile, &statbuf);
	assert(r==0);
    }

    r = db->close(db, 0);
    r = txn->abort(txn); assert(r == 0);

    {
#if USE_TDB
        {
            DBT dname;
            DBT iname;
            dbt_init(&dname, "test.db", sizeof("test.db"));
            dbt_init(&iname, NULL, 0);
            iname.flags |= DB_DBT_MALLOC;
            r = env->get_iname(env, &dname, &iname);
            CKERR2(r, DB_NOTFOUND);
        }
#endif
        toku_struct_stat statbuf;
        r = toku_stat(ENVDIR "/test.db", &statbuf);
        assert(r!=0);
        assert(errno==ENOENT);
    }

    r = env->close(env, 0); assert(r == 0);



}

int
test_main(int UU(argc), char UU(*const argv[])) {
    test_abort_create();
    return 0;
}
