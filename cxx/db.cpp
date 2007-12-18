#include <assert.h>
#include <db.h>
#include <errno.h>
#include "db_cxx.h"

Db::Db(DbEnv *env, u_int32_t flags)
    :      the_Env(env)
{
    the_db   = 0;

    if (the_Env == 0) {
	is_private_env = 1;
    }
    DB *tmp_db;
    int ret = db_create(&tmp_db, the_Env->get_DB_ENV(), flags & !(DB_CXX_NO_EXCEPTIONS));
    if (ret!=0) {
	assert(0); // make an error
    }
    the_db = tmp_db; 
    tmp_db->api_internal = this;
    if (is_private_env) {
	the_Env = new DbEnv(tmp_db->dbenv, flags & DB_CXX_NO_EXCEPTIONS);
    }
}

Db::~Db() {
    if (is_private_env) {
	delete the_Env; // The destructor closes the env.
    }
    if (!the_db) {
	close(0); // the user should have called close, but we do it here if not done.
    }
}

int Db::close (u_int32_t flags) {
    if (!the_db) {
	return EINVAL;
    }
    the_db->api_internal = 0;

    int ret = the_db->close(the_db, flags);

    the_db = 0;
    // Do we need to clean up "private environments"?
    // What about cursors?  They should be cleaned up already, but who did it?

    return ret;
}

int Db::open(DbTxn *txn, const char *filename, const char *subname, DBTYPE typ, u_int32_t flags, int mode) {
    int ret = the_db->open(the_db, txn->get_DB_TXN(), filename, subname, typ, flags, mode);
    return ret;
}

int Db::put(DbTxn *txn, Dbt *key, Dbt *data, u_int32_t flags) {
    int ret = the_db->put(the_db, txn->get_DB_TXN(), key->get_DBT(), data->get_DBT(), flags);
    return ret;
}

int Db::cursor(DbTxn *txn, Dbc **cursorp, u_int32_t flags) {
    int ret = the_db->cursor(the_db, txn->get_DB_TXN(), (DBC**)cursorp, flags);
    return ret;
}
int Db::set_pagesize(u_int32_t size) {
    int ret = the_db->set_pagesize(the_db, size);
    return ret;
}

