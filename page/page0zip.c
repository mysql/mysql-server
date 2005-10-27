/******************************************************
Compressed page interface

(c) 2005 Innobase Oy

Created June 2005 by Marko Makela
*******************************************************/

#define THIS_MODULE
#include "page0zip.h"
#ifdef UNIV_NONINL
# include "page0zip.ic"
#endif
#undef THIS_MODULE
#include "page0page.h"
#include "mtr0log.h"
#include "zlib.h"

/**************************************************************************
Compress a page. */

ibool
page_zip_compress(
/*==============*/
				/* out: TRUE on success, FALSE on failure;
				page_zip will be left intact on failure. */
	page_zip_des_t*	page_zip,/* out: compressed page */
	const page_t*	page)	/* in: uncompressed page */
{
	z_stream	c_stream;
	int		err;
	byte*		buf;
	ulint		trailer_len;

	ut_ad(page_zip_simple_validate(page_zip));
#ifdef UNIV_DEBUG
	if (page_is_comp((page_t*) page)) {
		ut_ad(page_simple_validate_new((page_t*) page));
	} else {
		ut_ad(page_simple_validate_old((page_t*) page));
	}
#endif /* UNIV_DEBUG */

	buf = mem_alloc(page_zip->size - PAGE_DATA);

	/* Determine the length of the page trailer. */
	trailer_len = page + UNIV_PAGE_SIZE
			- page_dir_get_nth_slot((page_t*) page,
				page_dir_get_n_slots((page_t*) page) - 1);
	ut_ad(trailer_len < UNIV_PAGE_SIZE - PAGE_DATA);

	/* Compress the data payload. */
	c_stream.zalloc = (alloc_func) 0;
	c_stream.zfree = (free_func) 0;
	c_stream.opaque = (voidpf) 0;

	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	ut_a(err == Z_OK);

	c_stream.next_out = buf;
	c_stream.next_in = (void*) (page + PAGE_DATA);
	c_stream.avail_out = page_zip->size - (PAGE_DATA - 1) - trailer_len;
	c_stream.avail_in = page_header_get_field((page_t*) page,
				PAGE_HEAP_TOP) - PAGE_DATA;

	err = deflate(&c_stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&c_stream);
		mem_free(buf);
		return(FALSE);
	}

	err = deflateEnd(&c_stream);
	ut_a(err == Z_OK);

	ut_ad(c_stream.avail_in == page_header_get_field((page_t*) page,
				PAGE_HEAP_TOP) - PAGE_DATA);
	ut_ad(c_stream.avail_out == page_zip->size - (PAGE_DATA - 1)
				- trailer_len);
	ut_a(c_stream.total_in == (uLong) c_stream.avail_in);
	ut_a(c_stream.total_out <= (uLong) c_stream.avail_out);

	page_zip->m_end = page_zip->m_start = PAGE_DATA + c_stream.total_out;
	/* Copy the page header */
	memcpy(page_zip->data, page, PAGE_DATA);
	/* Copy the compressed data */
	memcpy(page_zip->data + PAGE_DATA, buf, c_stream.total_out);
	/* Zero out the area reserved for the modification log */
	memset(page_zip->data + PAGE_DATA + c_stream.total_out, 0,
		page_zip->size - PAGE_DATA - trailer_len - c_stream.total_out);
	/* Copy the page trailer */
	memcpy(page_zip->data + page_zip->size - trailer_len,
			page + UNIV_PAGE_SIZE - trailer_len, trailer_len);
	mem_free(buf);
	ut_ad(page_zip_validate(page_zip, page));
	return(TRUE);
}

/**************************************************************************
Read an integer from the modification log of the compressed page. */
static
ulint
page_zip_ulint_read(
/*================*/
				/* out: length of the integer, in bytes;
				zero on failure */
	const byte*	src,	/* in: where to read */
	ulint*		dest)	/* out: the decoded integer */
{
	ulint	num	= (unsigned char) *src;
	if (num < 128) {
		*dest = num;	/* 0xxxxxxx: 0..127 */
		return(1);
	}
	if (num < 192) {	/* 10xxxxxx xxxxxxxx: 0..16383 */
		*dest = ((num << 8) & ~0x8000) | (unsigned char) src[1];
		return(2);
	}
	*dest = ULINT_MAX;
	return(0);		/* 11xxxxxxx xxxxxxxx: reserved */
}

