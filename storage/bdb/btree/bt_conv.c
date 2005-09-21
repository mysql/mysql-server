/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: bt_conv.c,v 11.15 2004/01/28 03:35:48 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_swap.h"
#include "dbinc/btree.h"

/*
 * __bam_pgin --
 *	Convert host-specific page layout from the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __bam_pgin __P((DB_ENV *, DB *, db_pgno_t, void *, DBT *));
 */
int
__bam_pgin(dbenv, dummydbp, pg, pp, cookie)
	DB_ENV *dbenv;
	DB *dummydbp;
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;
	PAGE *h;

	pginfo = (DB_PGINFO *)cookie->data;
	if (!F_ISSET(pginfo, DB_AM_SWAP))
		return (0);

	h = pp;
	return (TYPE(h) == P_BTREEMETA ?  __bam_mswap(pp) :
	    __db_byteswap(dbenv, dummydbp, pg, pp, pginfo->db_pagesize, 1));
}

/*
 * __bam_pgout --
 *	Convert host-specific page layout to the host-independent format
 *	stored on disk.
 *
 * PUBLIC: int __bam_pgout __P((DB_ENV *, DB *, db_pgno_t, void *, DBT *));
 */
int
__bam_pgout(dbenv, dummydbp, pg, pp, cookie)
	DB_ENV *dbenv;
	DB *dummydbp;
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;
	PAGE *h;

	pginfo = (DB_PGINFO *)cookie->data;
	if (!F_ISSET(pginfo, DB_AM_SWAP))
		return (0);

	h = pp;
	return (TYPE(h) == P_BTREEMETA ?  __bam_mswap(pp) :
	    __db_byteswap(dbenv, dummydbp, pg, pp, pginfo->db_pagesize, 0));
}

/*
 * __bam_mswap --
 *	Swap the bytes on the btree metadata page.
 *
 * PUBLIC: int __bam_mswap __P((PAGE *));
 */
int
__bam_mswap(pg)
	PAGE *pg;
{
	u_int8_t *p;

	__db_metaswap(pg);

	p = (u_int8_t *)pg + sizeof(DBMETA);

	SWAP32(p);		/* maxkey */
	SWAP32(p);		/* minkey */
	SWAP32(p);		/* re_len */
	SWAP32(p);		/* re_pad */
	SWAP32(p);		/* root */
	p += 92 * sizeof(u_int32_t); /* unused */
	SWAP32(p);		/* crypto_magic */

	return (0);
}
