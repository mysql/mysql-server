/**********************************************************************
File-space management

(c) 1995 Innobase Oy

Created 11/29/1995 Heikki Tuuri
***********************************************************************/

#include "fsp0fsp.h"

#include "buf0buf.h"
#include "fil0fil.h"
#include "sync0sync.h"
#include "mtr0log.h"
#include "fut0fut.h"
#include "ut0byte.h"

/* The data structures in files are defined just as byte strings in C */
typedef	byte	fsp_header_t;
typedef	byte	xdes_t;		
typedef byte	fseg_page_header_t;

/* Rw-latch protecting the whole file space system */
rw_lock_t	fsp_latch;


/*			SPACE HEADER		
			============

File space header data structure: this data structure
is contained in the first page of a space. The space for this header
is reserved in every extent descriptor page, but used only in the first. */
#define FSP_HEADER_OFFSET	FIL_PAGE_DATA	/* Offset of the space header
						within a file page */
/*-------------------------------------*/
#define	FSP_SIZE		0	/* Current
					size of the space in pages */
#define	FSP_FREE_LIMIT		4	/* Minimum page number for which
					the free list has not been initialized:
					the pages >= this limit are, by
					definition, free */
#define	FSP_LOWEST_NO_WRITE	8	/* The lowest page offset for which
					the page has not been written to disk
					(if it has been written, we know
					that the OS has really reserved
					the physical space for the page) */
#define	FSP_FRAG_N_USED		12	/* number of used pages in
					the FSP_FREE_FRAG list */
#define	FSP_FREE		16	/* list of free extents */
#define	FSP_FREE_FRAG		(16 + FLST_BASE_NODE_SIZE)
					/* list of partially free extents not
					belonging to any segment */
#define	FSP_FULL_FRAG		(16 + 2 * FLST_BASE_NODE_SIZE)
					/* list of full extents not belonging
					to any segment */
#define FSP_SEG_ID		(16 + 3 * FLST_BASE_NODE_SIZE)
					/* 8 bytes which give the first
#define FSP_SEG_HDRS_FULL	(24 + 3 * FLST_BASE_NODE_SIZE)
					/* list of pages containing segment
					headers, where all the segment header
					slots are reserved */
#define FSP_SEG_HDRS_FREE	(24 + 4 * FLST_BASE_NODE_SIZE)
					/* list of pages containing segment
					headers, where not all the segment
					header slots are reserved */
/*-------------------------------------*/
/* File space header size */
#define	FSP_HEADER_SIZE		(24 + 4 * FLST_BASE_NODE_SIZE)

#define	FSP_FREE_ADD		4	/* this many free extents are added
					to the free list from above
					FSP_FREE_LIMIT at a time */

					
/*			SEGMENT HEADER
			==============

Segment header which is created for each segment in a tablespace, on a
page of its own. NOTE: in purge we assume that a segment having only one
currently used page can be freed in a few steps, so that the freeing cannot
fill the file buffer with bufferfixed file pages. */

#define FSEG_HDR_PAGE_NODE	FSEG_PAGE_DATA
					/* the list node for linking
					segment header pages */

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
#define FSEG_HEADER_SIZE	(16 + 3 * FLST_BASE_NODE_SIZE +\
				FSEG_FRAG_ARR_N_SLOTS * FSEG_FRAG_SLOT_SIZE)

#define FSP_SEG_HDRS_PER_PAGE	((UNIV_PAGE_SIZE - FSEG_ARR_OFFSET - 10)\
				 / FSEG_HEADER_SIZE)
				/* Number of segment headers which fit on a
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
					
#define FSEG_FRAG_LIMIT		FSEG_FRAG_N_ARR_SLOTS
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

File extent descriptor data structure: contains bits to tell
which pages in the extent are free and which contain old tuple
version to clean. */

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
					in the extent*/
/*-------------------------------------*/
					
#define	XDES_BITS_PER_PAGE	2	/* How many bits are there per page */
#define	XDES_FREE_BIT		0	/* Index of the bit which tells if
					the page is free */
#define	XDES_CLEAN_BIT		1	/* Index of the bit which tells if
					there are old versions of tuples
					on the page */
/* States of a descriptor */
#define	XDES_FREE		1	/* extent is in free list of space */
#define	XDES_FREE_FRAG		2	/* extent is in free fragment list of
					space */
#define	XDES_FULL_FRAG		3	/* extent is in full fragment list of
					space */
#define	XDES_FSEG		4	/* extent belongs to a segment*/

/* Number of pages described in a single descriptor page:
currently each page description takes less than
1 byte. */
#define XDES_DESCRIBED_PER_PAGE		UNIV_PAGE_SIZE

/* File extent data structure size in bytes. The "+ 7 ) / 8"
part in the definition rounds the number of bytes upward. */
#define	XDES_SIZE	(XDES_BITMAP +\
			 (FSP_EXTENT_SIZE * XDES_BITS_PER_PAGE + 7) / 8)

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
	fseg_header_t*	seg_header, /* in: segment header */
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
	fseg_header_t* 	header,	/* in: segment header */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr);	/* in: mtr handle */
/************************************************************************
Marks a page used. The page must reside within the extents of the given
segment. */
static
void
fseg_mark_page_used(
/*================*/
	fseg_header_t*	seg_header,/* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr);	/* in: mtr */
/**************************************************************************
Frees a single page of a segment. */
static
void
fseg_free_page_low(
/*===============*/
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr);	/* in: mtr handle */
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
	fseg_header_t*	header,	/* in: segment header */
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
	ulint		space,	/* in: space */
	fsp_header_t*	header,	/* in: space header */
	mtr_t*		mtr);	/* in: mtr */

/**************************************************************************
Gets a descriptor bit of a page. */
UNIV_INLINE
bool
xdes_get_bit(
/*=========*/
				/* out: TRUE if free */
	xdes_t*		descr,	/* in: descriptor */
	ulint		bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ulint		offset,	/* in: page offset within extent:
				0 ... FSP_EXTENT_SIZE - 1 */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	index;
	ulint	byte_index;
	ulint	bit_index;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));
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
	xdes_t*		descr,	/* in: descriptor */
	ulint		bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ulint		offset,	/* in: page offset within extent:
				0 ... FSP_EXTENT_SIZE - 1 */
	bool		val,	/* in: bit value */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	index;
	ulint	byte_index;
	ulint	bit_index;
	ulint	descr_byte;
	
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));
	ut_ad((bit == XDES_FREE_BIT) || (bit == XDES_CLEAN_BIT));
	ut_ad(offset < FSP_EXTENT_SIZE);

	index = bit + XDES_BITS_PER_PAGE * offset;

	byte_index = index / 8;
	bit_index = index % 8;

	descr_byte = mtr_read_ulint(descr + XDES_BITMAP + byte_index,
				  MLOG_1BYTE, mtr);
		
	descr_byte = ut_bit_set_nth(descr_byte, bit_index, val);

	mlog_write_ulint(descr + XDES_BITMAP + byte_index,
				  descr_byte, MLOG_1BYTE, mtr);
}	

/**************************************************************************
Looks for a descriptor bit having the desired value. Starts from hint
and scans upward; at the end of the extent the search is wrapped to
the start of the extent. */
UNIV_INLINE
ulint
xdes_find_bit(
/*==========*/
				/* out: bit index of the bit,
				ULINT_UNDEFINED if not found */
	xdes_t*		descr,	/* in: descriptor */
	ulint		bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	bool		val,	/* in: desired bit value */
	ulint		hint,	/* in: hint of which bit position would be
				desirable */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	i;
	
	ut_ad(descr && mtr);
	ut_ad(val <= TRUE);
	ut_ad(hint < FSP_EXTENT_SIZE);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));

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
				/* out: bit index of the bit,
				ULINT_UNDEFINED if not found */
	xdes_t*		descr,	/* in: descriptor */
	ulint		bit,	/* in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	bool		val,	/* in: desired bit value */
	ulint		hint,	/* in: hint of which bit position would be
				desirable */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	i;
	
	ut_ad(descr && mtr);
	ut_ad(val <= TRUE);
	ut_ad(hint < FSP_EXTENT_SIZE);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));

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
	xdes_t*		descr,	/* in: descriptor */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	i;
	ulint	count	= 0;
	
	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));

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
bool
xdes_is_free(
/*=========*/
				/* out: TRUE if totally free */
	xdes_t*		descr,	/* in: descriptor */
	mtr_t*		mtr)	/* in: mtr */
{
	if (0 == xdes_get_n_used(descr, mtr)) {
		return(TRUE);
	} else {
		return(FALSE);
	}
}

/**************************************************************************
Returns true if extent contains no free pages. */
UNIV_INLINE
bool
xdes_is_full(
/*=========*/
				/* out: TRUE if full */
	xdes_t*		descr,	/* in: descriptor */
	mtr_t*		mtr)	/* in: mtr */
{
	if (FSP_EXTENT_SIZE == xdes_get_n_used(descr, mtr)) {
		return(TRUE);
	} else {
		return(FALSE);
	}
}

/**************************************************************************
Sets the state of an xdes. */
UNIV_INLINE
void
xdes_set_state(
/*===========*/
	xdes_t*		descr,	/* in: descriptor */
	ulint		state,	/* in: state to set */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ut_ad(descr && mtr);
	ut_ad(state >= XDES_FREE);
	ut_ad(state <= XDES_FSEG);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));

	mlog_write_ulint(descr + XDES_STATE, state, MLOG_4BYTES, mtr); 
}

