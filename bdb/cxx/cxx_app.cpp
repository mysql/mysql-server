/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: cxx_app.cpp,v 11.38 2000/12/21 20:30:18 dda Exp $";
#endif /* not lint */

#include <errno.h>
#include <stdio.h>              // needed for set_error_stream
#include <string.h>

#include "db_cxx.h"
#include "cxx_int.h"

#include "db_int.h"
#include "common_ext.h"

// The reason for a static variable is that some structures
// (like Dbts) have no connection to any Db or DbEnv, so when
// errors occur in their methods, we must have some reasonable
// way to determine whether to throw or return errors.
//
// This variable is taken from flags whenever a DbEnv is constructed.
// Normally there is only one DbEnv per program, and even if not,
// there is typically a single policy of throwing or returning.
//
static int last_known_error_policy = ON_ERROR_UNKNOWN;

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbEnv                                   //
//                                                                    //
////////////////////////////////////////////////////////////////////////

ostream *DbEnv::error_stream_ = 0;

// _destroy_check is called when there is a user error in a
// destructor, specifically when close has not been called for an
// object (even if it was never opened).  If the DbEnv is being
// destroyed we cannot always use DbEnv::error_stream_, so we'll
// use cerr in that case.
//
void DbEnv::_destroy_check(const char *str, int isDbEnv)
{
	ostream *out;

	out = error_stream_;
	if (out == NULL || isDbEnv == 1)
		out = &cerr;

	(*out) << "DbEnv::_destroy_check: open " << str << " object destroyed\n";
}

// A truism for the DbEnv object is that there is a valid
// DB_ENV handle from the constructor until close().
// After the close, the DB_ENV handle is invalid and
// no operations are permitted on the DbEnv (other than
// destructor).  Leaving the DbEnv handle open and not
// doing a close is generally considered an error.
//
// We used to allow DbEnv objects to be closed and reopened.
// This implied always keeping a valid DB_ENV object, and
// coordinating the open objects between Db/DbEnv turned
// out to be overly complicated.  Now we do not allow this.

DbEnv::DbEnv(u_int32_t flags)
:	imp_(0)
,	construct_error_(0)
,	construct_flags_(flags)
,	tx_recover_callback_(0)
,	paniccall_callback_(0)
{
	int err;

	COMPQUIET(err, 0);
	if ((err = initialize(0)) != 0)
		DB_ERROR("DbEnv::DbEnv", err, error_policy());
}

DbEnv::DbEnv(DB_ENV *env, u_int32_t flags)
:	imp_(0)
,	construct_error_(0)
,	construct_flags_(flags)
,	tx_recover_callback_(0)
,	paniccall_callback_(0)
{
	int err;

	COMPQUIET(err, 0);
	if ((err = initialize(env)) != 0)
		DB_ERROR("DbEnv::DbEnv", err, error_policy());
}

// Note: if the user has not closed, we call _destroy_check
// to warn against this non-safe programming practice,
// and call close anyway.
//
DbEnv::~DbEnv()
{
	DB_ENV *env = unwrap(this);

	if (env != NULL) {
		_destroy_check("DbEnv", 1);
		(void)env->close(env, 0);

		// extra safety
		cleanup();
	}
}

// called by Db destructor when the DbEnv is owned by DB.
void DbEnv::cleanup()
{
	DB_ENV *env = unwrap(this);

	if (env != NULL) {
		env->cj_internal = 0;
		imp_ = 0;
	}
}

int DbEnv::close(u_int32_t flags)
{
	DB_ENV *env = unwrap(this);
	int err, init_err;

	COMPQUIET(init_err, 0);

	// after a close (no matter if success or failure),
	// the underlying DB_ENV object must not be accessed,
	// so we clean up in advance.
	//
	cleanup();

	// It's safe to throw an error after the close,
	// since our error mechanism does not peer into
	// the DB* structures.
	//
	if ((err = env->close(env, flags)) != 0) {
		DB_ERROR("DbEnv::close", err, error_policy());
	}
	return (err);
}

void DbEnv::err(int error, const char *format, ...)
{
	va_list args;
	DB_ENV *env = unwrap(this);

	va_start(args, format);
	__db_real_err(env, error, 1, 1, format, args);
	va_end(args);
}

