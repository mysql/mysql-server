/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: rep_util.c,v 1.51 2002/09/05 02:30:00 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/fop.h"
#include "dbinc/hash.h"
#include "dbinc/log.h"
#include "dbinc/qam.h"
#include "dbinc/rep.h"
#include "dbinc/txn.h"

/*
 * rep_util.c:
 *	Miscellaneous replication-related utility functions, including
 *	those called by other subsystems.
 */
static int __rep_cmp_bylsn __P((const void *, const void *));
static int __rep_cmp_bypage __P((const void *, const void *));

#ifdef REP_DIAGNOSTIC
static void __rep_print_logmsg __P((DB_ENV *, const DBT *, DB_LSN *));
#endif

/*
 * __rep_check_alloc --
 *	Make sure the array of TXN_REC entries is of at least size n.
 *	(This function is called by the __*_getpgnos() functions in
 *	*.src.)
 *
 * PUBLIC: int __rep_check_alloc __P((DB_ENV *, TXN_RECS *, int));
 */
int
__rep_check_alloc(dbenv, r, n)
	DB_ENV *dbenv;
	TXN_RECS *r;
	int n;
{
	int nalloc, ret;

	while (r->nalloc < r->npages + n) {
		nalloc = r->nalloc == 0 ? 20 : r->nalloc * 2;

		if ((ret = __os_realloc(dbenv, nalloc * sizeof(LSN_PAGE),
		    &r->array)) != 0)
			return (ret);

		r->nalloc = nalloc;
	}

	return (0);
}

/*
 * __rep_send_message --
 *	This is a wrapper for sending a message.  It takes care of constructing
 * the REP_CONTROL structure and calling the user's specified send function.
 *
 * PUBLIC: int __rep_send_message __P((DB_ENV *, int,
 * PUBLIC:     u_int32_t, DB_LSN *, const DBT *, u_int32_t));
 */
int
__rep_send_message(dbenv, eid, rtype, lsnp, dbtp, flags)
	DB_ENV *dbenv;
	int eid;
	u_int32_t rtype;
	DB_LSN *lsnp;
	const DBT *dbtp;
	u_int32_t flags;
{
	DB_REP *db_rep;
	REP *rep;
	DBT cdbt, scrap_dbt;
	REP_CONTROL cntrl;
	u_int32_t send_flags;
	int ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	/* Set up control structure. */
	memset(&cntrl, 0, sizeof(cntrl));
	if (lsnp == NULL)
		ZERO_LSN(cntrl.lsn);
	else
		cntrl.lsn = *lsnp;
	cntrl.rectype = rtype;
	cntrl.flags = flags;
	cntrl.rep_version = DB_REPVERSION;
	cntrl.log_version = DB_LOGVERSION;
	MUTEX_LOCK(dbenv, db_rep->mutexp);
	cntrl.gen = rep->gen;
	MUTEX_UNLOCK(dbenv, db_rep->mutexp);

	memset(&cdbt, 0, sizeof(cdbt));
	cdbt.data = &cntrl;
	cdbt.size = sizeof(cntrl);

	/* Don't assume the send function will be tolerant of NULL records. */
	if (dbtp == NULL) {
		memset(&scrap_dbt, 0, sizeof(DBT));
		dbtp = &scrap_dbt;
	}

	send_flags = (LF_ISSET(DB_PERMANENT) ? DB_REP_PERMANENT : 0);

#if 0
	__rep_print_message(dbenv, eid, &cntrl, "rep_send_message");
#endif
#ifdef REP_DIAGNOSTIC
	if (rtype == REP_LOG)
		__rep_print_logmsg(dbenv, dbtp, lsnp);
#endif
	ret = db_rep->rep_send(dbenv, &cdbt, dbtp, eid, send_flags);

	/*
	 * We don't hold the rep lock, so this could miscount if we race.
	 * I don't think it's worth grabbing the mutex for that bit of
	 * extra accuracy.
	 */
	if (ret == 0)
		rep->stat.st_msgs_sent++;
	else
		rep->stat.st_msgs_send_failures++;

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
		(void)__qam_init_print(dbenv, &ptab, &ptabsize);
		(void)__ham_init_print(dbenv, &ptab, &ptabsize);
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
	DB_LSN last_lsn, lsn;
	DB_REP *db_rep;
	DBT dbt;
	LOG *lp;
	REP *rep;
	int change, ret, t_ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	MUTEX_LOCK(dbenv, db_rep->mutexp);
	ELECTION_DONE(rep);
	change = rep->gen != cntrl->gen || rep->master_id != eid;
	if (change) {
		rep->gen = cntrl->gen;
		rep->master_id = eid;
		F_SET(rep, REP_F_RECOVER);
		rep->stat.st_master_changes++;
	}
	MUTEX_UNLOCK(dbenv, db_rep->mutexp);

	if (!change)
		return (0);

	/*
	 * If the master changed, we need to start the process of
	 * figuring out what our last valid log record is.  However,
	 * if both the master and we agree that the max LSN is 0,0,
	 * then there is no recovery to be done.  If we are at 0 and
	 * the master is not, then we just need to request all the log
	 * records from the master.
	 */
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	R_LOCK(dbenv, &dblp->reginfo);
	last_lsn = lsn = lp->lsn;
	if (last_lsn.offset > sizeof(LOGP))
		last_lsn.offset -= lp->len;
	R_UNLOCK(dbenv, &dblp->reginfo);
	if (IS_INIT_LSN(lsn) || IS_ZERO_LSN(lsn)) {
empty:		MUTEX_LOCK(dbenv, db_rep->mutexp);
		F_CLR(rep, REP_F_RECOVER);
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);

		if (IS_INIT_LSN(cntrl->lsn))
			ret = 0;
		else
			ret = __rep_send_message(dbenv, rep->master_id,
			    REP_ALL_REQ, &lsn, NULL, 0);

		if (ret == 0)
			ret = DB_REP_NEWMASTER;
		return (ret);
	} else if (last_lsn.offset <= sizeof(LOGP)) {
		/*
		 * We have just changed log files and need to set lastlsn
		 * to the last record in the previous log files.
		 */
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			return (ret);
		memset(&dbt, 0, sizeof(dbt));
		ret = logc->get(logc, &last_lsn, &dbt, DB_LAST);
		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		if (ret == DB_NOTFOUND)
			goto empty;
		if (ret != 0)
			return (ret);
	}

	R_LOCK(dbenv, &dblp->reginfo);
	lp->verify_lsn = last_lsn;
	R_UNLOCK(dbenv, &dblp->reginfo);
	if ((ret = __rep_send_message(dbenv,
	    eid, REP_VERIFY_REQ, &last_lsn, NULL, 0)) != 0)
		return (ret);

	return (DB_REP_NEWMASTER);
}

