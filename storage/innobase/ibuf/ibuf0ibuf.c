/******************************************************
Insert buffer

(c) 1997 Innobase Oy

Created 7/19/1997 Heikki Tuuri
*******************************************************/

#include "ibuf0ibuf.h"

#ifdef UNIV_NONINL
#include "ibuf0ibuf.ic"
#endif

#include "buf0buf.h"
#include "buf0rea.h"
#include "fsp0fsp.h"
#include "trx0sys.h"
#include "fil0fil.h"
#include "thr0loc.h"
#include "rem0rec.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "btr0btr.h"
#include "sync0sync.h"
#include "dict0boot.h"
#include "fut0lst.h"
#include "lock0lock.h"
#include "log0recv.h"
#include "que0que.h"

/*	STRUCTURE OF AN INSERT BUFFER RECORD

In versions < 4.1.x:

1. The first field is the page number.
2. The second field is an array which stores type info for each subsequent
   field. We store the information which affects the ordering of records, and
   also the physical storage size of an SQL NULL value. E.g., for CHAR(10) it
   is 10 bytes.
3. Next we have the fields of the actual index record.

In versions >= 4.1.x:

Note that contary to what we planned in the 1990's, there will only be one
insert buffer tree, and that is in the system tablespace of InnoDB.

1. The first field is the space id.
2. The second field is a one-byte marker (0) which differentiates records from
   the < 4.1.x storage format.
3. The third field is the page number.
4. The fourth field contains the type info, where we have also added 2 bytes to
   store the charset. In the compressed table format of 5.0.x we must add more
   information here so that we can build a dummy 'index' struct which 5.0.x
   can use in the binary search on the index page in the ibuf merge phase.
5. The rest of the fields contain the fields of the actual index record.

In versions >= 5.0.3:

The first byte of the fourth field is an additional marker (0) if the record
is in the compact format.  The presence of this marker can be detected by
looking at the length of the field modulo DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE.

The high-order bit of the character set field in the type info is the
"nullable" flag for the field. */


/*	PREVENTING DEADLOCKS IN THE INSERT BUFFER SYSTEM

If an OS thread performs any operation that brings in disk pages from
non-system tablespaces into the buffer pool, or creates such a page there,
then the operation may have as a side effect an insert buffer index tree
compression. Thus, the tree latch of the insert buffer tree may be acquired
in the x-mode, and also the file space latch of the system tablespace may
be acquired in the x-mode.

Also, an insert to an index in a non-system tablespace can have the same
effect. How do we know this cannot lead to a deadlock of OS threads? There
is a problem with the i\o-handler threads: they break the latching order
because they own x-latches to pages which are on a lower level than the
insert buffer tree latch, its page latches, and the tablespace latch an
insert buffer operation can reserve.

The solution is the following: Let all the tree and page latches connected
with the insert buffer be later in the latching order than the fsp latch and
fsp page latches.

Insert buffer pages must be such that the insert buffer is never invoked
when these pages are accessed as this would result in a recursion violating
the latching order. We let a special i/o-handler thread take care of i/o to
the insert buffer pages and the ibuf bitmap pages, as well as the fsp bitmap
pages and the first inode page, which contains the inode of the ibuf tree: let
us call all these ibuf pages. To prevent deadlocks, we do not let a read-ahead
access both non-ibuf and ibuf pages.

Then an i/o-handler for the insert buffer never needs to access recursively the
insert buffer tree and thus obeys the latching order. On the other hand, other
i/o-handlers for other tablespaces may require access to the insert buffer,
but because all kinds of latches they need to access there are later in the
latching order, no violation of the latching order occurs in this case,
either.

A problem is how to grow and contract an insert buffer tree. As it is later
in the latching order than the fsp management, we have to reserve the fsp
latch first, before adding or removing pages from the insert buffer tree.
We let the insert buffer tree have its own file space management: a free
list of pages linked to the tree root. To prevent recursive using of the
insert buffer when adding pages to the tree, we must first load these pages
to memory, obtaining a latch on them, and only after that add them to the
free list of the insert buffer tree. More difficult is removing of pages
from the free list. If there is an excess of pages in the free list of the
ibuf tree, they might be needed if some thread reserves the fsp latch,
intending to allocate more file space. So we do the following: if a thread
reserves the fsp latch, we check the writer count field of the latch. If
this field has value 1, it means that the thread did not own the latch
before entering the fsp system, and the mtr of the thread contains no
modifications to the fsp pages. Now we are free to reserve the ibuf latch,
and check if there is an excess of pages in the free list. We can then, in a
separate mini-transaction, take them out of the free list and free them to
the fsp system.

To avoid deadlocks in the ibuf system, we divide file pages into three levels:

(1) non-ibuf pages,
(2) ibuf tree pages and the pages in the ibuf tree free list, and
(3) ibuf bitmap pages.

No OS thread is allowed to access higher level pages if it has latches to
lower level pages; even if the thread owns a B-tree latch it must not access
the B-tree non-leaf pages if it has latches on lower level pages. Read-ahead
is only allowed for level 1 and 2 pages. Dedicated i/o-handler threads handle
exclusively level 1 i/o. A dedicated i/o handler thread handles exclusively
level 2 i/o. However, if an OS thread does the i/o handling for itself, i.e.,
it uses synchronous aio, it can access any pages, as long as it obeys the
access order rules. */

/* Buffer pool size per the maximum insert buffer size */
#define IBUF_POOL_SIZE_PER_MAX_SIZE	2

/* The insert buffer control structure */
ibuf_t*	ibuf			= NULL;

static ulint ibuf_rnd		= 986058871;

ulint	ibuf_flush_count	= 0;

#ifdef UNIV_IBUF_DEBUG
/* Dimensions for the ibuf_count array */
#define IBUF_COUNT_N_SPACES	500
#define IBUF_COUNT_N_PAGES	2000

/* Buffered entry counts for file pages, used in debugging */
static ulint*	ibuf_counts[IBUF_COUNT_N_SPACES];

static ibool	ibuf_counts_inited	= FALSE;
#endif

/* The start address for an insert buffer bitmap page bitmap */
#define IBUF_BITMAP		PAGE_DATA

/* Offsets in bits for the bits describing a single page in the bitmap */
#define	IBUF_BITMAP_FREE	0
#define IBUF_BITMAP_BUFFERED	2
#define IBUF_BITMAP_IBUF	3	/* TRUE if page is a part of the ibuf
					tree, excluding the root page, or is
					in the free list of the ibuf */

/* Number of bits describing a single page */
#define IBUF_BITS_PER_PAGE	4
#if IBUF_BITS_PER_PAGE % 2
# error "IBUF_BITS_PER_PAGE must be an even number!"
#endif

/* The mutex used to block pessimistic inserts to ibuf trees */
static mutex_t	ibuf_pessimistic_insert_mutex;

/* The mutex protecting the insert buffer structs */
static mutex_t	ibuf_mutex;

/* The mutex protecting the insert buffer bitmaps */
static mutex_t	ibuf_bitmap_mutex;

/* The area in pages from which contract looks for page numbers for merge */
#define	IBUF_MERGE_AREA			8

/* Inside the merge area, pages which have at most 1 per this number less
buffered entries compared to maximum volume that can buffered for a single
page are merged along with the page whose buffer became full */
#define IBUF_MERGE_THRESHOLD		4

/* In ibuf_contract at most this number of pages is read to memory in one
batch, in order to merge the entries for them in the insert buffer */
#define	IBUF_MAX_N_PAGES_MERGED		IBUF_MERGE_AREA

/* If the combined size of the ibuf trees exceeds ibuf->max_size by this
many pages, we start to contract it in connection to inserts there, using
non-synchronous contract */
#define IBUF_CONTRACT_ON_INSERT_NON_SYNC	0

/* Same as above, but use synchronous contract */
#define IBUF_CONTRACT_ON_INSERT_SYNC		5

/* Same as above, but no insert is done, only contract is called */
#define IBUF_CONTRACT_DO_NOT_INSERT		10

/* TODO: how to cope with drop table if there are records in the insert
buffer for the indexes of the table? Is there actually any problem,
because ibuf merge is done to a page when it is read in, and it is
still physically like the index page even if the index would have been
dropped! So, there seems to be no problem. */

/**********************************************************************
Validates the ibuf data structures when the caller owns ibuf_mutex. */

ibool
ibuf_validate_low(void);
/*===================*/
			/* out: TRUE if ok */

/**********************************************************************
Sets the flag in the current OS thread local storage denoting that it is
inside an insert buffer routine. */
UNIV_INLINE
void
ibuf_enter(void)
/*============*/
{
	ibool*	ptr;

	ptr = thr_local_get_in_ibuf_field();

	ut_ad(*ptr == FALSE);

	*ptr = TRUE;
}

/**********************************************************************
Sets the flag in the current OS thread local storage denoting that it is
exiting an insert buffer routine. */
UNIV_INLINE
void
ibuf_exit(void)
/*===========*/
{
	ibool*	ptr;

	ptr = thr_local_get_in_ibuf_field();

	ut_ad(*ptr == TRUE);

	*ptr = FALSE;
}

/**********************************************************************
Returns TRUE if the current OS thread is performing an insert buffer
routine. */

ibool
ibuf_inside(void)
/*=============*/
		/* out: TRUE if inside an insert buffer routine: for instance,
		a read-ahead of non-ibuf pages is then forbidden */
{
	return(*thr_local_get_in_ibuf_field());
}

/**********************************************************************
Gets the ibuf header page and x-latches it. */
static
page_t*
ibuf_header_page_get(
/*=================*/
			/* out: insert buffer header page */
	ulint	space,	/* in: space id */
	mtr_t*	mtr)	/* in: mtr */
{
	page_t*	page;

	ut_a(space == 0);

	ut_ad(!ibuf_inside());

	page = buf_page_get(space, FSP_IBUF_HEADER_PAGE_NO, RW_X_LATCH, mtr);

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_IBUF_HEADER);
#endif /* UNIV_SYNC_DEBUG */

	return(page);
}

/**********************************************************************
Gets the root page and x-latches it. */
static
page_t*
ibuf_tree_root_get(
/*===============*/
				/* out: insert buffer tree root page */
	ibuf_data_t*	data,	/* in: ibuf data */
	ulint		space,	/* in: space id */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*	page;

	ut_a(space == 0);
	ut_ad(ibuf_inside());

	mtr_x_lock(dict_tree_get_lock((data->index)->tree), mtr);

	page = buf_page_get(space, FSP_IBUF_TREE_ROOT_PAGE_NO, RW_X_LATCH,
									mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_TREE_NODE);
#endif /* UNIV_SYNC_DEBUG */

	return(page);
}

#ifdef UNIV_IBUF_DEBUG
/**********************************************************************
Gets the ibuf count for a given page. */

ulint
ibuf_count_get(
/*===========*/
			/* out: number of entries in the insert buffer
			currently buffered for this page */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	ut_ad(space < IBUF_COUNT_N_SPACES);
	ut_ad(page_no < IBUF_COUNT_N_PAGES);

	if (!ibuf_counts_inited) {

		return(0);
	}

	return(*(ibuf_counts[space] + page_no));
}

/**********************************************************************
Sets the ibuf count for a given page. */
static
void
ibuf_count_set(
/*===========*/
	ulint	space,	/* in: space id */
	ulint	page_no,/* in: page number */
	ulint	val)	/* in: value to set */
{
	ut_a(space < IBUF_COUNT_N_SPACES);
	ut_a(page_no < IBUF_COUNT_N_PAGES);
	ut_a(val < UNIV_PAGE_SIZE);

	*(ibuf_counts[space] + page_no) = val;
}
#endif

/**********************************************************************
Creates the insert buffer data structure at a database startup and initializes
the data structures for the insert buffer. */

void
ibuf_init_at_db_start(void)
/*=======================*/
{
	ibuf = mem_alloc(sizeof(ibuf_t));

	/* Note that also a pessimistic delete can sometimes make a B-tree
	grow in size, as the references on the upper levels of the tree can
	change */

	ibuf->max_size = buf_pool_get_curr_size() / UNIV_PAGE_SIZE
						/ IBUF_POOL_SIZE_PER_MAX_SIZE;

	UT_LIST_INIT(ibuf->data_list);

	ibuf->size = 0;

#ifdef UNIV_IBUF_DEBUG
	{
		ulint	i, j;

		for (i = 0; i < IBUF_COUNT_N_SPACES; i++) {

			ibuf_counts[i] = mem_alloc(sizeof(ulint)
						* IBUF_COUNT_N_PAGES);
			for (j = 0; j < IBUF_COUNT_N_PAGES; j++) {
				ibuf_count_set(i, j, 0);
			}
		}

		ibuf_counts_inited = TRUE;
	}
#endif
	mutex_create(&ibuf_pessimistic_insert_mutex,
		SYNC_IBUF_PESS_INSERT_MUTEX);

	mutex_create(&ibuf_mutex, SYNC_IBUF_MUTEX);

	mutex_create(&ibuf_bitmap_mutex, SYNC_IBUF_BITMAP_MUTEX);

	fil_ibuf_init_at_db_start();
}

