/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: dbreg_stat.c,v 12.5 2005/10/12 15:01:47 margo Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

#ifdef HAVE_STATISTICS
static int __dbreg_print_dblist __P((DB_ENV *, u_int32_t));

/*
 * __dbreg_stat_print --
 *	Print the dbreg statistics.
 *
 * PUBLIC: int __dbreg_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__dbreg_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	int ret;

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __dbreg_print_dblist(dbenv, flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __dbreg_print_fname --
 *	Display the contents of an FNAME structure.
 *
 * PUBLIC: void __dbreg_print_fname __P((DB_ENV *, FNAME *));
 */
void
__dbreg_print_fname(dbenv, fnp)
	DB_ENV *dbenv;
	FNAME *fnp;
{
	static const FN fn[] = {
		{ DB_FNAME_DURABLE,	"DB_FNAME_DURABLE" },
		{ DB_FNAME_NOTLOGGED,	"DB_FNAME_NOTLOGGED" },
		{ 0,			NULL }
	};

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB handle FNAME contents:");
	STAT_LONG("log ID", fnp->id);
	STAT_ULONG("Meta pgno", fnp->meta_pgno);
	__db_print_fileid(dbenv, fnp->ufid, "\tFile ID");
	STAT_ULONG("create txn", fnp->create_txnid);
	__db_prflags(dbenv, NULL, fnp->flags, fn, NULL, "\tFlags");
}

/*
 * __dbreg_print_dblist --
 *	Display the DB_ENV's list of files.
 */
static int
__dbreg_print_dblist(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB *dbp;
	DB_LOG *dblp;
	FNAME *fnp;
	LOG *lp;
	int del, first;
	char *name;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "LOG FNAME list:");
	__mutex_print_debug_single(
	    dbenv, "File name mutex", lp->mtx_filelist, flags);

	STAT_LONG("Fid max", lp->fid_max);

	MUTEX_LOCK(dbenv, lp->mtx_filelist);
	for (first = 1, fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
		if (first) {
			first = 0;
			__db_msg(dbenv,
			    "ID\tName\tType\tPgno\tTxnid\tDBP-info");
		}
		if (fnp->name_off == INVALID_ROFF)
			name = "";
		else
			name = R_ADDR(&dblp->reginfo, fnp->name_off);

		dbp = fnp->id >= dblp->dbentry_cnt ? NULL :
		    dblp->dbentry[fnp->id].dbp;
		del = fnp->id >= dblp->dbentry_cnt ? 0 :
		    dblp->dbentry[fnp->id].deleted;
		__db_msg(dbenv, "%ld\t%s\t%s\t%lu\t%lx\t%s %d %lx %lx",
		    (long)fnp->id, name,
		    __db_dbtype_to_string(fnp->s_type),
		    (u_long)fnp->meta_pgno, (u_long)fnp->create_txnid,
		    dbp == NULL ? "No DBP" : "DBP", del, P_TO_ULONG(dbp),
		    (u_long)(dbp == NULL ? 0 : dbp->flags));
	}
	MUTEX_UNLOCK(dbenv, lp->mtx_filelist);

	return (0);
}
#endif
