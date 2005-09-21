/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: sequence.c,v 1.26 2004/10/25 17:59:28 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_page.h"
#include "dbinc/db_swap.h"
#include "dbinc/db_am.h"
#include "dbinc/mp.h"
#include "dbinc_auto/sequence_ext.h"

#ifdef HAVE_SEQUENCE
#define	SEQ_ILLEGAL_AFTER_OPEN(seq, name)				\
	if (seq->seq_key.data != NULL)					\
		return (__db_mi_open((seq)->seq_dbp->dbenv, name, 1));

#define	SEQ_ILLEGAL_BEFORE_OPEN(seq, name)				\
	if (seq->seq_key.data == NULL)					\
		return (__db_mi_open((seq)->seq_dbp->dbenv, name, 0));

#define SEQ_SWAP(rp)	\
	do {							\
		M_32_SWAP((rp)->seq_version);			\
		M_32_SWAP((rp)->flags);				\
		M_64_SWAP((rp)->seq_value);			\
		M_64_SWAP((rp)->seq_max);			\
		M_64_SWAP((rp)->seq_min);			\
	} while (0)

#define SEQ_SWAP_IN(seq) \
	do {								\
		if (__db_isbigendian()) {				\
			memcpy(&seq->seq_record, seq->seq_data.data,	\
			     sizeof(seq->seq_record));			\
			SEQ_SWAP(&seq->seq_record);			\
		}							\
	} while (0)
		
#define SEQ_SWAP_OUT(seq) \
	do {								\
		if (__db_isbigendian()) {				\
			memcpy(seq->seq_data.data,			\
			     &seq->seq_record, sizeof(seq->seq_record));\
			SEQ_SWAP((DB_SEQ_RECORD*)seq->seq_data.data);	\
		}							\
	} while (0)
		

static int __seq_close __P((DB_SEQUENCE *, u_int32_t));
static int __seq_get __P((DB_SEQUENCE *,
			    DB_TXN *, int32_t,  db_seq_t *, u_int32_t));
static int __seq_get_cachesize __P((DB_SEQUENCE *, int32_t *));
static int __seq_get_flags __P((DB_SEQUENCE *, u_int32_t *));
static int __seq_get_key __P((DB_SEQUENCE *, DBT *));
static int __seq_get_range __P((DB_SEQUENCE *, db_seq_t *, db_seq_t *));
static int __seq_set_range __P((DB_SEQUENCE *, db_seq_t, db_seq_t));
static int __seq_get_db __P((DB_SEQUENCE *, DB **));
static int __seq_initial_value __P((DB_SEQUENCE *, db_seq_t));
static int __seq_open __P((DB_SEQUENCE *, DB_TXN *, DBT *, u_int32_t));
static int __seq_remove __P((DB_SEQUENCE *, DB_TXN *, u_int32_t));
static int __seq_set_cachesize __P((DB_SEQUENCE *, int32_t));
static int __seq_set_flags __P((DB_SEQUENCE *, u_int32_t));
static int __seq_update __P((DB_SEQUENCE *, DB_TXN *, int32_t, u_int32_t));

/*
 * db_sequence_create --
 *	DB_SEQUENCE constructor.
 *
 * EXTERN: int db_sequence_create __P((DB_SEQUENCE **, DB *, u_int32_t));
 */