/**********************************************************************
Updates the size information in an ibuf data, assuming the segment size has
not changed. */
static
void
ibuf_data_sizes_update(
/*===================*/
	ibuf_data_t*	data,	/* in: ibuf data struct */
	page_t*		root,	/* in: ibuf tree root */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	old_size;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&ibuf_mutex));
#endif /* UNIV_SYNC_DEBUG */

	old_size = data->size;

	data->free_list_len = flst_get_len(root + PAGE_HEADER
					   + PAGE_BTR_IBUF_FREE_LIST, mtr);

	data->height = 1 + btr_page_get_level(root, mtr);

	data->size = data->seg_size - (1 + data->free_list_len);
				/* the '1 +' is the ibuf header page */
	ut_ad(data->size < data->seg_size);

	if (page_get_n_recs(root) == 0) {

		data->empty = TRUE;
	} else {
		data->empty = FALSE;
	}

	ut_ad(ibuf->size + data->size >= old_size);

	ibuf->size = ibuf->size + data->size - old_size;

/*	fprintf(stderr, "ibuf size %lu, space ibuf size %lu\n", ibuf->size,
							data->size); */
}

/**********************************************************************
Creates the insert buffer data struct for a single tablespace. Reads the
root page of the insert buffer tree in the tablespace. This function can
be called only after the dictionary system has been initialized, as this
creates also the insert buffer table and index into this tablespace. */

ibuf_data_t*
ibuf_data_init_for_space(
/*=====================*/
			/* out, own: ibuf data struct, linked to the list
			in ibuf control structure */
	ulint	space)	/* in: space id */
{
	ibuf_data_t*	data;
	page_t*		root;
	page_t*		header_page;
	mtr_t		mtr;
	char		buf[50];
	dict_table_t*	table;
	dict_index_t*	index;
	ulint		n_used;

	ut_a(space == 0);

#ifdef UNIV_LOG_DEBUG
	if (space % 2 == 1) {

		fputs("No ibuf op in replicate space\n", stderr);

		return(NULL);
	}
#endif
	data = mem_alloc(sizeof(ibuf_data_t));

	data->space = space;

	mtr_start(&mtr);

	mutex_enter(&ibuf_mutex);

	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header_page = ibuf_header_page_get(space, &mtr);

	fseg_n_reserved_pages(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
								&n_used, &mtr);
	ibuf_enter();

	ut_ad(n_used >= 2);

	data->seg_size = n_used;

	root = buf_page_get(space, FSP_IBUF_TREE_ROOT_PAGE_NO, RW_X_LATCH,
								&mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(root, SYNC_TREE_NODE);
#endif /* UNIV_SYNC_DEBUG */

	data->size = 0;
	data->n_inserts = 0;
	data->n_merges = 0;
	data->n_merged_recs = 0;

	ibuf_data_sizes_update(data, root, &mtr);
/*
	if (!data->empty) {
		fprintf(stderr,
"InnoDB: index entries found in the insert buffer\n");
	} else {
		fprintf(stderr,
"InnoDB: insert buffer empty\n");
	}
*/
	mutex_exit(&ibuf_mutex);

	mtr_commit(&mtr);

	ibuf_exit();

	sprintf(buf, "SYS_IBUF_TABLE_%lu", (ulong) space);
	/* use old-style record format for the insert buffer */
	table = dict_mem_table_create(buf, space, 2, 0);

	dict_mem_table_add_col(table, "PAGE_NO", DATA_BINARY, 0, 0, 0);
	dict_mem_table_add_col(table, "TYPES", DATA_BINARY, 0, 0, 0);

	table->id = ut_dulint_add(DICT_IBUF_ID_MIN, space);

	dict_table_add_to_cache(table);

	index = dict_mem_index_create(buf, "CLUST_IND", space,
				DICT_CLUSTERED | DICT_UNIVERSAL | DICT_IBUF,2);

	dict_mem_index_add_field(index, "PAGE_NO", 0);
	dict_mem_index_add_field(index, "TYPES", 0);

	index->id = ut_dulint_add(DICT_IBUF_ID_MIN, space);

	dict_index_add_to_cache(table, index, FSP_IBUF_TREE_ROOT_PAGE_NO);

	data->index = dict_table_get_first_index(table);

	mutex_enter(&ibuf_mutex);

	UT_LIST_ADD_LAST(data_list, ibuf->data_list, data);

	mutex_exit(&ibuf_mutex);

	return(data);
}

/*************************************************************************
Initializes an ibuf bitmap page. */

void
ibuf_bitmap_page_init(
/*==================*/
	page_t*	page,	/* in: bitmap page */
	mtr_t*	mtr)	/* in: mtr */
{
	ulint	bit_offset;
	ulint	byte_offset;

	/* Write all zeros to the bitmap */

	bit_offset = XDES_DESCRIBED_PER_PAGE * IBUF_BITS_PER_PAGE;

	byte_offset = bit_offset / 8 + 1; /* better: (bit_offset + 7) / 8 */

	fil_page_set_type(page, FIL_PAGE_IBUF_BITMAP);

	memset(page + IBUF_BITMAP, 0, byte_offset);

	/* The remaining area (up to the page trailer) is uninitialized. */

	mlog_write_initial_log_record(page, MLOG_IBUF_BITMAP_INIT, mtr);
}

/*************************************************************************
Parses a redo log record of an ibuf bitmap page init. */

byte*
ibuf_parse_bitmap_init(
/*===================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr __attribute__((unused)), /* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ut_ad(ptr && end_ptr);

	if (page) {
		ibuf_bitmap_page_init(page, mtr);
	}

	return(ptr);
}

/************************************************************************
Gets the desired bits for a given page from a bitmap page. */
UNIV_INLINE
ulint
ibuf_bitmap_page_get_bits(
/*======================*/
			/* out: value of bits */
	page_t*	page,	/* in: bitmap page */
	ulint	page_no,/* in: page whose bits to get */
	ulint	bit,	/* in: IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ... */
	mtr_t*	mtr __attribute__((unused)))	/* in: mtr containing an
						x-latch to the bitmap
						page */
{
	ulint	byte_offset;
	ulint	bit_offset;
	ulint	map_byte;
	ulint	value;

	ut_ad(bit < IBUF_BITS_PER_PAGE);
#if IBUF_BITS_PER_PAGE % 2
# error "IBUF_BITS_PER_PAGE % 2 != 0"
#endif
	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
						MTR_MEMO_PAGE_X_FIX));

	bit_offset = (page_no % XDES_DESCRIBED_PER_PAGE) * IBUF_BITS_PER_PAGE
		+ bit;

	byte_offset = bit_offset / 8;
	bit_offset = bit_offset % 8;

	ut_ad(byte_offset + IBUF_BITMAP < UNIV_PAGE_SIZE);

	map_byte = mach_read_from_1(page + IBUF_BITMAP + byte_offset);

	value = ut_bit_get_nth(map_byte, bit_offset);

	if (bit == IBUF_BITMAP_FREE) {
		ut_ad(bit_offset + 1 < 8);

		value = value * 2 + ut_bit_get_nth(map_byte, bit_offset + 1);
	}

	return(value);
}

/************************************************************************
Sets the desired bit for a given page in a bitmap page. */
static
void
ibuf_bitmap_page_set_bits(
/*======================*/
	page_t*	page,	/* in: bitmap page */
	ulint	page_no,/* in: page whose bits to set */
	ulint	bit,	/* in: IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ... */
	ulint	val,	/* in: value to set */
	mtr_t*	mtr)	/* in: mtr containing an x-latch to the bitmap page */
{
	ulint	byte_offset;
	ulint	bit_offset;
	ulint	map_byte;

	ut_ad(bit < IBUF_BITS_PER_PAGE);
#if IBUF_BITS_PER_PAGE % 2
# error "IBUF_BITS_PER_PAGE % 2 != 0"
#endif
	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
						MTR_MEMO_PAGE_X_FIX));
#ifdef UNIV_IBUF_DEBUG
	ut_a((bit != IBUF_BITMAP_BUFFERED) || (val != FALSE)
		|| (0 == ibuf_count_get(buf_frame_get_space_id(page),
				page_no)));
#endif
	bit_offset = (page_no % XDES_DESCRIBED_PER_PAGE) * IBUF_BITS_PER_PAGE
		+ bit;

	byte_offset = bit_offset / 8;
	bit_offset = bit_offset % 8;

	ut_ad(byte_offset + IBUF_BITMAP < UNIV_PAGE_SIZE);

	map_byte = mach_read_from_1(page + IBUF_BITMAP + byte_offset);

	if (bit == IBUF_BITMAP_FREE) {
		ut_ad(bit_offset + 1 < 8);
		ut_ad(val <= 3);

		map_byte = ut_bit_set_nth(map_byte, bit_offset, val / 2);
		map_byte = ut_bit_set_nth(map_byte, bit_offset + 1, val % 2);
	} else {
		ut_ad(val <= 1);
		map_byte = ut_bit_set_nth(map_byte, bit_offset, val);
	}

	mlog_write_ulint(page + IBUF_BITMAP + byte_offset, map_byte,
							MLOG_1BYTE, mtr);
}

/************************************************************************
Calculates the bitmap page number for a given page number. */
UNIV_INLINE
ulint
ibuf_bitmap_page_no_calc(
/*=====================*/
				/* out: the bitmap page number where
				the file page is mapped */
	ulint	page_no)	/* in: tablespace page number */
{
	return(FSP_IBUF_BITMAP_OFFSET
		+ XDES_DESCRIBED_PER_PAGE
		* (page_no / XDES_DESCRIBED_PER_PAGE));
}

/************************************************************************
Gets the ibuf bitmap page where the bits describing a given file page are
stored. */
static
page_t*
ibuf_bitmap_get_map_page(
/*=====================*/
			/* out: bitmap page where the file page is mapped,
			that is, the bitmap page containing the descriptor
			bits for the file page; the bitmap page is
			x-latched */
	ulint	space,	/* in: space id of the file page */
	ulint	page_no,/* in: page number of the file page */
	mtr_t*	mtr)	/* in: mtr */
{
	page_t*	page;

	page = buf_page_get(space, ibuf_bitmap_page_no_calc(page_no),
							RW_X_LATCH, mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_IBUF_BITMAP);
#endif /* UNIV_SYNC_DEBUG */

	return(page);
}

/****************************************************************************
Sets the free bits of the page in the ibuf bitmap. This is done in a separate
mini-transaction, hence this operation does not restrict further work to only
ibuf bitmap operations, which would result if the latch to the bitmap page
were kept. */
UNIV_INLINE
void
ibuf_set_free_bits_low(
/*===================*/
	ulint	type,	/* in: index type */
	page_t*	page,	/* in: index page; free bit is set if the index is
			non-clustered and page level is 0 */
	ulint	val,	/* in: value to set: < 4 */
	mtr_t*	mtr)	/* in: mtr */
{
	page_t*	bitmap_page;

	if (type & DICT_CLUSTERED) {

		return;
	}

	if (btr_page_get_level_low(page) != 0) {

		return;
	}

	bitmap_page = ibuf_bitmap_get_map_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page), mtr);
#ifdef UNIV_IBUF_DEBUG
	/* fprintf(stderr,
		"Setting page no %lu free bits to %lu should be %lu\n",
					buf_frame_get_page_no(page), val,
				ibuf_index_page_calc_free(page)); */

	ut_a(val <= ibuf_index_page_calc_free(page));
#endif
	ibuf_bitmap_page_set_bits(bitmap_page, buf_frame_get_page_no(page),
						IBUF_BITMAP_FREE, val, mtr);

}

/****************************************************************************
Sets the free bit of the page in the ibuf bitmap. This is done in a separate
mini-transaction, hence this operation does not restrict further work to only
ibuf bitmap operations, which would result if the latch to the bitmap page
were kept. */

