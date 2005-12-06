/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: log_debug.c,v 1.5 2005/10/14 01:17:09 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"

static int __log_printf_int __P((DB_ENV *, DB_TXN *, const char *, va_list));

/*
 * __log_printf_capi --
 *	Write a printf-style format string into the DB log.
 *
 * PUBLIC: int __log_printf_capi __P((DB_ENV *, DB_TXN *, const char *, ...))
 * PUBLIC:    __attribute__ ((__format__ (__printf__, 3, 4)));
 */
int
#ifdef STDC_HEADERS
__log_printf_capi(DB_ENV *dbenv, DB_TXN *txnid, const char *fmt, ...)
#else
__log_printf_capi(dbenv, txnid, fmt, va_alist)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

#ifdef STDC_HEADERS
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = __log_printf_pp(dbenv, txnid, fmt, ap);
	va_end(ap);

	return (ret);
}

/*
 * __log_printf_pp --
 *	Handle the arguments and call an internal routine to do the work.
 *
 *	The reason this routine isn't just folded into __log_printf_capi
 *	is because the C++ API has to call a C API routine, and you can
 *	only pass variadic arguments to a single routine.
 *
 * PUBLIC: int __log_printf_pp
 * PUBLIC:     __P((DB_ENV *, DB_TXN *, const char *, va_list));
 */
int
__log_printf_pp(dbenv, txnid, fmt, ap)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	const char *fmt;
	va_list ap;
{
	DB_THREAD_INFO *ip;
	int rep_check, ret, t_ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_printf", DB_INIT_LOG);

	ENV_ENTER(dbenv, ip);
	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check && (ret = __env_rep_enter(dbenv, 0)) != 0)
		return (ret);

	ret = __log_printf_int(dbenv, txnid, fmt, ap);

	if (rep_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && (ret) == 0)
		ret = t_ret;
	va_end(ap);
	ENV_LEAVE(dbenv, ip);

	return (ret);
}

/*
 * __log_printf --
 *	Write a printf-style format string into the DB log.
 *
 * PUBLIC: int __log_printf __P((DB_ENV *, DB_TXN *, const char *, ...))
 * PUBLIC:    __attribute__ ((__format__ (__printf__, 3, 4)));
 */
int
#ifdef STDC_HEADERS
__log_printf(DB_ENV *dbenv, DB_TXN *txnid, const char *fmt, ...)
#else
__log_printf(dbenv, txnid, fmt, va_alist)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

#ifdef STDC_HEADERS
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = __log_printf_int(dbenv, txnid, fmt, ap);
	va_end(ap);

	return (ret);
}

/*
 * __log_printf_int --
 *	Write a printf-style format string into the DB log (internal).
 */
static int
__log_printf_int(dbenv, txnid, fmt, ap)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	const char *fmt;
	va_list ap;
{
	DBT opdbt, msgdbt;
	DB_LSN lsn;
	char __logbuf[2048];	/* !!!: END OF THE STACK DON'T TRUST SPRINTF. */

	if (!DBENV_LOGGING(dbenv)) {
		__db_err(dbenv, "Logging not currently permitted");
		return (EAGAIN);
	}

	memset(&opdbt, 0, sizeof(opdbt));
	opdbt.data = "DIAGNOSTIC";
	opdbt.size = sizeof("DIAGNOSTIC") - 1;

	memset(&msgdbt, 0, sizeof(msgdbt));
	msgdbt.data = __logbuf;
	msgdbt.size = (u_int32_t)vsnprintf(__logbuf, sizeof(__logbuf), fmt, ap);

	return (__db_debug_log(
	    dbenv, txnid, &lsn, 0, &opdbt, -1, &msgdbt, NULL, 0));
}
