#include <db_cxx.h>

int Dbc::close (void) {
    DBC *dbc = this;
    int ret = dbc->c_close(dbc);
    DbEnv *env = (DbEnv*)dbc->dbp->api_internal;
    return env->maybe_throw_error(ret);
}

int Dbc::get(Dbt* key, Dbt *data, u_int32_t flags) {
    DBC *dbc = this;
    int ret = dbc->c_get(dbc, key, data, flags);
    DbEnv *env = (DbEnv*)dbc->dbp->api_internal;
    return env->maybe_throw_error(ret);
}
