/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <stdarg.h>
#include <errno.h>
#include <toku_assert.h>
#include "../ft/fttypes.h"
#include <db_cxx.h>

DbEnv::DbEnv (u_int32_t flags)
    : do_no_exceptions((flags&DB_CXX_NO_EXCEPTIONS)!=0),
      errcall(NULL)
{
    int ret = db_env_create(&the_env, flags & ~DB_CXX_NO_EXCEPTIONS);
    assert(ret==0); // should do an error.
    the_env->api1_internal = this;
}

DbEnv::DbEnv(DB_ENV *env, u_int32_t flags)
    : do_no_exceptions((flags&DB_CXX_NO_EXCEPTIONS)!=0), _error_stream(0)
{
    the_env = env;
    if (env == 0) {
	DB_ENV *new_env;
	int ret = db_env_create(&new_env, flags & ~DB_CXX_NO_EXCEPTIONS);
	assert(ret==0); // should do an error.
	the_env = new_env;
    }
    the_env->api1_internal = this;
}

// If still open, close it.  In most cases, the caller should call close explicitly so that they can catch the exceptions.
DbEnv::~DbEnv(void)
{
    if (the_env!=NULL) {
	(void)the_env->close(the_env, 0);
	the_env = 0;
    }
}

int DbEnv::close(u_int32_t flags) {
    int ret = EINVAL;
    if (the_env)
        ret = the_env->close(the_env, flags);
    the_env = 0; /* get rid of the env ref, so we don't touch it (even if we failed, or when the destructor is called) */
    return maybe_throw_error(ret);
}

int DbEnv::open(const char *home, u_int32_t flags, int mode) {
    int ret = the_env->open(the_env, home, flags, mode);
    return maybe_throw_error(ret);
}

int DbEnv::set_cachesize(u_int32_t gbytes, u_int32_t bytes, int ncache) {
    int ret = the_env->set_cachesize(the_env, gbytes, bytes, ncache);
    return maybe_throw_error(ret);
}

int DbEnv::set_redzone(u_int32_t percent) {
    int ret = the_env->set_redzone(the_env, percent);
    return maybe_throw_error(ret);
}

int DbEnv::set_flags(u_int32_t flags, int onoff) {
    int ret = the_env->set_flags(the_env, flags, onoff);
    return maybe_throw_error(ret);
}

#if DB_VERSION_MAJOR<4 || (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=4)
int DbEnv::set_lk_max(u_int32_t flags) {
    int ret = the_env->set_lk_max(the_env, flags);
    return maybe_throw_error(ret);
}
#endif

int DbEnv::txn_begin(DbTxn *parenttxn, DbTxn **txnp, u_int32_t flags) {
    DB_TXN *txn;
    int ret = the_env->txn_begin(the_env, parenttxn->get_DB_TXN(), &txn, flags);
    if (ret==0) {
	*txnp = new DbTxn(txn);
    }
    return maybe_throw_error(ret);
}

int DbEnv::set_default_bt_compare(bt_compare_fcn_type bt_compare_fcn) {
    int ret = the_env->set_default_bt_compare(the_env, bt_compare_fcn);
    return maybe_throw_error(ret);
}

int DbEnv::set_data_dir(const char *dir) {
    int ret = the_env->set_data_dir(the_env, dir);
    return maybe_throw_error(ret);
}

void DbEnv::set_errpfx(const char *errpfx) {
    the_env->set_errpfx(the_env, errpfx);
}

int DbEnv::maybe_throw_error(int err, DbEnv *env, int no_exceptions) throw (DbException) {
    if (err==0 || err==DB_NOTFOUND || err==DB_KEYEXIST) return err;
    if (no_exceptions) return err;
    if (err==DB_LOCK_DEADLOCK) {
	DbDeadlockException e(env);
	throw e;
    } else {
	DbException e(err);
	e.set_env(env);
	throw e;
    }
}

int DbEnv::maybe_throw_error(int err) throw (DbException) {
    return maybe_throw_error(err, this, do_no_exceptions);
}

extern "C" void toku_ydb_error_all_cases(const DB_ENV * env,
                                         int error,
                                         BOOL include_stderrstring,
                                         BOOL use_stderr_if_nothing_else,
                                         const char *fmt, va_list ap);

void DbEnv::err(int error, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    toku_ydb_error_all_cases(the_env, error, TRUE, TRUE, fmt, ap);
    va_end(ap);
}

void DbEnv::set_errfile(FILE *errfile) {
    the_env->set_errfile(the_env, errfile);
}

int DbEnv::get_flags(u_int32_t *flagsp) {
    int ret = the_env->get_flags(the_env, flagsp);
    return maybe_throw_error(ret);
}

extern "C" void toku_db_env_errcall_c(const DB_ENV *dbenv_c, const char *errpfx, const char *msg) {
    DbEnv *dbenv = (DbEnv *) dbenv_c->api1_internal;
    dbenv->errcall(dbenv, errpfx, msg);
}

void DbEnv::set_errcall(void (*db_errcall_fcn)(const DbEnv *, const char *, const char *)) {
    errcall = db_errcall_fcn;
    the_env->set_errcall(the_env, toku_db_env_errcall_c);
}

extern "C" void toku_db_env_error_stream_c(const DB_ENV *dbenv_c, const char *errpfx, const char *msg) {
    DbEnv *dbenv = (DbEnv *) dbenv_c->api1_internal;
    if (dbenv->_error_stream) {
        if (errpfx) *(dbenv->_error_stream) << errpfx;
        if (msg) *(dbenv->_error_stream) << ":" << msg << "\n";
    }
}

void DbEnv::set_error_stream(std::ostream *new_error_stream) {
    _error_stream = new_error_stream;
    the_env->set_errcall(the_env, toku_db_env_error_stream_c);
}

int DbEnv::set_lk_max_locks(u_int32_t max_locks) {
    int ret = the_env->set_lk_max_locks(the_env, max_locks);
    return maybe_throw_error(ret);
}

int DbEnv::get_lk_max_locks(u_int32_t *max_locks) {
    int ret = the_env->get_lk_max_locks(the_env, max_locks);
    return maybe_throw_error(ret);
}

