/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_reclaim.c,v 11.5 2000/04/07 14:26:58 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_am.h"

/*
 * Assume that we enter with a valid pgno.  We traverse a set of
 * duplicate pages.  The format of the callback routine is:
 * callback(dbp, page, cookie, did_put).  did_put is an output
 * value that will be set to 1 by the callback routine if it
 * already put the page back.  Otherwise, this routine must
 * put the page.
 *
 * PUBLIC: int __db_traverse_dup __P((DB *,
 * PUBLIC:    db_pgno_t, int (*)(DB *, PAGE *, void *, int *), void *));
 */
int
__db_traverse_dup(dbp, pgno, callback, cookie)
	DB *dbp;
	db_pgno_t pgno;
	int (*callback) __P((DB *, PAGE *, void *, int *));
	void *cookie;
{
	PAGE *p;
	int did_put, i, opgno, ret;

	do {
		did_put = 0;
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &p)) != 0)
			return (ret);
		pgno = NEXT_PGNO(p);

		for (i = 0; i < NUM_ENT(p); i++) {
			if (B_TYPE(GET_BKEYDATA(p, i)->type) == B_OVERFLOW) {
				opgno = GET_BOVERFLOW(p, i)->pgno;
				if ((ret = __db_traverse_big(dbp,
				    opgno, callback, cookie)) != 0)
					goto err;
			}
		}

		if ((ret = callback(dbp, p, cookie, &did_put)) != 0)
			goto err;

		if (!did_put)
			if ((ret = memp_fput(dbp->mpf, p, 0)) != 0)
				return (ret);
	} while (pgno != PGNO_INVALID);

	if (0) {
err:		if (did_put == 0)
			(void)memp_fput(dbp->mpf, p, 0);
	}
	return (ret);
}

/*
 * __db_traverse_big
 *	Traverse a chain of overflow pages and call the callback routine
 * on each one.  The calling convention for the callback is:
 *	callback(dbp, page, cookie, did_put),
 * where did_put is a return value indicating if the page in question has
 * already been returned to the mpool.
 *
 * PUBLIC: int __db_traverse_big __P((DB *,
 * PUBLIC:     db_pgno_t, int (*)(DB *, PAGE *, void *, int *), void *));
 */
int
__db_traverse_big(dbp, pgno, callback, cookie)
	DB *dbp;
	db_pgno_t pgno;
	int (*callback) __P((DB *, PAGE *, void *, int *));
	void *cookie;
{
	PAGE *p;
	int did_put, ret;

	do {
		did_put = 0;
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &p)) != 0)
			return (ret);
		pgno = NEXT_PGNO(p);
		if ((ret = callback(dbp, p, cookie, &did_put)) == 0 &&
		    !did_put)
			ret = memp_fput(dbp->mpf, p, 0);
	} while (ret == 0 && pgno != PGNO_INVALID);

	return (ret);
}

/*
 * __db_reclaim_callback
 * This is the callback routine used during a delete of a subdatabase.
 * we are traversing a btree or hash table and trying to free all the
 * pages.  Since they share common code for duplicates and overflow
 * items, we traverse them identically and use this routine to do the
 * actual free.  The reason that this is callback is because hash uses
 * the same traversal code for statistics gathering.
 *
 * PUBLIC: int __db_reclaim_callback __P((DB *, PAGE *, void *, int *));
 */
int
__db_reclaim_callback(dbp, p, cookie, putp)
	DB *dbp;
	PAGE *p;
	void *cookie;
	int *putp;
{
	int ret;

	COMPQUIET(dbp, NULL);

	if ((ret = __db_free(cookie, p)) != 0)
		return (ret);
	*putp = 1;

	return (0);
}