/**************************************************************************
Gets the state of an xdes. */
UNIV_INLINE
ulint
xdes_get_state(
/*===========*/
				/* out: state */
	xdes_t*		descr,	/* in: descriptor */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));

	return(mtr_read_ulint(descr + XDES_STATE, MLOG_4BYTES, mtr)); 
}

/**************************************************************************
Inits an extent descriptor to free and clean state. */
UNIV_INLINE
void
xdes_init(
/*======*/
	xdes_t*		descr,	/* in: descriptor */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	i;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(descr),
				MTR_MEMO_PAGE_X_LOCK));

	for (i = 0; i < FSP_EXTENT_SIZE; i++) {
		xdes_set_bit(descr, XDES_FREE_BIT, i, TRUE, mtr);
		xdes_set_bit(descr, XDES_CLEAN_BIT, i, TRUE, mtr);
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
	ut_ad(UNIV_PAGE_SIZE > XDES_ARR_OFFSET
		+ (XDES_DESCRIBED_PER_PAGE / FSP_EXTENT_SIZE) * XDES_SIZE);

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
Gets pointer to a the extent descriptor of a page. The page where the
extent descriptor resides is x-locked. If the page offset is equal to the free
limit of the space, adds new extents from above the free limit
to the space free list, if not free limit == space size. This adding
is necessary to make the descriptor defined, as they are uninitialized
above the free limit. */
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
	ulint		limit;
	ulint		size;
	buf_block_t*	buf_page;
	ulint		descr_page_no;
	page_t*		descr_page;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, &fsp_latch, MTR_MEMO_X_LOCK));

	/* Read free limit and space size */
	limit = mtr_read_ulint(sp_header + FSP_FREE_LIMIT, MLOG_4BYTES, mtr);
	size  = mtr_read_ulint(sp_header + FSP_SIZE, MLOG_4BYTES, mtr);

	/* If offset is >= size or > limit, return NULL */
	if ((offset >= size) || (offset > limit)) {
		return(NULL);
	}

	/* If offset is == limit, fill free list of the space. */
	if (offset == limit) {
		fsp_fill_free_list(space, sp_header, mtr);
	}

	descr_page_no = xdes_calc_descriptor_page(offset);

	if (descr_page_no == 0) {
		/* It is on the space header page */

		descr_page = buf_frame_align(sp_header);
	} else {
	
		buf_page = buf_page_get(space, descr_page_no, mtr);
		buf_page_x_lock(buf_page, mtr);
		descr_page = buf_block_get_frame(buf_page);
	}	

	return(descr_page + XDES_ARR_OFFSET
	       + XDES_SIZE * xdes_calc_descriptor_index(offset));
}

/************************************************************************
Gets pointer to a the extent descriptor of a page. The page where the
extent descriptor resides is x-locked. If the page offset is equal to the free
limit of the space, adds new extents from above the free limit
to the space free list, if not free limit == space size. This adding
is necessary to make the descriptor defined, as they are uninitialized
above the free limit. */
static
xdes_t*
xdes_get_descriptor(
/*================*/
				/* out: pointer to the extent descriptor,
				NULL if the page does not exist in the
				space or if offset > free limit */
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: page offset; 
				if equal to the free limit,
				we try to add new extents to
				the space free list */
	mtr_t*		mtr)	/* in: mtr handle */
{
	fsp_header_t*	sp_header;
	buf_block_t*	block;

	block = buf_page_get(space, 0, mtr); /* get space header */
	sp_header = FSP_HEADER_OFFSET + buf_block_get_frame(block);
	buf_page_x_lock(block, mtr);
	
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
	xdes_t*		descr;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, &fsp_latch, MTR_MEMO_X_LOCK));
	
	descr = fut_get_ptr_x_lock(space, lst_node, mtr) - XDES_FLST_NODE;

	return(descr);
}

/************************************************************************
Gets pointer to the next descriptor in a descriptor list and x-locks
its page. */
UNIV_INLINE
xdes_t*
xdes_lst_get_next(
/*==============*/
	xdes_t*		descr,	/* in: pointer to a descriptor */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	space;

	ut_ad(mtr && descr);

	space = buf_page_get_space(buf_block_align(descr));

	return(xdes_lst_get_descriptor(space,
		flst_get_next_addr(descr + XDES_FLST_NODE, mtr), mtr));
}

/************************************************************************
Returns page offset of the first page in extent described by a descriptor.
*/
UNIV_INLINE
ulint
xdes_get_offset(
/*============*/
				/* out: offset of the first page in extent */
	xdes_t*		descr)	/* in: extent descriptor */
{
	buf_block_t*	buf_page;

	ut_ad(descr);

	buf_page = buf_block_align(descr);

	return(buf_page_get_offset(buf_page)
		+ ((descr - buf_frame_align(descr) - XDES_ARR_OFFSET)
		   / XDES_SIZE)
		  * FSP_EXTENT_SIZE);
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
	buf_block_t*	block;
	
	ut_ad(mtr);
	
	block = buf_page_get(id, 0, mtr);

	buf_page_x_lock(block, mtr);	

	return(FSP_HEADER_OFFSET + buf_block_get_frame(block));
}

/**************************************************************************
Initializes the file space system mutex. */

void
fsp_init(void)
/*==========*/
{
	rw_lock_create(&fsp_latch);
}

/**************************************************************************
Initializes the space header of a new created space. */

void
fsp_header_init(
/*============*/
	ulint		space,	/* in: space id */
	ulint		size,	/* in: current size in blocks */
	mtr_t*		mtr)	/* in: mini-transaction handle */	
{
	fsp_header_t*	header;
	
	ut_ad(mtr);

	mtr_x_lock(&fsp_latch, mtr);	

	header = fsp_get_space_header(space, mtr);

	mlog_write_ulint(header + FSP_SIZE, size, MLOG_4BYTES, mtr); 
	mlog_write_ulint(header + FSP_FREE_LIMIT, 0, MLOG_4BYTES, mtr); 
	mlog_write_ulint(header + FSP_LOWEST_NO_WRITE, 0, MLOG_4BYTES, mtr); 
	mlog_write_ulint(header + FSP_FRAG_N_USED, 0, MLOG_4BYTES, mtr); 
	
	flst_init(header + FSP_FREE, mtr);
	flst_init(header + FSP_FREE_FRAG, mtr);
	flst_init(header + FSP_FULL_FRAG, mtr);
	flst_init(header + FSP_SEG_HDRS_FULL, mtr);
	flst_init(header + FSP_SEG_HDRS_FREE, mtr);

	mlog_write_dulint(header + FSP_SEG_ID, ut_dulint_create(0, 1),
							MLOG_8BYTES, mtr); 
}

/**************************************************************************
Increases the space size field of a space. */

void
fsp_header_inc_size(
/*================*/
	ulint		space,	/* in: space id */
	ulint		size_inc,/* in: size increment in pages */
	mtr_t*		mtr)	/* in: mini-transaction handle */	
{
	fsp_header_t*	header;
	ulint		size;
	
	ut_ad(mtr);

	mtr_x_lock(&fsp_latch, mtr);	

	header = fsp_get_space_header(space, mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_SIZE, size + size_inc, MLOG_4BYTES, mtr); 
}