int
db_sequence_create(seqp, dbp, flags)
	DB_SEQUENCE **seqp;
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_SEQUENCE *seq;
	int ret;

	dbenv = dbp->dbenv;

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	default:
		return (__db_ferr(dbenv, "db_sequence_create", 0));
	}

	DB_ILLEGAL_BEFORE_OPEN(dbp, "db_sequence_create");

	/* Allocate the sequence. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(*seq), &seq)) != 0)
		return (ret);

	seq->seq_dbp = dbp;
	seq->close = __seq_close;
	seq->get = __seq_get;
	seq->get_cachesize = __seq_get_cachesize;
	seq->set_cachesize = __seq_set_cachesize;
	seq->get_db = __seq_get_db;
	seq->get_flags = __seq_get_flags;
	seq->get_key = __seq_get_key;
	seq->get_range = __seq_get_range;
	seq->initial_value = __seq_initial_value;
	seq->open = __seq_open;
	seq->remove = __seq_remove;
	seq->set_flags = __seq_set_flags;
	seq->set_range = __seq_set_range;
	seq->stat = __seq_stat;
	seq->stat_print = __seq_stat_print;
	seq->seq_rp = &seq->seq_record;
	*seqp = seq;

	return (0);
}

/*
 * __seq_open --
 *	DB_SEQUENCE->open method.
 *
 */
static int
__seq_open(seq, txn, keyp, flags)
	DB_SEQUENCE *seq;
	DB_TXN *txn;
	DBT *keyp;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	DB_SEQ_RECORD *rp;
	u_int32_t tflags;
	int txn_local, ret;
#define	SEQ_OPEN_FLAGS (DB_AUTO_COMMIT | DB_CREATE | DB_EXCL | DB_THREAD)

	dbp = seq->seq_dbp;
	dbenv = dbp->dbenv;
	txn_local = 0;

	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->open");
	if (keyp->size == 0) {
		__db_err(dbenv, "Zero length sequence key specified");
		return (EINVAL);
	}

	if (LF_ISSET(~SEQ_OPEN_FLAGS))
		return (__db_ferr(dbenv, "DB_SEQUENCE->open", 0));

	if ((ret = dbp->get_flags(dbp, &tflags)) != 0)
		return (ret);

	if (FLD_ISSET(tflags, DB_DUP)) {
		__db_err(dbenv,
	"Sequences not supported in databases configured for duplicate data");
		return (EINVAL);
	}

	if (LF_ISSET(DB_THREAD)) {
		dbmp = dbenv->mp_handle;
		if ((ret = __db_mutex_setup(dbenv, dbmp->reginfo,
		     &seq->seq_mutexp, MUTEX_ALLOC | MUTEX_THREAD)) != 0)
			return (ret);
	}

	memset(&seq->seq_data, 0, sizeof(DBT));
	if (__db_isbigendian()) {
		if ((ret = __os_umalloc(dbenv,
		     sizeof(seq->seq_record), &seq->seq_data.data)) != 0)
			goto err;
		seq->seq_data.flags = DB_DBT_REALLOC;
	} else {
		seq->seq_data.data = &seq->seq_record;
		seq->seq_data.flags = DB_DBT_USERMEM;
	}

	seq->seq_data.ulen = seq->seq_data.size = sizeof(seq->seq_record);
	seq->seq_rp = &seq->seq_record;

	memset(&seq->seq_key, 0, sizeof(DBT));
	if ((ret = __os_malloc(dbenv, keyp->size, &seq->seq_key.data)) != 0)
		return (ret);
	memcpy(seq->seq_key.data, keyp->data, keyp->size);
	seq->seq_key.size = seq->seq_key.ulen = keyp->size;
	seq->seq_key.flags = DB_DBT_USERMEM;

		