void DbEnv::errx(const char *format, ...)
{
	va_list args;
	DB_ENV *env = unwrap(this);

	va_start(args, format);
	__db_real_err(env, 0, 0, 1, format, args);
	va_end(args);
}

// used internally during constructor
// to associate an existing DB_ENV with this DbEnv,
// or create a new one.  If there is an error,
// construct_error_ is set; this is examined during open.
//
int DbEnv::initialize(DB_ENV *env)
{
	int err;

	last_known_error_policy = error_policy();

	if (env == 0) {
		// Create a new DB_ENV environment.
		if ((err = ::db_env_create(&env,
			construct_flags_ & ~DB_CXX_NO_EXCEPTIONS)) != 0) {
			construct_error_ = err;
			return (err);
		}
	}
	imp_ = wrap(env);
	env->cj_internal = this;    // for DB_ENV* to DbEnv* conversion
	return (0);
}

// Return a tristate value corresponding to whether we should
// throw exceptions on errors:
//   ON_ERROR_RETURN
//   ON_ERROR_THROW
//   ON_ERROR_UNKNOWN
//
int DbEnv::error_policy()
{
	if ((construct_flags_ & DB_CXX_NO_EXCEPTIONS) != 0) {
		return (ON_ERROR_RETURN);
	}
	else {
		return (ON_ERROR_THROW);
	}
}

// If an error occurred during the constructor, report it now.
// Otherwise, call the underlying DB->open method.
//
int DbEnv::open(const char *db_home, u_int32_t flags, int mode)
{
	DB_ENV *env = unwrap(this);
	int err;

	if ((err = construct_error_) != 0)
		DB_ERROR("Db::open", err, error_policy());
	else if ((err = env->open(env, db_home, flags, mode)) != 0)
		DB_ERROR("DbEnv::open", err, error_policy());

	return (err);
}

int DbEnv::remove(const char *db_home, u_int32_t flags)
{
	DB_ENV *env;
	int ret;

	env = unwrap(this);

	// after a remove (no matter if success or failure),
	// the underlying DB_ENV object must not be accessed,
	// so we clean up in advance.
	//
	cleanup();

	if ((ret = env->remove(env, db_home, flags)) != 0)
		DB_ERROR("DbEnv::remove", ret, error_policy());

	return (ret);
}

// Report an error associated with the DbEnv.
// error_policy is one of:
//   ON_ERROR_THROW     throw an error
//   ON_ERROR_RETURN    do nothing here, the caller will return an error
//   ON_ERROR_UNKNOWN   defer the policy to policy saved in DbEnv::DbEnv
//
void DbEnv::runtime_error(const char *caller, int error, int error_policy)
{
	if (error_policy == ON_ERROR_UNKNOWN)
		error_policy = last_known_error_policy;
	if (error_policy == ON_ERROR_THROW) {
		// Creating and throwing the object in two separate
		// statements seems to be necessary for HP compilers.
		DbException except(caller, error);
		throw except;
	}
}

// static method
char *DbEnv::strerror(int error)
{
	return (db_strerror(error));
}

// This is a 'glue' function declared as extern "C" so it will
// be compatible with picky compilers that do not allow mixing
// of function pointers to 'C' functions with function pointers
// to C++ functions.
//
extern "C"
void _stream_error_function_c(const char *prefix, char *message)
{
	DbEnv::_stream_error_function(prefix, message);
}

void DbEnv::_stream_error_function(const char *prefix, char *message)
{
	// HP compilers need the extra casts, we don't know why.
	if (error_stream_) {
		if (prefix) {
			(*error_stream_) << prefix << (const char *)": ";
		}
		if (message) {
			(*error_stream_) << (const char *)message;
		}
		(*error_stream_) << (const char *)"\n";
	}
}

// Note: This actually behaves a bit like a static function,
// since DB_ENV.db_errcall has no information about which
// db_env triggered the call.  A user that has multiple DB_ENVs
// will simply not be able to have different streams for each one.
//
void DbEnv::set_error_stream(ostream *stream)
{
	DB_ENV *dbenv = unwrap(this);

	error_stream_ = stream;
	dbenv->set_errcall(dbenv, (stream == 0) ? 0 :
			   _stream_error_function_c);
}

