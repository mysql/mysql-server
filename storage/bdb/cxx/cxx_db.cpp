/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: cxx_db.cpp,v 11.71 2002/08/26 22:13:36 mjc Exp $";
#endif /* not lint */

#include <errno.h>
#include <string.h>

#include "db_cxx.h"
#include "dbinc/cxx_int.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc_auto/db_auto.h"
#include "dbinc_auto/crdel_auto.h"
#include "dbinc/db_dispatch.h"
#include "dbinc_auto/db_ext.h"
#include "dbinc_auto/common_ext.h"

// Helper macros for simple methods that pass through to the
// underlying C method. It may return an error or raise an exception.
// Note this macro expects that input _argspec is an argument
// list element (e.g., "char *arg") and that _arglist is the arguments
// that should be passed through to the C method (e.g., "(db, arg)")
//
#define	DB_METHOD(_name, _argspec, _arglist, _retok)			\
int Db::_name _argspec							\
{									\
	int ret;							\
	DB *db = unwrap(this);						\
									\
	ret = db->_name _arglist;					\
	if (!_retok(ret))						\
		DB_ERROR("Db::" # _name, ret, error_policy());		\
	return (ret);							\
}

#define	DB_METHOD_CHECKED(_name, _cleanup, _argspec, _arglist, _retok)	\
int Db::_name _argspec							\
{									\
	int ret;							\
	DB *db = unwrap(this);						\
									\
	if (!db) {							\
		DB_ERROR("Db::" # _name, EINVAL, error_policy());	\
		return (EINVAL);					\
	}								\
	if (_cleanup)							\
		cleanup();						\
	ret = db->_name _arglist;					\
	if (!_retok(ret))						\
		DB_ERROR("Db::" # _name, ret, error_policy());		\
	return (ret);							\
}

#define	DB_METHOD_QUIET(_name, _argspec, _arglist)			\
int Db::_name _argspec							\
{									\
	DB *db = unwrap(this);						\
									\
	return (db->_name _arglist);					\
}

#define	DB_METHOD_VOID(_name, _argspec, _arglist)			\
void Db::_name _argspec							\
{									\
	DB *db = unwrap(this);						\
									\
	db->_name _arglist;						\
}

// A truism for the Db object is that there is a valid
// DB handle from the constructor until close().
// After the close, the DB handle is invalid and
// no operations are permitted on the Db (other than
// destructor).  Leaving the Db handle open and not
// doing a close is generally considered an error.
//
// We used to allow Db objects to be closed and reopened.
// This implied always keeping a valid DB object, and
// coordinating the open objects between Db/DbEnv turned
// out to be overly complicated.  Now we do not allow this.

Db::Db(DbEnv *env, u_int32_t flags)
:	imp_(0)
,	env_(env)
,	construct_error_(0)
,	flags_(0)
,	construct_flags_(flags)
,	append_recno_callback_(0)
,	associate_callback_(0)
,	bt_compare_callback_(0)
,	bt_prefix_callback_(0)
,	dup_compare_callback_(0)
,	feedback_callback_(0)
,	h_hash_callback_(0)
{
	if (env_ == 0)
		flags_ |= DB_CXX_PRIVATE_ENV;

	if ((construct_error_ = initialize()) != 0)
		DB_ERROR("Db::Db", construct_error_, error_policy());
}

// If the DB handle is still open, we close it.  This is to make stack
// allocation of Db objects easier so that they are cleaned up in the error
// path.  If the environment was closed prior to this, it may cause a trap, but
// an error message is generated during the environment close.  Applications
// should call close explicitly in normal (non-exceptional) cases to check the
// return value.
//
Db::~Db()
{
	DB *db;

	db = unwrap(this);
	if (db != NULL) {
		cleanup();
		(void)db->close(db, 0);
	}
}

// private method to initialize during constructor.
// initialize must create a backing DB object,
// and if that creates a new DB_ENV, it must be tied to a new DbEnv.
//
int Db::initialize()
{
	DB *db;
	DB_ENV *cenv = unwrap(env_);
	int ret;
	u_int32_t cxx_flags;

	cxx_flags = construct_flags_ & DB_CXX_NO_EXCEPTIONS;

	// Create a new underlying DB object.
	// We rely on the fact that if a NULL DB_ENV* is given,
	// one is allocated by DB.
	//
	if ((ret = db_create(&db, cenv,
			     construct_flags_ & ~cxx_flags)) != 0)
		return (ret);

	// Associate the DB with this object
	imp_ = wrap(db);
	db->api_internal = this;

	// Create a new DbEnv from a DB_ENV* if it was created locally.
	// It is deleted in Db::close().
	//
	if ((flags_ & DB_CXX_PRIVATE_ENV) != 0)
		env_ = new DbEnv(db->dbenv, cxx_flags);

	return (0);
}

// private method to cleanup after destructor or during close.
// If the environment was created by this Db object, we optionally
// delete it, or return it so the caller can delete it after
// last use.
//
void Db::cleanup()
{
	DB *db = unwrap(this);

	if (db != NULL) {
		// extra safety
		db->api_internal = 0;
		imp_ = 0;

		// we must dispose of the DbEnv object if
		// we created it.  This will be the case
		// if a NULL DbEnv was passed into the constructor.
		// The underlying DB_ENV object will be inaccessible
		// after the close, so we must clean it up now.
		//
		if ((flags_ & DB_CXX_PRIVATE_ENV) != 0) {
			env_->cleanup();
			delete env_;
			env_ = 0;
		}
	}
}

// Return a tristate value corresponding to whether we should
// throw exceptions on errors:
//   ON_ERROR_RETURN
//   ON_ERROR_THROW
//   ON_ERROR_UNKNOWN
//
int Db::error_policy()
{
	if (env_ != NULL)
		return (env_->error_policy());
	else {
		// If the env_ is null, that means that the user
		// did not attach an environment, so the correct error
		// policy can be deduced from constructor flags
		// for this Db.
		//
		if ((construct_flags_ & DB_CXX_NO_EXCEPTIONS) != 0) {
			return (ON_ERROR_RETURN);
		}
		else {
			return (ON_ERROR_THROW);
		}
	}
}

int Db::close(u_int32_t flags)
{
	DB *db = unwrap(this);
	int ret;

	// after a DB->close (no matter if success or failure),
	// the underlying DB object must not be accessed,
	// so we clean up in advance.
	//
	cleanup();

	// It's safe to throw an error after the close,
	// since our error mechanism does not peer into
	// the DB* structures.
	//
	if ((ret = db->close(db, flags)) != 0)
		DB_ERROR("Db::close", ret, error_policy());

	return (ret);
}

// The following cast implies that Dbc can be no larger than DBC
DB_METHOD(cursor, (DbTxn *txnid, Dbc **cursorp, u_int32_t flags),
    (db, unwrap(txnid), (DBC **)cursorp, flags),
    DB_RETOK_STD)

DB_METHOD(del, (DbTxn *txnid, Dbt *key, u_int32_t flags),
    (db, unwrap(txnid), key, flags),
    DB_RETOK_DBDEL)

void Db::err(int error, const char *format, ...)
{
	DB *db = unwrap(this);

	DB_REAL_ERR(db->dbenv, error, 1, 1, format);
}

void Db::errx(const char *format, ...)
{
	DB *db = unwrap(this);

	DB_REAL_ERR(db->dbenv, 0, 0, 1, format);
}

DB_METHOD(fd, (int *fdp),
    (db, fdp),
    DB_RETOK_STD)

int Db::get(DbTxn *txnid, Dbt *key, Dbt *value, u_int32_t flags)
{
	DB *db = unwrap(this);
	int ret;

	ret = db->get(db, unwrap(txnid), key, value, flags);

	if (!DB_RETOK_DBGET(ret)) {
		if (ret == ENOMEM && DB_OVERFLOWED_DBT(value))
			DB_ERROR_DBT("Db::get", value, error_policy());
		else
			DB_ERROR("Db::get", ret, error_policy());
	}

	return (ret);
}

int Db::get_byteswapped(int *isswapped)
{
	DB *db = (DB *)unwrapConst(this);
	return (db->get_byteswapped(db, isswapped));
}

int Db::get_type(DBTYPE *dbtype)
{
	DB *db = (DB *)unwrapConst(this);
	return (db->get_type(db, dbtype));
}

// Dbc is a "compatible" subclass of DBC - that is, no virtual functions
// or even extra data members, so these casts, although technically
// non-portable, "should" always be okay.
DB_METHOD(join, (Dbc **curslist, Dbc **cursorp, u_int32_t flags),
    (db, (DBC **)curslist, (DBC **)cursorp, flags),
    DB_RETOK_STD)

DB_METHOD(key_range,
    (DbTxn *txnid, Dbt *key, DB_KEY_RANGE *results, u_int32_t flags),
    (db, unwrap(txnid), key, results, flags),
    DB_RETOK_STD)

// If an error occurred during the constructor, report it now.
// Otherwise, call the underlying DB->open method.
//
int Db::open(DbTxn *txnid, const char *file, const char *database,
	     DBTYPE type, u_int32_t flags, int mode)
{
	int ret;
	DB *db = unwrap(this);

	if (construct_error_ != 0)
		ret = construct_error_;
	else
		ret = db->open(db, unwrap(txnid), file, database, type, flags,
		    mode);

	if (!DB_RETOK_STD(ret))
		DB_ERROR("Db::open", ret, error_policy());

	return (ret);
}

int Db::pget(DbTxn *txnid, Dbt *key, Dbt *pkey, Dbt *value, u_int32_t flags)
{
	DB *db = unwrap(this);
	int ret;

	ret = db->pget(db, unwrap(txnid), key, pkey, value, flags);

	/* The logic here is identical to Db::get - reuse the macro. */
	if (!DB_RETOK_DBGET(ret)) {
		if (ret == ENOMEM && DB_OVERFLOWED_DBT(value))
			DB_ERROR_DBT("Db::pget", value, error_policy());
		else
			DB_ERROR("Db::pget", ret, error_policy());
	}

	return (ret);
}

DB_METHOD(put,
    (DbTxn *txnid, Dbt *key, Dbt *value, u_int32_t flags),
    (db, unwrap(txnid), key, value, flags),
    DB_RETOK_DBPUT)

DB_METHOD_CHECKED(rename, 1,
    (const char *file, const char *database, const char *newname,
    u_int32_t flags),
    (db, file, database, newname, flags), DB_RETOK_STD)

DB_METHOD_CHECKED(remove, 1,
    (const char *file, const char *database, u_int32_t flags),
    (db, file, database, flags), DB_RETOK_STD)

DB_METHOD_CHECKED(truncate, 0,
    (DbTxn *txnid, u_int32_t *countp, u_int32_t flags),
    (db, unwrap(txnid), countp, flags), DB_RETOK_STD)

DB_METHOD_CHECKED(stat, 0,
    (void *sp, u_int32_t flags), (db, sp, flags), DB_RETOK_STD)

DB_METHOD_CHECKED(sync, 0,
    (u_int32_t flags), (db, flags), DB_RETOK_STD)

DB_METHOD_CHECKED(upgrade, 0,
    (const char *name, u_int32_t flags), (db, name, flags), DB_RETOK_STD)

////////////////////////////////////////////////////////////////////////
//
// callbacks
//
// *_intercept_c are 'glue' functions that must be declared
// as extern "C" so to be typesafe.  Using a C++ method, even
// a static class method with 'correct' arguments, will not pass
// the test; some picky compilers do not allow mixing of function
// pointers to 'C' functions with function pointers to C++ functions.
//
// One wart with this scheme is that the *_callback_ method pointer
// must be declared public to be accessible by the C intercept.
// It's possible to accomplish the goal without this, and with
// another public transfer method, but it's just too much overhead.
// These callbacks are supposed to be *fast*.
//
// The DBTs we receive in these callbacks from the C layer may be
// manufactured there, but we want to treat them as a Dbts.
// Technically speaking, these DBTs were not constructed as a Dbts,
// but it should be safe to cast them as such given that Dbt is a
// *very* thin extension of the DBT.  That is, Dbt has no additional
// data elements, does not use virtual functions, virtual inheritance,
// multiple inheritance, RTI, or any other language feature that
// causes the structure to grow or be displaced.  Although this may
// sound risky, a design goal of C++ is complete structure
// compatibility with C, and has the philosophy 'if you don't use it,
// you shouldn't incur the overhead'.  If the C/C++ compilers you're
// using on a given machine do not have matching struct layouts, then
// a lot more things will be broken than just this.
//
// The alternative, creating a Dbt here in the callback, and populating
// it from the DBT, is just too slow and cumbersome to be very useful.

// These macros avoid a lot of boilerplate code for callbacks

#define	DB_CALLBACK_C_INTERCEPT(_name, _rettype, _cargspec,		\
    _return, _cxxargs)							\
extern "C" _rettype _db_##_name##_intercept_c _cargspec			\
{									\
	Db *cxxthis;							\
									\
	DB_ASSERT(cthis != NULL);					\
	cxxthis = (Db *)cthis->api_internal;				\
	DB_ASSERT(cxxthis != NULL);					\
	DB_ASSERT(cxxthis->_name##_callback_ != 0);			\
									\
	_return (*cxxthis->_name##_callback_) _cxxargs;			\
}

#define	DB_SET_CALLBACK(_cxxname, _name, _cxxargspec, _cb)		\
int Db::_cxxname _cxxargspec						\
{									\
	DB *cthis = unwrap(this);					\
									\
	_name##_callback_ = _cb;					\
	return ((*(cthis->_cxxname))(cthis,				\
	    (_cb) ? _db_##_name##_intercept_c : NULL));			\
}

/* associate callback - doesn't quite fit the pattern because of the flags */
DB_CALLBACK_C_INTERCEPT(associate,
    int, (DB *cthis, const DBT *key, const DBT *data, DBT *retval),
    return, (cxxthis, Dbt::get_const_Dbt(key), Dbt::get_const_Dbt(data),
    Dbt::get_Dbt(retval)))

int Db::associate(DbTxn *txn, Db *secondary, int (*callback)(Db *, const Dbt *,
	const Dbt *, Dbt *), u_int32_t flags)
{
	DB *cthis = unwrap(this);

	/* Since the secondary Db is used as the first argument
	 * to the callback, we store the C++ callback on it
	 * rather than on 'this'.
	 */
	secondary->associate_callback_ = callback;
	return ((*(cthis->associate))(cthis, unwrap(txn), unwrap(secondary),
	    (callback) ? _db_associate_intercept_c : NULL, flags));
}

DB_CALLBACK_C_INTERCEPT(feedback,
    void, (DB *cthis, int opcode, int pct),
    /* no return */ (void), (cxxthis, opcode, pct))

DB_SET_CALLBACK(set_feedback, feedback,
    (void (*arg)(Db *cxxthis, int opcode, int pct)), arg)

DB_CALLBACK_C_INTERCEPT(append_recno,
    int, (DB *cthis, DBT *data, db_recno_t recno),
    return, (cxxthis, Dbt::get_Dbt(data), recno))

DB_SET_CALLBACK(set_append_recno, append_recno,
    (int (*arg)(Db *cxxthis, Dbt *data, db_recno_t recno)), arg)

DB_CALLBACK_C_INTERCEPT(bt_compare,
    int, (DB *cthis, const DBT *data1, const DBT *data2),
    return,
    (cxxthis, Dbt::get_const_Dbt(data1), Dbt::get_const_Dbt(data2)))

DB_SET_CALLBACK(set_bt_compare, bt_compare,
    (int (*arg)(Db *cxxthis, const Dbt *data1, const Dbt *data2)), arg)

DB_CALLBACK_C_INTERCEPT(bt_prefix,
    size_t, (DB *cthis, const DBT *data1, const DBT *data2),
    return,
    (cxxthis, Dbt::get_const_Dbt(data1), Dbt::get_const_Dbt(data2)))

DB_SET_CALLBACK(set_bt_prefix, bt_prefix,
    (size_t (*arg)(Db *cxxthis, const Dbt *data1, const Dbt *data2)), arg)

DB_CALLBACK_C_INTERCEPT(dup_compare,
    int, (DB *cthis, const DBT *data1, const DBT *data2),
    return,
    (cxxthis, Dbt::get_const_Dbt(data1), Dbt::get_const_Dbt(data2)))

DB_SET_CALLBACK(set_dup_compare, dup_compare,
    (int (*arg)(Db *cxxthis, const Dbt *data1, const Dbt *data2)), arg)

DB_CALLBACK_C_INTERCEPT(h_hash,
    u_int32_t, (DB *cthis, const void *data, u_int32_t len),
    return, (cxxthis, data, len))

DB_SET_CALLBACK(set_h_hash, h_hash,
    (u_int32_t (*arg)(Db *cxxthis, const void *data, u_int32_t len)), arg)

// This is a 'glue' function declared as extern "C" so it will
// be compatible with picky compilers that do not allow mixing
// of function pointers to 'C' functions with function pointers
// to C++ functions.
//
extern "C"
int _verify_callback_c(void *handle, const void *str_arg)
{
	char *str;
	__DB_OSTREAMCLASS *out;

	str = (char *)str_arg;
	out = (__DB_OSTREAMCLASS *)handle;

	(*out) << str;
	if (out->fail())
		return (EIO);

	return (0);
}

int Db::verify(const char *name, const char *subdb,
	       __DB_OSTREAMCLASS *ostr, u_int32_t flags)
{
	DB *db = unwrap(this);
	int ret;

	if (!db)
		ret = EINVAL;
	else
		ret = __db_verify_internal(db, name, subdb, ostr,
		    _verify_callback_c, flags);

	if (!DB_RETOK_STD(ret))
		DB_ERROR("Db::verify", ret, error_policy());

	return (ret);
}

DB_METHOD(set_bt_compare, (bt_compare_fcn_type func),
    (db, func), DB_RETOK_STD)
DB_METHOD(set_bt_maxkey, (u_int32_t bt_maxkey),
    (db, bt_maxkey), DB_RETOK_STD)
DB_METHOD(set_bt_minkey, (u_int32_t bt_minkey),
    (db, bt_minkey), DB_RETOK_STD)
DB_METHOD(set_bt_prefix, (bt_prefix_fcn_type func),
    (db, func), DB_RETOK_STD)
DB_METHOD(set_dup_compare, (dup_compare_fcn_type func),
    (db, func), DB_RETOK_STD)
DB_METHOD(set_encrypt, (const char *passwd, int flags),
    (db, passwd, flags), DB_RETOK_STD)
DB_METHOD_VOID(set_errfile, (FILE *errfile), (db, errfile))
DB_METHOD_VOID(set_errpfx, (const char *errpfx), (db, errpfx))
DB_METHOD(set_flags, (u_int32_t flags), (db, flags),
    DB_RETOK_STD)
DB_METHOD(set_h_ffactor, (u_int32_t h_ffactor),
    (db, h_ffactor), DB_RETOK_STD)
DB_METHOD(set_h_hash, (h_hash_fcn_type func),
    (db, func), DB_RETOK_STD)
DB_METHOD(set_h_nelem, (u_int32_t h_nelem),
    (db, h_nelem), DB_RETOK_STD)
DB_METHOD(set_lorder, (int db_lorder), (db, db_lorder),
    DB_RETOK_STD)
DB_METHOD(set_pagesize, (u_int32_t db_pagesize),
    (db, db_pagesize), DB_RETOK_STD)
DB_METHOD(set_re_delim, (int re_delim),
    (db, re_delim), DB_RETOK_STD)
DB_METHOD(set_re_len, (u_int32_t re_len),
    (db, re_len), DB_RETOK_STD)
DB_METHOD(set_re_pad, (int re_pad),
    (db, re_pad), DB_RETOK_STD)
DB_METHOD(set_re_source, (char *re_source),
    (db, re_source), DB_RETOK_STD)
DB_METHOD(set_q_extentsize, (u_int32_t extentsize),
    (db, extentsize), DB_RETOK_STD)

DB_METHOD_QUIET(set_alloc, (db_malloc_fcn_type malloc_fcn,
    db_realloc_fcn_type realloc_fcn, db_free_fcn_type free_fcn),
    (db, malloc_fcn, realloc_fcn, free_fcn))

void Db::set_errcall(void (*arg)(const char *, char *))
{
	env_->set_errcall(arg);
}

void *Db::get_app_private() const
{
	return unwrapConst(this)->app_private;
}

void Db::set_app_private(void *value)
{
	unwrap(this)->app_private = value;
}

DB_METHOD(set_cachesize, (u_int32_t gbytes, u_int32_t bytes, int ncache),
    (db, gbytes, bytes, ncache), DB_RETOK_STD)
DB_METHOD(set_cache_priority, (DB_CACHE_PRIORITY priority),
    (db, priority), DB_RETOK_STD)

int Db::set_paniccall(void (*callback)(DbEnv *, int))
{
	return (env_->set_paniccall(callback));
}

void Db::set_error_stream(__DB_OSTREAMCLASS *error_stream)
{
	env_->set_error_stream(error_stream);
}
