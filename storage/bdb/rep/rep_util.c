/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_util.c,v 12.42 2005/10/27 01:26:02 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
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

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"
#ifdef REP_DIAGNOSTIC
#include "dbinc/db_page.h"
#include "dbinc/fop.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/qam.h"
#endif

/*
 * rep_util.c:
 *	Miscellaneous replication-related utility functions, including
 *	those called by other subsystems.
 */

#define	TIMESTAMP_CHECK(dbenv, ts, renv) do {				\
	if (renv->op_timestamp != 0 &&					\
	    renv->op_timestamp + DB_REGENV_TIMEOUT < ts) {		\
		REP_SYSTEM_LOCK(dbenv);					\
		F_CLR(renv, DB_REGENV_REPLOCKED);			\
		renv->op_timestamp = 0;					\
		REP_SYSTEM_UNLOCK(dbenv);				\
	}								\
} while (0)

#ifdef REP_DIAGNOSTIC
static void __rep_print_logmsg __P((DB_ENV *, const DBT *, DB_LSN *));
#endif

/*
 * __rep_bulk_message --
 *	This is a wrapper for putting a record into a bulk buffer.  Since
 * we have different bulk buffers, the caller must hand us the information
 * we need to put the record into the correct buffer.  All bulk buffers
 * are protected by the REP->mtx_clientdb.
 *
 * PUBLIC: int __rep_bulk_message __P((DB_ENV *, REP_BULK *, REP_THROTTLE *,
 * PUBLIC:     DB_LSN *, const DBT *, u_int32_t));
 */
int
__rep_bulk_message(dbenv, bulk, repth, lsn, dbt, flags)
	DB_ENV *dbenv;
	REP_BULK *bulk;
	REP_THROTTLE *repth;
	DB_LSN *lsn;
	const DBT *dbt;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REP *rep;
	int ret;
	u_int32_t recsize, typemore;
	u_int8_t *p;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	ret = 0;

	/*
	 * Figure out the total number of bytes needed for this record.
	 */
	recsize = dbt->size + sizeof(DB_LSN) + sizeof(dbt->size);

	/*
	 * If *this* buffer is actively being transmitted, wait until
	 * we can use it.
	 */
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	while (FLD_ISSET(*(bulk->flagsp), BULK_XMIT)) {
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		__os_sleep(dbenv, 1, 0);
		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	}

	/*
	 * If the record is bigger than the buffer entirely, send the
	 * current buffer and then return DB_REP_BULKOVF so that this
	 * record is sent as a singleton.  Do we have enough info to
	 * do that here?  XXX
	 */
	if (recsize > bulk->len) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "bulk_msg: Record %d (0x%x) larger than entire buffer 0x%x",
		    recsize, recsize, bulk->len));
		rep->stat.st_bulk_overflows++;
		(void)__rep_send_bulk(dbenv, bulk, flags);
		/*
		 * XXX __rep_send_message...
		 */
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		return (DB_REP_BULKOVF);
	}
	/*
	 * If this record doesn't fit, send the current buffer.
	 * Sending the buffer will reset the offset, but we will
	 * drop the mutex while sending so we need to keep checking
	 * if we're racing.
	 */
	while (recsize + *(bulk->offp) > bulk->len) {
		RPRINT(dbenv, rep, (dbenv, &mb,
	    "bulk_msg: Record %lu (%#lx) doesn't fit.  Send %lu (%#lx) now.",
		    (u_long)recsize, (u_long)recsize,
		    (u_long)bulk->len, (u_long)bulk->len));
		rep->stat.st_bulk_fills++;
		if ((ret = __rep_send_bulk(dbenv, bulk, flags)) != 0)
			break;
	}

	/*
	 * If we're using throttling, see if we are at the throttling
	 * limit before we do any more work here, by checking if the
	 * call to rep_send_throttle changed the repth->type to the
	 * *_MORE message type.  If the throttling code hits the limit
	 * then we're done here.
	 */
	if (bulk->type == REP_BULK_LOG)
		typemore = REP_LOG_MORE;
	else
		typemore = REP_PAGE_MORE;
	if (repth != NULL &&
	    (ret = __rep_send_throttle(dbenv, bulk->eid, repth,
	    REP_THROTTLE_ONLY)) == 0 && repth->type == typemore) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "bulk_msg: Record %d (0x%x) hit throttle limit.",
		    recsize, recsize));
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		return (ret);
	}

	/*
	 * Now we own the buffer, and we know our record fits into it.
	 * The buffer is structured with the len, LSN and then the record.
	 * Copy the record into the buffer.  Then if we need to,
	 * send the buffer.
	 */
	/*
	 * First thing is the length of the dbt record.
	 */
	p = bulk->addr + *(bulk->offp);
	memcpy(p, &dbt->size, sizeof(dbt->size));
	p += sizeof(dbt->size);
	/*
	 * The next thing is the LSN.  We need LSNs for both pages and
	 * log records.  For log records, this is obviously, the LSN of
	 * this record.  For pages, the LSN is used by the internal init code.
	 */
	memcpy(p, lsn, sizeof(DB_LSN));
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "bulk_msg: Copying LSN [%lu][%lu] of %lu bytes to %#lx",
	    (u_long)lsn->file, (u_long)lsn->offset, (u_long)dbt->size,
	    P_TO_ULONG(p)));
	p += sizeof(DB_LSN);
	/*
	 * If we're the first record, we need to save the first
	 * LSN in the bulk structure.
	 */
	if (*(bulk->offp) == 0)
		bulk->lsn = *lsn;
	/*
	 * Now copy the record and finally adjust the offset.
	 */
	memcpy(p, dbt->data, dbt->size);
	p += dbt->size;
	*(bulk->offp) = (uintptr_t)p - (uintptr_t)bulk->addr;
	rep->stat.st_bulk_records++;
	/*
	 * Send the buffer if it is a perm record or a force.
	 */
	if (LF_ISSET(DB_LOG_PERM) || FLD_ISSET(*(bulk->flagsp), BULK_FORCE)) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "bulk_msg: Send buffer after copy due to %s",
		    LF_ISSET(DB_LOG_PERM) ? "PERM" : "FORCE"));
		ret = __rep_send_bulk(dbenv, bulk, flags);
	}
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	return (ret);

}

