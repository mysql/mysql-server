/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: log_method.c,v 12.4 2005/07/21 18:21:25 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/log.h"

/*
 * __log_dbenv_create --
 *	Log specific initialization of the DB_ENV structure.
 *
 * PUBLIC: void __log_dbenv_create __P((DB_ENV *));
 */
void
__log_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 */
	dbenv->lg_bsize = 0;
	dbenv->lg_regionmax = LG_BASE_REGION_SIZE;
}

/*
 * PUBLIC: int __log_get_lg_bsize __P((DB_ENV *, u_int32_t *));
 */
int
__log_get_lg_bsize(dbenv, lg_bsizep)
	DB_ENV *dbenv;
	u_int32_t *lg_bsizep;
{
	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->lg_handle, "DB_ENV->get_lg_bsize", DB_INIT_LOG);

	if (LOGGING_ON(dbenv)) {
		/* Cannot be set after open, no lock required to read. */
		*lg_bsizep = ((LOG *)
		    ((DB_LOG *)dbenv->lg_handle)->reginfo.primary)->buffer_size;
	} else
		*lg_bsizep = dbenv->lg_bsize;
	return (0);
}

/*
 * __log_set_lg_bsize --
 *	DB_ENV->set_lg_bsize.
 *
 * PUBLIC: int __log_set_lg_bsize __P((DB_ENV *, u_int32_t));
 */
int
__log_set_lg_bsize(dbenv, lg_bsize)
	DB_ENV *dbenv;
	u_int32_t lg_bsize;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_lg_bsize");

	dbenv->lg_bsize = lg_bsize;
	return (0);
}

/*
 * PUBLIC: int __log_get_lg_filemode __P((DB_ENV *, int *));
 */
int
__log_get_lg_filemode(dbenv, lg_modep)
	DB_ENV *dbenv;
	int *lg_modep;
{
	DB_LOG *dblp;

	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->lg_handle, "DB_ENV->get_lg_filemode", DB_INIT_LOG);

	if (LOGGING_ON(dbenv)) {
		dblp = dbenv->lg_handle;
		LOG_SYSTEM_LOCK(dbenv);
		*lg_modep = ((LOG *)dblp->reginfo.primary)->filemode;
		LOG_SYSTEM_UNLOCK(dbenv);
	} else
		*lg_modep = dbenv->lg_filemode;

	return (0);
}

/*
 * __log_set_lg_filemode --
 *	DB_ENV->set_lg_filemode.
 *
 * PUBLIC: int __log_set_lg_filemode __P((DB_ENV *, int));
 */
int
__log_set_lg_filemode(dbenv, lg_mode)
	DB_ENV *dbenv;
	int lg_mode;
{
	DB_LOG *dblp;
	LOG *lp;

	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->lg_handle, "DB_ENV->set_lg_filemode", DB_INIT_LOG);

	if (LOGGING_ON(dbenv)) {
		dblp = dbenv->lg_handle;
		lp = dblp->reginfo.primary;
		LOG_SYSTEM_LOCK(dbenv);
		lp->filemode = lg_mode;
		LOG_SYSTEM_UNLOCK(dbenv);
	} else
		dbenv->lg_filemode = lg_mode;

	return (0);
}

/*
 * PUBLIC: int __log_get_lg_max __P((DB_ENV *, u_int32_t *));
 */
int
__log_get_lg_max(dbenv, lg_maxp)
	DB_ENV *dbenv;
	u_int32_t *lg_maxp;
{
	DB_LOG *dblp;

	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->lg_handle, "DB_ENV->get_lg_max", DB_INIT_LOG);

	if (LOGGING_ON(dbenv)) {
		dblp = dbenv->lg_handle;
		LOG_SYSTEM_LOCK(dbenv);
		*lg_maxp = ((LOG *)dblp->reginfo.primary)->log_nsize;
		LOG_SYSTEM_UNLOCK(dbenv);
	} else
		*lg_maxp = dbenv->lg_size;

	return (0);
}

/*
 * __log_set_lg_max --
 *	DB_ENV->set_lg_max.
 *
 * PUBLIC: int __log_set_lg_max __P((DB_ENV *, u_int32_t));
 */
int
__log_set_lg_max(dbenv, lg_max)
	DB_ENV *dbenv;
	u_int32_t lg_max;
{
	DB_LOG *dblp;
	LOG *lp;
	int ret;

	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->lg_handle, "DB_ENV->set_lg_max", DB_INIT_LOG);

	if (LOGGING_ON(dbenv)) {
		if ((ret = __log_check_sizes(dbenv, lg_max, 0)) != 0)
			return (ret);
		dblp = dbenv->lg_handle;
		lp = dblp->reginfo.primary;
		LOG_SYSTEM_LOCK(dbenv);
		lp->log_nsize = lg_max;
		LOG_SYSTEM_UNLOCK(dbenv);
	} else
		dbenv->lg_size = lg_max;

	return (0);
}

/*
 * PUBLIC: int __log_get_lg_regionmax __P((DB_ENV *, u_int32_t *));
 */
