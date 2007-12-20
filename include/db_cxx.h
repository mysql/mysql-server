#include <db.h>
#include <string.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

class Dbt;
class DbEnv;
class DbTxn;
class Dbc;

// DBT and Dbt objects are the same pointers.  So watch out if you use Dbt to make other classes (e.g., with subclassing).
class Dbt : private DBT
{
    friend class Dbc;
 public:

    void *    get_data(void) const     { return data; }
    void      set_data(void *p)        { data = p; }
    
    u_int32_t get_size(void) const     { return size; }
    void      set_size(u_int32_t p)    { size =  p; }
		       
    u_int32_t get_flags() const        { return flags; }
    void      set_flags(u_int32_t f)   { flags = f; }

    DBT *get_DBT(void)                 { return (DBT*)this; }

    Dbt(void */*data*/, u_int32_t /*size*/);
    Dbt(void);
    ~Dbt();

 private:
    // Nothing here.
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
    int close(u_int32_t /*flags*/);
    int cursor(DbTxn */*txnid*/, Dbc **/*cursorp*/, u_int32_t /*flags*/);
    int del(DbTxn */*txnid*/, Dbt */*key*/, u_int32_t /*flags*/);
    int get(DbTxn */*txnid*/, Dbt */*key*/, Dbt */*data*/, u_int32_t /*flags*/);
    int open(DbTxn */*txnid*/, const char */*name*/, const char */*subname*/, DBTYPE, u_int32_t, int);
    int put(DbTxn *, Dbt *, Dbt *, u_int32_t);
    int get_flags(u_int32_t *);
    int set_flags(u_int32_t);
    int set_pagesize(u_int32_t);

 private:
    DB *the_db;
    DbEnv *the_Env;
    int is_private_env;
};

class DbEnv {
    friend class Db;
 public:
    DbEnv(u_int32_t flags);

    DB_ENV *get_DB_ENV(void) {
	if (this==0) return 0;
	return the_env;
    }

    /* C++ analogues of the C functions. */
    int close(u_int32_t);
    int open(const char *, u_int32_t, int);
    int set_cachesize(u_int32_t, u_int32_t, int);
#if DB_VERSION_MAJOR<4 || (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=4)
    // set_lk_max is only defined for versions up to 4.4
    int set_lk_max(u_int32_t);
#endif
    int txn_begin(DbTxn *, DbTxn **, u_int32_t);


 private:
    DB_ENV *the_env;

    DbEnv(DB_ENV *, u_int32_t flags);
};

	
class DbTxn {
 public:
    int commit (u_int32_t /*flags*/);

    DB_TXN *get_DB_TXN()
	{
	    if (this==0) return 0;
	    return the_txn;
	}

    DbTxn(DB_TXN*);
 private:
    DB_TXN *the_txn;
};

class Dbc : protected DBC
{
 public:
    int close(void);
    int get(Dbt*, Dbt *, u_int32_t);

};

