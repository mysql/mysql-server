/******************************************************
Index page routines

(c) 1994-1996 Innobase Oy

Created 2/2/1994 Heikki Tuuri
*******************************************************/

#ifndef page0types_h
#define page0types_h

#include "univ.i"
#include "dict0types.h"
#include "mtr0types.h"

/* Type of the index page */
/* The following define eliminates a name collision on HP-UX */
#define page_t	   ib_page_t
typedef	byte		page_t;
typedef struct page_search_struct	page_search_t;
typedef struct page_cur_struct	page_cur_t;

typedef byte				page_zip_t;
typedef struct page_zip_des_struct	page_zip_des_t;

/* The following definitions would better belong to page0zip.h,
but we cannot include page0zip.h from rem0rec.ic, because
page0*.h includes rem0rec.h and may include rem0rec.ic. */

#define PAGE_ZIP_SSIZE_BITS 3

#define PAGE_ZIP_MIN_SIZE_SHIFT	10	/* log2 of smallest compressed size */
#define PAGE_ZIP_MIN_SIZE	(1 << PAGE_ZIP_MIN_SIZE_SHIFT)

#define PAGE_ZIP_NUM_SSIZE (UNIV_PAGE_SIZE_SHIFT - PAGE_ZIP_MIN_SIZE_SHIFT + 2)
#if PAGE_ZIP_NUM_SSIZE > (1 << PAGE_ZIP_SSIZE_BITS)
# error "PAGE_ZIP_NUM_SSIZE > (1 << PAGE_ZIP_SSIZE_BITS)"
#endif

/* Compressed page descriptor */
struct page_zip_des_struct
{
	page_zip_t*	data;		/* compressed page data */

#ifdef UNIV_DEBUG
	unsigned	m_start:16;	/* start offset of modification log */
#endif /* UNIV_DEBUG */
	unsigned	m_end:16;	/* end offset of modification log */
	unsigned	m_nonempty:1;	/* TRUE if the modification log
					is not empty */
	unsigned	n_blobs:12;	/* number of externally stored
					columns on the page; the maximum
					is 744 on a 16 KiB page */
	unsigned	ssize:PAGE_ZIP_SSIZE_BITS;
					/* 0 or compressed page size;
					the size in bytes is
					PAGE_ZIP_MIN_SIZE << (ssize - 1). */
};

/** Compression statistics for a given page size */
struct page_zip_stat_struct {
	/** Number of page compressions */
	ulint		compressed;
	/** Number of successful page compressions */
	ulint		compressed_ok;
	/** Number of page decompressions */
	ulint		decompressed;
	/** Duration of page compressions in microseconds */
	ib_uint64_t	compressed_usec;
	/** Duration of page decompressions in microseconds */
	ib_uint64_t	decompressed_usec;
};

typedef struct page_zip_stat_struct page_zip_stat_t;

/** Statistics on compression, indexed by page_zip_des_t::ssize - 1 */
extern page_zip_stat_t page_zip_stat[PAGE_ZIP_NUM_SSIZE - 1];

/**************************************************************************
Write the "deleted" flag of a record on a compressed page.  The flag must
already have been written on the uncompressed page. */
UNIV_INTERN
void
page_zip_rec_set_deleted(
/*=====================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: record on the uncompressed page */
	ulint		flag)	/* in: the deleted flag (nonzero=TRUE) */
	__attribute__((nonnull));

/**************************************************************************
Write the "owned" flag of a record on a compressed page.  The n_owned field
must already have been written on the uncompressed page. */
UNIV_INTERN
void
page_zip_rec_set_owned(
/*===================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: record on the uncompressed page */
	ulint		flag)	/* in: the owned flag (nonzero=TRUE) */
	__attribute__((nonnull));

/**************************************************************************
Shift the dense page directory when a record is deleted. */
UNIV_INTERN
void
page_zip_dir_delete(
/*================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	byte*		rec,	/* in: deleted record */
	dict_index_t*	index,	/* in: index of rec */
	const ulint*	offsets,/* in: rec_get_offsets(rec) */
	const byte*	free)	/* in: previous start of the free list */
	__attribute__((nonnull(1,2,3,4)));

/**************************************************************************
Add a slot to the dense page directory. */
UNIV_INTERN
void
page_zip_dir_add_slot(
/*==================*/
	page_zip_des_t*	page_zip,	/* in/out: compressed page */
	ulint		is_clustered)	/* in: nonzero for clustered index,
					zero for others */
	__attribute__((nonnull));
#endif
