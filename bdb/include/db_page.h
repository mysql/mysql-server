/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_page.h,v 11.28 2000/12/06 19:55:45 ubell Exp $
 */

#ifndef _DB_PAGE_H_
#define	_DB_PAGE_H_

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * DB page formats.
 *
 * !!!
 * This implementation requires that values within the following structures
 * NOT be padded -- note, ANSI C permits random padding within structures.
 * If your compiler pads randomly you can just forget ever making DB run on
 * your system.  In addition, no data type can require larger alignment than
 * its own size, e.g., a 4-byte data element may not require 8-byte alignment.
 *
 * Note that key/data lengths are often stored in db_indx_t's -- this is
 * not accidental, nor does it limit the key/data size.  If the key/data
 * item fits on a page, it's guaranteed to be small enough to fit into a
 * db_indx_t, and storing it in one saves space.
 */

#define	PGNO_INVALID	0	/* Invalid page number in any database. */
#define	PGNO_BASE_MD	0	/* Base database: metadata page number. */

/* Page types. */
#define	P_INVALID	0	/* Invalid page type. */
#define	__P_DUPLICATE	1	/* Duplicate. DEPRECATED in 3.1 */
#define	P_HASH		2	/* Hash. */
#define	P_IBTREE	3	/* Btree internal. */
#define	P_IRECNO	4	/* Recno internal. */
#define	P_LBTREE	5	/* Btree leaf. */
#define	P_LRECNO	6	/* Recno leaf. */
#define	P_OVERFLOW	7	/* Overflow. */
#define	P_HASHMETA	8	/* Hash metadata page. */
#define	P_BTREEMETA	9	/* Btree metadata page. */
#define	P_QAMMETA	10	/* Queue metadata page. */
#define	P_QAMDATA	11	/* Queue data page. */
#define	P_LDUP		12	/* Off-page duplicate leaf. */
#define	P_PAGETYPE_MAX	13

/*
 * When we create pages in mpool, we ask mpool to clear some number of bytes
 * in the header.  This number must be at least as big as the regular page
 * headers and cover enough of the btree and hash meta-data pages to obliterate
 * the page type.
 */
#define	DB_PAGE_DB_LEN		32
#define	DB_PAGE_QUEUE_LEN	0

/************************************************************************
 GENERIC METADATA PAGE HEADER
 *
 * !!!
 * The magic and version numbers have to be in the same place in all versions
 * of the metadata page as the application may not have upgraded the database.
 ************************************************************************/
typedef struct _dbmeta31 {
	DB_LSN	  lsn;		/* 00-07: LSN. */
	db_pgno_t pgno;		/* 08-11: Current page number. */
	u_int32_t magic;	/* 12-15: Magic number. */
	u_int32_t version;	/* 16-19: Version. */
	u_int32_t pagesize;	/* 20-23: Pagesize. */
	u_int8_t  unused1[1];	/*    24: Unused. */
	u_int8_t  type;		/*    25: Page type. */
	u_int8_t  unused2[2];	/* 26-27: Unused. */
	u_int32_t free;		/* 28-31: Free list page number. */
	DB_LSN	  unused3;	/* 32-39: former Lsn for allocation */
	u_int32_t key_count;	/* 40-43: Cached key count. */
	u_int32_t record_count;	/* 44-47: Cached record count. */
	u_int32_t flags;	/* 48-51: Flags: unique to each AM. */
				/* 52-71: Unique file ID. */
	u_int8_t  uid[DB_FILE_ID_LEN];
} DBMETA31, DBMETA;

/************************************************************************
 BTREE METADATA PAGE LAYOUT
 ************************************************************************/
