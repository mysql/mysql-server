/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_err.c,v 11.38 2001/01/22 21:50:25 sue Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_shash.h"
#include "lock.h"
#include "lock_ext.h"
#include "log.h"
#include "log_ext.h"
#include "mp.h"
#include "mp_ext.h"
#include "txn.h"
#include "txn_ext.h"
#include "clib_ext.h"
#include "common_ext.h"
#include "db_auto.h"

static void __db_errcall __P((const DB_ENV *, int, int, const char *, va_list));
static void __db_errfile __P((const DB_ENV *, int, int, const char *, va_list));

/*
 * __db_fchk --
 *	General flags checking routine.
 *
 * PUBLIC: int __db_fchk __P((DB_ENV *, const char *, u_int32_t, u_int32_t));
 */
int
__db_fchk(dbenv, name, flags, ok_flags)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags, ok_flags;
{
	return (LF_ISSET(~ok_flags) ? __db_ferr(dbenv, name, 0) : 0);
}

/*
 * __db_fcchk --
 *	General combination flags checking routine.
 *
 * PUBLIC: int __db_fcchk
 * PUBLIC:    __P((DB_ENV *, const char *, u_int32_t, u_int32_t, u_int32_t));
 */
int
__db_fcchk(dbenv, name, flags, flag1, flag2)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t flags, flag1, flag2;
{
	return (LF_ISSET(flag1) &&
	    LF_ISSET(flag2) ? __db_ferr(dbenv, name, 1) : 0);
}

/*
 * __db_ferr --
 *	Common flag errors.
 *
 * PUBLIC: int __db_ferr __P((const DB_ENV *, const char *, int));
 */
int
__db_ferr(dbenv, name, iscombo)
	const DB_ENV *dbenv;
	const char *name;
	int iscombo;
{
	__db_err(dbenv, "illegal flag %sspecified to %s",
	    iscombo ? "combination " : "", name);
	return (EINVAL);
}

/*
 * __db_pgerr --
 *	Error when unable to retrieve a specified page.
 *
 * PUBLIC: int __db_pgerr __P((DB *, db_pgno_t));
 */
int
__db_pgerr(dbp, pgno)
	DB *dbp;
	db_pgno_t pgno;
{
	/*
	 * Three things are certain:
	 * Death, taxes, and lost data.
	 * Guess which has occurred.
	 */
	__db_err(dbp->dbenv,
	    "unable to create/retrieve page %lu", (u_long)pgno);
	return (__db_panic(dbp->dbenv, EIO));
}

/*
 * __db_pgfmt --
 *	Error when a page has the wrong format.
 *
 * PUBLIC: int __db_pgfmt __P((DB *, db_pgno_t));
 */
int
__db_pgfmt(dbp, pgno)
	DB *dbp;
	db_pgno_t pgno;
{
	__db_err(dbp->dbenv,
	    "page %lu: illegal page type or format", (u_long)pgno);
	return (__db_panic(dbp->dbenv, EINVAL));
}

/*
 * __db_eopnotsup --
 *	Common operation not supported message.
 *
 * PUBLIC: int __db_eopnotsup __P((const DB_ENV *));
 */
int
__db_eopnotsup(dbenv)
	const DB_ENV *dbenv;
{
	__db_err(dbenv, "operation not supported");
#ifdef EOPNOTSUPP
	return (EOPNOTSUPP);
#else
	return (EINVAL);
#endif
}

#ifdef DIAGNOSTIC
/*
 * __db_assert --
 *	Error when an assertion fails.  Only checked if #DIAGNOSTIC defined.
 *
 * PUBLIC: #ifdef DIAGNOSTIC
 * PUBLIC: void __db_assert __P((const char *, const char *, int));
 * PUBLIC: #endif
 */
void
__db_assert(failedexpr, file, line)
	const char *failedexpr, *file;
	int line;
{
	(void)fprintf(stderr,
	    "__db_assert: \"%s\" failed: file \"%s\", line %d\n",
	    failedexpr, file, line);
	fflush(stderr);

	/* We want a stack trace of how this could possibly happen. */
	abort();

	/* NOTREACHED */
}
#endif

/*
 * __db_panic_msg --
 *	Just report that someone else paniced.
 *
 * PUBLIC: int __db_panic_msg __P((DB_ENV *));
 */
int
__db_panic_msg(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv, "region error detected; run recovery.");
	return (DB_RUNRECOVERY);
}

/*
 * __db_panic --
 *	Lock out the tree due to unrecoverable error.
 *
 * PUBLIC: int __db_panic __P((DB_ENV *, int));
 */
int
__db_panic(dbenv, errval)
	DB_ENV *dbenv;
	int errval;
{

	if (dbenv != NULL) {
		((REGENV *)((REGINFO *)dbenv->reginfo)->primary)->panic = 1;

		dbenv->db_panic = errval;

		__db_err(dbenv, "PANIC: %s", db_strerror(errval));

		if (dbenv->db_paniccall != NULL)
			dbenv->db_paniccall(dbenv, errval);
	}

	/*
	 * Chaos reigns within.
	 * Reflect, repent, and reboot.
	 * Order shall return.
	 */
	return (DB_RUNRECOVERY);
}