/**************************************************************************
Puts new extents to the free list if there are free extents above the free
limit. If an extent happens to contain an extent descriptor page, the extent
is put to the FSP_FREE_FRAG list with the page marked as used. */
static
void
fsp_fill_free_list(
/*===============*/
	ulint		space,	/* in: space */
	fsp_header_t*	header,	/* in: space header */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint		limit;
	ulint		size;
	ulint		i;
	xdes_t*		descr;
	ulint		count 	= 0;
	ulint		frag_n_used;

	ut_ad(header && mtr);
	
	/* Check if we can fill free list from above the free list limit */
	size  = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	limit = mtr_read_ulint(header + FSP_FREE_LIMIT, MLOG_4BYTES, mtr);

	i = limit;		
	while ((i + FSP_EXTENT_SIZE <= size) && (count < FSP_FREE_ADD)) {
		mlog_write_ulint(header + FSP_FREE_LIMIT,
					i + FSP_EXTENT_SIZE, MLOG_4BYTES, mtr); 

		descr = xdes_get_descriptor_with_space_hdr(header, space, i,
									mtr);
		xdes_init(descr, mtr);

		ut_ad(XDES_DESCRIBED_PER_PAGE % FSP_EXTENT_SIZE == 0);

		if (0 == i % XDES_DESCRIBED_PER_PAGE) {
			/* The first page in the extent is a descriptor page:
			mark it used */
			xdes_set_bit(descr, XDES_FREE_BIT, 0, FALSE, mtr);
			xdes_set_state(descr, XDES_FREE_FRAG, mtr);
			flst_add_last(header + FSP_FREE_FRAG,
					descr + XDES_FLST_NODE, mtr);
			frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
						     MLOG_4BYTES, mtr);
			mlog_write_ulint(header + FSP_FRAG_N_USED,
					frag_n_used + 1,
					MLOG_4BYTES, mtr);
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
				/* out: extent descriptor, NULL if cannot
				be allocated */
	ulint		space,	/* in: space id */
	ulint		hint,	/* in: hint of which extent would be
				desirable: any page offset in the extent
				goes; the hint must not be > FSP_FREE_LIMIT */
	mtr_t*		mtr)	/* in: mtr */
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
			fsp_fill_free_list(space, header, mtr);
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
			/* out: the page offset, FIL_NULL
			if no page could be allocated */
	ulint	space,	/* in: space id */
	ulint	hint,	/* in: hint of which page would be desirable */
	mtr_t*	mtr)	/* in: mtr handle */
{
	fsp_header_t*	header;
	fil_addr_t	first;
	xdes_t*		descr;
	ulint		free;
	ulint		frag_n_used;
	
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
			a free extent and add it to the FREE_FRAG
			list. NOTE that the allocation may have as a
			side-effect that an extent containing a descriptor
			page is added to the FREE_FRAG list. But we will
			allocate our page from the allocated free extent. */
			
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

	/* Now we have in descr an extent with at least one free page.
	Look for a free page in the extent. */
	free = xdes_find_bit(descr, XDES_FREE_BIT, TRUE,
				hint % FSP_EXTENT_SIZE, mtr);
	ut_a(free != ULINT_UNDEFINED);

	xdes_set_bit(descr, XDES_FREE_BIT, free, FALSE, mtr);

	/* Update the FRAG_N_USED field */
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
					MLOG_4BYTES, mtr);
	frag_n_used++;
	mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used,
				MLOG_4BYTES, mtr);
	
	if (xdes_is_full(descr, mtr)) {
		/* The fragment is full: move it to another list */
		flst_remove(header + FSP_FREE_FRAG,
				descr + XDES_FLST_NODE, mtr);
		xdes_set_state(descr, XDES_FULL_FRAG, mtr);
		flst_add_last(header + FSP_FULL_FRAG,
				descr + XDES_FLST_NODE, mtr);
		mlog_write_ulint(header + FSP_FRAG_N_USED,
				frag_n_used - FSP_EXTENT_SIZE,
				MLOG_4BYTES, mtr);
	}
	return(xdes_get_offset(descr) + free);
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

	header = fsp_get_space_header(space, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, page, mtr);

	state = xdes_get_state(descr, mtr);
	
	ut_a((state == XDES_FREE_FRAG) || (state == XDES_FULL_FRAG));

	ut_a(xdes_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr)
		== FALSE);

	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);

	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
					MLOG_4BYTES, mtr);

	if (state == XDES_FULL_FRAG) {
		/* The fragment was full: move it to another list */
		flst_remove(header + FSP_FULL_FRAG,
				descr + XDES_FLST_NODE, mtr);
		xdes_set_state(descr, XDES_FREE_FRAG, mtr);
		flst_add_last(header + FSP_FREE_FRAG,
				descr + XDES_FLST_NODE, mtr);
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
		flst_remove(header + FSP_FREE_FRAG,
				descr + XDES_FLST_NODE, mtr);
		fsp_free_extent(space, page, mtr);
	}		
}

/**************************************************************************
Returns an extent to the free list of a space. */
static
void
fsp_free_extent(
/*============*/
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset in the extent */
	mtr_t*		mtr)	/* in: mtr */
{
	fsp_header_t*	header;
	xdes_t*		descr;
	
	ut_ad(mtr);

	header = fsp_get_space_header(space, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, page, mtr);

	ut_a(xdes_get_state(descr, mtr) != XDES_FREE);

	xdes_init(descr, mtr);

	flst_add_last(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);
}

/**************************************************************************
Looks for an unused segment header on a segment header page. */ 
UNIV_INLINE
fseg_header_t*
fsp_seg_hdr_page_get_nth_hdr(
/*=========================*/
			/* out: segment header */
	page_t*	page,	/* in: segment header page */
	ulint	i,	/* in: search forward starting from this index */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	ut_ad(i < FSP_SEG_HDRS_PER_PAGE);
	ut_ad(mtr_memo_contains(mtr, page, MTR_MEMO_PAGE_X_LOCK));

	return(page + FSEG_ARR_OFFSET + FSEG_HEADER_SIZE * i);
}

