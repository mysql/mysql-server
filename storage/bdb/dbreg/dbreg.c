/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: dbreg.c,v 11.68 2002/08/28 19:05:27 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

/*
 * The dbreg subsystem, as its name implies, registers database handles so
 * that we can associate log messages with them without logging a filename
 * or a full, unique DB ID.  Instead, we assign each dbp an int32_t which is
 * easy and cheap to log, and use this subsystem to map back and forth.
 *
 * Overview of how dbreg ids are managed:
 *
 * OPEN
 *	dbreg_setup (Creates FNAME struct.)
 *	dbreg_new_id (Assigns new ID to dbp and logs it.  May be postponed
 *	until we attempt to log something else using that dbp, if the dbp
 *	was opened on a replication client.)
 *
 * CLOSE
 *	dbreg_close_id  (Logs closure of dbp/revocation of ID.)
 *	dbreg_revoke_id (As name implies, revokes ID.)
 *	dbreg_teardown (Destroys FNAME.)
 *
 * RECOVERY
 *	dbreg_setup
 *	dbreg_assign_id (Assigns a particular ID we have in the log to a dbp.)
 *
 *	sometimes: dbreg_revoke_id; dbreg_teardown
 *	other times: normal close path
 *
 * A note about locking:
 *
 *	FNAME structures are referenced only by their corresponding dbp's
 *	until they have a valid id.
 *
 *	Once they have a valid id, they must get linked into the log
 *	region list so they can get logged on checkpoints.
 *
 *	An FNAME that may/does have a valid id must be accessed under
 *	protection of the fq_mutex, with the following exception:
 *
 *	We don't want to have to grab the fq_mutex on every log
 *	record, and it should be safe not to do so when we're just
 *	looking at the id, because once allocated, the id should
 *	not change under a handle until the handle is closed.
 *
 *	If a handle is closed during an attempt by another thread to
 *	log with it, well, the application doing the close deserves to
 *	go down in flames and a lot else is about to fail anyway.
 *
 *	When in the course of logging we encounter an invalid id
 *	and go to allocate it lazily, we *do* need to check again
 *	after grabbing the mutex, because it's possible to race with
 *	another thread that has also decided that it needs to allocate
 *	a id lazily.
 *
 * See SR #5623 for further discussion of the new dbreg design.
 */

/*
 * __dbreg_setup --
 *	Allocate and initialize an FNAME structure.  The FNAME structures
 * live in the log shared region and map one-to-one with open database handles.
 * When the handle needs to be logged, the FNAME should have a valid fid
 * allocated.  If the handle currently isn't logged, it still has an FNAME
 * entry.  If we later discover that the handle needs to be logged, we can
 * allocate a id for it later.  (This happens when the handle is on a
 * replication client that later becomes a master.)
 *
 * PUBLIC: int __dbreg_setup __P((DB *, const char *, u_int32_t));
 */
int
__dbreg_setup(dbp, name, create_txnid)
	DB *dbp;
	const char *name;
	u_int32_t create_txnid;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	FNAME *fnp;
	int ret;
	size_t len;
	void *namep;

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;

	fnp = NULL;
	namep = NULL;

	/* Allocate an FNAME and, if necessary, a buffer for the name itself. */
	R_LOCK(dbenv, &dblp->reginfo);
	if ((ret =
	    __db_shalloc(dblp->reginfo.addr, sizeof(FNAME), 0, &fnp)) != 0) {
		R_UNLOCK(dbenv, &dblp->reginfo);
		return (ret);
	}
	memset(fnp, 0, sizeof(FNAME));
	if (name != NULL) {
		len = strlen(name) + 1;
		if ((ret = __db_shalloc(dblp->reginfo.addr,
		    len, 0, &namep)) != 0) {
			R_UNLOCK(dbenv, &dblp->reginfo);
			return (ret);
		}
		fnp->name_off = R_OFFSET(&dblp->reginfo, namep);
		memcpy(namep, name, len);
	} else
		fnp->name_off = INVALID_ROFF;

	R_UNLOCK(dbenv, &dblp->reginfo);

	/*
	 * Fill in all the remaining info that we'll need later to register
	 * the file, if we use it for logging.
	 */
	fnp->id = DB_LOGFILEID_INVALID;
	fnp->s_type = dbp->type;
	memcpy(fnp->ufid, dbp->fileid, DB_FILE_ID_LEN);
	fnp->meta_pgno = dbp->meta_pgno;
	fnp->create_txnid = create_txnid;

	dbp->log_filename = fnp;

	return (0);
}

/*
 * __dbreg_teardown --
 *	Destroy a DB handle's FNAME struct.
 *
 * PUBLIC: int __dbreg_teardown __P((DB *));
 */
