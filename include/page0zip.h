/******************************************************
Compressed page interface

(c) 2005 Innobase Oy

Created June 2005 by Marko Makela
*******************************************************/

#ifndef page0zip_h
#define page0zip_h

#ifdef UNIV_MATERIALIZE
# undef UNIV_INLINE
# define UNIV_INLINE
#endif

#include "mtr0types.h"
#include "page0types.h"

/**************************************************************************
Initialize a compressed page descriptor. */
UNIV_INLINE
void
page_zip_des_init(
/*==============*/
	page_zip_des_t*	page_zip);	/* in/out: compressed page
					descriptor */

/**************************************************************************
Compress a page. */

ibool
page_zip_compress(
/*==============*/
				/* out: TRUE on success, FALSE on failure;
				page_zip will be left intact on failure. */
	page_zip_des_t*	page_zip,/* out: compressed page */
	const page_t*	page);	/* in: uncompressed page */

/**************************************************************************
Decompress a page. */

ibool
page_zip_decompress(
/*================*/
				/* out: TRUE on success, FALSE on failure */
	page_zip_des_t*	page_zip,/* in: data, size; out: m_start, m_end */
	page_t*		page,	/* out: uncompressed page, may be trashed */
	mtr_t*		mtr)	/* in: mini-transaction handle,
				or NULL if no logging is needed */
	__attribute__((warn_unused_result, nonnull(1, 2)));

#ifdef UNIV_DEBUG
/**************************************************************************
Validate a compressed page descriptor. */
UNIV_INLINE
ibool
page_zip_simple_validate(
/*=====================*/
						/* out: TRUE if ok */
	const page_zip_des_t*	page_zip);	/* in: compressed page
						descriptor */

/**************************************************************************
Check that the compressed and decompressed pages match. */

ibool
page_zip_validate(
/*==============*/
	const page_zip_des_t*	page_zip,/* in: compressed page */
	const page_t*		page);	/* in: uncompressed page */
#endif /* UNIV_DEBUG */

/**************************************************************************
Determine the encoded length of an integer in the modification log. */
UNIV_INLINE
ulint
page_zip_ulint_size(
/*================*/
			/* out: length of the integer, in bytes */
	ulint	num)	/* in: the integer */
		__attribute__((const));

/**************************************************************************
Determine the size of a modification log entry. */
UNIV_INLINE
ulint
page_zip_entry_size(
/*================*/
			/* out: length of the log entry, in bytes */
	ulint	pos,	/* in: offset of the uncompressed page */
	ulint	length)	/* in: length of the data */
		__attribute__((const));

/**************************************************************************
Ensure that enough space is available in the modification log.
If not, try to compress the page. */
UNIV_INLINE
ibool
page_zip_alloc(
/*===========*/
				/* out: TRUE if enough space is available */
	page_zip_des_t*	page_zip,/* in/out: compressed page;
				will only be modified if compression is needed
				and successful */
	const page_t*	page,	/* in: uncompressed page */
	ulint		size)	/* in: size of modification log entries */
	__attribute__((nonnull));

/**************************************************************************
Determine if enough space is available in the modification log. */
UNIV_INLINE
ibool
page_zip_available(
/*===============*/
					/* out: TRUE if enough space
					is available */
	const page_zip_des_t*	page_zip,/* in: compressed page */
	ulint			size)	/* in: requested size of
					modification log entries */
	__attribute__((warn_unused_result, nonnull, pure));

#ifdef UNIV_DEBUG
/**************************************************************************
Determine if enough space is available in the modification log. */

ibool
page_zip_available_noninline(
/*=========================*/
					/* out: TRUE if enough space
					is available */
	const page_zip_des_t*	page_zip,/* in: compressed page */
	ulint			size)
	__attribute__((warn_unused_result, nonnull, pure));
#endif /* UNIV_DEBUG */

/**************************************************************************
Write data to the compressed portion of a page.  The data must already
have been written to the uncompressed page. */

void
page_zip_write(
/*===========*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	str,	/* in: address on the uncompressed page */
	ulint		length)	/* in: length of the data */
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
Write data to the uncompressed trailer portion of a page.  The data must
already have been written to the uncompressed page. */
UNIV_INLINE
void
page_zip_write_trailer(
/*===================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	str,	/* in: address on the uncompressed page */
	ulint		length)	/* in: length of the data */
	__attribute__((nonnull));

#ifdef UNIV_MATERIALIZE
# undef UNIV_INLINE
# define UNIV_INLINE	UNIV_INLINE_ORIGINAL
#endif

#ifndef UNIV_NONINL
# include "page0zip.ic"
#endif

#endif /* page0zip_h */
