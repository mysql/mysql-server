/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam.c,v 11.72 2001/01/16 20:10:55 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "db_am.h"
#include "mp.h"
#include "lock.h"
#include "log.h"
#include "btree.h"
#include "qam.h"

static int __qam_c_close __P((DBC *, db_pgno_t, int *));
static int __qam_c_del __P((DBC *));
static int __qam_c_destroy __P((DBC *));
static int __qam_c_get __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
static int __qam_c_put __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
static int __qam_getno __P((DB *, const DBT *, db_recno_t *));

/*
 * __qam_position --
 *	Position a queued access method cursor at a record.  This returns
 *	the page locked.  *exactp will be set if the record is valid.
 * PUBLIC: int __qam_position
 * PUBLIC:       __P((DBC *, db_recno_t *, qam_position_mode, int *));
 */
int
__qam_position(dbc, recnop, mode, exactp)
	DBC *dbc;		/* open cursor */
	db_recno_t *recnop;	/* pointer to recno to find */
	qam_position_mode mode;/* locking: read or write */
	int *exactp;		/* indicate if it was found */
{
	QUEUE_CURSOR *cp;
	DB *dbp;
	QAMDATA  *qp;
	db_pgno_t pg;
	int ret;

	dbp = dbc->dbp;
	cp = (QUEUE_CURSOR *)dbc->internal;

	/* Fetch the page for this recno. */
	pg = QAM_RECNO_PAGE(dbp, *recnop);

	if ((ret = __db_lget(dbc, 0, pg, mode == QAM_READ ?
	     DB_LOCK_READ : DB_LOCK_WRITE, 0, &cp->lock)) != 0)
		return (ret);
	cp->page = NULL;
	*exactp = 0;
	if ((ret = __qam_fget(dbp, &pg,
	    mode == QAM_WRITE ? DB_MPOOL_CREATE : 0,
	    &cp->page)) != 0) {
		/* We did not fetch it, we can release the lock. */
		(void)__LPUT(dbc, cp->lock);
		cp->lock.off = LOCK_INVALID;
		if (mode != QAM_WRITE && (ret == EINVAL || ret == ENOENT))
			return (0);
		return (ret);
	}
	cp->pgno = pg;
	cp->indx = QAM_RECNO_INDEX(dbp, pg, *recnop);

	if (PGNO(cp->page) == 0) {
		if (F_ISSET(dbp, DB_AM_RDONLY)) {
			*exactp = 0;
			return (0);
		}
		PGNO(cp->page) = pg;
		TYPE(cp->page) = P_QAMDATA;
	}

	qp = QAM_GET_RECORD(dbp, cp->page, cp->indx);
	*exactp = F_ISSET(qp, QAM_VALID);

	return (ret);
}

/*
 * __qam_pitem --
 *	Put an item on a queue page.  Copy the data to the page and set the
 *	VALID and SET bits.  If logging and the record was previously set,
 *	log that data, otherwise just log the new data.
 *
 *   pagep must be write locked
 *
 * PUBLIC: int __qam_pitem
 * PUBLIC:     __P((DBC *,  QPAGE *, u_int32_t, db_recno_t, DBT *));
 */
int
__qam_pitem(dbc, pagep, indx, recno, data)
	DBC *dbc;
	QPAGE *pagep;
	u_int32_t indx;
	db_recno_t recno;
	DBT *data;
{
	DB *dbp;
	DBT olddata, pdata, *datap;
	QAMDATA *qp;
	QUEUE *t;
	u_int32_t size;
	u_int8_t *dest, *p;
	int alloced, ret;

	alloced = ret = 0;

	dbp = dbc->dbp;
	t = (QUEUE *)dbp->q_internal;

	if (data->size > t->re_len)
		goto len_err;

	qp = QAM_GET_RECORD(dbp, pagep, indx);

	p = qp->data;
	size = data->size;
	datap = data;
	if (F_ISSET(data, DB_DBT_PARTIAL)) {
		if (data->doff + data->dlen > t->re_len) {
			alloced = data->dlen;
			goto len_err;
		}
		if (data->size != data->dlen) {
len_err:		__db_err(dbp->dbenv,
			    "Length improper for fixed length record %lu",
			    (u_long)(alloced ? alloced : data->size));
			return (EINVAL);
		}
		if (data->size == t->re_len)
			goto no_partial;

		/*
		 * If we are logging, then we have to build the record
		 * first, otherwise, we can simply drop the change
		 * directly on the page.  After this clause, make
		 * sure that datap and p are set up correctly so that
		 * copying datap into p does the right thing.
		 *
		 * Note, I am changing this so that if the existing
		 * record is not valid, we create a complete record
		 * to log so that both this and the recovery code is simpler.
		 */

		if (DB_LOGGING(dbc) || !F_ISSET(qp, QAM_VALID)) {
			datap = &pdata;
			memset(datap, 0, sizeof(*datap));

			if ((ret = __os_malloc(dbp->dbenv,
			    t->re_len, NULL, &datap->data)) != 0)
				return (ret);
			alloced = 1;
			datap->size = t->re_len;

			/*
			 * Construct the record if it's valid, otherwise set it
			 * all to the pad character.
			 */
			dest = datap->data;
			if (F_ISSET(qp, QAM_VALID))
				memcpy(dest, p, t->re_len);
			else
				memset(dest, t->re_pad, t->re_len);

			dest += data->doff;
			memcpy(dest, data->data, data->size);
		} else {
			datap = data;
			p += data->doff;
		}
	}

no_partial:
	if (DB_LOGGING(dbc)) {
		olddata.size = 0;
		if (F_ISSET(qp, QAM_SET)) {
			olddata.data = qp->data;
			olddata.size = t->re_len;
		}
		if ((ret = __qam_add_log(dbp->dbenv, dbc->txn, &LSN(pagep),
		    0, dbp->log_fileid, &LSN(pagep), pagep->pgno,
		    indx, recno, datap, qp->flags,
		    olddata.size == 0 ? NULL : &olddata)) != 0)
			goto err;
	}

	F_SET(qp, QAM_VALID | QAM_SET);
	memcpy(p, datap->data, datap->size);
	if (!F_ISSET(data, DB_DBT_PARTIAL))
		memset(p + datap->size,  t->re_pad, t->re_len - datap->size);

err:	if (alloced)
		__os_free(datap->data, t->re_len);

	return (ret);
}
/*
 * __qam_c_put
 *	Cursor put for queued access method.
 *	BEFORE and AFTER cannot be specified.
 */