int
__dbreg_teardown(dbp)
	DB *dbp;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	FNAME *fnp;

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;
	fnp = dbp->log_filename;

	/*
	 * We may not have an FNAME if we were never opened.  This is not an
	 * error.
	 */
	if (fnp == NULL)
		return (0);

	DB_ASSERT(fnp->id == DB_LOGFILEID_INVALID);

	R_LOCK(dbenv, &dblp->reginfo);
	if (fnp->name_off != INVALID_ROFF)
		__db_shalloc_free(dblp->reginfo.addr,
		    R_ADDR(&dblp->reginfo, fnp->name_off));
	__db_shalloc_free(dblp->reginfo.addr, fnp);
	R_UNLOCK(dbenv, &dblp->reginfo);

	dbp->log_filename = NULL;

	return (0);
}

/*
 * __dbreg_new_id --
 *	Assign an unused dbreg id to this database handle.
 *
 * PUBLIC: int __dbreg_new_id __P((DB *, DB_TXN *));
 */
int
__dbreg_new_id(dbp, txn)
	DB *dbp;
	DB_TXN *txn;
{
	DBT fid_dbt, r_name;
	DB_ENV *dbenv;
	DB_LOG *dblp;
	DB_LSN unused;
	FNAME *fnp;
	LOG *lp;
	int32_t id;
	int ret;

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	fnp = dbp->log_filename;

	/* The fq_mutex protects the FNAME list and id management. */
	MUTEX_LOCK(dbenv, &lp->fq_mutex);

	/*
	 * It's possible that after deciding we needed to call this function,
	 * someone else allocated an ID before we grabbed the lock.  Check
	 * to make sure there was no race and we have something useful to do.
	 */
	if (fnp->id != DB_LOGFILEID_INVALID) {
		MUTEX_UNLOCK(dbenv, &lp->fq_mutex);
		return (0);
	}

	/* Get an unused ID from the free list. */
	if ((ret = __dbreg_pop_id(dbenv, &id)) != 0)
		goto err;

	/* If no ID was found, allocate a new one. */
	if (id == DB_LOGFILEID_INVALID)
		id = lp->fid_max++;

	fnp->id = id;

	/* Hook the FNAME into the list of open files. */
	SH_TAILQ_INSERT_HEAD(&lp->fq, fnp, q, __fname);

	/*
	 * Log the registry.  We should only request a new ID in situations
	 * where logging is reasonable.
	 */
	DB_ASSERT(!F_ISSET(dbp, DB_AM_RECOVER));

	memset(&fid_dbt, 0, sizeof(fid_dbt));
	memset(&r_name, 0, sizeof(r_name));
	if (fnp->name_off != INVALID_ROFF) {
		r_name.data = R_ADDR(&dblp->reginfo, fnp->name_off);
		r_name.size = (u_int32_t)strlen((char *)r_name.data) + 1;
	}
	fid_dbt.data = dbp->fileid;
	fid_dbt.size = DB_FILE_ID_LEN;
	if ((ret = __dbreg_register_log(dbenv, txn, &unused, 0, LOG_OPEN,
	    r_name.size == 0 ? NULL : &r_name, &fid_dbt, id, fnp->s_type,
	    fnp->meta_pgno, fnp->create_txnid)) != 0)
		goto err;

	DB_ASSERT(dbp->type == fnp->s_type);
	DB_ASSERT(dbp->meta_pgno == fnp->meta_pgno);

	if ((ret = __dbreg_add_dbentry(dbenv, dblp, dbp, id)) != 0)
		goto err;

err:	MUTEX_UNLOCK(dbenv, &lp->fq_mutex);
	return (ret);
}

/*
 * __dbreg_assign_id --
 *	Assign a particular dbreg id to this database handle.
 *
 * PUBLIC: int __dbreg_assign_id __P((DB *, int32_t));
 */
