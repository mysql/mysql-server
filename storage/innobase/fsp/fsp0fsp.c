/**********************************************************************
File space management

(c) 1995 Innobase Oy

Created 11/29/1995 Heikki Tuuri
***********************************************************************/

#include "fsp0fsp.h"

#ifdef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#include "buf0buf.h"
#include "fil0fil.h"
#include "sync0sync.h"
#include "mtr0log.h"
#include "fut0fut.h"
#include "ut0byte.h"
#include "srv0srv.h"
#include "page0types.h"
#include "ibuf0ibuf.h"
#include "btr0btr.h"
#include "btr0sea.h"
#include "dict0boot.h"
#include "dict0mem.h"
#include "log0log.h"


#define FSP_HEADER_OFFSET	FIL_PAGE_DATA	/* Offset of the space header
						within a file page */

/* The data structures in files are defined just as byte strings in C */
typedef	byte	fsp_header_t;
typedef	byte	xdes_t;

/*			SPACE HEADER
			============

File space header data structure: this data structure is contained in the
first page of a space. The space for this header is reserved in every extent
descriptor page, but used only in the first. */

/*-------------------------------------*/
#define FSP_SPACE_ID		0	/* space id */
#define FSP_NOT_USED		4	/* this field contained a value up to
					which we know that the modifications
					in the database have been flushed to
					the file space; not used now */
#define	FSP_SIZE		8	/* Current size of the space in
					pages */
#define	FSP_FREE_LIMIT		12	/* Minimum page number for which the
					free list has not been initialized:
					the pages >= this limit are, by
					definition, free; note that in a
					single-table tablespace where size
					< 64 pages, this number is 64, i.e.,
					we have initialized the space
					about the first extent, but have not
					physically allocted those pages to the
					file */
#define	FSP_LOWEST_NO_WRITE	16	/* The lowest page offset for which
					the page has not been written to disk
					(if it has been written, we know that
					the OS has really reserved the
					physical space for the page) */
#define	FSP_FRAG_N_USED		20	/* number of used pages in the
					FSP_FREE_FRAG list */
#define	FSP_FREE		24	/* list of free extents */
#define	FSP_FREE_FRAG		(24 + FLST_BASE_NODE_SIZE)
					/* list of partially free extents not
					belonging to any segment */
#define	FSP_FULL_FRAG		(24 + 2 * FLST_BASE_NODE_SIZE)
					/* list of full extents not belonging
					to any segment */
#define FSP_SEG_ID		(24 + 3 * FLST_BASE_NODE_SIZE)
					/* 8 bytes which give the first unused
					segment id */
#define FSP_SEG_INODES_FULL	(32 + 3 * FLST_BASE_NODE_SIZE)
					/* list of pages containing segment
					headers, where all the segment inode
					slots are reserved */
#define FSP_SEG_INODES_FREE	(32 + 4 * FLST_BASE_NODE_SIZE)
					/* list of pages containing segment
					headers, where not all the segment
					header slots are reserved */
/*-------------------------------------*/
/* File space header size */
#define	FSP_HEADER_SIZE		(32 + 5 * FLST_BASE_NODE_SIZE)

#define	FSP_FREE_ADD		4	/* this many free extents are added
					to the free list from above
					FSP_FREE_LIMIT at a time */

/*			FILE SEGMENT INODE
			==================

Segment inode which is created for each segment in a tablespace. NOTE: in
purge we assume that a segment having only one currently used page can be
freed in a few steps, so that the freeing cannot fill the file buffer with
bufferfixed file pages. */

typedef	byte	fseg_inode_t;

#define FSEG_INODE_PAGE_NODE	FSEG_PAGE_DATA
					/* the list node for linking
					segment inode pages */

#define FSEG_ARR_OFFSET		(FSEG_PAGE_DATA + FLST_NODE_SIZE)
/*-------------------------------------*/
#define	FSEG_ID			0	/* 8 bytes of segment id: if this is
					ut_dulint_zero, it means that the
					header is unused */
#define FSEG_NOT_FULL_N_USED	8
					/* number of used segment pages in
					the FSEG_NOT_FULL list */
#define	FSEG_FREE		12
					/* list of free extents of this
					segment */
#define	FSEG_NOT_FULL		(12 + FLST_BASE_NODE_SIZE)
					/* list of partially free extents */
#define	FSEG_FULL		(12 + 2 * FLST_BASE_NODE_SIZE)
					/* list of full extents */
#define	FSEG_MAGIC_N		(12 + 3 * FLST_BASE_NODE_SIZE)
					/* magic number used in debugging */
#define	FSEG_FRAG_ARR		(16 + 3 * FLST_BASE_NODE_SIZE)
					/* array of individual pages
					belonging to this segment in fsp
					fragment extent lists */
#define FSEG_FRAG_ARR_N_SLOTS	(FSP_EXTENT_SIZE / 2)
					/* number of slots in the array for
					the fragment pages */
#define	FSEG_FRAG_SLOT_SIZE	4	/* a fragment page slot contains its
					page number within space, FIL_NULL
					means that the slot is not in use */
/*-------------------------------------*/
#define FSEG_INODE_SIZE	(16 + 3 * FLST_BASE_NODE_SIZE + FSEG_FRAG_ARR_N_SLOTS * FSEG_FRAG_SLOT_SIZE)

#define FSP_SEG_INODES_PER_PAGE	((UNIV_PAGE_SIZE - FSEG_ARR_OFFSET - 10) / FSEG_INODE_SIZE)
				/* Number of segment inodes which fit on a
				single page */

#define FSEG_MAGIC_N_VALUE	97937874

#define	FSEG_FILLFACTOR		8	/* If this value is x, then if
					the number of unused but reserved
					pages in a segment is less than
					reserved pages * 1/x, and there are
					at least FSEG_FRAG_LIMIT used pages,
					then we allow a new empty extent to
					be added to the segment in
					fseg_alloc_free_page. Otherwise, we
					use unused pages of the segment. */

#define FSEG_FRAG_LIMIT		FSEG_FRAG_ARR_N_SLOTS
					/* If the segment has >= this many
					used pages, it may be expanded by
					allocating extents to the segment;
					until that only individual fragment
					pages are allocated from the space */

#define	FSEG_FREE_LIST_LIMIT	40	/* If the reserved size of a segment
					is at least this many extents, we
					allow extents to be put to the free
					list of the extent: at most
					FSEG_FREE_LIST_MAX_LEN many */
#define	FSEG_FREE_LIST_MAX_LEN	4


/*			EXTENT DESCRIPTOR
			=================

File extent descriptor data structure: contains bits to tell which pages in
the extent are free and which contain old tuple version to clean. */

/*-------------------------------------*/
#define	XDES_ID			0	/* The identifier of the segment
					to which this extent belongs */
#define XDES_FLST_NODE		8	/* The list node data structure
					for the descriptors */
#define	XDES_STATE		(FLST_NODE_SIZE + 8)
					/* contains state information
					of the extent */
#define	XDES_BITMAP		(FLST_NODE_SIZE + 12)
					/* Descriptor bitmap of the pages
					in the extent */
/*-------------------------------------*/

#define	XDES_BITS_PER_PAGE	2	/* How many bits are there per page */
#define	XDES_FREE_BIT		0	/* Index of the bit which tells if
					the page is free */
#define	XDES_CLEAN_BIT		1	/* NOTE: currently not used!
					Index of the bit which tells if
					there are old versions of tuples
					on the page */
/* States of a descriptor */
#define	XDES_FREE		1	/* extent is in free list of space */
#define	XDES_FREE_FRAG		2	/* extent is in free fragment list of
					space */
#define	XDES_FULL_FRAG		3	/* extent is in full fragment list of
					space */
#define	XDES_FSEG		4	/* extent belongs to a segment */

/* File extent data structure size in bytes. The "+ 7 ) / 8" part in the
definition rounds the number of bytes upward. */
#define	XDES_SIZE	(XDES_BITMAP + (FSP_EXTENT_SIZE * XDES_BITS_PER_PAGE + 7) / 8)

/* Offset of the descriptor array on a descriptor page */
#define	XDES_ARR_OFFSET		(FSP_HEADER_OFFSET + FSP_HEADER_SIZE)

/**************************************************************************
Returns an extent to the free list of a space. */
static
void
fsp_free_extent(
/*============*/
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset in the extent */
	mtr_t*		mtr);	/* in: mtr */
/**************************************************************************
Frees an extent of a segment to the space free list. */
static
void
fseg_free_extent(
/*=============*/
	fseg_inode_t*	seg_inode, /* in: segment inode */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset in the extent */
	mtr_t*		mtr);	/* in: mtr handle */
/**************************************************************************
Calculates the number of pages reserved by a segment, and how
many pages are currently used. */
static
ulint
fseg_n_reserved_pages_low(
/*======================*/
				/* out: number of reserved pages */
	fseg_inode_t*	header,	/* in: segment inode */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr);	/* in: mtr handle */
/************************************************************************
Marks a page used. The page must reside within the extents of the given
segment. */
static
void
fseg_mark_page_used(
/*================*/
	fseg_inode_t*	seg_inode,/* in: segment inode */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr);	/* in: mtr */
/**************************************************************************
Returns the first extent descriptor for a segment. We think of the extent
lists of the segment catenated in the order FSEG_FULL -> FSEG_NOT_FULL
-> FSEG_FREE. */
static
xdes_t*
fseg_get_first_extent(
/*==================*/
				/* out: the first extent descriptor, or NULL if
				none */
	fseg_inode_t*	inode,	/* in: segment inode */
	mtr_t*		mtr);	/* in: mtr */
/**************************************************************************
Puts new extents to the free list if
there are free extents above the free limit. If an extent happens
to contain an extent descriptor page, the extent is put to
the FSP_FREE_FRAG list with the page marked as used. */
static
void
fsp_fill_free_list(
/*===============*/
	ibool		init_space,	/* in: TRUE if this is a single-table
					tablespace and we are only initing
					the tablespace's first extent
					descriptor page and ibuf bitmap page;
					then we do not allocate more extents */
	ulint		space,		/* in: space */
	fsp_header_t*	header,		/* in: space header */
	mtr_t*		mtr);		/* in: mtr */
/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation. */
static
ulint
fseg_alloc_free_page_low(
/*=====================*/
				/* out: the allocated page number, FIL_NULL
				if no page could be allocated */
	ulint		space,	/* in: space */
	fseg_inode_t*	seg_inode, /* in: segment inode */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction, /* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr);	/* in: mtr handle */


/**************************************************************************
Reads the file space size stored in the header page. */

ulint
fsp_get_size_low(
/*=============*/
			/* out: tablespace size stored in the space header */
	page_t*	page)	/* in: header page (page 0 in the tablespace) */
{
	return(mach_read_from_4(page + FSP_HEADER_OFFSET + FSP_SIZE));
}

/**************************************************************************
Gets a pointer to the space header and x-locks its page. */
UNIV_INLINE
fsp_header_t*
fsp_get_space_header(
/*=================*/
			/* out: pointer to the space header, page x-locked */
	ulint	id,	/* in: space id */
	mtr_t*	mtr)	/* in: mtr */
{
	fsp_header_t*	header;

	ut_ad(mtr);

	header = FSP_HEADER_OFFSET + buf_page_get(id, 0, RW_X_LATCH, mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(header, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */
	return(header);
}

/**************************************************************************
Gets a descriptor bit of a page. */
UNIV_INLINE
ibool
xdes_get_bit(
/*=========*/
			/* out: TRUE if free */
	xdes_t*	descr,	/* in: descriptor */
	ulint	bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ulint	offset,	/* in: page offset within extent:
			0 ... FSP_EXTENT_SIZE - 1 */
	mtr_t*	mtr)	/* in: mtr */
{
	ulint	index;
	ulint	byte_index;
	ulint	bit_index;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
						MTR_MEMO_PAGE_X_FIX));
	ut_ad((bit == XDES_FREE_BIT) || (bit == XDES_CLEAN_BIT));
	ut_ad(offset < FSP_EXTENT_SIZE);

	index = bit + XDES_BITS_PER_PAGE * offset;

	byte_index = index / 8;
	bit_index = index % 8;

	return(ut_bit_get_nth(
		   mtr_read_ulint(descr + XDES_BITMAP + byte_index,
							MLOG_1BYTE, mtr),
		   bit_index));
}

/**************************************************************************
Sets a descriptor bit of a page. */
UNIV_INLINE
void
xdes_set_bit(
/*=========*/
	xdes_t*	descr,	/* in: descriptor */
	ulint	bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ulint	offset,	/* in: page offset within extent:
			0 ... FSP_EXTENT_SIZE - 1 */
	ibool	val,	/* in: bit value */
	mtr_t*	mtr)	/* in: mtr */
{
	ulint	index;
	ulint	byte_index;
	ulint	bit_index;
	ulint	descr_byte;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
							MTR_MEMO_PAGE_X_FIX));
	ut_ad((bit == XDES_FREE_BIT) || (bit == XDES_CLEAN_BIT));
	ut_ad(offset < FSP_EXTENT_SIZE);

	index = bit + XDES_BITS_PER_PAGE * offset;

	byte_index = index / 8;
	bit_index = index % 8;

	descr_byte = mtr_read_ulint(descr + XDES_BITMAP + byte_index,
							MLOG_1BYTE, mtr);
	descr_byte = ut_bit_set_nth(descr_byte, bit_index, val);

	mlog_write_ulint(descr + XDES_BITMAP + byte_index, descr_byte,
							MLOG_1BYTE, mtr);
}