/*
 * __rep_lockpgno_init
 *	Create a dispatch table for acquiring locks on each log record.
 *
 * PUBLIC: int __rep_lockpgno_init __P((DB_ENV *,
 * PUBLIC:     int (***)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *),
 * PUBLIC:     size_t *));
 */
int
__rep_lockpgno_init(dbenv, dtabp, dtabsizep)
	DB_ENV *dbenv;
	int (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t *dtabsizep;
{
	int ret;

	/* Initialize dispatch table. */
	*dtabsizep = 0;
	*dtabp = NULL;
	if ((ret = __bam_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0 ||
	    (ret = __crdel_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0 ||
	    (ret = __db_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0 ||
	    (ret = __dbreg_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0 ||
	    (ret = __fop_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0 ||
	    (ret = __qam_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0 ||
	    (ret = __ham_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0 ||
	    (ret = __txn_init_getpgnos(dbenv, dtabp, dtabsizep)) != 0)
		return (ret);

	return (0);
}

/*
 * __rep_unlockpages --
 *	Unlock the pages locked in __rep_lockpages.
 *
 * PUBLIC: int __rep_unlockpages __P((DB_ENV *, u_int32_t));
 */
int
__rep_unlockpages(dbenv, lid)
	DB_ENV *dbenv;
	u_int32_t lid;
{
	DB_LOCKREQ req, *lvp;

	req.op = DB_LOCK_PUT_ALL;
	return (dbenv->lock_vec(dbenv, lid, 0, &req, 1, &lvp));
}

/*
 * __rep_lockpages --
 *	Called to gather and lock pages in preparation for both
 *	single transaction apply as well as client synchronization
 *	with a new master.  A non-NULL key_lsn means that we're locking
 *	in order to apply a single log record during client recovery
 *	to the joint LSN.  A non-NULL max_lsn means that we are applying
 *	a transaction whose commit is at max_lsn.
 *
 * PUBLIC: int __rep_lockpages __P((DB_ENV *,
 * PUBLIC:     int (**)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *),
 * PUBLIC:     size_t, DB_LSN *, DB_LSN *, TXN_RECS *, u_int32_t));
 */
int
__rep_lockpages(dbenv, dtab, dtabsize, key_lsn, max_lsn, recs, lid)
	DB_ENV *dbenv;
	int (**dtab)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t dtabsize;
	DB_LSN *key_lsn, *max_lsn;
	TXN_RECS *recs;
	u_int32_t lid;
{
	DBT data_dbt, lo;
	DB_LOCK l;
	DB_LOCKREQ *lvp;
	DB_LOGC *logc;
	DB_LSN tmp_lsn;
	TXN_RECS tmp, *t;
	db_pgno_t cur_pgno;
	linfo_t locks;
	int i, ret, t_ret, unique;
	u_int32_t cur_fid;

	/*
	 * There are two phases:  First, we have to traverse backwards through
	 * the log records gathering the list of all the pages accessed.  Once
	 * we have this information we can acquire all the locks we need.
	 */

	/* Initialization */
	memset(&locks, 0, sizeof(locks));
	ret = 0;

	t = recs != NULL ? recs : &tmp;
	t->npages = t->nalloc = 0;
	t->array = NULL;

	/*
	 * We've got to be in one mode or the other; else life will either
	 * be excessively boring or overly exciting.
	 */
	DB_ASSERT(key_lsn != NULL || max_lsn != NULL);
	DB_ASSERT(key_lsn == NULL || max_lsn == NULL);

	/*
	 * Phase 1:  Fill in the pgno array.
	 */
	memset(&data_dbt, 0, sizeof(data_dbt));
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		F_SET(&data_dbt, DB_DBT_REALLOC);

	/* Single transaction apply. */
	if (max_lsn != NULL) {
		DB_ASSERT(0); /* XXX */
		/*
		tmp_lsn = *max_lsn;
		if ((ret = __rep_apply_thread(dbenv, dtab, dtabsize,
		    &data_dbt, &tmp_lsn, t)) != 0)
			goto err;
			*/
	}

	/* In recovery. */
	if (key_lsn != NULL) {
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			goto err;
		ret = logc->get(logc, key_lsn, &data_dbt, DB_SET);

		/* Save lsn values, since dispatch functions can change them. */
		tmp_lsn = *key_lsn;
		ret = __db_dispatch(dbenv,
		    dtab, dtabsize, &data_dbt, &tmp_lsn, DB_TXN_GETPGNOS, t);

		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;

		/*
		 * If ret == DB_DELETED, this record refers to a temporary
		 * file and there's nothing to apply.
		 */
		if (ret == DB_DELETED) {
			ret = 0;
			goto out;
		} else if (ret != 0)
			goto err;
	}

	if (t->npages == 0)
		goto out;

	/* Phase 2: Write lock all the pages. */

	/* Sort the entries in the array by page number. */
	qsort(t->array, t->npages, sizeof(LSN_PAGE), __rep_cmp_bypage);

	/* Count the number of unique pages. */
	cur_fid = DB_LOGFILEID_INVALID;
	cur_pgno = PGNO_INVALID;
	unique = 0;
	for (i = 0; i < t->npages; i++) {
		if (F_ISSET(&t->array[i], LSN_PAGE_NOLOCK))
			continue;
		if (t->array[i].pgdesc.pgno != cur_pgno ||
		    t->array[i].fid != cur_fid) {
			cur_pgno = t->array[i].pgdesc.pgno;
			cur_fid = t->array[i].fid;
			unique++;
		}
	}

	if (unique == 0)
		goto out;

	/* Handle single lock case specially, else allocate space for locks. */
	if (unique == 1) {
		memset(&lo, 0, sizeof(lo));
		lo.data = &t->array[0].pgdesc;
		lo.size = sizeof(t->array[0].pgdesc);
		ret = dbenv->lock_get(dbenv, lid, 0, &lo, DB_LOCK_WRITE, &l);
		goto out2;
	}

	/* Multi-lock case. */
	locks.n = unique;
	if ((ret = __os_calloc(dbenv,
	    unique, sizeof(DB_LOCKREQ), &locks.reqs)) != 0)
		goto err;
	if ((ret = __os_calloc(dbenv, unique, sizeof(DBT), &locks.objs)) != 0)
		goto err;

	unique = 0;
	cur_fid = DB_LOGFILEID_INVALID;
	cur_pgno = PGNO_INVALID;
	for (i = 0; i < t->npages; i++) {
		if (F_ISSET(&t->array[i], LSN_PAGE_NOLOCK))
			continue;
		if (t->array[i].pgdesc.pgno != cur_pgno ||
		    t->array[i].fid != cur_fid) {
			cur_pgno = t->array[i].pgdesc.pgno;
			cur_fid = t->array[i].fid;
			locks.reqs[unique].op = DB_LOCK_GET;
			locks.reqs[unique].mode = DB_LOCK_WRITE;
			locks.reqs[unique].obj = &locks.objs[unique];
			locks.objs[unique].data = &t->array[i].pgdesc;
			locks.objs[unique].size = sizeof(t->array[i].pgdesc);
			unique++;
		}
	}

	/* Finally, get the locks. */
	if ((ret =
	    dbenv->lock_vec(dbenv, lid, 0, locks.reqs, unique, &lvp)) != 0) {
		/*
		 * If we were unsuccessful, unlock any locks we acquired before
		 * the error and return the original error value.
		 */
		(void)__rep_unlockpages(dbenv, lid);
	}

err:
out:	if (locks.objs != NULL)
		__os_free(dbenv, locks.objs);
	if (locks.reqs != NULL)
		__os_free(dbenv, locks.reqs);

	/*
	 * Before we return, sort by LSN so that we apply records in the
	 * right order.
	 */
	qsort(t->array, t->npages, sizeof(LSN_PAGE), __rep_cmp_bylsn);

out2:	if ((ret != 0 || recs == NULL) && t->nalloc != 0) {
		__os_free(dbenv, t->array);
		t->array = NULL;
		t->npages = t->nalloc = 0;
	}

	if (F_ISSET(&data_dbt, DB_DBT_REALLOC) && data_dbt.data != NULL)
		__os_ufree(dbenv, data_dbt.data);

	return (ret);
}

/*
 * __rep_cmp_bypage and __rep_cmp_bylsn --
 *	Sort functions for qsort.  "bypage" sorts first by page numbers and
 *	then by the LSN.  "bylsn" sorts first by the LSN, then by page numbers.
 */
static int
__rep_cmp_bypage(a, b)
	const void *a, *b;
{
	LSN_PAGE *ap, *bp;

	ap = (LSN_PAGE *)a;
	bp = (LSN_PAGE *)b;

	if (ap->fid < bp->fid)
		return (-1);

	if (ap->fid > bp->fid)
		return (1);

	if (ap->pgdesc.pgno < bp->pgdesc.pgno)
		return (-1);

	if (ap->pgdesc.pgno > bp->pgdesc.pgno)
		return (1);

	if (ap->lsn.file < bp->lsn.file)
		return (-1);

	if (ap->lsn.file > bp->lsn.file)
		return (1);

	if (ap->lsn.offset < bp->lsn.offset)
		return (-1);

	if (ap->lsn.offset > bp->lsn.offset)
		return (1);

	return (0);
}

static int
__rep_cmp_bylsn(a, b)
	const void *a, *b;
{
	LSN_PAGE *ap, *bp;

	ap = (LSN_PAGE *)a;
	bp = (LSN_PAGE *)b;

	if (ap->lsn.file < bp->lsn.file)
		return (-1);

	if (ap->lsn.file > bp->lsn.file)
		return (1);

	if (ap->lsn.offset < bp->lsn.offset)
		return (-1);

	if (ap->lsn.offset > bp->lsn.offset)
		return (1);

	if (ap->fid < bp->fid)
		return (-1);

	if (ap->fid > bp->fid)
		return (1);

	if (ap->pgdesc.pgno < bp->pgdesc.pgno)
		return (-1);

	if (ap->pgdesc.pgno > bp->pgdesc.pgno)
		return (1);

	return (0);
}

/*
 * __rep_is_client
 *	Used by other subsystems to figure out if this is a replication
 * client sites.
 *
 * PUBLIC: int __rep_is_client __P((DB_ENV *));
 */
int
__rep_is_client(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	REP *rep;
	int ret;

	if ((db_rep = dbenv->rep_handle) == NULL)
		return (0);
	rep = db_rep->region;

	MUTEX_LOCK(dbenv, db_rep->mutexp);
	ret = F_ISSET(rep, REP_F_UPGRADE | REP_F_LOGSONLY);
	MUTEX_UNLOCK(dbenv, db_rep->mutexp);
	return (ret);
}

/*
 * __rep_send_vote
 *	Send this site's vote for the election.
 *
 * PUBLIC: int __rep_send_vote __P((DB_ENV *, DB_LSN *, int, int, int));
 */
int
__rep_send_vote(dbenv, lsnp, nsites, pri, tiebreaker)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	int nsites, pri, tiebreaker;
{
	DBT vote_dbt;
	REP_VOTE_INFO vi;

	memset(&vi, 0, sizeof(vi));

	vi.priority = pri;
	vi.nsites = nsites;
	vi.tiebreaker = tiebreaker;

	memset(&vote_dbt, 0, sizeof(vote_dbt));
	vote_dbt.data = &vi;
	vote_dbt.size = sizeof(vi);

	return (__rep_send_message(dbenv,
	    DB_EID_BROADCAST, REP_VOTE1, lsnp, &vote_dbt, 0));
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
	MUTEX_LOCK(dbenv, &renv->mutex);
	if ((ret = __db_shalloc(infop->addr,
	    sizeof(nalloc * sizeof(int)), sizeof(int), &tally)) == 0) {
		if (rep->tally_off != INVALID_ROFF)
			 __db_shalloc_free(infop->addr,
			    R_ADDR(infop, rep->tally_off));
		rep->asites = nalloc;
		rep->nsites = nsites;
		rep->tally_off = R_OFFSET(infop, tally);
	}
	MUTEX_UNLOCK(dbenv, &renv->mutex);
	return (ret);
}

#ifdef NOTYET
static int __rep_send_file __P((DB_ENV *, DBT *, u_int32_t));
/*
 * __rep_send_file --
 *	Send an entire file, one block at a time.
 */
static int
__rep_send_file(dbenv, rec, eid)
	DB_ENV *dbenv;
	DBT *rec;
	u_int32_t eid;
{
	DB *dbp;
	DB_LOCK lk;
	DB_MPOOLFILE *mpf;
	DBC *dbc;
	DBT rec_dbt;
	PAGE *pagep;
	db_pgno_t last_pgno, pgno;
	int ret, t_ret;

	dbp = NULL;
	dbc = NULL;
	pagep = NULL;
	mpf = NULL;
	LOCK_INIT(lk);

	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		goto err;

	if ((ret = dbp->open(dbp, rec->data, NULL, DB_UNKNOWN, 0, 0)) != 0)
		goto err;

	if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
		goto err;
	/*
	 * Force last_pgno to some value that will let us read the meta-dat
	 * page in the following loop.
	 */
	memset(&rec_dbt, 0, sizeof(rec_dbt));
	last_pgno = 1;
	for (pgno = 0; pgno <= last_pgno; pgno++) {
		if ((ret = __db_lget(dbc, 0, pgno, DB_LOCK_READ, 0, &lk)) != 0)
			goto err;

		if ((ret = mpf->get(mpf, &pgno, 0, &pagep)) != 0)
			goto err;

		if (pgno == 0)
			last_pgno = ((DBMETA *)pagep)->last_pgno;

		rec_dbt.data = pagep;
		rec_dbt.size = dbp->pgsize;
		if ((ret = __rep_send_message(dbenv, eid,
		    REP_FILE, NULL, &rec_dbt, pgno == last_pgno)) != 0)
			goto err;
		ret = mpf->put(mpf, pagep, 0);
		pagep = NULL;
		if (ret != 0)
			goto err;
		ret = __LPUT(dbc, lk);
		LOCK_INIT(lk);
		if (ret != 0)
			goto err;
	}

err:	if (LOCK_ISSET(lk) && (t_ret = __LPUT(dbc, lk)) != 0 && ret == 0)
		ret = t_ret;
	if (dbc != NULL && (t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	if (pagep != NULL && (t_ret = mpf->put(mpf, pagep, 0)) != 0 && ret == 0)
		ret = t_ret;
	if (dbp != NULL && (t_ret = dbp->close(dbp, 0)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}
#endif

#if 0
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
	case REP_ELECT:
		type = "elect";
		break;
	case REP_FILE:
		type = "file";
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
	case REP_PAGE_REQ:
		type = "page_req";
		break;
	case REP_PLIST:
		type = "plist";
		break;
	case REP_PLIST_REQ:
		type = "plist_req";
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
	printf("%s %s: gen = %d eid %d, type %s, LSN [%u][%u]\n",
	    dbenv->db_home, str, rp->gen, eid, type, rp->lsn.file,
	    rp->lsn.offset);
}
#endif
