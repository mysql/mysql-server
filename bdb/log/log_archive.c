/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log_archive.c,v 11.39 2002/08/06 05:00:31 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/log.h"
#include "dbinc/qam.h"
#include "dbinc/txn.h"

static int __absname __P((DB_ENV *, char *, char *, char **));
static int __build_data __P((DB_ENV *, char *, char ***));
static int __cmpfunc __P((const void *, const void *));
static int __usermem __P((DB_ENV *, char ***));

/*
 * __log_archive --
 *	Supporting function for db_archive(1).
 *
 * PUBLIC: int __log_archive __P((DB_ENV *, char **[], u_int32_t));
 */
int
__log_archive(dbenv, listp, flags)
	DB_ENV *dbenv;
	char ***listp;
	u_int32_t flags;
{
	DBT rec;
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN stable_lsn;
	__txn_ckp_args *ckp_args;
	char **array, **arrayp, *name, *p, *pref, buf[MAXPATHLEN];
	int array_size, db_arch_abs, n, ret;
	u_int32_t fnum;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lg_handle, "DB_ENV->log_archive", DB_INIT_LOG);

	name = NULL;
	dblp = dbenv->lg_handle;
	COMPQUIET(fnum, 0);

#define	OKFLAGS	(DB_ARCH_ABS | DB_ARCH_DATA | DB_ARCH_LOG)
	if (flags != 0) {
		if ((ret = __db_fchk(
		    dbenv, "DB_ENV->log_archive", flags, OKFLAGS)) != 0)
			return (ret);
		if ((ret = __db_fcchk(dbenv, "DB_ENV->log_archive",
		    flags, DB_ARCH_DATA, DB_ARCH_LOG)) != 0)
			return (ret);
	}

	if (LF_ISSET(DB_ARCH_ABS)) {
		db_arch_abs = 1;
		LF_CLR(DB_ARCH_ABS);
	} else
		db_arch_abs = 0;

	if (flags == 0 || flags == DB_ARCH_DATA)
		ENV_REQUIRES_CONFIG(dbenv,
		    dbenv->tx_handle, "DB_ENV->log_archive", DB_INIT_TXN);

	/*
	 * Get the absolute pathname of the current directory.  It would
	 * be nice to get the shortest pathname of the database directory,
	 * but that's just not possible.
	 *
	 * XXX
	 * Can't trust getcwd(3) to set a valid errno.  If it doesn't, just
	 * guess that we ran out of memory.
	 */
	if (db_arch_abs) {
		__os_set_errno(0);
		if ((pref = getcwd(buf, sizeof(buf))) == NULL) {
			if (__os_get_errno() == 0)
				__os_set_errno(ENOMEM);
			return (__os_get_errno());
		}
	} else
		pref = NULL;

	switch (flags) {
	case DB_ARCH_DATA:
		return (__build_data(dbenv, pref, listp));
	case DB_ARCH_LOG:
		memset(&rec, 0, sizeof(rec));
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			return (ret);
#ifdef UMRW
		ZERO_LSN(stable_lsn);
#endif
		ret = logc->get(logc, &stable_lsn, &rec, DB_LAST);
		(void)logc->close(logc, 0);
		if (ret != 0)
			return (ret);
		fnum = stable_lsn.file;
		break;
	case 0:
		memset(&rec, 0, sizeof(rec));
		if (__txn_getckp(dbenv, &stable_lsn) != 0) {
			/*
			 * A failure return means that there's no checkpoint
			 * in the log (so we are not going to be deleting
			 * any log files).
			 */
			*listp = NULL;
			return (0);
		}
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			return (ret);
		if ((ret = logc->get(logc, &stable_lsn, &rec, DB_SET)) != 0 ||
		    (ret = __txn_ckp_read(dbenv, rec.data, &ckp_args)) != 0) {
			/*
			 * A return of DB_NOTFOUND may only mean that the
			 * checkpoint LSN is before the beginning of the
			 * log files that we still have.  This is not
			 * an error;  it just means our work is done.
			 */
			if (ret == DB_NOTFOUND) {
				*listp = NULL;
				ret = 0;
			}
			(void)logc->close(logc, 0);
			return (ret);
		}
		if ((ret = logc->close(logc, 0)) != 0)
			return (ret);
		stable_lsn = ckp_args->ckp_lsn;
		__os_free(dbenv, ckp_args);

		/* Remove any log files before the last stable LSN. */
		fnum = stable_lsn.file - 1;
		break;
	}