void
ibuf_set_free_bits(
/*===============*/
	ulint	type,	/* in: index type */
	page_t*	page,	/* in: index page; free bit is set if the index is
			non-clustered and page level is 0 */
	ulint	val,	/* in: value to set: < 4 */
	ulint	max_val)/* in: ULINT_UNDEFINED or a maximum value which
			the bits must have before setting; this is for
			debugging */
{
	mtr_t	mtr;
	page_t*	bitmap_page;

	if (type & DICT_CLUSTERED) {

		return;
	}

	if (btr_page_get_level_low(page) != 0) {

		return;
	}

	mtr_start(&mtr);

	bitmap_page = ibuf_bitmap_get_map_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page), &mtr);

	if (max_val != ULINT_UNDEFINED) {
#ifdef UNIV_IBUF_DEBUG
		ulint	old_val;

		old_val = ibuf_bitmap_page_get_bits(bitmap_page,
					buf_frame_get_page_no(page),
						IBUF_BITMAP_FREE, &mtr);
		if (old_val != max_val) {
			/* fprintf(stderr,
			"Ibuf: page %lu old val %lu max val %lu\n",
			buf_frame_get_page_no(page), old_val, max_val); */
		}

		ut_a(old_val <= max_val);
#endif
	}
#ifdef UNIV_IBUF_DEBUG
/*	fprintf(stderr, "Setting page no %lu free bits to %lu should be %lu\n",
					buf_frame_get_page_no(page), val,
				ibuf_index_page_calc_free(page)); */

	ut_a(val <= ibuf_index_page_calc_free(page));
#endif
	ibuf_bitmap_page_set_bits(bitmap_page, buf_frame_get_page_no(page),
						IBUF_BITMAP_FREE, val, &mtr);
	mtr_commit(&mtr);
}

/****************************************************************************
Resets the free bits of the page in the ibuf bitmap. This is done in a
separate mini-transaction, hence this operation does not restrict further
work to only ibuf bitmap operations, which would result if the latch to the
bitmap page were kept. */

void
ibuf_reset_free_bits_with_type(
/*===========================*/
	ulint	type,	/* in: index type */
	page_t*	page)	/* in: index page; free bits are set to 0 if the index
			is non-clustered and non-unique and the page level is
			0 */
{
	ibuf_set_free_bits(type, page, 0, ULINT_UNDEFINED);
}

/****************************************************************************
Resets the free bits of the page in the ibuf bitmap. This is done in a
separate mini-transaction, hence this operation does not restrict further
work to solely ibuf bitmap operations, which would result if the latch to
the bitmap page were kept. */

void
ibuf_reset_free_bits(
/*=================*/
	dict_index_t*	index,	/* in: index */
	page_t*		page)	/* in: index page; free bits are set to 0 if
				the index is non-clustered and non-unique and
				the page level is 0 */
{
	ibuf_set_free_bits(index->type, page, 0, ULINT_UNDEFINED);
}

/**************************************************************************
Updates the free bits for a page to reflect the present state. Does this
in the mtr given, which means that the latching order rules virtually prevent
any further operations for this OS thread until mtr is committed. */

void
ibuf_update_free_bits_low(
/*======================*/
	dict_index_t*	index,		/* in: index */
	page_t*		page,		/* in: index page */
	ulint		max_ins_size,	/* in: value of maximum insert size
					with reorganize before the latest
					operation performed to the page */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	before;
	ulint	after;

	before = ibuf_index_page_calc_free_bits(max_ins_size);

	after = ibuf_index_page_calc_free(page);

	if (before != after) {
		ibuf_set_free_bits_low(index->type, page, after, mtr);
	}
}

/**************************************************************************
Updates the free bits for the two pages to reflect the present state. Does
this in the mtr given, which means that the latching order rules virtually
prevent any further operations until mtr is committed. */

void
ibuf_update_free_bits_for_two_pages_low(
/*====================================*/
	dict_index_t*	index,	/* in: index */
	page_t*		page1,	/* in: index page */
	page_t*		page2,	/* in: index page */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	state;

	/* As we have to x-latch two random bitmap pages, we have to acquire
	the bitmap mutex to prevent a deadlock with a similar operation
	performed by another OS thread. */

	mutex_enter(&ibuf_bitmap_mutex);

	state = ibuf_index_page_calc_free(page1);

	ibuf_set_free_bits_low(index->type, page1, state, mtr);

	state = ibuf_index_page_calc_free(page2);

	ibuf_set_free_bits_low(index->type, page2, state, mtr);

	mutex_exit(&ibuf_bitmap_mutex);
}

/**************************************************************************
Returns TRUE if the page is one of the fixed address ibuf pages. */
UNIV_INLINE
ibool
ibuf_fixed_addr_page(
/*=================*/
			/* out: TRUE if a fixed address ibuf i/o page */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	return((space == 0 && page_no == IBUF_TREE_ROOT_PAGE_NO)
			|| ibuf_bitmap_page(page_no));
}

/***************************************************************************
Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages. */

ibool
ibuf_page(
/*======*/
			/* out: TRUE if level 2 or level 3 page */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	page_t*	bitmap_page;
	mtr_t	mtr;
	ibool	ret;

	if (recv_no_ibuf_operations) {
		/* Recovery is running: no ibuf operations should be
		performed */

		return(FALSE);
	}

	if (ibuf_fixed_addr_page(space, page_no)) {

		return(TRUE);
	}

	if (space != 0) {
		/* Currently we only have an ibuf tree in space 0 */

		return(FALSE);
	}

	ut_ad(fil_space_get_type(space) == FIL_TABLESPACE);

	mtr_start(&mtr);

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, &mtr);

	ret = ibuf_bitmap_page_get_bits(bitmap_page, page_no, IBUF_BITMAP_IBUF,
									&mtr);
	mtr_commit(&mtr);

	return(ret);
}

/***************************************************************************
Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages. */

ibool
ibuf_page_low(
/*==========*/
			/* out: TRUE if level 2 or level 3 page */
	ulint	space,	/* in: space id */
	ulint	page_no,/* in: page number */
	mtr_t*	mtr)	/* in: mtr which will contain an x-latch to the
			bitmap page if the page is not one of the fixed
			address ibuf pages */
{
	page_t*	bitmap_page;
	ibool	ret;

#ifdef UNIV_LOG_DEBUG
	if (space % 2 != 0) {

		fputs("No ibuf in a replicate space\n", stderr);

		return(FALSE);
	}
#endif
	if (ibuf_fixed_addr_page(space, page_no)) {

		return(TRUE);
	}

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, mtr);

	ret = ibuf_bitmap_page_get_bits(bitmap_page, page_no, IBUF_BITMAP_IBUF,
									mtr);
	return(ret);
}

/************************************************************************
Returns the page number field of an ibuf record. */
static
ulint
ibuf_rec_get_page_no(
/*=================*/
			/* out: page number */
	rec_t*	rec)	/* in: ibuf record */
{
	byte*	field;
	ulint	len;

	ut_ad(ibuf_inside());
	ut_ad(rec_get_n_fields_old(rec) > 2);

	field = rec_get_nth_field_old(rec, 1, &len);

	if (len == 1) {
		/* This is of the >= 4.1.x record format */
		ut_a(trx_sys_multiple_tablespace_format);

		field = rec_get_nth_field_old(rec, 2, &len);
	} else {
		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		field = rec_get_nth_field_old(rec, 0, &len);
	}

	ut_a(len == 4);

	return(mach_read_from_4(field));
}

/************************************************************************
Returns the space id field of an ibuf record. For < 4.1.x format records
returns 0. */
static
ulint
ibuf_rec_get_space(
/*===============*/
			/* out: space id */
	rec_t*	rec)	/* in: ibuf record */
{
	byte*	field;
	ulint	len;

	ut_ad(ibuf_inside());
	ut_ad(rec_get_n_fields_old(rec) > 2);

	field = rec_get_nth_field_old(rec, 1, &len);

	if (len == 1) {
		/* This is of the >= 4.1.x record format */

		ut_a(trx_sys_multiple_tablespace_format);
		field = rec_get_nth_field_old(rec, 0, &len);
		ut_a(len == 4);

		return(mach_read_from_4(field));
	}

	ut_a(trx_doublewrite_must_reset_space_ids);
	ut_a(!trx_sys_multiple_tablespace_format);

	return(0);
}