retry:	if ((ret = dbp->get(dbp, txn, &seq->seq_key, &seq->seq_data, 0)) != 0) {
		if (ret == DB_BUFFER_SMALL &&
		    seq->seq_data.size > sizeof(seq->seq_record)) {
			seq->seq_data.flags = DB_DBT_REALLOC;
			seq->seq_data.data = NULL;
			goto retry;
		}
		if ((ret != DB_NOTFOUND && ret != DB_KEYEMPTY) ||
		    !LF_ISSET(DB_CREATE))
			goto err;
		ret = 0;

		rp = &seq->seq_record;
		tflags = DB_NOOVERWRITE;
		tflags |= LF_ISSET(DB_AUTO_COMMIT);
		if (!F_ISSET(rp, DB_SEQ_RANGE_SET)) {
			rp->seq_max = INT64_MAX;
			rp->seq_min = INT64_MIN;
		}
		/* INC is the default. */
		if (!F_ISSET(rp, DB_SEQ_DEC))
			F_SET(rp, DB_SEQ_INC);

		rp->seq_version = DB_SEQUENCE_VERSION;

		if (rp->seq_value > rp->seq_max ||
		    rp->seq_value < rp->seq_min) {
			__db_err(dbenv, "Sequence value out of range");
			ret = EINVAL;
			goto err;
		} else {
			SEQ_SWAP_OUT(seq);
			if ((ret = dbp->put(dbp, txn,
			    &seq->seq_key, &seq->seq_data, tflags)) != 0) {
				__db_err(dbenv, "Sequence create failed");
				goto err;
			}
		}
	} else if (LF_ISSET(DB_CREATE) && LF_ISSET(DB_EXCL)) {
		ret = EEXIST;
		goto err;
	} else if (seq->seq_data.size < sizeof(seq->seq_record)) {
		__db_err(dbenv, "Bad sequence record format");
		ret = EINVAL;
		goto err;
	}

	if (!__db_isbigendian())
		seq->seq_rp = seq->seq_data.data;

	/*
	 * The first release was stored in native mode.
	 * Check the verison number before swapping.
	 */
	rp = seq->seq_data.data;
	if (rp->seq_version == DB_SEQUENCE_OLDVER) {
oldver:		rp->seq_version = DB_SEQUENCE_VERSION;
		if (__db_isbigendian()) {
			if (IS_AUTO_COMMIT(dbp, txn, flags)) {
				if ((ret =
				     __db_txn_auto_init(dbenv, &txn)) != 0)
					return (ret);
				txn_local = 1;
				LF_CLR(DB_AUTO_COMMIT);
				goto retry;
			} else
				txn_local = 0;
			memcpy(&seq->seq_record, rp, sizeof(seq->seq_record));
			SEQ_SWAP_OUT(seq);
		}
		if ((ret = dbp->put(dbp,
		     txn, &seq->seq_key, &seq->seq_data, 0)) != 0)
			goto err;
	}
	rp = seq->seq_rp;

	SEQ_SWAP_IN(seq);

	if (rp->seq_version != DB_SEQUENCE_VERSION) {
		/*
		 * The database may have moved from one type
		 * of machine to another, check here.
		 * If we moved from little-end to big-end then
		 * the swap above will make the version correct.
		 * If the move was from big to little
		 * then we need to swap to see if this
		 * is an old version.
		 */
		if (rp->seq_version == DB_SEQUENCE_OLDVER)
			goto oldver;
		M_32_SWAP(rp->seq_version);
		if (rp->seq_version == DB_SEQUENCE_OLDVER) {
			SEQ_SWAP(rp);
			goto oldver;
		}
		M_32_SWAP(rp->seq_version);
		__db_err(dbenv,
		     "Unknown sequence version: %d", rp->seq_version);
		goto err;
	}

	seq->seq_last_value = rp->seq_value;
	if (F_ISSET(rp, DB_SEQ_INC))
		seq->seq_last_value--;
	else
		seq->seq_last_value++;

err:	if (ret != 0) {
		__os_free(dbenv, seq->seq_key.data);
		seq->seq_key.data = NULL;
	}
	return (txn_local ? __db_txn_auto_resolve(dbenv, txn, 0, ret) : ret);
}

/*
 * __seq_get_cachesize --
 *	Accessor for value passed into DB_SEQUENCE->set_cachesize call.
 *
 */
static int
__seq_get_cachesize(seq, cachesize)
	DB_SEQUENCE *seq;
	int32_t *cachesize;
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_cachesize");

	*cachesize = seq->seq_cache_size;
	return (0);
}

/*
 * __seq_set_cachesize --
 *	DB_SEQUENCE->set_cachesize.
 *
 */
