/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log_register.c,v 11.35 2001/01/10 16:04:19 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "log.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

/*
 * log_register --
 *	Register a file name.
 */
int
log_register(dbenv, dbp, name)
	DB_ENV *dbenv;
	DB *dbp;
	const char *name;
{
	DBT fid_dbt, r_name;
	DB_LOG *dblp;
	DB_LSN r_unused;
	FNAME *found_fnp, *fnp, *recover_fnp, *reuse_fnp;
	LOG *lp;
	size_t len;
	int32_t maxid;
	int inserted, ok, ret;
	void *namep;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_log_register(dbenv, dbp, name));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lg_handle, DB_INIT_LOG);

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	fnp = reuse_fnp = NULL;
	inserted = ret = 0;
	namep = NULL;

	/* Check the arguments. */
	if (dbp->type != DB_BTREE && dbp->type != DB_QUEUE &&
	    dbp->type != DB_HASH && dbp->type != DB_RECNO) {
		__db_err(dbenv, "log_register: unknown DB file type");
		return (EINVAL);
	}

	R_LOCK(dbenv, &dblp->reginfo);

	/*
	 * See if we've already got this file in the log, finding the
	 * (maximum+1) in-use file id and some available file id (if we
	 * find an available fid, we'll use it, else we'll have to allocate
	 * one after the maximum that we found).
	 */
	ok = 0;
	found_fnp = recover_fnp = NULL;
	for (maxid = 0, fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
		if (F_ISSET(dblp, DBLOG_RECOVER) && fnp->id == dbp->log_fileid)
			recover_fnp = fnp;
		if (fnp->ref == 0) {		/* Entry is not in use. */
			if (reuse_fnp == NULL)
				reuse_fnp = fnp;
			continue;
		}
		if (memcmp(dbp->fileid, fnp->ufid, DB_FILE_ID_LEN) == 0) {
			if (fnp->meta_pgno == 0) {
				if (fnp->locked == 1) {
					__db_err(dbenv, "File is locked");
					return (EINVAL);
				}
				if (found_fnp != NULL) {
					fnp = found_fnp;
					goto found;
				}
				ok = 1;
			}
			if (dbp->meta_pgno == fnp->meta_pgno) {
				if (F_ISSET(dblp, DBLOG_RECOVER)) {
					if (fnp->id != dbp->log_fileid) {
						/*
						 * If we are in recovery, there
						 * is only one dbp on the list.
						 * If the refcount goes to 0,
						 * we will clear the list.  If
						 * it doesn't, we want to leave
						 * the dbp where it is, so
						 * passing a NULL to rem_logid
						 * is correct.
						 */
						__log_rem_logid(dblp,
						    NULL, fnp->id);
						if (recover_fnp != NULL)
							break;
						continue;
					}
					fnp->ref = 1;
					goto found;
				}
				++fnp->ref;
				if (ok)
					goto found;
				found_fnp = fnp;
			}
		}
		if (maxid <= fnp->id)
			maxid = fnp->id + 1;
	}
	if ((fnp = found_fnp) != NULL)
		goto found;

	/* Fill in fnp structure. */
	if (recover_fnp != NULL)	/* This has the right number */
		fnp = recover_fnp;
	else if (reuse_fnp != NULL)	/* Reuse existing one. */
		fnp = reuse_fnp;
	else {				/* Allocate a new one. */
		if ((ret = __db_shalloc(dblp->reginfo.addr,
		    sizeof(FNAME), 0, &fnp)) != 0)
			goto mem_err;
		fnp->id = maxid;
	}

	if (F_ISSET(dblp, DBLOG_RECOVER))
		fnp->id = dbp->log_fileid;

	fnp->ref = 1;
	fnp->locked = 0;
	fnp->s_type = dbp->type;
	memcpy(fnp->ufid, dbp->fileid, DB_FILE_ID_LEN);
	fnp->meta_pgno = dbp->meta_pgno;

	if (name != NULL) {
		len = strlen(name) + 1;
		if ((ret =
		    __db_shalloc(dblp->reginfo.addr, len, 0, &namep)) != 0) {
mem_err:		__db_err(dbenv,
			    "Unable to allocate memory to register %s", name);
			goto err;
	}
		fnp->name_off = R_OFFSET(&dblp->reginfo, namep);
		memcpy(namep, name, len);
	} else
		fnp->name_off = INVALID_ROFF;

	/* Only do the insert if we allocated a new fnp. */
	if (reuse_fnp == NULL && recover_fnp == NULL)
		SH_TAILQ_INSERT_HEAD(&lp->fq, fnp, q, __fname);
	inserted = 1;

	/* Log the registry. */
	if (!F_ISSET(dblp, DBLOG_RECOVER)) {
		/*
		 * We allow logging on in-memory databases, so the name here
		 * could be NULL.
		 */
		if (name != NULL) {
			r_name.data = (void *)name;
			r_name.size = strlen(name) + 1;
		}
		memset(&fid_dbt, 0, sizeof(fid_dbt));
		fid_dbt.data = dbp->fileid;
		fid_dbt.size = DB_FILE_ID_LEN;
		if ((ret = __log_register_log(dbenv, NULL, &r_unused,
		    0, LOG_OPEN, name == NULL ? NULL : &r_name,
		    &fid_dbt, fnp->id, dbp->type, dbp->meta_pgno)) != 0)
			goto err;
	}