typedef struct _btmeta31 {
#define	BTM_DUP		0x001	/*	  Duplicates. */
#define	BTM_RECNO	0x002	/*	  Recno tree. */
#define	BTM_RECNUM	0x004	/*	  Btree: maintain record count. */
#define	BTM_FIXEDLEN	0x008	/*	  Recno: fixed length records. */
#define	BTM_RENUMBER	0x010	/*	  Recno: renumber on insert/delete. */
#define	BTM_SUBDB	0x020	/*	  Subdatabases. */
#define	BTM_DUPSORT	0x040	/*	  Duplicates are sorted. */
#define	BTM_MASK	0x07f
	DBMETA	dbmeta;		/* 00-71: Generic meta-data header. */

	u_int32_t maxkey;	/* 72-75: Btree: Maxkey. */
	u_int32_t minkey;	/* 76-79: Btree: Minkey. */
	u_int32_t re_len;	/* 80-83: Recno: fixed-length record length. */
	u_int32_t re_pad;	/* 84-87: Recno: fixed-length record pad. */
	u_int32_t root;		/* 88-92: Root page. */

	/*
	 * Minimum page size is 128.
	 */
} BTMETA31, BTMETA;

/************************************************************************
 HASH METADATA PAGE LAYOUT
 ************************************************************************/
typedef struct _hashmeta31 {
#define	DB_HASH_DUP	0x01	/*	  Duplicates. */
#define	DB_HASH_SUBDB	0x02	/*	  Subdatabases. */
#define	DB_HASH_DUPSORT	0x04	/*	  Duplicates are sorted. */
	DBMETA dbmeta;		/* 00-71: Generic meta-data page header. */

	u_int32_t max_bucket;	/* 72-75: ID of Maximum bucket in use */
	u_int32_t high_mask;	/* 76-79: Modulo mask into table */
	u_int32_t low_mask;	/* 80-83: Modulo mask into table lower half */
	u_int32_t ffactor;	/* 84-87: Fill factor */
	u_int32_t nelem;	/* 88-91: Number of keys in hash table */
	u_int32_t h_charkey;	/* 92-95: Value of hash(CHARKEY) */
#define	NCACHED	32		/* number of spare points */
				/* 96-223: Spare pages for overflow */
	u_int32_t spares[NCACHED];

	/*
	 * Minimum page size is 256.
	 */
} HMETA31, HMETA;

/************************************************************************
 QUEUE METADATA PAGE LAYOUT
 ************************************************************************/
/*
 * QAM Meta data page structure
 *
 */
typedef struct _qmeta32 {
	DBMETA    dbmeta;	/* 00-71: Generic meta-data header. */

	u_int32_t first_recno;	/* 72-75: First not deleted record. */
	u_int32_t cur_recno;	/* 76-79: Last recno allocated. */
	u_int32_t re_len;	/* 80-83: Fixed-length record length. */
	u_int32_t re_pad;	/* 84-87: Fixed-length record pad. */
	u_int32_t rec_page;	/* 88-91: Records Per Page. */
	u_int32_t page_ext;	/* 92-95: Pages per extent */

	/*
	 * Minimum page size is 128.
	 */
} QMETA32, QMETA;

/*
 * DBMETASIZE is a constant used by __db_file_setup and DB->verify
 * as a buffer which is guaranteed to be larger than any possible
 * metadata page size and smaller than any disk sector.
 */
#define	DBMETASIZE	256

/************************************************************************
 BTREE/HASH MAIN PAGE LAYOUT
 ************************************************************************/
/*
 *	+-----------------------------------+
 *	|    lsn    |   pgno    | prev pgno |
 *	+-----------------------------------+
 *	| next pgno |  entries  | hf offset |
 *	+-----------------------------------+
 *	|   level   |   type    |   index   |
 *	+-----------------------------------+
 *	|   index   | free -->              |
 *	+-----------+-----------------------+
 *	|	 F R E E A R E A            |
 *	+-----------------------------------+
 *	|              <-- free |   item    |
 *	+-----------------------------------+
 *	|   item    |   item    |   item    |
 *	+-----------------------------------+
 *
 * sizeof(PAGE) == 26 bytes, and the following indices are guaranteed to be
 * two-byte aligned.
 *
 * For hash and btree leaf pages, index items are paired, e.g., inp[0] is the
 * key for inp[1]'s data.  All other types of pages only contain single items.
 */