static int
__seq_set_cachesize(seq, cachesize)
	DB_SEQUENCE *seq;
	int32_t cachesize;
{
	if (cachesize < 0) {
		__db_err(seq->seq_dbp->dbenv,
		     "Illegal cache size: %d", cachesize);
		return (EINVAL);
	}
	seq->seq_cache_size = cachesize;
	return (0);
}

#define	SEQ_SET_FLAGS	(DB_SEQ_WRAP | DB_SEQ_INC | DB_SEQ_DEC)
/*
 * __seq_get_flags --
 *	Accessor for flags passed into DB_SEQUENCE->open call
 *
 */
static int
__seq_get_flags(seq, flagsp)
	DB_SEQUENCE *seq;
	u_int32_t *flagsp;
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_flags");

	*flagsp = F_ISSET(seq->seq_rp, SEQ_SET_FLAGS);
	return (0);
}

/*
 * __seq_set_flags --
 *	DB_SEQUENCE->set_flags.
 *
 */
static int
__seq_set_flags(seq, flags)
	DB_SEQUENCE *seq;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_SEQ_RECORD *rp;
	int ret;

	dbenv = seq->seq_dbp->dbenv;
	rp = seq->seq_rp;
	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->set_flags");

	if (LF_ISSET(~SEQ_SET_FLAGS))
		return (__db_ferr(dbenv, "DB_SEQUENCE->set_flags", 0));

	if ((ret = __db_fcchk(dbenv,
	     "DB_SEQUENCE->set_flags", flags, DB_SEQ_DEC, DB_SEQ_INC)) != 0)
		return (ret);

	if (LF_ISSET(DB_SEQ_DEC | DB_SEQ_INC))
		F_CLR(rp, DB_SEQ_DEC | DB_SEQ_INC);
	F_SET(rp, flags);

	return (0);
}

/*
 * __seq_initial_value --
 *	DB_SEQUENCE->init_value.
 *
 */
static int
__seq_initial_value(seq, value)
	DB_SEQUENCE *seq;
	db_seq_t value;
{
	DB_ENV *dbenv;
	DB_SEQ_RECORD *rp;

	dbenv = seq->seq_dbp->dbenv;
	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->init_value");

	rp = seq->seq_rp;
	if (F_ISSET(rp, DB_SEQ_RANGE_SET) &&
	     (value > rp->seq_max || value < rp->seq_min)) {
		__db_err(dbenv, "Sequence value out of range");
		return (EINVAL);
	}

	rp->seq_value = value;

	return (0);
}

/*
 * __seq_get_range --
 *	Accessor for range passed into DB_SEQUENCE->set_range call
 *
 */
static int
__seq_get_range(seq, minp, maxp)
	DB_SEQUENCE *seq;
	db_seq_t *minp, *maxp;
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_range");

	F_SET(seq->seq_rp, DB_SEQ_RANGE_SET);
	*minp = seq->seq_rp->seq_min;
	*maxp = seq->seq_rp->seq_max;
	return (0);
}

/*
 * __seq_set_range --
 *	SEQUENCE->set_range.
 *
 */
static int
__seq_set_range(seq, min, max)
	DB_SEQUENCE *seq;
	db_seq_t min, max;
{
	DB_ENV *dbenv;

	dbenv = seq->seq_dbp->dbenv;
	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->set_range");

	if (min >= max) {
		__db_err(dbenv, "Illegal sequence range");
		return (EINVAL);
	}

	seq->seq_rp->seq_min = min;
	seq->seq_rp->seq_max = max;
	F_SET(seq->seq_rp, DB_SEQ_RANGE_SET);

	return (0);
}

