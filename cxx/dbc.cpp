/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

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

#if 0
// No longer present (see #4576)
int Dbc::del(u_int32_t flags) {
    DBC *dbc = this;
    int ret = dbc->c_del(dbc, flags);
    DB_ENV *dbenv_c=dbc->dbp->dbenv;
    DbEnv *env = (DbEnv*)dbenv_c->api1_internal;
    return env->maybe_throw_error(ret);
}
#endif

int Dbc::count(db_recno_t *count, u_int32_t flags) {
    DBC *dbc = this;
    int ret = dbc->c_count(dbc, count, flags);
    DB_ENV *dbenv_c=dbc->dbp->dbenv;
    DbEnv *env = (DbEnv*)dbenv_c->api1_internal;
    return env->maybe_throw_error(ret);
}

// Not callable, but some compilers require it to be defined anyway.
Dbc::~Dbc()
{
}

