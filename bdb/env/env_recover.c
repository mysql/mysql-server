/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2000\nSleepycat Software Inc.  All rights reserved.\n";
static const char revid[] =
    "$Id: env_recover.c,v 11.33 2001/01/04 22:38:42 ubell Exp $";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_dispatch.h"
#include "db_am.h"
#include "log.h"
#include "txn.h"

static float __lsn_diff __P((DB_LSN *, DB_LSN *, DB_LSN *, u_int32_t, int));
static int   __log_earliest __P((DB_ENV *, int32_t *, DB_LSN *));

/*
 * __db_apprec --
 *	Perform recovery.
 *
 * PUBLIC: int __db_apprec __P((DB_ENV *, u_int32_t));
 */
int
__db_apprec(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DBT data;
	DB_LSN ckp_lsn, first_lsn, last_lsn, lowlsn, lsn, open_lsn;
	DB_TXNREGION *region;
	__txn_ckp_args *ckp_args;
	time_t now, tlow;
	float nfiles;
	int32_t low;
	int is_thread, progress, ret;
	void *txninfo;

	COMPQUIET(nfiles, (float)0);

	/*
	 * Save the state of the thread flag -- we don't need it on at the
	 * moment because we're single-threaded until recovery is complete.
	 */
	is_thread = F_ISSET(dbenv, DB_ENV_THREAD) ? 1 : 0;
	F_CLR(dbenv, DB_ENV_THREAD);
	F_SET((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);

	/*
	 * If the user is specifying recover to a particular point in time,
	 * verify that the logs present are sufficient to do this.
	 */
	ZERO_LSN(lowlsn);
	if (dbenv->tx_timestamp != 0) {
		if ((ret = __log_earliest(dbenv, &low, &lowlsn)) != 0)
			return (ret);
		if ((int32_t)dbenv->tx_timestamp < low) {
			char t1[30], t2[30];

			strcpy(t1, ctime(&dbenv->tx_timestamp));
			tlow = (time_t)low;
			strcpy(t2, ctime(&tlow));
			__db_err(dbenv,
		     "Invalid recovery timestamp %.*s; earliest time is %.*s",
			    24, t1, 24, t2);
			return (EINVAL);
		}
	}

	/* Initialize the transaction list. */
	if ((ret = __db_txnlist_init(dbenv, &txninfo)) != 0)
		return (ret);

	/*
	 * Recovery is done in three passes:
	 * Pass #0:
	 *	We need to find the position from which we will open files
	 *	We need to open files beginning with the last to next
	 *	checkpoint because we might have crashed after writing the
	 *	last checkpoint record, but before having written out all
	 *	the open file information.
	 *
	 * Pass #1:
	 *	Read forward through the log from the second to last checkpoint
	 *	opening and closing files so that at the end of the log we have
	 *	the "current" set of files open.
	 *
	 * Pass #2:
	 *	Read backward through the log undoing any uncompleted TXNs.
	 *	There are three cases:
	 *	    1.  If doing catastrophic recovery, we read to the beginning
	 *		of the log
	 *	    2.  If we are doing normal reovery, then we have to roll
	 *		back to the most recent checkpoint that occurs
	 *		before the most recent checkpoint LSN, which is
	 *		returned by __log_findckp().
	 *	    3.  If we are recovering to a point in time, then we have
	 *		to roll back to the checkpoint whose ckp_lsn is earlier
	 *		than the specified time.  __log_earliest will figure
	 *		this out for us.
	 *	In case 2, "uncompleted TXNs" include all those who commited
	 *	after the user's specified timestamp.
	 *
	 * Pass #3:
	 *	Read forward through the log from the LSN found in pass #2,
	 *	redoing any committed TXNs (which commited after any user-
	 *	specified rollback point).  During this pass, checkpoint
	 *	file information is ignored, and file openings and closings
	 *	are redone.
	 */

	/*
	 * Find out the last lsn, so that we can estimate how far along we
	 * are in recovery.  This will help us determine how much log there
	 * is between the first LSN that we're going to be working with and
	 * the last one.  We assume that each of the three phases takes the
	 * same amount of time (a false assumption) and then use the %-age
	 * of the amount of log traversed to figure out how much of the
	 * pass we've accomplished.
	 */
	memset(&data, 0, sizeof(data));
	if (dbenv->db_feedback != NULL &&
	    (ret = log_get(dbenv, &last_lsn, &data, DB_LAST)) != 0)
		goto out;

	/*
	 * Pass #0
	 * Find the second to last checkpoint in the log.  This is the point
	 * from which we want to begin pass #1 (the open files pass).
	 */
	ckp_args = NULL;

	if (LF_ISSET(DB_RECOVER_FATAL)) {
		if ((ret = log_get(dbenv, &ckp_lsn, &data, DB_FIRST)) != 0) {
			if (ret == DB_NOTFOUND)
				ret = 0;
			else
				__db_err(dbenv, "First log record not found");
			goto out;
		}
		open_lsn = ckp_lsn;
	} else if ((ret =
	     log_get(dbenv, &ckp_lsn, &data, DB_CHECKPOINT)) != 0) {
		/*
		 * If we don't find a checkpoint, start from the beginning.
		 * If that fails, we're done.  Note, we do not require that
		 * there be log records if we're performing recovery.
		 */
first:		if ((ret = log_get(dbenv, &ckp_lsn, &data, DB_FIRST)) != 0) {
			if (ret == DB_NOTFOUND)
				ret = 0;
			else
				__db_err(dbenv, "First log record not found");
			goto out;
		}
		open_lsn = ckp_lsn;
	} else if ((ret = __txn_ckp_read(dbenv, data.data, &ckp_args)) != 0) {
		__db_err(dbenv, "Invalid checkpoint record at [%ld][%ld]\n",
		    (u_long)ckp_lsn.file, (u_long)ckp_lsn.offset);
		goto out;
	} else if (IS_ZERO_LSN(ckp_args->last_ckp) ||
	    (ret = log_get(dbenv, &ckp_args->last_ckp, &data, DB_SET)) != 0)
		goto first;
	else
		open_lsn = ckp_args->last_ckp;

	if (dbenv->db_feedback != NULL) {
		if (last_lsn.file == open_lsn.file)
			nfiles = (float)(last_lsn.offset - open_lsn.offset) /
			    dbenv->lg_max;
		else
			nfiles = (float)(last_lsn.file - open_lsn.file) +
			    (float)(dbenv->lg_max - open_lsn.offset +
			    last_lsn.offset) / dbenv->lg_max;
		/* We are going to divide by nfiles; make sure it isn't 0. */
		if (nfiles == 0)
			nfiles = (float)0.001;
	}

	/*
	 * Pass #1
	 * Now, ckp_lsn is either the lsn of the last checkpoint
	 * or the lsn of the first record in the log.  Open_lsn is
	 * the second to last checkpoint or the beinning of the log;
	 * begin the open files pass from that lsn, and proceed to
	 * the end of the log.
	 */
	lsn = open_lsn;
	for (;;) {
		if (dbenv->db_feedback != NULL) {
			progress = (int)(33 * (__lsn_diff(&open_lsn,
			   &last_lsn, &lsn, dbenv->lg_max, 1) / nfiles));
			dbenv->db_feedback(dbenv, DB_RECOVER, progress);
		}
		ret = __db_dispatch(dbenv,
		    &data, &lsn, DB_TXN_OPENFILES, txninfo);
		if (ret != 0 && ret != DB_TXN_CKP)
			goto msgerr;
		if ((ret = log_get(dbenv, &lsn, &data, DB_NEXT)) != 0) {
			if (ret == DB_NOTFOUND)
				break;
			goto out;
		}
	}

	/*
	 * Pass #2.
	 *
	 * Before we can begin pass #2, backward roll phase, we determine how
	 * far back in the log to recover.  If we are doing catastrophic
	 * recovery, then we go as far back as we have files.  If we are
	 * doing normal recovery, we go as back to the most recent checkpoint
	 * that occurs before the most recent checkpoint LSN.  If we are
	 * recovering to a point in time, then rollback to the checkpoint whose
	 * ckp_lsn precedes the first log record (and then roll forward to
	 * the appropriate timestamp in Pass #3).
	 */
	if (LF_ISSET(DB_RECOVER_FATAL)) {
		ZERO_LSN(first_lsn);
	} else if (dbenv->tx_timestamp != 0)
		first_lsn = lowlsn;
	else
		if ((ret = __log_findckp(dbenv, &first_lsn)) == DB_NOTFOUND) {
			/*
			 * We don't require that log files exist if recovery
			 * was specified.
			 */
			ret = 0;
			goto out;
		}

	if (FLD_ISSET(dbenv->verbose, DB_VERB_RECOVERY))
		__db_err(dbenv, "Recovery starting from [%lu][%lu]",
		    (u_long)first_lsn.file, (u_long)first_lsn.offset);

	for (ret = log_get(dbenv, &lsn, &data, DB_LAST);
	    ret == 0 && log_compare(&lsn, &first_lsn) > 0;
	    ret = log_get(dbenv, &lsn, &data, DB_PREV)) {
		if (dbenv->db_feedback != NULL) {
			progress = 34 + (int)(33 * (__lsn_diff(&open_lsn,
			    &last_lsn, &lsn, dbenv->lg_max, 0) / nfiles));
			dbenv->db_feedback(dbenv, DB_RECOVER, progress);
		}
		ret = __db_dispatch(dbenv,
		    &data, &lsn, DB_TXN_BACKWARD_ROLL, txninfo);
		if (ret != 0) {
			if (ret != DB_TXN_CKP)
				goto msgerr;
			else
				ret = 0;
		}
	}
	if (ret != 0 && ret != DB_NOTFOUND)
		goto out;

	/*
	 * Pass #3.
	 */
	for (ret = log_get(dbenv, &lsn, &data, DB_NEXT);
	    ret == 0; ret = log_get(dbenv, &lsn, &data, DB_NEXT)) {
		if (dbenv->db_feedback != NULL) {
			progress = 67 + (int)(33 * (__lsn_diff(&open_lsn,
			    &last_lsn, &lsn, dbenv->lg_max, 1) / nfiles));
			dbenv->db_feedback(dbenv, DB_RECOVER, progress);
		}
		ret = __db_dispatch(dbenv,
		    &data, &lsn, DB_TXN_FORWARD_ROLL, txninfo);
		if (ret != 0) {
			if (ret != DB_TXN_CKP)
				goto msgerr;
			else
				ret = 0;
		}
	}
	if (ret != DB_NOTFOUND)
		goto out;

	/*
	 * Process any pages that were on the limbo list
	 * and move them to the free list.  Do this
	 * before checkpointing the database.
	 */
	 if ((ret = __db_do_the_limbo(dbenv, txninfo)) != 0)
		goto out;

	/*
	 * Now set the last checkpoint lsn and the current time,
	 * take a checkpoint, and reset the txnid.
	 */
	(void)time(&now);
	region = ((DB_TXNMGR *)dbenv->tx_handle)->reginfo.primary;
	region->last_txnid = ((DB_TXNHEAD *)txninfo)->maxid;
	region->last_ckp = ckp_lsn;
	region->time_ckp = (u_int32_t)now;

	/*
	 * Take two checkpoints so that we don't re-recover any of the
	 * work we've already done.
	 */
	if ((ret = txn_checkpoint(dbenv, 0, 0, DB_FORCE)) != 0)
		goto out;

	/* Now close all the db files that are open. */
	__log_close_files(dbenv);

	if ((ret = txn_checkpoint(dbenv, 0, 0, DB_FORCE)) != 0)
		goto out;
	region->last_txnid = TXN_MINIMUM;

	if (FLD_ISSET(dbenv->verbose, DB_VERB_RECOVERY)) {
		__db_err(dbenv, "Recovery complete at %.24s", ctime(&now));
		__db_err(dbenv, "%s %lx %s [%lu][%lu]",
		    "Maximum transaction ID",
		    ((DB_TXNHEAD *)txninfo)->maxid,
		    "Recovery checkpoint",
		    (u_long)region->last_ckp.file,
		    (u_long)region->last_ckp.offset);
	}

	if (0) {
msgerr:		__db_err(dbenv, "Recovery function for LSN %lu %lu failed",
		    (u_long)lsn.file, (u_long)lsn.offset);
	}

out:	if (is_thread)
		F_SET(dbenv, DB_ENV_THREAD);
	__db_txnlist_end(dbenv, txninfo);
	if (ckp_args != NULL)
		__os_free(ckp_args, sizeof(*ckp_args));
	F_CLR((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);

	dbenv->tx_timestamp = 0;
	return (ret);
}

/*
 * Figure out how many logfiles we have processed.  If we are moving
 * forward (is_forward != 0), then we're computing current - low.  If
 * we are moving backward, we are computing high - current.  max is
 * the number of bytes per logfile.
 */
static float
__lsn_diff(low, high, current, max, is_forward)
	DB_LSN *low, *high, *current;
	u_int32_t max;
	int is_forward;
{
	float nf;

	/*
	 * There are three cases in each direction.  If you are in the
	 * same file, then all you need worry about is the difference in
	 * offsets.  If you are in different files, then either your offsets
	 * put you either more or less than the integral difference in the
	 * number of files -- we need to handle both of these.
	 */
	if (is_forward) {
		if (current->file == low->file)
			nf = (float)(current->offset - low->offset) / max;
		else if (current->offset < low->offset)
			nf = (float)(current->file - low->file - 1) +
			    (float)(max - low->offset + current->offset) / max;
		else
			nf = (float)(current->file - low->file) +
			    (float)(current->offset - low->offset) / max;
	} else {
		if (current->file == high->file)
			nf = (float)(high->offset - current->offset) / max;
		else if (current->offset > high->offset)
			nf = (float)(high->file - current->file - 1) +
			    (float)(max - current->offset + high->offset) / max;
		else
			nf = (float)(high->file - current->file) +
			    (float)(high->offset - current->offset) / max;
	}
	return (nf);
}

/*
 * __log_earliest --
 *
 * Return the earliest recovery point for the log files present.  The
 * earliest recovery time is the time stamp of the first checkpoint record
 * whose checkpoint LSN is greater than the first LSN we process.
 */
static int
__log_earliest(dbenv, lowtime, lowlsn)
	DB_ENV *dbenv;
	int32_t *lowtime;
	DB_LSN *lowlsn;
{
	DB_LSN first_lsn, lsn;
	DBT data;
	__txn_ckp_args *ckpargs;
	u_int32_t rectype;
	int cmp, ret;

	memset(&data, 0, sizeof(data));
	/*
	 * Read forward through the log looking for the first checkpoint
	 * record whose ckp_lsn is greater than first_lsn.
	 */

	for (ret = log_get(dbenv, &first_lsn, &data, DB_FIRST);
	    ret == 0; ret = log_get(dbenv, &lsn, &data, DB_NEXT)) {
		if (ret != 0)
			break;
		memcpy(&rectype, data.data, sizeof(rectype));
		if (rectype != DB_txn_ckp)
			continue;
		if ((ret = __txn_ckp_read(dbenv, data.data, &ckpargs)) == 0) {
			cmp = log_compare(&ckpargs->ckp_lsn, &first_lsn);
			*lowlsn = ckpargs->ckp_lsn;
			*lowtime = ckpargs->timestamp;

			__os_free(ckpargs, 0);
			if (cmp >= 0)
				break;
		}
	}

	return (ret);
}
