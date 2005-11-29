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
#include "ut0sort.h"
#include "zlib.h"

/* Please refer to ../include/page0zip.ic for a description of the
compressed page format. */

/* The infimum and supremum records are omitted from the compressed page.
On compress, we compare that the records are there, and on uncompress we
restore the records. */
static const byte infimum_extra[] = {
	0x01,			/* info_bits=0, n_owned=1 */
	0x00, 0x02		/* heap_no=0, status=2 */
	/* ?, ?	*/		/* next=(first user rec, or supremum) */
};
static const byte infimum_data[] = {
	0x69, 0x6e, 0x66, 0x69,
	0x6d, 0x75, 0x6d, 0x00	/* "infimum\0" */
};
static const byte supremum_extra_data[] = {
	/* 0x0?, */		/* info_bits=0, n_owned=1..8 */
	0x00, 0x0b,		/* heap_no=1, status=3 */
	0x00, 0x00,		/* next=0 */
	0x73, 0x75, 0x70, 0x72,
	0x65, 0x6d, 0x75, 0x6d	/* "supremum" */
};

/**************************************************************************
Populate the dense page directory from the sparse directory. */
static
void
page_zip_dir_encode(
/*================*/
	const page_t*	page,	/* in: compact page */
	page_zip_des_t*	page_zip,/* out: dense directory on compressed page */
	const rec_t**	recs)	/* in: array of 0, out: dense page directory
				sorted by ascending address (and heap_no) */
{
	byte*	rec;
	ulint	status;
	ulint	min_mark;
	ulint	heap_no;
	ulint	i;
	ulint	n_heap;
	ulint	offs;

	min_mark = 0;

	if (mach_read_from_2((page_t*) page + (PAGE_HEADER + PAGE_LEVEL))) {
		status = REC_STATUS_NODE_PTR;
		if (UNIV_UNLIKELY(mach_read_from_4((page_t*) page
					+ FIL_PAGE_PREV) == FIL_NULL)) {
			min_mark = REC_INFO_MIN_REC_FLAG;
		}
	} else {
		status = REC_STATUS_ORDINARY;
	}

	n_heap = page_dir_get_n_heap((page_t*) page);

	/* Traverse the list of stored records in the collation order,
	starting from the first user record. */

	rec = (page_t*) page + PAGE_NEW_INFIMUM, TRUE;

	i = 0;

	for (;;) {
		ulint	info_bits;
		offs = rec_get_next_offs(rec, TRUE);
		if (UNIV_UNLIKELY(offs == PAGE_NEW_SUPREMUM)) {
			break;
		}
		rec = (page_t*) page + offs;
		heap_no = rec_get_heap_no_new(rec);
		ut_a(heap_no > 0);
		ut_a(heap_no < n_heap);
		ut_a(!(offs & ~PAGE_ZIP_DIR_SLOT_MASK));
		ut_a(offs);

		if (UNIV_UNLIKELY(rec_get_n_owned_new(rec))) {
			offs |= PAGE_ZIP_DIR_SLOT_OWNED;
		}

		info_bits = rec_get_info_bits(rec, TRUE);
		if (UNIV_UNLIKELY(info_bits & REC_INFO_DELETED_FLAG)) {
			info_bits &= ~REC_INFO_DELETED_FLAG;
			offs |= PAGE_ZIP_DIR_SLOT_DEL;
		}
		ut_a(info_bits == min_mark);
		/* Only the smallest user record can have
		REC_INFO_MIN_REC_FLAG set. */
		min_mark = 0;

		page_zip_dir_set(page_zip, i++, offs);

		/* Ensure that each heap_no occurs at most once. */
		ut_a(!recs[heap_no - 2]); /* exclude infimum and supremum */
		recs[heap_no - 2] = rec;

		ut_a(rec_get_status(rec) == status);
	}

	offs = page_header_get_field((page_t*) page, PAGE_FREE);

	/* Traverse the free list (of deleted records). */
	while (offs) {
		ut_ad(!(offs & ~PAGE_ZIP_DIR_SLOT_MASK));
		rec = (page_t*) page + offs;

		heap_no = rec_get_heap_no_new(rec);
		ut_a(heap_no >= 2); /* only user records can be deleted */
		ut_a(heap_no < n_heap);

		ut_a(!rec[-REC_N_NEW_EXTRA_BYTES]); /* info_bits and n_owned */
		ut_a(rec_get_status(rec) == status);

		page_zip_dir_set(page_zip, i++, offs);

		/* Ensure that each heap_no occurs at most once. */
		ut_a(!recs[heap_no - 2]); /* exclude infimum and supremum */
		recs[heap_no - 2] = rec;

		offs = rec_get_next_offs(rec, TRUE);
	}

	/* Ensure that each heap no occurs at least once. */
	ut_a(i + 2/* infimum and supremum */ == n_heap);
}