/**************************************************************************
Looks for a used segment header on a segment header page. */ 
static
ulint
fsp_seg_hdr_page_find_used(
/*=======================*/
			/* out: segment header index, or ULINT_UNDEFINED
			if not found */
	page_t*	page,	/* in: segment header page */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	ulint		i;
	fseg_header_t*	header;

	for (i = 0; i < FSP_SEG_HDRS_PER_PAGE; i++) {

		header = fsp_seg_hdr_page_get_nth_hdr(page, i, mtr);

		if (ut_dulint_cmp(mach_read_from_8(header + FSEG_ID),
						ut_dulint_zero) != 0) {
			/* This is used */
			
			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Looks for an unused segment header on a segment header page. */ 
static
ulint
fsp_seg_hdr_page_find_free(
/*=======================*/
			/* out: segment header index, or ULINT_UNDEFINED
			if not found */
	page_t*	page,	/* in: segment header page */
	ulint	j,	/* in: search forward starting from this index */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	ulint		i;
	fseg_header_t*	header;

	for (i = j; i < FSP_SEG_HDRS_PER_PAGE; i++) {

		header = fsp_seg_hdr_page_get_nth_hdr(page, i, mtr);

		if (ut_dulint_cmp(mach_read_from_8(header + FSEG_ID),
						ut_dulint_zero) == 0) {
			/* This is unused */
			
			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Allocates a new file segment header page. */
static
bool
fsp_alloc_seg_hdr_page(
/*===================*/
					/* out: TRUE if could be allocated */
	fsp_header_t*	space_header,	/* in: space header */
	mtr_t*		mtr)		/* in: mini-transaction handle */
{
	buf_block_t*	block;
	ulint		page_no;
	page_t*		page;
	fseg_header_t*	header;
	ulint		i;
	
	page_no = fsp_alloc_free_page(buf_frame_get_space(space_header),
								0, mtr);
	if (page_no == FIL_NULL) {

		return(FALSE);
	}

	block = buf_page_get(buf_frame_get_space(space_header), page_no, mtr);

	buf_page_x_lock(block, mtr);

	page = buf_block_get_frame(block);

	for (i = 0; i < FSP_SEG_HDRS_PER_PAGE; i++) {

		header = fsp_seg_hdr_page_get_nth_hdr(page, i, mtr);

		mlog_write_dulint(header + FSEG_ID, ut_dulint_zero,
					MLOG_8BYTES, mtr);
	}

	flst_add_last(space_header + FSP_SEG_HDRS_FREE,
				page + FSEG_HDR_PAGE_NODE, mtr);
	return(TRUE);
}

/**************************************************************************
Allocates a new file segment header. */
static
fseg_header_t*
fsp_alloc_seg_header(
/*=================*/
					/* out: segment header, or NULL if
					not enough space */
	fsp_header_t*	space_header,	/* in: space header */
	mtr_t*		mtr)		/* in: mini-transaction handle */
{
	buf_block_t*	block;
	ulint		page_no;
	page_t*		page;
	fseg_header_t*	header;
	ulint		n;
	bool		success;
	
	if (flst_get_len(space_header + FSP_SEG_HDRS_FREE, mtr) == 0) {
		/* Allocate a new segment header page */

		success = fsp_alloc_seg_hdr_page(space_header, mtr);

		if (!success) {

			return(NULL);
		}
	}

	page_no = flst_get_first(space_header + FSP_SEG_HDRS_FREE, mtr).page;

	block = buf_page_get(buf_frame_get_space(space_header), page_no, mtr);

	buf_page_x_lock(block, mtr);

	page = buf_block_get_frame(block);

	n = fsp_seg_hdr_page_find_free(page, 0, mtr);

	ut_a(n != ULINT_UNDEFINED);

	header = fsp_seg_hdr_page_get_nth_hdr(page, n, mtr);

	if (ULINT_UNDEFINED == fsp_seg_hdr_page_find_free(page, n + 1, mtr)) {

		/* There are no other unused headers left on the page: move it
		to another list */

		flst_remove(space_header + FSP_SEG_HDRS_FREE,
				page + FSEG_HDR_PAGE_NODE, mtr);

		flst_add_last(space_header + FSP_SEG_HDRS_FULL,
				page + FSEG_HDR_PAGE_NODE, mtr);
	}

	return(header);	
}

/**************************************************************************
Frees a file segment header. */
static
void
fsp_free_seg_header(
/*================*/
	ulint		space,	/* in: space id */
	fseg_header_t*	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mini-transaction handle */
{
	page_t*		page;
	fsp_header_t*	space_header;
	
	page = buf_frame_align(header);

	space_header = fsp_get_space_header(space, mtr);

	ut_ad(mach_read_from_4(header + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	if (ULINT_UNDEFINED == fsp_seg_hdr_page_find_free(page, mtr)) {

		/* Move the page to another list */

		flst_remove(space_header + FSP_SEG_HDRS_FULL,
				page + FSEG_HDR_PAGE_NODE, mtr);

		flst_add_last(space_header + FSP_SEG_HDRS_FREE,
				page + FSEG_HDR_PAGE_NODE, mtr);
	}

	mlog_write_dulint(header + FSEG_ID, ut_dulint_zero, MLOG_8BYTES, mtr); 
	mlog_write_ulint(header + FSEG_MAGIC_N, 0, MLOG_4BYTES, mtr); 
	
	if (ULINT_UNDEFINED == fsp_seg_hdr_page_find_used(page, mtr)) {

		/* There are no other used headers left on the page: free it */

		flst_remove(space_header + FSP_SEG_HDRS_FREE,
				page + FSEG_HDR_PAGE_NODE, mtr);

		fsp_free_page(space, page_no, mtr);		
	}
}

/**************************************************************************
Gets the page number from the nth fragment page slot. */
UNIV_INLINE
ulint
fseg_get_nth_frag_page_no(
/*======================*/
				/* out: page number, FIL_NULL if not in use */
	fseg_header_t* 	header,	/* in: segment header */
	ulint		n,	/* in: slot index */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ut_ad(header && mtr);
	ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(header),
							MTR_MEMO_PAGE_X_LOCK));

	return(mach_read_from_4(header + FSEG_FRAG_ARR
				+ n * FSEG_FRAG_SLOT_SIZE));
}

/**************************************************************************
Sets the page number in the nth fragment page slot. */
UNIV_INLINE
void
fseg_set_nth_frag_page_no(
/*======================*/
	fseg_header_t* 	header,	/* in: segment header */
	ulint		n,	/* in: slot index */
	ulint		page_no,/* in: page number to set */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ut_ad(header && mtr);
	ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(header),
							MTR_MEMO_PAGE_X_LOCK));

	mlog_write_ulint(header + FSEG_FRAG_ARR + n * FSEG_FRAG_SLOT_SIZE,
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
	fseg_header_t* 	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	i;
	ulint	page_no;

	ut_ad(header && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		page_no = fseg_get_nth_frag_page_no(header, i, mtr);

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
	fseg_header_t* 	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	i;
	ulint	page_no;

	ut_ad(header && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		page_no = fseg_get_nth_frag_page_no(header,
					FSEG_ARR_N_SLOTS - i - 1, mtr);

		if (page_no != FIL_NULL) {

			return(i);
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
	fseg_header_t* 	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	i;
	ulint	count	= 0;

	ut_ad(header && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		if (FIL_NULL != fseg_get_nth_frag_page_no(header, i, mtr)) {
			count++;
		}
	}
	return(count);
}

/**************************************************************************
Creates a new segment. */

ulint
fseg_create(
/*========*/
			/* out: the page number where the segment header is
			placed, FIL_NULL if could not create segment because
			lack of space */
	ulint	space,	/* in: space id */
	ulint*	offset,	/* out: byte offset of the segment header on its
			page */
	mtr_t*	mtr)	/* in: mtr */
{
	buf_block_t*	block;
	buf_frame_t*	frame;
	fsp_header_t*	space_header;
	fseg_header_t*	header;
	dulint		seg_id;
	ulint		i;

	ut_ad(mtr);

	mtr_x_lock(&fsp_latch, mtr);	

	space_header = fsp_get_space_header(space, mtr);

	header = fsp_alloc_seg_header(space_header, mtr);

	if (header == NULL) {

		return(FIL_NULL);
	}

	/* Read the next segment id from space header and increment the
	value in space header */

	seg_id = mtr_read_dulint(space_header + FSP_SEG_ID, MLOG_8BYTES, mtr);

	mlog_write_dulint(space_header + FSP_SEG_ID, ut_dulint_add(seg_id, 1),
							MLOG_8BYTES, mtr);

	mlog_write_dulint(header + FSEG_ID, seg_id, MLOG_8BYTES, mtr); 
	mlog_write_ulint(header + FSEG_NOT_FULL_N_USED, 0, MLOG_4BYTES, mtr); 

	flst_init(header + FSEG_FREE, mtr);
	flst_init(header + FSEG_NOT_FULL, mtr);
	flst_init(header + FSEG_FULL, mtr);

	mlog_write_ulint(header + FSEG_MAGIC_N, FSEG_MAGIC_N_VALUE,
							MLOG_4BYTES, mtr); 
	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		fseg_set_nth_frag_page_no(header, i, FIL_NULL, mtr);
	}

	*offset = header - buf_frame_align(header);
	return(buf_frame_get_page(buf_frame_align(header)));	
}

/**************************************************************************
Calculates the number of pages reserved by a segment, and how
many pages are currently used. */

ulint
fseg_n_reserved_pages(
/*==================*/
				/* out: number of reserved pages */
	fseg_header_t* 	header,	/* in: segment header */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	ret;

	mtr_x_lock(&fsp_latch, mtr);	

	ret = fseg_n_reserved_pages_low(header, used, mtr);

	return(ret);
}

/**************************************************************************
Calculates the number of pages reserved by a segment, and how
many pages are currently used. */
static
ulint
fseg_n_reserved_pages_low(
/*======================*/
				/* out: number of reserved pages */
	fseg_header_t* 	header,	/* in: segment header */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr)	/* in: mtr handle */
{
	ulint	ret;

	ut_ad(header && used && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(header),
				MTR_MEMO_BUF_FIX));
	
	buf_page_x_lock(buf_block_align(header), mtr);

	*used = mtr_read_ulint(header + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(header + FSEG_FULL, mtr)
		+ fseg_get_n_frag_pages(header, mtr);

	ret = fseg_get_n_frag_pages(header, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(header + FSEG_FREE, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(header + FSEG_NOT_FULL, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(header + FSEG_FULL, mtr);

	return(ret);
}

/*************************************************************************
Tries to fill the free list of a segment with consecutive free extents.
This happens if the segment is big enough to allowextents in the free list,
the free list is empty, and the extents can be allocated consecutively from
the hint onward. */
static
void
fseg_fill_free_list(
/*================*/
	fseg_header_t*	header,	/* in: segment header */
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
		
	ut_ad(header && mtr);

	buf_page_x_lock(buf_block_align(header), mtr);

	reserved = fseg_n_reserved_pages_low(header, &used, mtr);

	if (reserved < FSEG_FREE_LIST_LIMIT * FSP_EXTENT_SIZE) {
		/* The segment is too small to allow extents in free list */

		return;
	}

	if (flst_get_len(header + FSEG_FREE, mtr) > 0) {
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
		
		seg_id = mtr_read_dulint(header + FSEG_ID, MLOG_8BYTES, mtr);
		mlog_write_dulint(descr + XDES_ID, seg_id, MLOG_8BYTES, mtr);

		flst_add_last(header + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
		hint += FSP_EXTENT_SIZE;
	}
}

/*************************************************************************
Allocates a free extent for the segment: looks first in the
free list of the segment, then tries to allocate from the space free
list. NOTE that the extent returned is still placed in the segment free
list, not taken off it! */
static
xdes_t*
fseg_alloc_free_extent(
/*===================*/
				/* out: allocated extent, still placed in the
				segment free list, NULL if could
				not be allocated */
	fseg_header_t*	header,	/* in: segment header */
	ulint		space,	/* in: space id */
	mtr_t*		mtr)	/* in: mtr */
{
	xdes_t*		descr;
	dulint		seg_id;
	fil_addr_t 	first;
		
	buf_page_x_lock(buf_block_align(header), mtr);

	if (flst_get_len(header + FSEG_FREE, mtr) > 0) {
		/* Segment free list is not empty, allocate from it */
		
		first = flst_get_first(header + FSEG_FREE, mtr);

		descr = xdes_lst_get_descriptor(space, first, mtr);
	} else {
		/* Segment free list was empty, allocate from space */
		descr = fsp_alloc_free_extent(space, 0, mtr);

		if (descr == NULL) {
			return(NULL);
		}

		seg_id = mtr_read_dulint(header + FSEG_ID, MLOG_8BYTES, mtr);
		
		xdes_set_state(descr, XDES_FSEG, mtr);
		mlog_write_dulint(descr + XDES_ID, seg_id, MLOG_8BYTES, mtr);
		flst_add_last(header + FSEG_FREE,
				descr + XDES_FLST_NODE, mtr);
		
		/* Try to fill the segment free list */
		fseg_fill_free_list(header, space,
			xdes_get_offset(descr) + FSP_EXTENT_SIZE, mtr);
	}

	return(descr);
}

/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation. */

ulint
fseg_alloc_free_page(
/*=================*/
				/* out: the allocated page offset
				FIL_NULL if no page could be allocated */
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction, /* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr)	/* in: mtr handle */
{
	buf_block_t*	block;
	dulint		seg_id;
	fseg_page_header_t* page_header;
	ulint		space;
	ulint		used;
	ulint		reserved;
	fil_addr_t	first;
	xdes_t*		descr;		/* extent of the hinted page */
	ulint		ret_page;	/* the allocated page offset, FIL_NULL
					if could not be allocated */
	buf_block_t*	ret_buf_page;	
	buf_frame_t*	ret_frame;
	xdes_t*		ret_descr;	/* the extent of the allocated page */
	ulint		n;
	bool		frag_page_allocated = FALSE;
					
	ut_ad(seg_header && mtr);
	ut_ad((direction >= FSP_UP) && (direction <= FSP_NO_DIR));

	mtr_x_lock(&fsp_latch, mtr);	

	block = buf_block_align(seg_header);
	buf_page_x_lock(block, mtr);

	space = buf_page_get_space(block);

	seg_id = mtr_read_dulint(seg_header + FSEG_ID, MLOG_8BYTES, mtr);

	ut_ad(ut_dulint_cmp(seg_id, ut_dulint_zero) > 0);
	
	reserved = fseg_n_reserved_pages_low(seg_header, &used, mtr);
	
	descr = xdes_get_descriptor(space, hint, mtr);

	if (descr == NULL) {
		/* Hint outside space or too high above free limit:
		reset hint */
		hint = 0;
		descr = xdes_get_descriptor(space, hint, mtr);
	}
 
	/* In the big if-else below we look for ret_page and ret_descr */
	/*-------------------------------------------------------------*/ 
	if ((xdes_get_state(descr, mtr) == XDES_FSEG)
	           && (0 == ut_dulint_cmp(mtr_read_dulint(descr + XDES_ID,
							MLOG_8BYTES, mtr),
							seg_id))
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
		mlog_write_dulint(ret_descr + XDES_ID, seg_id, MLOG_8BYTES,
									mtr);
		flst_add_last(seg_header + FSEG_FREE,
				ret_descr + XDES_FLST_NODE, mtr);

		/* Try to fill the segment free list */
		fseg_fill_free_list(seg_header, space,
					hint + FSP_EXTENT_SIZE, mtr);
		ret_page = hint;
	/*-------------------------------------------------------------*/ 
	} else if ((direction != FSP_NO_DIR)
		   && ((reserved - used) < reserved / FSEG_FILLFACTOR)
		   && (used >= FSEG_FRAG_LIMIT)
		   && (NULL != (ret_descr =
			fseg_alloc_free_extent(seg_header, space, mtr)))) {

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
							MLOG_8BYTES, mtr),
						seg_id))
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
		if (flst_get_len(seg_header + FSEG_NOT_FULL, mtr) > 0) {
			first = flst_get_first(seg_header + FSEG_NOT_FULL,
						mtr);
		} else if (flst_get_len(seg_header + FSEG_FREE, mtr) > 0) {
			first = flst_get_first(seg_header + FSEG_FREE, mtr);
		} else {
			ut_error;
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
			n = fseg_find_free_frag_page_slot(seg_header, mtr);
			ut_a(n != FIL_NULL);

			fseg_set_nth_frag_page_no(seg_header, n, ret_page,
									mtr);
		}
	/*-------------------------------------------------------------*/ 
	} else {
		/* 7. We allocate a new extent and take its first page
		======================================================*/
		ret_descr = fseg_alloc_free_extent(seg_header, space, mtr);

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

	/* Initialize the allocated page to buffer pool, so that it can be
	obtained immediately with buf_page_get without need for disk read */
	
	ret_buf_page = buf_page_create(space, ret_page, mtr);

	if (!frag_page_allocated) {
		/* At this point we know the extent and the page offset.
		The extent is still in the appropriate list (FSEG_NOT_FULL or
		FSEG_FREE), and the page is not yet marked as used. */
		
		ut_ad(xdes_get_descriptor(space, ret_page, mtr) == ret_descr);
		ut_ad(xdes_get_bit(ret_descr, XDES_FREE_BIT,
				ret_page % FSP_EXTENT_SIZE, mtr) == TRUE);
		
		fseg_mark_page_used(seg_header, space, ret_page, mtr);
	}

	return(ret_page);	
}

/************************************************************************
Marks a page used. The page must reside within the extents of the given
segment. */
static
void
fseg_mark_page_used(
/*================*/
	fseg_header_t*	seg_header,/* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr)	/* in: mtr */
{
	xdes_t*	descr;
	ulint	not_full_n_used;

	ut_ad(seg_header && mtr);
	
	descr = xdes_get_descriptor(space, page, mtr);
	
	ut_ad(mtr_read_ulint(seg_header + FSEG_ID, MLOG_4BYTES, mtr) ==
		mtr_read_ulint(descr + XDES_ID, MLOG_4BYTES, mtr));

	if (xdes_is_free(descr, mtr)) {
		/* We move the extent from the free list to the
		NOT_FULL list */
		flst_remove(seg_header + FSEG_FREE,
				descr + XDES_FLST_NODE, mtr);
		flst_add_last(seg_header + FSEG_NOT_FULL,
				descr + XDES_FLST_NODE, mtr);
	}

	ut_ad(xdes_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr)
		== TRUE);

	/* We mark the page as used */
	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, FALSE, mtr);

	not_full_n_used = mtr_read_ulint(seg_header + FSEG_NOT_FULL_N_USED,
						MLOG_4BYTES, mtr);

	not_full_n_used++;
	mlog_write_ulint(seg_header + FSEG_NOT_FULL_N_USED,
					not_full_n_used, MLOG_4BYTES, mtr);

	if (xdes_is_full(descr, mtr)) {
		/* We move the extent from the NOT_FULL list to the
		FULL list */
		flst_remove(seg_header + FSEG_NOT_FULL,
				descr + XDES_FLST_NODE, mtr);
		flst_add_last(seg_header + FSEG_FULL,
				descr + XDES_FLST_NODE, mtr);
			
		mlog_write_ulint(seg_header + FSEG_NOT_FULL_N_USED,
				not_full_n_used - FSP_EXTENT_SIZE,
				MLOG_4BYTES, mtr);
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
	mtr_x_lock(&fsp_latch, mtr);	

	fseg_free_page_low(seg_header, space, page, mtr);
}

/**************************************************************************
Frees a single page of a segment. */
static
void
fseg_free_page_low(
/*===============*/
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr)	/* in: mtr handle */
{
	buf_block_t*	block;
	xdes_t*		descr;
	ulint		used;
	ulint		not_full_n_used;
	ulint		state;
	buf_block_t*	buf_page;
	buf_frame_t*	buf_frame;
	ulint		i;
	
	ut_ad(seg_header && mtr);

	block = buf_block_align(seg_header);
	buf_page_x_lock(block, mtr);	

	descr = xdes_get_descriptor(space, page, mtr);

	ut_a(descr);
	ut_a(xdes_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr)
		== FALSE);

	state = xdes_get_state(descr, mtr);

	if (state != XDES_FSEG) {
		/* The page is in the fragment pages of the segment */

		for (i = 0;; i++) {
			if (fseg_get_nth_frag_page_no(seg_header, i, mtr)
			    == page) {

				fseg_set_nth_frag_page_no(seg_header, i,
							FIL_NULL, mtr);
				break;
			}
		}

		fsp_free_page(space, page, mtr);
				
		return;
	}

	/* If we get here, the page is in some extent of the segment */	
	ut_a(0 == ut_dulint_cmp(
		mtr_read_dulint(descr + XDES_ID, MLOG_8BYTES, mtr),
		mtr_read_dulint(seg_header + FSEG_ID, MLOG_8BYTES, mtr)));

	not_full_n_used = mtr_read_ulint(seg_header + FSEG_NOT_FULL_N_USED,
					MLOG_4BYTES, mtr);
	if (xdes_is_full(descr, mtr)) {
		/* The fragment is full: move it to another list */
		flst_remove(seg_header + FSEG_FULL,
				descr + XDES_FLST_NODE, mtr);
		flst_add_last(seg_header + FSEG_NOT_FULL,
				descr + XDES_FLST_NODE, mtr);
		mlog_write_ulint(seg_header + FSEG_NOT_FULL_N_USED,
				not_full_n_used + FSP_EXTENT_SIZE - 1,
				MLOG_4BYTES, mtr);
	} else {
		ut_a(not_full_n_used > 0);
		mlog_write_ulint(seg_header + FSEG_NOT_FULL_N_USED,
				not_full_n_used - 1,
				MLOG_4BYTES, mtr);
	}

	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);

	if (xdes_is_free(descr, mtr)) {
	    	/* The extent has become free: free it to space */
		flst_remove(seg_header + FSEG_NOT_FULL,
				descr + XDES_FLST_NODE, mtr);
		fsp_free_extent(space, page, mtr);
	}		
}

/**************************************************************************
Frees an extent of a segment to the space free list. */
static
void
fseg_free_extent(
/*=============*/
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset in the extent */
	mtr_t*		mtr)	/* in: mtr handle */
{
	buf_block_t*	block;
	xdes_t*		descr;
	ulint		not_full_n_used;
	ulint		descr_n_used;
	
	ut_ad(seg_header && mtr);

	block = buf_block_align(seg_header);
	buf_page_x_lock(block, mtr);	

	descr = xdes_get_descriptor(space, page, mtr);

	ut_a(xdes_get_state(descr, mtr) == XDES_FSEG);
	ut_a(0 == ut_dulint_cmp(
		mtr_read_dulint(descr + XDES_ID, MLOG_8BYTES, mtr),
	     	mtr_read_dulint(seg_header + FSEG_ID, MLOG_8BYTES, mtr)));

	if (xdes_is_full(descr, mtr)) {
		flst_remove(seg_header + FSEG_FULL,
				descr + XDES_FLST_NODE, mtr);
	} else if (xdes_is_free(descr, mtr)) {
		flst_remove(seg_header + FSEG_FREE,
				descr + XDES_FLST_NODE, mtr);
	} else {
		flst_remove(seg_header + FSEG_NOT_FULL,
				descr + XDES_FLST_NODE, mtr);

		not_full_n_used = mtr_read_ulint(
					seg_header + FSEG_NOT_FULL_N_USED,
					MLOG_4BYTES, mtr);

		descr_n_used = xdes_get_n_used(descr, mtr);
		ut_a(not_full_n_used >= descr_n_used);
		mlog_write_ulint(seg_header + FSEG_NOT_FULL_N_USED,
				not_full_n_used - descr_n_used,
				MLOG_4BYTES, mtr);
	}
	fsp_free_extent(space, page, mtr);
}

/**************************************************************************
Frees part of a segment. This function can be used to free a segment
by repeatedly calling this function in different mini-transactions.
Doing the freeing in a single mini-transaction might result in too big
a mini-transaction. */

bool
fseg_free_step(
/*===========*/
			/* out: TRUE if freeing completed */
	ulint	space,	/* in: segment space id */
	ulint	page_no,/* in: segment header page number */
	ulint	offset,	/* in: segment header byte offset on page */
	mtr_t*	mtr)	/* in: mtr */
{
	buf_block_t*	block;
	ulint		n;
	ulint		page;
	xdes_t*		descr;
	fseg_header_t*	header;
	fil_addr_t	header_addr;

	header_addr.page = page_no;
	header_addr.boffset = offset;
	
	mtr_x_lock(&fsp_latch, mtr);

	header = fut_get_ptr_x_lock(space, header_addr, mtr);
	
	descr = fseg_get_first_extent(header, mtr);

	if (descr != NULL) {
		/* Free the extent held by the segment */
		page = xdes_get_offset(descr);

		fseg_free_extent(header, space, page, mtr);
	
		return(FALSE);
	}

	/* Free a frag page */

	n = fseg_get_last_used_frag_page_slot(header, mtr);

	if (n == ULINT_UNDEFINED) {
		/* Freeing completed: free the segment header */
		fsp_free_seg_header(space, header, mtr);

		return(TRUE);
	}

	fseg_free_page_low(header, space,
			fseg_get_nth_frag_page_no(header, n, mtr), mtr);

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
	ulint	offset)	/* in: byte offset of the segment header on that
			page */
{
	mtr_t		mtr;
	buf_block_t*	block;
	bool		finished;

	for (;;) {
		mtr_start(&mtr);

		block = buf_page_get(space, page_no, &mtr);
	
		finished = fseg_free_step(space, page_no, offset, &mtr);
	
		mtr_commit(&mtr);

		if (finished) {
			break;
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
	fseg_header_t*	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	fil_addr_t	first;
	ulint		space;
	xdes_t*		descr;
	
	ut_ad(header && mtr);

	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	space = buf_page_get_space(block);

	first = fil_addr_null;
	
	if (flst_get_len(header + FSEG_FULL, mtr) > 0) {
		first = flst_get_first(header + FSEG_FULL, mtr);
	} else if (flst_get_len(header + FSEG_NOT_FULL, mtr) > 0) {
		first = flst_get_first(header + FSEG_NOT_FULL, mtr);
	} else if (flst_get_len(header + FSEG_FREE, mtr) > 0) {
		first = flst_get_first(header + FSEG_FREE, mtr);
	}

	if (first.page == FIL_NULL) {
		return(NULL);
	} else {
		descr = xdes_lst_get_descriptor(space, first, mtr);
		return(descr);
	}
}

#ifdef notdefined

/**************************************************************************
Returns the last non-free extent descriptor for a segment. We think of
the extent lists of the segment catenated in the order FSEG_FULL ->
FSEG_NOT_FULL -> FSEG_FREE. */
static
xdes_t*
fseg_get_last_non_free_extent(
/*==========================*/
				/* out: the last extent descriptor, or NULL if
				none */
	fseg_header_t*	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	fil_addr_t	last;
	ulint		space;
	xdes_t*		descr;
	
	ut_ad(header && mtr);

	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	space = buf_page_get_space(block);

	last = fil_addr_null;
	
	if (flst_get_len(header + FSEG_NOT_FULL, mtr) > 0) {
		last = flst_get_last(header + FSEG_NOT_FULL, mtr);
	} else if (flst_get_len(header + FSEG_FULL, mtr) > 0) {
		last = flst_get_last(header + FSEG_FULL, mtr);
	}

	if (last.page == FIL_NULL) {
		return(NULL);
	} else {
		descr = xdes_lst_get_descriptor(space, last, mtr);
		return(descr);
	}
}

/**************************************************************************
Returns the next extent descriptor for a segment. We think of the extent
lists of the segment catenated in the order FSEG_FULL -> FSEG_NOT_FULL
-> FSEG_FREE. */
static
xdes_t*
fseg_get_next_extent(
/*=================*/
				/* out: next extent descriptor, or NULL if
				none */
	fseg_header_t*	header,	/* in: segment header */
	xdes_t*		descr,	/* in: previous extent descriptor */
	mtr_t*		mtr)	/* in: mtr */
{	
	fil_addr_t	next_addr;
	buf_block_t*	block;
	ulint		space;
	
	ut_ad(header && descr && mtr);
	
	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	space = buf_page_get_space(block);
	
	next_addr = flst_get_next_addr(descr + XDES_FLST_NODE, mtr);

	if (next_addr.page == FIL_NULL) {
		 /* This is the last extent in the list. */
		 if (xdes_is_full(descr, mtr)) {
		 	/* descr is in FSEG_FULL list */
			if (flst_get_len(header + FSEG_NOT_FULL, mtr) > 0) {
				next_addr = flst_get_first(header
							+ FSEG_NOT_FULL, mtr);
			} else if (flst_get_len(header + FSEG_FREE, mtr) > 0) {
				next_addr = flst_get_first(header
							+ FSEG_FREE, mtr);
			}
		 } else if (!xdes_is_full(descr, mtr)
			    && !xdes_is_free(descr, mtr)) {
			/* descr is in FSEG_NOT_FULL list */
		 	if (flst_get_len(header + FSEG_FREE, mtr) > 0) {
				next_addr = flst_get_first(header
							+ FSEG_FREE, mtr);
			}
		 }
	}

	if (next_addr.page != FIL_NULL) {
		descr = xdes_lst_get_descriptor(space, next_addr, mtr);
		ut_ad(descr);
		return(descr);
	} else {
		return(NULL);
	}
}

/**************************************************************************
Returns the previous extent descriptor for a segment. We think of the extent
lists of the segment catenated in the order FSEG_FULL -> FSEG_NOT_FULL
-> FSEG_FREE. */
static
xdes_t*
fseg_get_prev_extent(
/*=================*/
				/* out: previous extent descriptor, or NULL if
				none */
	fseg_header_t*	header,	/* in: segment header */
	xdes_t*		descr,	/* in: extent descriptor */
	mtr_t*		mtr)	/* in: mtr */
{	
	fil_addr_t	prev_addr;
	buf_block_t*	block;
	ulint		space;
	
	ut_ad(header && descr && mtr);	

	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	space = buf_page_get_space(block);
	
	prev_addr = flst_get_prev_addr(descr + XDES_FLST_NODE, mtr);

	if (prev_addr.page == FIL_NULL) {
		 /* This is the first extent in the list. */
		 if (xdes_is_free(descr, mtr)) {
		 	/* descr is in FSEG_FREE list */
			if (flst_get_len(header + FSEG_NOT_FULL, mtr) > 0) {
				prev_addr = flst_get_last(header
							+ FSEG_NOT_FULL, mtr);
			} else if (flst_get_len(header + FSEG_FULL, mtr) > 0) {
				prev_addr = flst_get_last(header
							+ FSEG_FULL, mtr);
			}
		 } else if (!xdes_is_full(descr, mtr)
			    && !xdes_is_free(descr, mtr)) {
			/* descr is in FSEG_NOT_FULL list */
		 	if (flst_get_len(header + FSEG_FULL, mtr) > 0) {
				prev_addr = flst_get_last(header
							+ FSEG_FULL, mtr);
			}
		 }
	}

	if (prev_addr.page != FIL_NULL) {
		descr = xdes_lst_get_descriptor(space, prev_addr, mtr);
		ut_ad(descr);
		return(descr);
	} else {
		return(NULL);
	}
}

/*************************************************************************
Gets the first used page number in the given extent assigned to a
specific segment, or its successors, in the order defined in
fsp_get_next_extent. */
static
ulint
fseg_extent_get_next_page_no(
/*=========================*/
				/* next used page number in the given extent
				or a successor of it, FIL_NULL if no page
				found */
	fseg_header_t*	header,	/* in: segment header */
	xdes_t*		descr,	/* in: extent descriptor, if this is NULL, the
				function returns FIL_NULL */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	bit;

	UT_NOT_USED(header);
	ut_ad((descr == NULL) || (xdes_get_state(descr, mtr) == XDES_FSEG));

	for (;;) {
		if (descr == NULL) {
			return(FIL_NULL);
		}

		bit = xdes_find_bit(descr, XDES_FREE_BIT, FALSE, 0, mtr);
					
		if (bit == ULINT_UNDEFINED) {
			/* No page found in this extent: the extent is in
			FSEG_FREE list, thus, no used page can be found
			in successors */
			return(FIL_NULL);
		} else {
			return(xdes_get_offset(descr) + bit);
		}
	}
}

/*************************************************************************
Gets the last used page number in the given extent assigned to a
specific segment, or its predecessor extents, in the order defined in
fsp_get_next_extent. If the page cannot be found from the extents,
the last page of the fragment list is returned, or FIL_NULL if it is
empty.*/
static
ulint
fseg_extent_get_prev_page_no(
/*=========================*/
				/* previous used page number in the given
				extent or a predecessor, FIL_NULL
				if no page found */
	fseg_header_t*	header,	/* in: segment header */
	xdes_t*		descr,	/* in: extent descriptor, if this is NULL, the
				function returns the last page of the fragment
				list, if any */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint		prev_page_no;
	ulint		bit;
	fil_addr_t	last_frag_page_addr;

	ut_ad((descr == NULL) || (xdes_get_state(descr, mtr) == XDES_FSEG));

	for (;;) {
		if (descr == NULL) {
			prev_page_no = FIL_NULL;
			break;
		}

		bit = xdes_find_bit_downward(descr, XDES_FREE_BIT, FALSE,
						FSP_EXTENT_SIZE - 1, mtr);
					
		if (bit == ULINT_UNDEFINED) {
			descr = fseg_get_prev_extent(header, descr, mtr);
		} else {
			prev_page_no = xdes_get_offset(descr) + bit;
			break;
		}
	}

	if (prev_page_no == FIL_NULL) {
		last_frag_page_addr = flst_get_last(header + FSEG_FRAG, mtr);
		prev_page_no = last_frag_page_addr.page;
	}

	return(prev_page_no);
}

/**************************************************************************
Returns the page number of the first segment page. If no pages have been
freed from the segment, and the pages were allocated with the hint page
number always one greater than previous page, then it is guaranteed that
this function returns the first allocated page. */

ulint
fseg_get_first_page_no(
/*===================*/
				/* out: page number, FIL_NULL if no
				page found */
	fseg_header_t*	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	ulint		first_page_no;
	xdes_t*		descr;
	fil_addr_t	first_frag_page_addr;
	
	ut_ad(header);

	mtr_x_lock(&fsp_latch, mtr);	

	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	/* Find first page */
	first_frag_page_addr = flst_get_first(header + FSEG_FRAG, mtr);
	first_page_no = first_frag_page_addr.page;
	
	if (first_page_no == FIL_NULL) {
		descr = fseg_get_first_extent(header, mtr);
		first_page_no = fseg_extent_get_next_page_no(header, descr,
								mtr);
	}
		
	return(first_page_no);
}

/**************************************************************************
Returns the page number of the last segment page. If no pages have been
freed from the segment, and the pages were allocated with the hint page
number always one greater than previous page, then it is guaranteed that
this function returns the last allocated page. */

ulint
fseg_get_last_page_no(
/*==================*/
				/* out: page number, FIL_NULL if no
				page found */
	fseg_header_t*	header,	/* in: segment header */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	ulint		last_page_no;
	xdes_t*		descr;
	
	ut_ad(header);

	mtr_x_lock(&fsp_latch, mtr);	

	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	descr = fseg_get_last_non_free_extent(header, mtr);
	last_page_no = fseg_extent_get_prev_page_no(header, descr, mtr);
		
	return(last_page_no);
}

/**************************************************************************
Returns the page number of the next segment page. If no pages have been
freed from the segment, and the pages were allocated with the hint page
number always one greater than previous page, then it is guaranteed that
this function steps the pages through in the order they were allocated
to the segment. */

ulint
fseg_get_next_page_no(
/*==================*/
				/* out: page number, FIL_NULL if no
				page left */
	fseg_header_t*	header,	/* in: segment header */
	ulint		page_no,/* in: previous page number */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	buf_frame_t*	frame;
	ulint		space;
	ulint		next_page_no;
	xdes_t*		descr;
	ulint		bit;
	fil_addr_t	next_frag_page_addr;
	fseg_page_header_t* page_header;
	
	ut_ad(header);

	mtr_x_lock(&fsp_latch, mtr);	

	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	space = buf_page_get_space(block);

	descr = xdes_get_descriptor(space, page_no, mtr);
	ut_ad(xdes_get_bit(descr, XDES_FREE_BIT,
	      		   page_no % FSP_EXTENT_SIZE, mtr) == FALSE);
	      		   
	if (xdes_get_state(descr, mtr) == XDES_FSEG) {
		/* The extent of the current page belongs to the segment */
		bit = xdes_find_bit(descr, XDES_FREE_BIT, FALSE,
				    (page_no + 1) % FSP_EXTENT_SIZE,
				    mtr);
		if ((bit == ULINT_UNDEFINED)
		    || (bit <= (page_no % FSP_EXTENT_SIZE))) {       
		    	/* No higher address pages in this extent */
			descr = fseg_get_next_extent(header, descr, mtr);
			next_page_no = fseg_extent_get_next_page_no(
						header, descr, mtr);
		} else {
			next_page_no = xdes_get_offset(descr) + bit;
		}
	} else {
		/* Current page is a fragment page */
		block = buf_page_get(space, page_no, mtr);
		buf_page_x_lock(block, mtr);
		frame = buf_block_get_frame(block);
		page_header = frame + FSEG_PAGE_HEADER_OFFSET;
		next_frag_page_addr = flst_get_next_addr(
					page_header + FSEG_PAGE_FRAG_NODE,
					mtr);
			
		next_page_no = next_frag_page_addr.page;
		if (next_page_no == FIL_NULL) {
			descr = fseg_get_first_extent(header, mtr);
			next_page_no = fseg_extent_get_next_page_no(
						header, descr, mtr);
		}
	}	
	return(next_page_no);
}

/**************************************************************************
Returns the page number of the previous segment page. If no pages have been
freed from the segment, and the pages were allocated with the hint page
number always one greater than the previous page, then it is guaranteed that
this function steps through the pages in the order opposite to the allocation
order of the pages. */

ulint
fseg_get_prev_page_no(
/*==================*/
				/* out: page number, FIL_NULL if no page
				left */
	fseg_header_t*	header,	/* in: segment header */
	ulint		page_no,/* in: page number */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	buf_frame_t*	frame;
	ulint		space;
	ulint		prev_page_no;
	xdes_t*		descr;
	ulint		bit;
	fil_addr_t	prev_frag_page_addr;
	fseg_page_header_t* page_header;
	
	ut_ad(header);

	mtr_x_lock(&fsp_latch, mtr);	

	block = buf_block_align(header);	
	buf_page_x_lock(block, mtr);

	space = buf_page_get_space(block);

	descr = xdes_get_descriptor(space, page_no, mtr);
	ut_ad(xdes_get_bit(descr, XDES_FREE_BIT,
	      		   page_no % FSP_EXTENT_SIZE, mtr) == FALSE);
	      		   
	if (xdes_get_state(descr, mtr) == XDES_FSEG) {
		/* The extent of the current page belongs to the segment */
		bit = xdes_find_bit_downward(descr, XDES_FREE_BIT, FALSE,
				    (page_no - 1) % FSP_EXTENT_SIZE,
				    mtr);
		if ((bit == ULINT_UNDEFINED)
		    || (bit >= (page_no % FSP_EXTENT_SIZE))) {       
		    	/* No lower address pages in this extent */
			descr = fseg_get_prev_extent(header, descr, mtr);
			prev_page_no = fseg_extent_get_prev_page_no(
						header, descr, mtr);
		} else {
			prev_page_no = xdes_get_offset(descr) + bit;
		}
	} else {
		/* Current page is a fragment page */
		block = buf_page_get(space, page_no, mtr);
		buf_page_x_lock(block, mtr);
		frame = buf_block_get_frame(block);
		page_header = frame + FSEG_PAGE_HEADER_OFFSET;
		prev_frag_page_addr = flst_get_prev_addr(
					page_header + FSEG_PAGE_FRAG_NODE,
					mtr);
			
		prev_page_no = prev_frag_page_addr.page;
	}	
	return(prev_page_no);
}

#endif

/***********************************************************************
Validates a segment. */
static
bool
fseg_validate_low(
/*==============*/
				/* out: TRUE if ok */
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr2)	/* in: mtr */
{
	ulint		space;
	dulint		seg_id;
	mtr_t		mtr;
	xdes_t*		descr;
	fil_addr_t	node_addr;
	ulint		n_used		= 0;
	ulint		n_used2		= 0;
	flst_node_t*	node;
	buf_frame_t*	frame;
	fseg_page_header_t*	page_header;
	
	ut_ad(mtr_memo_contains(mtr2, buf_block_align(header),
				MTR_MEMO_BUF_FIX));
	buf_page_x_lock(buf_block_align(header), mtr2);

	space = buf_page_get_space(buf_block_align(header));
	
	seg_id = mtr_read_dulint(header + FSEG_ID, MLOG_8BYTES, mtr2); 
	n_used = mtr_read_ulint(header + FSEG_NOT_FULL_N_USED,
					MLOG_4BYTES, mtr2); 

	flst_validate(header + FSEG_FRAG, mtr2);
	flst_validate(header + FSEG_FREE, mtr2);
	flst_validate(header + FSEG_NOT_FULL, mtr2);
	flst_validate(header + FSEG_FULL, mtr2);

	/* Validate FSEG_FREE list */
	node_addr = flst_get_first(header + FSEG_FREE, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == 0);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(0 == ut_dulint_cmp(
			mtr_read_dulint(descr + XDES_ID, MLOG_8BYTES,
					&mtr), seg_id));

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSEG_NOT_FULL list */

	node_addr = flst_get_first(header + FSEG_NOT_FULL, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) > 0);
		ut_a(xdes_get_n_used(descr, &mtr) < FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(0 == ut_dulint_cmp(
			mtr_read_dulint(descr + XDES_ID, MLOG_8BYTES,
					&mtr), seg_id));

		n_used2 += xdes_get_n_used(descr, &mtr);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSEG_FULL list */

	node_addr = flst_get_first(header + FSEG_FULL, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(0 == ut_dulint_cmp(
			mtr_read_dulint(descr + XDES_ID, MLOG_8BYTES,
					&mtr), seg_id));

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSEG_FRAG list */
	node_addr = flst_get_first(header + FSEG_FRAG, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		node = fut_get_ptr_x_lock(space, node_addr, &mtr);
		frame = buf_frame_align(node);
		page_header = frame + FSEG_PAGE_HEADER_OFFSET;
		ut_a(0 == ut_dulint_cmp(
			mtr_read_dulint(page_header + FSEG_PAGE_SEG_ID,
					MLOG_8BYTES, &mtr), seg_id));

		node_addr = flst_get_next_addr(node, &mtr);
		mtr_commit(&mtr);
	}
	
	ut_a(n_used == n_used2);

	return(TRUE);
}
	
/***********************************************************************
Validates a segment. */

bool
fseg_validate(
/*==========*/
				/* out: TRUE if ok */
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr2)	/* in: mtr */
{
	bool	ret;

	mtr_x_lock(&fsp_latch, mtr2);	
	ret = fseg_validate_low(header, mtr2);

	return(ret);
}

/***********************************************************************
Writes info of a segment. */
static
void
fseg_print_low(
/*===========*/
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint		space;
	ulint		seg_id_low;
	ulint		seg_id_high;
	ulint		n_used;
	ulint		n_frag;
	ulint		n_free;
	ulint		n_not_full;
	ulint		n_full;
	ulint		reserved;
	ulint		used;
	ulint		page_no;
	
	ut_ad(mtr_memo_contains(mtr, buf_block_align(header),
				MTR_MEMO_BUF_FIX));
	buf_page_x_lock(buf_block_align(header), mtr);

	space = buf_page_get_space(buf_block_align(header));
	page_no = buf_page_get_offset(buf_block_align(header));

	reserved = fseg_n_reserved_pages_low(header, &used, mtr);
	
	seg_id_low = ut_dulint_get_low(mtr_read_dulint(header + FSEG_ID,
						MLOG_8BYTES, mtr));
	seg_id_high = ut_dulint_get_high(mtr_read_dulint(header + FSEG_ID,
						MLOG_8BYTES, mtr));
	
	n_used = mtr_read_ulint(header + FSEG_NOT_FULL_N_USED,
					MLOG_4BYTES, mtr); 

	n_frag = flst_get_len(header + FSEG_FRAG, mtr);
	n_free = flst_get_len(header + FSEG_FREE, mtr);
	n_not_full = flst_get_len(header + FSEG_NOT_FULL, mtr);
	n_full = flst_get_len(header + FSEG_FULL, mtr);

	printf(
    "SEGMENT id %lu %lu space %lu; page %lu; res %lu used %lu; full ext %lu\n",
		seg_id_high, seg_id_low, space, page_no, reserved, used,
		n_full);
	printf(
    "fragm pages %lu; free extents %lu; not full extents %lu: pages %lu\n",
		n_frag, n_free, n_not_full, n_used);
}

/***********************************************************************
Writes info of a segment. */

void
fseg_print(
/*=======*/
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr)	/* in: mtr */
{
	mtr_x_lock(&fsp_latch, mtr);

	fseg_print_low(header, mtr);
}

/***********************************************************************
Validates the file space system and its segments. */

bool
fsp_validate(
/*=========*/
				/* out: TRUE if ok */
	ulint		space)	/* in: space id */
{
	fsp_header_t*	header;
	fseg_header_t*	seg_header;
	ulint		size;
	ulint		free_limit;
	ulint		frag_n_used;
	mtr_t		mtr;
	mtr_t		mtr2;
	xdes_t*		descr;
	fil_addr_t	node_addr;
	ulint		descr_count	= 0;
	ulint		n_used		= 0;
	ulint		n_used2		= 0;
	ulint		n_full_frag_pages;

	/* Start first a mini-transaction mtr2 to lock out all other threads
	from the fsp system */
	mtr_start(&mtr2);
	mtr_x_lock(&fsp_latch, &mtr2);	
	
	mtr_start(&mtr);
	mtr_x_lock(&fsp_latch, &mtr);	
	
	header = fsp_get_space_header(space, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr); 
	free_limit = mtr_read_ulint(header + FSP_FREE_LIMIT,
					MLOG_4BYTES, &mtr); 
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
					MLOG_4BYTES, &mtr); 

	n_full_frag_pages = FSP_EXTENT_SIZE *
				flst_get_len(header + FSP_FULL_FRAG, &mtr);
					
	ut_a(free_limit <= size);
					
	flst_validate(header + FSP_FREE, &mtr);
	flst_validate(header + FSP_FREE_FRAG, &mtr);
	flst_validate(header + FSP_FULL_FRAG, &mtr);
	flst_validate(header + FSP_SEGS, &mtr);

	mtr_commit(&mtr);

	/* Validate FSP_FREE list */
	mtr_start(&mtr);
	mtr_x_lock(&fsp_latch, &mtr);	

	header = fsp_get_space_header(space, &mtr);
	node_addr = flst_get_first(header + FSP_FREE, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		descr_count++;
		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == 0);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FREE);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSP_FREE_FRAG list */
	mtr_start(&mtr);
	mtr_x_lock(&fsp_latch, &mtr);	

	header = fsp_get_space_header(space, &mtr);
	node_addr = flst_get_first(header + FSP_FREE_FRAG, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

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
	mtr_x_lock(&fsp_latch, &mtr);	

	header = fsp_get_space_header(space, &mtr);
	node_addr = flst_get_first(header + FSP_FULL_FRAG, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		descr_count++;
		descr = xdes_lst_get_descriptor(space, node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FULL_FRAG);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}
	
	/* Validate segments */
	mtr_start(&mtr);
	mtr_x_lock(&fsp_latch, &mtr);	

	header = fsp_get_space_header(space, &mtr);
	node_addr = flst_get_first(header + FSP_SEGS, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		seg_header = fut_get_ptr_x_lock(space, node_addr,
						&mtr) - FSEG_FLST_NODE;
		fseg_validate_low(seg_header, &mtr);

		descr_count += flst_get_len(seg_header + FSEG_FREE, &mtr);
		descr_count += flst_get_len(seg_header + FSEG_FULL, &mtr);
		descr_count += flst_get_len(seg_header + FSEG_NOT_FULL, &mtr);

		n_used2 += flst_get_len(seg_header + FSEG_FRAG, &mtr);
		
		node_addr = flst_get_next_addr(seg_header + FSEG_FLST_NODE,
						&mtr);
		mtr_commit(&mtr);
	}
	
	ut_a(descr_count * FSP_EXTENT_SIZE == free_limit);
	ut_a(n_used + n_full_frag_pages
		== n_used2 + (free_limit + XDES_DESCRIBED_PER_PAGE - 1)
				 / XDES_DESCRIBED_PER_PAGE);
	ut_a(frag_n_used == n_used);

	mtr_commit(&mtr2);
	return(TRUE);
}

/***********************************************************************
Prints info of a file space. */

void
fsp_print(
/*======*/
	ulint		space)	/* in: space id */
{
	fsp_header_t*	header;
	fseg_header_t*	seg_header;
	ulint		size;
	ulint		free_limit;
	ulint		frag_n_used;
	mtr_t		mtr;
	mtr_t		mtr2;
	fil_addr_t	node_addr;
	ulint		n_free;
	ulint		n_free_frag;
	ulint		n_full_frag;
	ulint		n_segs;
	ulint		seg_id_low;
	ulint		seg_id_high;
	
	/* Start first a mini-transaction mtr2 to lock out all other threads
	from the fsp system */
	mtr_start(&mtr2);
	mtr_x_lock(&fsp_latch, &mtr2);	

	mtr_start(&mtr);
	mtr_x_lock(&fsp_latch, &mtr);	
	
	header = fsp_get_space_header(space, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr); 
	free_limit = mtr_read_ulint(header + FSP_FREE_LIMIT,
					MLOG_4BYTES, &mtr); 
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
					MLOG_4BYTES, &mtr); 

	n_free = flst_get_len(header + FSP_FREE, &mtr);
	n_free_frag = flst_get_len(header + FSP_FREE_FRAG, &mtr);
	n_full_frag = flst_get_len(header + FSP_FULL_FRAG, &mtr);
	n_segs = flst_get_len(header + FSP_SEGS, &mtr);
	seg_id_low = ut_dulint_get_low(mtr_read_dulint(header + FSP_SEG_ID,
						MLOG_8BYTES, &mtr));
	seg_id_high = ut_dulint_get_high(mtr_read_dulint(header + FSP_SEG_ID,
						MLOG_8BYTES, &mtr));
	
	printf("FILE SPACE INFO: id %lu\n", space);

	printf("size %lu, free limit %lu, free extents %lu\n",
		size, free_limit, n_free);
	printf(
	"not full frag extents %lu: used pages %lu, full frag extents %lu\n",
		n_free_frag, frag_n_used, n_full_frag);

	printf("number of segments %lu, first seg id not used %lu %lu\n",
		n_segs, seg_id_high, seg_id_low);
	
	/* Print segments */
	node_addr = flst_get_first(header + FSP_SEGS, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(&fsp_latch, &mtr);	

		seg_header = fut_get_ptr_x_lock(space, node_addr,
						&mtr) - FSEG_FLST_NODE;
		fseg_print_low(seg_header, &mtr);
		
		node_addr = flst_get_next_addr(seg_header + FSEG_FLST_NODE,
						&mtr);
		mtr_commit(&mtr);
	}
	
	mtr_commit(&mtr2);	
}