/************************************************************************
Creates a dummy index for inserting a record to a non-clustered index.
*/
static
dict_index_t*
ibuf_dummy_index_create(
/*====================*/
				/* out: dummy index */
	ulint		n,	/* in: number of fields */
	ibool		comp)	/* in: TRUE=use compact record format */
{
	dict_table_t*	table;
	dict_index_t*	index;

	table = dict_mem_table_create("IBUF_DUMMY",
		DICT_HDR_SPACE, n, comp ? DICT_TF_COMPACT : 0);

	index = dict_mem_index_create("IBUF_DUMMY", "IBUF_DUMMY",
		DICT_HDR_SPACE, 0, n);

	index->table = table;

	/* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
	index->cached = TRUE;

	return(index);
}
/************************************************************************
Add a column to the dummy index */
static
void
ibuf_dummy_index_add_col(
/*=====================*/
	dict_index_t*	index,	/* in: dummy index */
	dtype_t*	type,	/* in: the data type of the column */
	ulint		len)	/* in: length of the column */
{
	ulint	i	= index->table->n_def;
	dict_mem_table_add_col(index->table, "DUMMY",
		dtype_get_mtype(type),
		dtype_get_prtype(type),
		dtype_get_len(type),
		dtype_get_prec(type));
	dict_index_add_col(index,
		dict_table_get_nth_col(index->table, i), len);
}
/************************************************************************
Deallocates a dummy index for inserting a record to a non-clustered index.
*/
static
void
ibuf_dummy_index_free(
/*==================*/
	dict_index_t*	index)	/* in: dummy index */
{
	dict_table_t*	table = index->table;

	dict_mem_index_free(index);
	dict_mem_table_free(table);
}

/*************************************************************************
Builds the entry to insert into a non-clustered index when we have the
corresponding record in an ibuf index. */
static
dtuple_t*
ibuf_build_entry_from_ibuf_rec(
/*===========================*/
					/* out, own: entry to insert to
					a non-clustered index; NOTE that
					as we copy pointers to fields in
					ibuf_rec, the caller must hold a
					latch to the ibuf_rec page as long
					as the entry is used! */
	rec_t*		ibuf_rec,	/* in: record in an insert buffer */
	mem_heap_t*	heap,		/* in: heap where built */
	dict_index_t**	pindex)		/* out, own: dummy index that
					describes the entry */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	ulint		n_fields;
	byte*		types;
	const byte*	data;
	ulint		len;
	ulint		i;
	dict_index_t*	index;

	data = rec_get_nth_field_old(ibuf_rec, 1, &len);

	if (len > 1) {
		/* This a < 4.1.x format record */

		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		n_fields = rec_get_n_fields_old(ibuf_rec) - 2;
		tuple = dtuple_create(heap, n_fields);
		types = rec_get_nth_field_old(ibuf_rec, 1, &len);

		ut_a(len == n_fields * DATA_ORDER_NULL_TYPE_BUF_SIZE);

		for (i = 0; i < n_fields; i++) {
			field = dtuple_get_nth_field(tuple, i);

			data = rec_get_nth_field_old(ibuf_rec, i + 2, &len);

			dfield_set_data(field, data, len);

			dtype_read_for_order_and_null_size(
				dfield_get_type(field),
				types + i * DATA_ORDER_NULL_TYPE_BUF_SIZE);
		}

		*pindex = ibuf_dummy_index_create(n_fields, FALSE);
		return(tuple);
	}

	/* This a >= 4.1.x format record */

	ut_a(trx_sys_multiple_tablespace_format);
	ut_a(*data == 0);
	ut_a(rec_get_n_fields_old(ibuf_rec) > 4);

	n_fields = rec_get_n_fields_old(ibuf_rec) - 4;

	tuple = dtuple_create(heap, n_fields);

	types = rec_get_nth_field_old(ibuf_rec, 3, &len);

	ut_a(len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE <= 1);
	index = ibuf_dummy_index_create(n_fields,
				len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

	if (len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE) {
		/* compact record format */
		len--;
		ut_a(*types == 0);
		types++;
	}

	ut_a(len == n_fields * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

	for (i = 0; i < n_fields; i++) {
		field = dtuple_get_nth_field(tuple, i);

		data = rec_get_nth_field_old(ibuf_rec, i + 4, &len);

		dfield_set_data(field, data, len);

		dtype_new_read_for_order_and_null_size(
			dfield_get_type(field),
			types + i * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

		ibuf_dummy_index_add_col(index, dfield_get_type(field), len);
	}

	*pindex = index;
	return(tuple);
}

/************************************************************************
Returns the space taken by a stored non-clustered index entry if converted to
an index record. */
static
ulint
ibuf_rec_get_volume(
/*================*/
			/* out: size of index record in bytes + an upper
			limit of the space taken in the page directory */
	rec_t*	ibuf_rec)/* in: ibuf record */
{
	dtype_t	dtype;
	ibool	new_format	= FALSE;
	ulint	data_size	= 0;
	ulint	n_fields;
	byte*	types;
	byte*	data;
	ulint	len;
	ulint	i;

	ut_ad(ibuf_inside());
	ut_ad(rec_get_n_fields_old(ibuf_rec) > 2);

	data = rec_get_nth_field_old(ibuf_rec, 1, &len);

	if (len > 1) {
		/* < 4.1.x format record */

		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		n_fields = rec_get_n_fields_old(ibuf_rec) - 2;

		types = rec_get_nth_field_old(ibuf_rec, 1, &len);

		ut_ad(len == n_fields * DATA_ORDER_NULL_TYPE_BUF_SIZE);
	} else {
		/* >= 4.1.x format record */

		ut_a(trx_sys_multiple_tablespace_format);
		ut_a(*data == 0);

		types = rec_get_nth_field_old(ibuf_rec, 3, &len);

		ut_a(len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE <= 1);
		if (len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE) {
			/* compact record format */
			ulint		volume;
			dict_index_t*	dummy_index;
			mem_heap_t*	heap = mem_heap_create(500);
			dtuple_t*	entry =
				ibuf_build_entry_from_ibuf_rec(
					ibuf_rec, heap, &dummy_index);
			volume = rec_get_converted_size(dummy_index, entry);
			ibuf_dummy_index_free(dummy_index);
			mem_heap_free(heap);
			return(volume + page_dir_calc_reserved_space(1));
		}

		n_fields = rec_get_n_fields_old(ibuf_rec) - 4;

		new_format = TRUE;
	}

	for (i = 0; i < n_fields; i++) {
		if (new_format) {
			data = rec_get_nth_field_old(ibuf_rec, i + 4, &len);

			dtype_new_read_for_order_and_null_size(&dtype,
				types + i * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);
		} else {
			data = rec_get_nth_field_old(ibuf_rec, i + 2, &len);

			dtype_read_for_order_and_null_size(&dtype,
				types + i * DATA_ORDER_NULL_TYPE_BUF_SIZE);
		}

		if (len == UNIV_SQL_NULL) {
			data_size += dtype_get_sql_null_size(&dtype);
		} else {
			data_size += len;
		}
	}

	return(data_size + rec_get_converted_extra_size(data_size, n_fields)
			+ page_dir_calc_reserved_space(1));
}

/*************************************************************************
Builds the tuple to insert to an ibuf tree when we have an entry for a
non-clustered index. */
static
dtuple_t*
ibuf_entry_build(
/*=============*/
				/* out, own: entry to insert into an ibuf
				index tree; NOTE that the original entry
				must be kept because we copy pointers to its
				fields */
	dtuple_t*	entry,	/* in: entry for a non-clustered index */
	ibool		comp,	/* in: flag: TRUE=compact record format */
	ulint		space,	/* in: space id */
	ulint		page_no,/* in: index page number where entry should
				be inserted */
	mem_heap_t*	heap)	/* in: heap into which to build */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	dfield_t*	entry_field;
	ulint		n_fields;
	byte*		buf;
	byte*		buf2;
	ulint		i;

	/* Starting from 4.1.x, we have to build a tuple whose
	(1) first field is the space id,
	(2) the second field a single marker byte (0) to tell that this
	is a new format record,
	(3) the third contains the page number, and
	(4) the fourth contains the relevent type information of each data
	field; the length of this field % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE is
	 (a) 0 for b-trees in the old format, and
	 (b) 1 for b-trees in the compact format, the first byte of the field
	 being the marker (0);
	(5) and the rest of the fields are copied from entry. All fields
	in the tuple are ordered like the type binary in our insert buffer
	tree. */

	n_fields = dtuple_get_n_fields(entry);

	tuple = dtuple_create(heap, n_fields + 4);

	/* Store the space id in tuple */

	field = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, space);

	dfield_set_data(field, buf, 4);

	/* Store the marker byte field in tuple */

	field = dtuple_get_nth_field(tuple, 1);

	buf = mem_heap_alloc(heap, 1);

	/* We set the marker byte zero */

	mach_write_to_1(buf, 0);

	dfield_set_data(field, buf, 1);

	/* Store the page number in tuple */

	field = dtuple_get_nth_field(tuple, 2);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	ut_ad(comp == 0 || comp == 1);
	/* Store the type info in buf2, and add the fields from entry to
	tuple */
	buf2 = mem_heap_alloc(heap, n_fields
					* DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE
					+ comp);
	if (comp) {
		*buf2++ = 0; /* write the compact format indicator */
	}
	for (i = 0; i < n_fields; i++) {
		/* We add 4 below because we have the 4 extra fields at the
		start of an ibuf record */

		field = dtuple_get_nth_field(tuple, i + 4);
		entry_field = dtuple_get_nth_field(entry, i);
		dfield_copy(field, entry_field);

		dtype_new_store_for_order_and_null_size(
				buf2 + i * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE,
				dfield_get_type(entry_field));
	}

	/* Store the type info in buf2 to field 3 of tuple */

	field = dtuple_get_nth_field(tuple, 3);

	if (comp) {
		buf2--;
	}

	dfield_set_data(field, buf2, n_fields
					* DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE
					+ comp);
	/* Set all the types in the new tuple binary */

	dtuple_set_types_binary(tuple, n_fields + 4);

	return(tuple);
}

/*************************************************************************
Builds a search tuple used to search buffered inserts for an index page.
This is for < 4.1.x format records */
static
dtuple_t*
ibuf_search_tuple_build(
/*====================*/
				/* out, own: search tuple */
	ulint		space,	/* in: space id */
	ulint		page_no,/* in: index page number */
	mem_heap_t*	heap)	/* in: heap into which to build */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	byte*		buf;

	ut_a(space == 0);
	ut_a(trx_doublewrite_must_reset_space_ids);
	ut_a(!trx_sys_multiple_tablespace_format);

	tuple = dtuple_create(heap, 1);

	/* Store the page number in tuple */

	field = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	dtuple_set_types_binary(tuple, 1);

	return(tuple);
}

/*************************************************************************
Builds a search tuple used to search buffered inserts for an index page.
This is for >= 4.1.x format records. */
static
dtuple_t*
ibuf_new_search_tuple_build(
/*========================*/
				/* out, own: search tuple */
	ulint		space,	/* in: space id */
	ulint		page_no,/* in: index page number */
	mem_heap_t*	heap)	/* in: heap into which to build */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	byte*		buf;

	ut_a(trx_sys_multiple_tablespace_format);

	tuple = dtuple_create(heap, 3);

	/* Store the space id in tuple */

	field = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, space);

	dfield_set_data(field, buf, 4);

	/* Store the new format record marker byte */

	field = dtuple_get_nth_field(tuple, 1);

	buf = mem_heap_alloc(heap, 1);

	mach_write_to_1(buf, 0);

	dfield_set_data(field, buf, 1);

	/* Store the page number in tuple */

	field = dtuple_get_nth_field(tuple, 2);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	dtuple_set_types_binary(tuple, 3);

	return(tuple);
}

/*************************************************************************
Checks if there are enough pages in the free list of the ibuf tree that we
dare to start a pessimistic insert to the insert buffer. */
UNIV_INLINE
ibool
ibuf_data_enough_free_for_insert(
/*=============================*/
				/* out: TRUE if enough free pages in list */
	ibuf_data_t*	data)	/* in: ibuf data for the space */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&ibuf_mutex));
#endif /* UNIV_SYNC_DEBUG */

	/* We want a big margin of free pages, because a B-tree can sometimes
	grow in size also if records are deleted from it, as the node pointers
	can change, and we must make sure that we are able to delete the
	inserts buffered for pages that we read to the buffer pool, without
	any risk of running out of free space in the insert buffer. */

	if (data->free_list_len >= data->size / 2 + 3 * data->height) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Checks if there are enough pages in the free list of the ibuf tree that we
should remove them and free to the file space management. */
UNIV_INLINE
ibool
ibuf_data_too_much_free(
/*====================*/
				/* out: TRUE if enough free pages in list */
	ibuf_data_t*	data)	/* in: ibuf data for the space */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&ibuf_mutex));
#endif /* UNIV_SYNC_DEBUG */

	if (data->free_list_len >= 3 + data->size / 2 + 3 * data->height) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Allocates a new page from the ibuf file segment and adds it to the free
list. */
static
ulint
ibuf_add_free_page(
/*===============*/
					/* out: DB_SUCCESS, or DB_STRONG_FAIL
					if no space left */
	ulint		space,		/* in: space id */
	ibuf_data_t*	ibuf_data)	/* in: ibuf data for the space */
{
	mtr_t	mtr;
	page_t*	header_page;
	ulint	page_no;
	page_t*	page;
	page_t*	root;
	page_t*	bitmap_page;

	ut_a(space == 0);

	mtr_start(&mtr);

	/* Acquire the fsp latch before the ibuf header, obeying the latching
	order */
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header_page = ibuf_header_page_get(space, &mtr);

	/* Allocate a new page: NOTE that if the page has been a part of a
	non-clustered index which has subsequently been dropped, then the
	page may have buffered inserts in the insert buffer, and these
	should be deleted from there. These get deleted when the page
	allocation creates the page in buffer. Thus the call below may end
	up calling the insert buffer routines and, as we yet have no latches
	to insert buffer tree pages, these routines can run without a risk
	of a deadlock. This is the reason why we created a special ibuf
	header page apart from the ibuf tree. */

	page_no = fseg_alloc_free_page(header_page + IBUF_HEADER
					+ IBUF_TREE_SEG_HEADER, 0, FSP_UP,
									&mtr);
	if (page_no == FIL_NULL) {
		mtr_commit(&mtr);

		return(DB_STRONG_FAIL);
	}

	page = buf_page_get(space, page_no, RW_X_LATCH, &mtr);

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_TREE_NODE_NEW);
#endif /* UNIV_SYNC_DEBUG */

	ibuf_enter();

	mutex_enter(&ibuf_mutex);

	root = ibuf_tree_root_get(ibuf_data, space, &mtr);

	/* Add the page to the free list and update the ibuf size data */

	flst_add_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
		page + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE, &mtr);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_IBUF_FREE_LIST,
						MLOG_2BYTES, &mtr);

	ibuf_data->seg_size++;
	ibuf_data->free_list_len++;

	/* Set the bit indicating that this page is now an ibuf tree page
	(level 2 page) */

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, &mtr);

	ibuf_bitmap_page_set_bits(bitmap_page, page_no, IBUF_BITMAP_IBUF,
								TRUE, &mtr);
	mtr_commit(&mtr);

	mutex_exit(&ibuf_mutex);

	ibuf_exit();

	return(DB_SUCCESS);
}

/*************************************************************************
Removes a page from the free list and frees it to the fsp system. */
static
void
ibuf_remove_free_page(
/*==================*/
	ulint		space,		/* in: space id */
	ibuf_data_t*	ibuf_data)	/* in: ibuf data for the space */
{
	mtr_t	mtr;
	mtr_t	mtr2;
	page_t*	header_page;
	ulint	page_no;
	page_t*	page;
	page_t*	root;
	page_t*	bitmap_page;

	ut_a(space == 0);

	mtr_start(&mtr);

	/* Acquire the fsp latch before the ibuf header, obeying the latching
	order */
	mtr_x_lock(fil_space_get_latch(space), &mtr);

	header_page = ibuf_header_page_get(space, &mtr);

	/* Prevent pessimistic inserts to insert buffer trees for a while */
	mutex_enter(&ibuf_pessimistic_insert_mutex);

	ibuf_enter();

	mutex_enter(&ibuf_mutex);

	if (!ibuf_data_too_much_free(ibuf_data)) {

		mutex_exit(&ibuf_mutex);

		ibuf_exit();

		mutex_exit(&ibuf_pessimistic_insert_mutex);

		mtr_commit(&mtr);

		return;
	}

	mtr_start(&mtr2);

	root = ibuf_tree_root_get(ibuf_data, space, &mtr2);

	page_no = flst_get_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
									&mtr2)
		  .page;

	/* NOTE that we must release the latch on the ibuf tree root
	because in fseg_free_page we access level 1 pages, and the root
	is a level 2 page. */

	mtr_commit(&mtr2);
	mutex_exit(&ibuf_mutex);

	ibuf_exit();

	/* Since pessimistic inserts were prevented, we know that the
	page is still in the free list. NOTE that also deletes may take
	pages from the free list, but they take them from the start, and
	the free list was so long that they cannot have taken the last
	page from it. */

	fseg_free_page(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
							space, page_no, &mtr);
#ifdef UNIV_DEBUG_FILE_ACCESSES
	buf_page_reset_file_page_was_freed(space, page_no);