/**************************************************************************
Compress a page. */

ibool
page_zip_compress(
/*==============*/
				/* out: TRUE on success, FALSE on failure;
				page_zip will be left intact on failure. */
	page_zip_des_t*	page_zip,/* in: size; out: compressed page */
	const page_t*	page)	/* in: uncompressed page */
{
	z_stream	c_stream;
	int		err;
	byte*		buf;
	ulint		n_dense;
	const byte*	src;
	const byte**	recs;	/* dense page directory, sorted by address */
	mem_heap_t*	heap;

	ut_a(page_is_comp((page_t*) page));
	ut_ad(page_simple_validate_new((page_t*) page));
	ut_ad(page_zip_simple_validate(page_zip));

	/* Check the data that will be omitted. */
	ut_a(!memcmp(page + (PAGE_NEW_INFIMUM - REC_N_NEW_EXTRA_BYTES),
		     infimum_extra, sizeof infimum_extra));
	ut_a(!memcmp(page + PAGE_NEW_INFIMUM,
		     infimum_data, sizeof infimum_data));
	ut_a(page[PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES]
				/* info_bits == 0, n_owned <= max */
				<= PAGE_DIR_SLOT_MAX_N_OWNED);
	ut_a(!memcmp(page + (PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES + 1),
		     supremum_extra_data, sizeof supremum_extra_data));
	
	if (UNIV_UNLIKELY(!page_get_n_recs((page_t*) page))) {
		ut_a(rec_get_next_offs((page_t*) page + PAGE_NEW_INFIMUM, TRUE)
			== PAGE_NEW_SUPREMUM);
	}

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap((page_t*) page) - 2;
	ut_a(n_dense * PAGE_ZIP_DIR_SLOT_SIZE < page_zip->size);

	heap = mem_heap_create(page_zip->size
		+ n_dense * ((sizeof *recs) - PAGE_ZIP_DIR_SLOT_SIZE));

	recs = mem_heap_alloc(heap, n_dense * sizeof *recs);
	memset(recs, 0, n_dense * sizeof *recs);

	buf = mem_heap_alloc(heap, page_zip->size
			- PAGE_DATA - PAGE_ZIP_DIR_SLOT_SIZE * n_dense);

	page_zip_dir_encode(page, page_zip, recs);

	/* Compress the data payload. */
	c_stream.zalloc = (alloc_func) 0;
	c_stream.zfree = (free_func) 0;
	c_stream.opaque = (voidpf) 0;

	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	ut_a(err == Z_OK);

	c_stream.next_out = buf;
	c_stream.avail_out = page_zip->size - (PAGE_DATA + 1)
				- n_dense * PAGE_ZIP_DIR_SLOT_SIZE;

	if (UNIV_LIKELY(n_dense > 0)
	    && *recs == page + (PAGE_ZIP_START + REC_N_NEW_EXTRA_BYTES)) {
		src = page + (PAGE_ZIP_START + REC_N_NEW_EXTRA_BYTES);
		recs++;
		n_dense--;
	} else {
		src = page + PAGE_ZIP_START;
	}

	while (n_dense--) {
		c_stream.next_in = (void*) src;
		c_stream.avail_in = *recs - src - REC_N_NEW_EXTRA_BYTES;

		err = deflate(&c_stream, Z_NO_FLUSH);
		if (err != Z_OK) {
			goto zlib_error;
		}

		src = *recs++;
	}

	/* Compress the last record. */
	c_stream.next_in = (void*) src;
	c_stream.avail_in =
			page_header_get_field((page_t*) page, PAGE_HEAP_TOP)
			- ut_align_offset(src, UNIV_PAGE_SIZE);
	ut_a(c_stream.avail_in < UNIV_PAGE_SIZE
				- PAGE_ZIP_START - PAGE_DIR);

	err = deflate(&c_stream, Z_FINISH);

	if (err != Z_STREAM_END) {
zlib_error:
		deflateEnd(&c_stream);
		mem_heap_free(heap);
		return(FALSE);
	}

	err = deflateEnd(&c_stream);
	ut_a(err == Z_OK);

	page_zip->m_end = page_zip->m_start = PAGE_DATA + c_stream.total_out;
	/* Copy the page header */
	memcpy(page_zip->data, page, PAGE_DATA);
	/* Copy the compressed data */
	memcpy(page_zip->data + PAGE_DATA, buf, c_stream.total_out);
	/* Zero out the area reserved for the modification log */
	memset(page_zip->data + PAGE_DATA + c_stream.total_out, 0,
		c_stream.avail_out + 1);
	mem_heap_free(heap);
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
Compare two page directory entries. */
UNIV_INLINE
ibool
page_zip_dir_cmp(
/*=============*/
				/* out: positive if rec1 > rec2 */
	const rec_t*	rec1,	/* in: rec1 */
	const rec_t*	rec2)	/* in: rec2 */
{
	return(rec1 > rec2);
}

/**************************************************************************
Sort the dense page directory by address (heap_no). */
static
void
page_zip_dir_sort(
/*==============*/
	rec_t**	arr,	/* in/out: dense page directory */
	rec_t**	aux_arr,/* in/out: work area */
	ulint	low,	/* in: lower bound of the sorting area, inclusive */
	ulint	high)	/* in: upper bound of the sorting area, exclusive */
{
	UT_SORT_FUNCTION_BODY(page_zip_dir_sort, arr, aux_arr, low, high,
			      page_zip_dir_cmp);
}

/**************************************************************************
Populate the sparse page directory from the dense directory. */
static
ibool
page_zip_dir_decode(
/*================*/
					/* out: TRUE on success,
					FALSE on failure */
	const page_zip_des_t*	page_zip,/* in: dense page directory on
					compressed page */
	page_t*			page,	/* in: compact page with valid header;
					out: trailer and sparse page directory
					filled in */
	rec_t**			recs,	/* out: dense page directory sorted by
					ascending address (and heap_no) */
	rec_t**			recs_aux,/* in/out: scratch area */
	ulint			n_dense)/* in: number of user records, and
					size of recs[] and recs_aux[] */
{
	ulint	i;
	ulint	n_recs;
	byte*	slot;

	n_recs = page_get_n_recs(page);

	if (UNIV_UNLIKELY(n_recs > n_dense)) {
		return(FALSE);
	}

	/* Traverse the list of stored records in the sorting order,
	starting from the first user record. */

	slot = page + (UNIV_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE);
	UNIV_PREFETCH_RW(slot);

	/* Zero out the page trailer. */
	memset(slot + PAGE_DIR_SLOT_SIZE, 0, PAGE_DIR);

	mach_write_to_2(slot, PAGE_NEW_INFIMUM);
	slot -= PAGE_DIR_SLOT_SIZE;
	UNIV_PREFETCH_RW(slot);

	/* Initialize the sparse directory and copy the dense directory. */
	for (i = 0; i < n_recs; i++) {
		ulint	offs = page_zip_dir_get(page_zip, i);

		if (offs & PAGE_ZIP_DIR_SLOT_OWNED) {
			mach_write_to_2(slot, offs & PAGE_ZIP_DIR_SLOT_MASK);
			slot -= PAGE_DIR_SLOT_SIZE;
			UNIV_PREFETCH_RW(slot);
		}

		recs[i] = page + (offs & PAGE_ZIP_DIR_SLOT_MASK);
	}

	mach_write_to_2(slot, PAGE_NEW_SUPREMUM);
	if (UNIV_UNLIKELY(slot != page_dir_get_nth_slot(page,
				page_dir_get_n_slots(page) - 1))) {
		return(FALSE);
	}

	/* Copy the rest of the dense directory. */
	for (; i < n_dense; i++) {
		ulint	offs = page_zip_dir_get(page_zip, i);

		if (UNIV_UNLIKELY(offs & ~PAGE_ZIP_DIR_SLOT_MASK)) {
			return(FALSE);
		}

		recs[i] = page + offs;
	}

	if (UNIV_LIKELY(n_dense > 1)) {
		page_zip_dir_sort(recs, recs_aux, 0, n_dense);
	}
	return(TRUE);
}

static
ibool
page_zip_set_extra_bytes(
/*=====================*/
					/* out: TRUE on success,
					FALSE on failure */
	const page_zip_des_t*	page_zip,/* in: compressed page */
	page_t*			page,	/* in/out: uncompressed page */
	ulint			info_bits)/* in: REC_INFO_MIN_REC_FLAG or 0 */
{
	ulint	n;
	ulint	i;
	ulint	n_owned = 1;
	ulint	offs;
	rec_t*	rec;

	n = page_get_n_recs(page);
	rec = page + PAGE_NEW_INFIMUM;

	for (i = 0; i < n; i++) {
		offs = page_zip_dir_get(page_zip, i);

		if (UNIV_UNLIKELY(offs & PAGE_ZIP_DIR_SLOT_DEL)) {
			info_bits |= REC_INFO_DELETED_FLAG;
		}
		if (UNIV_UNLIKELY(offs & PAGE_ZIP_DIR_SLOT_OWNED)) {
			info_bits |= n_owned;
			n_owned = 1;
		} else {
			n_owned++;
		}
		offs &= PAGE_ZIP_DIR_SLOT_MASK;
		if (UNIV_UNLIKELY(offs < PAGE_ZIP_START
					+ REC_N_NEW_EXTRA_BYTES)) {
			return(FALSE);
		}

		rec_set_next_offs_new(rec, NULL, offs);
		rec = page + offs;
		rec[-REC_N_NEW_EXTRA_BYTES] = info_bits;
		info_bits = 0;
	}

	/* Set the next pointer of the last user record. */
	rec_set_next_offs_new(rec, NULL, PAGE_NEW_SUPREMUM);

	/* Set n_owned of the supremum record. */
	page[PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES] = n_owned;

	/* The dense directory excludes the infimum and supremum records. */
	n = page_dir_get_n_heap(page) - 2;

	if (i >= n) {

		return(UNIV_LIKELY(i == n));
	}

	offs = page_zip_dir_get(page_zip, i);

	/* Set the extra bytes of deleted records on the free list. */
	for (;;) {
		if (UNIV_UNLIKELY(!offs)
		    || UNIV_UNLIKELY(offs & ~PAGE_ZIP_DIR_SLOT_MASK)) {
			return(FALSE);
		}

		rec = page + offs;
		rec[-REC_N_NEW_EXTRA_BYTES] = 0; /* info_bits and n_owned */

		if (++i == n) {
			break;
		}

		offs = page_zip_dir_get(page_zip, i);
		rec_set_next_offs_new(rec, NULL, offs);
	}

	/* Terminate the free list. */
	rec[-REC_N_NEW_EXTRA_BYTES] = 0; /* info_bits and n_owned */
	rec_set_next_offs_new(rec, NULL, 0);

	return(TRUE);
}

/**************************************************************************
Apply the modification log to an uncompressed page. */
static
const byte*
page_zip_apply_log(
/*===============*/
				/* out: pointer to end of modification log,
				or NULL on failure */
	const byte*	data,	/* in: modification log */
	ulint		size,	/* in: maximum length of the log, in bytes */
	page_t*		page)	/* in/out: uncompressed page */
{
	const byte* const end = data + size;

	/* Apply the modification log. */
	while (*data) {
		ulint	ulint_len;
		ulint	length, offset;
		ulint_len = page_zip_ulint_read(data, &length);
		data += ulint_len;
		if (UNIV_UNLIKELY(!ulint_len)
				|| UNIV_UNLIKELY(data + length >= end)) {
			return(NULL);
		}
		ut_a(length > 0 && length < UNIV_PAGE_SIZE - PAGE_DATA);

		ulint_len = page_zip_ulint_read(data, &offset);
		data += ulint_len;
		if (UNIV_UNLIKELY(!ulint_len)
				|| UNIV_UNLIKELY(data + length >= end)) {
			return(NULL);
		}
		/* TODO: determine offset from heap_no */
		offset += PAGE_DATA;
		ut_a(offset + length < UNIV_PAGE_SIZE);

		memcpy(page + offset, data, length);
		data += length;
	}

	return(data);
}

/**************************************************************************
Decompress a page.  This function should tolerate errors on the compressed
page.  Instead of letting assertions fail, it will return FALSE if an
inconsistency is detected. */

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
	byte**		recs;	/* dense page directory, sorted by address */
	byte*		dst;
	ulint		heap_status;/* heap_no and status bits */
	ulint		n_dense;
	mem_heap_t*	heap;
	ulint		info_bits;

	ut_ad(page_zip_simple_validate(page_zip));

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap(page_zip->data) - 2;
	ut_a(n_dense * PAGE_ZIP_DIR_SLOT_SIZE < page_zip->size);

	heap = mem_heap_create(n_dense * (2 * sizeof *recs));
	recs = mem_heap_alloc(heap, n_dense * (2 * sizeof *recs));

	/* Copy the page header. */
	memcpy(page, page_zip->data, PAGE_DATA);

	/* Copy the page directory. */
	if (UNIV_UNLIKELY(!page_zip_dir_decode(page_zip, page,
				recs, recs + n_dense, n_dense))) {
		mem_heap_free(heap);
		return(FALSE);
	}

	/* Copy the infimum and supremum records. */
	memcpy(page + (PAGE_NEW_INFIMUM - REC_N_NEW_EXTRA_BYTES),
		     infimum_extra, sizeof infimum_extra);
	if (UNIV_UNLIKELY(!page_get_n_recs((page_t*) page))) {
		rec_set_next_offs_new(page + PAGE_NEW_INFIMUM,
				NULL, PAGE_NEW_SUPREMUM);
	} else {
		rec_set_next_offs_new(page + PAGE_NEW_INFIMUM,
				NULL,
				page_zip_dir_get(page_zip, 0)
				& PAGE_ZIP_DIR_SLOT_MASK);
	}
	memcpy(page + PAGE_NEW_INFIMUM, infimum_data, sizeof infimum_data);
	memcpy(page + (PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES + 1),
		     supremum_extra_data, sizeof supremum_extra_data);

	/* Decompress the user records. */
	d_stream.zalloc = (alloc_func) 0;
	d_stream.zfree = (free_func) 0;
	d_stream.opaque = (voidpf) 0;

	err = inflateInit(&d_stream);
	ut_a(err == Z_OK);

	d_stream.next_in = page_zip->data + PAGE_DATA;
	d_stream.avail_in = page_zip->size - (PAGE_DATA + 1)
				- n_dense * PAGE_ZIP_DIR_SLOT_SIZE;

	info_bits = 0;

	if (mach_read_from_2((page_t*) page + (PAGE_HEADER + PAGE_LEVEL))) {
		heap_status = REC_STATUS_NODE_PTR | 2 << REC_HEAP_NO_SHIFT;
		if (UNIV_UNLIKELY(mach_read_from_4((page_t*) page
					+ FIL_PAGE_PREV) == FIL_NULL)) {
			info_bits = REC_INFO_MIN_REC_FLAG;
		}
	} else {
		heap_status = REC_STATUS_ORDINARY | 2 << REC_HEAP_NO_SHIFT;
	}

	dst = page + PAGE_ZIP_START;

	if (UNIV_LIKELY(n_dense > 0)) {
		n_dense--;

		if (*recs == page + (PAGE_ZIP_START + REC_N_NEW_EXTRA_BYTES)) {
			dst = page + (PAGE_ZIP_START + REC_N_NEW_EXTRA_BYTES);
			recs++;
		} else {
			/* This is a special case: we are
			decompressing the extra bytes of the first
			user record.  As dst will not be pointing to a
			record, we do not set the heap_no and status
			bits.  On the next round of the loop, dst will
			point to the first user record. */

			goto first_inflate;
		}
	}

	while (n_dense--) {
		/* set heap_no and the status bits */
		mach_write_to_2(dst - REC_NEW_HEAP_NO, heap_status);
		heap_status += 1 << REC_HEAP_NO_SHIFT;
first_inflate:
		d_stream.next_out = dst;
		d_stream.avail_out = *recs - dst - REC_N_NEW_EXTRA_BYTES;

		ut_ad(d_stream.avail_out < UNIV_PAGE_SIZE
					- PAGE_ZIP_START - PAGE_DIR);
		err = inflate(&d_stream, Z_NO_FLUSH);
		switch (err) {
		case Z_OK:
			break;
		case Z_BUF_ERROR:
			if (!d_stream.avail_out) {
				break;
			}
			/* fall through */
		default:
			goto zlib_error;
		}

		dst = *recs++;
	}

	/* Decompress the last record. */
	d_stream.next_out = dst;
	d_stream.avail_out =
			page_header_get_field(page, PAGE_HEAP_TOP)
			- ut_align_offset(dst, UNIV_PAGE_SIZE);
	ut_a(d_stream.avail_out < UNIV_PAGE_SIZE
				- PAGE_ZIP_START - PAGE_DIR);

	/* set heap_no and the status bits */
	mach_write_to_2(dst - REC_NEW_HEAP_NO, heap_status);

	err = inflate(&d_stream, Z_FINISH);

	if (err != Z_STREAM_END) {
zlib_error:
		inflateEnd(&d_stream);
		mem_heap_free(heap);
		return(FALSE);
	}

	err = inflateEnd(&d_stream);
	ut_a(err == Z_OK);

	mem_heap_free(heap);

	if (UNIV_UNLIKELY(!page_zip_set_extra_bytes(
				page_zip, page, info_bits))) {
		return(FALSE);
	}

	/* Clear the unused heap space on the uncompressed page. */
	dst = page_header_get_ptr(page, PAGE_HEAP_TOP);
	memset(dst, 0, page_dir_get_nth_slot(page,
				page_dir_get_n_slots(page) - 1) - dst);

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap(page) - 2;

	page_zip->m_start = PAGE_DATA + d_stream.total_in;

	/* Apply the modification log. */
	{
		const byte*	mod_log_ptr;
		mod_log_ptr = page_zip_apply_log(
				page_zip->data + page_zip->m_start,
				d_stream.avail_in, page);
		if (UNIV_UNLIKELY(!mod_log_ptr)) {
			return(FALSE);
		}
		page_zip->m_end = mod_log_ptr - page_zip->data;
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
	page_t*		temp_page = buf_frame_alloc();
	ibool		valid;

	ut_ad(buf_block_get_page_zip(buf_block_align((byte*)page))
		== page_zip);

	valid = page_zip_decompress(&temp_page_zip, temp_page, NULL)
				&& !memcmp(page, temp_page,
				UNIV_PAGE_SIZE - FIL_PAGE_DATA_END);
	buf_frame_free(temp_page);
	return(valid);
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
	ulint	trailer_len = page_zip_dir_size(page_zip);
#endif /* UNIV_DEBUG */

	ut_ad(buf_block_get_page_zip(buf_block_align((byte*)str)) == page_zip);
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(!memcmp(ut_align_down((byte*) str, UNIV_PAGE_SIZE),
		page_zip->data, PAGE_ZIP_START));
	ut_ad(!page_zip->data[page_zip->m_end]);

	ut_ad(pos >= PAGE_DATA);
	ut_ad(pos + length <= UNIV_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE
		* page_dir_get_n_slots(buf_frame_align((byte*)str)));

	pos -= PAGE_DATA;
	/* TODO: encode heap_no instead of pos */

	ut_ad(page_zip_available(page_zip, page_zip_entry_size(pos, length)));

	/* Append to the modification log. */
	page_zip->m_end += page_zip_ulint_write(
				page_zip->data + page_zip->m_end, length);
	page_zip->m_end += page_zip_ulint_write(
				page_zip->data + page_zip->m_end, pos);
	memcpy(&page_zip->data[page_zip->m_end], str, length);
	page_zip->m_end += length;
	ut_ad(!page_zip->data[page_zip->m_end]);
	ut_ad(page_zip->m_end + trailer_len < page_zip->size);
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