/**************************************************************************
Looks for a descriptor bit having the desired value. Starts from hint
and scans upward; at the end of the extent the search is wrapped to
the start of the extent. */
UNIV_INLINE
ulint
xdes_find_bit(
/*==========*/
			/* out: bit index of the bit, ULINT_UNDEFINED if not
			found */
	xdes_t*	descr,	/* in: descriptor */
	ulint	bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ibool	val,	/* in: desired bit value */
	ulint	hint,	/* in: hint of which bit position would be desirable */
	mtr_t*	mtr)	/* in: mtr */
{
	ulint	i;

	ut_ad(descr && mtr);
	ut_ad(val <= TRUE);
	ut_ad(hint < FSP_EXTENT_SIZE);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
							MTR_MEMO_PAGE_X_FIX));
	for (i = hint; i < FSP_EXTENT_SIZE; i++) {
		if (val == xdes_get_bit(descr, bit, i, mtr)) {

			return(i);
		}
	}

	for (i = 0; i < hint; i++) {
		if (val == xdes_get_bit(descr, bit, i, mtr)) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Looks for a descriptor bit having the desired value. Scans the extent in
a direction opposite to xdes_find_bit. */
UNIV_INLINE
ulint
xdes_find_bit_downward(
/*===================*/
			/* out: bit index of the bit, ULINT_UNDEFINED if not
			found */
	xdes_t*	descr,	/* in: descriptor */
	ulint	bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ibool	val,	/* in: desired bit value */
	ulint	hint,	/* in: hint of which bit position would be desirable */
	mtr_t*	mtr)	/* in: mtr */
{
	ulint	i;

	ut_ad(descr && mtr);
	ut_ad(val <= TRUE);
	ut_ad(hint < FSP_EXTENT_SIZE);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
							MTR_MEMO_PAGE_X_FIX));
	for (i = hint + 1; i > 0; i--) {
		if (val == xdes_get_bit(descr, bit, i - 1, mtr)) {

			return(i - 1);
		}
	}

	for (i = FSP_EXTENT_SIZE - 1; i > hint; i--) {
		if (val == xdes_get_bit(descr, bit, i, mtr)) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Returns the number of used pages in a descriptor. */
UNIV_INLINE
ulint
xdes_get_n_used(
/*============*/
			/* out: number of pages used */
	xdes_t*	descr,	/* in: descriptor */
	mtr_t*	mtr)	/* in: mtr */
{
	ulint	i;
	ulint	count	= 0;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
							MTR_MEMO_PAGE_X_FIX));
	for (i = 0; i < FSP_EXTENT_SIZE; i++) {
		if (FALSE == xdes_get_bit(descr, XDES_FREE_BIT, i, mtr)) {
			count++;
		}
	}

	return(count);
}

/**************************************************************************
Returns true if extent contains no used pages. */
UNIV_INLINE
ibool
xdes_is_free(
/*=========*/
			/* out: TRUE if totally free */
	xdes_t*	descr,	/* in: descriptor */
	mtr_t*	mtr)	/* in: mtr */
{
	if (0 == xdes_get_n_used(descr, mtr)) {

		return(TRUE);
	}

	return(FALSE);
}

/**************************************************************************
Returns true if extent contains no free pages. */
UNIV_INLINE
ibool
xdes_is_full(
/*=========*/
			/* out: TRUE if full */
	xdes_t*	descr,	/* in: descriptor */
	mtr_t*	mtr)	/* in: mtr */
{
	if (FSP_EXTENT_SIZE == xdes_get_n_used(descr, mtr)) {

		return(TRUE);
	}

	return(FALSE);
}

/**************************************************************************
Sets the state of an xdes. */
UNIV_INLINE
void
xdes_set_state(
/*===========*/
	xdes_t*	descr,	/* in: descriptor */
	ulint	state,	/* in: state to set */
	mtr_t*	mtr)	/* in: mtr handle */
{
	ut_ad(descr && mtr);
	ut_ad(state >= XDES_FREE);
	ut_ad(state <= XDES_FSEG);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
							MTR_MEMO_PAGE_X_FIX));

	mlog_write_ulint(descr + XDES_STATE, state, MLOG_4BYTES, mtr);
}

/**************************************************************************
Gets the state of an xdes. */
UNIV_INLINE
ulint
xdes_get_state(
/*===========*/
			/* out: state */
	xdes_t*	descr,	/* in: descriptor */
	mtr_t*	mtr)	/* in: mtr handle */
{
	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
							MTR_MEMO_PAGE_X_FIX));

	return(mtr_read_ulint(descr + XDES_STATE, MLOG_4BYTES, mtr));
}

/**************************************************************************
Inits an extent descriptor to the free and clean state. */
UNIV_INLINE
void
xdes_init(
/*======*/
	xdes_t*	descr,	/* in: descriptor */
	mtr_t*	mtr)	/* in: mtr */
{
	ulint	i;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
							MTR_MEMO_PAGE_X_FIX));
	ut_ad((XDES_SIZE - XDES_BITMAP) % 4 == 0);

	for (i = XDES_BITMAP; i < XDES_SIZE; i += 4) {
		mlog_write_ulint(descr + i, 0xFFFFFFFFUL, MLOG_4BYTES, mtr);
	}

	xdes_set_state(descr, XDES_FREE, mtr);
}

/************************************************************************
Calculates the page where the descriptor of a page resides. */
UNIV_INLINE
ulint
xdes_calc_descriptor_page(
/*======================*/
				/* out: descriptor page offset */
	ulint	offset)		/* in: page offset */
{
#if UNIV_PAGE_SIZE <= XDES_ARR_OFFSET \
		+ (XDES_DESCRIBED_PER_PAGE / FSP_EXTENT_SIZE) * XDES_SIZE
# error
#endif

	return(ut_2pow_round(offset, XDES_DESCRIBED_PER_PAGE));
}

/************************************************************************
Calculates the descriptor index within a descriptor page. */
UNIV_INLINE
ulint
xdes_calc_descriptor_index(
/*=======================*/
				/* out: descriptor index */
	ulint	offset)		/* in: page offset */
{
	return(ut_2pow_remainder(offset, XDES_DESCRIBED_PER_PAGE) /
							FSP_EXTENT_SIZE);
}

/************************************************************************
Gets pointer to a the extent descriptor of a page. The page where the extent
descriptor resides is x-locked. If the page offset is equal to the free limit
of the space, adds new extents from above the free limit to the space free
list, if not free limit == space size. This adding is necessary to make the
descriptor defined, as they are uninitialized above the free limit. */
UNIV_INLINE
xdes_t*
xdes_get_descriptor_with_space_hdr(
/*===============================*/
				/* out: pointer to the extent descriptor,
				NULL if the page does not exist in the
				space or if offset > free limit */
	fsp_header_t*	sp_header,/* in: space header, x-latched */
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: page offset;
				if equal to the free limit,
				we try to add new extents to
				the space free list */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	limit;
	ulint	size;
	ulint	descr_page_no;
	page_t*	descr_page;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space),
						MTR_MEMO_X_LOCK));
	/* Read free limit and space size */
	limit = mtr_read_ulint(sp_header + FSP_FREE_LIMIT, MLOG_4BYTES, mtr);
	size  = mtr_read_ulint(sp_header + FSP_SIZE, MLOG_4BYTES, mtr);

	/* If offset is >= size or > limit, return NULL */

	if ((offset >= size) || (offset > limit)) {

		return(NULL);
	}

	/* If offset is == limit, fill free list of the space. */

	if (offset == limit) {
		fsp_fill_free_list(FALSE, space, sp_header, mtr);
	}

	descr_page_no = xdes_calc_descriptor_page(offset);

	if (descr_page_no == 0) {
		/* It is on the space header page */

		descr_page = buf_frame_align(sp_header);
	} else {
		descr_page = buf_page_get(space, descr_page_no, RW_X_LATCH,
									mtr);
#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(descr_page, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */
	}

	return(descr_page + XDES_ARR_OFFSET
		+ XDES_SIZE * xdes_calc_descriptor_index(offset));
}

/************************************************************************
Gets pointer to a the extent descriptor of a page. The page where the
extent descriptor resides is x-locked. If the page offset is equal to
the free limit of the space, adds new extents from above the free limit
to the space free list, if not free limit == space size. This adding
is necessary to make the descriptor defined, as they are uninitialized
above the free limit. */
static
xdes_t*
xdes_get_descriptor(
/*================*/
			/* out: pointer to the extent descriptor, NULL if the
			page does not exist in the space or if offset > free
			limit */
	ulint	space,	/* in: space id */
	ulint	offset,	/* in: page offset; if equal to the free limit,
			we try to add new extents to the space free list */
	mtr_t*	mtr)	/* in: mtr handle */
{
	fsp_header_t*	sp_header;

	sp_header = FSP_HEADER_OFFSET
				+ buf_page_get(space, 0, RW_X_LATCH, mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(sp_header, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */
	return(xdes_get_descriptor_with_space_hdr(sp_header, space, offset,
									mtr));
}

/************************************************************************
Gets pointer to a the extent descriptor if the file address
of the descriptor list node is known. The page where the
extent descriptor resides is x-locked. */
UNIV_INLINE
xdes_t*
xdes_lst_get_descriptor(
/*====================*/
				/* out: pointer to the extent descriptor */
	ulint		space,	/* in: space id */
	fil_addr_t	lst_node,/* in: file address of the list node
				contained in the descriptor */
	mtr_t*		mtr)	/* in: mtr handle */
{
	xdes_t*	descr;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space),
							MTR_MEMO_X_LOCK));
	descr = fut_get_ptr(space, lst_node, RW_X_LATCH, mtr) - XDES_FLST_NODE;

	return(descr);
}

/************************************************************************
Gets pointer to the next descriptor in a descriptor list and x-locks its
page. */
UNIV_INLINE
xdes_t*
xdes_lst_get_next(
/*==============*/
	xdes_t*	descr,	/* in: pointer to a descriptor */
	mtr_t*	mtr)	/* in: mtr handle */
{
	ulint	space;

	ut_ad(mtr && descr);

	space = buf_frame_get_space_id(descr);

	return(xdes_lst_get_descriptor(space,
		flst_get_next_addr(descr + XDES_FLST_NODE, mtr), mtr));
}

/************************************************************************
Returns page offset of the first page in extent described by a descriptor. */
UNIV_INLINE
ulint
xdes_get_offset(
/*============*/
			/* out: offset of the first page in extent */
	xdes_t*	descr)	/* in: extent descriptor */
{
	ut_ad(descr);

	return(buf_frame_get_page_no(descr)
		+ ((descr - buf_frame_align(descr) - XDES_ARR_OFFSET)
		   / XDES_SIZE)
		  * FSP_EXTENT_SIZE);
}

/***************************************************************
Inits a file page whose prior contents should be ignored. */
static
void
fsp_init_file_page_low(
/*===================*/
	byte*	ptr)	/* in: pointer to a page */
{
	page_t*	page;
	page = buf_frame_align(ptr);

	buf_block_align(page)->check_index_page_at_flush = FALSE;

#ifdef UNIV_BASIC_LOG_DEBUG
	memset(page, 0xff, UNIV_PAGE_SIZE);
#endif
	mach_write_to_8(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
							ut_dulint_zero);
	mach_write_to_8(page + FIL_PAGE_LSN, ut_dulint_zero);
}

/***************************************************************
Inits a file page whose prior contents should be ignored. */
static
void
fsp_init_file_page(
/*===============*/
	page_t*	page,	/* in: page */
	mtr_t*	mtr)	/* in: mtr */
{
	fsp_init_file_page_low(page);

	mlog_write_initial_log_record(page, MLOG_INIT_FILE_PAGE, mtr);
}

/***************************************************************
Parses a redo log record of a file page init. */

byte*
fsp_parse_init_file_page(
/*=====================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr __attribute__((unused)), /* in: buffer end */
	page_t*	page)	/* in: page or NULL */
{
	ut_ad(ptr && end_ptr);

	if (page) {
		fsp_init_file_page_low(page);
	}

	return(ptr);
}

/**************************************************************************
Initializes the fsp system. */

void
fsp_init(void)
/*==========*/
{
	/* Does nothing at the moment */
}

/**************************************************************************
Writes the space id to a tablespace header. This function is used past the
buffer pool when we in fil0fil.c create a new single-table tablespace. */

void
fsp_header_write_space_id(
/*======================*/
	page_t*	page,		/* in: first page in the space */
	ulint	space_id)	/* in: space id */
{
	mach_write_to_4(page + FSP_HEADER_OFFSET + FSP_SPACE_ID, space_id);
}

/**************************************************************************
Initializes the space header of a new created space and creates also the
insert buffer tree root if space == 0. */

void
fsp_header_init(
/*============*/
	ulint	space,	/* in: space id */
	ulint	size,	/* in: current size in blocks */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	fsp_header_t*	header;
	page_t*		page;

	ut_ad(mtr);

	mtr_x_lock(fil_space_get_latch(space), mtr);

	page = buf_page_create(space, 0, mtr);
	buf_page_get(space, 0, RW_X_LATCH, mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */

	/* The prior contents of the file page should be ignored */

	fsp_init_file_page(page, mtr);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_TYPE_FSP_HDR,
					MLOG_2BYTES, mtr);

	header = FSP_HEADER_OFFSET + page;

	mlog_write_ulint(header + FSP_SPACE_ID, space, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_NOT_USED, 0, MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSP_SIZE, size, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_FREE_LIMIT, 0, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_LOWEST_NO_WRITE, 0, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_FRAG_N_USED, 0, MLOG_4BYTES, mtr);

	flst_init(header + FSP_FREE, mtr);
	flst_init(header + FSP_FREE_FRAG, mtr);
	flst_init(header + FSP_FULL_FRAG, mtr);
	flst_init(header + FSP_SEG_INODES_FULL, mtr);
	flst_init(header + FSP_SEG_INODES_FREE, mtr);

	mlog_write_dulint(header + FSP_SEG_ID, ut_dulint_create(0, 1), mtr);
	if (space == 0) {
		fsp_fill_free_list(FALSE, space, header, mtr);
		btr_create(DICT_CLUSTERED | DICT_UNIVERSAL | DICT_IBUF, space,
			ut_dulint_add(DICT_IBUF_ID_MIN, space), FALSE, mtr);
	} else {
		fsp_fill_free_list(TRUE, space, header, mtr);
	}
}

/**************************************************************************
Reads the space id from the first page of a tablespace. */

