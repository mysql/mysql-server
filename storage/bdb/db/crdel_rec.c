/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: crdel_rec.c,v 11.68 2004/04/29 00:07:55 ubell Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/hash.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"

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
	REC_INTRO(__crdel_metasub_read, 0);

	if ((ret = __memp_fget(mpf, &argp->pgno, 0, &pagep)) != 0) {
		*lsnp = argp->prev_lsn;
		ret = 0;
		goto out;
	}

	modified = 0;
	cmp_p = log_compare(&LSN(pagep), &argp->lsn);
	CHECK_LSN(op, cmp_p, &LSN(pagep), &argp->lsn);

	if (cmp_p == 0 && DB_REDO(op)) {
		memcpy(pagep, argp->page.data, argp->page.size);
		LSN(pagep) = *lsnp;
		modified = 1;
	} else if (DB_UNDO(op)) {
		/*
		 * We want to undo this page creation.  The page creation
		 * happened in two parts.  First, we called __bam_new which
		 * was logged separately. Then we wrote the meta-data onto
		 * the page.  So long as we restore the LSN, then the recovery
		 * for __bam_new will do everything else.
		 * Don't bother checking the lsn on the page.  If we
		 * are rolling back the next thing is that this page
		 * will get freed.  Opening the subdb will have reinitialized
		 * the page, but not the lsn.
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
