#include <db.h>
#include <exception>
#include <string.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

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
    
    u_int32_t get_size(void) const     { return size; }
    void      set_size(u_int32_t p)    { size =  p; }
		       
    u_int32_t get_flags() const        { return flags; }
    void      set_flags(u_int32_t f)   { flags = f; }

    u_int32_t get_ulen() const         { return ulen; }
    void      set_ulen(u_int32_t p)    { ulen = p; }

    DBT *get_DBT(void)                              { return (DBT*)this; }
    const DBT *get_const_DBT(void) const            { return (const DBT*)this; }

    static Dbt* get_Dbt(DBT *dbt)                   { return (Dbt *)dbt; }
    static const Dbt* get_const_Dbt(const DBT *dbt) { return (const Dbt *)dbt; }

    Dbt(void */*data*/, u_int32_t /*size*/);
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
    Db(DbEnv *dbenv, u_int32_t flags);
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
    int open(DbTxn */*txn*/, const char */*name*/, const char */*subname*/, DBTYPE, u_int32_t/*flags*/, int/*mode*/);
    int close(u_int32_t /*flags*/);

    int cursor(DbTxn */*txn*/, Dbc **/*cursorp*/, u_int32_t /*flags*/);

    int del(DbTxn */*txn*/, Dbt */*key*/, u_int32_t /*flags*/);

    int get(DbTxn */*txn*/, Dbt */*key*/, Dbt */*data*/, u_int32_t /*flags*/);
    int pget(DbTxn *, Dbt *, Dbt *, Dbt *, u_int32_t);

    int put(DbTxn *, Dbt *, Dbt *, u_int32_t);

    int get_flags(u_int32_t *);
    int set_flags(u_int32_t);

    int set_pagesize(u_int32_t);

    int remove(const char *file, const char *database, u_int32_t flags);

    int set_bt_compare(bt_compare_fcn_type bt_compare_fcn);
    int set_bt_compare(int (*)(Db *, const Dbt *, const Dbt *));

    int set_dup_compare(dup_compare_fcn_type dup_compare_fcn);
    int set_dup_compare(int (*)(Db *, const Dbt *, const Dbt *));

    int associate(DbTxn *, Db *, int (*)(Db *, const Dbt *, const Dbt *, Dbt *), u_int32_t);

    /* the cxx callbacks must be public so they can be called by the c callback.  But it's really private. */
    int (*associate_callback_cxx)(Db *, const Dbt *, const Dbt *, Dbt*);
    int (*bt_compare_callback_cxx)(Db *, const Dbt *, const Dbt *);
    int (*dup_compare_callback_cxx)(Db *, const Dbt *, const Dbt *);

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
    DbEnv(u_int32_t flags);
    ~DbEnv(void);

    DB_ENV *get_DB_ENV(void) {
	if (this==0) return 0;
	return the_env;
    }

    /* C++ analogues of the C functions. */
    int close(u_int32_t);
    int open(const char *, u_int32_t, int);
    int set_cachesize(u_int32_t, u_int32_t, int);
    int set_flags(u_int32_t, int);
    int txn_begin(DbTxn *, DbTxn **, u_int32_t);
    int set_data_dir(const char *dir);
    void set_errpfx(const char *errpfx);
    void err(int error, const char *fmt, ...);
    void set_errfile(FILE *errfile);
    void set_errcall(void (*)(const DbEnv *, const char *, const char *));
    int get_flags(u_int32_t *flagsp);

    // locking
#if DB_VERSION_MAJOR<4 || (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=4)
    // set_lk_max is only defined for versions up to 4.4
    int set_lk_max(u_int32_t);
#endif
    int set_lk_max_locks(u_int32_t);
    int get_lk_max_locks(u_int32_t *);
    int set_lk_max_lockers(u_int32_t);
    int get_lk_max_lockers(u_int32_t *);
    int set_lk_max_objects(u_int32_t);
    int get_lk_max_objects(u_int32_t *);

// somewhat_private:
    int do_no_exceptions; // This should be private!!!
    void (*errcall)(const DbEnv *, const char *, const char *);

 private:
    DB_ENV *the_env;
    
    DbEnv(DB_ENV *, u_int32_t /*flags*/);
    int maybe_throw_error(int /*err*/) throw (DbException);
    static int maybe_throw_error(int, DbEnv*, int /*no_exceptions*/) throw (DbException);
};

	
class DbTxn {
 public:
    int commit (u_int32_t /*flags*/);
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
    int get(Dbt *, Dbt *, u_int32_t);
    int pget(Dbt *, Dbt *, Dbt *, u_int32_t);
    int del(u_int32_t);
    int count(db_recno_t *, u_int32_t);
 private:
    Dbc();  // User may not call it.
    ~Dbc(); // User may not delete it.
};