typedef struct _db_page {
	DB_LSN	  lsn;		/* 00-07: Log sequence number. */
	db_pgno_t pgno;		/* 08-11: Current page number. */
	db_pgno_t prev_pgno;	/* 12-15: Previous page number. */
	db_pgno_t next_pgno;	/* 16-19: Next page number. */
	db_indx_t entries;	/* 20-21: Number of items on the page. */
	db_indx_t hf_offset;	/* 22-23: High free byte page offset. */

	/*
	 * The btree levels are numbered from the leaf to the root, starting
	 * with 1, so the leaf is level 1, its parent is level 2, and so on.
	 * We maintain this level on all btree pages, but the only place that
	 * we actually need it is on the root page.  It would not be difficult
	 * to hide the byte on the root page once it becomes an internal page,
	 * so we could get this byte back if we needed it for something else.
	 */
#define	LEAFLEVEL	  1
#define	MAXBTREELEVEL	255
	u_int8_t  level;	/*    24: Btree tree level. */
	u_int8_t  type;		/*    25: Page type. */
	db_indx_t inp[1];	/* Variable length index of items. */
} PAGE;

/* PAGE element macros. */
#define	LSN(p)		(((PAGE *)p)->lsn)
#define	PGNO(p)		(((PAGE *)p)->pgno)
#define	PREV_PGNO(p)	(((PAGE *)p)->prev_pgno)
#define	NEXT_PGNO(p)	(((PAGE *)p)->next_pgno)
#define	NUM_ENT(p)	(((PAGE *)p)->entries)
#define	HOFFSET(p)	(((PAGE *)p)->hf_offset)
#define	LEVEL(p)	(((PAGE *)p)->level)
#define	TYPE(p)		(((PAGE *)p)->type)

/************************************************************************
 QUEUE MAIN PAGE LAYOUT
 ************************************************************************/
typedef struct _qpage {
	DB_LSN	  lsn;		/* 00-07: Log sequence number. */
	db_pgno_t pgno;		/* 08-11: Current page number. */
	u_int32_t unused0[3];	/* 12-23: Unused. */
	u_int8_t  unused1[1];	/*    24: Unused. */
	u_int8_t  type;		/*    25: Page type. */
	u_int8_t  unused2[2];	/* 26-27: Unused. */
} QPAGE;

/*
 * !!!
 * The next_pgno and prev_pgno fields are not maintained for btree and recno
 * internal pages.  Doing so only provides a minor performance improvement,
 * it's hard to do when deleting internal pages, and it increases the chance
 * of deadlock during deletes and splits because we have to re-link pages at
 * more than the leaf level.
 *
 * !!!
 * The btree/recno access method needs db_recno_t bytes of space on the root
 * page to specify how many records are stored in the tree.  (The alternative
 * is to store the number of records in the meta-data page, which will create
 * a second hot spot in trees being actively modified, or recalculate it from
 * the BINTERNAL fields on each access.)  Overload the PREV_PGNO field.
 */
#define	RE_NREC(p)							\
	((TYPE(p) == P_IBTREE || TYPE(p) == P_IRECNO) ?			\
	PREV_PGNO(p) : (TYPE(p) == P_LBTREE ? NUM_ENT(p) / 2 : NUM_ENT(p)))
#define	RE_NREC_ADJ(p, adj)						\
	PREV_PGNO(p) += adj;
#define	RE_NREC_SET(p, num)						\
	PREV_PGNO(p) = num;

/*
 * Initialize a page.
 *
 * !!!
 * Don't modify the page's LSN, code depends on it being unchanged after a
 * P_INIT call.
 */
#define	P_INIT(pg, pg_size, n, pg_prev, pg_next, btl, pg_type) do {	\
	PGNO(pg) = n;							\
	PREV_PGNO(pg) = pg_prev;					\
	NEXT_PGNO(pg) = pg_next;					\
	NUM_ENT(pg) = 0;						\
	HOFFSET(pg) = pg_size;						\
	LEVEL(pg) = btl;						\
	TYPE(pg) = pg_type;						\
} while (0)

/* Page header length (offset to first index). */
#define	P_OVERHEAD		(SSZA(PAGE, inp))

/* First free byte. */
#define	LOFFSET(pg)		(P_OVERHEAD + NUM_ENT(pg) * sizeof(db_indx_t))

/* Free space on a regular page. */
#define	P_FREESPACE(pg)		(HOFFSET(pg) - LOFFSET(pg))