/*
 * db_strerror --
 *	ANSI C strerror(3) for DB.
 */
char *
db_strerror(error)
	int error;
{
	if (error == 0)
		return ("Successful return: 0");
	if (error > 0)
		return (strerror(error));

	/*
	 * !!!
	 * The Tcl API requires that some of these return strings be compared
	 * against strings stored in application scripts.  So, any of these
	 * errors that do not invariably result in a Tcl exception may not be
	 * altered.
	 */
	switch (error) {
	case DB_INCOMPLETE:
		return ("DB_INCOMPLETE: Cache flush was unable to complete");
	case DB_KEYEMPTY:
		return ("DB_KEYEMPTY: Non-existent key/data pair");
	case DB_KEYEXIST:
		return ("DB_KEYEXIST: Key/data pair already exists");
	case DB_LOCK_DEADLOCK:
		return
		    ("DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock");
	case DB_LOCK_NOTGRANTED:
		return ("DB_LOCK_NOTGRANTED: Lock not granted");
	case DB_NOSERVER:
		return ("DB_NOSERVER: Fatal error, no server");
	case DB_NOSERVER_HOME:
		return ("DB_NOSERVER_HOME: Home unrecognized at server");
	case DB_NOSERVER_ID:
		return ("DB_NOSERVER_ID: Identifier unrecognized at server");
	case DB_NOTFOUND:
		return ("DB_NOTFOUND: No matching key/data pair found");
	case DB_OLD_VERSION:
		return ("DB_OLDVERSION: Database requires a version upgrade");
	case DB_RUNRECOVERY:
		return ("DB_RUNRECOVERY: Fatal error, run database recovery");
	case DB_VERIFY_BAD:
		return ("DB_VERIFY_BAD: Database verification failed");
	default: {
		/*
		 * !!!
		 * Room for a 64-bit number + slop.  This buffer is only used
		 * if we're given an unknown error, which should never happen.
		 * Note, however, we're no longer thread-safe if it does.
		 */
		static char ebuf[40];

		(void)snprintf(ebuf, sizeof(ebuf), "Unknown error: %d", error);
		return (ebuf);
	}
	}
}

/*
 * __db_err --
 *	Standard DB error routine.  The same as db_errx, except that we
 *	don't write to stderr if no output mechanism was specified.
 *
 * PUBLIC: void __db_err __P((const DB_ENV *, const char *, ...));
 */
void
#ifdef __STDC__
__db_err(const DB_ENV *dbenv, const char *fmt, ...)
#else
__db_err(dbenv, fmt, va_alist)
	const DB_ENV *dbenv;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

/*
	XXX
	Log the message.

	It would be nice to automatically log the error into the log files
	if the application is configured for logging.  The problem is that
	if we currently hold the log region mutex, we will self-deadlock.
	Leave all the structure in place, but turned off.  I'd like to fix
	this in the future by detecting if we have the log region already
	locked (e.g., a flag in the environment handle), or perhaps even
	have a finer granularity so that the only calls to __db_err we
	can't log are those made while we have the current log buffer
	locked, or perhaps have a separate buffer into which we log error
	messages.

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	__db_real_log(dbenv, NULL, "db_err", 0, fmt, ap);

	va_end(ap);
#endif
*/

	/* Tell the application. */
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	__db_real_err(dbenv, 0, 0, 0, fmt, ap);

	va_end(ap);
}

/*
 * __db_real_err --
 *	All the DB error routines end up here.
 *
 * PUBLIC: void __db_real_err
 * PUBLIC:     __P((const DB_ENV *, int, int, int, const char *, va_list));
 */
void
__db_real_err(dbenv, error, error_set, stderr_default, fmt, ap)
	const DB_ENV *dbenv;
	int error, error_set, stderr_default;
	const char *fmt;
	va_list ap;
{
	/* Call the user's callback function, if specified. */
	if (dbenv != NULL && dbenv->db_errcall != NULL)
		__db_errcall(dbenv, error, error_set, fmt, ap);

	/* Write to the user's file descriptor, if specified. */
	if (dbenv != NULL && dbenv->db_errfile != NULL)
		__db_errfile(dbenv, error, error_set, fmt, ap);

	/*
	 * If we have a default and we didn't do either of the above, write
	 * to the default.
	 */
	if (stderr_default && (dbenv == NULL ||
	    (dbenv->db_errcall == NULL && dbenv->db_errfile == NULL)))
		__db_errfile(dbenv, error, error_set, fmt, ap);
}

/*
 * __db_errcall --
 *	Do the error message work for callback functions.
 */
