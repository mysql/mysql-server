/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: bt_upgrade.c,v 11.19 2000/11/30 00:58:29 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <limits.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_swap.h"
#include "btree.h"
#include "db_am.h"
#include "db_upgrade.h"

/*
 * __bam_30_btreemeta --
 *	Upgrade the metadata pages from version 6 to version 7.
 *
 * PUBLIC: int __bam_30_btreemeta __P((DB *, char *, u_int8_t *));
 */
int
__bam_30_btreemeta(dbp, real_name, buf)
	DB *dbp;
	char *real_name;
	u_int8_t *buf;
{
	BTMETA30 *newmeta;
	BTMETA2X *oldmeta;
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	newmeta = (BTMETA30 *)buf;
	oldmeta = (BTMETA2X *)buf;

	/*
	 * Move things from the end up, so we do not overwrite things.
	 * We are going to create a new uid, so we can move the stuff
	 * at the end of the structure first, overwriting the uid.
	 */

	newmeta->re_pad = oldmeta->re_pad;
	newmeta->re_len = oldmeta->re_len;
	newmeta->minkey = oldmeta->minkey;
	newmeta->maxkey = oldmeta->maxkey;
	newmeta->dbmeta.free = oldmeta->free;
	newmeta->dbmeta.flags = oldmeta->flags;
	newmeta->dbmeta.type  = P_BTREEMETA;

	newmeta->dbmeta.version = 7;
	/* Replace the unique ID. */
	if ((ret = __os_fileid(dbenv, real_name, 1, buf + 36)) != 0)
		return (ret);

	newmeta->root = 1;

	return (0);
}

/*
 * __bam_31_btreemeta --
 *	Upgrade the database from version 7 to version 8.
 *
 * PUBLIC: int __bam_31_btreemeta
 * PUBLIC:      __P((DB *, char *, u_int32_t, DB_FH *, PAGE *, int *));
 */
int
__bam_31_btreemeta(dbp, real_name, flags, fhp, h, dirtyp)
	DB *dbp;
	char *real_name;
	u_int32_t flags;
	DB_FH *fhp;
	PAGE *h;
	int *dirtyp;
{
	BTMETA31 *newmeta;
	BTMETA30 *oldmeta;

	COMPQUIET(dbp, NULL);
	COMPQUIET(real_name, NULL);
	COMPQUIET(fhp, NULL);

	newmeta = (BTMETA31 *)h;
	oldmeta = (BTMETA30 *)h;

	/*
	 * Copy the effected fields down the page.
	 * The fields may overlap each other so we
	 * start at the bottom and use memmove.
	 */
	newmeta->root = oldmeta->root;
	newmeta->re_pad = oldmeta->re_pad;
	newmeta->re_len = oldmeta->re_len;
	newmeta->minkey = oldmeta->minkey;
	newmeta->maxkey = oldmeta->maxkey;
	memmove(newmeta->dbmeta.uid,
	     oldmeta->dbmeta.uid, sizeof(oldmeta->dbmeta.uid));
	newmeta->dbmeta.flags = oldmeta->dbmeta.flags;
	newmeta->dbmeta.record_count = 0;
	newmeta->dbmeta.key_count = 0;
	ZERO_LSN(newmeta->dbmeta.unused3);

	/* Set the version number. */
	newmeta->dbmeta.version = 8;

	/* Upgrade the flags. */
	if (LF_ISSET(DB_DUPSORT))
		F_SET(&newmeta->dbmeta, BTM_DUPSORT);

	*dirtyp = 1;
	return (0);
}

/*
 * __bam_31_lbtree --
 *      Upgrade the database btree leaf pages.
 *
 * PUBLIC: int __bam_31_lbtree
 * PUBLIC:      __P((DB *, char *, u_int32_t, DB_FH *, PAGE *, int *));
 */
int
__bam_31_lbtree(dbp, real_name, flags, fhp, h, dirtyp)
	DB *dbp;
	char *real_name;
	u_int32_t flags;
	DB_FH *fhp;
	PAGE *h;
	int *dirtyp;
{
	BKEYDATA *bk;
	db_pgno_t pgno;
	db_indx_t indx;
	int ret;

	ret = 0;
	for (indx = O_INDX; indx < NUM_ENT(h); indx += P_INDX) {
		bk = GET_BKEYDATA(h, indx);
		if (B_TYPE(bk->type) == B_DUPLICATE) {
			pgno = GET_BOVERFLOW(h, indx)->pgno;
			if ((ret = __db_31_offdup(dbp, real_name, fhp,
			    LF_ISSET(DB_DUPSORT) ? 1 : 0, &pgno)) != 0)
				break;
			if (pgno != GET_BOVERFLOW(h, indx)->pgno) {
				*dirtyp = 1;
				GET_BOVERFLOW(h, indx)->pgno = pgno;
			}
		}
	}

	return (ret);
}