static int
__seq_update(seq, txn, delta, flags)
	DB_SEQUENCE *seq;
	DB_TXN *txn;
	int32_t delta;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_SEQ_RECORD *rp;
	int32_t adjust;
	int ret;

	dbp = seq->seq_dbp;
	dbenv = dbp->dbenv;

	if (LF_ISSET(DB_AUTO_COMMIT) &&
	    (ret = __db_txn_auto_init(dbenv, &txn)) != 0)
		return (ret);
retry:
	if ((ret = dbp->get(dbp, txn, &seq->seq_key, &seq->seq_data, 0)) != 0) {
		if (ret == DB_BUFFER_SMALL &&
		    seq->seq_data.size > sizeof(seq->seq_record)) {
			seq->seq_data.flags = DB_DBT_REALLOC;
			seq->seq_data.data = NULL;
			goto retry;
		}
		goto err;
	}

	if (!__db_isbigendian())
		seq->seq_rp = seq->seq_data.data;
	SEQ_SWAP_IN(seq);
	rp = seq->seq_rp;

	if (seq->seq_data.size < sizeof(seq->seq_record)) {
		__db_err(dbenv, "Bad sequence record format");
		ret = EINVAL;
		goto err;
	}

	adjust = delta > seq->seq_cache_size ? delta : seq->seq_cache_size;

	/*
	 * Check whether this operation will cause the sequence to wrap.
	 *
	 * The sequence minimum and maximum values can be INT64_MIN and
	 * INT64_MAX, so we need to do the test carefully to cope with
	 * arithmetic overflow.  That means we need to check whether the value
	 * is in a range, we can't get away with a single comparison.
	 *
	 * For example, if seq_value == -1 and seq_max == INT64_MAX, the first
	 * test below will be true, since -1 - (INT64_MAX + 1) == INT64_MAX.
	 * The second part of the test makes sure that seq_value is close
	 * enough to the maximum to really cause wrapping.
	 */
	if (F_ISSET(rp, DB_SEQ_INC)) {
		if (rp->seq_value - ((rp->seq_max - adjust) + 2) >= 0 &&
		    (rp->seq_max + 1) - rp->seq_value >= 0) {
			if (F_ISSET(rp, DB_SEQ_WRAP))
				rp->seq_value = rp->seq_min;
			else {
overflow:			__db_err(dbenv, "Sequence overflow");
				ret = EINVAL;
				goto err;
			}
		}
	} else {
		if (rp->seq_value - (rp->seq_min - 1) >= 0 &&
		    (rp->seq_min + adjust - 2) - rp->seq_value >= 0) {
			if (F_ISSET(rp, DB_SEQ_WRAP))
				rp->seq_value = rp->seq_max;
			else
				goto overflow;
		}
		adjust = -adjust;
	}

	rp->seq_value += adjust;
	SEQ_SWAP_OUT(seq);
	ret = dbp->put(dbp, txn, &seq->seq_key, &seq->seq_data, 0);
	rp->seq_value -= adjust;
	if (ret != 0) {
		__db_err(dbenv, "Sequence update failed");
		goto err;
	}
	seq->seq_last_value = rp->seq_value + adjust;
	if (F_ISSET(rp, DB_SEQ_INC))
		seq->seq_last_value--;
	else
		seq->seq_last_value++;

err:	if (LF_ISSET(DB_AUTO_COMMIT))
		ret = __db_txn_auto_resolve(dbenv,
		    txn, LF_ISSET(DB_TXN_NOSYNC), ret);
	return (ret);

}

