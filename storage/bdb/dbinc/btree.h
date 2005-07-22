/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
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
 *
 * $Id: btree.h,v 11.50 2004/07/22 21:52:57 bostic Exp $
 */
#ifndef	_DB_BTREE_H_
#define	_DB_BTREE_H_

/* Forward structure declarations. */
struct __btree;		typedef struct __btree BTREE;
struct __cursor;	typedef struct __cursor BTREE_CURSOR;
struct __epg;		typedef struct __epg EPG;
struct __recno;		typedef struct __recno RECNO;

#define	DEFMINKEYPAGE	 (2)

/*
 * A recno order of 0 indicates that we don't have an order, not that we've
 * an order less than 1.
 */
#define	INVALID_ORDER	0

#define	ISINTERNAL(p)	(TYPE(p) == P_IBTREE || TYPE(p) == P_IRECNO)
#define	ISLEAF(p)	(TYPE(p) == P_LBTREE ||				\
			    TYPE(p) == P_LRECNO || TYPE(p) == P_LDUP)

/* Flags for __bam_cadjust_log(). */
#define	CAD_UPDATEROOT	0x01		/* Root page count was updated. */

/* Flags for __bam_split_log(). */
#define	SPL_NRECS	0x01		/* Split tree has record count. */

/* Flags for __bam_iitem(). */
#define	BI_DELETED	0x01		/* Key/data pair only placeholder. */

/* Flags for __bam_stkrel(). */
#define	STK_CLRDBC	0x01		/* Clear dbc->page reference. */
#define	STK_NOLOCK	0x02		/* Don't retain locks. */

/* Flags for __ram_ca(). These get logged, so make the values explicit. */
typedef enum {
	CA_DELETE = 0,			/* Delete the current record. */
	CA_IAFTER = 1,			/* Insert before the current record. */
	CA_IBEFORE = 2,			/* Insert after the current record. */
	CA_ICURRENT = 3			/* Overwrite the current record. */
} ca_recno_arg;

/*
 * Flags for __bam_search() and __bam_rsearch().
 *
 * Note, internal page searches must find the largest record less than key in
 * the tree so that descents work.  Leaf page searches must find the smallest
 * record greater than key so that the returned index is the record's correct
 * position for insertion.
 *
 * The flags parameter to the search routines describes three aspects of the
 * search: the type of locking required (including if we're locking a pair of
 * pages), the item to return in the presence of duplicates and whether or not
 * to return deleted entries.  To simplify both the mnemonic representation
 * and the code that checks for various cases, we construct a set of bitmasks.
 */
#define	S_READ		0x00001		/* Read locks. */
#define	S_WRITE		0x00002		/* Write locks. */

#define	S_APPEND	0x00040		/* Append to the tree. */
#define	S_DELNO		0x00080		/* Don't return deleted items. */
#define	S_DUPFIRST	0x00100		/* Return first duplicate. */
#define	S_DUPLAST	0x00200		/* Return last duplicate. */
#define	S_EXACT		0x00400		/* Exact items only. */
#define	S_PARENT	0x00800		/* Lock page pair. */
#define	S_STACK		0x01000		/* Need a complete stack. */
#define	S_PAST_EOF	0x02000		/* If doing insert search (or keyfirst
					 * or keylast operations), or a split
					 * on behalf of an insert, it's okay to
					 * return an entry one past end-of-page.
					 */
#define	S_STK_ONLY	0x04000		/* Just return info in the stack */

#define	S_DELETE	(S_WRITE | S_DUPFIRST | S_DELNO | S_EXACT | S_STACK)
#define	S_FIND		(S_READ | S_DUPFIRST | S_DELNO)
#define	S_FIND_WR	(S_WRITE | S_DUPFIRST | S_DELNO)
#define	S_INSERT	(S_WRITE | S_DUPLAST | S_PAST_EOF | S_STACK)
#define	S_KEYFIRST	(S_WRITE | S_DUPFIRST | S_PAST_EOF | S_STACK)
#define	S_KEYLAST	(S_WRITE | S_DUPLAST | S_PAST_EOF | S_STACK)
#define	S_WRPAIR	(S_WRITE | S_DUPLAST | S_PAST_EOF | S_PARENT)

