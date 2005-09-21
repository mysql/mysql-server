/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: debug.h,v 11.44 2004/09/24 00:43:18 bostic Exp $
 */

#ifndef _DB_DEBUG_H_
#define	_DB_DEBUG_H_

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Turn on additional error checking in gcc 3.X.
 */
#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5)
#define	__attribute__(s)
#endif

/*
 * When running with #DIAGNOSTIC defined, we smash memory and do memory
 * guarding with a special byte value.
 */
#define	CLEAR_BYTE	0xdb
#define	GUARD_BYTE	0xdc

/*
 * DB assertions.
 *
 * Use __STDC__ rather than STDC_HEADERS, the #e construct is ANSI C specific.
 */
#if defined(__STDC__) && defined(DIAGNOSTIC)
#define	DB_ASSERT(e)	((e) ? (void)0 : __db_assert(#e, __FILE__, __LINE__))
#else
#define	DB_ASSERT(e)
#endif

/*
 * "Shut that bloody compiler up!"
 *
 * Unused, or not-used-yet variable.  We need to write and then read the
 * variable, some compilers are too bloody clever by half.
 */
#define	COMPQUIET(n, v)							\
	(n) = (v);							\
	(n) = (n)

/*
 * Purify and other run-time tools complain about uninitialized reads/writes
 * of structure fields whose only purpose is padding, as well as when heap
 * memory that was never initialized is written to disk.
 */
#ifdef	UMRW
#define	UMRW_SET(v)	(v) = 0
#else
#define	UMRW_SET(v)
#endif

/*
 * Message handling.  Use a macro instead of a function because va_list
 * references to variadic arguments cannot be reset to the beginning of the
 * variadic argument list (and then rescanned), by functions other than the
 * original routine that took the variadic list of arguments.
 */
#if defined(STDC_HEADERS) || defined(__cplusplus)
#define	DB_REAL_ERR(env, error, error_set, default_stream, fmt) {	\
	va_list ap;							\
									\
	/* Call the application's callback function, if specified. */	\
	va_start(ap, fmt);						\
	if ((env) != NULL && (env)->db_errcall != NULL)			\
		__db_errcall(env, error, error_set, fmt, ap);		\
	va_end(ap);							\
									\
	/* Write to the application's file descriptor, if specified. */\
	va_start(ap, fmt);						\
	if ((env) != NULL && (env)->db_errfile != NULL)			\
		__db_errfile(env, error, error_set, fmt, ap);		\
	va_end(ap);							\
									\
	/*								\
	 * If we have a default and we didn't do either of the above,	\
	 * write to the default.					\
	 */								\
	va_start(ap, fmt);						\
	if ((default_stream) && ((env) == NULL ||			\
	    ((env)->db_errcall == NULL && (env)->db_errfile == NULL)))	\
		__db_errfile(env, error, error_set, fmt, ap);		\
	va_end(ap);							\
}
#else
#define	DB_REAL_ERR(env, error, error_set, default_stream, fmt) {	\
	va_list ap;							\
									\
	/* Call the application's callback function, if specified. */	\
	va_start(ap);							\
	if ((env) != NULL && (env)->db_errcall != NULL)			\
		__db_errcall(env, error, error_set, fmt, ap);		\
	va_end(ap);							\
									\
	/* Write to the application's file descriptor, if specified. */\
	va_start(ap);							\
	if ((env) != NULL && (env)->db_errfile != NULL)			\
		__db_errfile(env, error, error_set, fmt, ap);		\
	va_end(ap);							\
									\
	/*								\
	 * If we have a default and we didn't do either of the above,	\
	 * write to the default.					\
	 */								\
	va_start(ap);							\
	if ((default_stream) && ((env) == NULL ||			\
	    ((env)->db_errcall == NULL && (env)->db_errfile == NULL)))	\
		__db_errfile(env, error, error_set, fmt, ap);		\
	va_end(ap);							\
}
#endif
#if defined(STDC_HEADERS) || defined(__cplusplus)
#define	DB_REAL_MSG(env, fmt) {						\
	va_list ap;							\
									\
	/* Call the application's callback function, if specified. */	\
	va_start(ap, fmt);						\
	if ((env) != NULL && (env)->db_msgcall != NULL)			\
		__db_msgcall(env, fmt, ap);				\
	va_end(ap);							\
									\
	/*								\
	 * If the application specified a file descriptor, or we wrote	\
	 * to neither the application's callback routine or to its file	\
	 * descriptor, write to stdout.					\
	 */								\
	va_start(ap, fmt);						\
	if ((env) == NULL ||						\
	    (env)->db_msgfile != NULL || (env)->db_msgcall == NULL) {	\
		__db_msgfile(env, fmt, ap);				\
	}								\
	va_end(ap);							\
}
#else
#define	DB_REAL_MSG(env, fmt) {						\
	va_list ap;							\
									\
	/* Call the application's callback function, if specified. */	\
	va_start(ap);							\
	if ((env) != NULL && (env)->db_msgcall != NULL)			\
		__db_msgcall(env, fmt, ap);				\
	va_end(ap);							\
									\
	/*								\
	 * If the application specified a file descriptor, or we wrote	\
	 * to neither the application's callback routine or to its file	\
	 * descriptor, write to stdout.					\
	 */								\
	va_start(ap);							\
	if ((env) == NULL ||						\
	    (env)->db_msgfile != NULL || (env)->db_msgcall == NULL) {	\
		__db_msgfile(env, fmt, ap);				\
	}								\
	va_end(ap);							\
}
#endif