static int
__seq_get(seq, txn, delta, retp, flags)
	DB_SEQUENCE *seq;
	DB_TXN *txn;
	int32_t delta;
	db_seq_t *retp;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_SEQ_RECORD *rp;
	int ret;

	dbp = seq->seq_dbp;
	dbenv = dbp->dbenv;
	rp = seq->seq_rp;
	ret = 0;

	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get");

	if (delta <= 0) {
		__db_err(dbenv, "Sequence delta must be greater than 0");
		return (EINVAL);
	}
	MUTEX_THREAD_LOCK(dbenv, seq->seq_mutexp);

	if (rp->seq_min + delta > rp->seq_max) {
		__db_err(dbenv, "Sequence overflow");
		ret = EINVAL;
		goto err;
	}

	if (F_ISSET(rp, DB_SEQ_INC)) {
		if (seq->seq_last_value + 1 - rp->seq_value < delta &&
		   (ret = __seq_update(seq, txn, delta, flags)) != 0)
			goto err;

		rp = seq->seq_rp;
		*retp = rp->seq_value;
		rp->seq_value += delta;
	} else {
		if ((rp->seq_value - seq->seq_last_value) + 1 < delta &&
		    (ret = __seq_update(seq, txn, delta, flags)) != 0)
			goto err;

		rp = seq->seq_rp;
		*retp = rp->seq_value;
		rp->seq_value -= delta;
	}

err:	MUTEX_THREAD_UNLOCK(dbenv, seq->seq_mutexp);

	return (ret);
}

/*
 * __seq_get_db --
 *	Accessor for dbp passed into DB_SEQUENCE->open call
 *
 */
static int
__seq_get_db(seq, dbpp)
	DB_SEQUENCE *seq;
	DB **dbpp;
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_db");

	*dbpp = seq->seq_dbp;
	return (0);
}

/*
 * __seq_get_key --
 *	Accessor for key passed into DB_SEQUENCE->open call
 *
 */
static int
__seq_get_key(seq, key)
	DB_SEQUENCE *seq;
	DBT *key;
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_key");

	key->data = seq->seq_key.data;
	key->size = key->ulen = seq->seq_key.size;
	key->flags = seq->seq_key.flags;
	return (0);
}

/*
 * __seq_close --
 *	Close a sequence
 *
 */
static int
__seq_close(seq, flags)
	DB_SEQUENCE *seq;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	int ret;

	ret = 0;
	dbenv = seq->seq_dbp->dbenv;

	if (flags != 0)
		ret = __db_ferr(dbenv, "DB_SEQUENCE->close", 0);
	if (seq->seq_mutexp != NULL) {
		dbmp = dbenv->mp_handle;
		__db_mutex_free(dbenv, dbmp->reginfo, seq->seq_mutexp);
	}
	if (seq->seq_key.data != NULL)
		__os_free(dbenv, seq->seq_key.data);
	if (seq->seq_data.data != NULL &&
	    seq->seq_data.data != &seq->seq_record)
		__os_ufree(dbenv, seq->seq_data.data);
	seq->seq_key.data = NULL;
	memset(seq, CLEAR_BYTE, sizeof(*seq));
	__os_free(dbenv, seq);
	return (ret);
}

/*
 * __seq_remove --
 *	Remove a sequence from the database.
 */
static int
__seq_remove(seq, txn, flags)
	DB_SEQUENCE *seq;
	DB_TXN *txn;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int ret, t_ret;

	dbp = seq->seq_dbp;
	dbenv = dbp->dbenv;

	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->remove");

	if (LF_ISSET(DB_AUTO_COMMIT) &&
	    (ret = __db_txn_auto_init(dbenv, &txn)) != 0)
		goto err;

	ret = dbp->del(dbp, txn, &seq->seq_key, 0);

	if (LF_ISSET(DB_AUTO_COMMIT))
		ret = __db_txn_auto_resolve(dbenv,
		    txn, LF_ISSET(DB_TXN_NOSYNC), ret);

err: if ((t_ret = __seq_close(seq, 0)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

#else /* !HAVE_SEQUENCE */

int
db_sequence_create(seqp, dbp, flags)
	DB_SEQUENCE **seqp;
	DB *dbp;
	u_int32_t flags;
{
	COMPQUIET(seqp, NULL);
	COMPQUIET(flags, 0);
	__db_err(dbp->dbenv,
	    "library build did not include support for sequences");
	return (DB_OPNOTSUP);
}
#endif /* HAVE_SEQUENCE */