#endif
	ibuf_enter();

	mutex_enter(&ibuf_mutex);

	root = ibuf_tree_root_get(ibuf_data, space, &mtr);

	ut_ad(page_no == flst_get_last(root + PAGE_HEADER
					+ PAGE_BTR_IBUF_FREE_LIST, &mtr)
			 .page);

	page = buf_page_get(space, page_no, RW_X_LATCH, &mtr);

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_TREE_NODE);
#endif /* UNIV_SYNC_DEBUG */

	/* Remove the page from the free list and update the ibuf size data */

	flst_remove(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
		page + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE, &mtr);

	ibuf_data->seg_size--;
	ibuf_data->free_list_len--;

	mutex_exit(&ibuf_pessimistic_insert_mutex);

	/* Set the bit indicating that this page is no more an ibuf tree page
	(level 2 page) */

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, &mtr);

	ibuf_bitmap_page_set_bits(bitmap_page, page_no, IBUF_BITMAP_IBUF,
								FALSE, &mtr);
#ifdef UNIV_DEBUG_FILE_ACCESSES
	buf_page_set_file_page_was_freed(space, page_no);
#endif
	mtr_commit(&mtr);

	mutex_exit(&ibuf_mutex);

	ibuf_exit();
}

/***************************************************************************
Frees excess pages from the ibuf free list. This function is called when an OS
thread calls fsp services to allocate a new file segment, or a new page to a
file segment, and the thread did not own the fsp latch before this call. */