ulint
fsp_header_get_space_id(
/*====================*/
			/* out: space id, ULINT UNDEFINED if error */
	page_t*	page)	/* in: first page of a tablespace */
{
	ulint	fsp_id;
	ulint	id;

	fsp_id = mach_read_from_4(FSP_HEADER_OFFSET + page + FSP_SPACE_ID);

	id = mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

	if (id != fsp_id) {
		fprintf(stderr,
"InnoDB: Error: space id in fsp header %lu, but in the page header %lu\n",
			(ulong) fsp_id, (ulong) id);

		return(ULINT_UNDEFINED);
	}

	return(id);
}

/**************************************************************************
Increases the space size field of a space. */

void
fsp_header_inc_size(
/*================*/
	ulint	space,	/* in: space id */
	ulint	size_inc,/* in: size increment in pages */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	fsp_header_t*	header;
	ulint		size;

	ut_ad(mtr);

	mtr_x_lock(fil_space_get_latch(space), mtr);

	header = fsp_get_space_header(space, mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSP_SIZE, size + size_inc, MLOG_4BYTES,
									mtr);
}

/**************************************************************************
Gets the current free limit of a tablespace. The free limit means the
place of the first page which has never been put to the the free list
for allocation. The space above that address is initialized to zero.
Sets also the global variable log_fsp_current_free_limit. */

ulint
fsp_header_get_free_limit(
/*======================*/
			/* out: free limit in megabytes */
	ulint	space)	/* in: space id, must be 0 */
{
	fsp_header_t*	header;
	ulint		limit;
	mtr_t		mtr;

	ut_a(space == 0); /* We have only one log_fsp_current_... variable */

	mtr_start(&mtr);

	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	limit = mtr_read_ulint(header + FSP_FREE_LIMIT, MLOG_4BYTES, &mtr);

	limit = limit / ((1024 * 1024) / UNIV_PAGE_SIZE);

	log_fsp_current_free_limit_set_and_checkpoint(limit);

	mtr_commit(&mtr);

	return(limit);
}

/**************************************************************************
Gets the size of the tablespace from the tablespace header. If we do not
have an auto-extending data file, this should be equal to the size of the
data files. If there is an auto-extending data file, this can be smaller. */

ulint
fsp_header_get_tablespace_size(
/*===========================*/
			/* out: size in pages */
	ulint	space)	/* in: space id, must be 0 */
{
	fsp_header_t*	header;
	ulint		size;
	mtr_t		mtr;

	ut_a(space == 0); /* We have only one log_fsp_current_... variable */

	mtr_start(&mtr);

	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr);

	mtr_commit(&mtr);

	return(size);
}

/***************************************************************************
Tries to extend a single-table tablespace so that a page would fit in the
data file. */
static
ibool
fsp_try_extend_data_file_with_pages(
/*================================*/
					/* out: TRUE if success */
	ulint		space,		/* in: space */
	ulint		page_no,	/* in: page number */
	fsp_header_t*	header,		/* in: space header */
	mtr_t*		mtr)		/* in: mtr */
{
	ibool	success;
	ulint	actual_size;
	ulint	size;

	ut_a(space != 0);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	ut_a(page_no >= size);

	success = fil_extend_space_to_desired_size(&actual_size, space,
								page_no + 1);
	/* actual_size now has the space size in pages; it may be less than
	we wanted if we ran out of disk space */

	mlog_write_ulint(header + FSP_SIZE, actual_size, MLOG_4BYTES, mtr);

	return(success);
}

/***************************************************************************
Tries to extend the last data file of a tablespace if it is auto-extending. */
static
ibool
fsp_try_extend_data_file(
/*=====================*/
					/* out: FALSE if not auto-extending */
	ulint*		actual_increase,/* out: actual increase in pages, where
					we measure the tablespace size from
					what the header field says; it may be
					the actual file size rounded down to
					megabyte */
	ulint		space,		/* in: space */
	fsp_header_t*	header,		/* in: space header */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	size;
	ulint	new_size;
	ulint	old_size;
	ulint	size_increase;
	ulint	actual_size;
	ibool	success;

	*actual_increase = 0;

	if (space == 0 && !srv_auto_extend_last_data_file) {

		return(FALSE);
	}

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	old_size = size;

	if (space == 0 && srv_last_file_size_max != 0) {
		if (srv_last_file_size_max
			 < srv_data_file_sizes[srv_n_data_files - 1]) {

			fprintf(stderr,
"InnoDB: Error: Last data file size is %lu, max size allowed %lu\n",
				(ulong) srv_data_file_sizes[srv_n_data_files - 1],
				(ulong) srv_last_file_size_max);
		}

		size_increase = srv_last_file_size_max
				 - srv_data_file_sizes[srv_n_data_files - 1];
		if (size_increase > SRV_AUTO_EXTEND_INCREMENT) {
			size_increase = SRV_AUTO_EXTEND_INCREMENT;
		}
	} else {
		if (space == 0) {
			size_increase = SRV_AUTO_EXTEND_INCREMENT;
		} else {
			/* We extend single-table tablespaces first one extent
			at a time, but for bigger tablespaces more. It is not
			enough to extend always by one extent, because some
			extents are frag page extents. */

			if (size < FSP_EXTENT_SIZE) {
				/* Let us first extend the file to 64 pages */
				success = fsp_try_extend_data_file_with_pages(
					  space, FSP_EXTENT_SIZE - 1,
					  header, mtr);
				if (!success) {
					new_size = mtr_read_ulint(
					 header + FSP_SIZE, MLOG_4BYTES, mtr);

					*actual_increase = new_size - old_size;

					return(FALSE);
				}

				size = FSP_EXTENT_SIZE;
			}

			if (size < 32 * FSP_EXTENT_SIZE) {
				size_increase = FSP_EXTENT_SIZE;
			} else {
				/* Below in fsp_fill_free_list() we assume
				that we add at most FSP_FREE_ADD extents at
				a time */
				size_increase = FSP_FREE_ADD * FSP_EXTENT_SIZE;
			}
		}
	}

	if (size_increase == 0) {

		return(TRUE);
	}

	success = fil_extend_space_to_desired_size(&actual_size, space,
							size + size_increase);
	/* We ignore any fragments of a full megabyte when storing the size
	to the space header */

	mlog_write_ulint(header + FSP_SIZE,
	   ut_calc_align_down(actual_size, (1024 * 1024) / UNIV_PAGE_SIZE),
							MLOG_4BYTES, mtr);
	new_size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	*actual_increase = new_size - old_size;

	return(TRUE);
}

/**************************************************************************
Puts new extents to the free list if there are free extents above the free
limit. If an extent happens to contain an extent descriptor page, the extent
is put to the FSP_FREE_FRAG list with the page marked as used. */
static
void
fsp_fill_free_list(
/*===============*/
	ibool		init_space,	/* in: TRUE if this is a single-table
					tablespace and we are only initing
					the tablespace's first extent
					descriptor page and ibuf bitmap page;
					then we do not allocate more extents */
	ulint		space,		/* in: space */
	fsp_header_t*	header,		/* in: space header */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	limit;
	ulint	size;
	xdes_t*	descr;
	ulint	count		= 0;
	ulint	frag_n_used;
	page_t*	descr_page;
	page_t*	ibuf_page;
	ulint	actual_increase;
	ulint	i;
	mtr_t	ibuf_mtr;

	ut_ad(header && mtr);

	/* Check if we can fill free list from above the free list limit */
	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	limit = mtr_read_ulint(header + FSP_FREE_LIMIT, MLOG_4BYTES, mtr);

	if (space == 0 && srv_auto_extend_last_data_file
			&& size < limit + FSP_EXTENT_SIZE * FSP_FREE_ADD) {

		/* Try to increase the last data file size */
		fsp_try_extend_data_file(&actual_increase, space, header, mtr);
		size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	}

	if (space != 0 && !init_space
			&& size < limit + FSP_EXTENT_SIZE * FSP_FREE_ADD) {

		/* Try to increase the .ibd file size */
		fsp_try_extend_data_file(&actual_increase, space, header, mtr);
		size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	}

	i = limit;

	while ((init_space && i < 1)
		|| ((i + FSP_EXTENT_SIZE <= size) && (count < FSP_FREE_ADD))) {

		mlog_write_ulint(header + FSP_FREE_LIMIT, i + FSP_EXTENT_SIZE,
							MLOG_4BYTES, mtr);

		/* Update the free limit info in the log system and make
		a checkpoint */
		if (space == 0) {
			log_fsp_current_free_limit_set_and_checkpoint(
				(i + FSP_EXTENT_SIZE)
				/ ((1024 * 1024) / UNIV_PAGE_SIZE));
		}

		if (0 == i % XDES_DESCRIBED_PER_PAGE) {

			/* We are going to initialize a new descriptor page
			and a new ibuf bitmap page: the prior contents of the
			pages should be ignored. */

			if (i > 0) {
				descr_page = buf_page_create(space, i, mtr);
				buf_page_get(space, i, RW_X_LATCH, mtr);
#ifdef UNIV_SYNC_DEBUG
				buf_page_dbg_add_level(descr_page,
								SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */
				fsp_init_file_page(descr_page, mtr);
				mlog_write_ulint(descr_page + FIL_PAGE_TYPE,
					FIL_PAGE_TYPE_XDES, MLOG_2BYTES, mtr);
			}

			/* Initialize the ibuf bitmap page in a separate
			mini-transaction because it is low in the latching
			order, and we must be able to release its latch
			before returning from the fsp routine */

			mtr_start(&ibuf_mtr);

			ibuf_page = buf_page_create(space,
					i + FSP_IBUF_BITMAP_OFFSET, &ibuf_mtr);
			buf_page_get(space, i + FSP_IBUF_BITMAP_OFFSET,
							RW_X_LATCH, &ibuf_mtr);
#ifdef UNIV_SYNC_DEBUG
			buf_page_dbg_add_level(ibuf_page, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */
			fsp_init_file_page(ibuf_page, &ibuf_mtr);

			ibuf_bitmap_page_init(ibuf_page, &ibuf_mtr);

			mtr_commit(&ibuf_mtr);
		}

		descr = xdes_get_descriptor_with_space_hdr(header, space, i,
									mtr);
		xdes_init(descr, mtr);

#if XDES_DESCRIBED_PER_PAGE % FSP_EXTENT_SIZE
# error "XDES_DESCRIBED_PER_PAGE % FSP_EXTENT_SIZE != 0"
#endif

		if (0 == i % XDES_DESCRIBED_PER_PAGE) {

			/* The first page in the extent is a descriptor page
			and the second is an ibuf bitmap page: mark them
			used */

			xdes_set_bit(descr, XDES_FREE_BIT, 0, FALSE, mtr);
			xdes_set_bit(descr, XDES_FREE_BIT,
				FSP_IBUF_BITMAP_OFFSET, FALSE, mtr);
			xdes_set_state(descr, XDES_FREE_FRAG, mtr);

			flst_add_last(header + FSP_FREE_FRAG,
				descr + XDES_FLST_NODE, mtr);
			frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
				MLOG_4BYTES, mtr);
			mlog_write_ulint(header + FSP_FRAG_N_USED,
				frag_n_used + 2, MLOG_4BYTES, mtr);
		} else {
			flst_add_last(header + FSP_FREE,
				descr + XDES_FLST_NODE, mtr);
			count++;
		}

		i += FSP_EXTENT_SIZE;
	}
}

/**************************************************************************
Allocates a new free extent. */
static
xdes_t*
fsp_alloc_free_extent(
/*==================*/
			/* out: extent descriptor, NULL if cannot be
			allocated */
	ulint	space,	/* in: space id */
	ulint	hint,	/* in: hint of which extent would be desirable: any
			page offset in the extent goes; the hint must not
			be > FSP_FREE_LIMIT */
	mtr_t*	mtr)	/* in: mtr */
{
	fsp_header_t*	header;
	fil_addr_t	first;
	xdes_t*		descr;

	ut_ad(mtr);

	header = fsp_get_space_header(space, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, hint, mtr);

	if (descr && (xdes_get_state(descr, mtr) == XDES_FREE)) {
		/* Ok, we can take this extent */
	} else {
		/* Take the first extent in the free list */
		first = flst_get_first(header + FSP_FREE, mtr);

		if (fil_addr_is_null(first)) {
			fsp_fill_free_list(FALSE, space, header, mtr);

			first = flst_get_first(header + FSP_FREE, mtr);
		}

		if (fil_addr_is_null(first)) {

			return(NULL);	/* No free extents left */
		}

		descr = xdes_lst_get_descriptor(space, first, mtr);
	}

	flst_remove(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);

	return(descr);
}

/**************************************************************************
Allocates a single free page from a space. The page is marked as used. */
static
ulint
fsp_alloc_free_page(
/*================*/
			/* out: the page offset, FIL_NULL if no page could
			be allocated */
	ulint	space,	/* in: space id */
	ulint	hint,	/* in: hint of which page would be desirable */
	mtr_t*	mtr)	/* in: mtr handle */
{
	fsp_header_t*	header;
	fil_addr_t	first;
	xdes_t*		descr;
	page_t*		page;
	ulint		free;
	ulint		frag_n_used;
	ulint		page_no;
	ulint		space_size;
	ibool		success;

	ut_ad(mtr);

	header = fsp_get_space_header(space, mtr);

	/* Get the hinted descriptor */
	descr = xdes_get_descriptor_with_space_hdr(header, space, hint, mtr);

	if (descr && (xdes_get_state(descr, mtr) == XDES_FREE_FRAG)) {
		/* Ok, we can take this extent */
	} else {
		/* Else take the first extent in free_frag list */
		first = flst_get_first(header + FSP_FREE_FRAG, mtr);

		if (fil_addr_is_null(first)) {
			/* There are no partially full fragments: allocate
			a free extent and add it to the FREE_FRAG list. NOTE
			that the allocation may have as a side-effect that an
			extent containing a descriptor page is added to the
			FREE_FRAG list. But we will allocate our page from the
			the free extent anyway. */

			descr = fsp_alloc_free_extent(space, hint, mtr);

			if (descr == NULL) {
				/* No free space left */

				return(FIL_NULL);
			}

			xdes_set_state(descr, XDES_FREE_FRAG, mtr);
			flst_add_last(header + FSP_FREE_FRAG,
						descr + XDES_FLST_NODE, mtr);
		} else {
			descr = xdes_lst_get_descriptor(space, first, mtr);
		}

		/* Reset the hint */
		hint = 0;
	}

	/* Now we have in descr an extent with at least one free page. Look
	for a free page in the extent. */

	free = xdes_find_bit(descr, XDES_FREE_BIT, TRUE,
						hint % FSP_EXTENT_SIZE, mtr);
	if (free == ULINT_UNDEFINED) {

		ut_print_buf(stderr, ((byte*)descr) - 500, 1000);

		ut_error;
	}

	page_no = xdes_get_offset(descr) + free;

	space_size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	if (space_size <= page_no) {
		/* It must be that we are extending a single-table tablespace
		whose size is still < 64 pages */

		ut_a(space != 0);
		if (page_no >= FSP_EXTENT_SIZE) {
			fprintf(stderr,
"InnoDB: Error: trying to extend a single-table tablespace %lu\n"
"InnoDB: by single page(s) though the space size %lu. Page no %lu.\n",
			   (ulong) space, (ulong) space_size, (ulong) page_no);
			return(FIL_NULL);
		}
		success = fsp_try_extend_data_file_with_pages(space, page_no,
			header, mtr);
		if (!success) {
			/* No disk space left */
			return(FIL_NULL);
		}
	}

	xdes_set_bit(descr, XDES_FREE_BIT, free, FALSE, mtr);

	/* Update the FRAG_N_USED field */
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES,
									mtr);
	frag_n_used++;
	mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used, MLOG_4BYTES,
									mtr);
	if (xdes_is_full(descr, mtr)) {
		/* The fragment is full: move it to another list */
		flst_remove(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE,
									mtr);
		xdes_set_state(descr, XDES_FULL_FRAG, mtr);

		flst_add_last(header + FSP_FULL_FRAG, descr + XDES_FLST_NODE,
									mtr);
		mlog_write_ulint(header + FSP_FRAG_N_USED,
				frag_n_used - FSP_EXTENT_SIZE, MLOG_4BYTES,
									mtr);
	}

	/* Initialize the allocated page to the buffer pool, so that it can
	be obtained immediately with buf_page_get without need for a disk
	read. */

	buf_page_create(space, page_no, mtr);

	page = buf_page_get(space, page_no, RW_X_LATCH, mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */

	/* Prior contents of the page should be ignored */
	fsp_init_file_page(page, mtr);

	return(page_no);
}

