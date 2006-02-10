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
#define page_t     ib_page_t
typedef	byte		page_t;
typedef struct page_search_struct	page_search_t;
typedef struct page_cur_struct	page_cur_t;

typedef byte				page_zip_t;
typedef struct page_zip_des_struct	page_zip_des_t;

/* The following definitions would better belong to page0zip.h,
but we cannot include page0zip.h from rem0rec.ic, because
page0*.h includes rem0rec.h and may include rem0rec.ic. */

/* Compressed page descriptor */
struct page_zip_des_struct
{
	page_zip_t*	data;		/* compressed page data */
	ulint		size;		/* total size of compressed page */
	ulint		n_blobs;	/* number of externally stored
					columns */
	ulint		m_start;	/* start offset of modification log */
	ulint		m_end;		/* end offset of modification log */
};

/**************************************************************************
Write data to the compressed page.  The data must already be written to
the uncompressed page. */

void
page_zip_write(
/*===========*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: record whose data is being written */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	lint		offset,	/* in: start address of the block,
				relative to rec */
	ulint		length)	/* in: length of the data */
	__attribute__((nonnull));

/**************************************************************************
Clear a record on the uncompressed and compressed page, if possible. */

void
page_zip_clear_rec(
/*===============*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	byte*		rec,	/* in: record to clear */
	dict_index_t*	index,	/* in: index of rec */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	mtr_t*		mtr)	/* in: mini-transaction */
	__attribute__((nonnull));

/**************************************************************************
Write data to the uncompressed header portion of a page.  The data must
already have been written to the uncompressed page. */
UNIV_INLINE
void
page_zip_write_header(
/*==================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	str,	/* in: address on the uncompressed page */
	ulint		length)	/* in: length of the data */
	__attribute__((nonnull));

/**************************************************************************
Write the "deleted" flag of a record on a compressed page.  The flag must
already have been written on the uncompressed page. */

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

void
page_zip_rec_set_owned(
/*===================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: record on the uncompressed page */
	ulint		flag)	/* in: the owned flag (nonzero=TRUE) */
	__attribute__((nonnull));

/**************************************************************************
Shift the dense page directory when a record is deleted. */

void
page_zip_dir_delete(
/*================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: deleted record */
	const byte*	free)	/* in: previous start of the free list */
	__attribute__((nonnull));

#ifdef UNIV_DEBUG
/**************************************************************************
Determine if enough space is available in the modification log. */

ibool
page_zip_available_noninline(
/*=========================*/
					/* out: TRUE if enough space
					is available */
	const page_zip_des_t*	page_zip,/* in: compressed page */
	ulint			length,	/* in: sum of length in
					page_zip_write() calls */
	ulint			n_write,/* in: number of page_zip_write() */
	ulint			n_heap)	/* in: number of records that
					will be allocated from the heap */
	__attribute__((warn_unused_result, nonnull, pure));
#endif /* UNIV_DEBUG */

#endif 
