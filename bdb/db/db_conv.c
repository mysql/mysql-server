/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
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
static const char revid[] = "$Id: db_conv.c,v 11.11 2000/11/30 00:58:31 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_swap.h"
#include "db_am.h"
#include "btree.h"
#include "hash.h"
#include "qam.h"

/*
 * __db_pgin --
 *	Primary page-swap routine.
 *
 * PUBLIC: int __db_pgin __P((DB_ENV *, db_pgno_t, void *, DBT *));
 */
int
__db_pgin(dbenv, pg, pp, cookie)
	DB_ENV *dbenv;
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;

	pginfo = (DB_PGINFO *)cookie->data;

	switch (((PAGE *)pp)->type) {
	case P_HASH:
	case P_HASHMETA:
	case P_INVALID:
		return (__ham_pgin(dbenv, pg, pp, cookie));
	case P_BTREEMETA:
	case P_IBTREE:
	case P_IRECNO:
	case P_LBTREE:
	case P_LDUP:
	case P_LRECNO:
	case P_OVERFLOW:
		return (__bam_pgin(dbenv, pg, pp, cookie));
	case P_QAMMETA:
	case P_QAMDATA:
		return (__qam_pgin_out(dbenv, pg, pp, cookie));
	default:
		break;
	}
	return (__db_unknown_type(dbenv, "__db_pgin", ((PAGE *)pp)->type));
}

/*
 * __db_pgout --
 *	Primary page-swap routine.
 *
 * PUBLIC: int __db_pgout __P((DB_ENV *, db_pgno_t, void *, DBT *));
 */
int
__db_pgout(dbenv, pg, pp, cookie)
	DB_ENV *dbenv;
	db_pgno_t pg;
	void *pp;
	DBT *cookie;
{
	DB_PGINFO *pginfo;

	pginfo = (DB_PGINFO *)cookie->data;

	switch (((PAGE *)pp)->type) {
	case P_HASH:
	case P_HASHMETA:
	case P_INVALID:
		return (__ham_pgout(dbenv, pg, pp, cookie));
	case P_BTREEMETA:
	case P_IBTREE:
	case P_IRECNO:
	case P_LBTREE:
	case P_LDUP:
	case P_LRECNO:
	case P_OVERFLOW:
		return (__bam_pgout(dbenv, pg, pp, cookie));
	case P_QAMMETA:
	case P_QAMDATA:
		return (__qam_pgin_out(dbenv, pg, pp, cookie));
	default:
		break;
	}
	return (__db_unknown_type(dbenv, "__db_pgout", ((PAGE *)pp)->type));
}

/*
 * __db_metaswap --
 *	Byteswap the common part of the meta-data page.
 *
 * PUBLIC: void __db_metaswap __P((PAGE *));
 */
void
__db_metaswap(pg)
	PAGE *pg;
{
	u_int8_t *p;

	p = (u_int8_t *)pg;

	/* Swap the meta-data information. */
	SWAP32(p);	/* lsn.file */
	SWAP32(p);	/* lsn.offset */
	SWAP32(p);	/* pgno */
	SWAP32(p);	/* magic */
	SWAP32(p);	/* version */
	SWAP32(p);	/* pagesize */
	p += 4;		/* unused, page type, unused, unused */
	SWAP32(p);	/* free */
	SWAP32(p);	/* alloc_lsn part 1 */
	SWAP32(p);	/* alloc_lsn part 2 */
	SWAP32(p);	/* cached key count */
	SWAP32(p);	/* cached record count */
	SWAP32(p);	/* flags */
}

/*
 * __db_byteswap --
 *	Byteswap a page.
 *
 * PUBLIC: int __db_byteswap __P((DB_ENV *, db_pgno_t, PAGE *, size_t, int));
 */