/*
 * __rep_send_bulk --
 *	This function transmits the bulk buffer given.  It assumes the
 * caller holds the REP->mtx_clientdb.  We may release it and reacquire
 * it during this call.  We will return with it held.
 *
 * PUBLIC: int __rep_send_bulk __P((DB_ENV *, REP_BULK *, u_int32_t));
 */
int
__rep_send_bulk(dbenv, bulkp, flags)
	DB_ENV *dbenv;
	REP_BULK *bulkp;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REP *rep;
	DBT dbt;
	int ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	/*
	 * If the offset is 0, we're done.  There is nothing to send.
	 */
	if (*(bulkp->offp) == 0)
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	memset(&dbt, 0, sizeof(dbt));
	/*
	 * Set that this buffer is being actively transmitted.
	 */
	FLD_SET(*(bulkp->flagsp), BULK_XMIT);
	dbt.data = bulkp->addr;
	dbt.size = (u_int32_t)*(bulkp->offp);
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "send_bulk: Send %d (0x%x) bulk buffer bytes", dbt.size, dbt.size));
	/*
	 * Unlocked the mutex and now send the message.
	 */
	rep->stat.st_bulk_transfers++;
	ret = __rep_send_message(dbenv, bulkp->eid, bulkp->type, &bulkp->lsn,
	    &dbt, flags, 0);

	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	/*
	 * If we're successful, reset the offset pointer to 0.
	 * Clear the transmit flag regardless.
	 */
	if (ret == 0)
		*(bulkp->offp) = 0;
	FLD_CLR(*(bulkp->flagsp), BULK_XMIT);
	return (ret);
}

/*
 * __rep_bulk_alloc --
 *	This function allocates and initializes an internal bulk buffer.
 * This is used by the master when fulfilling a request for a chunk of
 * log records or a bunch of pages.
 *
 * PUBLIC: int __rep_bulk_alloc __P((DB_ENV *, REP_BULK *, int, uintptr_t *,
 * PUBLIC:    u_int32_t *, u_int32_t));
 */
int
__rep_bulk_alloc(dbenv, bulkp, eid, offp, flagsp, type)
	DB_ENV *dbenv;
	REP_BULK *bulkp;
	int eid;
	uintptr_t *offp;
	u_int32_t *flagsp, type;
{
	int ret;

	memset(bulkp, 0, sizeof(REP_BULK));
	*offp = *flagsp = 0;
	bulkp->len = MEGABYTE;
	if ((ret = __os_malloc(dbenv, bulkp->len, &bulkp->addr)) != 0)
		return (ret);
	bulkp->offp = offp;
	bulkp->type = type;
	bulkp->eid = eid;
	bulkp->flagsp = flagsp;
	return (ret);
}

/*
 * __rep_bulk_free --
 *	This function sends the remainder of the bulk buffer and frees it.
 *
 * PUBLIC: int __rep_bulk_free __P((DB_ENV *, REP_BULK *, u_int32_t));
 */
int
__rep_bulk_free(dbenv, bulkp, flags)
	DB_ENV *dbenv;
	REP_BULK *bulkp;
	u_int32_t flags;
{
	DB_REP *db_rep;
	int ret;

	db_rep = dbenv->rep_handle;

	MUTEX_LOCK(dbenv, db_rep->region->mtx_clientdb);
	ret = __rep_send_bulk(dbenv, bulkp, flags);
	MUTEX_UNLOCK(dbenv, db_rep->region->mtx_clientdb);
	__os_free(dbenv, bulkp->addr);
	return (ret);
}

/*
 * __rep_send_message --
 *	This is a wrapper for sending a message.  It takes care of constructing
 * the REP_CONTROL structure and calling the user's specified send function.
 *
 * PUBLIC: int __rep_send_message __P((DB_ENV *, int,
 * PUBLIC:     u_int32_t, DB_LSN *, const DBT *, u_int32_t, u_int32_t));
 */