int
__dbreg_assign_id(dbp, id)
	DB *dbp;
	int32_t id;
{
	DB *close_dbp;
	DB_ENV *dbenv;
	DB_LOG *dblp;
	FNAME *close_fnp, *fnp;
	LOG *lp;
	int ret;

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	fnp = dbp->log_filename;

	close_dbp = NULL;
	close_fnp = NULL;

	/* The fq_mutex protects the FNAME list and id management. */
	MUTEX_LOCK(dbenv, &lp->fq_mutex);

	/* We should only call this on DB handles that have no ID. */
	DB_ASSERT(fnp->id == DB_LOGFILEID_INVALID);

	/*
	 * Make sure there isn't already a file open with this ID. There can
	 * be in recovery, if we're recovering across a point where an ID got
	 * reused.
	 */
	if (__dbreg_id_to_fname(dblp, id, 1, &close_fnp) == 0) {
		/*
		 * We want to save off any dbp we have open with this id.
		 * We can't safely close it now, because we hold the fq_mutex,
		 * but we should be able to rely on it being open in this
		 * process, and we're running recovery, so no other thread
		 * should muck with it if we just put off closing it until
		 * we're ready to return.
		 *
		 * Once we have the dbp, revoke its id;  we're about to
		 * reuse it.
		 */
		ret = __dbreg_id_to_db_int(dbenv, NULL, &close_dbp, id, 0, 0);
		if (ret == ENOENT) {
			ret = 0;
			goto cont;
		} else if (ret != 0)
			goto err;

		if ((ret = __dbreg_revoke_id(close_dbp, 1)) != 0)
			goto err;
	}

	/*
	 * Remove this ID from the free list, if it's there, and make sure
	 * we don't allocate it anew.
	 */
cont:	if ((ret = __dbreg_pluck_id(dbenv, id)) != 0)
		goto err;
	if (id >= lp->fid_max)
		lp->fid_max = id + 1;

	/* Now go ahead and assign the id to our dbp. */
	fnp->id = id;
	SH_TAILQ_INSERT_HEAD(&lp->fq, fnp, q, __fname);

	if ((ret = __dbreg_add_dbentry(dbenv, dblp, dbp, id)) != 0)
		goto err;

err:	MUTEX_UNLOCK(dbenv, &lp->fq_mutex);

	/* There's nothing useful that our caller can do if this close fails. */
	if (close_dbp != NULL)
		(void)close_dbp->close(close_dbp, DB_NOSYNC);

	return (ret);
}

/*
 * __dbreg_revoke_id --
 *	Take a log id away from a dbp, in preparation for closing it,
 *	but without logging the close.
 *
 * PUBLIC: int __dbreg_revoke_id __P((DB *, int));
 */
int
__dbreg_revoke_id(dbp, have_lock)
	DB *dbp;
	int have_lock;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	FNAME *fnp;
	LOG *lp;
	int32_t id;
	int ret;

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	fnp = dbp->log_filename;

	/* If we lack an ID, this is a null-op. */
	if (fnp == NULL || fnp->id == DB_LOGFILEID_INVALID)
		return (0);

	if (!have_lock)
		MUTEX_LOCK(dbenv, &lp->fq_mutex);

	id = fnp->id;
	fnp->id = DB_LOGFILEID_INVALID;

	/* Remove the FNAME from the list of open files. */
	SH_TAILQ_REMOVE(&lp->fq, fnp, q, __fname);

	/* Remove this id from the dbentry table. */
	__dbreg_rem_dbentry(dblp, id);

	/* Push this id onto the free list. */
	ret = __dbreg_push_id(dbenv, id);

	if (!have_lock)
		MUTEX_UNLOCK(dbenv, &lp->fq_mutex);
	return (ret);
}

/*
 * __dbreg_close_id --
 *	Take a dbreg id away from a dbp that we're closing, and log
 * the unregistry.
 *
 * PUBLIC: int __dbreg_close_id __P((DB *, DB_TXN *));
 */
int
__dbreg_close_id(dbp, txn)
	DB *dbp;
	DB_TXN *txn;
{
	DBT fid_dbt, r_name, *dbtp;
	DB_ENV *dbenv;
	DB_LOG *dblp;
	DB_LSN r_unused;
	FNAME *fnp;
	LOG *lp;
	int ret;

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	fnp = dbp->log_filename;

	/* If we lack an ID, this is a null-op. */
	if (fnp == NULL || fnp->id == DB_LOGFILEID_INVALID)
		return (0);

	MUTEX_LOCK(dbenv, &lp->fq_mutex);

	if (fnp->name_off == INVALID_ROFF)
		dbtp = NULL;
	else {
		memset(&r_name, 0, sizeof(r_name));
		r_name.data = R_ADDR(&dblp->reginfo, fnp->name_off);
		r_name.size =
		    (u_int32_t)strlen((char *)r_name.data) + 1;
		dbtp = &r_name;
	}
	memset(&fid_dbt, 0, sizeof(fid_dbt));
	fid_dbt.data = fnp->ufid;
	fid_dbt.size = DB_FILE_ID_LEN;
	if ((ret = __dbreg_register_log(dbenv, txn,
	    &r_unused, 0, LOG_CLOSE, dbtp, &fid_dbt, fnp->id,
	    fnp->s_type, fnp->meta_pgno, TXN_INVALID)) != 0)
		goto err;

	ret = __dbreg_revoke_id(dbp, 1);

err:	MUTEX_UNLOCK(dbenv, &lp->fq_mutex);
	return (ret);
}
