/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_cxx.h,v 11.44 2000/12/21 20:30:18 dda Exp $
 */

#ifndef _DB_CXX_H_
#define	_DB_CXX_H_
//
// C++ assumptions:
//
// To ensure portability to many platforms, both new and old, we make
// few assumptions about the C++ compiler and library.  For example,
// we do not expect STL, templates or namespaces to be available.  The
// "newest" C++ feature used is exceptions, which are used liberally
// to transmit error information.  Even the use of exceptions can be
// disabled at runtime, to do so, use the DB_CXX_NO_EXCEPTIONS flags
// with the DbEnv or Db constructor.
//
// C++ naming conventions:
//
//  - All top level class names start with Db.
//  - All class members start with lower case letter.
//  - All private data members are suffixed with underscore.
//  - Use underscores to divide names into multiple words.
//  - Simple data accessors are named with get_ or set_ prefix.
//  - All method names are taken from names of functions in the C
//    layer of db (usually by dropping a prefix like "db_").
//    These methods have the same argument types and order,
//    other than dropping the explicit arg that acts as "this".
//
// As a rule, each DbFoo object has exactly one underlying DB_FOO struct
// (defined in db.h) associated with it.  In some cases, we inherit directly
// from the DB_FOO structure to make this relationship explicit.  Often,
// the underlying C layer allocates and deallocates these structures, so
// there is no easy way to add any data to the DbFoo class.  When you see
// a comment about whether data is permitted to be added, this is what
// is going on.  Of course, if we need to add data to such C++ classes
// in the future, we will arrange to have an indirect pointer to the
// DB_FOO struct (as some of the classes already have).
//

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Forward declarations
//

#include <iostream.h>
#include <stdarg.h>
#include "db.h"

class Db;                                        // forward
class Dbc;                                       // forward
class DbEnv;                                     // forward
class DbException;                               // forward
class DbInfo;                                    // forward
class DbLock;                                    // forward
class DbLsn;                                     // forward
class DbMpoolFile;                               // forward
class Dbt;                                       // forward
class DbTxn;                                     // forward

// These classes are not defined here and should be invisible
// to the user, but some compilers require forward references.
// There is one for each use of the DEFINE_DB_CLASS macro.

class DbImp;
class DbEnvImp;
class DbMpoolFileImp;
class DbTxnImp;

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Mechanisms for declaring classes
//

//
// Every class defined in this file has an _exported next to the class name.
// This is needed for WinTel machines so that the class methods can
// be exported or imported in a DLL as appropriate.  Users of the DLL
// use the define DB_USE_DLL.  When the DLL is built, DB_CREATE_DLL
// must be defined.
//
#if defined(_MSC_VER)

#  if defined(DB_CREATE_DLL)
#    define _exported __declspec(dllexport)      // creator of dll
#  elif defined(DB_USE_DLL)
#    define _exported __declspec(dllimport)      // user of dll
#  else
#    define _exported                            // static lib creator or user
#  endif

#else

#  define _exported

#endif

// DEFINE_DB_CLASS defines an imp_ data member and imp() accessor.
// The underlying type is a pointer to an opaque *Imp class, that
// gets converted to the correct implementation class by the implementation.
//
// Since these defines use "private/public" labels, and leave the access
// being "private", we always use these by convention before any data
// members in the private section of a class.  Keeping them in the
// private section also emphasizes that they are off limits to user code.
//
#define	DEFINE_DB_CLASS(name) \
	public: class name##Imp* imp() { return (imp_); } \
	public: const class name##Imp* constimp() const { return (imp_); } \
	private: class name##Imp* imp_

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Turn off inappropriate compiler warnings
//

#ifdef _MSC_VER

// These are level 4 warnings that are explicitly disabled.
// With Visual C++, by default you do not see above level 3 unless
// you use /W4.  But we like to compile with the highest level
// warnings to catch other errors.
//
// 4201: nameless struct/union
//       triggered by standard include file <winnt.h>
//
// 4514: unreferenced inline function has been removed
//       certain include files in MSVC define methods that are not called
//
#pragma warning(disable: 4201 4514)