int
__rep_send_message(dbenv, eid, rtype, lsnp, dbt, logflags, repflags)
	DB_ENV *dbenv;
	int eid;
	u_int32_t rtype;
	DB_LSN *lsnp;
	const DBT *dbt;
	u_int32_t logflags, repflags;
{
	DB_REP *db_rep;
	REP *rep;
	DBT cdbt, scrap_dbt;
	REP_CONTROL cntrl;
	int ret;
	u_int32_t myflags, rectype;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	/* Set up control structure. */
	memset(&cntrl, 0, sizeof(cntrl));
	if (lsnp == NULL)
		ZERO_LSN(cntrl.lsn);
	else
		cntrl.lsn = *lsnp;
	cntrl.rectype = rtype;
	cntrl.flags = logflags;
	cntrl.rep_version = DB_REPVERSION;
	cntrl.log_version = DB_LOGVERSION;
	cntrl.gen = rep->gen;

	memset(&cdbt, 0, sizeof(cdbt));
	cdbt.data = &cntrl;
	cdbt.size = sizeof(cntrl);

	/* Don't assume the send function will be tolerant of NULL records. */
	if (dbt == NULL) {
		memset(&scrap_dbt, 0, sizeof(DBT));
		dbt = &scrap_dbt;
	}

	REP_PRINT_MESSAGE(dbenv, eid, &cntrl, "rep_send_message");
#ifdef REP_DIAGNOSTIC
	if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION) && rtype == REP_LOG)
		__rep_print_logmsg(dbenv, dbt, lsnp);
#endif
	/*
	 * There are several types of records: commit and checkpoint records
	 * that affect database durability, regular log records that might
	 * be buffered on the master before being transmitted, and control
	 * messages which don't require the guarantees of permanency, but
	 * should not be buffered.
	 *
	 * There are request records that can be sent anywhere, and there
	 * are rerequest records that the app might want to send to the master.
	 */
	myflags = repflags;
	if (FLD_ISSET(logflags, DB_LOG_PERM))
		myflags |= DB_REP_PERMANENT;
	else if (rtype != REP_LOG || FLD_ISSET(logflags, DB_LOG_RESEND))
		myflags |= DB_REP_NOBUFFER;
	if (rtype == REP_LOG && !FLD_ISSET(logflags, DB_LOG_PERM)) {
		/*
		 * Check if this is a log record we just read that
		 * may need a DB_LOG_PERM.  This is of type REP_LOG,
		 * so we know that dbt is a log record.
		 */
		memcpy(&rectype, dbt->data, sizeof(rectype));
		if (rectype == DB___txn_regop || rectype == DB___txn_ckp)
			F_SET(&cntrl, DB_LOG_PERM);
	 }

	/*
	 * We set the LSN above to something valid.  Give the master the
	 * actual LSN so that they can coordinate with permanent records from
	 * the client if they want to.
	 */
	ret = dbenv->rep_send(dbenv, &cdbt, dbt, &cntrl.lsn, eid, myflags);

	/*
	 * We don't hold the rep lock, so this could miscount if we race.
	 * I don't think it's worth grabbing the mutex for that bit of
	 * extra accuracy.
	 */
	if (ret == 0)
		rep->stat.st_msgs_sent++;
	else {
		rep->stat.st_msgs_send_failures++;
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "rep_send_function returned: %d", ret));
	}
	return (ret);
}

#ifdef REP_DIAGNOSTIC
/*
 * __rep_print_logmsg --
 *	This is a debugging routine for printing out log records that
 * we are about to transmit to a client.
 */
static void
__rep_print_logmsg(dbenv, logdbt, lsnp)
	DB_ENV *dbenv;
	const DBT *logdbt;
	DB_LSN *lsnp;
{
	/* Static structures to hold the printing functions. */
	static int (**ptab)__P((DB_ENV *,
	    DBT *, DB_LSN *, db_recops, void *)) = NULL;
	size_t ptabsize = 0;

	if (ptabsize == 0) {
		/* Initialize the table. */
		(void)__bam_init_print(dbenv, &ptab, &ptabsize);
		(void)__crdel_init_print(dbenv, &ptab, &ptabsize);
		(void)__db_init_print(dbenv, &ptab, &ptabsize);
		(void)__dbreg_init_print(dbenv, &ptab, &ptabsize);
		(void)__fop_init_print(dbenv, &ptab, &ptabsize);
		(void)__ham_init_print(dbenv, &ptab, &ptabsize);
		(void)__qam_init_print(dbenv, &ptab, &ptabsize);
		(void)__txn_init_print(dbenv, &ptab, &ptabsize);
	}

	(void)__db_dispatch(dbenv,
	    ptab, ptabsize, (DBT *)logdbt, lsnp, DB_TXN_PRINT, NULL);
}
#endif

/*
 * __rep_new_master --
 *	Called after a master election to sync back up with a new master.
 * It's possible that we already know of this new master in which case
 * we don't need to do anything.
 *
 * This is written assuming that this message came from the master; we
 * need to enforce that in __rep_process_record, but right now, we have
 * no way to identify the master.
 *
 * PUBLIC: int __rep_new_master __P((DB_ENV *, REP_CONTROL *, int));
 */