/*
 * Debugging macro to log operations.
 *	If DEBUG_WOP is defined, log operations that modify the database.
 *	If DEBUG_ROP is defined, log operations that read the database.
 *
 * D dbp
 * T txn
 * O operation (string)
 * K key
 * A data
 * F flags
 */
#define	LOG_OP(C, T, O, K, A, F) {					\
	DB_LSN __lsn;							\
	DBT __op;							\
	if (DBC_LOGGING((C))) {						\
		memset(&__op, 0, sizeof(__op));				\
		__op.data = O;						\
		__op.size = strlen(O) + 1;				\
		(void)__db_debug_log((C)->dbp->dbenv, T, &__lsn, 0,	\
		    &__op, (C)->dbp->log_filename->id, K, A, F);	\
	}								\
}
#ifdef	DEBUG_ROP
#define	DEBUG_LREAD(C, T, O, K, A, F)	LOG_OP(C, T, O, K, A, F)
#else
#define	DEBUG_LREAD(C, T, O, K, A, F)
#endif
#ifdef	DEBUG_WOP
#define	DEBUG_LWRITE(C, T, O, K, A, F)	LOG_OP(C, T, O, K, A, F)
#else
#define	DEBUG_LWRITE(C, T, O, K, A, F)
#endif

/*
 * Hook for testing recovery at various places in the create/delete paths.
 * Hook for testing subdb locks.
 */
#if CONFIG_TEST
#define	DB_TEST_SUBLOCKS(env, flags) do {				\
	if ((env)->test_abort == DB_TEST_SUBDB_LOCKS)			\
		(flags) |= DB_LOCK_NOWAIT;				\
} while (0)

#define	DB_ENV_TEST_RECOVERY(env, val, ret, name) do {			\
	int __ret;							\
	PANIC_CHECK((env));						\
	if ((env)->test_copy == (val)) {				\
		/* COPY the FILE */					\
		if ((__ret = __db_testcopy((env), NULL, (name))) != 0)	\
			(ret) = __db_panic((env), __ret);		\
	}								\
	if ((env)->test_abort == (val)) {				\
		/* ABORT the TXN */					\
		(env)->test_abort = 0;					\
		(ret) = EINVAL;						\
		goto db_tr_err;						\
	}								\
} while (0)

#define	DB_TEST_RECOVERY(dbp, val, ret, name) do {			\
	int __ret;							\
	PANIC_CHECK((dbp)->dbenv);					\
	if ((dbp)->dbenv->test_copy == (val)) {				\
		/* Copy the file. */					\
		if (F_ISSET((dbp),					\
		    DB_AM_OPEN_CALLED) && (dbp)->mpf != NULL)		\
			(void)__db_sync(dbp);				\
		if ((__ret =						\
		    __db_testcopy((dbp)->dbenv, (dbp), (name))) != 0)	\
			(ret) = __db_panic((dbp)->dbenv, __ret);	\
	}								\
	if ((dbp)->dbenv->test_abort == (val)) {			\
		/* Abort the transaction. */				\
		(dbp)->dbenv->test_abort = 0;				\
		(ret) = EINVAL;						\
		goto db_tr_err;						\
	}								\
} while (0)

#define	DB_TEST_RECOVERY_LABEL	db_tr_err:

#define	DB_TEST_CHECKPOINT(env, val)					\
	if ((val) != 0)							\
		__os_sleep((env), (u_long)(val), 0)
#else
#define	DB_TEST_SUBLOCKS(env, flags)
#define	DB_ENV_TEST_RECOVERY(env, val, ret, name)
#define	DB_TEST_RECOVERY(dbp, val, ret, name)
#define	DB_TEST_RECOVERY_LABEL
#define	DB_TEST_CHECKPOINT(env, val)
#endif

#if defined(__cplusplus)
}
#endif
#endif /* !_DB_DEBUG_H_ */