/**************************************************************************
Frees a single page of a space. The page is marked as free and clean. */
static
void
fsp_free_page(
/*==========*/
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page offset */
	mtr_t*	mtr)	/* in: mtr handle */
{
	fsp_header_t*	header;
	xdes_t*		descr;
	ulint		state;
	ulint		frag_n_used;

	ut_ad(mtr);

/*	fprintf(stderr, "Freeing page %lu in space %lu\n", page, space); */

	header = fsp_get_space_header(space, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, page, mtr);

	state = xdes_get_state(descr, mtr);

	if (state != XDES_FREE_FRAG && state != XDES_FULL_FRAG) {
		fprintf(stderr,
"InnoDB: Error: File space extent descriptor of page %lu has state %lu\n",
								(ulong) page,
								(ulong) state);
		fputs("InnoDB: Dump of descriptor: ", stderr);
		ut_print_buf(stderr, ((byte*)descr) - 50, 200);
		putc('\n', stderr);

		if (state == XDES_FREE) {
			/* We put here some fault tolerance: if the page
			is already free, return without doing anything! */

			return;
		}

		ut_error;
	}

	if (xdes_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr)) {
		fprintf(stderr,
"InnoDB: Error: File space extent descriptor of page %lu says it is free\n"
"InnoDB: Dump of descriptor: ", (ulong) page);
		ut_print_buf(stderr, ((byte*)descr) - 50, 200);
		putc('\n', stderr);

		/* We put here some fault tolerance: if the page
		is already free, return without doing anything! */

		return;
	}

	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);

	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES,
									mtr);
	if (state == XDES_FULL_FRAG) {
		/* The fragment was full: move it to another list */
		flst_remove(header + FSP_FULL_FRAG, descr + XDES_FLST_NODE,
									mtr);
		xdes_set_state(descr, XDES_FREE_FRAG, mtr);
		flst_add_last(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE,
									mtr);
		mlog_write_ulint(header + FSP_FRAG_N_USED,
					frag_n_used + FSP_EXTENT_SIZE - 1,
							MLOG_4BYTES, mtr);
	} else {
		ut_a(frag_n_used > 0);
		mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used - 1,
							MLOG_4BYTES, mtr);
	}

	if (xdes_is_free(descr, mtr)) {
		/* The extent has become free: move it to another list */
		flst_remove(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE,
									mtr);
		fsp_free_extent(space, page, mtr);
	}
}

/**************************************************************************
Returns an extent to the free list of a space. */
static
void
fsp_free_extent(
/*============*/
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page offset in the extent */
	mtr_t*	mtr)	/* in: mtr */
{
	fsp_header_t*	header;
	xdes_t*		descr;

	ut_ad(mtr);

	header = fsp_get_space_header(space, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, page, mtr);

	if (xdes_get_state(descr, mtr) == XDES_FREE) {

		ut_print_buf(stderr, (byte*)descr - 500, 1000);

		ut_error;
	}

	xdes_init(descr, mtr);

	flst_add_last(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);
}

/**************************************************************************
Returns the nth inode slot on an inode page. */
UNIV_INLINE
fseg_inode_t*
fsp_seg_inode_page_get_nth_inode(
/*=============================*/
			/* out: segment inode */
	page_t*	page,	/* in: segment inode page */
	ulint	i,	/* in: inode index on page */
	mtr_t*	mtr __attribute__((unused))) /* in: mini-transaction handle */
{
	ut_ad(i < FSP_SEG_INODES_PER_PAGE);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));

	return(page + FSEG_ARR_OFFSET + FSEG_INODE_SIZE * i);
}