#define	LIST_INCREMENT	64
	/* Get some initial space. */
	array_size = 64;
	if ((ret = __os_malloc(dbenv,
	    sizeof(char *) * array_size, &array)) != 0)
		return (ret);
	array[0] = NULL;

	/* Build an array of the file names. */
	for (n = 0; fnum > 0; --fnum) {
		if ((ret = __log_name(dblp, fnum, &name, NULL, 0)) != 0)
			goto err;
		if (__os_exists(name, NULL) != 0) {
			if (LF_ISSET(DB_ARCH_LOG) && fnum == stable_lsn.file)
				continue;
			__os_free(dbenv, name);
			name = NULL;
			break;
		}

		if (n >= array_size - 2) {
			array_size += LIST_INCREMENT;
			if ((ret = __os_realloc(dbenv,
			    sizeof(char *) * array_size, &array)) != 0)
				goto err;
		}

		if (db_arch_abs) {
			if ((ret = __absname(dbenv,
			    pref, name, &array[n])) != 0)
				goto err;
			__os_free(dbenv, name);
		} else if ((p = __db_rpath(name)) != NULL) {
			if ((ret = __os_strdup(dbenv, p + 1, &array[n])) != 0)
				goto err;
			__os_free(dbenv, name);
		} else
			array[n] = name;

		name = NULL;
		array[++n] = NULL;
	}

	/* If there's nothing to return, we're done. */
	if (n == 0) {
		*listp = NULL;
		ret = 0;
		goto err;
	}

	/* Sort the list. */
	qsort(array, (size_t)n, sizeof(char *), __cmpfunc);

	/* Rework the memory. */
	if ((ret = __usermem(dbenv, &array)) != 0)
		goto err;

	*listp = array;
	return (0);

err:	if (array != NULL) {
		for (arrayp = array; *arrayp != NULL; ++arrayp)
			__os_free(dbenv, *arrayp);
		__os_free(dbenv, array);
	}
	if (name != NULL)
		__os_free(dbenv, name);
	return (ret);
}

/*
 * __build_data --
 *	Build a list of datafiles for return.
 */
static int
__build_data(dbenv, pref, listp)
	DB_ENV *dbenv;
	char *pref, ***listp;
{
	DBT rec;
	DB_LOGC *logc;
	DB_LSN lsn;
	__dbreg_register_args *argp;
	u_int32_t rectype;
	int array_size, last, n, nxt, ret, t_ret;
	char **array, **arrayp, **list, **lp, *p, *real_name;

	/* Get some initial space. */
	array_size = 64;
	if ((ret = __os_malloc(dbenv,
	    sizeof(char *) * array_size, &array)) != 0)
		return (ret);
	array[0] = NULL;

	memset(&rec, 0, sizeof(rec));
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		return (ret);
	for (n = 0; (ret = logc->get(logc, &lsn, &rec, DB_PREV)) == 0;) {
		if (rec.size < sizeof(rectype)) {
			ret = EINVAL;
			__db_err(dbenv, "DB_ENV->log_archive: bad log record");
			goto free_continue;
		}

		memcpy(&rectype, rec.data, sizeof(rectype));
		if (rectype != DB___dbreg_register)
			continue;
		if ((ret =
		    __dbreg_register_read(dbenv, rec.data, &argp)) != 0) {
			ret = EINVAL;
			__db_err(dbenv,
			    "DB_ENV->log_archive: unable to read log record");
			goto free_continue;
		}

		if (n >= array_size - 2) {
			array_size += LIST_INCREMENT;
			if ((ret = __os_realloc(dbenv,
			    sizeof(char *) * array_size, &array)) != 0)
				goto free_continue;
		}

		if ((ret = __os_strdup(dbenv,
		    argp->name.data, &array[n++])) != 0)
			goto free_continue;
		array[n] = NULL;

		if (argp->ftype == DB_QUEUE) {
			if ((ret = __qam_extent_names(dbenv,
			    argp->name.data, &list)) != 0)
				goto q_err;
			for (lp = list;
			    lp != NULL && *lp != NULL; lp++) {
				if (n >= array_size - 2) {
					array_size += LIST_INCREMENT;
					if ((ret = __os_realloc(dbenv,
					    sizeof(char *) *
					    array_size, &array)) != 0)
						goto q_err;
				}
				if ((ret =
				    __os_strdup(dbenv, *lp, &array[n++])) != 0)
					goto q_err;
				array[n] = NULL;
			}
q_err:			if (list != NULL)
				__os_free(dbenv, list);
		}
free_continue:	__os_free(dbenv, argp);
		if (ret != 0)
			break;
	}
	if (ret == DB_NOTFOUND)
		ret = 0;
	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (ret != 0)
		goto err1;

	/* If there's nothing to return, we're done. */
	if (n == 0) {
		ret = 0;
		*listp = NULL;
		goto err1;
	}

	/* Sort the list. */
	qsort(array, (size_t)n, sizeof(char *), __cmpfunc);

	/*
	 * Build the real pathnames, discarding nonexistent files and
	 * duplicates.
	 */
	for (last = nxt = 0; nxt < n;) {
		/*
		 * Discard duplicates.  Last is the next slot we're going
		 * to return to the user, nxt is the next slot that we're
		 * going to consider.
		 */
		if (last != nxt) {
			array[last] = array[nxt];
			array[nxt] = NULL;
		}
		for (++nxt; nxt < n &&
		    strcmp(array[last], array[nxt]) == 0; ++nxt) {
			__os_free(dbenv, array[nxt]);
			array[nxt] = NULL;
		}

		/* Get the real name. */
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, array[last], 0, NULL, &real_name)) != 0)
			goto err2;

		/* If the file doesn't exist, ignore it. */
		if (__os_exists(real_name, NULL) != 0) {
			__os_free(dbenv, real_name);
			__os_free(dbenv, array[last]);
			array[last] = NULL;
			continue;
		}

		/* Rework the name as requested by the user. */
		__os_free(dbenv, array[last]);
		array[last] = NULL;
		if (pref != NULL) {
			ret = __absname(dbenv, pref, real_name, &array[last]);
			__os_free(dbenv, real_name);
			if (ret != 0)
				goto err2;
		} else if ((p = __db_rpath(real_name)) != NULL) {
			ret = __os_strdup(dbenv, p + 1, &array[last]);
			__os_free(dbenv, real_name);
			if (ret != 0)
				goto err2;
		} else
			array[last] = real_name;
		++last;
	}

	/* NULL-terminate the list. */
	array[last] = NULL;

	/* Rework the memory. */
	if ((ret = __usermem(dbenv, &array)) != 0)
		goto err1;

	*listp = array;
	return (0);