int
__rep_new_master(dbenv, cntrl, eid)
	DB_ENV *dbenv;
	REP_CONTROL *cntrl;
	int eid;
{
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN first_lsn, lsn;
	DB_REP *db_rep;
	DBT dbt;
	LOG *lp;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	int change, do_req, ret, t_ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	ret = 0;
	logc = NULL;
	REP_SYSTEM_LOCK(dbenv);
	__rep_elect_done(dbenv, rep);
	change = rep->gen != cntrl->gen || rep->master_id != eid;
	if (change) {
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Updating gen from %lu to %lu from master %d",
		    (u_long)rep->gen, (u_long)cntrl->gen, eid));
		rep->gen = cntrl->gen;
		if (rep->egen <= rep->gen)
			rep->egen = rep->gen + 1;
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Egen is %lu", (u_long)rep->egen));
		rep->master_id = eid;
		rep->stat.st_master_changes++;
		rep->stat.st_startup_complete = 0;
		/*
		 * If we're delaying client sync-up, we know we have a
		 * new/changed master now, set flag indicating we are
		 * actively delaying.
		 */
		if (FLD_ISSET(rep->config, REP_C_DELAYCLIENT))
			F_SET(rep, REP_F_DELAY);
		/*
		 * If we are already locking out others, we're either
		 * in the middle of sync-up recovery or internal init
		 * when this newmaster comes in (we also lockout in
		 * rep_start, but we cannot be racing that because we
		 * don't allow rep_proc_msg when rep_start is going on).
		 *
		 * If we were in the middle of an internal initialization
		 * and we've discovered a new master instead, clean up
		 * our old internal init information.  We need to clean
		 * up any flags and unlock our lockout.
		 */
		if (rep->in_recovery || F_ISSET(rep, REP_F_READY)) {
			(void)__rep_init_cleanup(dbenv, rep, DB_FORCE);
			F_CLR(rep, REP_F_RECOVER_MASK);
			rep->in_recovery = 0;
			F_CLR(rep, REP_F_READY);
		}
		F_SET(rep, REP_F_NOARCHIVE | REP_F_RECOVER_VERIFY);
	}
	REP_SYSTEM_UNLOCK(dbenv);

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	LOG_SYSTEM_LOCK(dbenv);
	lsn = lp->lsn;
	LOG_SYSTEM_UNLOCK(dbenv);

	if (!change) {
		/*
		 * If there wasn't a change, we might still have some
		 * catching up or verification to do.
		 */
		ret = 0;
		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		do_req = __rep_check_doreq(dbenv, rep);
		if (F_ISSET(rep, REP_F_RECOVER_VERIFY)) {
			lsn = lp->verify_lsn;
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			if (!F_ISSET(rep, REP_F_DELAY) &&
			    !IS_ZERO_LSN(lsn) && do_req)
				(void)__rep_send_message(dbenv, eid,
				    REP_VERIFY_REQ, &lsn, NULL, 0,
				    DB_REP_ANYWHERE);
		} else {
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			if (log_compare(&lsn, &cntrl->lsn) < 0 && do_req)
				(void)__rep_send_message(dbenv, eid,
				    REP_ALL_REQ, &lsn, NULL,
				    0, DB_REP_ANYWHERE);
			REP_SYSTEM_LOCK(dbenv);
			F_CLR(rep, REP_F_NOARCHIVE);
			REP_SYSTEM_UNLOCK(dbenv);
		}
		return (ret);
	}

	/*
	 * If the master changed, we need to start the process of
	 * figuring out what our last valid log record is.  However,
	 * if both the master and we agree that the max LSN is 0,0,
	 * then there is no recovery to be done.  If we are at 0 and
	 * the master is not, then we just need to request all the log
	 * records from the master.
	 */
	if (IS_INIT_LSN(lsn) || IS_ZERO_LSN(lsn)) {
		/*
		 * If we have no log, then we have no files to open
		 * in recovery, but we've opened what we can, which
		 * is none.  Mark DBREP_OPENFILES here.
		 */
empty:		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		F_SET(db_rep, DBREP_OPENFILES);
		ZERO_LSN(lp->verify_lsn);
		REP_SYSTEM_LOCK(dbenv);
		F_CLR(rep, REP_F_NOARCHIVE | REP_F_RECOVER_MASK);
		REP_SYSTEM_UNLOCK(dbenv);

		if (!IS_INIT_LSN(cntrl->lsn)) {
			/*
			 * We're making an ALL_REQ.  But now that we've
			 * cleared the flags, we're likely receiving new
			 * log records from the master, resulting in a gap
			 * immediately.  So to avoid multiple data streams,
			 * set the wait_recs value high now to give the master
			 * a chance to start sending us these records before
			 * the gap code re-requests the same gap.  Wait_recs
			 * will get reset once we start receiving these
			 * records.
			 */
			lp->wait_recs = rep->max_gap;
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			/*
			 * Don't send the ALL_REQ if we're delayed.  But we
			 * check here, after lp->wait_recs is set up so that
			 * when the app calls rep_sync, everything is ready
			 * to go.
			 */
			if (!F_ISSET(rep, REP_F_DELAY))
				(void)__rep_send_message(dbenv, eid,
				    REP_ALL_REQ, &lsn, NULL,
				    0, DB_REP_ANYWHERE);
		} else
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);

		return (DB_REP_NEWMASTER);
	}

	memset(&dbt, 0, sizeof(dbt));
	/*
	 * If this client is farther ahead on the log file than the master, see
	 * if there is any overlap in the logs.  If not, the client is too
	 * far ahead of the master and we cannot determine they're part of
	 * the same replication group.
	 */
	if (cntrl->lsn.file < lsn.file) {
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto err;
		if ((ret = __log_c_get(logc, &first_lsn, &dbt, DB_FIRST)) != 0)
			goto err;
		if (cntrl->lsn.file < first_lsn.file) {
			__db_err(dbenv,
    "Client too far ahead of master; unable to join replication group");
			ret = DB_REP_JOIN_FAILURE;
			goto err;
		}
		ret = __log_c_close(logc);
		logc = NULL;
		if (ret != 0)
			goto err;
	}
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		goto err;
	ret = __rep_log_backup(logc, &lsn);