/**************************************************************************
Looks for a used segment inode on a segment inode page. */
static
ulint
fsp_seg_inode_page_find_used(
/*=========================*/
			/* out: segment inode index, or ULINT_UNDEFINED
			if not found */
	page_t*	page,	/* in: segment inode page */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	ulint		i;
	fseg_inode_t*	inode;

	for (i = 0; i < FSP_SEG_INODES_PER_PAGE; i++) {

		inode = fsp_seg_inode_page_get_nth_inode(page, i, mtr);

		if (ut_dulint_cmp(mach_read_from_8(inode + FSEG_ID),
						ut_dulint_zero) != 0) {
			/* This is used */

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Looks for an unused segment inode on a segment inode page. */
static
ulint
fsp_seg_inode_page_find_free(
/*=========================*/
			/* out: segment inode index, or ULINT_UNDEFINED
			if not found */
	page_t*	page,	/* in: segment inode page */
	ulint	j,	/* in: search forward starting from this index */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	ulint		i;
	fseg_inode_t*	inode;

	for (i = j; i < FSP_SEG_INODES_PER_PAGE; i++) {

		inode = fsp_seg_inode_page_get_nth_inode(page, i, mtr);

		if (ut_dulint_cmp(mach_read_from_8(inode + FSEG_ID),
						ut_dulint_zero) == 0) {
			/* This is unused */

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Allocates a new file segment inode page. */
static
ibool
fsp_alloc_seg_inode_page(
/*=====================*/
					/* out: TRUE if could be allocated */
	fsp_header_t*	space_header,	/* in: space header */
	mtr_t*		mtr)		/* in: mini-transaction handle */
{
	fseg_inode_t*	inode;
	page_t*		page;
	ulint		page_no;
	ulint		space;
	ulint		i;

	space = buf_frame_get_space_id(space_header);

	page_no = fsp_alloc_free_page(space, 0, mtr);

	if (page_no == FIL_NULL) {

		return(FALSE);
	}

	page = buf_page_get(space, page_no, RW_X_LATCH, mtr);

	buf_block_align(page)->check_index_page_at_flush = FALSE;

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_INODE,
						MLOG_2BYTES, mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */

	for (i = 0; i < FSP_SEG_INODES_PER_PAGE; i++) {

		inode = fsp_seg_inode_page_get_nth_inode(page, i, mtr);

		mlog_write_dulint(inode + FSEG_ID, ut_dulint_zero, mtr);
	}

	flst_add_last(space_header + FSP_SEG_INODES_FREE,
					page + FSEG_INODE_PAGE_NODE, mtr);
	return(TRUE);
}

/**************************************************************************
Allocates a new file segment inode. */
static
fseg_inode_t*
fsp_alloc_seg_inode(
/*================*/
					/* out: segment inode, or NULL if
					not enough space */
	fsp_header_t*	space_header,	/* in: space header */
	mtr_t*		mtr)		/* in: mini-transaction handle */
{
	ulint		page_no;
	page_t*		page;
	fseg_inode_t*	inode;
	ibool		success;
	ulint		n;

	if (flst_get_len(space_header + FSP_SEG_INODES_FREE, mtr) == 0) {
		/* Allocate a new segment inode page */

		success = fsp_alloc_seg_inode_page(space_header, mtr);

		if (!success) {

			return(NULL);
		}
	}

	page_no = flst_get_first(space_header + FSP_SEG_INODES_FREE, mtr).page;

	page = buf_page_get(buf_frame_get_space_id(space_header), page_no,
							RW_X_LATCH, mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */

	n = fsp_seg_inode_page_find_free(page, 0, mtr);

	ut_a(n != ULINT_UNDEFINED);

	inode = fsp_seg_inode_page_get_nth_inode(page, n, mtr);

	if (ULINT_UNDEFINED == fsp_seg_inode_page_find_free(page, n + 1,
			mtr)) {
		/* There are no other unused headers left on the page: move it
		to another list */

		flst_remove(space_header + FSP_SEG_INODES_FREE,
				page + FSEG_INODE_PAGE_NODE, mtr);

		flst_add_last(space_header + FSP_SEG_INODES_FULL,
				page + FSEG_INODE_PAGE_NODE, mtr);
	}

	return(inode);
}

/**************************************************************************
Frees a file segment inode. */
static
void
fsp_free_seg_inode(
/*===============*/
	ulint		space,	/* in: space id */
	fseg_inode_t*	inode,	/* in: segment inode */
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	page_t*		page;
	fsp_header_t*	space_header;

	page = buf_frame_align(inode);

	space_header = fsp_get_space_header(space, mtr);

	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	if (ULINT_UNDEFINED == fsp_seg_inode_page_find_free(page, 0, mtr)) {

		/* Move the page to another list */

		flst_remove(space_header + FSP_SEG_INODES_FULL,
				page + FSEG_INODE_PAGE_NODE, mtr);

		flst_add_last(space_header + FSP_SEG_INODES_FREE,
				page + FSEG_INODE_PAGE_NODE, mtr);
	}

	mlog_write_dulint(inode + FSEG_ID, ut_dulint_zero, mtr);
	mlog_write_ulint(inode + FSEG_MAGIC_N, 0, MLOG_4BYTES, mtr);

	if (ULINT_UNDEFINED == fsp_seg_inode_page_find_used(page, mtr)) {

		/* There are no other used headers left on the page: free it */

		flst_remove(space_header + FSP_SEG_INODES_FREE,
				page + FSEG_INODE_PAGE_NODE, mtr);

		fsp_free_page(space, buf_frame_get_page_no(page), mtr);
	}
}

/**************************************************************************
Returns the file segment inode, page x-latched. */
static
fseg_inode_t*
fseg_inode_get(
/*===========*/
				/* out: segment inode, page x-latched */
	fseg_header_t*	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr handle */
{
	fil_addr_t	inode_addr;
	fseg_inode_t*	inode;

	inode_addr.page = mach_read_from_4(header + FSEG_HDR_PAGE_NO);
	inode_addr.boffset = mach_read_from_2(header + FSEG_HDR_OFFSET);

	inode = fut_get_ptr(mach_read_from_4(header + FSEG_HDR_SPACE),
						inode_addr, RW_X_LATCH, mtr);

	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	return(inode);
}

/**************************************************************************
Gets the page number from the nth fragment page slot. */
UNIV_INLINE
ulint
fseg_get_nth_frag_page_no(
/*======================*/
				/* out: page number, FIL_NULL if not in use */
	fseg_inode_t*	inode,	/* in: segment inode */
	ulint		n,	/* in: slot index */
	mtr_t*		mtr __attribute__((unused))) /* in: mtr handle */
{
	ut_ad(inode && mtr);
	ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(inode),
							MTR_MEMO_PAGE_X_FIX));
	return(mach_read_from_4(inode + FSEG_FRAG_ARR
						+ n * FSEG_FRAG_SLOT_SIZE));
}

/**************************************************************************
Sets the page number in the nth fragment page slot. */
UNIV_INLINE
void
fseg_set_nth_frag_page_no(
/*======================*/
	fseg_inode_t*	inode,	/* in: segment inode */
	ulint		n,	/* in: slot index */
	ulint		page_no,/* in: page number to set */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ut_ad(inode && mtr);
	ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(inode),
							MTR_MEMO_PAGE_X_FIX));

	mlog_write_ulint(inode + FSEG_FRAG_ARR + n * FSEG_FRAG_SLOT_SIZE,
						page_no, MLOG_4BYTES, mtr);
}

/**************************************************************************
Finds a fragment page slot which is free. */
static
ulint
fseg_find_free_frag_page_slot(
/*==========================*/
				/* out: slot index; ULINT_UNDEFINED if none
				found */
	fseg_inode_t*	inode,	/* in: segment inode */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	i;
	ulint	page_no;

	ut_ad(inode && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		page_no = fseg_get_nth_frag_page_no(inode, i, mtr);

		if (page_no == FIL_NULL) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Finds a fragment page slot which is used and last in the array. */
static
ulint
fseg_find_last_used_frag_page_slot(
/*===============================*/
				/* out: slot index; ULINT_UNDEFINED if none
				found */
	fseg_inode_t*	inode,	/* in: segment inode */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	i;
	ulint	page_no;

	ut_ad(inode && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		page_no = fseg_get_nth_frag_page_no(inode,
					FSEG_FRAG_ARR_N_SLOTS - i - 1, mtr);

		if (page_no != FIL_NULL) {

			return(FSEG_FRAG_ARR_N_SLOTS - i - 1);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Calculates reserved fragment page slots. */
static
ulint
fseg_get_n_frag_pages(
/*==================*/
				/* out: number of fragment pages */
	fseg_inode_t*	inode,	/* in: segment inode */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	i;
	ulint	count	= 0;

	ut_ad(inode && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		if (FIL_NULL != fseg_get_nth_frag_page_no(inode, i, mtr)) {
			count++;
		}
	}

	return(count);
}

/**************************************************************************
Creates a new segment. */

page_t*
fseg_create_general(
/*================*/
			/* out: the page where the segment header is placed,
			x-latched, NULL if could not create segment
			because of lack of space */
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /* in: byte offset of the created segment header
			on the page */
	ibool	has_done_reservation, /* in: TRUE if the caller has already
			done the reservation for the pages with
			fsp_reserve_free_extents (at least 2 extents: one for
			the inode and the other for the segment) then there is
			no need to do the check for this individual
			operation */
	mtr_t*	mtr)	/* in: mtr */
{
	fsp_header_t*	space_header;
	fseg_inode_t*	inode;
	dulint		seg_id;
	fseg_header_t*	header = 0; /* remove warning */
	rw_lock_t*	latch;
	ibool		success;
	ulint		n_reserved;
	page_t*		ret		= NULL;
	ulint		i;

	ut_ad(mtr);

	if (page != 0) {
		header = byte_offset + buf_page_get(space, page, RW_X_LATCH,
									mtr);
	}

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex)
		|| mtr_memo_contains(mtr, fil_space_get_latch(space),
			MTR_MEMO_X_LOCK));
#endif /* UNIV_SYNC_DEBUG */
	latch = fil_space_get_latch(space);

	mtr_x_lock(latch, mtr);

	if (rw_lock_get_x_lock_count(latch) == 1) {
		/* This thread did not own the latch before this call: free
		excess pages from the insert buffer free list */

		if (space == 0) {
			ibuf_free_excess_pages(space);
		}
	}

	if (!has_done_reservation) {
		success = fsp_reserve_free_extents(&n_reserved, space, 2,
							FSP_NORMAL, mtr);
		if (!success) {
			return(NULL);
		}
	}

	space_header = fsp_get_space_header(space, mtr);

	inode = fsp_alloc_seg_inode(space_header, mtr);

	if (inode == NULL) {

		goto funct_exit;
	}

	/* Read the next segment id from space header and increment the
	value in space header */

	seg_id = mtr_read_dulint(space_header + FSP_SEG_ID, mtr);

	mlog_write_dulint(space_header + FSP_SEG_ID, ut_dulint_add(seg_id, 1),
							mtr);

	mlog_write_dulint(inode + FSEG_ID, seg_id, mtr);
	mlog_write_ulint(inode + FSEG_NOT_FULL_N_USED, 0, MLOG_4BYTES, mtr);

	flst_init(inode + FSEG_FREE, mtr);
	flst_init(inode + FSEG_NOT_FULL, mtr);
	flst_init(inode + FSEG_FULL, mtr);

	mlog_write_ulint(inode + FSEG_MAGIC_N, FSEG_MAGIC_N_VALUE,
							MLOG_4BYTES, mtr);
	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		fseg_set_nth_frag_page_no(inode, i, FIL_NULL, mtr);
	}

	if (page == 0) {
		page = fseg_alloc_free_page_low(space, inode, 0, FSP_UP, mtr);

		if (page == FIL_NULL) {

			fsp_free_seg_inode(space, inode, mtr);

			goto funct_exit;
		}

		header = byte_offset
			 + buf_page_get(space, page, RW_X_LATCH, mtr);
		mlog_write_ulint(header - byte_offset + FIL_PAGE_TYPE,
					FIL_PAGE_TYPE_SYS, MLOG_2BYTES, mtr);
	}

	mlog_write_ulint(header + FSEG_HDR_OFFSET,
			inode - buf_frame_align(inode), MLOG_2BYTES, mtr);

	mlog_write_ulint(header + FSEG_HDR_PAGE_NO,
			buf_frame_get_page_no(inode), MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSEG_HDR_SPACE, space, MLOG_4BYTES, mtr);

	ret = buf_frame_align(header);

funct_exit:
	if (!has_done_reservation) {

		fil_space_release_free_extents(space, n_reserved);
	}

	return(ret);
}

/**************************************************************************
Creates a new segment. */

page_t*
fseg_create(
/*========*/
			/* out: the page where the segment header is placed,
			x-latched, NULL if could not create segment
			because of lack of space */
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /* in: byte offset of the created segment header
			on the page */
	mtr_t*	mtr)	/* in: mtr */
{
	return(fseg_create_general(space, page, byte_offset, FALSE, mtr));
}

/**************************************************************************
Calculates the number of pages reserved by a segment, and how many pages are
currently used. */
static
ulint
fseg_n_reserved_pages_low(
/*======================*/
				/* out: number of reserved pages */
	fseg_inode_t*	inode,	/* in: segment inode */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	ret;

	ut_ad(inode && used && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(inode),
						MTR_MEMO_PAGE_X_FIX));

	*used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL, mtr)
		+ fseg_get_n_frag_pages(inode, mtr);

	ret = fseg_get_n_frag_pages(inode, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FREE, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_NOT_FULL, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL, mtr);

	return(ret);
}

/**************************************************************************
Calculates the number of pages reserved by a segment, and how many pages are
currently used. */

ulint
fseg_n_reserved_pages(
/*==================*/
				/* out: number of reserved pages */
	fseg_header_t*	header,	/* in: segment header */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint		ret;
	fseg_inode_t*	inode;
	ulint		space;

	space = buf_frame_get_space_id(header);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex)
		|| mtr_memo_contains(mtr, fil_space_get_latch(space),
			MTR_MEMO_X_LOCK));
#endif /* UNIV_SYNC_DEBUG */
	mtr_x_lock(fil_space_get_latch(space), mtr);

	inode = fseg_inode_get(header, mtr);

	ret = fseg_n_reserved_pages_low(inode, used, mtr);

	return(ret);
}

/*************************************************************************
Tries to fill the free list of a segment with consecutive free extents.
This happens if the segment is big enough to allow extents in the free list,
the free list is empty, and the extents can be allocated consecutively from
the hint onward. */
static
void
fseg_fill_free_list(
/*================*/
	fseg_inode_t*	inode,	/* in: segment inode */
	ulint		space,	/* in: space id */
	ulint		hint,	/* in: hint which extent would be good as
				the first extent */
	mtr_t*		mtr)	/* in: mtr */
{
	xdes_t*	descr;
	ulint	i;
	dulint	seg_id;
	ulint	reserved;
	ulint	used;

	ut_ad(inode && mtr);

	reserved = fseg_n_reserved_pages_low(inode, &used, mtr);

	if (reserved < FSEG_FREE_LIST_LIMIT * FSP_EXTENT_SIZE) {

		/* The segment is too small to allow extents in free list */

		return;
	}

	if (flst_get_len(inode + FSEG_FREE, mtr) > 0) {
		/* Free list is not empty */

		return;
	}

	for (i = 0; i < FSEG_FREE_LIST_MAX_LEN; i++) {
		descr = xdes_get_descriptor(space, hint, mtr);

		if ((descr == NULL) ||
			(XDES_FREE != xdes_get_state(descr, mtr))) {

			/* We cannot allocate the desired extent: stop */

			return;
		}

		descr = fsp_alloc_free_extent(space, hint, mtr);

		xdes_set_state(descr, XDES_FSEG, mtr);

		seg_id = mtr_read_dulint(inode + FSEG_ID, mtr);
		mlog_write_dulint(descr + XDES_ID, seg_id, mtr);

		flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
		hint += FSP_EXTENT_SIZE;
	}
}

/*************************************************************************
Allocates a free extent for the segment: looks first in the free list of the
segment, then tries to allocate from the space free list. NOTE that the extent
returned still resides in the segment free list, it is not yet taken off it! */
static
xdes_t*
fseg_alloc_free_extent(
/*===================*/
				/* out: allocated extent, still placed in the
				segment free list, NULL if could
				not be allocated */
	fseg_inode_t*	inode,	/* in: segment inode */
	ulint		space,	/* in: space id */
	mtr_t*		mtr)	/* in: mtr */
{
	xdes_t*		descr;
	dulint		seg_id;
	fil_addr_t	first;

	if (flst_get_len(inode + FSEG_FREE, mtr) > 0) {
		/* Segment free list is not empty, allocate from it */

		first = flst_get_first(inode + FSEG_FREE, mtr);

		descr = xdes_lst_get_descriptor(space, first, mtr);
	} else {
		/* Segment free list was empty, allocate from space */
		descr = fsp_alloc_free_extent(space, 0, mtr);

		if (descr == NULL) {

			return(NULL);
		}

		seg_id = mtr_read_dulint(inode + FSEG_ID, mtr);

		xdes_set_state(descr, XDES_FSEG, mtr);
		mlog_write_dulint(descr + XDES_ID, seg_id, mtr);
		flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);

		/* Try to fill the segment free list */
		fseg_fill_free_list(inode, space,
			xdes_get_offset(descr) + FSP_EXTENT_SIZE, mtr);
	}

	return(descr);
}

/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation. */
static
ulint
fseg_alloc_free_page_low(
/*=====================*/
				/* out: the allocated page number, FIL_NULL
				if no page could be allocated */
	ulint		space,	/* in: space */
	fseg_inode_t*	seg_inode, /* in: segment inode */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction, /* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr)	/* in: mtr handle */
{
	fsp_header_t*	space_header;
	ulint		space_size;
	dulint		seg_id;
	ulint		used;
	ulint		reserved;
	xdes_t*		descr;		/* extent of the hinted page */
	ulint		ret_page;	/* the allocated page offset, FIL_NULL
					if could not be allocated */
	xdes_t*		ret_descr;	/* the extent of the allocated page */
	page_t*		page;
	ibool		frag_page_allocated = FALSE;
	ibool		success;
	ulint		n;

	ut_ad(mtr);
	ut_ad((direction >= FSP_UP) && (direction <= FSP_NO_DIR));
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N) ==
							FSEG_MAGIC_N_VALUE);
	seg_id = mtr_read_dulint(seg_inode + FSEG_ID, mtr);

	ut_ad(ut_dulint_cmp(seg_id, ut_dulint_zero) > 0);

	reserved = fseg_n_reserved_pages_low(seg_inode, &used, mtr);

	space_header = fsp_get_space_header(space, mtr);

	descr = xdes_get_descriptor_with_space_hdr(space_header, space,
		hint, mtr);
	if (descr == NULL) {
		/* Hint outside space or too high above free limit: reset
		hint */
		hint = 0;
		descr = xdes_get_descriptor(space, hint, mtr);
	}

	/* In the big if-else below we look for ret_page and ret_descr */
	/*-------------------------------------------------------------*/
	if ((xdes_get_state(descr, mtr) == XDES_FSEG)
		   && (0 == ut_dulint_cmp(mtr_read_dulint(descr + XDES_ID,
							mtr), seg_id))
		   && (xdes_get_bit(descr, XDES_FREE_BIT,
				hint % FSP_EXTENT_SIZE, mtr) == TRUE)) {

		/* 1. We can take the hinted page
		=================================*/
		ret_descr = descr;
		ret_page = hint;
	/*-------------------------------------------------------------*/
	} else if ((xdes_get_state(descr, mtr) == XDES_FREE)
		   && ((reserved - used) < reserved / FSEG_FILLFACTOR)
		   && (used >= FSEG_FRAG_LIMIT)) {

		/* 2. We allocate the free extent from space and can take
		=========================================================
		the hinted page
		===============*/
		ret_descr = fsp_alloc_free_extent(space, hint, mtr);

		ut_a(ret_descr == descr);

		xdes_set_state(ret_descr, XDES_FSEG, mtr);
		mlog_write_dulint(ret_descr + XDES_ID, seg_id, mtr);
		flst_add_last(seg_inode + FSEG_FREE,
					ret_descr + XDES_FLST_NODE, mtr);

		/* Try to fill the segment free list */
		fseg_fill_free_list(seg_inode, space,
					hint + FSP_EXTENT_SIZE, mtr);
		ret_page = hint;
	/*-------------------------------------------------------------*/
	} else if ((direction != FSP_NO_DIR)
		   && ((reserved - used) < reserved / FSEG_FILLFACTOR)
		   && (used >= FSEG_FRAG_LIMIT)
		   && (NULL != (ret_descr =
			fseg_alloc_free_extent(seg_inode, space, mtr)))) {

		/* 3. We take any free extent (which was already assigned above
		===============================================================
		in the if-condition to ret_descr) and take the lowest or
		========================================================
		highest page in it, depending on the direction
		==============================================*/
		ret_page = xdes_get_offset(ret_descr);

		if (direction == FSP_DOWN) {
			ret_page += FSP_EXTENT_SIZE - 1;
		}
	/*-------------------------------------------------------------*/
	} else if ((xdes_get_state(descr, mtr) == XDES_FSEG)
		   && (0 == ut_dulint_cmp(mtr_read_dulint(descr + XDES_ID,
							mtr), seg_id))
		   && (!xdes_is_full(descr, mtr))) {

		/* 4. We can take the page from the same extent as the
		======================================================
		hinted page (and the extent already belongs to the
		==================================================
		segment)
		========*/
		ret_descr = descr;
		ret_page = xdes_get_offset(ret_descr) +
				xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE,
					hint % FSP_EXTENT_SIZE, mtr);
	/*-------------------------------------------------------------*/
	} else if (reserved - used > 0) {
		/* 5. We take any unused page from the segment
		==============================================*/
		fil_addr_t	first;

		if (flst_get_len(seg_inode + FSEG_NOT_FULL, mtr) > 0) {
			first = flst_get_first(seg_inode + FSEG_NOT_FULL,
									mtr);
		} else if (flst_get_len(seg_inode + FSEG_FREE, mtr) > 0) {
			first = flst_get_first(seg_inode + FSEG_FREE, mtr);
		} else {
			ut_error;
			return(FIL_NULL);
		}

		ret_descr = xdes_lst_get_descriptor(space, first, mtr);
		ret_page = xdes_get_offset(ret_descr) +
				xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE,
								0, mtr);
	/*-------------------------------------------------------------*/
	} else if (used < FSEG_FRAG_LIMIT) {
		/* 6. We allocate an individual page from the space
		===================================================*/
		ret_page = fsp_alloc_free_page(space, hint, mtr);
		ret_descr = NULL;

		frag_page_allocated = TRUE;

		if (ret_page != FIL_NULL) {
			/* Put the page in the fragment page array of the
			segment */
			n = fseg_find_free_frag_page_slot(seg_inode, mtr);
			ut_a(n != FIL_NULL);

			fseg_set_nth_frag_page_no(seg_inode, n, ret_page,
									mtr);
		}
	/*-------------------------------------------------------------*/
	} else {
		/* 7. We allocate a new extent and take its first page
		======================================================*/
		ret_descr = fseg_alloc_free_extent(seg_inode, space, mtr);

		if (ret_descr == NULL) {
			ret_page = FIL_NULL;
		} else {
			ret_page = xdes_get_offset(ret_descr);
		}
	}

	if (ret_page == FIL_NULL) {
		/* Page could not be allocated */

		return(FIL_NULL);
	}

	if (space != 0) {
		space_size = fil_space_get_size(space);

		if (space_size <= ret_page) {
			/* It must be that we are extending a single-table
			tablespace whose size is still < 64 pages */

			if (ret_page >= FSP_EXTENT_SIZE) {
				fprintf(stderr,
"InnoDB: Error (2): trying to extend a single-table tablespace %lu\n"
"InnoDB: by single page(s) though the space size %lu. Page no %lu.\n",
					(ulong) space, (ulong) space_size,
					(ulong) ret_page);
				return(FIL_NULL);
			}

			success = fsp_try_extend_data_file_with_pages(space,
						ret_page, space_header, mtr);
			if (!success) {
				/* No disk space left */
				return(FIL_NULL);
			}
		}
	}

	if (!frag_page_allocated) {
		/* Initialize the allocated page to buffer pool, so that it
		can be obtained immediately with buf_page_get without need
		for a disk read */

		page = buf_page_create(space, ret_page, mtr);

		ut_a(page == buf_page_get(space, ret_page, RW_X_LATCH, mtr));

#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page, SYNC_FSP_PAGE);
#endif /* UNIV_SYNC_DEBUG */

		/* The prior contents of the page should be ignored */
		fsp_init_file_page(page, mtr);

		/* At this point we know the extent and the page offset.
		The extent is still in the appropriate list (FSEG_NOT_FULL
		or FSEG_FREE), and the page is not yet marked as used. */

		ut_ad(xdes_get_descriptor(space, ret_page, mtr) == ret_descr);
		ut_ad(xdes_get_bit(ret_descr, XDES_FREE_BIT,
				ret_page % FSP_EXTENT_SIZE, mtr) == TRUE);

		fseg_mark_page_used(seg_inode, space, ret_page, mtr);
	}

	buf_reset_check_index_page_at_flush(space, ret_page);

	return(ret_page);
}

