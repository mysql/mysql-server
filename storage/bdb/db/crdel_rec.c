/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: crdel_rec.c,v 12.6 2005/10/20 18:57:04 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/fop.h"
#include "dbinc/hash.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

/*
 * __crdel_metasub_recover --
 *	Recovery function for metasub.
 *
 * PUBLIC: int __crdel_metasub_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__crdel_metasub_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__crdel_metasub_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	PAGE *pagep;
	int cmp_p, modified, ret;

	pagep = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__crdel_metasub_print);
	REC_INTRO(__crdel_metasub_read, 0, 0);

	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		/* If this is an in-memory file, this might be OK. */
		if (F_ISSET(file_dbp, DB_AM_INMEM) && (ret = __memp_fget(mpf,
		    &argp->pgno, DB_MPOOL_CREATE, &pagep)) == 0)
			LSN_NOT_LOGGED(LSN(pagep));
		else {
			*lsnp = argp->prev_lsn;
			ret = 0;
			goto out;
		}
	}

	modified = 0;
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(dbenv, op, cmp_p, &LSN(pagep), &argp->lsn);

	if (cmp_p == 0 && DB_REDO(op)) {
		memcpy(pagep, argp->page.data, argp->page.size);
		LSN(pagep) = *lsnp;
		modified = 1;

		/*
		 * If this was an in-memory database and we are re-creating
		 * and this is the meta-data page, then we need to set up a
		 * bunch of fields in the dbo as well.
		 */
		if (F_ISSET(file_dbp, DB_AM_INMEM) &&
		    argp->pgno == PGNO_BASE_MD &&
		    (ret = __db_meta_setup(file_dbp->dbenv,
		    file_dbp, file_dbp->dname, (DBMETA *)pagep, 0, 1)) != 0)
			goto out;
	} else if (DB_UNDO(op)) {
		/*
		 * We want to undo this page creation.  The page creation
		 * happened in two parts.  First, we called __bam_new which
		 * was logged separately. Then we wrote the meta-data onto
		 * the page.  So long as we restore the LSN, then the recovery
		 * for __bam_new will do everything else.
		 *
		 * Don't bother checking the lsn on the page.  If we are
		 * rolling back the next thing is that this page will get
		 * freed.  Opening the subdb will have reinitialized the
		 * page, but not the lsn.
		 */
		LSN(pagep) = argp->lsn;
		modified = 1;
	}
	if ((ret = __memp_fput(mpf, pagep, modified ? DB_MPOOL_DIRTY : 0)) != 0)
		goto out;
	pagep = NULL;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (pagep != NULL)
		(void)__memp_fput(mpf, pagep, 0);
	REC_CLOSE;
}

/*
 * __crdel_inmem_create_recover --
 *	Recovery function for inmem_create.
 *
 * PUBLIC: int __crdel_inmem_create_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__crdel_inmem_create_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	DB *dbp;
	__crdel_inmem_create_args *argp;
	int do_close, ret, t_ret;

	COMPQUIET(info, NULL);
	dbp = NULL;
	do_close = 0;
	REC_PRINT(__crdel_inmem_create_print);
	REC_NOOP_INTRO(__crdel_inmem_create_read);

	/* First, see if the DB handle already exists. */
	if (argp->fileid == DB_LOGFILEID_INVALID) {
		if (DB_REDO(op))
			ret = ENOENT;
		else
			ret = 0;
	} else
		ret = __dbreg_id_to_db_int(dbenv,
		    argp->txnid, &dbp, argp->fileid, 0, 0);

	if (DB_REDO(op)) {
		/*
		 * If the dbreg failed, that means that we're creating a
		 * tmp file.
		 */
		if (ret != 0) {
			if ((ret = db_create(&dbp, dbenv, 0)) != 0)
				goto out;

			F_SET(dbp, DB_AM_RECOVER | DB_AM_INMEM);
			memcpy(dbp->fileid, argp->fid.data, DB_FILE_ID_LEN);
			if (((ret = __os_strdup(dbenv,
			    argp->name.data, &dbp->dname)) != 0))
				goto out;

			/*
			 * This DBP is never going to be entered into the
			 * dbentry table, so if we leave it open here,
			 * then we're going to lose it.
			 */
			do_close = 1;
		}

		/* Now, set the fileid. */
		memcpy(dbp->fileid, argp->fid.data, argp->fid.size);
		if ((ret = __memp_set_fileid(dbp->mpf, dbp->fileid)) != 0)
			goto out;
		dbp->preserve_fid = 1;
		MAKE_INMEM(dbp);
		if ((ret = __db_dbenv_setup(dbp,
		    NULL, NULL, argp->name.data, TXN_INVALID, 0)) != 0)
			goto out;
		ret = __db_dbenv_mpool(dbp, argp->name.data, 0);

		if (ret == ENOENT) {
			dbp->pgsize = argp->pgsize;
			if ((ret = __db_dbenv_mpool(dbp,
			    argp->name.data, DB_CREATE)) != 0)
				goto out;
		} else if (ret != 0)
			goto out;
	}

	if (DB_UNDO(op)) {
		if (ret == 0)
			ret = __memp_nameop(dbenv, argp->fid.data, NULL,
			    (const char *)argp->name.data,  NULL, 1);

		if (ret == ENOENT || ret == DB_DELETED)
			ret = 0;
		else
			goto out;
	}

	*lsnp = argp->prev_lsn;

out:	if (dbp != NULL) {
		t_ret = 0;
		if (DB_UNDO(op))
			t_ret = __db_refresh(dbp, NULL, DB_NOSYNC, NULL, 0);
		else if (do_close || ret != 0)
			t_ret = __db_close(dbp, NULL, DB_NOSYNC);
		if (t_ret != 0 && ret == 0)
			ret = t_ret;
	}
	REC_NOOP_CLOSE;
}

/*
 * __crdel_inmem_rename_recover --
 *	Recovery function for inmem_rename.
 *
 * PUBLIC: int __crdel_inmem_rename_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__crdel_inmem_rename_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__crdel_inmem_rename_args *argp;
	u_int8_t *fileid;
	int ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__crdel_inmem_rename_print);
	REC_NOOP_INTRO(__crdel_inmem_rename_read);
	fileid = argp->fid.data;

	/* Void out errors because the files may or may not still exist. */
	if (DB_REDO(op))
		(void)__memp_nameop(dbenv, fileid,
		    (const char *)argp->newname.data,
		    (const char *)argp->oldname.data,
		    (const char *)argp->newname.data, 1);

	if (DB_UNDO(op))
		(void)__memp_nameop(dbenv, fileid,
		    (const char *)argp->oldname.data,
		    (const char *)argp->newname.data,
		    (const char *)argp->oldname.data, 1);

	*lsnp = argp->prev_lsn;
	ret = 0;

	REC_NOOP_CLOSE;
}

/*
 * __crdel_inmem_remove_recover --
 *	Recovery function for inmem_remove.
 *
 * PUBLIC: int __crdel_inmem_remove_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__crdel_inmem_remove_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__crdel_inmem_remove_args *argp;
	int ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__crdel_inmem_remove_print);
	REC_NOOP_INTRO(__crdel_inmem_remove_read);

	/*
	 * Since removes are delayed; there is no undo for a remove; only redo.
	 * The remove may fail, which is OK.
	 */
	if (DB_REDO(op)) {
		(void)__memp_nameop(dbenv,
		    argp->fid.data, NULL, argp->name.data, NULL, 1);
	}

	*lsnp = argp->prev_lsn;
	ret = 0;

	REC_NOOP_CLOSE;
}