err:	if (logc != NULL && (t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	if (ret == DB_NOTFOUND) {
		/*
		 * If we don't have an identification record, we still might
		 * have some log records but we're discarding them to sync
		 * up with the master from the start.  Therefore,
		 * truncate our log and go to the no log case.
		 */
		INIT_LSN(lsn);
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "No commit or ckp found.  Truncate log."));
		(void)__log_vtruncate(dbenv, &lsn, &lsn, NULL);
		infop = dbenv->reginfo;
		renv = infop->primary;
		REP_SYSTEM_LOCK(dbenv);
		(void)time(&renv->rep_timestamp);
		REP_SYSTEM_UNLOCK(dbenv);
		goto empty;
	}

	/*
	 * If we failed here, we need to clear the flags we may
	 * have set above because we're not going to be setting
	 * the verify_lsn.
	 */
	if (ret != 0) {
		REP_SYSTEM_LOCK(dbenv);
		F_CLR(rep, REP_F_RECOVER_MASK | REP_F_DELAY);
		REP_SYSTEM_UNLOCK(dbenv);
		return (ret);
	}

	/*
	 * Finally, we have a record to ask for.
	 */
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	lp->verify_lsn = lsn;
	lp->rcvd_recs = 0;
	lp->wait_recs = rep->request_gap;
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	if (!F_ISSET(rep, REP_F_DELAY))
		(void)__rep_send_message(dbenv,
		    eid, REP_VERIFY_REQ, &lsn, NULL, 0, DB_REP_ANYWHERE);

	return (DB_REP_NEWMASTER);
}

/*
 * __rep_is_client
 *	Used by other subsystems to figure out if this is a replication
 * client site.
 *
 * PUBLIC: int __rep_is_client __P((DB_ENV *));
 */
int
__rep_is_client(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;

	if (!REP_ON(dbenv))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	/*
	 * Don't just return F_ISSET since that converts unsigned
	 * into signed.
	 */
	return (F_ISSET(rep, REP_F_CLIENT) ? 1 : 0);
}

/*
 * __rep_noarchive
 *	Used by log_archive to determine if it is okay to remove
 * log files.
 *
 * PUBLIC: int __rep_noarchive __P((DB_ENV *));
 */
int
__rep_noarchive(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	time_t timestamp;

	infop = dbenv->reginfo;
	renv = infop->primary;

	/*
	 * This is tested before REP_ON below because we always need
	 * to obey if any replication process has disabled archiving.
	 * Everything is in the environment region that we need here.
	 */
	if (F_ISSET(renv, DB_REGENV_REPLOCKED)) {
		(void)time(&timestamp);
		TIMESTAMP_CHECK(dbenv, timestamp, renv);
		/*
		 * Check if we're still locked out after checking
		 * the timestamp.
		 */
		if (F_ISSET(renv, DB_REGENV_REPLOCKED))
			return (EINVAL);
	}

	if (!REP_ON(dbenv))
		return (0);
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	if (F_ISSET(rep, REP_F_NOARCHIVE))
		return (1);
	return (0);
}

/*
 * __rep_send_vote
 *	Send this site's vote for the election.
 *
 * PUBLIC: void __rep_send_vote __P((DB_ENV *, DB_LSN *, int, int, int,
 * PUBLIC:    u_int32_t, u_int32_t, int, u_int32_t));
 */
void
__rep_send_vote(dbenv, lsnp, nsites, nvotes, pri, tie, egen, eid, vtype)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	int eid, nsites, nvotes, pri;
	u_int32_t egen, tie, vtype;
{
	DBT vote_dbt;
	REP_VOTE_INFO vi;

	memset(&vi, 0, sizeof(vi));

	vi.egen = egen;
	vi.priority = pri;
	vi.nsites = nsites;
	vi.nvotes = nvotes;
	vi.tiebreaker = tie;

	memset(&vote_dbt, 0, sizeof(vote_dbt));
	vote_dbt.data = &vi;
	vote_dbt.size = sizeof(vi);

	(void)__rep_send_message(dbenv, eid, vtype, lsnp, &vote_dbt, 0, 0);
}

/*
 * __rep_elect_done
 *	Clear all election information for this site.  Assumes the
 *	caller hold the region mutex.
 *
 * PUBLIC: void __rep_elect_done __P((DB_ENV *, REP *));
 */
void
__rep_elect_done(dbenv, rep)
	DB_ENV *dbenv;
	REP *rep;
{
	int inelect;
	u_int32_t endsec, endusec;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#else
	COMPQUIET(dbenv, NULL);
#endif
	inelect = IN_ELECTION_TALLY(rep);
	F_CLR(rep, REP_F_EPHASE1 | REP_F_EPHASE2 | REP_F_TALLY);
	rep->sites = 0;
	rep->votes = 0;
	if (inelect) {
		if (rep->esec != 0) {
			__os_clock(dbenv, &endsec, &endusec);
			__db_difftime(rep->esec, endsec, rep->eusec, endusec,
			    &rep->stat.st_election_sec,
			    &rep->stat.st_election_usec);
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Election finished in %u.%06u sec",
			    rep->stat.st_election_sec,
			    rep->stat.st_election_usec));
			rep->esec = 0;
			rep->eusec = 0;
		}
		rep->egen++;
	}
	RPRINT(dbenv, rep, (dbenv, &mb,
	    "Election done; egen %lu", (u_long)rep->egen));
}

