#include <db_cxx.h>

int Dbc::close (void) {
    DBC *dbc = this;
    DbEnv *env = (DbEnv*)dbc->dbp->api_internal; // Must grab the env before closing the cursor.
    int ret = dbc->c_close(dbc);
    return env->maybe_throw_error(ret);
}

int Dbc::get(Dbt* key, Dbt *data, u_int32_t flags) {
    DBC *dbc = this;
    int ret = dbc->c_get(dbc, key, data, flags);
    DB_ENV *dbenv_c=dbc->dbp->dbenv;
    DbEnv *env = (DbEnv*)dbenv_c->api1_internal;
    return env->maybe_throw_error(ret);
}

int Dbc::del(u_int32_t flags) {
    DBC *dbc = this;
    int ret = dbc->c_del(dbc, flags);
    DB_ENV *dbenv_c=dbc->dbp->dbenv;
    DbEnv *env = (DbEnv*)dbenv_c->api1_internal;
    return env->maybe_throw_error(ret);
}

// Not callable, but some compilers require it to be defined anyway.
Dbc::~Dbc()
{
}