#endif

// Some interfaces can be customized by allowing users
// to define callback functions.  For performance and
// logistical reasons, some callbacks require you do
// declare the functions in C, or in an extern "C" block.
//
extern "C" {
	typedef void * (*db_malloc_fcn_type)
		(size_t);
	typedef void * (*db_realloc_fcn_type)
		(void *, size_t);
	typedef int (*bt_compare_fcn_type)
		(DB *, const DBT *, const DBT *);
	typedef size_t (*bt_prefix_fcn_type)
		(DB *, const DBT *, const DBT *);
	typedef int (*dup_compare_fcn_type)
		(DB *, const DBT *, const DBT *);
	typedef u_int32_t (*h_hash_fcn_type)
		(DB *, const void *, u_int32_t);
	typedef int (*pgin_fcn_type)(DB_ENV *dbenv,
		 db_pgno_t pgno, void *pgaddr, DBT *pgcookie);
	typedef int (*pgout_fcn_type)(DB_ENV *dbenv,
		 db_pgno_t pgno, void *pgaddr, DBT *pgcookie);
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Exception classes
//

// Almost any error in the DB library throws a DbException.
// Every exception should be considered an abnormality
// (e.g. bug, misuse of DB, file system error).
//
// NOTE: We would like to inherit from class exception and
//       let it handle what(), but there are
//       MSVC++ problems when <exception> is included.
//
class _exported DbException
{
public:
	virtual ~DbException();
	DbException(int err);
	DbException(const char *description);
	DbException(const char *prefix, int err);
	DbException(const char *prefix1, const char *prefix2, int err);
	int get_errno() const;
	virtual const char *what() const;

	DbException(const DbException &);
	DbException &operator = (const DbException &);

private:
	char *what_;
	int err_;                   // errno
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Lock classes
//

class _exported DbLock
{
	friend class DbEnv;

public:
	DbLock();

	int put(DbEnv *env);

	DbLock(const DbLock &);
	DbLock &operator = (const DbLock &);

protected:
	// We can add data to this class if needed
	// since its contained class is not allocated by db.
	// (see comment at top)

	DbLock(DB_LOCK);
	DB_LOCK lock_;
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Log classes
//

class _exported DbLsn : protected DB_LSN
{
	friend class DbEnv;          // friendship needed to cast to base class
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Memory pool classes
//

class _exported DbMpoolFile
{
	friend class DbEnv;

public:
	int close();
	int get(db_pgno_t *pgnoaddr, u_int32_t flags, void *pagep);
	int put(void *pgaddr, u_int32_t flags);
	int set(void *pgaddr, u_int32_t flags);
	int sync();

	static int open(DbEnv *envp, const char *file,
			u_int32_t flags, int mode, size_t pagesize,
			DB_MPOOL_FINFO *finfop, DbMpoolFile **mpf);

private:
	// We can add data to this class if needed
	// since it is implemented via a pointer.
	// (see comment at top)

	// Note: use DbMpoolFile::open()
	// to get pointers to a DbMpoolFile,
	// and call DbMpoolFile::close() rather than delete to release them.
	//
	DbMpoolFile();

	// Shut g++ up.
protected:
	~DbMpoolFile();

private:
	// no copying
	DbMpoolFile(const DbMpoolFile &);
	void operator = (const DbMpoolFile &);

	DEFINE_DB_CLASS(DbMpoolFile);
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Transaction classes
//

class _exported DbTxn
{
	friend class DbEnv;

public:
	int abort();
	int commit(u_int32_t flags);
	u_int32_t id();
	int prepare();

private:
	// We can add data to this class if needed
	// since it is implemented via a pointer.
	// (see comment at top)

	// Note: use DbEnv::txn_begin() to get pointers to a DbTxn,
	// and call DbTxn::abort() or DbTxn::commit rather than
	// delete to release them.
	//
	DbTxn();
	~DbTxn();

	// no copying
	DbTxn(const DbTxn &);
	void operator = (const DbTxn &);

	DEFINE_DB_CLASS(DbTxn);
};

//
// Berkeley DB environment class.  Provides functions for opening databases.
// User of this library can use this class as a starting point for
// developing a DB application - derive their application class from
// this one, add application control logic.
//
// Note that if you use the default constructor, you must explicitly
// call appinit() before any other db activity (e.g. opening files)
//
class _exported DbEnv
{
	friend class Db;
	friend class DbLock;
	friend class DbMpoolFile;

public:

	~DbEnv();

	// After using this constructor, you can set any needed
	// parameters for the environment using the set_* methods.
	// Then call open() to finish initializing the environment
	// and attaching it to underlying files.
	//
	DbEnv(u_int32_t flags);

	// These methods match those in the C interface.
	//
	int close(u_int32_t);
	void err(int, const char *, ...);
	void errx(const char *, ...);
	int open(const char *, u_int32_t, int);
	int remove(const char *, u_int32_t);
	int set_cachesize(u_int32_t, u_int32_t, int);
	int set_data_dir(const char *);
	void set_errcall(void (*)(const char *, char *));
	void set_errfile(FILE *);
	void set_errpfx(const char *);
	int set_flags(u_int32_t, int);
	int set_feedback(void (*)(DbEnv *, int, int));
	int set_recovery_init(int (*)(DbEnv *));
	int set_lg_bsize(u_int32_t);
	int set_lg_dir(const char *);
	int set_lg_max(u_int32_t);
	int set_lk_conflicts(u_int8_t *, int);
	int set_lk_detect(u_int32_t);
	int set_lk_max(u_int32_t);
	int set_lk_max_lockers(u_int32_t);
	int set_lk_max_locks(u_int32_t);
	int set_lk_max_objects(u_int32_t);
	int set_mp_mmapsize(size_t);
	int set_mutexlocks(int);
	static int set_pageyield(int);
	int set_paniccall(void (*)(DbEnv *, int));
	static int set_panicstate(int);
	static int set_region_init(int);
	int set_server(char *, long, long, u_int32_t);
	int set_shm_key(long);
	int set_tmp_dir(const char *);
	static int set_tas_spins(u_int32_t);
	int set_tx_max(u_int32_t);
	int set_tx_recover(int (*)(DbEnv *, Dbt *, DbLsn *, db_recops));
	int set_tx_timestamp(time_t *);
	int set_verbose(u_int32_t which, int onoff);

	// Version information.  A static method so it can be obtained anytime.
	//
	static char *version(int *major, int *minor, int *patch);

	// Convert DB errors to strings
	static char *strerror(int);

	// If an error is detected and the error call function
	// or stream is set, a message is dispatched or printed.
	// If a prefix is set, each message is prefixed.
	//
	// You can use set_errcall() or set_errfile() above to control
	// error functionality.  Alternatively, you can call
	// set_error_stream() to force all errors to a C++ stream.
	// It is unwise to mix these approaches.
	//
	void set_error_stream(ostream *);

	// used internally
	static void runtime_error(const char *caller, int err,
				  int error_policy);

	// Lock functions
	//
	int lock_detect(u_int32_t flags, u_int32_t atype, int *aborted);
	int lock_get(u_int32_t locker, u_int32_t flags, const Dbt *obj,
		     db_lockmode_t lock_mode, DbLock *lock);
	int lock_id(u_int32_t *idp);
	int lock_stat(DB_LOCK_STAT **statp, db_malloc_fcn_type db_malloc_fcn);
	int lock_vec(u_int32_t locker, u_int32_t flags, DB_LOCKREQ list[],
		     int nlist, DB_LOCKREQ **elistp);

	// Log functions
	//
	int log_archive(char **list[], u_int32_t flags, db_malloc_fcn_type db_malloc_fcn);
	static int log_compare(const DbLsn *lsn0, const DbLsn *lsn1);
	int log_file(DbLsn *lsn, char *namep, size_t len);
	int log_flush(const DbLsn *lsn);
	int log_get(DbLsn *lsn, Dbt *data, u_int32_t flags);
	int log_put(DbLsn *lsn, const Dbt *data, u_int32_t flags);

	int log_register(Db *dbp, const char *name);
	int log_stat(DB_LOG_STAT **spp, db_malloc_fcn_type db_malloc_fcn);
	int log_unregister(Db *dbp);

	// Mpool functions
	//
	int memp_register(int ftype,
			  pgin_fcn_type pgin_fcn,
			  pgout_fcn_type pgout_fcn);

	int memp_stat(DB_MPOOL_STAT **gsp, DB_MPOOL_FSTAT ***fsp,
		      db_malloc_fcn_type db_malloc_fcn);
	int memp_sync(DbLsn *lsn);
	int memp_trickle(int pct, int *nwrotep);

	// Transaction functions
	//
	int txn_begin(DbTxn *pid, DbTxn **tid, u_int32_t flags);
	int txn_checkpoint(u_int32_t kbyte, u_int32_t min, u_int32_t flags);
	int txn_stat(DB_TXN_STAT **statp, db_malloc_fcn_type db_malloc_fcn);

	// These are public only because they need to be called
	// via C functions.  They should never be called by users
	// of this class.
	//
	static void _stream_error_function(const char *, char *);
	static int _tx_recover_intercept(DB_ENV *env, DBT *dbt, DB_LSN *lsn,
					db_recops op);
	static void _paniccall_intercept(DB_ENV *env, int errval);
	static int _recovery_init_intercept(DB_ENV *env);
	static void _feedback_intercept(DB_ENV *env, int opcode, int pct);
	static void _destroy_check(const char *str, int isDbEnv);

private:
	void cleanup();
	int initialize(DB_ENV *env);
	int error_policy();

	// Used internally
	DbEnv(DB_ENV *, u_int32_t flags);

	// no copying
	DbEnv(const DbEnv &);
	void operator = (const DbEnv &);

	DEFINE_DB_CLASS(DbEnv);

	// instance data
	int construct_error_;
	u_int32_t construct_flags_;
	Db *headdb_;
	Db *taildb_;
	int (*tx_recover_callback_)(DbEnv *, Dbt *, DbLsn *, db_recops);
	int (*recovery_init_callback_)(DbEnv *);
	void (*paniccall_callback_)(DbEnv *, int);
	void (*feedback_callback_)(DbEnv *, int, int);

	// class data
	static ostream *error_stream_;
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Table access classes
//

//
// Represents a database table = a set of keys with associated values.
//
class _exported Db
{
	friend class DbEnv;

public:
	Db(DbEnv*, u_int32_t);      // create a Db object, then call open()
	~Db();                      // does *not* call close.

	// These methods exactly match those in the C interface.
	//
	int close(u_int32_t flags);
	int cursor(DbTxn *txnid, Dbc **cursorp, u_int32_t flags);
	int del(DbTxn *txnid, Dbt *key, u_int32_t flags);
	void err(int, const char *, ...);
	void errx(const char *, ...);
	int fd(int *fdp);
	int get(DbTxn *txnid, Dbt *key, Dbt *data, u_int32_t flags);
	int get_byteswapped() const;
	DBTYPE get_type() const;
	int join(Dbc **curslist, Dbc **dbcp, u_int32_t flags);
	int key_range(DbTxn *, Dbt *, DB_KEY_RANGE *, u_int32_t);
	int open(const char *, const char *subname, DBTYPE, u_int32_t, int);
	int put(DbTxn *, Dbt *, Dbt *, u_int32_t);
	int remove(const char *, const char *, u_int32_t);
	int rename(const char *, const char *, const char *, u_int32_t);
	int set_bt_compare(bt_compare_fcn_type);
	int set_bt_maxkey(u_int32_t);
	int set_bt_minkey(u_int32_t);
	int set_bt_prefix(bt_prefix_fcn_type);
	int set_cachesize(u_int32_t, u_int32_t, int);
	int set_dup_compare(dup_compare_fcn_type);
	void set_errcall(void (*)(const char *, char *));
	void set_errfile(FILE *);
	void set_errpfx(const char *);
	int set_append_recno(int (*)(Db *, Dbt *, db_recno_t));
	int set_feedback(void (*)(Db *, int, int));
	int set_flags(u_int32_t);
	int set_h_ffactor(u_int32_t);
	int set_h_hash(h_hash_fcn_type);
	int set_h_nelem(u_int32_t);
	int set_lorder(int);
	int set_malloc(db_malloc_fcn_type);
	int set_pagesize(u_int32_t);
	int set_paniccall(void (*)(DbEnv *, int));
	int set_realloc(db_realloc_fcn_type);
	int set_re_delim(int);
	int set_re_len(u_int32_t);
	int set_re_pad(int);
	int set_re_source(char *);
	int set_q_extentsize(u_int32_t);
	int stat(void *sp, db_malloc_fcn_type db_malloc_fcn, u_int32_t flags);
	int sync(u_int32_t flags);
	int upgrade(const char *name, u_int32_t flags);
	int verify(const char *, const char *, ostream *, u_int32_t);

	// This additional method is available for C++
	//
	void set_error_stream(ostream *);

	// These are public only because it needs to be called
	// via C functions.  It should never be called by users
	// of this class.
	//
	static void _feedback_intercept(DB *db, int opcode, int pct);
	static int _append_recno_intercept(DB *db, DBT *data, db_recno_t recno);
private:

	// no copying
	Db(const Db &);
	Db &operator = (const Db &);

	DEFINE_DB_CLASS(Db);

	void cleanup();
	int initialize();
	int error_policy();

	// instance data
	DbEnv *env_;
	Db *next_;
	Db *prev_;
	int construct_error_;
	u_int32_t flags_;
	u_int32_t construct_flags_;
	void (*feedback_callback_)(Db *, int, int);
	int (*append_recno_callback_)(Db *, Dbt *, db_recno_t);
};

//
// A chunk of data, maybe a key or value.
//
class _exported Dbt : private DBT
{
	friend class Dbc;
	friend class Db;
	friend class DbEnv;

public:

	// key/data
	void *get_data() const;
	void set_data(void *);

	// key/data length
	u_int32_t get_size() const;
	void set_size(u_int32_t);

	// RO: length of user buffer.
	u_int32_t get_ulen() const;
	void set_ulen(u_int32_t);

	// RO: get/put record length.
	u_int32_t get_dlen() const;
	void set_dlen(u_int32_t);

	// RO: get/put record offset.
	u_int32_t get_doff() const;
	void set_doff(u_int32_t);

	// flags
	u_int32_t get_flags() const;
	void set_flags(u_int32_t);

	Dbt(void *data, size_t size);
	Dbt();
	~Dbt();
	Dbt(const Dbt &);
	Dbt &operator = (const Dbt &);

private:
	// We can add data to this class if needed
	// since parent class is not allocated by db.
	// (see comment at top)
};

class _exported Dbc : protected DBC
{
	friend class Db;

public:
	int close();
	int count(db_recno_t *countp, u_int32_t flags);
	int del(u_int32_t flags);
	int dup(Dbc** cursorp, u_int32_t flags);
	int get(Dbt* key, Dbt *data, u_int32_t flags);
	int put(Dbt* key, Dbt *data, u_int32_t flags);

private:
	// No data is permitted in this class (see comment at top)

	// Note: use Db::cursor() to get pointers to a Dbc,
	// and call Dbc::close() rather than delete to release them.
	//
	Dbc();
	~Dbc();

	// no copying
	Dbc(const Dbc &);
	Dbc &operator = (const Dbc &);
};
#endif /* !_DB_CXX_H_ */