/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation. */

ulint
fseg_alloc_free_page_general(
/*=========================*/
				/* out: allocated page offset, FIL_NULL if no
				page could be allocated */
	fseg_header_t*	seg_header,/* in: segment header */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction,/* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	ibool		has_done_reservation, /* in: TRUE if the caller has
				already done the reservation for the page
				with fsp_reserve_free_extents, then there
				is no need to do the check for this individual
				page */
	mtr_t*		mtr)	/* in: mtr handle */
{
	fseg_inode_t*	inode;
	ulint		space;
	rw_lock_t*	latch;
	ibool		success;
	ulint		page_no;
	ulint		n_reserved;

	space = buf_frame_get_space_id(seg_header);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex)
		|| mtr_memo_contains(mtr, fil_space_get_latch(space),
			MTR_MEMO_X_LOCK));
#endif /* UNIV_SYNC_DEBUG */
	latch = fil_space_get_latch(space);

	mtr_x_lock(latch, mtr);

	if (rw_lock_get_x_lock_count(latch) == 1) {
		/* This thread did not own the latch before this call: free
		excess pages from the insert buffer free list */

		if (space == 0) {
			ibuf_free_excess_pages(space);
		}
	}

	inode = fseg_inode_get(seg_header, mtr);

	if (!has_done_reservation) {
		success = fsp_reserve_free_extents(&n_reserved, space, 2,
							FSP_NORMAL, mtr);
		if (!success) {
			return(FIL_NULL);
		}
	}

	page_no = fseg_alloc_free_page_low(buf_frame_get_space_id(inode),
					inode, hint, direction, mtr);
	if (!has_done_reservation) {
		fil_space_release_free_extents(space, n_reserved);
	}

	return(page_no);
}

/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation. */

ulint
fseg_alloc_free_page(
/*=================*/
				/* out: allocated page offset, FIL_NULL if no
				page could be allocated */
	fseg_header_t*	seg_header,/* in: segment header */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction,/* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr)	/* in: mtr handle */
{
	return(fseg_alloc_free_page_general(seg_header, hint, direction,
								FALSE, mtr));
}

/**************************************************************************
Checks that we have at least 2 frag pages free in the first extent of a
single-table tablespace, and they are also physically initialized to the data
file. That is we have already extended the data file so that those pages are
inside the data file. If not, this function extends the tablespace with
pages. */
static
ibool
fsp_reserve_free_pages(
/*===================*/
					/* out: TRUE if there were >= 3 free
					pages, or we were able to extend */
	ulint		space,		/* in: space id, must be != 0 */
	fsp_header_t*	space_header,	/* in: header of that space,
					x-latched */
	ulint		size,		/* in: size of the tablespace in pages,
					must be < FSP_EXTENT_SIZE / 2 */
	mtr_t*		mtr)		/* in: mtr */
{
	xdes_t*	descr;
	ulint	n_used;

	ut_a(space != 0);
	ut_a(size < FSP_EXTENT_SIZE / 2);

	descr = xdes_get_descriptor_with_space_hdr(space_header, space, 0,
									mtr);
	n_used = xdes_get_n_used(descr, mtr);

	ut_a(n_used <= size);

	if (size >= n_used + 2) {

		return(TRUE);
	}

	return(fsp_try_extend_data_file_with_pages(space, n_used + 1,
							  space_header, mtr));
}

/**************************************************************************
Reserves free pages from a tablespace. All mini-transactions which may
use several pages from the tablespace should call this function beforehand
and reserve enough free extents so that they certainly will be able
to do their operation, like a B-tree page split, fully. Reservations
must be released with function fil_space_release_free_extents!

The alloc_type below has the following meaning: FSP_NORMAL means an
operation which will probably result in more space usage, like an
insert in a B-tree; FSP_UNDO means allocation to undo logs: if we are
deleting rows, then this allocation will in the long run result in
less space usage (after a purge); FSP_CLEANING means allocation done
in a physical record delete (like in a purge) or other cleaning operation
which will result in less space usage in the long run. We prefer the latter
two types of allocation: when space is scarce, FSP_NORMAL allocations
will not succeed, but the latter two allocations will succeed, if possible.
The purpose is to avoid dead end where the database is full but the
user cannot free any space because these freeing operations temporarily
reserve some space.

Single-table tablespaces whose size is < 32 pages are a special case. In this
function we would liberally reserve several 64 page extents for every page
split or merge in a B-tree. But we do not want to waste disk space if the table
only occupies < 32 pages. That is why we apply different rules in that special
case, just ensuring that there are 3 free pages available. */

ibool
fsp_reserve_free_extents(
/*=====================*/
			/* out: TRUE if we were able to make the reservation */
	ulint*	n_reserved,/* out: number of extents actually reserved; if we
			return TRUE and the tablespace size is < 64 pages,
			then this can be 0, otherwise it is n_ext */
	ulint	space,	/* in: space id */
	ulint	n_ext,	/* in: number of extents to reserve */
	ulint	alloc_type,/* in: FSP_NORMAL, FSP_UNDO, or FSP_CLEANING */
	mtr_t*	mtr)	/* in: mtr */
{
	fsp_header_t*	space_header;
	rw_lock_t*	latch;
	ulint		n_free_list_ext;
	ulint		free_limit;
	ulint		size;
	ulint		n_free;
	ulint		n_free_up;
	ulint		reserve;
	ibool		success;
	ulint		n_pages_added;

	ut_ad(mtr);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex)
		|| mtr_memo_contains(mtr, fil_space_get_latch(space),
			MTR_MEMO_X_LOCK));
#endif /* UNIV_SYNC_DEBUG */
	*n_reserved = n_ext;

	latch = fil_space_get_latch(space);

	mtr_x_lock(latch, mtr);

	space_header = fsp_get_space_header(space, mtr);
try_again:
	size = mtr_read_ulint(space_header + FSP_SIZE, MLOG_4BYTES, mtr);

	if (size < FSP_EXTENT_SIZE / 2) {
		/* Use different rules for small single-table tablespaces */
		*n_reserved = 0;
		return(fsp_reserve_free_pages(space, space_header, size, mtr));
	}

	n_free_list_ext = flst_get_len(space_header + FSP_FREE, mtr);

	free_limit = mtr_read_ulint(space_header + FSP_FREE_LIMIT,
							MLOG_4BYTES, mtr);

	/* Below we play safe when counting free extents above the free limit:
	some of them will contain extent descriptor pages, and therefore
	will not be free extents */

	n_free_up = (size - free_limit) / FSP_EXTENT_SIZE;

	if (n_free_up > 0) {
		n_free_up--;
		n_free_up = n_free_up - n_free_up
				/ (XDES_DESCRIBED_PER_PAGE / FSP_EXTENT_SIZE);
	}

	n_free = n_free_list_ext + n_free_up;

	if (alloc_type == FSP_NORMAL) {
		/* We reserve 1 extent + 0.5 % of the space size to undo logs
		and 1 extent + 0.5 % to cleaning operations; NOTE: this source
		code is duplicated in the function below! */

		reserve = 2 + ((size / FSP_EXTENT_SIZE) * 2) / 200;

		if (n_free <= reserve + n_ext) {

			goto try_to_extend;
		}
	} else if (alloc_type == FSP_UNDO) {
		/* We reserve 0.5 % of the space size to cleaning operations */

		reserve = 1 + ((size / FSP_EXTENT_SIZE) * 1) / 200;

		if (n_free <= reserve + n_ext) {

			goto try_to_extend;
		}
	} else {
		ut_a(alloc_type == FSP_CLEANING);
	}

	success = fil_space_reserve_free_extents(space, n_free, n_ext);

	if (success) {
		return(TRUE);
	}
try_to_extend:
	success = fsp_try_extend_data_file(&n_pages_added, space,
							space_header, mtr);
	if (success && n_pages_added > 0) {

		goto try_again;
	}

	return(FALSE);
}

/**************************************************************************
This function should be used to get information on how much we still
will be able to insert new data to the database without running out the
tablespace. Only free extents are taken into account and we also subtract
the safety margin required by the above function fsp_reserve_free_extents. */

ulint
fsp_get_available_space_in_free_extents(
/*====================================*/
			/* out: available space in kB */
	ulint	space)	/* in: space id */
{
	fsp_header_t*	space_header;
	ulint		n_free_list_ext;
	ulint		free_limit;
	ulint		size;
	ulint		n_free;
	ulint		n_free_up;
	ulint		reserve;
	rw_lock_t*	latch;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	mtr_start(&mtr);

	latch = fil_space_get_latch(space);

	mtr_x_lock(latch, &mtr);

	space_header = fsp_get_space_header(space, &mtr);

	size = mtr_read_ulint(space_header + FSP_SIZE, MLOG_4BYTES, &mtr);

	n_free_list_ext = flst_get_len(space_header + FSP_FREE, &mtr);

	free_limit = mtr_read_ulint(space_header + FSP_FREE_LIMIT,
							MLOG_4BYTES, &mtr);
	mtr_commit(&mtr);

	if (size < FSP_EXTENT_SIZE) {
		ut_a(space != 0);	/* This must be a single-table
					tablespace */

		return(0);		/* TODO: count free frag pages and
					return a value based on that */
	}

	/* Below we play safe when counting free extents above the free limit:
	some of them will contain extent descriptor pages, and therefore
	will not be free extents */

	n_free_up = (size - free_limit) / FSP_EXTENT_SIZE;

	if (n_free_up > 0) {
		n_free_up--;
		n_free_up = n_free_up - n_free_up
				/ (XDES_DESCRIBED_PER_PAGE / FSP_EXTENT_SIZE);
	}

	n_free = n_free_list_ext + n_free_up;

	/* We reserve 1 extent + 0.5 % of the space size to undo logs
	and 1 extent + 0.5 % to cleaning operations; NOTE: this source
	code is duplicated in the function above! */

	reserve = 2 + ((size / FSP_EXTENT_SIZE) * 2) / 200;

	if (reserve > n_free) {
		return(0);
	}

	return(((n_free - reserve) * FSP_EXTENT_SIZE)
					* (UNIV_PAGE_SIZE / 1024));
}