/* Get a pointer to the bytes at a specific index. */
#define	P_ENTRY(pg, indx)	((u_int8_t *)pg + ((PAGE *)pg)->inp[indx])

/************************************************************************
 OVERFLOW PAGE LAYOUT
 ************************************************************************/

/*
 * Overflow items are referenced by HOFFPAGE and BOVERFLOW structures, which
 * store a page number (the first page of the overflow item) and a length
 * (the total length of the overflow item).  The overflow item consists of
 * some number of overflow pages, linked by the next_pgno field of the page.
 * A next_pgno field of PGNO_INVALID flags the end of the overflow item.
 *
 * Overflow page overloads:
 *	The amount of overflow data stored on each page is stored in the
 *	hf_offset field.
 *
 *	The implementation reference counts overflow items as it's possible
 *	for them to be promoted onto btree internal pages.  The reference
 *	count is stored in the entries field.
 */
#define	OV_LEN(p)	(((PAGE *)p)->hf_offset)
#define	OV_REF(p)	(((PAGE *)p)->entries)

/* Maximum number of bytes that you can put on an overflow page. */
#define	P_MAXSPACE(psize)	((psize) - P_OVERHEAD)

/* Free space on an overflow page. */
#define	P_OVFLSPACE(psize, pg)		(P_MAXSPACE(psize) - HOFFSET(pg))

/************************************************************************
 HASH PAGE LAYOUT
 ************************************************************************/

/* Each index references a group of bytes on the page. */
#define	H_KEYDATA	1	/* Key/data item. */
#define	H_DUPLICATE	2	/* Duplicate key/data item. */
#define	H_OFFPAGE	3	/* Overflow key/data item. */
#define	H_OFFDUP	4	/* Overflow page of duplicates. */

/*
 * !!!
 * Items on hash pages are (potentially) unaligned, so we can never cast the
 * (page + offset) pointer to an HKEYDATA, HOFFPAGE or HOFFDUP structure, as
 * we do with B+tree on-page structures.  Because we frequently want the type
 * field, it requires no alignment, and it's in the same location in all three
 * structures, there's a pair of macros.
 */
#define	HPAGE_PTYPE(p)		(*(u_int8_t *)p)
#define	HPAGE_TYPE(pg, indx)	(*P_ENTRY(pg, indx))

/*
 * The first and second types are H_KEYDATA and H_DUPLICATE, represented
 * by the HKEYDATA structure:
 *
 *	+-----------------------------------+
 *	|    type   | key/data ...          |
 *	+-----------------------------------+
 *
 * For duplicates, the data field encodes duplicate elements in the data
 * field:
 *
 *	+---------------------------------------------------------------+
 *	|    type   | len1 | element1 | len1 | len2 | element2 | len2   |
 *	+---------------------------------------------------------------+
 *
 * Thus, by keeping track of the offset in the element, we can do both
 * backward and forward traversal.
 */
typedef struct _hkeydata {
	u_int8_t  type;		/*    00: Page type. */
	u_int8_t  data[1];	/* Variable length key/data item. */
} HKEYDATA;
#define	HKEYDATA_DATA(p)	(((u_int8_t *)p) + SSZA(HKEYDATA, data))

/*
 * The length of any HKEYDATA item. Note that indx is an element index,
 * not a PAIR index.
 */
#define	LEN_HITEM(pg, pgsize, indx)					\
	(((indx) == 0 ? pgsize :					\
	((PAGE *)(pg))->inp[indx - 1]) - ((PAGE *)(pg))->inp[indx])

#define	LEN_HKEYDATA(pg, psize, indx)					\
	(LEN_HITEM(pg, psize, indx) - HKEYDATA_SIZE(0))

/*
 * Page space required to add a new HKEYDATA item to the page, with and
 * without the index value.
 */
#define	HKEYDATA_SIZE(len)						\
	((len) + SSZA(HKEYDATA, data))
#define	HKEYDATA_PSIZE(len)						\
	(HKEYDATA_SIZE(len) + sizeof(db_indx_t))