/*
 * Various routines pass around page references.  A page reference is
 * a pointer to the page, and the indx indicates an item on the page.
 * Each page reference may include a lock.
 */
struct __epg {
	PAGE	     *page;		/* The page. */
	db_indx_t     indx;		/* The index on the page. */
	db_indx_t     entries;		/* The number of entries on page */
	DB_LOCK	      lock;		/* The page's lock. */
	db_lockmode_t lock_mode;	/* The lock mode. */
};

/*
 * We maintain a stack of the pages that we're locking in the tree.  Grow
 * the stack as necessary.
 *
 * XXX
 * Temporary fix for #3243 -- clear the page and lock from the stack entry.
 * The correct fix is to never release a stack that doesn't hold items.
 */
#define	BT_STK_CLR(c) do {						\
	(c)->csp = (c)->sp;						\
	(c)->csp->page = NULL;						\
	LOCK_INIT((c)->csp->lock);					\
} while (0)

#define	BT_STK_ENTER(dbenv, c, pagep, page_indx, l, mode, ret) do {	\
	if ((ret = ((c)->csp == (c)->esp ?				\
	    __bam_stkgrow(dbenv, c) : 0)) == 0) {			\
		(c)->csp->page = pagep;					\
		(c)->csp->indx = page_indx;				\
		(c)->csp->entries = NUM_ENT(pagep);			\
		(c)->csp->lock = l;					\
		(c)->csp->lock_mode = mode;				\
	}								\
} while (0)

#define	BT_STK_PUSH(dbenv, c, pagep, page_indx, lock, mode, ret) do {	\
	BT_STK_ENTER(dbenv, c, pagep, page_indx, lock, mode, ret);	\
	++(c)->csp;							\
} while (0)

#define	BT_STK_NUM(dbenv, c, pagep, page_indx, ret) do {		\
	if ((ret =							\
	    (c)->csp == (c)->esp ? __bam_stkgrow(dbenv, c) : 0) == 0) {	\
		(c)->csp->page = NULL;					\
		(c)->csp->indx = page_indx;				\
		(c)->csp->entries = NUM_ENT(pagep);			\
		LOCK_INIT((c)->csp->lock);				\
		(c)->csp->lock_mode = DB_LOCK_NG;			\
	}								\
} while (0)

#define	BT_STK_NUMPUSH(dbenv, c, pagep, page_indx, ret) do {		\
	BT_STK_NUM(dbenv, cp, pagep, page_indx, ret);			\
	++(c)->csp;							\
} while (0)

#define	BT_STK_POP(c)							\
	((c)->csp == (c)->sp ? NULL : --(c)->csp)

/* Btree/Recno cursor. */
struct __cursor {
	/* struct __dbc_internal */
	__DBC_INTERNAL

	/* btree private part */
	EPG		*sp;		/* Stack pointer. */
	EPG		*csp;		/* Current stack entry. */
	EPG		*esp;		/* End stack pointer. */
	EPG		 stack[5];

	db_indx_t	 ovflsize;	/* Maximum key/data on-page size. */

	db_recno_t	 recno;		/* Current record number. */
	u_int32_t	 order;		/* Relative order among deleted curs. */

	/*
	 * Btree:
	 * We set a flag in the cursor structure if the underlying object has
	 * been deleted.  It's not strictly necessary, we could get the same
	 * information by looking at the page itself, but this method doesn't
	 * require us to retrieve the page on cursor delete.
	 *
	 * Recno:
	 * When renumbering recno databases during deletes, cursors referencing
	 * "deleted" records end up positioned between two records, and so must
	 * be specially adjusted on the next operation.
	 */
#define	C_DELETED	0x0001		/* Record was deleted. */
	/*
	 * There are three tree types that require maintaining record numbers.
	 * Recno AM trees, Btree AM trees for which the DB_RECNUM flag was set,
	 * and Btree off-page duplicate trees.
	 */
#define	C_RECNUM	0x0002		/* Tree requires record counts. */
	/*
	 * Recno trees have immutable record numbers by default, but optionally
	 * support mutable record numbers.  Off-page duplicate Recno trees have
	 * mutable record numbers.  All Btrees with record numbers (including
	 * off-page duplicate trees) are mutable by design, no flag is needed.
	 */
#define	C_RENUMBER	0x0004		/* Tree records are mutable. */
	u_int32_t	 flags;
};