int
__db_byteswap(dbenv, pg, h, pagesize, pgin)
	DB_ENV *dbenv;
	db_pgno_t pg;
	PAGE *h;
	size_t pagesize;
	int pgin;
{
	BINTERNAL *bi;
	BKEYDATA *bk;
	BOVERFLOW *bo;
	RINTERNAL *ri;
	db_indx_t i, len, tmp;
	u_int8_t *p, *end;

	COMPQUIET(pg, 0);

	if (pgin) {
		M_32_SWAP(h->lsn.file);
		M_32_SWAP(h->lsn.offset);
		M_32_SWAP(h->pgno);
		M_32_SWAP(h->prev_pgno);
		M_32_SWAP(h->next_pgno);
		M_16_SWAP(h->entries);
		M_16_SWAP(h->hf_offset);
	}

	switch (h->type) {
	case P_HASH:
		for (i = 0; i < NUM_ENT(h); i++) {
			if (pgin)
				M_16_SWAP(h->inp[i]);

			switch (HPAGE_TYPE(h, i)) {
			case H_KEYDATA:
				break;
			case H_DUPLICATE:
				len = LEN_HKEYDATA(h, pagesize, i);
				p = HKEYDATA_DATA(P_ENTRY(h, i));
				for (end = p + len; p < end;) {
					if (pgin) {
						P_16_SWAP(p);
						memcpy(&tmp,
						    p, sizeof(db_indx_t));
						p += sizeof(db_indx_t);
					} else {
						memcpy(&tmp,
						    p, sizeof(db_indx_t));
						SWAP16(p);
					}
					p += tmp;
					SWAP16(p);
				}
				break;
			case H_OFFDUP:
				p = HOFFPAGE_PGNO(P_ENTRY(h, i));
				SWAP32(p);			/* pgno */
				break;
			case H_OFFPAGE:
				p = HOFFPAGE_PGNO(P_ENTRY(h, i));
				SWAP32(p);			/* pgno */
				SWAP32(p);			/* tlen */
				break;
			}

		}

		/*
		 * The offsets in the inp array are used to determine
		 * the size of entries on a page; therefore they
		 * cannot be converted until we've done all the
		 * entries.
		 */
		if (!pgin)
			for (i = 0; i < NUM_ENT(h); i++)
				M_16_SWAP(h->inp[i]);
		break;
	case P_LBTREE:
	case P_LDUP:
	case P_LRECNO:
		for (i = 0; i < NUM_ENT(h); i++) {
			if (pgin)
				M_16_SWAP(h->inp[i]);

			/*
			 * In the case of on-page duplicates, key information
			 * should only be swapped once.
			 */
			if (h->type == P_LBTREE && i > 1) {
				if (pgin) {
					if (h->inp[i] == h->inp[i - 2])
						continue;
				} else {
					M_16_SWAP(h->inp[i]);
					if (h->inp[i] == h->inp[i - 2])
						continue;
					M_16_SWAP(h->inp[i]);
				}
			}

			bk = GET_BKEYDATA(h, i);
			switch (B_TYPE(bk->type)) {
			case B_KEYDATA:
				M_16_SWAP(bk->len);
				break;
			case B_DUPLICATE:
			case B_OVERFLOW:
				bo = (BOVERFLOW *)bk;
				M_32_SWAP(bo->pgno);
				M_32_SWAP(bo->tlen);
				break;
			}

			if (!pgin)
				M_16_SWAP(h->inp[i]);
		}
		break;
	case P_IBTREE:
		for (i = 0; i < NUM_ENT(h); i++) {
			if (pgin)
				M_16_SWAP(h->inp[i]);

			bi = GET_BINTERNAL(h, i);
			M_16_SWAP(bi->len);
			M_32_SWAP(bi->pgno);
			M_32_SWAP(bi->nrecs);

			switch (B_TYPE(bi->type)) {
			case B_KEYDATA:
				break;
			case B_DUPLICATE:
			case B_OVERFLOW:
				bo = (BOVERFLOW *)bi->data;
				M_32_SWAP(bo->pgno);
				M_32_SWAP(bo->tlen);
				break;
			}

			if (!pgin)
				M_16_SWAP(h->inp[i]);
		}
		break;
	case P_IRECNO:
		for (i = 0; i < NUM_ENT(h); i++) {
			if (pgin)
				M_16_SWAP(h->inp[i]);

			ri = GET_RINTERNAL(h, i);
			M_32_SWAP(ri->pgno);
			M_32_SWAP(ri->nrecs);

			if (!pgin)
				M_16_SWAP(h->inp[i]);
		}
		break;
	case P_OVERFLOW:
	case P_INVALID:
		/* Nothing to do. */
		break;
	default:
		return (__db_unknown_type(dbenv, "__db_byteswap", h->type));
	}

	if (!pgin) {
		/* Swap the header information. */
		M_32_SWAP(h->lsn.file);
		M_32_SWAP(h->lsn.offset);
		M_32_SWAP(h->pgno);
		M_32_SWAP(h->prev_pgno);
		M_32_SWAP(h->next_pgno);
		M_16_SWAP(h->entries);
		M_16_SWAP(h->hf_offset);
	}
	return (0);
}