/************************************************************************
Marks a page used. The page must reside within the extents of the given
segment. */
static
void
fseg_mark_page_used(
/*================*/
	fseg_inode_t*	seg_inode,/* in: segment inode */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr)	/* in: mtr */
{
	xdes_t*	descr;
	ulint	not_full_n_used;

	ut_ad(seg_inode && mtr);

	descr = xdes_get_descriptor(space, page, mtr);

	ut_ad(mtr_read_ulint(seg_inode + FSEG_ID, MLOG_4BYTES, mtr) ==
		mtr_read_ulint(descr + XDES_ID, MLOG_4BYTES, mtr));

	if (xdes_is_free(descr, mtr)) {
		/* We move the extent from the free list to the
		NOT_FULL list */
		flst_remove(seg_inode + FSEG_FREE, descr + XDES_FLST_NODE,
									mtr);
		flst_add_last(seg_inode + FSEG_NOT_FULL,
						descr + XDES_FLST_NODE, mtr);
	}

	ut_ad(xdes_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr)
								== TRUE);
	/* We mark the page as used */
	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, FALSE, mtr);

	not_full_n_used = mtr_read_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
							MLOG_4BYTES, mtr);
	not_full_n_used++;
	mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED, not_full_n_used,
							MLOG_4BYTES, mtr);
	if (xdes_is_full(descr, mtr)) {
		/* We move the extent from the NOT_FULL list to the
		FULL list */
		flst_remove(seg_inode + FSEG_NOT_FULL,
						descr + XDES_FLST_NODE, mtr);
		flst_add_last(seg_inode + FSEG_FULL,
						descr + XDES_FLST_NODE, mtr);

		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
					not_full_n_used - FSP_EXTENT_SIZE,
							MLOG_4BYTES, mtr);
	}
}

/**************************************************************************
Frees a single page of a segment. */
static
void
fseg_free_page_low(
/*===============*/
	fseg_inode_t*	seg_inode, /* in: segment inode */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr)	/* in: mtr handle */
{
	xdes_t*	descr;
	ulint	not_full_n_used;
	ulint	state;
	dulint	descr_id;
	dulint	seg_id;
	ulint	i;

	ut_ad(seg_inode && mtr);
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N) ==
							FSEG_MAGIC_N_VALUE);

	/* Drop search system page hash index if the page is found in
	the pool and is hashed */

	btr_search_drop_page_hash_when_freed(space, page);

	descr = xdes_get_descriptor(space, page, mtr);

	ut_a(descr);
	if (xdes_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr)) {
		fputs("InnoDB: Dump of the tablespace extent descriptor: ",
			stderr);
		ut_print_buf(stderr, descr, 40);

		fprintf(stderr, "\n"
"InnoDB: Serious error! InnoDB is trying to free page %lu\n"
"InnoDB: though it is already marked as free in the tablespace!\n"
"InnoDB: The tablespace free space info is corrupt.\n"
"InnoDB: You may need to dump your InnoDB tables and recreate the whole\n"
"InnoDB: database!\n", (ulong) page);
	crash:
		fputs(
"InnoDB: Please refer to\n"
"InnoDB: http://dev.mysql.com/doc/refman/5.0/en/forcing-recovery.html\n"
"InnoDB: about forcing recovery.\n", stderr);
		ut_error;
	}

	state = xdes_get_state(descr, mtr);

	if (state != XDES_FSEG) {
		/* The page is in the fragment pages of the segment */

		for (i = 0;; i++) {
			if (fseg_get_nth_frag_page_no(seg_inode, i, mtr)
				== page) {

				fseg_set_nth_frag_page_no(seg_inode, i,
							FIL_NULL, mtr);
				break;
			}
		}

		fsp_free_page(space, page, mtr);

		return;
	}

	/* If we get here, the page is in some extent of the segment */

	descr_id = mtr_read_dulint(descr + XDES_ID, mtr);
	seg_id = mtr_read_dulint(seg_inode + FSEG_ID, mtr);
/*
	fprintf(stderr,
"InnoDB: InnoDB is freeing space %lu page %lu,\n"
"InnoDB: which belongs to descr seg %lu %lu\n"
"InnoDB: segment %lu %lu.\n",
		   space, page,
		   ut_dulint_get_high(descr_id),
		   ut_dulint_get_low(descr_id),
		   ut_dulint_get_high(seg_id),
		   ut_dulint_get_low(seg_id));
*/
	if (0 != ut_dulint_cmp(descr_id, seg_id)) {
		fputs("InnoDB: Dump of the tablespace extent descriptor: ",
			stderr);
		ut_print_buf(stderr, descr, 40);
		fputs("\nInnoDB: Dump of the segment inode: ", stderr);
		ut_print_buf(stderr, seg_inode, 40);
		putc('\n', stderr);

		fprintf(stderr,
"InnoDB: Serious error: InnoDB is trying to free space %lu page %lu,\n"
"InnoDB: which does not belong to segment %lu %lu but belongs\n"
"InnoDB: to segment %lu %lu.\n",
		   (ulong) space, (ulong) page,
		   (ulong) ut_dulint_get_high(descr_id),
		   (ulong) ut_dulint_get_low(descr_id),
		   (ulong) ut_dulint_get_high(seg_id),
		   (ulong) ut_dulint_get_low(seg_id));
		goto crash;
	}

	not_full_n_used = mtr_read_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
							MLOG_4BYTES, mtr);
	if (xdes_is_full(descr, mtr)) {
		/* The fragment is full: move it to another list */
		flst_remove(seg_inode + FSEG_FULL,
						descr + XDES_FLST_NODE, mtr);
		flst_add_last(seg_inode + FSEG_NOT_FULL,
						descr + XDES_FLST_NODE, mtr);
		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
					not_full_n_used + FSP_EXTENT_SIZE - 1,
							MLOG_4BYTES, mtr);
	} else {
		ut_a(not_full_n_used > 0);
		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
					not_full_n_used - 1, MLOG_4BYTES, mtr);
	}

	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);

	if (xdes_is_free(descr, mtr)) {
		/* The extent has become free: free it to space */
		flst_remove(seg_inode + FSEG_NOT_FULL,
						descr + XDES_FLST_NODE, mtr);
		fsp_free_extent(space, page, mtr);
	}
}

/**************************************************************************
Frees a single page of a segment. */

void
fseg_free_page(
/*===========*/
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr)	/* in: mtr handle */
{
	fseg_inode_t*	seg_inode;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex)
		|| mtr_memo_contains(mtr, fil_space_get_latch(space),
			MTR_MEMO_X_LOCK));
#endif /* UNIV_SYNC_DEBUG */
	mtr_x_lock(fil_space_get_latch(space), mtr);

	seg_inode = fseg_inode_get(seg_header, mtr);

	fseg_free_page_low(seg_inode, space, page, mtr);

#ifdef UNIV_DEBUG_FILE_ACCESSES
	buf_page_set_file_page_was_freed(space, page);
#endif
}

/**************************************************************************
Frees an extent of a segment to the space free list. */
static
void
fseg_free_extent(
/*=============*/
	fseg_inode_t*	seg_inode, /* in: segment inode */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: a page in the extent */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	first_page_in_extent;
	xdes_t*	descr;
	ulint	not_full_n_used;
	ulint	descr_n_used;
	ulint	i;

	ut_ad(seg_inode && mtr);

	descr = xdes_get_descriptor(space, page, mtr);

	ut_a(xdes_get_state(descr, mtr) == XDES_FSEG);
	ut_a(0 == ut_dulint_cmp(
		mtr_read_dulint(descr + XDES_ID, mtr),
		mtr_read_dulint(seg_inode + FSEG_ID, mtr)));

	first_page_in_extent = page - (page % FSP_EXTENT_SIZE);

	for (i = 0; i < FSP_EXTENT_SIZE; i++) {
		if (FALSE == xdes_get_bit(descr, XDES_FREE_BIT, i, mtr)) {

			/* Drop search system page hash index if the page is
			found in the pool and is hashed */

			btr_search_drop_page_hash_when_freed(space,
					first_page_in_extent + i);
		}
	}

	if (xdes_is_full(descr, mtr)) {
		flst_remove(seg_inode + FSEG_FULL,
				descr + XDES_FLST_NODE, mtr);
	} else if (xdes_is_free(descr, mtr)) {
		flst_remove(seg_inode + FSEG_FREE,
				descr + XDES_FLST_NODE, mtr);
	} else {
		flst_remove(seg_inode + FSEG_NOT_FULL,
				descr + XDES_FLST_NODE, mtr);

		not_full_n_used = mtr_read_ulint(
					seg_inode + FSEG_NOT_FULL_N_USED,
					MLOG_4BYTES, mtr);

		descr_n_used = xdes_get_n_used(descr, mtr);
		ut_a(not_full_n_used >= descr_n_used);
		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
				not_full_n_used - descr_n_used,
				MLOG_4BYTES, mtr);
	}

	fsp_free_extent(space, page, mtr);

#ifdef UNIV_DEBUG_FILE_ACCESSES
	for (i = 0; i < FSP_EXTENT_SIZE; i++) {

		buf_page_set_file_page_was_freed(space,
						first_page_in_extent + i);
	}
#endif
}

/**************************************************************************
Frees part of a segment. This function can be used to free a segment by
repeatedly calling this function in different mini-transactions. Doing
the freeing in a single mini-transaction might result in too big a
mini-transaction. */

ibool
fseg_free_step(
/*===========*/
				/* out: TRUE if freeing completed */
	fseg_header_t*	header,	/* in, own: segment header; NOTE: if the header
				resides on the first page of the frag list
				of the segment, this pointer becomes obsolete
				after the last freeing step */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint		n;
	ulint		page;
	xdes_t*		descr;
	fseg_inode_t*	inode;
	ulint		space;

	space = buf_frame_get_space_id(header);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex)
		|| mtr_memo_contains(mtr, fil_space_get_latch(space),
			MTR_MEMO_X_LOCK));
#endif /* UNIV_SYNC_DEBUG */
	mtr_x_lock(fil_space_get_latch(space), mtr);

	descr = xdes_get_descriptor(space, buf_frame_get_page_no(header), mtr);

	/* Check that the header resides on a page which has not been
	freed yet */

	ut_a(descr);
	ut_a(xdes_get_bit(descr, XDES_FREE_BIT, buf_frame_get_page_no(header)
					% FSP_EXTENT_SIZE, mtr) == FALSE);
	inode = fseg_inode_get(header, mtr);

	descr = fseg_get_first_extent(inode, mtr);

	if (descr != NULL) {
		/* Free the extent held by the segment */
		page = xdes_get_offset(descr);

		fseg_free_extent(inode, space, page, mtr);

		return(FALSE);
	}

	/* Free a frag page */
	n = fseg_find_last_used_frag_page_slot(inode, mtr);

	if (n == ULINT_UNDEFINED) {
		/* Freeing completed: free the segment inode */
		fsp_free_seg_inode(space, inode, mtr);

		return(TRUE);
	}

	fseg_free_page_low(inode, space,
			fseg_get_nth_frag_page_no(inode, n, mtr), mtr);

	n = fseg_find_last_used_frag_page_slot(inode, mtr);

	if (n == ULINT_UNDEFINED) {
		/* Freeing completed: free the segment inode */
		fsp_free_seg_inode(space, inode, mtr);

		return(TRUE);
	}

	return(FALSE);
}

/**************************************************************************
Frees part of a segment. Differs from fseg_free_step because this function
leaves the header page unfreed. */

ibool
fseg_free_step_not_header(
/*======================*/
				/* out: TRUE if freeing completed, except the
				header page */
	fseg_header_t*	header,	/* in: segment header which must reside on
				the first fragment page of the segment */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint		n;
	ulint		page;
	xdes_t*		descr;
	fseg_inode_t*	inode;
	ulint		space;
	ulint		page_no;

	space = buf_frame_get_space_id(header);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex)
		|| mtr_memo_contains(mtr, fil_space_get_latch(space),
			MTR_MEMO_X_LOCK));
#endif /* UNIV_SYNC_DEBUG */
	mtr_x_lock(fil_space_get_latch(space), mtr);

	inode = fseg_inode_get(header, mtr);

	descr = fseg_get_first_extent(inode, mtr);

	if (descr != NULL) {
		/* Free the extent held by the segment */
		page = xdes_get_offset(descr);

		fseg_free_extent(inode, space, page, mtr);

		return(FALSE);
	}

	/* Free a frag page */

	n = fseg_find_last_used_frag_page_slot(inode, mtr);

	if (n == ULINT_UNDEFINED) {
		ut_error;
	}

	page_no = fseg_get_nth_frag_page_no(inode, n, mtr);

	if (page_no == buf_frame_get_page_no(header)) {

		return(TRUE);
	}

	fseg_free_page_low(inode, space, page_no, mtr);

	return(FALSE);
}

/***********************************************************************
Frees a segment. The freeing is performed in several mini-transactions,
so that there is no danger of bufferfixing too many buffer pages. */

void
fseg_free(
/*======*/
	ulint	space,	/* in: space id */
	ulint	page_no,/* in: page number where the segment header is
			placed */
	ulint	offset) /* in: byte offset of the segment header on that
			page */
{
	mtr_t		mtr;
	ibool		finished;
	fseg_header_t*	header;
	fil_addr_t	addr;

	addr.page = page_no;
	addr.boffset = offset;

	for (;;) {
		mtr_start(&mtr);

		header = fut_get_ptr(space, addr, RW_X_LATCH, &mtr);

		finished = fseg_free_step(header, &mtr);

		mtr_commit(&mtr);

		if (finished) {

			return;
		}
	}
}