// static method
char *DbEnv::version(int *major, int *minor, int *patch)
{
	return (db_version(major, minor, patch));
}

// This is a variant of the DB_WO_ACCESS macro to define a simple set_
// method calling the underlying C method, but unlike a simple
// set method, it may return an error or raise an exception.
// Note this macro expects that input _argspec is an argument
// list element (e.g. "char *arg") defined in terms of "arg".
//
#define	DB_DBENV_ACCESS(_name, _argspec)                       \
							       \
int DbEnv::set_##_name(_argspec)                               \
{                                                              \
	int ret;                                               \
	DB_ENV *dbenv = unwrap(this);                          \
							       \
	if ((ret = (*(dbenv->set_##_name))(dbenv, arg)) != 0) {\
		DB_ERROR("DbEnv::set_" # _name, ret, error_policy()); \
	}                                                      \
	return (ret);                                          \
}

#define	DB_DBENV_ACCESS_NORET(_name, _argspec)                 \
							       \
void DbEnv::set_##_name(_argspec)                              \
{                                                              \
	DB_ENV *dbenv = unwrap(this);                          \
							       \
	(*(dbenv->set_##_name))(dbenv, arg);                   \
	return;                                                \
}

DB_DBENV_ACCESS_NORET(errfile, FILE *arg)
DB_DBENV_ACCESS_NORET(errpfx, const char *arg)

// We keep these alphabetical by field name,
// for comparison with Java's list.
//
DB_DBENV_ACCESS(data_dir, const char *arg)
DB_DBENV_ACCESS(lg_bsize, u_int32_t arg)
DB_DBENV_ACCESS(lg_dir, const char *arg)
DB_DBENV_ACCESS(lg_max, u_int32_t arg)
DB_DBENV_ACCESS(lk_detect, u_int32_t arg)
DB_DBENV_ACCESS(lk_max, u_int32_t arg)
DB_DBENV_ACCESS(lk_max_lockers, u_int32_t arg)
DB_DBENV_ACCESS(lk_max_locks, u_int32_t arg)
DB_DBENV_ACCESS(lk_max_objects, u_int32_t arg)
DB_DBENV_ACCESS(mp_mmapsize, size_t arg)
DB_DBENV_ACCESS(mutexlocks, int arg)
DB_DBENV_ACCESS(tmp_dir, const char *arg)
DB_DBENV_ACCESS(tx_max, u_int32_t arg)

// Here are the set methods that don't fit the above mold.
//
extern "C" {
	typedef void (*db_errcall_fcn_type)
		(const char *, char *);
};

void DbEnv::set_errcall(void (*arg)(const char *, char *))
{
	DB_ENV *dbenv = unwrap(this);

	// XXX
	// We are casting from a function ptr declared with C++
	// linkage to one (same arg types) declared with C
	// linkage.  It's hard to imagine a pair of C/C++
	// compilers from the same vendor for which this
	// won't work.  Unfortunately, we can't use a
	// intercept function like the others since the
	// function does not have a (DbEnv*) as one of
	// the args.  If this causes trouble, we can pull
	// the same trick we use in Java, namely stuffing
	// a (DbEnv*) pointer into the prefix.  We're
	// avoiding this for the moment because it obfuscates.
	//
	(*(dbenv->set_errcall))(dbenv, (db_errcall_fcn_type)arg);
}

int DbEnv::set_cachesize(u_int32_t gbytes, u_int32_t bytes, int ncache)
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	if ((ret =
	    (*(dbenv->set_cachesize))(dbenv, gbytes, bytes, ncache)) != 0)
		DB_ERROR("DbEnv::set_cachesize", ret, error_policy());

	return (ret);
}

int DbEnv::set_flags(u_int32_t flags, int onoff)
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	if ((ret = (dbenv->set_flags)(dbenv, flags, onoff)) != 0)
		DB_ERROR("DbEnv::set_flags", ret, error_policy());

	return (ret);
}

int DbEnv::set_lk_conflicts(u_int8_t *lk_conflicts, int lk_max)
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	if ((ret = (*(dbenv->set_lk_conflicts))
	     (dbenv, lk_conflicts, lk_max)) != 0)
		DB_ERROR("DbEnv::set_lk_conflicts", ret, error_policy());

	return (ret);
}

// static method
int DbEnv::set_pageyield(int arg)
{
	int ret;

	if ((ret = db_env_set_pageyield(arg)) != 0)
		DB_ERROR("DbEnv::set_pageyield", ret, last_known_error_policy);

	return (ret);
}

// static method
int DbEnv::set_panicstate(int arg)
{
	int ret;

	if ((ret = db_env_set_panicstate(arg)) != 0)
		DB_ERROR("DbEnv::set_panicstate", ret, last_known_error_policy);

	return (ret);
}

// static method
int DbEnv::set_region_init(int arg)
{
	int ret;

	if ((ret = db_env_set_region_init(arg)) != 0)
		DB_ERROR("DbEnv::set_region_init", ret, last_known_error_policy);

	return (ret);
}

int DbEnv::set_server(char *host, long tsec, long ssec, u_int32_t flags)
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	if ((ret = dbenv->set_server(dbenv, host, tsec, ssec, flags)) != 0)
		DB_ERROR("DbEnv::set_server", ret, error_policy());

	return (ret);
}

int DbEnv::set_shm_key(long shm_key)
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	if ((ret = dbenv->set_shm_key(dbenv, shm_key)) != 0)
		DB_ERROR("DbEnv::set_shm_key", ret, error_policy());

	return (ret);
}

// static method
int DbEnv::set_tas_spins(u_int32_t arg)
{
	int ret;

	if ((ret = db_env_set_tas_spins(arg)) != 0)
		DB_ERROR("DbEnv::set_tas_spins", ret, last_known_error_policy);

	return (ret);
}

int DbEnv::set_verbose(u_int32_t which, int onoff)
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	if ((ret = (*(dbenv->set_verbose))(dbenv, which, onoff)) != 0)
		DB_ERROR("DbEnv::set_verbose", ret, error_policy());

	return (ret);
}