static void
__db_errcall(dbenv, error, error_set, fmt, ap)
	const DB_ENV *dbenv;
	int error, error_set;
	const char *fmt;
	va_list ap;
{
	char *p;
	char __errbuf[2048];	/* !!!: END OF THE STACK DON'T TRUST SPRINTF. */

	p = __errbuf;
	if (fmt != NULL) {
		p += vsnprintf(__errbuf, sizeof(__errbuf), fmt, ap);
		if (error_set) {
			*p++ = ':';
			*p++ = ' ';
		}
	}
	if (error_set)
		(void)strcpy(p, db_strerror(error));

	dbenv->db_errcall(dbenv->db_errpfx, __errbuf);
}

/*
 * __db_errfile --
 *	Do the error message work for FILE *s.
 */
static void
__db_errfile(dbenv, error, error_set, fmt, ap)
	const DB_ENV *dbenv;
	int error, error_set;
	const char *fmt;
	va_list ap;
{
	FILE *fp;

	fp = dbenv == NULL ||
	    dbenv->db_errfile == NULL ? stderr : dbenv->db_errfile;

	if (dbenv != NULL && dbenv->db_errpfx != NULL)
		(void)fprintf(fp, "%s: ", dbenv->db_errpfx);
	if (fmt != NULL) {
		(void)vfprintf(fp, fmt, ap);
		if (error_set)
			(void)fprintf(fp, ": ");
	}
	if (error_set)
		(void)fprintf(fp, "%s", db_strerror(error));
	(void)fprintf(fp, "\n");
	(void)fflush(fp);
}

/*
 * __db_logmsg --
 *	Write information into the DB log.
 *
 * PUBLIC: void __db_logmsg __P((const DB_ENV *,
 * PUBLIC:     DB_TXN *, const char *, u_int32_t, const char *, ...));
 */
void
#ifdef __STDC__
__db_logmsg(const DB_ENV *dbenv,
    DB_TXN *txnid, const char *opname, u_int32_t flags, const char *fmt, ...)
#else
__db_logmsg(dbenv, txnid, opname, flags, fmt, va_alist)
	const DB_ENV *dbenv;
	DB_TXN *txnid;
	const char *opname, *fmt;
	u_int32_t flags;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	__db_real_log(dbenv, txnid, opname, flags, fmt, ap);

	va_end(ap);
}

/*
 * __db_real_log --
 *	Write information into the DB log.
 *
 * PUBLIC: void __db_real_log __P((const DB_ENV *,
 * PUBLIC:     DB_TXN *, const char *, u_int32_t, const char *, va_list ap));
 */
void
#ifdef __STDC__
__db_real_log(const DB_ENV *dbenv, DB_TXN *txnid,
    const char *opname, u_int32_t flags, const char *fmt, va_list ap)
#else
__db_real_log(dbenv, txnid, opname, flags, fmt, ap)
	const DB_ENV *dbenv;
	DB_TXN *txnid;
	const char *opname, *fmt;
	u_int32_t flags;
	va_list ap;
#endif
{
	DBT opdbt, msgdbt;
	DB_LSN lsn;
	char __logbuf[2048];	/* !!!: END OF THE STACK DON'T TRUST SPRINTF. */

	if (!LOGGING_ON(dbenv))
		return;

	memset(&opdbt, 0, sizeof(opdbt));
	opdbt.data = (void *)opname;
	opdbt.size = strlen(opname) + 1;

	memset(&msgdbt, 0, sizeof(msgdbt));
	msgdbt.data = __logbuf;
	msgdbt.size = vsnprintf(__logbuf, sizeof(__logbuf), fmt, ap);

	/*
	 * XXX
	 * Explicitly discard the const.  Otherwise, we have to const DB_ENV
	 * references throughout the logging subsystem.
	 */
	__db_debug_log(
	    (DB_ENV *)dbenv, txnid, &lsn, flags, &opdbt, -1, &msgdbt, NULL, 0);
}

/*
 * __db_unknown_flag -- report internal error
 *
 * PUBLIC: int __db_unknown_flag __P((DB_ENV *, char *, u_int32_t));
 */
int
__db_unknown_flag(dbenv, routine, flag)
	DB_ENV *dbenv;
	char *routine;
	u_int32_t flag;
{
	__db_err(dbenv, "%s: Unknown flag: 0x%x", routine, flag);
	DB_ASSERT(0);
	return (EINVAL);
}

/*
 * __db_unknown_type -- report internal error
 *
 * PUBLIC: int __db_unknown_type __P((DB_ENV *, char *, u_int32_t));
 */
int
__db_unknown_type(dbenv, routine, type)
	DB_ENV *dbenv;
	char *routine;
	u_int32_t type;
{
	__db_err(dbenv, "%s: Unknown db type: 0x%x", routine, type);
	DB_ASSERT(0);
	return (EINVAL);
}

#ifdef DIAGNOSTIC
/*
 * __db_missing_txn_err --
 *	Cannot combine operations with and without transactions.
 *
 * PUBLIC: #ifdef DIAGNOSTIC
 * PUBLIC: int __db_missing_txn_err __P((DB_ENV *));
 * PUBLIC: #endif
 */
int
__db_missing_txn_err(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv,
    "DB handle previously used in transaction, missing transaction handle.");
	return (EINVAL);
}
#endif