/**************************************************************************
Returns the first extent descriptor for a segment. We think of the extent
lists of the segment catenated in the order FSEG_FULL -> FSEG_NOT_FULL
-> FSEG_FREE. */
static
xdes_t*
fseg_get_first_extent(
/*==================*/
				/* out: the first extent descriptor, or NULL if
				none */
	fseg_inode_t*	inode,	/* in: segment inode */
	mtr_t*		mtr)	/* in: mtr */
{
	fil_addr_t	first;
	ulint		space;
	xdes_t*		descr;

	ut_ad(inode && mtr);

	space = buf_frame_get_space_id(inode);

	first = fil_addr_null;

	if (flst_get_len(inode + FSEG_FULL, mtr) > 0) {

		first = flst_get_first(inode + FSEG_FULL, mtr);

	} else if (flst_get_len(inode + FSEG_NOT_FULL, mtr) > 0) {

		first = flst_get_first(inode + FSEG_NOT_FULL, mtr);

	} else if (flst_get_len(inode + FSEG_FREE, mtr) > 0) {

		first = flst_get_first(inode + FSEG_FREE, mtr);
	}

	if (first.page == FIL_NULL) {

		return(NULL);
	}
	descr = xdes_lst_get_descriptor(space, first, mtr);

	return(descr);
}

/***********************************************************************
Validates a segment. */
static
ibool
fseg_validate_low(
/*==============*/
				/* out: TRUE if ok */
	fseg_inode_t*	inode, /* in: segment inode */
	mtr_t*		mtr2)	/* in: mtr */
{
	ulint		space;
	dulint		seg_id;
	mtr_t		mtr;
	xdes_t*		descr;
	fil_addr_t	node_addr;
	ulint		n_used		= 0;
	ulint		n_used2		= 0;

	ut_ad(mtr_memo_contains(mtr2, buf_block_align(inode),
							MTR_MEMO_PAGE_X_FIX));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	space = buf_frame_get_space_id(inode);

	seg_id = mtr_read_dulint(inode + FSEG_ID, mtr2);
	n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED,
							MLOG_4BYTES, mtr2);
	flst_validate(inode + FSEG_FREE, mtr2);
	flst_validate(inode + FSEG_NOT_FULL, mtr2);
	flst_validate(inode + FSEG_FULL, mtr2);

	/* Validate FSEG_FREE list */
	node_addr = flst_get_first(inode + FSEG_FREE, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space), &mtr);

		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == 0);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(0 == ut_dulint_cmp(
			mtr_read_dulint(descr + XDES_ID, &mtr), seg_id));

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSEG_NOT_FULL list */

	node_addr = flst_get_first(inode + FSEG_NOT_FULL, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space), &mtr);

		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) > 0);
		ut_a(xdes_get_n_used(descr, &mtr) < FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(0 == ut_dulint_cmp(
			mtr_read_dulint(descr + XDES_ID, &mtr), seg_id));

		n_used2 += xdes_get_n_used(descr, &mtr);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSEG_FULL list */

	node_addr = flst_get_first(inode + FSEG_FULL, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space), &mtr);

		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(0 == ut_dulint_cmp(
			mtr_read_dulint(descr + XDES_ID, &mtr), seg_id));

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	ut_a(n_used == n_used2);

	return(TRUE);
}

/***********************************************************************
Validates a segment. */

ibool
fseg_validate(
/*==========*/
				/* out: TRUE if ok */
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr2)	/* in: mtr */
{
	fseg_inode_t*	inode;
	ibool		ret;
	ulint		space;

	space = buf_frame_get_space_id(header);

	mtr_x_lock(fil_space_get_latch(space), mtr2);

	inode = fseg_inode_get(header, mtr2);

	ret = fseg_validate_low(inode, mtr2);

	return(ret);
}

/***********************************************************************
Writes info of a segment. */
static
void
fseg_print_low(
/*===========*/
	fseg_inode_t*	inode, /* in: segment inode */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	space;
	ulint	seg_id_low;
	ulint	seg_id_high;
	ulint	n_used;
	ulint	n_frag;
	ulint	n_free;
	ulint	n_not_full;
	ulint	n_full;
	ulint	reserved;
	ulint	used;
	ulint	page_no;
	dulint	 d_var;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(inode),
							MTR_MEMO_PAGE_X_FIX));
	space = buf_frame_get_space_id(inode);
	page_no = buf_frame_get_page_no(inode);

	reserved = fseg_n_reserved_pages_low(inode, &used, mtr);

	d_var = mtr_read_dulint(inode + FSEG_ID, mtr);

	seg_id_low = ut_dulint_get_low(d_var);
	seg_id_high = ut_dulint_get_high(d_var);

	n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED,
							MLOG_4BYTES, mtr);
	n_frag = fseg_get_n_frag_pages(inode, mtr);
	n_free = flst_get_len(inode + FSEG_FREE, mtr);
	n_not_full = flst_get_len(inode + FSEG_NOT_FULL, mtr);
	n_full = flst_get_len(inode + FSEG_FULL, mtr);

	fprintf(stderr,
"SEGMENT id %lu %lu space %lu; page %lu; res %lu used %lu; full ext %lu\n"
"fragm pages %lu; free extents %lu; not full extents %lu: pages %lu\n",
		(ulong) seg_id_high, (ulong) seg_id_low, (ulong) space, (ulong) page_no,
		(ulong) reserved, (ulong) used, (ulong) n_full,
		(ulong) n_frag, (ulong) n_free, (ulong) n_not_full,
		(ulong) n_used);
}

/***********************************************************************
Writes info of a segment. */

void
fseg_print(
/*=======*/
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr)	/* in: mtr */
{
	fseg_inode_t*	inode;
	ulint		space;

	space = buf_frame_get_space_id(header);

	mtr_x_lock(fil_space_get_latch(space), mtr);

	inode = fseg_inode_get(header, mtr);

	fseg_print_low(inode, mtr);
}

/***********************************************************************
Validates the file space system and its segments. */

ibool
fsp_validate(
/*=========*/
			/* out: TRUE if ok */
	ulint	space)	/* in: space id */
{
	fsp_header_t*	header;
	fseg_inode_t*	seg_inode;
	page_t*		seg_inode_page;
	ulint		size;
	ulint		free_limit;
	ulint		frag_n_used;
	mtr_t		mtr;
	mtr_t		mtr2;
	xdes_t*		descr;
	fil_addr_t	node_addr;
	fil_addr_t	next_node_addr;
	ulint		descr_count	= 0;
	ulint		n_used		= 0;
	ulint		n_used2		= 0;
	ulint		n_full_frag_pages;
	ulint		n;
	ulint		seg_inode_len_free;
	ulint		seg_inode_len_full;

	/* Start first a mini-transaction mtr2 to lock out all other threads
	from the fsp system */
	mtr_start(&mtr2);
	mtr_x_lock(fil_space_get_latch(space), &mtr2);

	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr);
	free_limit = mtr_read_ulint(header + FSP_FREE_LIMIT,
					MLOG_4BYTES, &mtr);
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
					MLOG_4BYTES, &mtr);

	n_full_frag_pages = FSP_EXTENT_SIZE *
				flst_get_len(header + FSP_FULL_FRAG, &mtr);

	ut_a(free_limit <= size || (space != 0 && size < FSP_EXTENT_SIZE));

	flst_validate(header + FSP_FREE, &mtr);
	flst_validate(header + FSP_FREE_FRAG, &mtr);
	flst_validate(header + FSP_FULL_FRAG, &mtr);

	mtr_commit(&mtr);

	/* Validate FSP_FREE list */
	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);
	node_addr = flst_get_first(header + FSP_FREE, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space), &mtr);

		descr_count++;
		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == 0);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FREE);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSP_FREE_FRAG list */
	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);
	node_addr = flst_get_first(header + FSP_FREE_FRAG, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space), &mtr);

		descr_count++;
		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) > 0);
		ut_a(xdes_get_n_used(descr, &mtr) < FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FREE_FRAG);

		n_used += xdes_get_n_used(descr, &mtr);
		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);

		mtr_commit(&mtr);
	}

	/* Validate FSP_FULL_FRAG list */
	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);
	node_addr = flst_get_first(header + FSP_FULL_FRAG, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space), &mtr);

		descr_count++;
		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FULL_FRAG);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate segments */
	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FULL, &mtr);

	seg_inode_len_full = flst_get_len(header + FSP_SEG_INODES_FULL, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		for (n = 0; n < FSP_SEG_INODES_PER_PAGE; n++) {

			mtr_start(&mtr);
			mtr_x_lock(fil_space_get_latch(space), &mtr);

			seg_inode_page = fut_get_ptr(space, node_addr,
				RW_X_LATCH, &mtr) - FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page, n, &mtr);
			ut_a(ut_dulint_cmp(
				     mach_read_from_8(seg_inode + FSEG_ID),
				     ut_dulint_zero) != 0);
			fseg_validate_low(seg_inode, &mtr);

			descr_count += flst_get_len(seg_inode + FSEG_FREE,
				&mtr);
			descr_count += flst_get_len(seg_inode + FSEG_FULL,
				&mtr);
			descr_count += flst_get_len(seg_inode + FSEG_NOT_FULL,
				&mtr);

			n_used2 += fseg_get_n_frag_pages(seg_inode, &mtr);

			next_node_addr = flst_get_next_addr(seg_inode_page
				+ FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		}

		node_addr = next_node_addr;
	}

	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FREE, &mtr);

	seg_inode_len_free = flst_get_len(header + FSP_SEG_INODES_FREE, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		for (n = 0; n < FSP_SEG_INODES_PER_PAGE; n++) {

			mtr_start(&mtr);
			mtr_x_lock(fil_space_get_latch(space), &mtr);

			seg_inode_page = fut_get_ptr(space, node_addr,
				RW_X_LATCH, &mtr) - FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page,	n, &mtr);
			if (ut_dulint_cmp(mach_read_from_8(
						  seg_inode + FSEG_ID),
					ut_dulint_zero) != 0) {
				fseg_validate_low(seg_inode, &mtr);

				descr_count += flst_get_len(
					seg_inode + FSEG_FREE, &mtr);
				descr_count += flst_get_len(
					seg_inode + FSEG_FULL, &mtr);
				descr_count += flst_get_len(
					seg_inode + FSEG_NOT_FULL, &mtr);
				n_used2 += fseg_get_n_frag_pages(
					seg_inode, &mtr);
			}

			next_node_addr = flst_get_next_addr(seg_inode_page
				+ FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		}

		node_addr = next_node_addr;
	}

	ut_a(descr_count * FSP_EXTENT_SIZE == free_limit);
	ut_a(n_used + n_full_frag_pages
		== n_used2 + 2* ((free_limit + XDES_DESCRIBED_PER_PAGE - 1)
			/ XDES_DESCRIBED_PER_PAGE)
		+ seg_inode_len_full + seg_inode_len_free);
	ut_a(frag_n_used == n_used);

	mtr_commit(&mtr2);

	return(TRUE);
}

/***********************************************************************
Prints info of a file space. */

void
fsp_print(
/*======*/
	ulint	space)	/* in: space id */
{
	fsp_header_t*	header;
	fseg_inode_t*	seg_inode;
	page_t*		seg_inode_page;
	ulint		size;
	ulint		free_limit;
	ulint		frag_n_used;
	fil_addr_t	node_addr;
	fil_addr_t	next_node_addr;
	ulint		n_free;
	ulint		n_free_frag;
	ulint		n_full_frag;
	ulint		seg_id_low;
	ulint		seg_id_high;
	ulint		n;
	ulint		n_segs		= 0;
	dulint		d_var;
	mtr_t		mtr;
	mtr_t		mtr2;

	/* Start first a mini-transaction mtr2 to lock out all other threads
	from the fsp system */

	mtr_start(&mtr2);

	mtr_x_lock(fil_space_get_latch(space), &mtr2);

	mtr_start(&mtr);

	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr);

	free_limit = mtr_read_ulint(header + FSP_FREE_LIMIT, MLOG_4BYTES,
									&mtr);
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES,
									&mtr);
	n_free = flst_get_len(header + FSP_FREE, &mtr);
	n_free_frag = flst_get_len(header + FSP_FREE_FRAG, &mtr);
	n_full_frag = flst_get_len(header + FSP_FULL_FRAG, &mtr);

	d_var = mtr_read_dulint(header + FSP_SEG_ID, &mtr);

	seg_id_low = ut_dulint_get_low(d_var);
	seg_id_high = ut_dulint_get_high(d_var);

	fprintf(stderr,
"FILE SPACE INFO: id %lu\n"
"size %lu, free limit %lu, free extents %lu\n"
"not full frag extents %lu: used pages %lu, full frag extents %lu\n"
"first seg id not used %lu %lu\n",
		(long) space,
		(ulong) size, (ulong) free_limit, (ulong) n_free,
		(ulong) n_free_frag, (ulong) frag_n_used, (ulong) n_full_frag,
		(ulong) seg_id_high, (ulong) seg_id_low);

	mtr_commit(&mtr);

	/* Print segments */

	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FULL, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		for (n = 0; n < FSP_SEG_INODES_PER_PAGE; n++) {

			mtr_start(&mtr);
			mtr_x_lock(fil_space_get_latch(space), &mtr);

			seg_inode_page = fut_get_ptr(space, node_addr,
				RW_X_LATCH, &mtr) - FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page,	n, &mtr);
			ut_a(ut_dulint_cmp(mach_read_from_8(
						   seg_inode + FSEG_ID),
					ut_dulint_zero) != 0);
			fseg_print_low(seg_inode, &mtr);

			n_segs++;

			next_node_addr = flst_get_next_addr(seg_inode_page
				+ FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		}

		node_addr = next_node_addr;
	}

	mtr_start(&mtr);
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header = fsp_get_space_header(space, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FREE, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		for (n = 0; n < FSP_SEG_INODES_PER_PAGE; n++) {

			mtr_start(&mtr);
			mtr_x_lock(fil_space_get_latch(space), &mtr);

			seg_inode_page = fut_get_ptr(space, node_addr,
				RW_X_LATCH, &mtr) - FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page,	n, &mtr);
			if (ut_dulint_cmp(mach_read_from_8(
						  seg_inode + FSEG_ID),
					ut_dulint_zero) != 0) {

				fseg_print_low(seg_inode, &mtr);
				n_segs++;
			}

			next_node_addr = flst_get_next_addr(seg_inode_page
				+ FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		}

		node_addr = next_node_addr;
	}

	mtr_commit(&mtr2);

	fprintf(stderr, "NUMBER of file segments: %lu\n", (ulong) n_segs);
}
