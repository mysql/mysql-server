/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: fop_rec.c,v 1.31 2004/09/22 03:45:25 bostic Exp $
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
#include "dbinc/db_am.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

/*
 * __fop_create_recover --
 *	Recovery function for create.
 *
 * PUBLIC: int __fop_create_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__fop_create_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	DB_FH *fhp;
	__fop_create_args *argp;
	char *real_name;
	int ret;

	real_name = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__fop_create_print);
	REC_NOOP_INTRO(__fop_create_read);

	if ((ret = __db_appname(dbenv, (APPNAME)argp->appname,
	    (const char *)argp->name.data, 0, NULL, &real_name)) != 0)
		goto out;

	if (DB_UNDO(op))
		(void)__os_unlink(dbenv, real_name);
	else if (DB_REDO(op)) {
		if ((ret = __os_open(dbenv, real_name,
		    DB_OSO_CREATE | DB_OSO_EXCL, argp->mode, &fhp)) == 0)
			(void)__os_closehandle(dbenv, fhp);
		else
			goto out;
	}

	*lsnp = argp->prev_lsn;

out: if (real_name != NULL)
		__os_free(dbenv, real_name);

	REC_NOOP_CLOSE;
}

/*
 * __fop_remove_recover --
 *	Recovery function for remove.
 *
 * PUBLIC: int __fop_remove_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__fop_remove_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__fop_remove_args *argp;
	char *real_name;
	int ret;

	real_name = NULL;
	COMPQUIET(info, NULL);
	REC_PRINT(__fop_remove_print);
	REC_NOOP_INTRO(__fop_remove_read);

	if ((ret = __db_appname(dbenv, (APPNAME)argp->appname,
	    (const char *)argp->name.data, 0, NULL, &real_name)) != 0)
		goto out;

	/* Its ok if the file is not there. */
	if (DB_REDO(op))
		(void)__memp_nameop(dbenv,
		    (u_int8_t *)argp->fid.data, NULL, real_name, NULL);

	*lsnp = argp->prev_lsn;
out:	if (real_name != NULL)
		__os_free(dbenv, real_name);
	REC_NOOP_CLOSE;
}

/*
 * __fop_write_recover --
 *	Recovery function for writechunk.
 *
 * PUBLIC: int __fop_write_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__fop_write_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__fop_write_args *argp;
	int ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__fop_write_print);
	REC_NOOP_INTRO(__fop_write_read);

	ret = 0;
	if (DB_UNDO(op))
		DB_ASSERT(argp->flag != 0);
	else if (DB_REDO(op))
		ret = __fop_write(dbenv,
		    argp->txnid, argp->name.data, argp->appname,
		    NULL, argp->pgsize, argp->pageno, argp->offset,
		    argp->page.data, argp->page.size, argp->flag, 0);

	if (ret == 0)
		*lsnp = argp->prev_lsn;
	REC_NOOP_CLOSE;
}

/*
 * __fop_rename_recover --
 *	Recovery function for rename.
 *
 * PUBLIC: int __fop_rename_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__fop_rename_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__fop_rename_args *argp;
	DB_FH *fhp;
	DBMETA *meta;
	char *real_new, *real_old, *src;
	int ret;
	u_int8_t *fileid, mbuf[DBMETASIZE];

	real_new = NULL;
	real_old = NULL;
	ret = 0;
	fhp = NULL;
	meta = (DBMETA *)&mbuf[0];

	COMPQUIET(info, NULL);
	REC_PRINT(__fop_rename_print);
	REC_NOOP_INTRO(__fop_rename_read);
	fileid = argp->fileid.data;

	if ((ret = __db_appname(dbenv, (APPNAME)argp->appname,
	    (const char *)argp->newname.data, 0, NULL, &real_new)) != 0)
		goto out;
	if ((ret = __db_appname(dbenv, (APPNAME)argp->appname,
	    (const char *)argp->oldname.data, 0, NULL, &real_old)) != 0)
		goto out;

	/*
	 * Verify that we are manipulating the correct file.  We should always
	 * be OK on an ABORT or an APPLY, but during recovery, we have to
	 * check.
	 */
	if (op != DB_TXN_ABORT && op != DB_TXN_APPLY) {
		src = DB_UNDO(op) ? real_new : real_old;
		/*
		 * Interpret any error as meaning that the file either doesn't
		 * exist, doesn't have a meta-data page, or is in some other
		 * way, shape or form, incorrect, so that we should not restore
		 * it.
		 */
		if (__os_open(dbenv, src, 0, 0, &fhp) != 0)
			goto done;
		if (__fop_read_meta(dbenv,
		    src, mbuf, DBMETASIZE, fhp, 1, NULL) != 0)
			goto done;
		if (__db_chk_meta(dbenv, NULL, meta, 1) != 0)
			goto done;
		if (memcmp(argp->fileid.data, meta->uid, DB_FILE_ID_LEN) != 0)
			goto done;
		(void)__os_closehandle(dbenv, fhp);
		fhp = NULL;
	}

	if (DB_UNDO(op))
		(void)__memp_nameop(dbenv, fileid,
		    (const char *)argp->oldname.data, real_new, real_old);
	if (DB_REDO(op))
		(void)__memp_nameop(dbenv, fileid,
		    (const char *)argp->newname.data, real_old, real_new);