/* Put a HKEYDATA item at the location referenced by a page entry. */
#define	PUT_HKEYDATA(pe, kd, len, type) {				\
	((HKEYDATA *)pe)->type = type;					\
	memcpy((u_int8_t *)pe + sizeof(u_int8_t), kd, len);		\
}

/*
 * Macros the describe the page layout in terms of key-data pairs.
 */
#define	H_NUMPAIRS(pg)			(NUM_ENT(pg) / 2)
#define	H_KEYINDEX(indx)		(indx)
#define	H_DATAINDEX(indx)		((indx) + 1)
#define	H_PAIRKEY(pg, indx)		P_ENTRY(pg, H_KEYINDEX(indx))
#define	H_PAIRDATA(pg, indx)		P_ENTRY(pg, H_DATAINDEX(indx))
#define	H_PAIRSIZE(pg, psize, indx)					\
	(LEN_HITEM(pg, psize, H_KEYINDEX(indx)) +			\
	LEN_HITEM(pg, psize, H_DATAINDEX(indx)))
#define	LEN_HDATA(p, psize, indx) LEN_HKEYDATA(p, psize, H_DATAINDEX(indx))
#define	LEN_HKEY(p, psize, indx) LEN_HKEYDATA(p, psize, H_KEYINDEX(indx))

/*
 * The third type is the H_OFFPAGE, represented by the HOFFPAGE structure:
 */
typedef struct _hoffpage {
	u_int8_t  type;		/*    00: Page type and delete flag. */
	u_int8_t  unused[3];	/* 01-03: Padding, unused. */
	db_pgno_t pgno;		/* 04-07: Offpage page number. */
	u_int32_t tlen;		/* 08-11: Total length of item. */
} HOFFPAGE;

#define	HOFFPAGE_PGNO(p)	(((u_int8_t *)p) + SSZ(HOFFPAGE, pgno))
#define	HOFFPAGE_TLEN(p)	(((u_int8_t *)p) + SSZ(HOFFPAGE, tlen))

/*
 * Page space required to add a new HOFFPAGE item to the page, with and
 * without the index value.
 */
#define	HOFFPAGE_SIZE		(sizeof(HOFFPAGE))
#define	HOFFPAGE_PSIZE		(HOFFPAGE_SIZE + sizeof(db_indx_t))

/*
 * The fourth type is H_OFFDUP represented by the HOFFDUP structure:
 */
typedef struct _hoffdup {
	u_int8_t  type;		/*    00: Page type and delete flag. */
	u_int8_t  unused[3];	/* 01-03: Padding, unused. */
	db_pgno_t pgno;		/* 04-07: Offpage page number. */
} HOFFDUP;
#define	HOFFDUP_PGNO(p)		(((u_int8_t *)p) + SSZ(HOFFDUP, pgno))

/*
 * Page space required to add a new HOFFDUP item to the page, with and
 * without the index value.
 */
#define	HOFFDUP_SIZE		(sizeof(HOFFDUP))

/************************************************************************
 BTREE PAGE LAYOUT
 ************************************************************************/

/* Each index references a group of bytes on the page. */
#define	B_KEYDATA	1	/* Key/data item. */
#define	B_DUPLICATE	2	/* Duplicate key/data item. */
#define	B_OVERFLOW	3	/* Overflow key/data item. */

/*
 * We have to store a deleted entry flag in the page.   The reason is complex,
 * but the simple version is that we can't delete on-page items referenced by
 * a cursor -- the return order of subsequent insertions might be wrong.  The
 * delete flag is an overload of the top bit of the type byte.
 */
#define	B_DELETE	(0x80)
#define	B_DCLR(t)	(t) &= ~B_DELETE
#define	B_DSET(t)	(t) |= B_DELETE
#define	B_DISSET(t)	((t) & B_DELETE)

#define	B_TYPE(t)	((t) & ~B_DELETE)
#define	B_TSET(t, type, deleted) {					\
	(t) = (type);							\
	if (deleted)							\
		B_DSET(t);						\
}

/*
 * The first type is B_KEYDATA, represented by the BKEYDATA structure:
 */
typedef struct _bkeydata {
	db_indx_t len;		/* 00-01: Key/data item length. */
	u_int8_t  type;		/*    02: Page type AND DELETE FLAG. */
	u_int8_t  data[1];	/* Variable length key/data item. */
} BKEYDATA;