// This is a 'glue' function declared as extern "C" so it will
// be compatible with picky compilers that do not allow mixing
// of function pointers to 'C' functions with function pointers
// to C++ functions.
//
extern "C"
int _tx_recover_intercept_c(DB_ENV *env, DBT *dbt,
			    DB_LSN *lsn, db_recops op)
{
	return (DbEnv::_tx_recover_intercept(env, dbt, lsn, op));
}

int DbEnv::_tx_recover_intercept(DB_ENV *env, DBT *dbt,
				DB_LSN *lsn, db_recops op)
{
	if (env == 0) {
		DB_ERROR("DbEnv::tx_recover_callback", EINVAL, ON_ERROR_UNKNOWN);
		return (EINVAL);
	}
	DbEnv *cxxenv = (DbEnv *)env->cj_internal;
	if (cxxenv == 0) {
		DB_ERROR("DbEnv::tx_recover_callback", EINVAL, ON_ERROR_UNKNOWN);
		return (EINVAL);
	}
	if (cxxenv->tx_recover_callback_ == 0) {
		DB_ERROR("DbEnv::tx_recover_callback", EINVAL, cxxenv->error_policy());
		return (EINVAL);
	}
	Dbt *cxxdbt = (Dbt *)dbt;
	DbLsn *cxxlsn = (DbLsn *)lsn;
	return ((*cxxenv->tx_recover_callback_)(cxxenv, cxxdbt, cxxlsn, op));
}

int DbEnv::set_tx_recover
    (int (*arg)(DbEnv *, Dbt *, DbLsn *, db_recops))
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	tx_recover_callback_ = arg;
	if ((ret =
	    (*(dbenv->set_tx_recover))(dbenv, _tx_recover_intercept_c)) != 0)
		DB_ERROR("DbEnv::set_tx_recover", ret, error_policy());

	return (ret);
}

int DbEnv::set_tx_timestamp(time_t *timestamp)
{
	int ret;
	DB_ENV *dbenv = unwrap(this);

	if ((ret = dbenv->set_tx_timestamp(dbenv, timestamp)) != 0)
		DB_ERROR("DbEnv::set_tx_timestamp", ret, error_policy());

	return (ret);
}