/**************************************************************************
Write an integer to the modification log of the compressed page. */
static
ulint
page_zip_ulint_write(
/*=================*/
			/* out: length of the integer, in bytes;
			zero on failure */
	byte*	dest,	/* in: where to write */
	ulint	num)	/* out: integer to write */
{
	if (num < 128) {
		*dest = num;	/* 0xxxxxxx: 0..127 */
		return(1);
	}
	if (num < 16384) {	/* 10xxxxxx xxxxxxxx: 0..16383 */
		dest[0] = num >> 8 | 0x80;
		dest[1] = num;
		return(2);
	}
	ut_error;
	return(0);		/* 11xxxxxxx xxxxxxxx: reserved */
}

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
{
	z_stream	d_stream;
	int		err;
	ulint		trailer_len;

	ut_ad(page_zip_simple_validate(page_zip));
	trailer_len = PAGE_DIR
			+ PAGE_DIR_SLOT_SIZE
			* page_dir_get_n_slots((page_t*) page_zip->data);
	ut_ad(trailer_len < page_zip->size - PAGE_DATA);
	ut_ad(page_header_get_field((page_t*) page_zip->data, PAGE_HEAP_TOP)
				<= UNIV_PAGE_SIZE - trailer_len);

	d_stream.zalloc = (alloc_func) 0;
	d_stream.zfree = (free_func) 0;
	d_stream.opaque = (voidpf) 0;

	err = inflateInit(&d_stream);
	ut_a(err == Z_OK);

	d_stream.next_in = page_zip->data + PAGE_DATA;
	d_stream.next_out = page + PAGE_DATA;
	d_stream.avail_in = page_zip->size - trailer_len - (PAGE_DATA - 1);
	d_stream.avail_out = page_header_get_field(page_zip->data, PAGE_HEAP_TOP)
			- PAGE_DATA;

	err = inflate(&d_stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		inflateEnd(&d_stream);
		return(FALSE);
	}
	err = inflateEnd(&d_stream);
	ut_a(err == Z_OK);

	ut_ad(d_stream.avail_in
	== page_zip->size - trailer_len - (PAGE_DATA - 1));
	ut_ad(d_stream.avail_out
	== page_header_get_field(page_zip->data, PAGE_HEAP_TOP) - PAGE_DATA);
	ut_a(d_stream.total_in <= (uLong) d_stream.avail_in);
	ut_a(d_stream.total_out == d_stream.total_out);

	page_zip->m_end = page_zip->m_start = PAGE_DATA + d_stream.total_in;
	/* Copy the page header */
	memcpy(page, page_zip->data, PAGE_DATA);
	/* Copy the page trailer */
	memcpy(page_zip->data + page_zip->size - trailer_len,
			page + UNIV_PAGE_SIZE - trailer_len, trailer_len);
	/* Apply the modification log. */
	while (page_zip->data[page_zip->m_end]) {
		ulint	ulint_len;
		ulint	length, offset;
		ulint_len = page_zip_ulint_read(page_zip->data + page_zip->m_end,
								&length);
		page_zip->m_end += ulint_len;
		if (!ulint_len
		|| page_zip->m_end + length >= page_zip->size - trailer_len) {
			return(FALSE);
		}
		ut_a(length > 0 && length < UNIV_PAGE_SIZE - PAGE_DATA);

		ulint_len = page_zip_ulint_read(page_zip->data + page_zip->m_end,
								&offset);
		page_zip->m_end += ulint_len;
		if (!ulint_len
		|| page_zip->m_end + length >= page_zip->size - trailer_len) {
			return(FALSE);
		}

		offset += PAGE_DATA;
		ut_a(offset + length < UNIV_PAGE_SIZE - trailer_len);

		memcpy(page + offset, page_zip->data + page_zip->m_end, length);
		page_zip->m_end += length;
	}

	ut_a(page_is_comp(page));
	ut_ad(page_simple_validate_new(page));

	if (UNIV_LIKELY_NULL(mtr)) {
		byte*	log_ptr	= mlog_open(mtr, 11);
		if (log_ptr) {
			log_ptr = mlog_write_initial_log_record_fast(
					page, MLOG_COMP_DECOMPRESS,
					log_ptr, mtr);
			mlog_close(mtr, log_ptr);
		}
	}

	return(TRUE);
}

#ifdef UNIV_DEBUG
/**************************************************************************
Check that the compressed and decompressed pages match. */

ibool
page_zip_validate(
/*==============*/
	const page_zip_des_t*	page_zip,	/* in: compressed page */
	const page_t*		page)		/* in: uncompressed page */
{
	page_zip_des_t	temp_page_zip = *page_zip;
	page_t		temp_page[UNIV_PAGE_SIZE];

	ut_ad(buf_block_get_page_zip(buf_block_align((byte*)page))
		== page_zip);

	return(page_zip_decompress(&temp_page_zip, temp_page, NULL)
				&& !memcmp(page, temp_page, UNIV_PAGE_SIZE));
}
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
{
	ulint	pos = ut_align_offset(str, UNIV_PAGE_SIZE);
#ifdef UNIV_DEBUG
	ulint	trailer_len = PAGE_DIR
			+ PAGE_DIR_SLOT_SIZE
			* page_dir_get_n_slots((page_t*) page_zip->data);
#endif /* UNIV_DEBUG */

	ut_ad(buf_block_get_page_zip(buf_block_align((byte*)str)) == page_zip);
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(page_dir_get_n_slots(ut_align_down((byte*) str, UNIV_PAGE_SIZE))
		== page_dir_get_n_slots((page_t*) page_zip->data));
	ut_ad(!page_zip->data[page_zip->m_end]);

	ut_ad(PAGE_DATA + trailer_len < page_zip->size);

	ut_ad(pos >= PAGE_DATA);
	ut_ad(pos + length <= UNIV_PAGE_SIZE - trailer_len);

	pos -= PAGE_DATA;

	ut_ad(page_zip_available(page_zip, page_zip_entry_size(pos, length)));

	/* Append to the modification log. */
	page_zip->m_end += page_zip_ulint_write(
				page_zip->data + page_zip->m_end, length);
	page_zip->m_end += page_zip_ulint_write(
				page_zip->data + page_zip->m_end, pos);
	memcpy(&page_zip->data[page_zip->m_end], str, length);
	page_zip->m_end += length;
	ut_ad(!page_zip->data[page_zip->m_end]);
	ut_ad(page_zip->m_end < page_zip->size - trailer_len);
	ut_ad(page_zip_validate(page_zip,
				ut_align_down((byte*) str, UNIV_PAGE_SIZE)));
}

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
{
	return(page_zip_available(page_zip, size));
}
#endif /* UNIV_DEBUG */
