/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_meta.c,v 11.61 2002/08/08 03:57:48 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"
#include "dbinc/db_am.h"

static void __db_init_meta __P((void *, u_int32_t, db_pgno_t, u_int32_t));

/*
 * __db_init_meta --
 *	Helper function for __db_new that initializes the important fields in
 * a meta-data page (used instead of P_INIT).  We need to make sure that we
 * retain the page number and LSN of the existing page.
 */
static void
__db_init_meta(p, pgsize, pgno, pgtype)
	void *p;
	u_int32_t pgsize;
	db_pgno_t pgno;
	u_int32_t pgtype;
{
	DB_LSN save_lsn;
	DBMETA *meta;

	meta = (DBMETA *)p;
	save_lsn = meta->lsn;
	memset(meta, 0, sizeof(DBMETA));
	meta->lsn = save_lsn;
	meta->pagesize = pgsize;
	meta->pgno = pgno;
	meta->type = (u_int8_t)pgtype;
}

/*
 * __db_new --
 *	Get a new page, preferably from the freelist.
 *
 * PUBLIC: int __db_new __P((DBC *, u_int32_t, PAGE **));
 */
int
__db_new(dbc, type, pagepp)
	DBC *dbc;
	u_int32_t type;
	PAGE **pagepp;
{
	DBMETA *meta;
	DB *dbp;
	DB_LOCK metalock;
	DB_LSN lsn;
	DB_MPOOLFILE *mpf;
	PAGE *h;
	db_pgno_t pgno, newnext;
	int meta_flags, extend, ret;

	meta = NULL;
	meta_flags = 0;
	dbp = dbc->dbp;
	mpf = dbp->mpf;
	h = NULL;
	newnext = PGNO_INVALID;

	pgno = PGNO_BASE_MD;
	if ((ret = __db_lget(dbc,
	    LCK_ALWAYS, pgno, DB_LOCK_WRITE, 0, &metalock)) != 0)
		goto err;
	if ((ret = mpf->get(mpf, &pgno, 0, (PAGE **)&meta)) != 0)
		goto err;
	if (meta->free == PGNO_INVALID) {
		pgno = meta->last_pgno + 1;
		ZERO_LSN(lsn);
		extend = 1;
	} else {
		pgno = meta->free;
		if ((ret = mpf->get(mpf, &pgno, 0, &h)) != 0)
			goto err;

		/*
		 * We want to take the first page off the free list and
		 * then set meta->free to the that page's next_pgno, but
		 * we need to log the change first.
		 */
		newnext = h->next_pgno;
		lsn = h->lsn;
		extend = 0;
	}

	/*
	 * Log the allocation before fetching the new page.  If we
	 * don't have room in the log then we don't want to tell
	 * mpool to extend the file.
	 */
	if (DBC_LOGGING(dbc)) {
		if ((ret = __db_pg_alloc_log(dbp, dbc->txn, &LSN(meta), 0,
		    &LSN(meta), PGNO_BASE_MD, &lsn, pgno,
		    (u_int32_t)type, newnext)) != 0)
			goto err;
	} else
		LSN_NOT_LOGGED(LSN(meta));

	meta_flags = DB_MPOOL_DIRTY;
	meta->free = newnext;

	if (extend == 1) {
		meta->last_pgno++;
		if ((ret = mpf->get(mpf, &pgno, DB_MPOOL_NEW, &h)) != 0)
			goto err;
		ZERO_LSN(h->lsn);
		h->pgno = pgno;
		DB_ASSERT(pgno == meta->last_pgno);
	}
	LSN(h) = LSN(meta);

	DB_ASSERT(TYPE(h) == P_INVALID);

	if (TYPE(h) != P_INVALID)
		return (__db_panic(dbp->dbenv, EINVAL));

	(void)mpf->put(mpf, (PAGE *)meta, DB_MPOOL_DIRTY);
	(void)__TLPUT(dbc, metalock);

	switch (type) {
		case P_BTREEMETA:
		case P_HASHMETA:
		case P_QAMMETA:
			__db_init_meta(h, dbp->pgsize, h->pgno, type);
			break;
		default:
			P_INIT(h, dbp->pgsize,
			    h->pgno, PGNO_INVALID, PGNO_INVALID, 0, type);
			break;
	}

	/*
	 * If dirty reads are enabled and we are in a transaction, we could
	 * abort this allocation after the page(s) pointing to this
	 * one have their locks downgraded.  This would permit dirty readers
	 * to access this page which is ok, but they must be off the
	 * page when we abort.  This will also prevent updates happening
	 * to this page until we commit.
	 */
	if (F_ISSET(dbc->dbp, DB_AM_DIRTY) && dbc->txn != NULL) {
		if ((ret = __db_lget(dbc, 0,
		    h->pgno, DB_LOCK_WWRITE, 0, &metalock)) != 0)
			goto err;
	}
	*pagepp = h;
	return (0);

err:	if (h != NULL)
		(void)mpf->put(mpf, h, 0);
	if (meta != NULL)
		(void)mpf->put(mpf, meta, meta_flags);
	(void)__TLPUT(dbc, metalock);
	return (ret);
}

