/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: seq_stat.c,v 1.19 2004/09/28 17:28:15 bostic Exp $
 */

#include "db_config.h"

#ifdef HAVE_SEQUENCE
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc_auto/sequence_ext.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"

#ifdef HAVE_STATISTICS
static int __seq_print_all __P((DB_SEQUENCE *, u_int32_t));
static int __seq_print_stats __P((DB_SEQUENCE *, u_int32_t));

/*
 * __seq_stat --
 *	Get statistics from the sequence.
 *
 * PUBLIC: int __seq_stat __P((DB_SEQUENCE *, DB_SEQUENCE_STAT **, u_int32_t));
 */
int
__seq_stat(seq, spp, flags)
	DB_SEQUENCE *seq;
	DB_SEQUENCE_STAT **spp;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_SEQ_RECORD record;
	DB_SEQUENCE_STAT *sp;
	DBT data;
	int ret;

	dbp = seq->seq_dbp;
	dbenv = dbp->dbenv;
	switch (flags) {
	case DB_STAT_CLEAR:
	case DB_STAT_ALL:
	case 0:
		break;
	default:
		return (__db_ferr(dbenv, "DB_SEQUENCE->stat", 0));
	}

	/* Allocate and clear the structure. */
	if ((ret = __os_umalloc(dbenv, sizeof(*sp), &sp)) != 0)
		return (ret);
	memset(sp, 0, sizeof(*sp));

	if (seq->seq_mutexp != NULL) {
		sp->st_wait = seq->seq_mutexp->mutex_set_wait;
		sp->st_nowait = seq->seq_mutexp->mutex_set_nowait;

		if (LF_ISSET(DB_STAT_CLEAR))
			MUTEX_CLEAR(seq->seq_mutexp);
	}
	memset(&data, 0, sizeof(data));
	data.data = &record;
	data.ulen = sizeof(record);
	data.flags = DB_DBT_USERMEM;
retry:	if ((ret = dbp->get(dbp, NULL, &seq->seq_key, &data, 0)) != 0) {
		if (ret == DB_BUFFER_SMALL &&
		    data.size > sizeof(seq->seq_record)) {
			if ((ret = __os_malloc(dbenv,
			    data.size, &data.data)) != 0)
				return (ret);
			data.ulen = data.size;
			goto retry;
		}
		return (ret);
	}

	if (data.data != &record)
		memcpy(&record, data.data, sizeof(record));
	sp->st_current = record.seq_value;
	sp->st_value = seq->seq_record.seq_value;
	sp->st_last_value = seq->seq_last_value;
	sp->st_min = seq->seq_record.seq_min;
	sp->st_max = seq->seq_record.seq_max;
	sp->st_cache_size = seq->seq_cache_size;
	sp->st_flags = seq->seq_record.flags;

	*spp = sp;
	if (data.data != &record)
		__os_free(dbenv, data.data);
	return (0);
}

/*
 * __seq_stat_print --
 *	Print statistics from the sequence.
 *
 * PUBLIC: int __seq_stat_print __P((DB_SEQUENCE *, u_int32_t));
 */
int
__seq_stat_print(seq, flags)
	DB_SEQUENCE *seq;
	u_int32_t flags;
{
	int ret;

	if ((ret = __seq_print_stats(seq, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __seq_print_all(seq, flags)) != 0)
		return (ret);

	return (0);

}

static const FN __db_seq_flags_fn[] = {
	{ DB_SEQ_DEC,		"decrement" },
	{ DB_SEQ_INC,		"increment" },
	{ DB_SEQ_RANGE_SET,	"range set (internal)" },
	{ DB_SEQ_WRAP,		"wraparound at end" },
	{ 0,			NULL }
};

/*
 * __db_get_seq_flags_fn --
 *	Return the __db_seq_flags_fn array.
 *
 * PUBLIC: const FN * __db_get_seq_flags_fn __P((void));
 */
const FN *
__db_get_seq_flags_fn()
{
	return (__db_seq_flags_fn);
}

/*
 * __seq_print_stats --
 *	Display sequence stat structure.
 */
static int
__seq_print_stats(seq, flags)
	DB_SEQUENCE *seq;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_SEQUENCE_STAT *sp;
	int ret;

	dbenv = seq->seq_dbp->dbenv;

	if ((ret = __seq_stat(seq, &sp, flags)) != 0)
		return (ret);
	__db_dl_pct(dbenv,
	    "The number of sequence locks that required waiting",
	    (u_long)sp->st_wait,
	     DB_PCT(sp->st_wait, sp->st_wait + sp->st_nowait), NULL);
	STAT_FMT("The current sequence value",
	     INT64_FMT, int64_t, sp->st_current);
	STAT_FMT("The cached sequence value",
	    INT64_FMT, int64_t, sp->st_value);
	STAT_FMT("The last cached sequence value",
	    INT64_FMT, int64_t, sp->st_last_value);
	STAT_FMT("The minimum sequence value",
	    INT64_FMT, int64_t, sp->st_value);
	STAT_FMT("The maximum sequence value",
	    INT64_FMT, int64_t, sp->st_value);
	STAT_ULONG("The cache size", sp->st_cache_size);
	__db_prflags(dbenv, NULL,
	    sp->st_flags, __db_seq_flags_fn, NULL, "\tSequence flags");
	__os_ufree(seq->seq_dbp->dbenv, sp);
	return (0);
}

/*
 * __seq_print_all --
 *	Display sequence debugging information - none for now.
 *	(The name seems a bit strange, no?)
 */
static int
__seq_print_all(seq, flags)
	DB_SEQUENCE *seq;
	u_int32_t flags;
{
	COMPQUIET(seq, NULL);
	COMPQUIET(flags, 0);
	return (0);
}

#else /* !HAVE_STATISTICS */

int
__seq_stat(seq, statp, flags)
	DB_SEQUENCE *seq;
	DB_SEQUENCE_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(seq->seq_dbp->dbenv));
}

int
__seq_stat_print(seq, flags)
	DB_SEQUENCE *seq;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(seq->seq_dbp->dbenv));
}

/*
 * __db_get_seq_flags_fn --
 *	Return the __db_seq_flags_fn array.
 *
 * PUBLIC: const FN * __db_get_seq_flags_fn __P((void));
 */
const FN *
__db_get_seq_flags_fn()
{
	static const FN __db_seq_flags_fn[] = {
		{ 0,	NULL }
	};

	/*
	 * !!!
	 * The Tcl API uses this interface, stub it off.
	 */
	return (__db_seq_flags_fn);
}
#endif /* !HAVE_STATISTICS */
#endif /* HAVE_SEQUENCE */