err2:	/*
	 * XXX
	 * We've possibly inserted NULLs into the array list, so clean up a
	 * bit so that the other error processing works.
	 */
	if (array != NULL)
		for (; nxt < n; ++nxt)
			__os_free(dbenv, array[nxt]);
	/* FALLTHROUGH */

err1:	if (array != NULL) {
		for (arrayp = array; *arrayp != NULL; ++arrayp)
			__os_free(dbenv, *arrayp);
		__os_free(dbenv, array);
	}
	return (ret);
}

/*
 * __absname --
 *	Return an absolute path name for the file.
 */
static int
__absname(dbenv, pref, name, newnamep)
	DB_ENV *dbenv;
	char *pref, *name, **newnamep;
{
	size_t l_pref, l_name;
	int isabspath, ret;
	char *newname;

	l_name = strlen(name);
	isabspath = __os_abspath(name);
	l_pref = isabspath ? 0 : strlen(pref);

	/* Malloc space for concatenating the two. */
	if ((ret = __os_malloc(dbenv,
	    l_pref + l_name + 2, &newname)) != 0)
		return (ret);
	*newnamep = newname;

	/* Build the name.  If `name' is an absolute path, ignore any prefix. */
	if (!isabspath) {
		memcpy(newname, pref, l_pref);
		if (strchr(PATH_SEPARATOR, newname[l_pref - 1]) == NULL)
			newname[l_pref++] = PATH_SEPARATOR[0];
	}
	memcpy(newname + l_pref, name, l_name + 1);

	return (0);
}

/*
 * __usermem --
 *	Create a single chunk of memory that holds the returned information.
 *	If the user has their own malloc routine, use it.
 */
static int
__usermem(dbenv, listp)
	DB_ENV *dbenv;
	char ***listp;
{
	size_t len;
	int ret;
	char **array, **arrayp, **orig, *strp;

	/* Find out how much space we need. */
	for (len = 0, orig = *listp; *orig != NULL; ++orig)
		len += sizeof(char *) + strlen(*orig) + 1;
	len += sizeof(char *);

	/* Allocate it and set up the pointers. */
	if ((ret = __os_umalloc(dbenv, len, &array)) != 0)
		return (ret);

	strp = (char *)(array + (orig - *listp) + 1);

	/* Copy the original information into the new memory. */
	for (orig = *listp, arrayp = array; *orig != NULL; ++orig, ++arrayp) {
		len = strlen(*orig);
		memcpy(strp, *orig, len + 1);
		*arrayp = strp;
		strp += len + 1;

		__os_free(dbenv, *orig);
	}

	/* NULL-terminate the list. */
	*arrayp = NULL;

	__os_free(dbenv, *listp);
	*listp = array;

	return (0);
}

static int
__cmpfunc(p1, p2)
	const void *p1, *p2;
{
	return (strcmp(*((char * const *)p1), *((char * const *)p2)));
}
