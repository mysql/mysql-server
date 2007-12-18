#include <db.h>
#include <string.h>

class Dbt;

// DBT and Dbt objects are the same pointers.  So watch out if you use Dbt to make other classes (e.g., with subclassing).
class Dbt : private DBT
{
 public:

    void *    get_data(void) const     { return data; }
    void      set_data(void *p)        { data = p; }
    
    u_int32_t get_size(void) const     { return size; }
    void      set_size(u_int32_t  p)   { size =  p; }
		       
    DBT *get_DBT(void)                 { return (DBT*)this; }

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

    DB *Db::get_DB(void) {
	return the_db;
    }
    const DB *Db::get_const_DB() const {
	return the_db;
    }
    static Db *Db::get_Db(DB *db) {
	return (Db*)db->toku_internal;
    }
    static const Db *Db::get_const_Db(const DB *db) {
	return (Db*)db->toku_internal;
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


 private:
    DB *the_db;
	
};