found:	/*
	 * If we found the entry in the shared area, then the file is
	 * already open, so there is no need to log the open.  We only
	 * log the open and closes on the first open and last close.
	 */
	if (!F_ISSET(dblp, DBLOG_RECOVER) &&
	    (ret = __log_add_logid(dbenv, dblp, dbp, fnp->id)) != 0)
			goto err;

	if (!F_ISSET(dblp, DBLOG_RECOVER))
		dbp->log_fileid = fnp->id;

	if (0) {
err:		if (inserted)
			SH_TAILQ_REMOVE(&lp->fq, fnp, q, __fname);
		if (namep != NULL)
			__db_shalloc_free(dblp->reginfo.addr, namep);
		if (fnp != NULL)
			__db_shalloc_free(dblp->reginfo.addr, fnp);
	}

	R_UNLOCK(dbenv, &dblp->reginfo);

	return (ret);
}

/*
 * log_unregister --
 *	Discard a registered file name.
 */
int
log_unregister(dbenv, dbp)
	DB_ENV *dbenv;
	DB *dbp;
{
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_log_unregister(dbenv, dbp));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lg_handle, DB_INIT_LOG);

	ret = __log_filelist_update(dbenv, dbp, dbp->log_fileid, NULL, NULL);
	dbp->log_fileid = DB_LOGFILEID_INVALID;
	return (ret);
}

/*
 * PUBLIC: int __log_filelist_update
 * PUBLIC:    __P((DB_ENV *, DB *, int32_t, const char *, int *));
 *
 *  Utility player for updating and logging the file list.  Called
 *  for 3 reasons:
 *	1) mark file closed: newname == NULL.
 *	2) change filename: newname != NULL.
 *	3) from recovery to verify & change filename if necessary, set != NULL.
 */