/*
 * __rep_grow_sites --
 *	Called to allocate more space in the election tally information.
 * Called with the rep mutex held.  We need to call the region mutex, so
 * we need to make sure that we *never* acquire those mutexes in the
 * opposite order.
 *
 * PUBLIC: int __rep_grow_sites __P((DB_ENV *dbenv, int nsites));
 */
int
__rep_grow_sites(dbenv, nsites)
	DB_ENV *dbenv;
	int nsites;
{
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	int nalloc, ret, *tally;

	rep = ((DB_REP *)dbenv->rep_handle)->region;

	/*
	 * Allocate either twice the current allocation or nsites,
	 * whichever is more.
	 */
	nalloc = 2 * rep->asites;
	if (nalloc < nsites)
		nalloc = nsites;

	infop = dbenv->reginfo;
	renv = infop->primary;
	MUTEX_LOCK(dbenv, renv->mtx_regenv);

	/*
	 * We allocate 2 tally regions, one for tallying VOTE1's and
	 * one for VOTE2's.  Always grow them in tandem, because if we
	 * get more VOTE1's we'll always expect more VOTE2's then too.
	 */
	if ((ret = __db_shalloc(infop,
	    (size_t)nalloc * sizeof(REP_VTALLY), sizeof(REP_VTALLY),
	    &tally)) == 0) {
		if (rep->tally_off != INVALID_ROFF)
			 __db_shalloc_free(
			     infop, R_ADDR(infop, rep->tally_off));
		rep->tally_off = R_OFFSET(infop, tally);
		if ((ret = __db_shalloc(infop,
		    (size_t)nalloc * sizeof(REP_VTALLY), sizeof(REP_VTALLY),
		    &tally)) == 0) {
			/* Success */
			if (rep->v2tally_off != INVALID_ROFF)
				 __db_shalloc_free(infop,
				    R_ADDR(infop, rep->v2tally_off));
			rep->v2tally_off = R_OFFSET(infop, tally);
			rep->asites = nalloc;
			rep->nsites = nsites;
		} else {
			/*
			 * We were unable to allocate both.  So, we must
			 * free the first one and reinitialize.  If
			 * v2tally_off is valid, it is from an old
			 * allocation and we are clearing it all out due
			 * to the error.
			 */
			if (rep->v2tally_off != INVALID_ROFF)
				 __db_shalloc_free(infop,
				    R_ADDR(infop, rep->v2tally_off));
			__db_shalloc_free(infop,
			    R_ADDR(infop, rep->tally_off));
			rep->v2tally_off = rep->tally_off = INVALID_ROFF;
			rep->asites = 0;
			rep->nsites = 0;
		}
	}
	MUTEX_UNLOCK(dbenv, renv->mtx_regenv);
	return (ret);
}

/*
 * __env_rep_enter --
 *
 * Check if we are in the middle of replication initialization and/or
 * recovery, and if so, disallow operations.  If operations are allowed,
 * increment handle-counts, so that we do not start recovery while we
 * are operating in the library.
 *
 * PUBLIC: int __env_rep_enter __P((DB_ENV *, int));
 */