// This is a 'glue' function declared as extern "C" so it will
// be compatible with picky compilers that do not allow mixing
// of function pointers to 'C' functions with function pointers
// to C++ functions.
//
extern "C"
void _paniccall_intercept_c(DB_ENV *env, int errval)
{
	DbEnv::_paniccall_intercept(env, errval);
}

void DbEnv::_paniccall_intercept(DB_ENV *env, int errval)
{
	if (env == 0) {
		DB_ERROR("DbEnv::paniccall_callback", EINVAL, ON_ERROR_UNKNOWN);
	}
	DbEnv *cxxenv = (DbEnv *)env->cj_internal;
	if (cxxenv == 0) {
		DB_ERROR("DbEnv::paniccall_callback", EINVAL, ON_ERROR_UNKNOWN);
	}
	if (cxxenv->paniccall_callback_ == 0) {
		DB_ERROR("DbEnv::paniccall_callback", EINVAL, cxxenv->error_policy());
	}
	(*cxxenv->paniccall_callback_)(cxxenv, errval);
}

int DbEnv::set_paniccall(void (*arg)(DbEnv *, int))
{
	DB_ENV *dbenv = unwrap(this);

	paniccall_callback_ = arg;

	return ((*(dbenv->set_paniccall))(dbenv, _paniccall_intercept_c));
}

// This is a 'glue' function declared as extern "C" so it will
// be compatible with picky compilers that do not allow mixing
// of function pointers to 'C' functions with function pointers
// to C++ functions.
//
extern "C"
int _recovery_init_intercept_c(DB_ENV *env)
{
	return (DbEnv::_recovery_init_intercept(env));
}

int DbEnv::_recovery_init_intercept(DB_ENV *env)
{
	if (env == 0) {
		DB_ERROR("DbEnv::recovery_init_callback", EINVAL,
			 ON_ERROR_UNKNOWN);
	}
	DbEnv *cxxenv = (DbEnv *)env->cj_internal;
	if (cxxenv == 0) {
		DB_ERROR("DbEnv::recovery_init_callback", EINVAL,
			 ON_ERROR_UNKNOWN);
	}
	if (cxxenv->recovery_init_callback_ == 0) {
		DB_ERROR("DbEnv::recovery_init_callback", EINVAL,
			 cxxenv->error_policy());
	}
	return ((*cxxenv->recovery_init_callback_)(cxxenv));
}

int DbEnv::set_recovery_init(int (*arg)(DbEnv *))
{
	DB_ENV *dbenv = unwrap(this);

	recovery_init_callback_ = arg;

	return ((*(dbenv->set_recovery_init))(dbenv, _recovery_init_intercept_c));
}

// This is a 'glue' function declared as extern "C" so it will
// be compatible with picky compilers that do not allow mixing
// of function pointers to 'C' functions with function pointers
// to C++ functions.
//
extern "C"
void _feedback_intercept_c(DB_ENV *env, int opcode, int pct)
{
	DbEnv::_feedback_intercept(env, opcode, pct);
}

void DbEnv::_feedback_intercept(DB_ENV *env, int opcode, int pct)
{
	if (env == 0) {
		DB_ERROR("DbEnv::feedback_callback", EINVAL, ON_ERROR_UNKNOWN);
		return;
	}
	DbEnv *cxxenv = (DbEnv *)env->cj_internal;
	if (cxxenv == 0) {
		DB_ERROR("DbEnv::feedback_callback", EINVAL, ON_ERROR_UNKNOWN);
		return;
	}
	if (cxxenv->feedback_callback_ == 0) {
		DB_ERROR("DbEnv::feedback_callback", EINVAL,
			 cxxenv->error_policy());
		return;
	}
	(*cxxenv->feedback_callback_)(cxxenv, opcode, pct);
}

int DbEnv::set_feedback(void (*arg)(DbEnv *, int, int))
{
	DB_ENV *dbenv = unwrap(this);

	feedback_callback_ = arg;

	return ((*(dbenv->set_feedback))(dbenv, _feedback_intercept_c));
}
