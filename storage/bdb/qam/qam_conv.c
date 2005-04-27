/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_conv.c,v 11.14 2002/08/06 06:17:02 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_swap.h"
#include "dbinc/db_am.h"

/*
 * __qam_mswap --
 *	Swap the bytes on the queue metadata page.
 *
 * PUBLIC: int __qam_mswap __P((PAGE *));
 */
int
__qam_mswap(pg)
	PAGE *pg;
{
	u_int8_t *p;

	 __db_metaswap(pg);

	 p = (u_int8_t *)pg + sizeof(DBMETA);

	SWAP32(p);		/* first_recno */
	SWAP32(p);		/* cur_recno */
	SWAP32(p);		/* re_len */
	SWAP32(p);		/* re_pad */
	SWAP32(p);		/* rec_page */
	SWAP32(p);		/* page_ext */
	p += 91 * sizeof(u_int32_t); /* unused */
	SWAP32(p);		/* crypto_magic */

	return (0);
}

/*
 * __qam_pgin_out --
 *	Convert host-specific page layout to/from the host-independent format
 *	stored on disk.
 *  We only need to fix up a few fields in the header
 *
 * PUBLIC: int __qam_pgin_out __P((DB_ENV *, db_pgno_t, void *, DBT *));
 */
int
__qam_pgin_out(dbenv, pg, pp, cookie)
	DB_ENV *dbenv;
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;
	QPAGE *h;

	COMPQUIET(pg, 0);
	COMPQUIET(dbenv, NULL);
	pginfo = (DB_PGINFO *)cookie->data;
	if (!F_ISSET(pginfo, DB_AM_SWAP))
		return (0);

	h = pp;
	if (h->type == P_QAMMETA)
	    return (__qam_mswap(pp));

	M_32_SWAP(h->lsn.file);
	M_32_SWAP(h->lsn.offset);
	M_32_SWAP(h->pgno);

	return (0);
}