int
__env_rep_enter(dbenv, checklock)
	DB_ENV *dbenv;
	int checklock;
{
	DB_REP *db_rep;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	int cnt;
	time_t	timestamp;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	infop = dbenv->reginfo;
	renv = infop->primary;
	if (checklock && F_ISSET(renv, DB_REGENV_REPLOCKED)) {
		(void)time(&timestamp);
		TIMESTAMP_CHECK(dbenv, timestamp, renv);
		/*
		 * Check if we're still locked out after checking
		 * the timestamp.
		 */
		if (F_ISSET(renv, DB_REGENV_REPLOCKED))
			return (EINVAL);
	}

	REP_SYSTEM_LOCK(dbenv);
	for (cnt = 0; rep->in_recovery;) {
		REP_SYSTEM_UNLOCK(dbenv);
		if (FLD_ISSET(rep->config, REP_C_NOWAIT)) {
			__db_err(dbenv,
    "Operation locked out.  Waiting for replication recovery to complete");
			return (DB_REP_LOCKOUT);
		}
		__os_sleep(dbenv, 1, 0);
		REP_SYSTEM_LOCK(dbenv);
		if (++cnt % 60 == 0)
			__db_err(dbenv,
    "DB_ENV handle waiting %d minutes for replication recovery to complete",
			    cnt / 60);
	}
	rep->handle_cnt++;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __env_db_rep_exit --
 *
 *	Decrement handle count upon routine exit.
 *
 * PUBLIC: int __env_db_rep_exit __P((DB_ENV *));
 */
int
__env_db_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	rep->handle_cnt--;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __db_rep_enter --
 *	Called in replicated environments to keep track of in-use handles
 * and prevent any concurrent operation during recovery.  If checkgen is
 * non-zero, then we verify that the dbp has the same handle as the env.
 *
 * If return_now is non-zero, we'll return DB_DEADLOCK immediately, else we'll
 * sleep before returning DB_DEADLOCK.  Without the sleep, it is likely
 * the application will immediately try again and could reach a retry
 * limit before replication has a chance to finish.  The sleep increases
 * the probability that an application retry will succeed.
 *
 * PUBLIC: int __db_rep_enter __P((DB *, int, int, int));
 */
int
__db_rep_enter(dbp, checkgen, checklock, return_now)
	DB *dbp;
	int checkgen, checklock, return_now;
{
	DB_ENV *dbenv;
	DB_REP *db_rep;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	time_t	timestamp;

	dbenv = dbp->dbenv;
	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	infop = dbenv->reginfo;
	renv = infop->primary;

	if (checklock && F_ISSET(renv, DB_REGENV_REPLOCKED)) {
		(void)time(&timestamp);
		TIMESTAMP_CHECK(dbenv, timestamp, renv);
		/*
		 * Check if we're still locked out after checking
		 * the timestamp.
		 */
		if (F_ISSET(renv, DB_REGENV_REPLOCKED))
			return (EINVAL);
	}
	REP_SYSTEM_LOCK(dbenv);
	if (F_ISSET(rep, REP_F_READY)) {
		REP_SYSTEM_UNLOCK(dbenv);
		if (!return_now)
			__os_sleep(dbenv, 5, 0);
		return (DB_LOCK_DEADLOCK);
	}

	if (checkgen && dbp->timestamp != renv->rep_timestamp) {
		REP_SYSTEM_UNLOCK(dbenv);
		__db_err(dbenv, "%s %s",
		    "replication recovery unrolled committed transactions;",
		    "open DB and DBcursor handles must be closed");
		return (DB_REP_HANDLE_DEAD);
	}
	rep->handle_cnt++;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __op_rep_enter --
 *
 *	Check if we are in the middle of replication initialization and/or
 * recovery, and if so, disallow new multi-step operations, such as
 * transaction and memp gets.  If operations are allowed,
 * increment the op_cnt, so that we do not start recovery while we have
 * active operations.
 *
 * PUBLIC: int __op_rep_enter __P((DB_ENV *));
 */
int
__op_rep_enter(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;
	int cnt;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	for (cnt = 0; F_ISSET(rep, REP_F_READY);) {
		REP_SYSTEM_UNLOCK(dbenv);
		if (FLD_ISSET(rep->config, REP_C_NOWAIT)) {
			__db_err(dbenv,
    "Operation locked out.  Waiting for replication recovery to complete");
			return (DB_REP_LOCKOUT);
		}
		__os_sleep(dbenv, 5, 0);
		cnt += 5;
		REP_SYSTEM_LOCK(dbenv);
		if (cnt % 60 == 0)
			__db_err(dbenv,
	"__op_rep_enter waiting %d minutes for op count to drain",
			    cnt / 60);
	}
	rep->op_cnt++;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __op_rep_exit --
 *
 *	Decrement op count upon transaction commit/abort/discard or
 *	memp_fput.
 *
 * PUBLIC: int __op_rep_exit __P((DB_ENV *));
 */
int
__op_rep_exit(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	DB_ASSERT(rep->op_cnt > 0);
	rep->op_cnt--;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __rep_get_gen --
 *
 *	Get the generation number from a replicated environment.
 *
 * PUBLIC: int __rep_get_gen __P((DB_ENV *, u_int32_t *));
 */
int
__rep_get_gen(dbenv, genp)
	DB_ENV *dbenv;
	u_int32_t *genp;
{
	DB_REP *db_rep;
	REP *rep;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	REP_SYSTEM_LOCK(dbenv);
	if (rep->recover_gen > rep->gen)
		*genp = rep->recover_gen;
	else
		*genp = rep->gen;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __rep_lockout --
 *	Coordinate with other threads in the library and active txns so
 *	that we can run single-threaded, for recovery or internal backup.
 *	Assumes the caller holds the region mutex.
 *
 * PUBLIC: int __rep_lockout __P((DB_ENV *, REP *, u_int32_t));
 */
int
__rep_lockout(dbenv, rep, msg_th)
	DB_ENV *dbenv;
	REP *rep;
	u_int32_t msg_th;
{
	int wait_cnt;

	/* Phase 1: set REP_F_READY and wait for op_cnt to go to 0. */
	F_SET(rep, REP_F_READY);
	for (wait_cnt = 0; rep->op_cnt != 0;) {
		REP_SYSTEM_UNLOCK(dbenv);
		__os_sleep(dbenv, 1, 0);
#if defined(DIAGNOSTIC) || defined(CONFIG_TEST)
		if (++wait_cnt % 60 == 0)
			__db_err(dbenv,
	"Waiting for txn_cnt to run replication recovery/backup for %d minutes",
			wait_cnt / 60);
#endif
		REP_SYSTEM_LOCK(dbenv);
	}

	/*
	 * Phase 2: set in_recovery and wait for handle count to go
	 * to 0 and for the number of threads in __rep_process_message
	 * to go to 1 (us).
	 */
	rep->in_recovery = 1;
	for (wait_cnt = 0; rep->handle_cnt != 0 || rep->msg_th > msg_th;) {
		REP_SYSTEM_UNLOCK(dbenv);
		__os_sleep(dbenv, 1, 0);
#ifdef DIAGNOSTIC
		if (++wait_cnt % 60 == 0)
			__db_err(dbenv,
"Waiting for handle count to run replication recovery/backup for %d minutes",
			wait_cnt / 60);
#endif
		REP_SYSTEM_LOCK(dbenv);
	}

	return (0);
}

/*
 * __rep_send_throttle -
 *	Send a record, throttling if necessary.  Callers of this function
 * will throttle - breaking out of their loop, if the repth->type field
 * changes from the normal message type to the *_MORE message type.
 * This function will send the normal type unless throttling gets invoked.
 * Then it sets the type field and sends the _MORE message.
 *
 * PUBLIC: int __rep_send_throttle __P((DB_ENV *, int, REP_THROTTLE *,
 * PUBLIC:    u_int32_t));
 */
int
__rep_send_throttle(dbenv, eid, repth, flags)
	DB_ENV *dbenv;
	int eid;
	REP_THROTTLE *repth;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REP *rep;
	u_int32_t size, typemore;
	int check_limit;

	check_limit = repth->gbytes != 0 || repth->bytes != 0;
	/*
	 * If we only want to do throttle processing and we don't have it
	 * turned on, return immediately.
	 */
	if (!check_limit && LF_ISSET(REP_THROTTLE_ONLY))
		return (0);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	typemore = 0;
	if (repth->type == REP_LOG)
		typemore = REP_LOG_MORE;
	if (repth->type == REP_PAGE)
		typemore = REP_PAGE_MORE;
	DB_ASSERT(typemore != 0);

	/*
	 * data_dbt.size is only the size of the log
	 * record;  it doesn't count the size of the
	 * control structure. Factor that in as well
	 * so we're not off by a lot if our log records
	 * are small.
	 */
	size = repth->data_dbt->size + sizeof(REP_CONTROL);
	if (check_limit) {
		if (repth->lsn.offset == 28) {
			repth->type = typemore;
			goto send;
		}
		while (repth->bytes <= size) {
			if (repth->gbytes > 0) {
				repth->bytes += GIGABYTE;
				--(repth->gbytes);
				continue;
			}
			/*
			 * We don't hold the rep mutex,
			 * and may miscount.
			 */
			rep->stat.st_nthrottles++;
			repth->type = typemore;
			goto send;
		}
		repth->bytes -= size;
	}
	/*
	 * Always send if it is typemore, otherwise send only if
	 * REP_THROTTLE_ONLY is not set.
	 */
send:	if ((repth->type == typemore || !LF_ISSET(REP_THROTTLE_ONLY)) &&
	    (__rep_send_message(dbenv, eid, repth->type,
	    &repth->lsn, repth->data_dbt, DB_LOG_RESEND, 0) != 0))
		return (1);
	return (0);
}

#ifdef DIAGNOSTIC
/*
 * PUBLIC: void __rep_print_message __P((DB_ENV *, int, REP_CONTROL *, char *));
 */
void
__rep_print_message(dbenv, eid, rp, str)
	DB_ENV *dbenv;
	int eid;
	REP_CONTROL *rp;
	char *str;
{
	DB_MSGBUF mb;
	char *type;

	switch (rp->rectype) {
	case REP_ALIVE:
		type = "alive";
		break;
	case REP_ALIVE_REQ:
		type = "alive_req";
		break;
	case REP_ALL_REQ:
		type = "all_req";
		break;
	case REP_BULK_LOG:
		type = "bulk_log";
		break;
	case REP_BULK_PAGE:
		type = "bulk_page";
		break;
	case REP_DUPMASTER:
		type = "dupmaster";
		break;
	case REP_FILE:
		type = "file";
		break;
	case REP_FILE_FAIL:
		type = "file_fail";
		break;
	case REP_FILE_REQ:
		type = "file_req";
		break;
	case REP_LOG:
		type = "log";
		break;
	case REP_LOG_MORE:
		type = "log_more";
		break;
	case REP_LOG_REQ:
		type = "log_req";
		break;
	case REP_MASTER_REQ:
		type = "master_req";
		break;
	case REP_NEWCLIENT:
		type = "newclient";
		break;
	case REP_NEWFILE:
		type = "newfile";
		break;
	case REP_NEWMASTER:
		type = "newmaster";
		break;
	case REP_NEWSITE:
		type = "newsite";
		break;
	case REP_PAGE:
		type = "page";
		break;
	case REP_PAGE_FAIL:
		type = "page_fail";
		break;
	case REP_PAGE_MORE:
		type = "page_more";
		break;
	case REP_PAGE_REQ:
		type = "page_req";
		break;
	case REP_REREQUEST:
		type = "rerequest";
		break;
	case REP_UPDATE:
		type = "update";
		break;
	case REP_UPDATE_REQ:
		type = "update_req";
		break;
	case REP_VERIFY:
		type = "verify";
		break;
	case REP_VERIFY_FAIL:
		type = "verify_fail";
		break;
	case REP_VERIFY_REQ:
		type = "verify_req";
		break;
	case REP_VOTE1:
		type = "vote1";
		break;
	case REP_VOTE2:
		type = "vote2";
		break;
	default:
		type = "NOTYPE";
		break;
	}
	RPRINT(dbenv, ((REP *)((DB_REP *)(dbenv)->rep_handle)->region),
	    (dbenv, &mb, "%s %s: gen = %lu eid %d, type %s, LSN [%lu][%lu]",
	    dbenv->db_home, str, (u_long)rp->gen,
	    eid, type, (u_long)rp->lsn.file, (u_long)rp->lsn.offset));
}
#endif