/*
 * Threshhold value, as a function of bt_minkey, of the number of
 * bytes a key/data pair can use before being placed on an overflow
 * page.  Assume every item requires the maximum alignment for
 * padding, out of sheer paranoia.
 */
#define	B_MINKEY_TO_OVFLSIZE(dbp, minkey, pgsize)			\
	((u_int16_t)(((pgsize) - P_OVERHEAD(dbp)) / ((minkey) * P_INDX) -\
	    (BKEYDATA_PSIZE(0) + DB_ALIGN(1, sizeof(int32_t)))))

/*
 * The maximum space that a single item can ever take up on one page.
 * Used by __bam_split to determine whether a split is still necessary.
 */
#define	B_MAX(a,b)	(((a) > (b)) ? (a) : (b))
#define	B_MAXSIZEONPAGE(ovflsize)					\
	(B_MAX(BOVERFLOW_PSIZE, BKEYDATA_PSIZE(ovflsize)))

/*
 * The in-memory, per-tree btree/recno data structure.
 */
struct __btree {			/* Btree access method. */
	/*
	 * !!!
	 * These fields are write-once (when the structure is created) and
	 * so are ignored as far as multi-threading is concerned.
	 */
	db_pgno_t bt_meta;		/* Database meta-data page. */
	db_pgno_t bt_root;		/* Database root page. */

	u_int32_t bt_maxkey;		/* Maximum keys per page. */
	u_int32_t bt_minkey;		/* Minimum keys per page. */

					/* Btree comparison function. */
	int (*bt_compare) __P((DB *, const DBT *, const DBT *));
					/* Btree prefix function. */
	size_t (*bt_prefix) __P((DB *, const DBT *, const DBT *));

					/* Recno access method. */
	int	  re_pad;		/* Fixed-length padding byte. */
	int	  re_delim;		/* Variable-length delimiting byte. */
	u_int32_t re_len;		/* Length for fixed-length records. */
	char	 *re_source;		/* Source file name. */

	/*
	 * !!!
	 * The bt_lpgno field is NOT protected by any mutex, and for this
	 * reason must be advisory only, so, while it is read/written by
	 * multiple threads, DB is completely indifferent to the quality
	 * of its information.
	 */
	db_pgno_t bt_lpgno;		/* Last insert location. */
	DB_LSN	  bt_llsn;		/* Last insert LSN. */

	/*
	 * !!!
	 * The re_modified field is NOT protected by any mutex, and for this
	 * reason cannot be anything more complicated than a zero/non-zero
	 * value.  The actual writing of the backing source file cannot be
	 * threaded, so clearing the flag isn't a problem.
	 */
	int	  re_modified;		/* If the tree was modified. */

	/*
	 * !!!
	 * These fields are ignored as far as multi-threading is concerned.
	 * There are no transaction semantics associated with backing files,
	 * nor is there any thread protection.
	 */
	FILE		*re_fp;		/* Source file handle. */
	int		 re_eof;	/* Backing source file EOF reached. */
	db_recno_t	 re_last;	/* Last record number read. */
};

/*
 * Modes for the __bam_curadj recovery records (btree_curadj).
 * These appear in log records, so we wire the values and
 * do not leave it up to the compiler.
 */
typedef enum {
	DB_CA_DI	= 1,
	DB_CA_DUP	= 2,
	DB_CA_RSPLIT	= 3,
	DB_CA_SPLIT	= 4
} db_ca_mode;

#include "dbinc_auto/btree_auto.h"
#include "dbinc_auto/btree_ext.h"
#include "dbinc/db_am.h"
#endif /* !_DB_BTREE_H_ */
