/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: hash_conv.c,v 11.5 2000/03/31 00:30:32 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_swap.h"
#include "hash.h"

/*
 * __ham_pgin --
 *	Convert host-specific page layout from the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __ham_pgin __P((DB_ENV *, db_pgno_t, void *, DBT *));
 */
int
__ham_pgin(dbenv, pg, pp, cookie)
	DB_ENV *dbenv;
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;
	PAGE *h;

	h = pp;
	pginfo = (DB_PGINFO *)cookie->data;

	/*
	 * The hash access method does blind reads of pages, causing them
	 * to be created.  If the type field isn't set it's one of them,
	 * initialize the rest of the page and return.
	 */
	if (h->type != P_HASHMETA && h->pgno == PGNO_INVALID) {
		P_INIT(pp, pginfo->db_pagesize,
		    pg, PGNO_INVALID, PGNO_INVALID, 0, P_HASH);
		return (0);
	}

	if (!pginfo->needswap)
		return (0);

	return (h->type == P_HASHMETA ?  __ham_mswap(pp) :
	    __db_byteswap(dbenv, pg, pp, pginfo->db_pagesize, 1));
}

/*
 * __ham_pgout --
 *	Convert host-specific page layout to the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __ham_pgout __P((DB_ENV *, db_pgno_t, void *, DBT *));
 */
int
__ham_pgout(dbenv, pg, pp, cookie)
	DB_ENV *dbenv;
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;
	PAGE *h;

	pginfo = (DB_PGINFO *)cookie->data;
	if (!pginfo->needswap)
		return (0);

	h = pp;
	return (h->type == P_HASHMETA ?  __ham_mswap(pp) :
	     __db_byteswap(dbenv, pg, pp, pginfo->db_pagesize, 0));
}

/*
 * __ham_mswap --
 *	Swap the bytes on the hash metadata page.
 *
 * PUBLIC: int __ham_mswap __P((void *));
 */
int
__ham_mswap(pg)
	void *pg;
{
	u_int8_t *p;
	int i;

	__db_metaswap(pg);

	p = (u_int8_t *)pg + sizeof(DBMETA);

	SWAP32(p);		/* max_bucket */
	SWAP32(p);		/* high_mask */
	SWAP32(p);		/* low_mask */
	SWAP32(p);		/* ffactor */
	SWAP32(p);		/* nelem */
	SWAP32(p);		/* h_charkey */
	for (i = 0; i < NCACHED; ++i)
		SWAP32(p);	/* spares */
	return (0);
}