/*
 * __db_free --
 *	Add a page to the head of the freelist.
 *
 * PUBLIC: int __db_free __P((DBC *, PAGE *));
 */
int
__db_free(dbc, h)
	DBC *dbc;
	PAGE *h;
{
	DBMETA *meta;
	DB *dbp;
	DBT ldbt;
	DB_LOCK metalock;
	DB_MPOOLFILE *mpf;
	db_pgno_t pgno;
	u_int32_t dirty_flag;
	int ret, t_ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;

	/*
	 * Retrieve the metadata page and insert the page at the head of
	 * the free list.  If either the lock get or page get routines
	 * fail, then we need to put the page with which we were called
	 * back because our caller assumes we take care of it.
	 */
	dirty_flag = 0;
	pgno = PGNO_BASE_MD;
	if ((ret = __db_lget(dbc,
	    LCK_ALWAYS, pgno, DB_LOCK_WRITE, 0, &metalock)) != 0)
		goto err;
	if ((ret = mpf->get(mpf, &pgno, 0, (PAGE **)&meta)) != 0) {
		(void)__TLPUT(dbc, metalock);
		goto err;
	}

	DB_ASSERT(h->pgno != meta->free);
	/* Log the change. */
	if (DBC_LOGGING(dbc)) {
		memset(&ldbt, 0, sizeof(ldbt));
		ldbt.data = h;
		ldbt.size = P_OVERHEAD(dbp);
		if ((ret = __db_pg_free_log(dbp,
		    dbc->txn, &LSN(meta), 0, h->pgno,
		    &LSN(meta), PGNO_BASE_MD, &ldbt, meta->free)) != 0) {
			(void)mpf->put(mpf, (PAGE *)meta, 0);
			(void)__TLPUT(dbc, metalock);
			goto err;
		}
	} else
		LSN_NOT_LOGGED(LSN(meta));
	LSN(h) = LSN(meta);

	P_INIT(h, dbp->pgsize, h->pgno, PGNO_INVALID, meta->free, 0, P_INVALID);

	meta->free = h->pgno;

	/* Discard the metadata page. */
	if ((t_ret =
	    mpf->put(mpf, (PAGE *)meta, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = __TLPUT(dbc, metalock)) != 0 && ret == 0)
		ret = t_ret;

	/* Discard the caller's page reference. */
	dirty_flag = DB_MPOOL_DIRTY;
err:	if ((t_ret = mpf->put(mpf, h, dirty_flag)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * XXX
	 * We have to unlock the caller's page in the caller!
	 */
	return (ret);
}

#ifdef DEBUG
/*
 * __db_lprint --
 *	Print out the list of locks currently held by a cursor.
 *
 * PUBLIC: int __db_lprint __P((DBC *));
 */
int
__db_lprint(dbc)
	DBC *dbc;
{
	DB *dbp;
	DB_LOCKREQ req;

	dbp = dbc->dbp;

	if (LOCKING_ON(dbp->dbenv)) {
		req.op = DB_LOCK_DUMP;
		dbp->dbenv->lock_vec(dbp->dbenv, dbc->locker, 0, &req, 1, NULL);
	}
	return (0);
}
#endif

/*
 * Implement the rules for transactional locking.  We can release the previous
 * lock if we are not in a transaction or COUPLE_ALWAYS is specifed (used in
 * record locking).  If we are doing dirty reads then we can release read locks
 * and down grade write locks.
 */
#define	DB_PUT_ACTION(dbc, action, lockp)				\
	    (((action == LCK_COUPLE || action == LCK_COUPLE_ALWAYS) &&	\
	    LOCK_ISSET(*lockp)) ?					\
	    (dbc->txn == NULL || action == LCK_COUPLE_ALWAYS ||		\
	    (F_ISSET(dbc, DBC_DIRTY_READ) &&				\
	    (lockp)->mode == DB_LOCK_DIRTY)) ? LCK_COUPLE :		\
	    (F_ISSET((dbc)->dbp, DB_AM_DIRTY) &&			\
	    (lockp)->mode == DB_LOCK_WRITE) ? LCK_DOWNGRADE : 0 : 0)

/*
 * __db_lget --
 *	The standard lock get call.
 *
 * PUBLIC: int __db_lget __P((DBC *,
 * PUBLIC:     int, db_pgno_t, db_lockmode_t, u_int32_t, DB_LOCK *));
 */
int
__db_lget(dbc, action, pgno, mode, lkflags, lockp)
	DBC *dbc;
	int action;
	db_pgno_t pgno;
	db_lockmode_t mode;
	u_int32_t lkflags;
	DB_LOCK *lockp;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_LOCKREQ couple[2], *reqp;
	DB_TXN *txn;
	int has_timeout, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	txn = dbc->txn;

	/*
	 * We do not always check if we're configured for locking before
	 * calling __db_lget to acquire the lock.
	 */
	if (CDB_LOCKING(dbenv) ||
	    !LOCKING_ON(dbenv) || F_ISSET(dbc, DBC_COMPENSATE) ||
	    (F_ISSET(dbc, DBC_RECOVER) &&
	    (action != LCK_ROLLBACK || F_ISSET(dbenv, DB_ENV_REP_CLIENT))) ||
	    (action != LCK_ALWAYS && F_ISSET(dbc, DBC_OPD))) {
		LOCK_INIT(*lockp);
		return (0);
	}

	dbc->lock.pgno = pgno;
	if (lkflags & DB_LOCK_RECORD)
		dbc->lock.type = DB_RECORD_LOCK;
	else
		dbc->lock.type = DB_PAGE_LOCK;
	lkflags &= ~DB_LOCK_RECORD;

	/*
	 * If the transaction enclosing this cursor has DB_LOCK_NOWAIT set,
	 * pass that along to the lock call.
	 */
	if (DB_NONBLOCK(dbc))
		lkflags |= DB_LOCK_NOWAIT;

	if (F_ISSET(dbc, DBC_DIRTY_READ) && mode == DB_LOCK_READ)
		mode = DB_LOCK_DIRTY;

	has_timeout = txn != NULL && F_ISSET(txn, TXN_LOCKTIMEOUT);

	switch (DB_PUT_ACTION(dbc, action, lockp)) {
	case LCK_COUPLE:
lck_couple:	couple[0].op = has_timeout? DB_LOCK_GET_TIMEOUT : DB_LOCK_GET;
		couple[0].obj = &dbc->lock_dbt;
		couple[0].mode = mode;
		if (action == LCK_COUPLE_ALWAYS)
			action = LCK_COUPLE;
		UMRW_SET(couple[0].timeout);
		if (has_timeout)
			couple[0].timeout = txn->lock_timeout;
		if (action == LCK_COUPLE) {
			couple[1].op = DB_LOCK_PUT;
			couple[1].lock = *lockp;
		}

		ret = dbenv->lock_vec(dbenv, dbc->locker,
		    lkflags, couple, action == LCK_COUPLE ? 2 : 1, &reqp);
		if (ret == 0 || reqp == &couple[1])
			*lockp = couple[0].lock;
		break;
	case LCK_DOWNGRADE:
		if ((ret = dbenv->lock_downgrade(
		    dbenv, lockp, DB_LOCK_WWRITE, 0)) != 0)
			return (ret);
		/* FALL THROUGH */
	default:
		if (has_timeout)
			goto lck_couple;
		ret = dbenv->lock_get(dbenv,
		    dbc->locker, lkflags, &dbc->lock_dbt, mode, lockp);
		break;
	}

	return (ret);
}

/*
 * __db_lput --
 *	The standard lock put call.
 *
 * PUBLIC: int __db_lput __P((DBC *, DB_LOCK *));
 */
int
__db_lput(dbc, lockp)
	DBC *dbc;
	DB_LOCK *lockp;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbc->dbp->dbenv;

	switch (DB_PUT_ACTION(dbc, LCK_COUPLE, lockp)) {
	case LCK_COUPLE:
		ret = dbenv->lock_put(dbenv, lockp);
		break;
	case LCK_DOWNGRADE:
		ret = __lock_downgrade(dbenv, lockp, DB_LOCK_WWRITE, 0);
		break;
	default:
		ret = 0;
		break;
	}

	return (ret);
}