int
__log_get_lg_regionmax(dbenv, lg_regionmaxp)
	DB_ENV *dbenv;
	u_int32_t *lg_regionmaxp;
{
	ENV_NOT_CONFIGURED(dbenv,
	    dbenv->lg_handle, "DB_ENV->get_lg_regionmax", DB_INIT_LOG);

	if (LOGGING_ON(dbenv)) {
		/* Cannot be set after open, no lock required to read. */
		*lg_regionmaxp = ((LOG *)
		    ((DB_LOG *)dbenv->lg_handle)->reginfo.primary)->regionmax;
	} else
		*lg_regionmaxp = dbenv->lg_regionmax;
	return (0);
}

/*
 * __log_set_lg_regionmax --
 *	DB_ENV->set_lg_regionmax.
 *
 * PUBLIC: int __log_set_lg_regionmax __P((DB_ENV *, u_int32_t));
 */
int
__log_set_lg_regionmax(dbenv, lg_regionmax)
	DB_ENV *dbenv;
	u_int32_t lg_regionmax;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "DB_ENV->set_lg_regionmax");

					/* Let's not be silly. */
	if (lg_regionmax != 0 && lg_regionmax < LG_BASE_REGION_SIZE) {
		__db_err(dbenv,
		    "log file size must be >= %d", LG_BASE_REGION_SIZE);
		return (EINVAL);
	}

	dbenv->lg_regionmax = lg_regionmax;
	return (0);
}

/*
 * PUBLIC: int __log_get_lg_dir __P((DB_ENV *, const char **));
 */
int
__log_get_lg_dir(dbenv, dirp)
	DB_ENV *dbenv;
	const char **dirp;
{
	*dirp = dbenv->db_log_dir;
	return (0);
}

/*
 * __log_set_lg_dir --
 *	DB_ENV->set_lg_dir.
 *
 * PUBLIC: int __log_set_lg_dir __P((DB_ENV *, const char *));
 */
int
__log_set_lg_dir(dbenv, dir)
	DB_ENV *dbenv;
	const char *dir;
{
	if (dbenv->db_log_dir != NULL)
		__os_free(dbenv, dbenv->db_log_dir);
	return (__os_strdup(dbenv, dir, &dbenv->db_log_dir));
}

/*
 * __log_get_flags --
 *	DB_ENV->get_flags.
 *
 * PUBLIC: void __log_get_flags __P((DB_ENV *, u_int32_t *));
 */
void
__log_get_flags(dbenv, flagsp)
	DB_ENV *dbenv;
	u_int32_t *flagsp;
{
	DB_LOG *dblp;
	LOG *lp;
	u_int32_t flags;

	if ((dblp = dbenv->lg_handle) == NULL)
		return;

	lp = dblp->reginfo.primary;

	flags = *flagsp;
	if (lp->db_log_autoremove)
		LF_SET(DB_LOG_AUTOREMOVE);
	else
		LF_CLR(DB_LOG_AUTOREMOVE);
	if (lp->db_log_inmemory)
		LF_SET(DB_LOG_INMEMORY);
	else
		LF_CLR(DB_LOG_INMEMORY);
	*flagsp = flags;
}

/*
 * __log_set_flags --
 *	DB_ENV->set_flags.
 *
 * PUBLIC: void __log_set_flags __P((DB_ENV *, u_int32_t, int));
 */
void
__log_set_flags(dbenv, flags, on)
	DB_ENV *dbenv;
	u_int32_t flags;
	int on;
{
	DB_LOG *dblp;
	LOG *lp;

	if ((dblp = dbenv->lg_handle) == NULL)
		return;

	lp = dblp->reginfo.primary;

	if (LF_ISSET(DB_LOG_AUTOREMOVE))
		lp->db_log_autoremove = on ? 1 : 0;
	if (LF_ISSET(DB_LOG_INMEMORY))
		lp->db_log_inmemory = on ? 1 : 0;
}

/*
 * __log_check_sizes --
 *	Makes sure that the log file size and log buffer size are compatible.
 *
 * PUBLIC: int __log_check_sizes __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__log_check_sizes(dbenv, lg_max, lg_bsize)
	DB_ENV *dbenv;
	u_int32_t lg_max;
	u_int32_t lg_bsize;
{
	LOG *lp;
	int inmem;

	if (LOGGING_ON(dbenv)) {
		lp = ((DB_LOG *)dbenv->lg_handle)->reginfo.primary;
		inmem = lp->db_log_inmemory;
		lg_bsize = lp->buffer_size;
	} else
		inmem = (F_ISSET(dbenv, DB_ENV_LOG_INMEMORY) != 0);

	if (inmem) {
		if (lg_bsize == 0)
			lg_bsize = LG_BSIZE_INMEM;
		if (lg_max == 0)
			lg_max = LG_MAX_INMEM;

		if (lg_bsize <= lg_max) {
			__db_err(dbenv,
		  "in-memory log buffer must be larger than the log file size");
			return (EINVAL);
		}
	}

	return (0);
}