static int
__qam_c_put(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	QUEUE_CURSOR *cp;
	DB *dbp;
	DB_LOCK lock;
	QMETA *meta;
	db_pgno_t pg;
	db_recno_t new_cur, new_first;
	u_int32_t opcode;
	int exact, ret, t_ret;

	COMPQUIET(key, NULL);

	dbp = dbc->dbp;
	if (pgnop != NULL)
		*pgnop = PGNO_INVALID;

	cp = (QUEUE_CURSOR *)dbc->internal;

	/* Write lock the record. */
	if ((ret = __db_lget(dbc,
	    0, cp->recno, DB_LOCK_WRITE, DB_LOCK_RECORD, &lock)) != 0)
		return (ret);

	if ((ret = __qam_position(dbc,
	    &cp->recno, QAM_WRITE, &exact)) != 0) {
		/* We could not get the page, we can release the record lock. */
		__LPUT(dbc, lock);
		return (ret);
	}

	if (exact && flags == DB_NOOVERWRITE) {
		ret = __TLPUT(dbc, lock);
		/* Doing record locking, release the page lock */
		if ((t_ret = __LPUT(dbc, cp->lock)) == 0)
			cp->lock.off = LOCK_INVALID;
		else
			if (ret == 0)
				ret = t_ret;
		if ((t_ret =
		     __qam_fput(dbp, cp->pgno, cp->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		cp->page = NULL;
		return (ret == 0 ? DB_KEYEXIST : ret);
	}

	/* Put the item on the page. */
	ret = __qam_pitem(dbc, (QPAGE *)cp->page, cp->indx, cp->recno, data);

	/* Doing record locking, release the page lock */
	if ((t_ret = __LPUT(dbc, cp->lock)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret =
	    __qam_fput(dbp, cp->pgno, cp->page, DB_MPOOL_DIRTY)) && ret == 0)
		ret = t_ret;
	cp->page = NULL;
	cp->lock = lock;
	cp->lock_mode = DB_LOCK_WRITE;
	if (ret != 0)
		return (ret);

	/* We may need to reset the head or tail of the queue. */
	pg = ((QUEUE *)dbp->q_internal)->q_meta;
	if ((ret = __db_lget(dbc, 0, pg,  DB_LOCK_WRITE, 0, &lock)) != 0)
		return (ret);
	if ((ret = memp_fget(dbp->mpf, &pg, 0, &meta)) != 0) {
		/* We did not fetch it, we can release the lock. */
		(void)__LPUT(dbc, lock);
		return (ret);
	}

	opcode = 0;
	new_cur = new_first = 0;

	/*
	 * If the put address is outside the queue, adjust the head and
	 * tail of the queue.  If the order is inverted we move
	 * the one which is closer.  The first case is when the
	 * queue is empty, move first and current to where the new
	 * insert is.
	 */

	if (meta->first_recno == meta->cur_recno) {
		new_first = cp->recno;
		new_cur = cp->recno + 1;
		if (new_cur == RECNO_OOB)
			new_cur++;
		opcode |= QAM_SETFIRST;
		opcode |= QAM_SETCUR;
	} else {
		if (QAM_BEFORE_FIRST(meta, cp->recno) &&
		    (meta->first_recno <= meta->cur_recno ||
		    meta->first_recno - cp->recno < cp->recno - meta->cur_recno)) {
			new_first = cp->recno;
			opcode |= QAM_SETFIRST;
		}

		if (meta->cur_recno == cp->recno ||
		    (QAM_AFTER_CURRENT(meta, cp->recno) &&
		    (meta->first_recno <= meta->cur_recno ||
		    cp->recno - meta->cur_recno <= meta->first_recno - cp->recno))) {
			new_cur = cp->recno + 1;
			if (new_cur == RECNO_OOB)
				new_cur++;
			opcode |= QAM_SETCUR;
		}
	}

	if (opcode != 0 && DB_LOGGING(dbc)) {
		ret = __qam_mvptr_log(dbp->dbenv, dbc->txn, &meta->dbmeta.lsn,
		    0, opcode, dbp->log_fileid, meta->first_recno, new_first,
		    meta->cur_recno, new_cur, &meta->dbmeta.lsn);
	}

	if (opcode & QAM_SETCUR)
		meta->cur_recno = new_cur;
	if (opcode & QAM_SETFIRST)
		meta->first_recno = new_first;

	if ((t_ret =
	    memp_fput(dbp->mpf, meta, opcode != 0 ? DB_MPOOL_DIRTY : 0)) != 0 &&
	    ret == 0)
		ret = t_ret;

	/* Don't hold the meta page long term. */
	if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __qam_put --
 *	Add a record to the queue.
 *	If we are doing anything but appending, just call qam_c_put to do the
 *	work.  Otherwise we fast path things here.
 *
 * PUBLIC: int __qam_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__qam_put(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	QUEUE_CURSOR *cp;
	DBC *dbc;
	DB_LOCK lock;
	QMETA *meta;
	QPAGE *page;
	QUEUE *qp;
	db_pgno_t pg;
	db_recno_t recno;
	int ret, t_ret;

	PANIC_CHECK(dbp->dbenv);
	DB_CHECK_TXN(dbp, txn);

	/* Allocate a cursor. */
	if ((ret = dbp->cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, dbc->txn, "qam_put", key, data, flags);

	cp = (QUEUE_CURSOR *)dbc->internal;

	/* Check for invalid flags. */
	if ((ret = __db_putchk(dbp,
	    key, data, flags, F_ISSET(dbp, DB_AM_RDONLY), 0)) != 0)
		goto done;

	/* If not appending, then just call the cursor routine */
	if (flags != DB_APPEND) {
		if ((ret = __qam_getno(dbp, key, &cp->recno)) != 0)
			goto done;

		ret = __qam_c_put(dbc, NULL, data, flags, NULL);
		goto done;
	}

	/* Write lock the meta page. */
	pg = ((QUEUE *)dbp->q_internal)->q_meta;
	if ((ret = __db_lget(dbc, 0, pg,  DB_LOCK_WRITE, 0, &lock)) != 0)
		goto done;
	if ((ret = memp_fget(dbp->mpf, &pg, 0, &meta)) != 0) {
		/* We did not fetch it, we can release the lock. */
		(void)__LPUT(dbc, lock);
		goto done;
	}

	/* Record that we are going to allocate a record. */
	if (DB_LOGGING(dbc)) {
		__qam_inc_log(dbp->dbenv,
		    dbc->txn, &meta->dbmeta.lsn,
		    0, dbp->log_fileid, &meta->dbmeta.lsn);
	}

	/* Get the next record number. */
	recno = meta->cur_recno;
	meta->cur_recno++;
	if (meta->cur_recno == RECNO_OOB)
		meta->cur_recno++;
	if (meta->cur_recno == meta->first_recno) {
		meta->cur_recno--;
		if (meta->cur_recno == RECNO_OOB)
			meta->cur_recno--;
		(void)__LPUT(dbc, lock);
		ret = EFBIG;
		goto err;
	}

	if (QAM_BEFORE_FIRST(meta, recno))
		meta->first_recno = recno;

	/* Lock the record and release meta page lock. */
	if ((ret = __db_lget(dbc,
	    1, recno, DB_LOCK_WRITE, DB_LOCK_RECORD, &lock)) != 0)
		goto err;

	/*
	 * The application may modify the data based on the selected record
	 * number.
	 */
	if (flags == DB_APPEND && dbc->dbp->db_append_recno != NULL &&
	    (ret = dbc->dbp->db_append_recno(dbc->dbp, data, recno)) != 0) {
		(void)__LPUT(dbc, lock);
		goto err;
	}

	cp->lock = lock;
	cp->lock_mode = DB_LOCK_WRITE;

	pg = QAM_RECNO_PAGE(dbp, recno);

	/* Fetch and write lock the data page. */
	if ((ret = __db_lget(dbc, 0, pg,  DB_LOCK_WRITE, 0, &lock)) != 0)
		goto err;
	if ((ret = __qam_fget(dbp, &pg, DB_MPOOL_CREATE, &page)) != 0) {
		/* We did not fetch it, we can release the lock. */
		(void)__LPUT(dbc, lock);
		goto err;
	}

	/* See if this is a new page. */
	if (page->pgno == 0) {
		page->pgno = pg;
		page->type = P_QAMDATA;
	}

	/* Put the item on the page and log it. */
	ret = __qam_pitem(dbc, page,
	    QAM_RECNO_INDEX(dbp, pg, recno), recno, data);

	/* Doing record locking, release the page lock */
	if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret
	    = __qam_fput(dbp, pg, page, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;

	/* Return the record number to the user. */
	if (ret == 0)
		ret = __db_retcopy(dbp, key,
		    &recno, sizeof(recno), &dbc->rkey.data, &dbc->rkey.ulen);

	/* See if we are leaving the extent. */
	qp = (QUEUE *) dbp->q_internal;
	if (qp->page_ext != 0
	    && (recno % (qp->page_ext * qp->rec_page) == 0
	    || recno == UINT32_T_MAX)) {
		if ((ret =
		    __db_lget(dbc, 0, pg,  DB_LOCK_WRITE, 0, &lock)) != 0)
			goto err;
		if (!QAM_AFTER_CURRENT(meta, recno))
			ret = __qam_fclose(dbp, pg);
		(void)__LPUT(dbc, lock);
	}

err:
	/* Release the meta page. */
	if ((t_ret
	     = memp_fput(dbp->mpf, meta, DB_MPOOL_DIRTY)) != 0 && ret == 0)
		ret = t_ret;

done:
	/* Discard the cursor. */
	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __qam_c_del --
 *	Qam cursor->am_del function
 */
static int
__qam_c_del(dbc)
	DBC *dbc;
{
	QUEUE_CURSOR *cp;
	DB *dbp;
	DBT data;
	DB_LOCK lock;
	PAGE *pagep;
	QAMDATA *qp;
	QMETA *meta;
	db_pgno_t pg;
	int exact, ret, t_ret;

	dbp = dbc->dbp;
	cp = (QUEUE_CURSOR *)dbc->internal;

	pg = ((QUEUE *)dbp->q_internal)->q_meta;
	if ((ret = __db_lget(dbc, 0, pg,  DB_LOCK_READ, 0, &lock)) != 0)
		return (ret);
	if ((ret = memp_fget(dbp->mpf, &pg, 0, &meta)) != 0) {
		/* We did not fetch it, we can release the lock. */
		(void)__LPUT(dbc, lock);
		return (ret);
	}

	if (QAM_NOT_VALID(meta, cp->recno))
		ret = DB_NOTFOUND;

	/* Don't hold the meta page long term. */
	if ((t_ret = __LPUT(dbc, lock)) != 0 && ret == 0)
		ret = t_ret;
	if ((t_ret = memp_fput(dbp->mpf, meta, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (ret != 0)
		return (ret);

	if ((ret = __db_lget(dbc,
	    0, cp->recno, DB_LOCK_WRITE, DB_LOCK_RECORD, &lock)) != 0)
		return (ret);

	cp->lock_mode = DB_LOCK_WRITE;
	/* Find the record ; delete only deletes exact matches. */
	if ((ret = __qam_position(dbc,
	    &cp->recno, QAM_WRITE, &exact)) != 0) {
		cp->lock = lock;
		return (ret);
	}
	if (!exact) {
		ret = DB_NOTFOUND;
		goto err1;
	}

	pagep = cp->page;
	qp = QAM_GET_RECORD(dbp, pagep, cp->indx);

	if (DB_LOGGING(dbc)) {
		if (((QUEUE *)dbp->q_internal)->page_ext == 0
		    || ((QUEUE *)dbp->q_internal)->re_len == 0) {
			if ((ret =
			    __qam_del_log(dbp->dbenv,
			    dbc->txn, &LSN(pagep), 0,
			    dbp->log_fileid, &LSN(pagep),
			    pagep->pgno, cp->indx, cp->recno)) != 0)
				goto err1;
		} else {
			data.size = ((QUEUE *)dbp->q_internal)->re_len;
			data.data = qp->data;
			if ((ret =
			    __qam_delext_log(dbp->dbenv, dbc->txn,
			    &LSN(pagep), 0, dbp->log_fileid, &LSN(pagep),
			    pagep->pgno, cp->indx, cp->recno, &data)) != 0)
				goto err1;
		}
	}

	F_CLR(qp, QAM_VALID);

err1:
	if ((t_ret = __qam_fput(
	    dbp, cp->pgno, cp->page, ret == 0 ? DB_MPOOL_DIRTY : 0)) != 0)
		return (ret ? ret : t_ret);
	cp->page = NULL;
	/* Doing record locking, release the page lock */
	if ((t_ret = __LPUT(dbc, cp->lock)) != 0) {
		cp->lock = lock;
		return (ret ? ret : t_ret);
	}
	cp->lock = lock;
	return (ret);
}

/*
 * __qam_delete --
 *	Queue db->del function.
 *
 * PUBLIC: int __qam_delete __P((DB *, DB_TXN *, DBT *, u_int32_t));
 */
int
__qam_delete(dbp, txn, key, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	QUEUE_CURSOR *cp;
	DBC *dbc;
	int ret, t_ret;

	PANIC_CHECK(dbp->dbenv);
	DB_CHECK_TXN(dbp, txn);

	/* Check for invalid flags. */
	if ((ret =
	    __db_delchk(dbp, key, flags, F_ISSET(dbp, DB_AM_RDONLY))) != 0)
		return (ret);

	/* Acquire a cursor. */
	if ((ret = dbp->cursor(dbp, txn, &dbc, DB_WRITELOCK)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, txn, "qam_delete", key, NULL, flags);

	cp = (QUEUE_CURSOR *)dbc->internal;
	if ((ret = __qam_getno(dbp, key, &cp->recno)) != 0)
		goto err;

	ret = __qam_c_del(dbc);

	/* Release the cursor. */
err:	if ((t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

#ifdef	DEBUG_WOP
#define	QDEBUG
#endif

/*
 * __qam_c_get --
 *	Queue cursor->c_get function.
 */
static int
__qam_c_get(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	DB *dbp;
	DB_LOCK lock, pglock, metalock, save_lock;
	DBT tmp;
	PAGE *pg;
	QAMDATA *qp;
	QMETA *meta;
	QUEUE *t;
	QUEUE_CURSOR *cp;
	db_indx_t save_indx;
	db_lockmode_t lock_mode;
	db_pgno_t metapno, save_page;
	db_recno_t current, first, save_recno;
	qam_position_mode mode;
	u_int32_t rec_extent;
	int exact, is_first, locked, ret, t_ret, wait, with_delete;
	int put_mode, meta_dirty, retrying, skip_again, wrapped;

	cp = (QUEUE_CURSOR *)dbc->internal;
	dbp = dbc->dbp;

	PANIC_CHECK(dbp->dbenv);

	wait = 0;
	with_delete = 0;
	retrying = 0;
	rec_extent = 0;
	lock_mode = DB_LOCK_READ;
	mode = QAM_READ;
	put_mode = 0;
	t_ret = 0;
	*pgnop = 0;
	pg = NULL;
	skip_again = 0;

	if (F_ISSET(dbc, DBC_RMW)) {
		lock_mode = DB_LOCK_WRITE;
		mode = QAM_WRITE;
	}

	if (flags == DB_CONSUME_WAIT) {
		wait = 1;
		flags = DB_CONSUME;
	}
	if (flags == DB_CONSUME) {
		DB_CHECK_TXN(dbp, dbc->txn);
		with_delete = 1;
		flags = DB_FIRST;
		lock_mode = DB_LOCK_WRITE;
		mode = QAM_CONSUME;
	}

	DEBUG_LREAD(dbc, dbc->txn, "qam_c_get",
	    flags == DB_SET || flags == DB_SET_RANGE ? key : NULL, NULL, flags);

	is_first = 0;

	t = (QUEUE *)dbp->q_internal;
	/* get the meta page */
	metapno = t->q_meta;
	if ((ret = __db_lget(dbc, 0, metapno, lock_mode, 0, &metalock)) != 0)
		return (ret);
	locked = 1;
	if ((ret = memp_fget(dbp->mpf, &metapno, 0, &meta)) != 0) {
		/* We did not fetch it, we can release the lock. */
		(void)__LPUT(dbc, metalock);
		return (ret);
	}

	first = 0;

	/* Make lint and friends happy. */
	meta_dirty = 0;

	/* Release any previous lock if not in a transaction. */
	if (cp->lock.off != LOCK_INVALID) {
		(void)__TLPUT(dbc, cp->lock);
		cp->lock.off = LOCK_INVALID;
	}

retry:	/* Update the record number. */
	switch (flags) {
	case DB_CURRENT:
		break;
	case DB_NEXT_DUP:
		ret = DB_NOTFOUND;
		goto err;
		/* NOTREACHED */
	case DB_NEXT:
	case DB_NEXT_NODUP:
		if (cp->recno != RECNO_OOB) {
			++cp->recno;
			/* Wrap around, skipping zero. */
			if (cp->recno == RECNO_OOB)
				cp->recno++;
			break;
		}
		/* FALLTHROUGH */
	case DB_FIRST:
		flags = DB_NEXT;
		is_first = 1;

		/* get the first record number */
		cp->recno = first = meta->first_recno;

		break;
	case DB_PREV:
	case DB_PREV_NODUP:
		if (cp->recno != RECNO_OOB) {
			if (QAM_BEFORE_FIRST(meta, cp->recno)
			    || cp->recno == meta->first_recno) {
				ret = DB_NOTFOUND;
				goto err;
			}
			--cp->recno;
			/* Wrap around, skipping zero. */
			if (cp->recno == RECNO_OOB)
				--cp->recno;
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:
		if (meta->first_recno == meta->cur_recno) {
			ret = DB_NOTFOUND;
			goto err;
		}
		cp->recno = meta->cur_recno - 1;
		if (cp->recno == RECNO_OOB)
			cp->recno--;
		break;
	case DB_GET_BOTH:
	case DB_SET:
	case DB_SET_RANGE:
		if ((ret = __qam_getno(dbp, key, &cp->recno)) != 0)
			goto err;
		break;
	default:
		ret = __db_unknown_flag(dbp->dbenv, "__qam_c_get", flags);
		goto err;
	}

	/*
	 * Check to see if we are out of data.  Current points to
	 * the first free slot.
	 */
	if (cp->recno == meta->cur_recno ||
	    QAM_AFTER_CURRENT(meta, cp->recno)) {
		ret = DB_NOTFOUND;
		pg = NULL;
		if (wait) {
			flags = DB_FIRST;
			/*
			 * If first is not set, then we skipped a
			 * locked record, go back and find it.
			 * If we find a locked record again
			 * wait for it.
			 */
			if (first == 0) {
				retrying = 1;
				goto retry;
			}
			if (CDB_LOCKING(dbp->dbenv)) {
				if ((ret = lock_get(dbp->dbenv, dbc->locker,
				    DB_LOCK_SWITCH, &dbc->lock_dbt,
				    DB_LOCK_WAIT, &dbc->mylock)) != 0)
					goto err;
				if ((ret = lock_get(dbp->dbenv, dbc->locker,
				    DB_LOCK_UPGRADE, &dbc->lock_dbt, DB_LOCK_WRITE,
				    &dbc->mylock)) != 0)
					goto err;
				goto retry;
			}
			/*
			 * Wait for someone to update the meta page.
			 * This will probably mean there is something
			 * in the queue.  We then go back up and
			 * try again.
			 */
			if (locked == 0) {
				if ((ret = __db_lget( dbc,
				    0, metapno, lock_mode, 0, &metalock)) != 0)
					goto err;
				locked = 1;
				if (cp->recno != RECNO_OOB &&
				    !QAM_AFTER_CURRENT(meta, cp->recno))
					goto retry;
			}
			if ((ret = __db_lget(dbc, 0, metapno,
			    DB_LOCK_WAIT, DB_LOCK_SWITCH, &metalock)) != 0)
				goto err;
			if ((ret = lock_get(dbp->dbenv, dbc->locker,
			    DB_LOCK_UPGRADE, &dbc->lock_dbt, DB_LOCK_WRITE,
			    &metalock)) != 0)
				goto err;
			locked = 1;
			goto retry;
		}

		goto err;
	}

	/* Don't hold the meta page long term. */
	if (locked) {
		if ((ret = __LPUT(dbc, metalock)) != 0)
			goto err;
		locked = 0;
	}

	/* Lock the record. */
	if ((ret = __db_lget(dbc, 0, cp->recno, lock_mode,
	    (with_delete && !retrying) ?
	    DB_LOCK_NOWAIT | DB_LOCK_RECORD : DB_LOCK_RECORD,
	    &lock)) == DB_LOCK_NOTGRANTED && with_delete) {
#ifdef QDEBUG
		__db_logmsg(dbp->dbenv,
		    dbc->txn, "Queue S", 0, "%x %d %d %d",
		    dbc->locker, cp->recno, first, meta->first_recno);
#endif
		first = 0;
		goto retry;
	}

	if (ret != 0)
		goto err;

	/*
	 * In the DB_FIRST or DB_LAST cases we must wait and then start over
	 * since the first/last may have moved while we slept.
	 * We release our locks and try again.
	 */
	if ((!with_delete && is_first) || flags == DB_LAST) {
		if ((ret =
		    __db_lget(dbc, 0, metapno, lock_mode, 0, &metalock)) != 0)
			goto err;
		if (cp->recno !=
		    (is_first ? meta->first_recno : (meta->cur_recno - 1))) {
			__LPUT(dbc, lock);
			if (is_first)
				flags = DB_FIRST;
			locked = 1;
			goto retry;
		}
		/* Don't hold the meta page long term. */
		if ((ret = __LPUT(dbc, metalock)) != 0)
			goto err;
	}

	/* Position the cursor on the record. */
	if ((ret = __qam_position(dbc, &cp->recno, mode, &exact)) != 0) {
		/* We cannot get the page, release the record lock. */
		(void)__LPUT(dbc, lock);
		goto err;
	}

	pg = cp->page;
	pglock = cp->lock;
	cp->lock = lock;
	cp->lock_mode = lock_mode;

	if (!exact) {
		if (flags == DB_NEXT || flags == DB_NEXT_NODUP
		    || flags == DB_PREV || flags == DB_PREV_NODUP
		    || flags == DB_LAST) {
			/* Release locks and try again. */
			if (pg != NULL)
				(void)__qam_fput(dbp, cp->pgno, pg, 0);
			cp->page = pg = NULL;
			(void)__LPUT(dbc, pglock);
			(void)__LPUT(dbc, cp->lock);
			if (flags == DB_LAST)
				flags = DB_PREV;
			if (!with_delete)
				is_first = 0;
			retrying = 0;
			goto retry;
		}
		/* this is for the SET and SET_RANGE cases */
		ret = DB_KEYEMPTY;
		goto err1;
	}

	/* Return the key if the user didn't give us one. */
	if (key != NULL && flags != DB_SET && flags != DB_GET_BOTH &&
	    (ret = __db_retcopy(dbp, key, &cp->recno, sizeof(cp->recno),
	    &dbc->rkey.data, &dbc->rkey.ulen)) != 0)
		goto err1;

	if (key != NULL)
		F_SET(key, DB_DBT_ISSET);

	qp = QAM_GET_RECORD(dbp, pg, cp->indx);

	/* Return the data item. */
	if (flags == DB_GET_BOTH) {
		/*
		 * Need to compare
		 */
		tmp.data = qp->data;
		tmp.size = t->re_len;
		if ((ret = __bam_defcmp(dbp, data, &tmp)) != 0) {
			ret = DB_NOTFOUND;
			goto err1;
		}
	}
	if (data != NULL && (ret = __db_retcopy(dbp, data,
	    qp->data, t->re_len, &dbc->rdata.data, &dbc->rdata.ulen)) != 0)
		goto err1;

	if (data != NULL)
		F_SET(data, DB_DBT_ISSET);

	/* Finally, if we are doing DB_CONSUME mark the record. */
	if (with_delete) {
		if (DB_LOGGING(dbc)) {
			if (t->page_ext == 0 || t->re_len == 0) {
				if ((ret = __qam_del_log(dbp->dbenv, dbc->txn,
				    &LSN(pg), 0, dbp->log_fileid, &LSN(pg),
				    pg->pgno, cp->indx, cp->recno)) != 0)
					goto err1;
			} else {
				tmp.data = qp->data;
				tmp.size = t->re_len;
				if ((ret =
				   __qam_delext_log(dbp->dbenv, dbc->txn,
				   &LSN(pg), 0, dbp->log_fileid, &LSN(pg),
				   pg->pgno, cp->indx, cp->recno, &tmp)) != 0)
					goto err1;
			}
		}

		F_CLR(qp, QAM_VALID);
		put_mode = DB_MPOOL_DIRTY;

		if ((ret = __LPUT(dbc, pglock)) != 0)
			goto err;

		/*
		 * Now we need to update the metapage
		 * first pointer. If we have deleted
		 * the record that is pointed to by
		 * first_recno then we move it as far
		 * forward as we can without blocking.
		 * The metapage lock must be held for
		 * the whole scan otherwise someone could
		 * do a random insert behind where we are
		 * looking.
		 */

		if (locked == 0 && (ret = __db_lget(
		    dbc, 0, metapno, lock_mode, 0, &metalock)) != 0)
			goto err1;
		locked = 1;
#ifdef QDEBUG
		__db_logmsg(dbp->dbenv,
		    dbc->txn, "Queue D", 0, "%x %d %d %d",
		    dbc->locker, cp->recno, first, meta->first_recno);
#endif
		/*
		 * See if we deleted the "first" record.  If
		 * first is zero then we skipped something,
		 * see if first_recno has been move passed
		 * that to the record that we deleted.
		 */
		if (first == 0)
			first = cp->recno;
		if (first != meta->first_recno)
			goto done;

		save_page = cp->pgno;
		save_indx = cp->indx;
		save_recno = cp->recno;
		save_lock = cp->lock;

		/*
		 * If we skipped some deleted records, we need to
		 * reposition on the first one.  Get a lock
		 * in case someone is trying to put it back.
		 */
		if (first != cp->recno) {
			ret = __db_lget(dbc, 0, first, DB_LOCK_READ,
			    DB_LOCK_NOWAIT | DB_LOCK_RECORD, &lock);
			if (ret == DB_LOCK_NOTGRANTED) {
				ret = 0;
				goto done;
			}
			if (ret != 0)
				goto err1;
			if ((ret =
			    __qam_fput(dbp, cp->pgno, cp->page, put_mode)) != 0)
				goto err1;
			cp->page = NULL;
			put_mode = 0;
			if ((ret = __qam_position(dbc,
			    &first, QAM_READ, &exact)) != 0 || exact != 0) {
				(void)__LPUT(dbc, lock);
				goto err1;
			}
			if ((ret =__LPUT(dbc, lock)) != 0)
				goto err1;
			if ((ret = __LPUT(dbc, cp->lock)) != 0)
				goto err1;
		}

		current = meta->cur_recno;
		wrapped = 0;
		if (first > current)
			wrapped = 1;
		rec_extent = meta->page_ext * meta->rec_page;

		/* Loop until we find a record or hit current */
		for (;;) {
			/*
			 * Check to see if we are moving off the extent
			 * and remove the extent.
			 * If we are moving off a page we need to
			 * get rid of the buffer.
			 * Wait for the lagging readers to move off the
			 * page.
			 */
			if (rec_extent != 0
			    && ((exact = first % rec_extent == 0)
			    || first % meta->rec_page == 0
			    || first == UINT32_T_MAX)) {
				if (exact == 1 && (ret = __db_lget(dbc,
				    0, cp->pgno, DB_LOCK_WRITE, 0, &cp->lock)) != 0)
					break;

#ifdef QDEBUG
				__db_logmsg(dbp->dbenv,
				    dbc->txn, "Queue R", 0, "%x %d %d %d",
				    dbc->locker, cp->pgno, first, meta->first_recno);
#endif
				put_mode |= DB_MPOOL_DISCARD;
				if ((ret = __qam_fput(dbp,
				    cp->pgno, cp->page, put_mode)) != 0)
					break;
				cp->page = NULL;

				if (exact == 1) {
					ret = __qam_fremove(dbp, cp->pgno);
					t_ret = __LPUT(dbc, cp->lock);
				}
				if (ret != 0)
					break;
				if (t_ret != 0) {
					ret = t_ret;
					break;
				}
			} else if ((ret =
			    __qam_fput(dbp, cp->pgno, cp->page, put_mode)) != 0)
				break;
			cp->page = NULL;
			first++;
			if (first == RECNO_OOB) {
				wrapped = 0;
				first++;
			}

			/*
			 * LOOP EXIT when we come move to the current
			 * pointer.
			 */
			if (!wrapped && first >= current)
				break;

			ret = __db_lget(dbc, 0, first, DB_LOCK_READ,
			    DB_LOCK_NOWAIT | DB_LOCK_RECORD, &lock);
			if (ret == DB_LOCK_NOTGRANTED) {
				ret = 0;
				break;
			}
			if (ret != 0)
				break;

			if ((ret = __qam_position(dbc,
			    &first, QAM_READ, &exact)) != 0) {
				(void)__LPUT(dbc, lock);
				break;
			}
			put_mode = 0;
			if ((ret =__LPUT(dbc, lock)) != 0
			    || (ret = __LPUT(dbc, cp->lock)) != 0 ||exact) {
				if ((t_ret = __qam_fput(dbp, cp->pgno,
				    cp->page, put_mode)) != 0 && ret == 0)
					ret = t_ret;
				cp->page = NULL;
				break;
			}
		}

		cp->pgno = save_page;
		cp->indx = save_indx;
		cp->recno = save_recno;
		cp->lock = save_lock;

		/*
		 * We have advanced as far as we can.
		 * Advance first_recno to this point.
		 */
		if (meta->first_recno != first) {
#ifdef QDEBUG
		__db_logmsg(dbp->dbenv, dbc->txn, "Queue M",
		    0, "%x %d %d %d", dbc->locker, cp->recno,
		    first, meta->first_recno);
#endif
			if (DB_LOGGING(dbc))
				if ((ret =
				     __qam_incfirst_log(dbp->dbenv,
				     dbc->txn, &meta->dbmeta.lsn, 0,
				     dbp->log_fileid, cp->recno)) != 0)
					goto err;
			meta->first_recno = first;
			meta_dirty = 1;
		}
	}

done:
err1:	if (cp->page != NULL) {
		t_ret = __qam_fput(dbp, cp->pgno, cp->page, put_mode);

		if (!ret)
			ret = t_ret;
		/* Doing record locking, release the page lock */
		t_ret = __LPUT(dbc, pglock);
		cp->page = NULL;
	}

err:	if (!ret)
		ret = t_ret;
	if (meta) {

		/* release the meta page */
		t_ret = memp_fput(
		    dbp->mpf, meta, meta_dirty ? DB_MPOOL_DIRTY : 0);

		if (!ret)
			ret = t_ret;

		/* Don't hold the meta page long term. */
		if (locked)
			t_ret = __LPUT(dbc, metalock);
	}
	DB_ASSERT(metalock.off == LOCK_INVALID);

	/*
	 * There is no need to keep the record locked if we are
	 * not in a transaction.
	 */
	if (t_ret == 0)
		t_ret = __TLPUT(dbc, cp->lock);

	return (ret ? ret : t_ret);
}

/*
 * __qam_c_close --
 *	Close down the cursor from a single use.
 */
static int
__qam_c_close(dbc, root_pgno, rmroot)
	DBC *dbc;
	db_pgno_t root_pgno;
	int *rmroot;
{
	QUEUE_CURSOR *cp;

	COMPQUIET(root_pgno, 0);
	COMPQUIET(rmroot, NULL);

	cp = (QUEUE_CURSOR *)dbc->internal;

	/* Discard any locks not acquired inside of a transaction. */
	if (cp->lock.off != LOCK_INVALID) {
		(void)__TLPUT(dbc, cp->lock);
		cp->lock.off = LOCK_INVALID;
	}

	cp->page = NULL;
	cp->pgno = PGNO_INVALID;
	cp->indx = 0;
	cp->lock.off = LOCK_INVALID;
	cp->lock_mode = DB_LOCK_NG;
	cp->recno = RECNO_OOB;
	cp->flags = 0;

	return (0);
}

/*
 * __qam_c_dup --
 *	Duplicate a queue cursor, such that the new one holds appropriate
 *	locks for the position of the original.
 *
 * PUBLIC: int __qam_c_dup __P((DBC *, DBC *));
 */
int
__qam_c_dup(orig_dbc, new_dbc)
	DBC *orig_dbc, *new_dbc;
{
	QUEUE_CURSOR *orig, *new;

	orig = (QUEUE_CURSOR *)orig_dbc->internal;
	new = (QUEUE_CURSOR *)new_dbc->internal;

	new->recno = orig->recno;

	/* reget the long term lock if we are not in a xact */
	if (orig_dbc->txn != NULL ||
	    !STD_LOCKING(orig_dbc) || orig->lock.off == LOCK_INVALID)
		return (0);

	return (__db_lget(new_dbc,
	    0, new->recno, new->lock_mode, DB_LOCK_RECORD, &new->lock));
}

/*
 * __qam_c_init
 *
 * PUBLIC: int __qam_c_init __P((DBC *));
 */
int
__qam_c_init(dbc)
	DBC *dbc;
{
	QUEUE_CURSOR *cp;
	DB *dbp;
	int ret;

	dbp = dbc->dbp;

	/* Allocate the internal structure. */
	cp = (QUEUE_CURSOR *)dbc->internal;
	if (cp == NULL) {
		if ((ret =
		    __os_calloc(dbp->dbenv, 1, sizeof(QUEUE_CURSOR), &cp)) != 0)
			return (ret);
		dbc->internal = (DBC_INTERNAL *)cp;
	}

	/* Initialize methods. */
	dbc->c_close = __db_c_close;
	dbc->c_count = __db_c_count;
	dbc->c_del = __db_c_del;
	dbc->c_dup = __db_c_dup;
	dbc->c_get = __db_c_get;
	dbc->c_put = __db_c_put;
	dbc->c_am_close = __qam_c_close;
	dbc->c_am_del = __qam_c_del;
	dbc->c_am_destroy = __qam_c_destroy;
	dbc->c_am_get = __qam_c_get;
	dbc->c_am_put = __qam_c_put;
	dbc->c_am_writelock = NULL;

	return (0);
}

/*
 * __qam_c_destroy --
 *	Close a single cursor -- internal version.
 */
static int
__qam_c_destroy(dbc)
	DBC *dbc;
{
	/* Discard the structures. */
	__os_free(dbc->internal, sizeof(QUEUE_CURSOR));

	return (0);
}

/*
 * __qam_getno --
 *	Check the user's record number.
 */
static int
__qam_getno(dbp, key, rep)
	DB *dbp;
	const DBT *key;
	db_recno_t *rep;
{
	if ((*rep = *(db_recno_t *)key->data) == 0) {
		__db_err(dbp->dbenv, "illegal record number of 0");
		return (EINVAL);
	}
	return (0);
}