/* Get a BKEYDATA item for a specific index. */
#define	GET_BKEYDATA(pg, indx)						\
	((BKEYDATA *)P_ENTRY(pg, indx))

/*
 * Page space required to add a new BKEYDATA item to the page, with and
 * without the index value.
 */
#define	BKEYDATA_SIZE(len)						\
	ALIGN((len) + SSZA(BKEYDATA, data), sizeof(u_int32_t))
#define	BKEYDATA_PSIZE(len)						\
	(BKEYDATA_SIZE(len) + sizeof(db_indx_t))

/*
 * The second and third types are B_DUPLICATE and B_OVERFLOW, represented
 * by the BOVERFLOW structure.
 */
typedef struct _boverflow {
	db_indx_t unused1;	/* 00-01: Padding, unused. */
	u_int8_t  type;		/*    02: Page type AND DELETE FLAG. */
	u_int8_t  unused2;	/*    03: Padding, unused. */
	db_pgno_t pgno;		/* 04-07: Next page number. */
	u_int32_t tlen;		/* 08-11: Total length of item. */
} BOVERFLOW;

/* Get a BOVERFLOW item for a specific index. */
#define	GET_BOVERFLOW(pg, indx)						\
	((BOVERFLOW *)P_ENTRY(pg, indx))

/*
 * Page space required to add a new BOVERFLOW item to the page, with and
 * without the index value.
 */
#define	BOVERFLOW_SIZE							\
	ALIGN(sizeof(BOVERFLOW), sizeof(u_int32_t))
#define	BOVERFLOW_PSIZE							\
	(BOVERFLOW_SIZE + sizeof(db_indx_t))

/*
 * Btree leaf and hash page layouts group indices in sets of two, one for the
 * key and one for the data.  Everything else does it in sets of one to save
 * space.  Use the following macros so that it's real obvious what's going on.
 */
#define	O_INDX	1
#define	P_INDX	2

/************************************************************************
 BTREE INTERNAL PAGE LAYOUT
 ************************************************************************/

/*
 * Btree internal entry.
 */
typedef struct _binternal {
	db_indx_t  len;		/* 00-01: Key/data item length. */
	u_int8_t   type;	/*    02: Page type AND DELETE FLAG. */
	u_int8_t   unused;	/*    03: Padding, unused. */
	db_pgno_t  pgno;	/* 04-07: Page number of referenced page. */
	db_recno_t nrecs;	/* 08-11: Subtree record count. */
	u_int8_t   data[1];	/* Variable length key item. */
} BINTERNAL;

/* Get a BINTERNAL item for a specific index. */
#define	GET_BINTERNAL(pg, indx)						\
	((BINTERNAL *)P_ENTRY(pg, indx))

/*
 * Page space required to add a new BINTERNAL item to the page, with and
 * without the index value.
 */
#define	BINTERNAL_SIZE(len)						\
	ALIGN((len) + SSZA(BINTERNAL, data), sizeof(u_int32_t))
#define	BINTERNAL_PSIZE(len)						\
	(BINTERNAL_SIZE(len) + sizeof(db_indx_t))

/************************************************************************
 RECNO INTERNAL PAGE LAYOUT
 ************************************************************************/

/*
 * The recno internal entry.
 */
typedef struct _rinternal {
	db_pgno_t  pgno;	/* 00-03: Page number of referenced page. */
	db_recno_t nrecs;	/* 04-07: Subtree record count. */
} RINTERNAL;

/* Get a RINTERNAL item for a specific index. */
#define	GET_RINTERNAL(pg, indx)						\
	((RINTERNAL *)P_ENTRY(pg, indx))

/*
 * Page space required to add a new RINTERNAL item to the page, with and
 * without the index value.
 */
#define	RINTERNAL_SIZE							\
	ALIGN(sizeof(RINTERNAL), sizeof(u_int32_t))
#define	RINTERNAL_PSIZE							\
	(RINTERNAL_SIZE + sizeof(db_indx_t))

#if defined(__cplusplus)
}
#endif

#endif /* _DB_PAGE_H_ */