void
ibuf_free_excess_pages(
/*===================*/
	ulint	space)	/* in: space id */
{
	ibuf_data_t*	ibuf_data;
	ulint		i;

	if (space != 0) {
		fprintf(stderr,
"InnoDB: Error: calling ibuf_free_excess_pages for space %lu\n", (ulong) space);
		return;
	}

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(fil_space_get_latch(space), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(rw_lock_get_x_lock_count(fil_space_get_latch(space)) == 1);
	ut_ad(!ibuf_inside());

	/* NOTE: We require that the thread did not own the latch before,
	because then we know that we can obey the correct latching order
	for ibuf latches */

	ibuf_data = fil_space_get_ibuf_data(space);

	if (ibuf_data == NULL) {
		/* Not yet initialized */

#ifdef UNIV_DEBUG
		/*fprintf(stderr,
			"Ibuf for space %lu not yet initialized\n", space); */
#endif

		return;
	}

	/* Free at most a few pages at a time, so that we do not delay the
	requested service too much */

	for (i = 0; i < 4; i++) {

		mutex_enter(&ibuf_mutex);

		if (!ibuf_data_too_much_free(ibuf_data)) {

			mutex_exit(&ibuf_mutex);

			return;
		}

		mutex_exit(&ibuf_mutex);

		ibuf_remove_free_page(space, ibuf_data);
	}
}

/*************************************************************************
Reads page numbers from a leaf in an ibuf tree. */
static
ulint
ibuf_get_merge_page_nos(
/*====================*/
				/* out: a lower limit for the combined volume
				of records which will be merged */
	ibool		contract,/* in: TRUE if this function is called to
				contract the tree, FALSE if this is called
				when a single page becomes full and we look
				if it pays to read also nearby pages */
	rec_t*		rec,	/* in: record from which we read up and down
				in the chain of records */
	ulint*		space_ids,/* in/out: space id's of the pages */
	ib_longlong*	space_versions,/* in/out: tablespace version
				timestamps; used to prevent reading in old
				pages after DISCARD + IMPORT tablespace */
	ulint*		page_nos,/* in/out: buffer for at least
				IBUF_MAX_N_PAGES_MERGED many page numbers;
				the page numbers are in an ascending order */
	ulint*		n_stored)/* out: number of page numbers stored to
				page_nos in this function */
{
	ulint	prev_page_no;
	ulint	prev_space_id;
	ulint	first_page_no;
	ulint	first_space_id;
	ulint	rec_page_no;
	ulint	rec_space_id;
	ulint	sum_volumes;
	ulint	volume_for_page;
	ulint	rec_volume;
	ulint	limit;
	ulint	n_pages;

	*n_stored = 0;

	limit = ut_min(IBUF_MAX_N_PAGES_MERGED, buf_pool->curr_size / 4);

	if (page_rec_is_supremum(rec)) {

		rec = page_rec_get_prev(rec);
	}

	if (page_rec_is_infimum(rec)) {

		rec = page_rec_get_next(rec);
	}

	if (page_rec_is_supremum(rec)) {

		return(0);
	}

	first_page_no = ibuf_rec_get_page_no(rec);
	first_space_id = ibuf_rec_get_space(rec);
	n_pages = 0;
	prev_page_no = 0;
	prev_space_id = 0;

	/* Go backwards from the first rec until we reach the border of the
	'merge area', or the page start or the limit of storeable pages is
	reached */

	while (!page_rec_is_infimum(rec) && UNIV_LIKELY(n_pages < limit)) {

		rec_page_no = ibuf_rec_get_page_no(rec);
		rec_space_id = ibuf_rec_get_space(rec);

		if (rec_space_id != first_space_id
			|| rec_page_no / IBUF_MERGE_AREA
			!= first_page_no / IBUF_MERGE_AREA) {

			break;
		}

		if (rec_page_no != prev_page_no
			|| rec_space_id != prev_space_id) {
			n_pages++;
		}

		prev_page_no = rec_page_no;
		prev_space_id = rec_space_id;

		rec = page_rec_get_prev(rec);
	}

	rec = page_rec_get_next(rec);

	/* At the loop start there is no prev page; we mark this with a pair
	of space id, page no (0, 0) for which there can never be entries in
	the insert buffer */

	prev_page_no = 0;
	prev_space_id = 0;
	sum_volumes = 0;
	volume_for_page = 0;

	while (*n_stored < limit) {
		if (page_rec_is_supremum(rec)) {
			/* When no more records available, mark this with
			another 'impossible' pair of space id, page no */
			rec_page_no = 1;
			rec_space_id = 0;
		} else {
			rec_page_no = ibuf_rec_get_page_no(rec);
			rec_space_id = ibuf_rec_get_space(rec);
			ut_ad(rec_page_no > IBUF_TREE_ROOT_PAGE_NO);
		}

#ifdef UNIV_IBUF_DEBUG
		ut_a(*n_stored < IBUF_MAX_N_PAGES_MERGED);
#endif
		if ((rec_space_id != prev_space_id
				|| rec_page_no != prev_page_no)
			&& (prev_space_id != 0 || prev_page_no != 0)) {

			if ((prev_page_no == first_page_no
					&& prev_space_id == first_space_id)
				|| contract
				|| (volume_for_page >
					((IBUF_MERGE_THRESHOLD - 1)
						* 4 * UNIV_PAGE_SIZE
						/ IBUF_PAGE_SIZE_PER_FREE_SPACE)
					/ IBUF_MERGE_THRESHOLD)) {

				space_ids[*n_stored] = prev_space_id;
				space_versions[*n_stored]
						= fil_space_get_version(
							prev_space_id);
				page_nos[*n_stored] = prev_page_no;

				(*n_stored)++;

				sum_volumes += volume_for_page;
			}

			if (rec_space_id != first_space_id
				|| rec_page_no / IBUF_MERGE_AREA
				!= first_page_no / IBUF_MERGE_AREA) {

				break;
			}

			volume_for_page = 0;
		}

		if (rec_page_no == 1 && rec_space_id == 0) {
			/* Supremum record */

			break;
		}

		rec_volume = ibuf_rec_get_volume(rec);

		volume_for_page += rec_volume;

		prev_page_no = rec_page_no;
		prev_space_id = rec_space_id;

		rec = page_rec_get_next(rec);
	}

#ifdef UNIV_IBUF_DEBUG
	ut_a(*n_stored <= IBUF_MAX_N_PAGES_MERGED);
#endif
/*	fprintf(stderr, "Ibuf merge batch %lu pages %lu volume\n", *n_stored,
							sum_volumes); */
	return(sum_volumes);
}

/*************************************************************************
Contracts insert buffer trees by reading pages to the buffer pool. */
static
ulint
ibuf_contract_ext(
/*==============*/
			/* out: a lower limit for the combined size in bytes
			of entries which will be merged from ibuf trees to the
			pages read, 0 if ibuf is empty */
	ulint*	n_pages,/* out: number of pages to which merged */
	ibool	sync)	/* in: TRUE if the caller wants to wait for the
			issued read with the highest tablespace address
			to complete */
{
	ulint		rnd_pos;
	ibuf_data_t*	data;
	btr_pcur_t	pcur;
	ulint		space;
	ibool		all_trees_empty;
	ulint		page_nos[IBUF_MAX_N_PAGES_MERGED];
	ulint		space_ids[IBUF_MAX_N_PAGES_MERGED];
	ib_longlong	space_versions[IBUF_MAX_N_PAGES_MERGED];
	ulint		n_stored;
	ulint		sum_sizes;
	mtr_t		mtr;

	*n_pages = 0;
loop:
	ut_ad(!ibuf_inside());

	mutex_enter(&ibuf_mutex);

	ut_ad(ibuf_validate_low());

	/* Choose an ibuf tree at random (though there really is only one tree
	in the current implementation) */
	ibuf_rnd += 865558671;

	rnd_pos = ibuf_rnd % ibuf->size;

	all_trees_empty = TRUE;

	data = UT_LIST_GET_FIRST(ibuf->data_list);

	for (;;) {
		if (!data->empty) {
			all_trees_empty = FALSE;

			if (rnd_pos < data->size) {

				break;
			}

			rnd_pos -= data->size;
		}

		data = UT_LIST_GET_NEXT(data_list, data);

		if (data == NULL) {
			if (all_trees_empty) {
				mutex_exit(&ibuf_mutex);

				return(0);
			}

			data = UT_LIST_GET_FIRST(ibuf->data_list);
		}
	}

	ut_ad(data);

	space = data->index->space;

	ut_a(space == 0);	/* We currently only have an ibuf tree in
				space 0 */
	mtr_start(&mtr);

	ibuf_enter();

	/* Open a cursor to a randomly chosen leaf of the tree, at a random
	position within the leaf */

	btr_pcur_open_at_rnd_pos(data->index, BTR_SEARCH_LEAF, &pcur, &mtr);

	if (0 == page_get_n_recs(btr_pcur_get_page(&pcur))) {

		/* This tree is empty */

		data->empty = TRUE;

		ibuf_exit();

		mtr_commit(&mtr);
		btr_pcur_close(&pcur);

		mutex_exit(&ibuf_mutex);

		goto loop;
	}

	mutex_exit(&ibuf_mutex);

	sum_sizes = ibuf_get_merge_page_nos(TRUE, btr_pcur_get_rec(&pcur),
			space_ids, space_versions, page_nos, &n_stored);
#ifdef UNIV_IBUF_DEBUG
	/* fprintf(stderr, "Ibuf contract sync %lu pages %lu volume %lu\n",
		sync, n_stored, sum_sizes); */
#endif
	ibuf_exit();

	mtr_commit(&mtr);
	btr_pcur_close(&pcur);

	buf_read_ibuf_merge_pages(sync, space_ids, space_versions, page_nos,
								   n_stored);
	*n_pages = n_stored;

	return(sum_sizes + 1);
}

/*************************************************************************
Contracts insert buffer trees by reading pages to the buffer pool. */

ulint
ibuf_contract(
/*==========*/
			/* out: a lower limit for the combined size in bytes
			of entries which will be merged from ibuf trees to the
			pages read, 0 if ibuf is empty */
	ibool	sync)	/* in: TRUE if the caller wants to wait for the
			issued read with the highest tablespace address
			to complete */
{
	ulint	n_pages;

	return(ibuf_contract_ext(&n_pages, sync));
}

/*************************************************************************
Contracts insert buffer trees by reading pages to the buffer pool. */

ulint
ibuf_contract_for_n_pages(
/*======================*/
			/* out: a lower limit for the combined size in bytes
			of entries which will be merged from ibuf trees to the
			pages read, 0 if ibuf is empty */
	ibool	sync,	/* in: TRUE if the caller wants to wait for the
			issued read with the highest tablespace address
			to complete */
	ulint	n_pages)/* in: try to read at least this many pages to
			the buffer pool and merge the ibuf contents to
			them */
{
	ulint	sum_bytes	= 0;
	ulint	sum_pages	= 0;
	ulint	n_bytes;
	ulint	n_pag2;

	while (sum_pages < n_pages) {
		n_bytes = ibuf_contract_ext(&n_pag2, sync);

		if (n_bytes == 0) {
			return(sum_bytes);
		}

		sum_bytes += n_bytes;
		sum_pages += n_pag2;
	}

	return(sum_bytes);
}

/*************************************************************************
Contract insert buffer trees after insert if they are too big. */
UNIV_INLINE
void
ibuf_contract_after_insert(
/*=======================*/
	ulint	entry_size)	/* in: size of a record which was inserted
				into an ibuf tree */
{
	ibool	sync;
	ulint	sum_sizes;
	ulint	size;

	mutex_enter(&ibuf_mutex);

	if (ibuf->size < ibuf->max_size + IBUF_CONTRACT_ON_INSERT_NON_SYNC) {
		mutex_exit(&ibuf_mutex);

		return;
	}

	sync = FALSE;

	if (ibuf->size >= ibuf->max_size + IBUF_CONTRACT_ON_INSERT_SYNC) {

		sync = TRUE;
	}

	mutex_exit(&ibuf_mutex);

	/* Contract at least entry_size many bytes */
	sum_sizes = 0;
	size = 1;

	while ((size > 0) && (sum_sizes < entry_size)) {

		size = ibuf_contract(sync);
		sum_sizes += size;
	}
}

/*************************************************************************
Gets an upper limit for the combined size of entries buffered in the insert
buffer for a given page. */

ulint
ibuf_get_volume_buffered(
/*=====================*/
				/* out: upper limit for the volume of
				buffered inserts for the index page, in bytes;
				we may also return UNIV_PAGE_SIZE, if the
				entries for the index page span on several
				pages in the insert buffer */
	btr_pcur_t*	pcur,	/* in: pcur positioned at a place in an
				insert buffer tree where we would insert an
				entry for the index page whose number is
				page_no, latch mode has to be BTR_MODIFY_PREV
				or BTR_MODIFY_TREE */
	ulint		space,	/* in: space id */
	ulint		page_no,/* in: page number of an index page */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	volume;
	rec_t*	rec;
	page_t*	page;
	ulint	prev_page_no;
	page_t*	prev_page;
	ulint	next_page_no;
	page_t*	next_page;

	ut_a(trx_sys_multiple_tablespace_format);

	ut_ad((pcur->latch_mode == BTR_MODIFY_PREV)
				|| (pcur->latch_mode == BTR_MODIFY_TREE));

	/* Count the volume of records earlier in the alphabetical order than
	pcur */

	volume = 0;

	rec = btr_pcur_get_rec(pcur);

	page = buf_frame_align(rec);

	if (page_rec_is_supremum(rec)) {
		rec = page_rec_get_prev(rec);
	}

	for (;;) {
		if (page_rec_is_infimum(rec)) {

			break;
		}

		if (page_no != ibuf_rec_get_page_no(rec)
			|| space != ibuf_rec_get_space(rec)) {

			goto count_later;
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_prev(rec);
	}

	/* Look at the previous page */

	prev_page_no = btr_page_get_prev(page, mtr);

	if (prev_page_no == FIL_NULL) {

		goto count_later;
	}

	prev_page = buf_page_get(0, prev_page_no, RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_next(prev_page, mtr)
			== buf_frame_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(prev_page, SYNC_TREE_NODE);
#endif /* UNIV_SYNC_DEBUG */

	rec = page_get_supremum_rec(prev_page);
	rec = page_rec_get_prev(rec);

	for (;;) {
		if (page_rec_is_infimum(rec)) {

			/* We cannot go to yet a previous page, because we
			do not have the x-latch on it, and cannot acquire one
			because of the latching order: we have to give up */

			return(UNIV_PAGE_SIZE);
		}

		if (page_no != ibuf_rec_get_page_no(rec)
			|| space != ibuf_rec_get_space(rec)) {

			goto count_later;
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_prev(rec);
	}

count_later:
	rec = btr_pcur_get_rec(pcur);

	if (!page_rec_is_supremum(rec)) {
		rec = page_rec_get_next(rec);
	}

	for (;;) {
		if (page_rec_is_supremum(rec)) {

			break;
		}

		if (page_no != ibuf_rec_get_page_no(rec)
			|| space != ibuf_rec_get_space(rec)) {

			return(volume);
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_next(rec);
	}

	/* Look at the next page */

	next_page_no = btr_page_get_next(page, mtr);

	if (next_page_no == FIL_NULL) {

		return(volume);
	}

	next_page = buf_page_get(0, next_page_no, RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_prev(next_page, mtr)
			== buf_frame_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(next_page, SYNC_TREE_NODE);
#endif /* UNIV_SYNC_DEBUG */

	rec = page_get_infimum_rec(next_page);
	rec = page_rec_get_next(rec);

	for (;;) {
		if (page_rec_is_supremum(rec)) {

			/* We give up */

			return(UNIV_PAGE_SIZE);
		}

		if (page_no != ibuf_rec_get_page_no(rec)
			|| space != ibuf_rec_get_space(rec)) {

			return(volume);
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_next(rec);
	}
}

/*************************************************************************
Reads the biggest tablespace id from the high end of the insert buffer
tree and updates the counter in fil_system. */

void
ibuf_update_max_tablespace_id(void)
/*===============================*/
{
	ulint		max_space_id;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	ibuf_data_t*	ibuf_data;
	dict_index_t*	ibuf_index;
	btr_pcur_t	pcur;
	mtr_t		mtr;

	ibuf_data = fil_space_get_ibuf_data(0);

	ibuf_index = ibuf_data->index;
	ut_a(!dict_table_is_comp(ibuf_index->table));

	ibuf_enter();

	mtr_start(&mtr);

	btr_pcur_open_at_index_side(FALSE, ibuf_index, BTR_SEARCH_LEAF,
							&pcur, TRUE, &mtr);
	btr_pcur_move_to_prev(&pcur, &mtr);

	if (btr_pcur_is_before_first_on_page(&pcur, &mtr)) {
		/* The tree is empty */

		max_space_id = 0;
	} else {
		rec = btr_pcur_get_rec(&pcur);

		field = rec_get_nth_field_old(rec, 0, &len);

		ut_a(len == 4);

		max_space_id = mach_read_from_4(field);
	}

	mtr_commit(&mtr);
	ibuf_exit();

	/* printf("Maximum space id in insert buffer %lu\n", max_space_id); */

	fil_set_max_space_id_if_bigger(max_space_id);
}

/*************************************************************************
Makes an index insert to the insert buffer, instead of directly to the disk
page, if this is possible. */
static
ulint
ibuf_insert_low(
/*============*/
				/* out: DB_SUCCESS, DB_FAIL, DB_STRONG_FAIL */
	ulint		mode,	/* in: BTR_MODIFY_PREV or BTR_MODIFY_TREE */
	dtuple_t*	entry,	/* in: index entry to insert */
	dict_index_t*	index,	/* in: index where to insert; must not be
				unique or clustered */
	ulint		space,	/* in: space id where to insert */
	ulint		page_no,/* in: page number where to insert */
	que_thr_t*	thr)	/* in: query thread */
{
	big_rec_t*	dummy_big_rec;
	ulint		entry_size;
	btr_pcur_t	pcur;
	btr_cur_t*	cursor;
	dtuple_t*	ibuf_entry;
	mem_heap_t*	heap;
	ulint		buffered;
	rec_t*		ins_rec;
	ibool		old_bit_value;
	page_t*		bitmap_page;
	ibuf_data_t*	ibuf_data;
	dict_index_t*	ibuf_index;
	page_t*		root;
	ulint		err;
	ibool		do_merge;
	ulint		space_ids[IBUF_MAX_N_PAGES_MERGED];
	ib_longlong	space_versions[IBUF_MAX_N_PAGES_MERGED];
	ulint		page_nos[IBUF_MAX_N_PAGES_MERGED];
	ulint		n_stored;
	ulint		bits;
	mtr_t		mtr;
	mtr_t		bitmap_mtr;

	ut_a(!(index->type & DICT_CLUSTERED));
	ut_ad(dtuple_check_typed(entry));

	ut_a(trx_sys_multiple_tablespace_format);

	do_merge = FALSE;

	/* Currently the insert buffer of space 0 takes care of inserts to all
	tablespaces */

	ibuf_data = fil_space_get_ibuf_data(0);

	ibuf_index = ibuf_data->index;

	mutex_enter(&ibuf_mutex);

	if (ibuf->size >= ibuf->max_size + IBUF_CONTRACT_DO_NOT_INSERT) {
		/* Insert buffer is now too big, contract it but do not try
		to insert */

		mutex_exit(&ibuf_mutex);

#ifdef UNIV_IBUF_DEBUG
		fputs("Ibuf too big\n", stderr);
#endif
		/* Use synchronous contract (== TRUE) */
		ibuf_contract(TRUE);

		return(DB_STRONG_FAIL);
	}

	mutex_exit(&ibuf_mutex);

	if (mode == BTR_MODIFY_TREE) {
		mutex_enter(&ibuf_pessimistic_insert_mutex);

		ibuf_enter();

		mutex_enter(&ibuf_mutex);

		while (!ibuf_data_enough_free_for_insert(ibuf_data)) {

			mutex_exit(&ibuf_mutex);

			ibuf_exit();

			mutex_exit(&ibuf_pessimistic_insert_mutex);

			err = ibuf_add_free_page(0, ibuf_data);

			if (err == DB_STRONG_FAIL) {

				return(err);
			}

			mutex_enter(&ibuf_pessimistic_insert_mutex);

			ibuf_enter();

			mutex_enter(&ibuf_mutex);
		}
	} else {
		ibuf_enter();
	}

	entry_size = rec_get_converted_size(index, entry);

	heap = mem_heap_create(512);

	/* Build the entry which contains the space id and the page number as
	the first fields and the type information for other fields, and which
	will be inserted to the insert buffer. */

	ibuf_entry = ibuf_entry_build(entry, dict_table_is_comp(index->table),
		space, page_no, heap);

	/* Open a cursor to the insert buffer tree to calculate if we can add
	the new entry to it without exceeding the free space limit for the
	page. */

	mtr_start(&mtr);

	btr_pcur_open(ibuf_index, ibuf_entry, PAGE_CUR_LE, mode, &pcur, &mtr);

	/* Find out the volume of already buffered inserts for the same index
	page */
	buffered = ibuf_get_volume_buffered(&pcur, space, page_no, &mtr);

#ifdef UNIV_IBUF_DEBUG
	ut_a((buffered == 0) || ibuf_count_get(space, page_no));
#endif
	mtr_start(&bitmap_mtr);

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, &bitmap_mtr);

	/* We check if the index page is suitable for buffered entries */

	if (buf_page_peek(space, page_no)
			|| lock_rec_expl_exist_on_page(space, page_no)) {
		err = DB_STRONG_FAIL;

		mtr_commit(&bitmap_mtr);

		goto function_exit;
	}

	bits = ibuf_bitmap_page_get_bits(bitmap_page, page_no,
						IBUF_BITMAP_FREE, &bitmap_mtr);

	if (buffered + entry_size + page_dir_calc_reserved_space(1)
				> ibuf_index_page_calc_free_from_bits(bits)) {
		mtr_commit(&bitmap_mtr);

		/* It may not fit */
		err = DB_STRONG_FAIL;

		do_merge = TRUE;

		ibuf_get_merge_page_nos(FALSE, btr_pcur_get_rec(&pcur),
					space_ids, space_versions,
					page_nos, &n_stored);
		goto function_exit;
	}

	/* Set the bitmap bit denoting that the insert buffer contains
	buffered entries for this index page, if the bit is not set yet */

	old_bit_value = ibuf_bitmap_page_get_bits(bitmap_page, page_no,
					IBUF_BITMAP_BUFFERED, &bitmap_mtr);
	if (!old_bit_value) {
		ibuf_bitmap_page_set_bits(bitmap_page, page_no,
				IBUF_BITMAP_BUFFERED, TRUE, &bitmap_mtr);
	}

	mtr_commit(&bitmap_mtr);

	cursor = btr_pcur_get_btr_cur(&pcur);

	if (mode == BTR_MODIFY_PREV) {
		err = btr_cur_optimistic_insert(BTR_NO_LOCKING_FLAG, cursor,
						ibuf_entry, &ins_rec,
						&dummy_big_rec, thr,
						&mtr);
		if (err == DB_SUCCESS) {
			/* Update the page max trx id field */
			page_update_max_trx_id(buf_frame_align(ins_rec),
							thr_get_trx(thr)->id);
		}
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);

		/* We acquire an x-latch to the root page before the insert,
		because a pessimistic insert releases the tree x-latch,
		which would cause the x-latching of the root after that to
		break the latching order. */

		root = ibuf_tree_root_get(ibuf_data, 0, &mtr);

		err = btr_cur_pessimistic_insert(BTR_NO_LOCKING_FLAG
						 | BTR_NO_UNDO_LOG_FLAG,
						cursor,
						ibuf_entry, &ins_rec,
						&dummy_big_rec, thr,
						&mtr);
		if (err == DB_SUCCESS) {
			/* Update the page max trx id field */
			page_update_max_trx_id(buf_frame_align(ins_rec),
							thr_get_trx(thr)->id);
		}

		ibuf_data_sizes_update(ibuf_data, root, &mtr);
	}

function_exit:
#ifdef UNIV_IBUF_DEBUG
	if (err == DB_SUCCESS) {
		printf(
"Incrementing ibuf count of space %lu page %lu\n"
"from %lu by 1\n", space, page_no, ibuf_count_get(space, page_no));

		ibuf_count_set(space, page_no,
					ibuf_count_get(space, page_no) + 1);
	}
#endif
	if (mode == BTR_MODIFY_TREE) {
		ut_ad(ibuf_validate_low());

		mutex_exit(&ibuf_mutex);
		mutex_exit(&ibuf_pessimistic_insert_mutex);
	}

	mtr_commit(&mtr);
	btr_pcur_close(&pcur);
	ibuf_exit();

	mem_heap_free(heap);

	mutex_enter(&ibuf_mutex);

	if (err == DB_SUCCESS) {
		ibuf_data->empty = FALSE;
		ibuf_data->n_inserts++;
	}

	mutex_exit(&ibuf_mutex);

	if ((mode == BTR_MODIFY_TREE) && (err == DB_SUCCESS)) {
		ibuf_contract_after_insert(entry_size);
	}

	if (do_merge) {
#ifdef UNIV_IBUF_DEBUG
		ut_a(n_stored <= IBUF_MAX_N_PAGES_MERGED);
#endif
		buf_read_ibuf_merge_pages(FALSE, space_ids, space_versions,
							page_nos, n_stored);
	}

	return(err);
}

/*************************************************************************
Makes an index insert to the insert buffer, instead of directly to the disk
page, if this is possible. Does not do insert if the index is clustered
or unique. */

ibool
ibuf_insert(
/*========*/
				/* out: TRUE if success */
	dtuple_t*	entry,	/* in: index entry to insert */
	dict_index_t*	index,	/* in: index where to insert */
	ulint		space,	/* in: space id where to insert */
	ulint		page_no,/* in: page number where to insert */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_a(trx_sys_multiple_tablespace_format);
	ut_ad(dtuple_check_typed(entry));

	ut_a(!(index->type & DICT_CLUSTERED));

	if (rec_get_converted_size(index, entry)
		>= page_get_free_space_of_empty(
			dict_table_is_comp(index->table)) / 2) {
		return(FALSE);
	}

	err = ibuf_insert_low(BTR_MODIFY_PREV, entry, index, space, page_no,
									thr);
	if (err == DB_FAIL) {
		err = ibuf_insert_low(BTR_MODIFY_TREE, entry, index, space,
							page_no, thr);
	}

	if (err == DB_SUCCESS) {
#ifdef UNIV_IBUF_DEBUG
		/* fprintf(stderr, "Ibuf insert for page no %lu of index %s\n",
			page_no, index->name); */
#endif
		return(TRUE);

	} else {
		ut_a(err == DB_STRONG_FAIL);

		return(FALSE);
	}
}

/************************************************************************
During merge, inserts to an index page a secondary index entry extracted
from the insert buffer. */
static
void
ibuf_insert_to_index_page(
/*======================*/
	dtuple_t*	entry,	/* in: buffered entry to insert */
	page_t*		page,	/* in: index page where the buffered entry
				should be placed */
	dict_index_t*	index,	/* in: record descriptor */
	mtr_t*		mtr)	/* in: mtr */
{
	page_cur_t	page_cur;
	ulint		low_match;
	rec_t*		rec;
	page_t*		bitmap_page;
	ulint		old_bits;

	ut_ad(ibuf_inside());
	ut_ad(dtuple_check_typed(entry));

	if (UNIV_UNLIKELY(dict_table_is_comp(index->table)
			!= (ibool)!!page_is_comp(page))) {
		fputs(
"InnoDB: Trying to insert a record from the insert buffer to an index page\n"
"InnoDB: but the 'compact' flag does not match!\n", stderr);
		goto dump;
	}

	rec = page_rec_get_next(page_get_infimum_rec(page));

	if (UNIV_UNLIKELY(rec_get_n_fields(rec, index)
			!= dtuple_get_n_fields(entry))) {
		fputs(
"InnoDB: Trying to insert a record from the insert buffer to an index page\n"
"InnoDB: but the number of fields does not match!\n", stderr);
	dump:
		buf_page_print(page);

		dtuple_print(stderr, entry);

		fputs(
"InnoDB: The table where where this index record belongs\n"
"InnoDB: is now probably corrupt. Please run CHECK TABLE on\n"
"InnoDB: your tables.\n"
"InnoDB: Send a detailed bug report to mysql@lists.mysql.com!\n", stderr);

		return;
	}

	low_match = page_cur_search(page, index, entry,
						PAGE_CUR_LE, &page_cur);

	if (low_match == dtuple_get_n_fields(entry)) {
		rec = page_cur_get_rec(&page_cur);

		btr_cur_del_unmark_for_ibuf(rec, mtr);
	} else {
		rec = page_cur_tuple_insert(&page_cur, entry, index, mtr);

		if (rec == NULL) {
			/* If the record did not fit, reorganize */

			btr_page_reorganize(page, index, mtr);

			page_cur_search(page, index, entry,
						PAGE_CUR_LE, &page_cur);

			/* This time the record must fit */
			if (UNIV_UNLIKELY(!page_cur_tuple_insert(
					&page_cur, entry, index, mtr))) {

				ut_print_timestamp(stderr);

				fprintf(stderr,
"InnoDB: Error: Insert buffer insert fails; page free %lu, dtuple size %lu\n",
				(ulong) page_get_max_insert_size(page, 1),
				(ulong) rec_get_converted_size(index, entry));
				fputs("InnoDB: Cannot insert index record ",
					stderr);
				dtuple_print(stderr, entry);
				fputs(
"\nInnoDB: The table where where this index record belongs\n"
"InnoDB: is now probably corrupt. Please run CHECK TABLE on\n"
"InnoDB: that table.\n", stderr);

				bitmap_page = ibuf_bitmap_get_map_page(
						buf_frame_get_space_id(page),
						buf_frame_get_page_no(page),
						mtr);
				old_bits = ibuf_bitmap_page_get_bits(
						bitmap_page,
						buf_frame_get_page_no(page),
						IBUF_BITMAP_FREE, mtr);

				fprintf(stderr, "Bitmap bits %lu\n", (ulong) old_bits);

				fputs(
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com\n", stderr);
			}
		}
	}
}

/*************************************************************************
Deletes from ibuf the record on which pcur is positioned. If we have to
resort to a pessimistic delete, this function commits mtr and closes
the cursor. */
static
ibool
ibuf_delete_rec(
/*============*/
				/* out: TRUE if mtr was committed and pcur
				closed in this operation */
	ulint		space,	/* in: space id */
	ulint		page_no,/* in: index page number where the record
				should belong */
	btr_pcur_t*	pcur,	/* in: pcur positioned on the record to
				delete, having latch mode BTR_MODIFY_LEAF */
	dtuple_t*	search_tuple,
				/* in: search tuple for entries of page_no */
	mtr_t*		mtr)	/* in: mtr */
{
	ibool		success;
	ibuf_data_t*	ibuf_data;
	page_t*		root;
	ulint		err;

	ut_ad(ibuf_inside());

	success = btr_cur_optimistic_delete(btr_pcur_get_btr_cur(pcur), mtr);

	if (success) {
#ifdef UNIV_IBUF_DEBUG
		printf(
"Decrementing ibuf count of space %lu page %lu\n"
"from %lu by 1\n", space, page_no, ibuf_count_get(space, page_no));
		ibuf_count_set(space, page_no,
					ibuf_count_get(space, page_no) - 1);
#endif
		return(FALSE);
	}

	/* We have to resort to a pessimistic delete from ibuf */
	btr_pcur_store_position(pcur, mtr);

	btr_pcur_commit_specify_mtr(pcur, mtr);

	/* Currently the insert buffer of space 0 takes care of inserts to all
	tablespaces */

	ibuf_data = fil_space_get_ibuf_data(0);

	mutex_enter(&ibuf_mutex);

	mtr_start(mtr);

	success = btr_pcur_restore_position(BTR_MODIFY_TREE, pcur, mtr);

	if (!success) {
		fprintf(stderr,
		"InnoDB: ERROR: Submit the output to http://bugs.mysql.com\n"
		"InnoDB: ibuf cursor restoration fails!\n"
		"InnoDB: ibuf record inserted to page %lu\n", (ulong) page_no);
		fflush(stderr);

		rec_print_old(stderr, btr_pcur_get_rec(pcur));
		rec_print_old(stderr, pcur->old_rec);
		dtuple_print(stderr, search_tuple);

		rec_print_old(stderr,
				page_rec_get_next(btr_pcur_get_rec(pcur)));
		fflush(stderr);

		btr_pcur_commit_specify_mtr(pcur, mtr);

		fputs("InnoDB: Validating insert buffer tree:\n", stderr);
		if (!btr_validate_tree(ibuf_data->index->tree, NULL)) {
			ut_error;
		}

		fprintf(stderr, "InnoDB: ibuf tree ok\n");
		fflush(stderr);

		btr_pcur_close(pcur);

		mutex_exit(&ibuf_mutex);

		return(TRUE);
	}

	root = ibuf_tree_root_get(ibuf_data, 0, mtr);

	btr_cur_pessimistic_delete(&err, TRUE, btr_pcur_get_btr_cur(pcur),
								FALSE, mtr);
	ut_a(err == DB_SUCCESS);

#ifdef UNIV_IBUF_DEBUG
	ibuf_count_set(space, page_no, ibuf_count_get(space, page_no) - 1);
#else
	UT_NOT_USED(space);
#endif
	ibuf_data_sizes_update(ibuf_data, root, mtr);

	ut_ad(ibuf_validate_low());

	btr_pcur_commit_specify_mtr(pcur, mtr);

	btr_pcur_close(pcur);

	mutex_exit(&ibuf_mutex);

	return(TRUE);
}

/*************************************************************************
When an index page is read from a disk to the buffer pool, this function
inserts to the page the possible index entries buffered in the insert buffer.
The entries are deleted from the insert buffer. If the page is not read, but
created in the buffer pool, this function deletes its buffered entries from
the insert buffer; there can exist entries for such a page if the page
belonged to an index which subsequently was dropped. */

void
ibuf_merge_or_delete_for_page(
/*==========================*/
	page_t*	page,	/* in: if page has been read from disk, pointer to
			the page x-latched, else NULL */
	ulint	space,	/* in: space id of the index page */
	ulint	page_no,/* in: page number of the index page */
	ibool	update_ibuf_bitmap)/* in: normally this is set to TRUE, but if
			we have deleted or are deleting the tablespace, then we
			naturally do not want to update a non-existent bitmap
			page */
{
	mem_heap_t*	heap;
	btr_pcur_t	pcur;
	dtuple_t*	entry;
	dtuple_t*	search_tuple;
	rec_t*		ibuf_rec;
	buf_block_t*	block;
	page_t*		bitmap_page;
	ibuf_data_t*	ibuf_data;
	ulint		n_inserts;
#ifdef UNIV_IBUF_DEBUG
	ulint		volume;
#endif
	ibool		tablespace_being_deleted = FALSE;
	ibool		corruption_noticed	= FALSE;
	mtr_t		mtr;

	if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {

		return;
	}

#ifdef UNIV_LOG_DEBUG
	if (space % 2 != 0) {

		fputs("No ibuf operation in a replicate space\n", stderr);

		return;
	}
#endif
	if (ibuf_fixed_addr_page(space, page_no) || fsp_descr_page(page_no)
					|| trx_sys_hdr_page(space, page_no)) {
		return;
	}

	if (update_ibuf_bitmap) {
		/* If the following returns FALSE, we get the counter
		incremented, and must decrement it when we leave this
		function. When the counter is > 0, that prevents tablespace
		from being dropped. */

		tablespace_being_deleted = fil_inc_pending_ibuf_merges(space);

		if (tablespace_being_deleted) {
			/* Do not try to read the bitmap page from space;
			just delete the ibuf records for the page */

			page = NULL;
			update_ibuf_bitmap = FALSE;
		}
	}

	if (update_ibuf_bitmap) {
		mtr_start(&mtr);
		bitmap_page = ibuf_bitmap_get_map_page(space, page_no, &mtr);

		if (!ibuf_bitmap_page_get_bits(bitmap_page, page_no,
						IBUF_BITMAP_BUFFERED, &mtr)) {
			/* No inserts buffered for this page */
			mtr_commit(&mtr);

			if (!tablespace_being_deleted) {
				fil_decr_pending_ibuf_merges(space);
			}

			return;
		}
		mtr_commit(&mtr);
	}

	/* Currently the insert buffer of space 0 takes care of inserts to all
	tablespaces */

	ibuf_data = fil_space_get_ibuf_data(0);

	ibuf_enter();

	heap = mem_heap_create(512);

	if (!trx_sys_multiple_tablespace_format) {
		ut_a(trx_doublewrite_must_reset_space_ids);
		search_tuple = ibuf_search_tuple_build(space, page_no, heap);
	} else {
		search_tuple = ibuf_new_search_tuple_build(space, page_no,
									heap);
	}

	if (page) {
		/* Move the ownership of the x-latch on the page to this OS
		thread, so that we can acquire a second x-latch on it. This
		is needed for the insert operations to the index page to pass
		the debug checks. */

		block = buf_block_align(page);
		rw_lock_x_lock_move_ownership(&(block->lock));

		if (fil_page_get_type(page) != FIL_PAGE_INDEX) {

			corruption_noticed = TRUE;

			ut_print_timestamp(stderr);

			mtr_start(&mtr);

			fputs("  InnoDB: Dump of the ibuf bitmap page:\n",
				stderr);

			bitmap_page = ibuf_bitmap_get_map_page(space, page_no,
									&mtr);
			buf_page_print(bitmap_page);

			mtr_commit(&mtr);

			fputs("\nInnoDB: Dump of the page:\n", stderr);

			buf_page_print(page);

			fprintf(stderr,
"InnoDB: Error: corruption in the tablespace. Bitmap shows insert\n"
"InnoDB: buffer records to page n:o %lu though the page\n"
"InnoDB: type is %lu, which is not an index page!\n"
"InnoDB: We try to resolve the problem by skipping the insert buffer\n"
"InnoDB: merge for this page. Please run CHECK TABLE on your tables\n"
"InnoDB: to determine if they are corrupt after this.\n\n"
"InnoDB: Please submit a detailed bug report to http://bugs.mysql.com\n\n",
				(ulong) page_no,
				(ulong) fil_page_get_type(page));
		}
	}

	n_inserts = 0;
#ifdef UNIV_IBUF_DEBUG
	volume = 0;
#endif
loop:
	mtr_start(&mtr);

	if (page) {
		ibool success = buf_page_get_known_nowait(RW_X_LATCH, page,
					BUF_KEEP_OLD,
					__FILE__, __LINE__,
					&mtr);
		ut_a(success);
#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page, SYNC_TREE_NODE);
#endif /* UNIV_SYNC_DEBUG */
	}

	/* Position pcur in the insert buffer at the first entry for this
	index page */
	btr_pcur_open_on_user_rec(ibuf_data->index, search_tuple, PAGE_CUR_GE,
						BTR_MODIFY_LEAF, &pcur, &mtr);
	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {
		ut_ad(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

		goto reset_bit;
	}

	for (;;) {
		ut_ad(btr_pcur_is_on_user_rec(&pcur, &mtr));

		ibuf_rec = btr_pcur_get_rec(&pcur);

		/* Check if the entry is for this index page */
		if (ibuf_rec_get_page_no(ibuf_rec) != page_no
			|| ibuf_rec_get_space(ibuf_rec) != space) {
			if (page) {
				page_header_reset_last_insert(page, &mtr);
			}
			goto reset_bit;
		}

		if (corruption_noticed) {
			fputs("InnoDB: Discarding record\n ", stderr);
			rec_print_old(stderr, ibuf_rec);
			fputs("\n from the insert buffer!\n\n", stderr);
		} else if (page) {
			/* Now we have at pcur a record which should be
			inserted to the index page; NOTE that the call below
			copies pointers to fields in ibuf_rec, and we must
			keep the latch to the ibuf_rec page until the
			insertion is finished! */
			dict_index_t*	dummy_index;
			dulint		max_trx_id = page_get_max_trx_id(
						buf_frame_align(ibuf_rec));
			page_update_max_trx_id(page, max_trx_id);

			entry = ibuf_build_entry_from_ibuf_rec(ibuf_rec,
							heap, &dummy_index);
#ifdef UNIV_IBUF_DEBUG
			volume += rec_get_converted_size(dummy_index, entry)
					+ page_dir_calc_reserved_space(1);
			ut_a(volume <= 4 * UNIV_PAGE_SIZE
					/ IBUF_PAGE_SIZE_PER_FREE_SPACE);
#endif
			ibuf_insert_to_index_page(entry, page,
						dummy_index, &mtr);
			ibuf_dummy_index_free(dummy_index);
		}

		n_inserts++;

		/* Delete the record from ibuf */
		if (ibuf_delete_rec(space, page_no, &pcur, search_tuple,
								&mtr)) {
			/* Deletion was pessimistic and mtr was committed:
			we start from the beginning again */

			goto loop;
		}

		if (btr_pcur_is_after_last_on_page(&pcur, &mtr)) {
			mtr_commit(&mtr);
			btr_pcur_close(&pcur);

			goto loop;
		}
	}

reset_bit:
#ifdef UNIV_IBUF_DEBUG
	if (ibuf_count_get(space, page_no) > 0) {
		/* btr_print_tree(ibuf_data->index->tree, 100);
		ibuf_print(); */
	}
#endif
	if (update_ibuf_bitmap) {
		bitmap_page = ibuf_bitmap_get_map_page(space, page_no, &mtr);
		ibuf_bitmap_page_set_bits(bitmap_page, page_no,
					IBUF_BITMAP_BUFFERED, FALSE, &mtr);
	if (page) {
		ulint old_bits = ibuf_bitmap_page_get_bits(bitmap_page,
				page_no, IBUF_BITMAP_FREE, &mtr);
		ulint new_bits = ibuf_index_page_calc_free(page);
#ifdef UNIV_IBUF_DEBUG
		/* fprintf(stderr, "Old bits %lu new bits %lu max size %lu\n",
			old_bits, new_bits,
			page_get_max_insert_size_after_reorganize(page, 1)); */
#endif
			if (old_bits != new_bits) {
				ibuf_bitmap_page_set_bits(bitmap_page, page_no,
							IBUF_BITMAP_FREE,
							new_bits, &mtr);
			}
		}
	}
#ifdef UNIV_IBUF_DEBUG
	/* fprintf(stderr,
		"Ibuf merge %lu records volume %lu to page no %lu\n",
					n_inserts, volume, page_no); */
#endif
	mtr_commit(&mtr);
	btr_pcur_close(&pcur);
	mem_heap_free(heap);

	/* Protect our statistics keeping from race conditions */
	mutex_enter(&ibuf_mutex);

	ibuf_data->n_merges++;
	ibuf_data->n_merged_recs += n_inserts;

	mutex_exit(&ibuf_mutex);

	if (update_ibuf_bitmap && !tablespace_being_deleted) {

		fil_decr_pending_ibuf_merges(space);
	}

	ibuf_exit();
#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(space, page_no) == 0);
#endif
}

/*************************************************************************
Deletes all entries in the insert buffer for a given space id. This is used
in DISCARD TABLESPACE and IMPORT TABLESPACE.
NOTE: this does not update the page free bitmaps in the space. The space will
become CORRUPT when you call this function! */

void
ibuf_delete_for_discarded_space(
/*============================*/
	ulint	space)	/* in: space id */
{
	mem_heap_t*	heap;
	btr_pcur_t	pcur;
	dtuple_t*	search_tuple;
	rec_t*		ibuf_rec;
	ulint		page_no;
	ibool		closed;
	ibuf_data_t*	ibuf_data;
	ulint		n_inserts;
	mtr_t		mtr;

	/* Currently the insert buffer of space 0 takes care of inserts to all
	tablespaces */

	ibuf_data = fil_space_get_ibuf_data(0);

	heap = mem_heap_create(512);

	/* Use page number 0 to build the search tuple so that we get the
	cursor positioned at the first entry for this space id */

	search_tuple = ibuf_new_search_tuple_build(space, 0, heap);

	n_inserts = 0;
loop:
	ibuf_enter();

	mtr_start(&mtr);

	/* Position pcur in the insert buffer at the first entry for the
	space */
	btr_pcur_open_on_user_rec(ibuf_data->index, search_tuple, PAGE_CUR_GE,
						BTR_MODIFY_LEAF, &pcur, &mtr);
	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {
		ut_ad(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

		goto leave_loop;
	}

	for (;;) {
		ut_ad(btr_pcur_is_on_user_rec(&pcur, &mtr));

		ibuf_rec = btr_pcur_get_rec(&pcur);

		/* Check if the entry is for this space */
		if (ibuf_rec_get_space(ibuf_rec) != space) {

			goto leave_loop;
		}

		page_no = ibuf_rec_get_page_no(ibuf_rec);

		n_inserts++;

		/* Delete the record from ibuf */
		closed = ibuf_delete_rec(space, page_no, &pcur, search_tuple,
									&mtr);
		if (closed) {
			/* Deletion was pessimistic and mtr was committed:
			we start from the beginning again */

			ibuf_exit();

			goto loop;
		}

		if (btr_pcur_is_after_last_on_page(&pcur, &mtr)) {
			mtr_commit(&mtr);
			btr_pcur_close(&pcur);

			ibuf_exit();

			goto loop;
		}
	}

leave_loop:
	mtr_commit(&mtr);
	btr_pcur_close(&pcur);

	/* Protect our statistics keeping from race conditions */
	mutex_enter(&ibuf_mutex);

	ibuf_data->n_merges++;
	ibuf_data->n_merged_recs += n_inserts;

	mutex_exit(&ibuf_mutex);
	/*
	fprintf(stderr,
		"InnoDB: Discarded %lu ibuf entries for space %lu\n",
		(ulong) n_inserts, (ulong) space);
	*/
	ibuf_exit();

	mem_heap_free(heap);
}


/**********************************************************************
Validates the ibuf data structures when the caller owns ibuf_mutex. */

ibool
ibuf_validate_low(void)
/*===================*/
			/* out: TRUE if ok */
{
	ibuf_data_t*	data;
	ulint		sum_sizes;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&ibuf_mutex));
#endif /* UNIV_SYNC_DEBUG */

	sum_sizes = 0;

	data = UT_LIST_GET_FIRST(ibuf->data_list);

	while (data) {
		sum_sizes += data->size;

		data = UT_LIST_GET_NEXT(data_list, data);
	}

	ut_a(sum_sizes == ibuf->size);

	return(TRUE);
}

/**********************************************************************
Looks if the insert buffer is empty. */

ibool
ibuf_is_empty(void)
/*===============*/
			/* out: TRUE if empty */
{
	ibuf_data_t*	data;
	ibool		is_empty;
	page_t*		root;
	mtr_t		mtr;

	ibuf_enter();

	mutex_enter(&ibuf_mutex);

	data = UT_LIST_GET_FIRST(ibuf->data_list);

	mtr_start(&mtr);

	root = ibuf_tree_root_get(data, 0, &mtr);

	if (page_get_n_recs(root) == 0) {

		is_empty = TRUE;

		if (data->empty == FALSE) {
			fprintf(stderr,
"InnoDB: Warning: insert buffer tree is empty but the data struct does not\n"
"InnoDB: know it. This condition is legal if the master thread has not yet\n"
"InnoDB: run to completion.\n");
		}
	} else {
		ut_a(data->empty == FALSE);

		is_empty = FALSE;
	}

	mtr_commit(&mtr);

	ut_a(data->space == 0);

	mutex_exit(&ibuf_mutex);

	ibuf_exit();

	return(is_empty);
}

/**********************************************************************
Prints info of ibuf. */

void
ibuf_print(
/*=======*/
	FILE*	file)	/* in: file where to print */
{
	ibuf_data_t*	data;
#ifdef UNIV_IBUF_DEBUG
	ulint		i;
#endif

	mutex_enter(&ibuf_mutex);

	data = UT_LIST_GET_FIRST(ibuf->data_list);

	while (data) {
		fprintf(file,
	"Ibuf: size %lu, free list len %lu, seg size %lu,\n"
	"%lu inserts, %lu merged recs, %lu merges\n",
			(ulong) data->size,
			(ulong) data->free_list_len,
			(ulong) data->seg_size,
			(ulong) data->n_inserts,
			(ulong) data->n_merged_recs,
			(ulong) data->n_merges);
#ifdef UNIV_IBUF_DEBUG
		for (i = 0; i < IBUF_COUNT_N_PAGES; i++) {
			if (ibuf_count_get(data->space, i) > 0) {

				fprintf(stderr,
					"Ibuf count for page %lu is %lu\n",
					(ulong) i,
					(ulong) ibuf_count_get(data->space, i));
			}
		}
#endif
		data = UT_LIST_GET_NEXT(data_list, data);
	}

	mutex_exit(&ibuf_mutex);
}