int
__log_filelist_update(dbenv, dbp, fid, newname, set)
	DB_ENV *dbenv;
	DB *dbp;
	int32_t fid;
	const char *newname;
	int *set;
{
	DBT fid_dbt, r_name;
	DB_LOG *dblp;
	DB_LSN r_unused;
	FNAME *fnp;
	LOG *lp;
	u_int32_t len, newlen;
	int ret;
	void *namep;

	ret = 0;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	R_LOCK(dbenv, &dblp->reginfo);

	/* Find the entry in the log. */
	for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname))
		if (fid == fnp->id)
			break;
	if (fnp == NULL) {
		__db_err(dbenv, "log_unregister: non-existent file id");
		ret = EINVAL;
		goto ret1;
	}

	/*
	 * Log the unregistry only if this is the last one and we are
	 * really closing the file or if this is an abort of a created
	 * file and we need to make sure there is a record in the log.
	 */
	namep = NULL;
	len = 0;
	if (fnp->name_off != INVALID_ROFF) {
		namep = R_ADDR(&dblp->reginfo, fnp->name_off);
		len = strlen(namep) + 1;
	}
	if (!F_ISSET(dblp, DBLOG_RECOVER) && fnp->ref == 1) {
		if (namep != NULL) {
			memset(&r_name, 0, sizeof(r_name));
			r_name.data = namep;
			r_name.size = len;
		}
		memset(&fid_dbt, 0, sizeof(fid_dbt));
		fid_dbt.data = fnp->ufid;
		fid_dbt.size = DB_FILE_ID_LEN;
		if ((ret = __log_register_log(dbenv, NULL, &r_unused,
		    0, LOG_CLOSE,
		    fnp->name_off == INVALID_ROFF ? NULL : &r_name,
		    &fid_dbt, fid, fnp->s_type, fnp->meta_pgno))
		    != 0)
			goto ret1;
	}

	/*
	 * If we are changing the name we must log this fact.
	 */
	if (newname != NULL) {
		DB_ASSERT(fnp->ref == 1);
		newlen = strlen(newname) + 1;
		if (!F_ISSET(dblp, DBLOG_RECOVER)) {
			r_name.data = (void *) newname;
			r_name.size = newlen;
			if ((ret = __log_register_log(dbenv,
			    NULL, &r_unused, 0, LOG_OPEN, &r_name, &fid_dbt,
			    fnp->id, fnp->s_type, fnp->meta_pgno)) != 0)
				goto ret1;
		}

		/*
		 * Check to see if the name is already correct.
		 */
		if (set != NULL) {
			if (len != newlen || memcmp(namep, newname, len) != 0)
				*set = 1;
			else {
				*set = 0;
				goto ret1;
			}
		}

		/*
		 * Change the name, realloc memory if necessary
		 */
		if (len < newlen) {
			__db_shalloc_free(dblp->reginfo.addr,
			    R_ADDR(&dblp->reginfo, fnp->name_off));
			if ((ret = __db_shalloc(
			    dblp->reginfo.addr, newlen, 0, &namep)) != 0) {
				__db_err(dbenv,
				    "Unable to allocate memory to register %s",
				    newname);
				goto ret1;
			}
			fnp->name_off = R_OFFSET(&dblp->reginfo, namep);
		} else
			namep = R_ADDR(&dblp->reginfo, fnp->name_off);
		memcpy(namep, newname, newlen);
	} else {

		/*
		 * If more than 1 reference, just decrement the reference
		 * and return.  Otherwise, free the name if one exists.
		 */
		DB_ASSERT(fnp->ref >= 1);
		--fnp->ref;
		if (fnp->ref == 0) {
			if (fnp->name_off != INVALID_ROFF)
				__db_shalloc_free(dblp->reginfo.addr,
				    R_ADDR(&dblp->reginfo, fnp->name_off));
			fnp->name_off = INVALID_ROFF;
		}

		/*
		 * Remove from the process local table.  If this
		 * operation is taking place during recovery, then
		 * the logid was never added to the table, so do not remove it.
		 */
		if (!F_ISSET(dblp, DBLOG_RECOVER))
			__log_rem_logid(dblp, dbp, fid);
	}

ret1:	R_UNLOCK(dbenv, &dblp->reginfo);
	return (ret);
}

/*
 * __log_file_lock -- lock a file for single access
 *	This only works if logging is on.
 *
 * PUBLIC: int __log_file_lock __P((DB *));
 */
int
__log_file_lock(dbp)
	DB *dbp;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	FNAME *fnp;
	LOG *lp;
	int ret;

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	ret = 0;
	R_LOCK(dbenv, &dblp->reginfo);

	for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
		if (fnp->ref == 0)
			continue;

		if (!memcmp(dbp->fileid, fnp->ufid, DB_FILE_ID_LEN)) {
			if (fnp->meta_pgno == 0) {
				if (fnp->ref != 1)
					goto err;

				fnp->locked = 1;
			} else {
err:				__db_err(dbp->dbenv, "File is open");
				ret = EINVAL;
				goto done;
			}

		}
	}
done:	R_UNLOCK(dbenv, &dblp->reginfo);
	return (ret);
}
