/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ifndef _DB_CXX_H_
#define _DB_CXX_H_

#include <db.h>
#include <iostream>
#include <exception>
#include <string.h>
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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

class Dbt;
class DbEnv;
class DbTxn;
class Dbc;
class DbException;

class DbException : public std::exception {
    friend class DbEnv;
 public:
    ~DbException() throw();
    DbException(int err);
    int get_errno() const;
    const char *what() const throw();
    DbEnv *get_env() const;
    void set_env(DbEnv *);
    DbException(const DbException &);
    DbException &operator = (const DbException &);
 private:
    char *the_what;
    int   the_err;
    DbEnv *the_env;
    void FillTheWhat(void);
};

class DbDeadlockException : public DbException {
 public:
    DbDeadlockException(DbEnv*);
};

class DbLockNotGrantedException {
};

class DbMemoryException {
};

class DbRunRecoveryException {
};

// DBT and Dbt objects are the same pointers.  So watch out if you use Dbt to make other classes (e.g., with subclassing).
class Dbt : private DBT {
    friend class Dbc;
 public:

    void *    get_data(void) const     { return data; }
    void      set_data(void *p)        { data = p; }
    
    uint32_t get_size(void) const     { return size; }
    void      set_size(uint32_t p)    { size =  p; }
		       
    uint32_t get_flags() const        { return flags; }
    void      set_flags(uint32_t f)   { flags = f; }

    uint32_t get_ulen() const         { return ulen; }
    void      set_ulen(uint32_t p)    { ulen = p; }

    DBT *get_DBT(void)                              { return (DBT*)this; }
    const DBT *get_const_DBT(void) const            { return (const DBT*)this; }

    static Dbt* get_Dbt(DBT *dbt)                   { return (Dbt *)dbt; }
    static const Dbt* get_const_Dbt(const DBT *dbt) { return (const Dbt *)dbt; }

    Dbt(void */*data*/, uint32_t /*size*/);
    Dbt(void);
    ~Dbt();

 private:
    // Nothing here.
};

extern "C" {
    typedef int (*bt_compare_fcn_type)(DB *db, const DBT *dbt1, const DBT *dbt2);
    typedef int (*dup_compare_fcn_type)(DB *db, const DBT *dbt1, const DBT *dbt2);
};

class Db {
 public:
    /* Functions to make C++ work, defined in the BDB C++ API documents */
    Db(DbEnv *dbenv, uint32_t flags);
    ~Db();

    DB *get_DB(void) {
	return the_db;
    }
    const DB *get_const_DB() const {
	return the_db;
    }
    static Db *get_Db(DB *db) {
	return (Db*)db->api_internal;
    }
    static const Db *get_const_Db(const DB *db) {
	return (Db*)db->api_internal;
    }

    /* C++ analogues of the C functions. */
    int open(DbTxn */*txn*/, const char */*name*/, const char */*subname*/, DBTYPE, uint32_t/*flags*/, int/*mode*/);
    int close(uint32_t /*flags*/);

    int cursor(DbTxn */*txn*/, Dbc **/*cursorp*/, uint32_t /*flags*/);

    int del(DbTxn */*txn*/, Dbt */*key*/, uint32_t /*flags*/);

    int get(DbTxn */*txn*/, Dbt */*key*/, Dbt */*data*/, uint32_t /*flags*/);

    int put(DbTxn *, Dbt *, Dbt *, uint32_t);

    int get_flags(uint32_t *);
    int set_flags(uint32_t);

    int set_pagesize(uint32_t);

    int remove(const char *file, const char *database, uint32_t flags);

#if 0
    int set_bt_compare(bt_compare_fcn_type bt_compare_fcn);
    int set_bt_compare(int (*)(Db *, const Dbt *, const Dbt *));
#endif

    int set_dup_compare(dup_compare_fcn_type dup_compare_fcn);
    int set_dup_compare(int (*)(Db *, const Dbt *, const Dbt *));

    int associate(DbTxn *, Db *, int (*)(Db *, const Dbt *, const Dbt *, Dbt *), uint32_t);

    int fd(int *);

    void set_errpfx(const char *errpfx);
    void set_error_stream(std::ostream *);

    /* the cxx callbacks must be public so they can be called by the c callback.  But it's really private. */
    int (*associate_callback_cxx)(Db *, const Dbt *, const Dbt *, Dbt*);
    int (*dup_compare_callback_cxx)(Db *, const Dbt *, const Dbt *);

    //int (do_bt_compare_callback_cxx)(Db *, const Dbt *, const Dbt *);


 private:
    DB *the_db;
    DbEnv *the_Env;
    int is_private_env;
};

class DbEnv {
    friend class Db;
    friend class Dbc;
    friend class DbTxn;
 public:
    DbEnv(uint32_t flags);
    ~DbEnv(void);

    DB_ENV *get_DB_ENV(void) {
	if (this==0) return 0;
	return the_env;
    }

    /* C++ analogues of the C functions. */
    int close(uint32_t);
    int open(const char *, uint32_t, int);
    int set_cachesize(uint32_t, uint32_t, int);
    int set_redzone(uint32_t);
    int set_flags(uint32_t, int);
    int txn_begin(DbTxn *, DbTxn **, uint32_t);
    int set_data_dir(const char *dir);
    void set_errpfx(const char *errpfx);
    void err(int error, const char *fmt, ...)
             __attribute__((__format__(__printf__, 3, 4)));
    void set_errfile(FILE *errfile);
    void set_errcall(void (*)(const DbEnv *, const char *, const char *));
    void set_error_stream(std::ostream *);
    int get_flags(uint32_t *flagsp);

    int set_default_bt_compare(bt_compare_fcn_type bt_compare_fcn);
    // Don't support this one for now.  It's a little tricky.
    // int set_default_bt_compare(int (*)(Db *, const Dbt *, const Dbt *));

    // locking
#if DB_VERSION_MAJOR<4 || (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=4)
    // set_lk_max is only defined for versions up to 4.4
    int set_lk_max(uint32_t);
#endif
    int set_lk_max_locks(uint32_t);
    int get_lk_max_locks(uint32_t *);

// somewhat_private:
    int do_no_exceptions; // This should be private!!!
    void (*errcall)(const DbEnv *, const char *, const char *);
    std::ostream *_error_stream;

    //int (*bt_compare_callback_cxx)(Db *, const Dbt *, const Dbt *);

 private:
    DB_ENV *the_env;
    
    DbEnv(DB_ENV *, uint32_t /*flags*/);
    int maybe_throw_error(int /*err*/) throw (DbException);
    static int maybe_throw_error(int, DbEnv*, int /*no_exceptions*/) throw (DbException);
};

	
class DbTxn {
 public:
    int commit (uint32_t /*flags*/);
    int abort ();

    virtual ~DbTxn();

    DB_TXN *get_DB_TXN()
	{
	    if (this==0) return 0;
	    return the_txn;
	}

    DbTxn(DB_TXN*);
 private:
    DB_TXN *the_txn;
    
};

class Dbc : protected DBC {
 public:
    int close(void);
    int get(Dbt *, Dbt *, uint32_t);
    int count(db_recno_t *, uint32_t);
 private:
    Dbc();  // User may not call it.
    ~Dbc(); // User may not delete it.
};

#endif
