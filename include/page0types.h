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

#define PAGE_ZIP_MIN_SIZE	1024	/* smallest page_zip_des_struct.size */

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
	byte*		rec,	/* in: deleted record */
	dict_index_t*	index,	/* in: index of rec */
	const ulint*	offsets,/* in: rec_get_offsets(rec) */
	const byte*	free)	/* in: previous start of the free list */
	__attribute__((nonnull(1,2,3,4)));

/**************************************************************************
Add a slot to the dense page directory. */

void
page_zip_dir_add_slot(
/*==================*/
	page_zip_des_t*	page_zip,	/* in/out: compressed page */
	ulint		is_clustered)	/* in: nonzero for clustered index,
					zero for others */
	__attribute__((nonnull));
#endif
