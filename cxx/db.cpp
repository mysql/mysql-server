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

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <db_cxx.h>

#define do_maybe_error(errno) 

Db::Db(DbEnv *env, u_int32_t flags)
    :      the_Env(env)
{
    assert(env); // modern versions of TokuDB require an env.
    the_db   = 0;

    is_private_env = (the_Env == 0);

    DB *tmp_db;
    int ret = db_create(&tmp_db, the_Env->get_DB_ENV(), flags & ~(DB_CXX_NO_EXCEPTIONS));
    if (ret!=0) {
	the_Env->maybe_throw_error(ret);
	// Otherwise cannot do much
	return;
    }
    the_db = tmp_db; 
    tmp_db->api_internal = this;
    if (is_private_env) {
	the_Env = new DbEnv(tmp_db->dbenv, flags & DB_CXX_NO_EXCEPTIONS);
    }
}

Db::~Db() {
    if (the_db) {
	close(0); // the user should have called close, but we do it here if not done.
	assert(the_db==0);
    }
    if (is_private_env && the_Env) {
	the_Env->close(0);
	delete the_Env;
    }
}

int Db::set_flags(u_int32_t flags) {
    int ret = the_db->set_flags(the_db, flags);
    return the_Env->maybe_throw_error(ret);
}

int Db::get_flags(u_int32_t *flags) {
    int ret = the_db->get_flags(the_db, flags);
    return the_Env->maybe_throw_error(ret);
}

int Db::close (u_int32_t flags) {
    if (!the_db) {
	return the_Env->maybe_throw_error(EINVAL);
    }
    the_db->api_internal = 0;


    int ret = the_db->close(the_db, flags);

    the_db = 0;

    int no_exceptions = the_Env->do_no_exceptions; // Get this out before possibly deleting the env
    
    if (is_private_env) {
	// The env was closed by the_db->close, we have to tell the DbEnv that the DB_ENV is gone, and delete it.
	the_Env->the_env = 0;
	delete the_Env;
	the_Env=0;
    }

    // Do we need to clean up "private environments"?
    // What about cursors?  They should be cleaned up already, but who did it?

    // This maybe_throw must be the static one because the env is gone.
    return DbEnv::maybe_throw_error(ret, NULL, no_exceptions);
}

int Db::open(DbTxn *txn, const char *filename, const char *subname, DBTYPE typ, u_int32_t flags, int mode) {
    int ret = the_db->open(the_db, txn->get_DB_TXN(), filename, subname, typ, flags, mode);
    return the_Env->maybe_throw_error(ret);
}

int Db::del(DbTxn *txn, Dbt *key, u_int32_t flags) {
    int ret = the_db->del(the_db, txn->get_DB_TXN(), key->get_DBT(), flags);
    return the_Env->maybe_throw_error(ret);
}

int Db::get(DbTxn *txn, Dbt *key, Dbt *data, u_int32_t flags) {
    int ret = the_db->get(the_db, txn->get_DB_TXN(), key->get_DBT(), data->get_DBT(), flags);
    return the_Env->maybe_throw_error(ret);
}

int Db::put(DbTxn *txn, Dbt *key, Dbt *data, u_int32_t flags) {
    int ret = the_db->put(the_db, txn->get_DB_TXN(), key->get_DBT(), data->get_DBT(), flags);
    return the_Env->maybe_throw_error(ret);
}

int Db::cursor(DbTxn *txn, Dbc **cursorp, u_int32_t flags) {
    int ret = the_db->cursor(the_db, txn->get_DB_TXN(), (DBC**)cursorp, flags);
    return the_Env->maybe_throw_error(ret);
}

int Db::set_pagesize(u_int32_t size) {
    int ret = the_db->set_pagesize(the_db, size);
    return the_Env->maybe_throw_error(ret);
}

int Db::remove(const char *file, const char *database, u_int32_t flags) {
    int ret = the_db->remove(the_db, file, database, flags);
    the_db = 0;
    return the_Env->maybe_throw_error(ret);
}

#if 0
extern "C" int toku_bt_compare_callback_c(DB *db_c, const DBT *a, const DBT *b) {
    Db *db_cxx=Db::get_Db(db_c);
    return db_cxx->do_bt_compare_callback_cxx(db_cxx, Dbt::get_const_Dbt(a), Dbt::get_const_Dbt(b));
}

int Db::do_bt_compare_callback_cxx(Db *db, const Dbt *a, const Dbt *b) {
    return the_Env->bt_compare_callback_cxx(db, a, b);
}

int Db::set_bt_compare(int (*bt_compare_callback)(Db *, const Dbt *, const Dbt *)) {
    bt_compare_callback_cxx = bt_compare_callback;
    int ret = the_db->set_bt_compare(the_db, toku_bt_compare_callback_c);
    return the_Env->maybe_throw_error(ret);
}

int Db::set_bt_compare(bt_compare_fcn_type bt_compare_fcn) {
    int ret = the_db->set_bt_compare(the_db, bt_compare_fcn);
    return the_Env->maybe_throw_error(ret);
}
#endif

int Db::fd(int *fdp) {
    int ret = the_db->fd(the_db, fdp);
    return the_Env->maybe_throw_error(ret);
}

extern "C" int toku_dup_compare_callback_c(DB *db_c, const DBT *a, const DBT *b) {
    Db *db_cxx=Db::get_Db(db_c);
    return db_cxx->dup_compare_callback_cxx(db_cxx, Dbt::get_const_Dbt(a), Dbt::get_const_Dbt(b));
}

void Db::set_errpfx(const char *errpfx) {
    the_Env->set_errpfx(errpfx);
}

void Db::set_error_stream(std::ostream *new_error_stream) {
    the_Env->set_error_stream(new_error_stream);
}