done:	*lsnp = argp->prev_lsn;
out:	if (real_new != NULL)
		__os_free(dbenv, real_new);
	if (real_old != NULL)
		__os_free(dbenv, real_old);
	if (fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);

	REC_NOOP_CLOSE;
}

/*
 * __fop_file_remove_recover --
 *	Recovery function for file_remove.  On the REDO pass, we need to
 * make sure no one recreated the file while we weren't looking.  On an
 * undo pass must check if the file we are interested in is the one that
 * exists and then set the status of the child transaction depending on
 * what we find out.
 *
 * PUBLIC: int __fop_file_remove_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__fop_file_remove_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__fop_file_remove_args *argp;
	DBMETA *meta;
	DB_FH *fhp;
	char *real_name;
	int is_real, is_tmp, ret;
	size_t len;
	u_int8_t mbuf[DBMETASIZE];
	u_int32_t cstat, ret_stat;

	fhp = NULL;
	is_real = is_tmp = 0;
	real_name = NULL;
	meta = (DBMETA *)&mbuf[0];
	REC_PRINT(__fop_file_remove_print);
	REC_NOOP_INTRO(__fop_file_remove_read);

	/*
	 * This record is only interesting on the backward, forward, and
	 * apply phases.
	 */
	if (op != DB_TXN_BACKWARD_ROLL &&
	    op != DB_TXN_FORWARD_ROLL && op != DB_TXN_APPLY)
		goto done;

	if ((ret = __db_appname(dbenv,
	    (APPNAME)argp->appname, argp->name.data, 0, NULL, &real_name)) != 0)
		goto out;

	/* Verify that we are manipulating the correct file.  */
	len = 0;
	if (__os_open(dbenv, real_name, 0, 0, &fhp) != 0 ||
	    (ret = __fop_read_meta(dbenv, real_name,
	    mbuf, DBMETASIZE, fhp, 1, &len)) != 0) {
		/*
		 * If len is non-zero, then the file exists and has something
		 * in it, but that something isn't a full meta-data page, so
		 * this is very bad.  Bail out!
		 */
		if (len != 0)
			goto out;

		/* File does not exist. */
		cstat = TXN_EXPECTED;
	} else {
		/*
		 * We can ignore errors here since we'll simply fail the
		 * checks below and assume this is the wrong file.
		 */
		(void)__db_chk_meta(dbenv, NULL, meta, 1);
		is_real =
		    memcmp(argp->real_fid.data, meta->uid, DB_FILE_ID_LEN) == 0;
		is_tmp =
		    memcmp(argp->tmp_fid.data, meta->uid, DB_FILE_ID_LEN) == 0;

		if (!is_real && !is_tmp)
			/* File exists, but isn't what we were removing. */
			cstat = TXN_IGNORE;
		else
			/* File exists and is the one that we were removing. */
			cstat = TXN_COMMIT;
	}
	if (fhp != NULL) {
		(void)__os_closehandle(dbenv, fhp);
		fhp = NULL;
	}

	if (DB_UNDO(op)) {
		/* On the backward pass, we leave a note for the child txn. */
		if ((ret = __db_txnlist_update(dbenv,
		    info, argp->child, cstat, NULL, &ret_stat, 1)) != 0)
			goto out;
	} else if (DB_REDO(op)) {
		/*
		 * On the forward pass, check if someone recreated the
		 * file while we weren't looking.
		 */
		if (cstat == TXN_COMMIT)
			(void)__memp_nameop(dbenv,
			    is_real ? argp->real_fid.data : argp->tmp_fid.data,
			    NULL, real_name, NULL);
	}

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (real_name != NULL)
		__os_free(dbenv, real_name);
	if (fhp != NULL)
		(void)__os_closehandle(dbenv, fhp);
	REC_NOOP_CLOSE;
}
