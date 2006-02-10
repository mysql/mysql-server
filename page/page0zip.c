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
#include "dict0boot.h"
#include "btr0cur.h"
#include "page0types.h"
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
Encode the length of a fixed-length column. */
static
byte*
page_zip_fixed_field_encode(
/*========================*/
			/* out: buf + length of encoded val */
	byte*	buf,	/* in: pointer to buffer where to write */
	ulint	val)	/* in: value to write */
{
	ut_ad(val >= 2);

	if (UNIV_LIKELY(val < 126)) {
		/*
		0 = nullable variable field of at most 255 bytes length;
		1 = not null variable field of at most 255 bytes length;
		126 = nullable variable field with maximum length >255;
		127 = not null variable field with maximum length >255
		*/
		*buf++ = val;
	} else {
		*buf++ = 0x80 | val >> 7;
		*buf++ = 0xff & val;
	}

	return(buf);
}

/**************************************************************************
Write the index information for the compressed page. */
static
ulint
page_zip_fields_encode(
/*===================*/
				/* out: used size of buf */
	ulint		n,	/* in: number of fields to compress */
	dict_index_t*	index,	/* in: index comprising at least n fields */
	ulint		trx_id_pos,/* in: position of the trx_id column
				in the index, or ULINT_UNDEFINED if
				this is a non-leaf page */
	byte*		buf)	/* out: buffer of (n + 1) * 2 bytes */
{
	const byte*	buf_start	= buf;
	ulint		i;
	ulint		col;
	ulint		trx_id_col	= 0;
	/* sum of lengths of preceding non-nullable fixed fields, or 0 */
	ulint		fixed_sum	= 0;

	ut_ad(trx_id_pos == ULINT_UNDEFINED || trx_id_pos < n);

	for (i = col = 0; i < n; i++) {
		dict_field_t*	field = dict_index_get_nth_field(index, i);
		ulint		val;

		if (dtype_get_prtype(dict_col_get_type(
						dict_field_get_col(field)))
						& DATA_NOT_NULL) {
			val = 1; /* set the "not nullable" flag */
		} else {
			val = 0; /* nullable field */
		}

		if (!field->fixed_len) {
			/* variable-length field */

			dtype_t*	type = dict_col_get_type(
					dict_field_get_col(field));

			if (UNIV_UNLIKELY(dtype_get_len(type) > 255)
			    || UNIV_UNLIKELY(dtype_get_mtype(type)
							== DATA_BLOB)) {
				val |= 0x7e; /* max > 255 bytes */
			}

			if (fixed_sum) {
				/* write out the length of any
				preceding non-nullable fields */
				buf = page_zip_fixed_field_encode(buf,
						fixed_sum << 1 | 1);
				fixed_sum = 0;
				col++;
			}

			*buf++ = val;
			col++;
		} else if (val) {
			/* fixed-length non-nullable field */
			if (i && UNIV_UNLIKELY(i == trx_id_pos)) {
				if (fixed_sum) {
					/* Write out the length of any
					preceding non-nullable fields,
					and start a new trx_id column. */
					buf = page_zip_fixed_field_encode(buf,
							fixed_sum << 1 | 1);
				}

				fixed_sum = field->fixed_len;
				trx_id_col = ++col;
			} else {
				/* add to the sum */
				fixed_sum += field->fixed_len;
			}
		} else {
			/* fixed-length nullable field */

			if (fixed_sum) {
				/* write out the length of any
				preceding non-nullable fields */
				buf = page_zip_fixed_field_encode(buf,
						fixed_sum << 1 | 1);
				fixed_sum = 0;
				col++;
			}

			buf = page_zip_fixed_field_encode(buf,
						field->fixed_len << 1);
			col++;
		}
	}

	if (fixed_sum) {
		/* Write out the lengths of last fixed-length columns. */
		buf = page_zip_fixed_field_encode(buf, fixed_sum << 1 | 1);
	}

	if (trx_id_pos != ULINT_UNDEFINED) {
		/* Write out the position of the trx_id column */
		if (trx_id_col < 128) {
			*buf++ = trx_id_col;
		} else {
			*buf++ = 0x80 | trx_id_col >> 7;
			*buf++ = 0xff & trx_id_col;
		}
	}

	ut_ad((ulint) (buf - buf_start) <= (n + 1) * 2);
	return((ulint) (buf - buf_start));
}

/**************************************************************************
Populate the dense page directory from the sparse directory. */
static
void
page_zip_dir_encode(
/*================*/
	const page_t*	page,	/* in: compact page */
	page_zip_des_t*	page_zip,/* out: dense directory on compressed page */
	const rec_t**	recs)	/* in: pointer to array of 0, or NULL;
				out: dense page directory sorted by ascending
				address (and heap_no) */
{
	byte*	rec;
	ulint	status;
	ulint	min_mark;
	ulint	heap_no;
	ulint	i;
	ulint	n_heap;
	ulint	offs;

	min_mark = 0;

	if (page_is_leaf(page)) {
		status = REC_STATUS_ORDINARY;
	} else {
		status = REC_STATUS_NODE_PTR;
		if (UNIV_UNLIKELY(mach_read_from_4((page_t*) page
					+ FIL_PAGE_PREV) == FIL_NULL)) {
			min_mark = REC_INFO_MIN_REC_FLAG;
		}
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

		if (UNIV_LIKELY_NULL(recs)) {
			/* Ensure that each heap_no occurs at most once. */
			ut_a(!recs[heap_no - 2]);
			/* exclude infimum and supremum */
			recs[heap_no - 2] = rec;
		}

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

		if (UNIV_LIKELY_NULL(recs)) {
			/* Ensure that each heap_no occurs at most once. */
			ut_a(!recs[heap_no - 2]);
			/* exclude infimum and supremum */
			recs[heap_no - 2] = rec;
		}

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
	page_zip_des_t*	page_zip,/* in: size; out: data, n_blobs,
				m_start, m_end */
	const page_t*	page,	/* in: uncompressed page */
	dict_index_t*	index,	/* in: index of the B-tree node */
	mtr_t*		mtr)	/* in: mini-transaction handle,
				or NULL if no logging is needed */
{
	z_stream	c_stream;
	int		err;
	ulint		n_fields;/* number of index fields needed */
	byte*		fields;	/* index field information */
	byte*		buf;	/* compressed payload of the page */
	ulint		n_dense;
	const rec_t**	recs;	/* dense page directory, sorted by address */
	mem_heap_t*	heap;
	ulint		trx_id_col;
	ulint*		offsets	= NULL;
	ulint		n_blobs	= 0;
	byte*		storage;/* storage of uncompressed columns */

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

	if (page_is_leaf(page)) {
		n_fields = dict_index_get_n_fields(index);
	} else {
		n_fields = dict_index_get_n_unique_in_tree(index);
	}

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap((page_t*) page) - 2;
	if (UNIV_UNLIKELY(n_dense * PAGE_ZIP_DIR_SLOT_SIZE
					>= page_zip->size)) {
		return(FALSE);
	}

	heap = mem_heap_create(page_zip->size
		+ n_fields * (2 + sizeof *offsets)
		+ n_dense * ((sizeof *recs) - PAGE_ZIP_DIR_SLOT_SIZE));

	recs = mem_heap_alloc(heap, n_dense * sizeof *recs);
	memset(recs, 0, n_dense * sizeof *recs);

	fields = mem_heap_alloc(heap, (n_fields + 1) * 2);

	buf = mem_heap_alloc(heap, page_zip->size
			- PAGE_DATA - PAGE_ZIP_DIR_SLOT_SIZE * n_dense);

	/* Compress the data payload. */
	c_stream.zalloc = (alloc_func) 0;
	c_stream.zfree = (free_func) 0;
	c_stream.opaque = (voidpf) 0;

	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	ut_a(err == Z_OK);

	c_stream.next_out = buf;
	/* Subtract the space reserved for uncompressed data. */
	/* Page header, n_relocated, end marker of modification log */
	c_stream.avail_out = page_zip->size
			- (PAGE_DATA + 2 * PAGE_ZIP_DIR_SLOT_SIZE);
	/* Dense page directory and uncompressed columns, if any */
	if (page_is_leaf(page)) {
		trx_id_col = dict_index_get_sys_col_pos(index, DATA_TRX_ID);
		ut_ad(trx_id_col > 0);
		if (trx_id_col == ULINT_UNDEFINED) {
			/* Signal the absence of trx_id
			in page_zip_fields_encode() */
			trx_id_col = 0;
			c_stream.avail_out -= n_dense * PAGE_ZIP_DIR_SLOT_SIZE;
		} else {
			c_stream.avail_out -= n_dense * (PAGE_ZIP_DIR_SLOT_SIZE
				+ DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
		}
	} else {
		c_stream.avail_out -= n_dense * (PAGE_ZIP_DIR_SLOT_SIZE
				+ REC_NODE_PTR_SIZE);
		trx_id_col = ULINT_UNDEFINED;
	}

	c_stream.avail_in = page_zip_fields_encode(
					n_fields, index, trx_id_col, fields);
	c_stream.next_in = fields;
	if (!trx_id_col) {
		trx_id_col = ULINT_UNDEFINED;
	}

	err = deflate(&c_stream, Z_FULL_FLUSH);
	if (err != Z_OK) {
		goto zlib_error;
	}

	/* TODO: do not write to page_zip->data until deflateEnd() */
	page_zip_set_n_relocated(page_zip, 0);
	page_zip_dir_encode(page, page_zip, recs);

	c_stream.next_in = (byte*) page + PAGE_ZIP_START;

	/* TODO: do not write to page_zip->data until deflateEnd() */
	storage = page_zip->data + page_zip->size
			- (n_dense + 1)
			* PAGE_ZIP_DIR_SLOT_SIZE;

	if (page_is_leaf(page)) {
		/* BTR_EXTERN_FIELD_REF storage */
		byte*	externs = storage - n_dense
				* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

		while (n_dense--) {
			ulint	i;
			rec_t*	rec = (rec_t*) *recs++;

			offsets = rec_get_offsets(rec, index, offsets,
					ULINT_UNDEFINED, &heap);
			ut_ad(rec_offs_n_fields(offsets) == n_fields);

			/* Compress the extra bytes. */
			c_stream.avail_in = rec - REC_N_NEW_EXTRA_BYTES
					- c_stream.next_in;

			if (c_stream.avail_in) {
				err = deflate(&c_stream, Z_NO_FLUSH);
				if (err != Z_OK) {
					goto zlib_error;
				}
			}

			/* Compress the data bytes. */

			c_stream.next_in = rec;

			/* Check if there are any externally stored columns.
			For each externally stored column, store the
			BTR_EXTERN_FIELD_REF separately._*/

			for (i = 0; i < n_fields; i++) {
				ulint	len;
				byte*	src;

				if (UNIV_UNLIKELY(i == trx_id_col)) {
				    ut_ad(!rec_offs_nth_extern(offsets, i));
				    /* Store trx_id and roll_ptr
				    in uncompressed form. */
				    src = rec_get_nth_field(rec, offsets,
								i, &len);
#ifdef UNIV_DEBUG
				    ut_ad(len == DATA_TRX_ID_LEN);
				    rec_get_nth_field(rec, offsets,
								i + 1, &len);
				    ut_ad(len == DATA_ROLL_PTR_LEN);
#endif /* UNIV_DEBUG */
				    /* Compress any preceding bytes. */
				    c_stream.avail_in = src - c_stream.next_in;

				    if (c_stream.avail_in) {
					err = deflate(&c_stream, Z_NO_FLUSH);
					if (err != Z_OK) {
					    goto zlib_error;
					}
				    }

				    ut_ad(c_stream.next_in == src);

				    memcpy(storage - (DATA_TRX_ID_LEN
							+ DATA_ROLL_PTR_LEN)
					* (rec_get_heap_no_new(rec) - 1),
					c_stream.next_in,
					DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);

				    c_stream.next_in +=
					DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;

				    /* Skip also roll_ptr */
				    i++;
				} else if (rec_offs_nth_extern(offsets, i)) {
				    src = rec_get_nth_field(rec, offsets,
								i, &len);
				    ut_ad(len > BTR_EXTERN_FIELD_REF_SIZE);
				    src += len - BTR_EXTERN_FIELD_REF_SIZE;

				    c_stream.avail_in = src - c_stream.next_in;
				    ut_ad(c_stream.avail_in);
				    err = deflate(&c_stream, Z_NO_FLUSH);
				    if (err != Z_OK) {
					goto zlib_error;
				    }

				    ut_ad(c_stream.next_in == src);

				    /* Reserve space for the data at
				    the end of the space reserved for
				    the compressed data and the page
				    modification log. */

				    if (UNIV_UNLIKELY(c_stream.avail_out
					    <= BTR_EXTERN_FIELD_REF_SIZE)) {
					/* out of space */
					goto zlib_error;
				    }

				    c_stream.avail_out
						-= BTR_EXTERN_FIELD_REF_SIZE;
				    externs -= BTR_EXTERN_FIELD_REF_SIZE;

				    ut_ad(externs > c_stream.next_in);

				    /* Copy the BLOB pointer */
				    memcpy(externs, c_stream.next_in,
						BTR_EXTERN_FIELD_REF_SIZE);
				    c_stream.next_in +=
						BTR_EXTERN_FIELD_REF_SIZE;
				    /* Increment the BLOB counter */
				    n_blobs++;
				}
			}

			/* Compress the last bytes of the record. */
			c_stream.avail_in = rec_get_end(rec, offsets)
					- c_stream.next_in;

			if (c_stream.avail_in) {
				err = deflate(&c_stream, Z_NO_FLUSH);
				if (err != Z_OK) {
					goto zlib_error;
				}
			}
		}
	} else {
		/* This is a node pointer page. */
		while (n_dense--) {
			rec_t*	rec = (rec_t*) *recs++;

			offsets = rec_get_offsets(rec, index, offsets,
					ULINT_UNDEFINED, &heap);
			ut_ad(rec_offs_n_fields(offsets) == n_fields + 1);
			/* Non-leaf nodes should not have any externally
			stored columns. */
			ut_ad(!rec_offs_any_extern(offsets));

			/* Compress the extra bytes. */
			c_stream.avail_in = rec - REC_N_NEW_EXTRA_BYTES
					- c_stream.next_in;

			if (c_stream.avail_in) {
				err = deflate(&c_stream, Z_NO_FLUSH);
				if (err != Z_OK) {
					goto zlib_error;
				}
			}

			/* Compress the data bytes, except node_ptr. */
			c_stream.next_in = rec;
			c_stream.avail_in = rec_offs_data_size(offsets)
					- REC_NODE_PTR_SIZE;
			ut_ad(c_stream.avail_in);

			err = deflate(&c_stream, Z_NO_FLUSH);
			if (err != Z_OK) {
				goto zlib_error;
			}

			memcpy(storage - REC_NODE_PTR_SIZE
					* (rec_get_heap_no_new(rec) - 1),
					c_stream.next_in, REC_NODE_PTR_SIZE);
			c_stream.next_in += REC_NODE_PTR_SIZE;
		}
	}

	ut_ad(page + page_header_get_field((page_t*) page, PAGE_HEAP_TOP)
			== c_stream.next_in);
	/* Finish the compression. */
	ut_ad(!c_stream.avail_in);

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
	page_zip->n_blobs = n_blobs;
	/* Copy the page header */
	memcpy(page_zip->data, page, PAGE_DATA);
	/* Copy the compressed data */
	memcpy(page_zip->data + PAGE_DATA, buf, c_stream.total_out);
	/* Zero out the area reserved for the modification log */
	memset(page_zip->data + PAGE_DATA + c_stream.total_out, 0,
		c_stream.avail_out + PAGE_ZIP_DIR_SLOT_SIZE);
	mem_heap_free(heap);
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, page));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */

	if (UNIV_LIKELY_NULL(mtr)) {
		mlog_open_and_write_index(mtr, (page_t*) page, index,
					MLOG_ZIP_COMPRESS, 0);
	}

	return(TRUE);
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
Deallocate the index information initialized by page_zip_fields_decode(). */
static
void
page_zip_fields_free(
/*=================*/
	dict_index_t*	index)	/* in: dummy index to be freed */
{
	if (index) {
		dict_table_t*	table = index->table;
		mem_heap_free(index->heap);
		mutex_free(&(table->autoinc_mutex));
		mem_heap_free(table->heap);
	}
}

/**************************************************************************
Read the index information for the compressed page. */
static
dict_index_t*
page_zip_fields_decode(
/*===================*/
				/* out,own: dummy index describing the page,
				or NULL on error */
	const byte*	buf,	/* in: index information */
	const byte*	end,	/* in: end of buf */
	ulint*		trx_id_col)/* in: NULL for non-leaf pages;
				for leaf pages, pointer to where to store
				the position of the trx_id column */
{
	const byte*	b;
	ulint		n;
	ulint		i;
	dict_table_t*	table;
	dict_index_t*	index;

	/* Determine the number of fields. */
	for (b = buf, n = 0; b < end; n++) {
		if (*b++ & 0x80) {
			b++; /* skip the second byte */
		}
	}

	if (UNIV_UNLIKELY(n > REC_MAX_N_FIELDS)
			|| UNIV_UNLIKELY(b > end)) {

		return(NULL);
	}

	if (trx_id_col) {
		n--;
	}

	table = dict_mem_table_create("ZIP_DUMMY", DICT_HDR_SPACE, n, TRUE);
	index = dict_mem_index_create("ZIP_DUMMY", "ZIP_DUMMY",
				DICT_HDR_SPACE, 0, n);
	index->table = table;
	index->n_uniq = n;
	/* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
	index->cached = TRUE;

	/* Initialize the fields. */
	for (b = buf, i = 0; i < n; i++) {
		ulint	val	= *b++;
		ulint	mtype;
		ulint	len;

		if (UNIV_UNLIKELY(val & 0x80)) {
			val = (val & 0x7f) << 7 | *b++;
		}

		len = val >> 1;

		switch (len) {
		case 0x7e:
			len = 0x7fff;
			/* fall through */
		case 0:
			mtype = DATA_BINARY;
			break;
		default:
			mtype = DATA_FIXBINARY;
		}
		dict_mem_table_add_col(table, "DUMMY", mtype,
				val & 1 ? DATA_NOT_NULL : 0, len, 0);
		dict_index_add_col(index,
				dict_table_get_nth_col(table, i), 0, 0);
	}

	/* Decode the position of the trx_id column. */
	if (trx_id_col) {
		ulint	val = *b++;
		if (UNIV_UNLIKELY(val & 0x80)) {
			val = (val & 0x7f) << 7 | *b++;
		}

		if (UNIV_UNLIKELY(val >= n)) {
			page_zip_fields_free(index);
			index = NULL;
		}

		if (!val) {
			val = ULINT_UNDEFINED;
		}

		*trx_id_col = val;
	}

	ut_ad(b == end);

	return(index);
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

		ut_ad((offs & PAGE_ZIP_DIR_SLOT_MASK)
				>= PAGE_ZIP_START + REC_N_NEW_EXTRA_BYTES);

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

/**************************************************************************
Initialize the REC_N_NEW_EXTRA_BYTES of each record. */
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

		rec_set_next_offs_new(rec, offs);
		rec = page + offs;
		rec[-REC_N_NEW_EXTRA_BYTES] = info_bits;
		info_bits = 0;
	}

	/* Set the next pointer of the last user record. */
	rec_set_next_offs_new(rec, PAGE_NEW_SUPREMUM);

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
		rec_set_next_offs_new(rec, offs);
	}

	/* Terminate the free list. */
	rec[-REC_N_NEW_EXTRA_BYTES] = 0; /* info_bits and n_owned */
	rec_set_next_offs_new(rec, 0);

	return(TRUE);
}

/**************************************************************************
Find the heap number of a record by binary search in the sorted
dense page directory. */
static
ulint
page_zip_find_heap_no(
/*==================*/
				/* out: the heap number of the smallest record
				in recs[] that is >= start; 0 if not found */
	const byte*	start,	/* in: start address of the record */
	rec_t**		recs,	/* in: dense page directory,
				sorted by address (indexed by heap_no - 2) */
	ulint		n_dense)/* in: number of entries in recs[] */
{
	ulint	low	= 0;
	ulint	high	= n_dense;
	ulint	mid;

	for (;;) {
		mid = (low + high) / 2;

		/* 'start' should be at least REC_N_NEW_EXTRA_BYTES
		smaller than the matching entry in recs[] */
		ut_ad(start != recs[mid]);

		if (UNIV_UNLIKELY(low == high)) {
			if (UNIV_UNLIKELY(start > recs[high])) {
				return(0);
			}
			break;
		}

		if (start > recs[mid]) {
			/* Too high */
			high = mid;
		} else {
			/* Either this is too low, or we found a match. */
			low = mid + 1;
			if (start > recs[low]) {
				/* The adjacent record does not match.
				This is the closest match. */
				break;
			}
		}
	}

	return(mid + 2);
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
	page_t*		page,	/* out: uncompressed page */
	rec_t**		recs,	/* in: dense page directory,
				sorted by address (indexed by heap_no - 2) */
	ulint		n_dense,/* in: size of recs[] */
	ulint		heap_status,
				/* in: heap_no and status bits for
				the next record to uncompress */
	dict_index_t*	index,	/* in: index of the page */
	ulint*		offsets)/* in/out: work area for
				rec_get_offsets_reverse() */
{
	const byte* const end = data + size;

	for (;;) {
		ulint	start;
		rec_t*	rec;
		ulint	len;
		ulint	hs;

		start = mach_read_from_2((byte*) data);
		if (UNIV_UNLIKELY(data + 2 >= end)) {
			return(NULL);
		}
		if (UNIV_UNLIKELY(!start)) {
			break;
		}
		if (UNIV_UNLIKELY(start < PAGE_ZIP_START)) {
			return(NULL);
		}

		data += 2;

		/* Determine the heap number of the record. */
		hs = page_zip_find_heap_no(page + start, recs, n_dense)
				<< REC_HEAP_NO_SHIFT;
		if (UNIV_UNLIKELY(!hs)) {
			return(NULL);
		}
		hs |= heap_status & ((1 << REC_HEAP_NO_SHIFT) - 1);

		/* This may either be an old record that is being
		overwritten (updated in place, or allocated from
		the free list), or a new record, with the next
		available_heap_no. */
		if (UNIV_UNLIKELY(hs > heap_status)) {
			return(NULL);
		} else if (hs == heap_status) {
			/* A new record was allocated from the heap. */
			heap_status += REC_HEAP_NO_SHIFT;
		}

		rec_get_offsets_reverse(data, index,
				heap_status & REC_STATUS_NODE_PTR,
				offsets);

		rec = page + start + rec_offs_extra_size(offsets);

		mach_write_to_2(rec - REC_NEW_HEAP_NO, hs);

		/* Copy the extra bytes (backwards). */
		{
			ulint	n = rec_offs_extra_size(offsets)
					- REC_N_NEW_EXTRA_BYTES;
			byte*	b = rec - REC_N_NEW_EXTRA_BYTES;
			while (n--) {
				*b-- = *data++;
			}
		}

		/* Copy the data bytes. */
		if (UNIV_UNLIKELY(heap_status & REC_STATUS_NODE_PTR)) {
			/* Non-leaf nodes should not contain any
			externally stored columns. */
			if (UNIV_UNLIKELY(rec_offs_any_extern(offsets))) {
				return(NULL);
			}

			len = rec_offs_data_size(offsets)
					- REC_NODE_PTR_SIZE;
			/* Copy the data bytes, except node_ptr. */
			if (UNIV_UNLIKELY(data + len >= end)) {
				return(NULL);
			}
			memcpy(rec, data, len);
			data += len;
		} else {
			ulint	i;
			byte*	next_out = rec;

			/* Check if there are any externally stored columns.
			For each externally stored column, skip the
			BTR_EXTERN_FIELD_REF._*/

			for (i = 0; i < rec_offs_n_fields(offsets); i++) {
				if (rec_offs_nth_extern(offsets, i)) {
					byte*	dst = rec_get_nth_field(
							rec, offsets, i, &len);
					ut_ad(len > BTR_EXTERN_FIELD_REF_SIZE);

					len += dst - next_out
						- BTR_EXTERN_FIELD_REF_SIZE;

					if (UNIV_UNLIKELY(data + len >= end)) {
						return(NULL);
					}
					memcpy(next_out, data, len);
					data += len;
					next_out += len
						+ BTR_EXTERN_FIELD_REF_SIZE;
				}
			}

			/* Copy the last bytes of the record.
			Skip roll_ptr and trx_id. */
			len = rec_get_end(rec, offsets)
					- (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
					- next_out;
			if (UNIV_UNLIKELY(data + len >= end)) {
				return(NULL);
			}
			memcpy(next_out, data, len);
			data += len;
		}
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
	page_zip_des_t*	page_zip,/* in: data, size;
				out: m_start, m_end, n_blobs */
	page_t*		page,	/* out: uncompressed page, may be trashed */
	mtr_t*		mtr)	/* in: mini-transaction handle,
				or NULL if no logging is needed */
{
	z_stream	d_stream;
	int		err;
	dict_index_t*	index	= NULL;
	rec_t**		recs;	/* dense page directory, sorted by address */
	rec_t**		recsc;	/* cursor to dense page directory */
	ulint		heap_status;/* heap_no and status bits */
	ulint		n_dense;/* number of user records on the page */
	ulint		reloc	= 0;/* index to page_zip_get_relocated() */
	ulint		orig	= ULINT_UNDEFINED;
				/* page_zip_get_relocated(reloc),
				or ULINT_UNDEFINED */
	ulint		trx_id_col = ULINT_UNDEFINED;
	mem_heap_t*	heap;
	ulint*		offsets	= NULL;
	ulint		info_bits = 0;
	const byte*	storage;
	const byte*	externs;

	ut_ad(page_zip_simple_validate(page_zip));

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap(page_zip->data) - 2;
	ut_a(n_dense * PAGE_ZIP_DIR_SLOT_SIZE < page_zip->size);

	heap = mem_heap_create(n_dense * (3 * sizeof *recs));
	recsc = recs = mem_heap_alloc(heap, n_dense * (2 * sizeof *recs));

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
				PAGE_NEW_SUPREMUM);
	} else {
		rec_set_next_offs_new(page + PAGE_NEW_INFIMUM,
				page_zip_dir_get(page_zip, 0)
				& PAGE_ZIP_DIR_SLOT_MASK);
	}
	memcpy(page + PAGE_NEW_INFIMUM, infimum_data, sizeof infimum_data);
	memcpy(page + (PAGE_NEW_SUPREMUM - REC_N_NEW_EXTRA_BYTES + 1),
		     supremum_extra_data, sizeof supremum_extra_data);

	d_stream.zalloc = (alloc_func) 0;
	d_stream.zfree = (free_func) 0;
	d_stream.opaque = (voidpf) 0;

	err = inflateInit(&d_stream);
	ut_a(err == Z_OK);

	d_stream.next_in = page_zip->data + PAGE_DATA;
	d_stream.avail_in = page_zip->size - (PAGE_DATA + 1);

	d_stream.next_out = page + PAGE_ZIP_START;
	d_stream.avail_out = UNIV_PAGE_SIZE - PAGE_ZIP_START;

	/* Decode the zlib header. */
	err = inflate(&d_stream, Z_BLOCK);
	if (err != Z_OK) {

		goto zlib_error;
	}

	/* Decode the index information. */
	err = inflate(&d_stream, Z_BLOCK);
	if (err != Z_OK) {

		goto zlib_error;
	}

	index = page_zip_fields_decode(page + PAGE_ZIP_START,
				d_stream.next_out,
				page_is_leaf(page) ? &trx_id_col : NULL);

	if (UNIV_UNLIKELY(!index)) {

		goto zlib_error;
	}

	/* Decompress the user records. */
	d_stream.next_out = page + PAGE_ZIP_START;

	{
		/* Pre-allocate the offsets
		for rec_get_offsets_reverse(). */
		ulint	n;

		if (page_is_leaf(page)) {
			n = dict_index_get_n_fields(index);
			heap_status = REC_STATUS_ORDINARY
				| 2 << REC_HEAP_NO_SHIFT;

			/* Subtract the space reserved
			for uncompressed data. */
			if (trx_id_col != ULINT_UNDEFINED) {
				d_stream.avail_in -= n_dense
					* (PAGE_ZIP_DIR_SLOT_SIZE
					+ DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
			} else {
				d_stream.avail_in -= n_dense
					* PAGE_ZIP_DIR_SLOT_SIZE;
			}
		} else {
			n = dict_index_get_n_unique_in_tree(index) + 1;
			heap_status = REC_STATUS_NODE_PTR
				| 2 << REC_HEAP_NO_SHIFT;

			if (UNIV_UNLIKELY(mach_read_from_4((page_t*) page
					+ FIL_PAGE_PREV) == FIL_NULL)) {
				info_bits = REC_INFO_MIN_REC_FLAG;
			}

			/* Subtract the space reserved
			for uncompressed data. */
			d_stream.avail_in -= n_dense
				* (PAGE_ZIP_DIR_SLOT_SIZE + REC_NODE_PTR_SIZE);
		}

		n += 1 + REC_OFFS_HEADER_SIZE;
		offsets = mem_heap_alloc(heap, n * sizeof(ulint));
		*offsets = n;
	}

	if (page_zip_get_n_relocated(page_zip)) {
		orig = page_zip_get_relocated(page_zip, reloc);
		reloc++;
	}

	page_zip->n_blobs = 0;

	if (UNIV_UNLIKELY(!n_dense)) {
		d_stream.avail_out = 0;
		err = inflate(&d_stream, Z_FINISH);

		if (err == Z_STREAM_END) {
			goto zlib_error;
		}

		goto zlib_done;
	}

	while (n_dense--) {
		byte* const	last	= d_stream.next_out;
		rec_t*		rec	= *recsc++;

		/* Was the record relocated? */
		if (UNIV_UNLIKELY(orig
				< ut_align_offset(rec, UNIV_PAGE_SIZE))) {
			/* The record was relocated since the page was
			compressed.  Get the original offset. */
			rec = page + orig;

			/* Get the offset of the next relocated record. */
			if (reloc < page_zip_get_n_relocated(page_zip)) {
				orig = page_zip_get_relocated(page_zip, reloc);
				ut_ad(ut_align_offset(rec, UNIV_PAGE_SIZE)
					< orig);
				reloc++;
			} else {
				/* End of list */
				orig = ULINT_UNDEFINED;
			}
		}

		d_stream.avail_out = rec - REC_N_NEW_EXTRA_BYTES - last;

		ut_ad(d_stream.avail_out < UNIV_PAGE_SIZE
			      - PAGE_ZIP_START - PAGE_DIR);
		err = inflate(&d_stream, Z_NO_FLUSH);
		switch (err) {
		case Z_OK:
			break;
		case Z_STREAM_END:
			/* Apparently, n_dense has grown
			since the time the page was last compressed. */
			if (UNIV_UNLIKELY(d_stream.next_out != last)) {
				/* Somehow, we got a partial record. */
				goto zlib_error;
			}
			goto zlib_done;
		case Z_BUF_ERROR:
			if (!d_stream.avail_out) {
				break;
			}
		default:
			goto zlib_error;
		}

		ut_ad(d_stream.next_out == rec - REC_N_NEW_EXTRA_BYTES);
		/* Prepare to decompress the data bytes. */
		d_stream.next_out = rec;
		/* Set heap_no and the status bits. */
		mach_write_to_2(rec - REC_NEW_HEAP_NO, heap_status);
		heap_status += 1 << REC_HEAP_NO_SHIFT;

		/* Read the offsets. The status bits are needed here. */
		offsets = rec_get_offsets(rec, index, offsets,
					ULINT_UNDEFINED, &heap);

		if (page_is_leaf(page)) {
			ulint	i;

			/* Check if there are any externally stored columns.
			For each externally stored column, restore the
			BTR_EXTERN_FIELD_REF separately._*/

			for (i = 0; i < rec_offs_n_fields(offsets); i++) {
				ulint	len;
				byte*	dst;

				if (UNIV_UNLIKELY(i == trx_id_col)) {
					/* Skip trx_id and roll_ptr */
					dst = rec_get_nth_field(
							rec, offsets, i, &len);
					if (UNIV_UNLIKELY(len < DATA_TRX_ID_LEN
							+ DATA_ROLL_PTR_LEN)
					    || rec_offs_nth_extern(
							offsets, i)) {

						goto zlib_error;
					}

					d_stream.avail_out = dst
						- d_stream.next_out;
					err = inflate(&d_stream, Z_NO_FLUSH);
					switch (err) {
					case Z_OK:
						break;
					case Z_STREAM_END:
						if (!n_dense) {
							/* This was the last
							record. */
							goto zlib_done;
						}
						goto zlib_error;
					case Z_BUF_ERROR:
						if (!d_stream.avail_out) {
							break;
						}
						/* fall through */
					default:
						goto zlib_error;
					}

					ut_ad(d_stream.next_out == dst);

					d_stream.avail_out -= DATA_TRX_ID_LEN
							+ DATA_ROLL_PTR_LEN;
					d_stream.next_out += DATA_TRX_ID_LEN
							+ DATA_ROLL_PTR_LEN;
				} else if (rec_offs_nth_extern(offsets, i)) {
					dst = rec_get_nth_field(
							rec, offsets, i, &len);
					ut_ad(len > BTR_EXTERN_FIELD_REF_SIZE);
					dst += len - BTR_EXTERN_FIELD_REF_SIZE;

					d_stream.avail_out = dst
						- d_stream.next_out;
					err = inflate(&d_stream, Z_NO_FLUSH);
					switch (err) {
					case Z_OK:
						break;
					case Z_STREAM_END:
						if (!n_dense) {
							/* This was the last
							record. */
							goto zlib_done;
						}
						goto zlib_error;
					case Z_BUF_ERROR:
						if (!d_stream.avail_out) {
							break;
						}
						/* fall through */
					default:
						goto zlib_error;
					}

					ut_ad(d_stream.next_out == dst);

					/* Reserve space for the data at
					the end of the space reserved for
					the compressed data and the
					page modification log. */

					if (UNIV_UNLIKELY(d_stream.avail_in
					    <= BTR_EXTERN_FIELD_REF_SIZE)) {
						/* out of space */
						goto zlib_error;
					}

					d_stream.avail_in
						-= BTR_EXTERN_FIELD_REF_SIZE;
					d_stream.next_out
						+= BTR_EXTERN_FIELD_REF_SIZE;
					page_zip->n_blobs++;
				}
			}

			/* Decompress the last bytes of the record. */
			d_stream.avail_out = rec_get_end(rec, offsets)
					- d_stream.next_out;

			err = inflate(&d_stream, Z_NO_FLUSH);
			switch (err) {
			case Z_OK:
				break;
			case Z_STREAM_END:
				if (!n_dense) {
					/* This was the last record. */
					goto zlib_done;
				}
				goto zlib_error;
			case Z_BUF_ERROR:
				if (!d_stream.avail_out) {
					break;
				}
				/* fall through */
			default:
				goto zlib_error;
			}
		} else {
			/* Non-leaf nodes should not have any externally
			stored columns. */
			ut_ad(!rec_offs_any_extern(offsets));

			/* Decompress the data bytes, except node_ptr. */
			d_stream.avail_out = rec_offs_data_size(offsets)
					- REC_NODE_PTR_SIZE;

			err = inflate(&d_stream, Z_NO_FLUSH);
			switch (err) {
			case Z_OK:
				break;
			case Z_STREAM_END:
				if (!n_dense) {
					/* This was the last record. */
					goto zlib_done;
				}
				goto zlib_error;
			case Z_BUF_ERROR:
				if (!d_stream.avail_out) {
					break;
				}
				/* fall through */
			default:
				goto zlib_error;
			}

			d_stream.next_out += REC_NODE_PTR_SIZE;
		}

		ut_ad(d_stream.next_out == rec_get_end(rec, offsets));
	}

	/* We should have run out of data in the loop. */
zlib_error:
	inflateEnd(&d_stream);
	goto err_exit;

zlib_done:
	err = inflateEnd(&d_stream);
	ut_a(err == Z_OK);

	/* Clear the unused heap space on the uncompressed page. */
	memset(d_stream.next_out, 0, page_dir_get_nth_slot(page,
			page_dir_get_n_slots(page) - 1) - d_stream.next_out);

	/* The dense directory excludes the infimum and supremum records. */
	n_dense = page_dir_get_n_heap(page) - 2;

	page_zip->m_start = PAGE_DATA + d_stream.total_in;

	/* Apply the modification log. */
	{
		const byte*	mod_log_ptr;
		mod_log_ptr = page_zip_apply_log(
				page_zip->data + page_zip->m_start,
				d_stream.avail_in, page, recs, n_dense,
				heap_status, index, offsets);

		if (UNIV_UNLIKELY(!mod_log_ptr)) {
			goto err_exit;
		}
		page_zip->m_end = mod_log_ptr - page_zip->data;
	}

	page_zip_fields_free(index);
	mem_heap_free(heap);

	if (UNIV_UNLIKELY(!page_zip_set_extra_bytes(
				page_zip, page, info_bits))) {
err_exit:
		page_zip_fields_free(index);
		mem_heap_free(heap);
		return(FALSE);
	}

	/* Copy the uncompressed fields. */

	storage = page_zip->data + page_zip->size
			- (n_dense + 1 + page_zip_get_n_relocated(page_zip))
			* PAGE_ZIP_DIR_SLOT_SIZE;
	externs = storage - n_dense * (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
	page_zip->n_blobs = 0;
	recsc = recs;

	while (n_dense--) {
		rec_t*	rec	= *recsc++;

		/* Read the offsets. The status bits are needed here. */
		offsets = rec_get_offsets(rec, index, offsets,
					ULINT_UNDEFINED, &heap);

		if (page_is_leaf(page)) {
			ulint	i;
			ulint	len;
			byte*	dst;

			/* Check if there are any externally stored columns.
			For each externally stored column, restore the
			BTR_EXTERN_FIELD_REF separately._*/

			for (i = 0; i < rec_offs_n_fields(offsets); i++) {
				if (rec_offs_nth_extern(offsets, i)) {
					dst = rec_get_nth_field(
							rec, offsets, i, &len);
					ut_ad(len > BTR_EXTERN_FIELD_REF_SIZE);
					dst += len - BTR_EXTERN_FIELD_REF_SIZE;

					externs -= BTR_EXTERN_FIELD_REF_SIZE;

					/* Copy the BLOB pointer */
					memcpy(dst, externs,
						BTR_EXTERN_FIELD_REF_SIZE);
				}
			}

			if (trx_id_col != ULINT_UNDEFINED) {
				dst = rec_get_nth_field(rec, offsets,
						trx_id_col, &len);
				ut_ad(len >= DATA_TRX_ID_LEN
					+ DATA_ROLL_PTR_LEN);

				memcpy(dst, storage
					- (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
					* (rec_get_heap_no_new(rec) - 1),
					DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
			}
		} else {
			/* Non-leaf nodes should not have any externally
			stored columns. */
			ut_ad(!rec_offs_any_extern(offsets));

			memcpy(rec_get_end(rec, offsets) - REC_NODE_PTR_SIZE,
					storage - REC_NODE_PTR_SIZE
					* (rec_get_heap_no_new(rec) - 1),
					REC_NODE_PTR_SIZE);
		}
	}

	ut_a(page_is_comp(page));
	ut_ad(page_simple_validate_new(page));

	if (UNIV_LIKELY_NULL(mtr)) {
		mlog_open_and_write_index(mtr, page, index,
					MLOG_ZIP_DECOMPRESS, 0);
	}

	return(TRUE);
}

#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
/**************************************************************************
Check that the compressed and decompressed pages match. */

ibool
page_zip_validate(
/*==============*/
	const page_zip_des_t*	page_zip,/* in: compressed page */
	const page_t*		page)	/* in: uncompressed page */
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
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */

/**************************************************************************
Write an entire record on the compressed page.  The data must already
have been written to the uncompressed page. */

void
page_zip_write_rec(
/*===============*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: record being written */
	const ulint*	offsets)/* in: rec_get_offsets(rec, index) */
{
	page_t*	page;
	byte*	data;
	byte*	storage;

	ut_ad(buf_block_get_page_zip(buf_block_align((byte*)rec)) == page_zip);
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip->size > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(rec_offs_comp(offsets));
	ut_ad(rec_offs_validate((rec_t*) rec, NULL, offsets));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(!memcmp(ut_align_down((byte*) rec, UNIV_PAGE_SIZE),
		page_zip->data, PAGE_DATA));

	page = ut_align_down((rec_t*) rec, UNIV_PAGE_SIZE);

	ut_ad(rec_get_start((rec_t*) rec, offsets) >= page + PAGE_ZIP_START);
	ut_ad(rec_get_end((rec_t*) rec, offsets) <= page + UNIV_PAGE_SIZE
			- PAGE_DIR - PAGE_DIR_SLOT_SIZE
			* page_dir_get_n_slots(page));

	/* Append to the modification log. */
	data = page_zip->data + page_zip->m_end;
	ut_ad(!mach_read_from_2(data));

	{
		/* Identify the record by writing its start address.  0 is
		reserved to indicate the end of the modification log. */
		const byte*	start	= rec_get_start((rec_t*) rec, offsets);
		const byte*	b	= rec - REC_N_NEW_EXTRA_BYTES;

		mach_write_to_2(data, ut_align_offset(start, UNIV_PAGE_SIZE));
		/* Write the extra bytes backwards, so that
		rec_offs_extra_size() can be easily computed in
		page_zip_apply_log() by invoking
		rec_get_offsets_reverse(). */

		while (b != start) {
			*data++ = *--b;
		}
	}

	/* Write the data bytes.  Store the uncompressed bytes separately. */
	storage = page_zip->data + page_zip->size
			- (page_dir_get_n_heap(page) - 1
			+ page_zip_get_n_relocated(page_zip))
			* PAGE_ZIP_DIR_SLOT_SIZE;

	if (page_is_leaf(page)) {
		ulint		i;
		ulint		len;
		const byte*	start = rec;

		/* Check if there are any externally stored columns.
		For each externally stored column, store the
		BTR_EXTERN_FIELD_REF separately._*/

		for (i = 0; i < rec_offs_n_fields(offsets); i++) {
			if (rec_offs_nth_extern(offsets, i)) {
				ulint		len;
				const byte*	src = rec_get_nth_field(
					(rec_t*) rec, offsets, i, &len);
				ut_ad(len > BTR_EXTERN_FIELD_REF_SIZE);
				src += len - BTR_EXTERN_FIELD_REF_SIZE;

				memcpy(data, start, src - start);
				data += src - start;
				start = src;

				/* TODO: copy the BLOB pointer to
				the appropriate place in the
				uncompressed BLOB pointer array */
			}
		}

		/* Log the last bytes of the record.
		Skip roll_ptr and trx_id. */
		len = rec_get_end((rec_t*) rec, offsets)
			- (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN) - start;

		memcpy(data, start, len);
		data += len;
		start += len;

		/* Copy roll_ptr and trx_id to the uncompressed area. */
		memcpy(storage - (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
				* (rec_get_heap_no_new((rec_t*) rec) - 2),
				start,
				DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
		ut_a(data < storage
				- (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
				* (page_dir_get_n_heap(page) - 2)
				- page_zip->n_blobs
				* BTR_EXTERN_FIELD_REF_SIZE
				- 2 /* for the modification log terminator */);
	} else {
		/* This is a node pointer page. */
		ulint	len;

		/* Non-leaf nodes should not have any externally
		stored columns. */
		ut_ad(!rec_offs_any_extern(offsets));

		/* Copy the data bytes, except node_ptr. */
		len = rec_offs_data_size(offsets) - REC_NODE_PTR_SIZE;
		memcpy(data, rec, len);
		data += len;

		/* Copy the node pointer to the uncompressed area. */
		memcpy(storage - REC_NODE_PTR_SIZE
				* (rec_get_heap_no_new((rec_t*) rec) - 2),
				rec + len,
				REC_NODE_PTR_SIZE);
		ut_a(data < storage
				- REC_NODE_PTR_SIZE
				* (page_dir_get_n_heap(page) - 2)
				- 2 /* for the modification log terminator */);
	}

	page_zip->m_end = data - page_zip->data;
	ut_a(!mach_read_from_2(data));

	/* TODO: write a redo log record (MLOG_ZIP_WRITE_REC),
	or is it at all needed? */
}

/**************************************************************************
Write the BLOB pointer of a record on the leaf page of a clustered index.
The information must already have been updated on the uncompressed page. */

void
page_zip_write_blob_ptr(
/*====================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in/out: record whose data is being
				written */
	dict_index_t*	index,	/* in: index of the page */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	ulint		n,	/* in: column index */
	mtr_t*		mtr)	/* in: mini-transaction handle,
				or NULL if no logging is needed */
{
	byte*	field;
	byte*	storage;
	page_t*	page	= buf_frame_align((byte*) rec);
	ulint	blob_no;
	ulint	next_offs;
	ulint	len;

	ut_ad(buf_block_get_page_zip(buf_block_align((byte*)rec)) == page_zip);
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip->size > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(rec_offs_comp(offsets));
	ut_ad(rec_offs_validate((rec_t*) rec, NULL, offsets));
	ut_ad(rec_offs_nth_extern(offsets, n));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(!memcmp(page, page_zip->data, PAGE_DATA));

	ut_ad(page_is_leaf(page));

	blob_no = 0;
	next_offs = rec_get_next_offs(page + PAGE_NEW_INFIMUM, TRUE);
	ut_a(next_offs > PAGE_NEW_SUPREMUM_END);

	do {
		rec_t*	r = page + next_offs;

		if (r == rec) {
			goto found;
		}

		blob_no += rec_get_n_extern_new(r, index, ULINT_UNDEFINED);

		next_offs = rec_get_next_offs(r, TRUE);
		ut_a(next_offs > 0);
	} while (next_offs != PAGE_NEW_SUPREMUM);

	ut_error;

found:
	blob_no += rec_get_n_extern_new(rec, index, n);
	ut_a(blob_no < page_zip->n_blobs);

	/* The heap number of the first user record is 2. */
	storage = page_zip->data + page_zip->size
			- (page_dir_get_n_heap(page) - 2)
			* PAGE_ZIP_DIR_SLOT_SIZE
			- (rec_get_heap_no_new((rec_t*) rec) - 2)
			* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
			- blob_no * BTR_EXTERN_FIELD_REF_SIZE;
	field = rec_get_nth_field((rec_t*) rec, offsets, n, &len);

	memcpy(storage, field + len - BTR_EXTERN_FIELD_REF_SIZE,
		BTR_EXTERN_FIELD_REF_SIZE);

	if (mtr) {
		mlog_write_initial_log_record(
				(rec_t*) rec, MLOG_ZIP_WRITE_BLOB_PTR, mtr);
		/* TODO: write n */
	}
}

/**************************************************************************
Write the node pointer of a record on a non-leaf compressed page. */

void
page_zip_write_node_ptr(
/*====================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	byte*		rec,	/* in/out: record */
	ulint		size,	/* in: data size of rec */
	ulint		ptr,	/* in: node pointer */
	mtr_t*		mtr)	/* in: mini-transaction, or NULL */
{
	byte*	field;
	byte*	storage;
	page_t*	page	= buf_frame_align(rec);

	ut_ad(buf_block_get_page_zip(buf_block_align(rec)) == page_zip);
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip->size > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(page_rec_is_comp(rec));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(!memcmp(page, page_zip->data, PAGE_DATA));

	ut_ad(!page_is_leaf(page));

	/* The heap number of the first user record is 2. */
	storage = page_zip->data + page_zip->size
			- (page_dir_get_n_heap(page) - 2)
			* PAGE_ZIP_DIR_SLOT_SIZE
			- (rec_get_heap_no_new(rec) - 2) * REC_NODE_PTR_SIZE;
	field = rec + size - REC_NODE_PTR_SIZE;

#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
	ut_a(!memcmp(storage, field, REC_NODE_PTR_SIZE));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
#if REC_NODE_PTR_SIZE != 4
# error "REC_NODE_PTR_SIZE != 4"
#endif
	mach_write_to_4(field, ptr);
	memcpy(storage, field, REC_NODE_PTR_SIZE);

	if (mtr) {
		mlog_write_initial_log_record(
				rec, MLOG_ZIP_WRITE_NODE_PTR, mtr);
	}
}

/**************************************************************************
Write the trx_id of a record on a B-tree leaf node page. */

void
page_zip_write_trx_id(
/*==================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	byte*		rec,	/* in/out: record */
	ulint		size,	/* in: data size of rec */
	dulint		trx_id,	/* in: transaction identifier */
	mtr_t*		mtr)	/* in: mini-transaction, or NULL */
{
	byte*	field;
	byte*	storage;
	page_t*	page	= ut_align_down(rec, UNIV_PAGE_SIZE);

	ut_ad(buf_block_get_page_zip(buf_block_align(rec)) == page_zip);
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip->size > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(page_rec_is_comp(rec));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(!memcmp(page, page_zip->data, PAGE_DATA));

	ut_ad(page_is_leaf(page));

	/* The heap number of the first user record is 2. */
	storage = page_zip->data + page_zip->size
			- (page_dir_get_n_heap(page) - 2)
			* PAGE_ZIP_DIR_SLOT_SIZE
			- (rec_get_heap_no_new(rec) - 2)
			* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
	field = rec + size
			- (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN);
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
	ut_a(!memcmp(storage, field, DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
#if DATA_TRX_ID_LEN != 6
# error "DATA_TRX_ID_LEN != 6"
#endif
	mach_write_to_6(field, trx_id);
	memcpy(storage, field, DATA_TRX_ID_LEN);

	if (mtr) {
		mlog_write_initial_log_record(
				rec, MLOG_ZIP_WRITE_TRX_ID, mtr);
	}
}

/**************************************************************************
Write the roll_ptr of a record on a B-tree leaf node page. */

void
page_zip_write_roll_ptr(
/*====================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	byte*		rec,	/* in/out: record */
	ulint		size,	/* in: data size of rec */
	dulint		roll_ptr,/* in: roll_ptr */
	mtr_t*		mtr)	/* in: mini-transaction, or NULL */
{
	byte*	field;
	byte*	storage;
	page_t*	page	= ut_align_down(rec, UNIV_PAGE_SIZE);

	ut_ad(buf_block_get_page_zip(buf_block_align(rec)) == page_zip);
	ut_ad(page_zip_simple_validate(page_zip));
	ut_ad(page_zip->size > PAGE_DATA + page_zip_dir_size(page_zip));
	ut_ad(page_rec_is_comp(rec));

	ut_ad(page_zip->m_start >= PAGE_DATA);
	ut_ad(!memcmp(page, page_zip->data, PAGE_DATA));

	ut_ad(page_is_leaf(page));

	/* The heap number of the first user record is 2. */
	storage = page_zip->data + page_zip->size
			- (page_dir_get_n_heap(page) - 2)
			* PAGE_ZIP_DIR_SLOT_SIZE
			- (rec_get_heap_no_new(rec) - 2)
			* (DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)
			+ DATA_TRX_ID_LEN;
	field = rec + size
			- DATA_ROLL_PTR_LEN;
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
	ut_a(!memcmp(storage, field, DATA_ROLL_PTR_LEN));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
#if DATA_ROLL_PTR_LEN != 7
# error "DATA_ROLL_PTR_LEN != 7"
#endif
	mach_write_to_7(field, roll_ptr);
	memcpy(storage, field, DATA_ROLL_PTR_LEN);

	if (mtr) {
		mlog_write_initial_log_record(
				rec, MLOG_ZIP_WRITE_TRX_ID, mtr);
	}
}

/**************************************************************************
Clear an area on the uncompressed and compressed page, if possible. */

void
page_zip_clear_rec(
/*===============*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	byte*		rec,	/* in: record to clear */
	dict_index_t*	index,	/* in: index of rec */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	mtr_t*		mtr)	/* in: mini-transaction */
{
#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
	ut_a(page_zip_validate(page_zip, ut_align_down(rec, UNIV_PAGE_SIZE)));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */
	ut_ad(rec_offs_validate(rec, index, offsets));

	if (page_zip_available(page_zip, rec_offs_size(offsets),
				page_is_leaf(page_zip->data), 0)) {
		memset(rec - rec_offs_extra_size(offsets), 0,
			rec_offs_extra_size(offsets) - REC_N_NEW_EXTRA_BYTES);
		memset(rec, 0, rec_offs_data_size(offsets));

		/* Log that the data was zeroed out. */
		page_zip_write_rec(page_zip, rec, offsets);
	} else {
		/* There is not enough space to log the clearing.
		Try to clear the block and to recompress the page. */

		byte*	buf = mem_alloc(rec_offs_size(offsets));
		memcpy(buf, rec - rec_offs_extra_size(offsets),
					rec_offs_size(offsets));

		memset(rec - rec_offs_extra_size(offsets), 0,
			rec_offs_extra_size(offsets) - REC_N_NEW_EXTRA_BYTES);
		memset(rec, 0, rec_offs_data_size(offsets));
		/* TODO: maybe log the memset()s? */

		if (UNIV_UNLIKELY(!page_zip_compress(page_zip,
					ut_align_down(rec, UNIV_PAGE_SIZE),
					index, mtr))) {
			/* Compression failed. Restore the block. */
			memcpy(rec - rec_offs_extra_size(offsets), buf,
				rec_offs_size(offsets));
		}

		mem_free(buf);
	}
}

/**************************************************************************
Populate the dense page directory on the compressed page
from the sparse directory on the uncompressed row_format=compact page. */
void
page_zip_dir_rewrite(
/*=================*/
	page_zip_des_t*	page_zip,/* out: dense directory on compressed page */
	const page_t*	page)	/* in: uncompressed page  */
{
	page_zip_dir_encode(page, page_zip, NULL);
}

/**************************************************************************
Write the "deleted" flag of a record on a compressed page.  The flag must
already have been written on the uncompressed page. */

void
page_zip_rec_set_deleted(
/*=====================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: record on the uncompressed page */
	ulint		flag)	/* in: the deleted flag (nonzero=TRUE) */
{
	byte*	slot = page_zip_dir_find(page_zip,
				ut_align_offset(rec, UNIV_PAGE_SIZE));
	ut_a(slot);
	if (flag) {
		*slot |= (PAGE_ZIP_DIR_SLOT_DEL >> 8);
	} else {
		*slot &= ~(PAGE_ZIP_DIR_SLOT_DEL >> 8);
	}
}

/**************************************************************************
Write the "owned" flag of a record on a compressed page.  The n_owned field
must already have been written on the uncompressed page. */

void
page_zip_rec_set_owned(
/*===================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: record on the uncompressed page */
	ulint		flag)	/* in: the owned flag (nonzero=TRUE) */
{
	byte*	slot = page_zip_dir_find(page_zip,
				ut_align_offset(rec, UNIV_PAGE_SIZE));
	ut_a(slot);
	if (flag) {
		*slot |= (PAGE_ZIP_DIR_SLOT_OWNED >> 8);
	} else {
		*slot &= ~(PAGE_ZIP_DIR_SLOT_OWNED >> 8);
	}
}


/**************************************************************************
Shift the dense page directory when a record is deleted. */

void
page_zip_dir_delete(
/*================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	const byte*	rec,	/* in: deleted record */
	const byte*	free)	/* in: previous start of the free list */
{
	byte*	slot_rec;
	byte*	slot_free;

	slot_rec = page_zip_dir_find(page_zip,
			ut_align_offset(rec, UNIV_PAGE_SIZE));
	slot_free = page_zip_dir_find_free(page_zip,
			ut_align_offset(free, UNIV_PAGE_SIZE));

	ut_a(slot_rec);

	if (UNIV_UNLIKELY(!slot_free)) {
		/* Make the last slot the start of the free list. */
		slot_free = page_zip->data + page_zip->size
				- PAGE_ZIP_DIR_SLOT_SIZE
				* page_dir_get_n_heap(page_zip->data);
	} else {
		ut_a(slot_free < slot_rec);
		/* Grow the free list by one slot by moving the start. */
		slot_free += PAGE_ZIP_DIR_SLOT_SIZE;
	}

	if (UNIV_LIKELY(slot_free < slot_rec)) {
		memmove(slot_free + PAGE_ZIP_DIR_SLOT_SIZE,
			slot_free,
			slot_rec - slot_free - PAGE_ZIP_DIR_SLOT_SIZE);
	}

	/* Write the entry for the deleted record.
	The "owned" and "deleted" flags will be cleared. */
	mach_write_to_2(slot_free, ut_align_offset(rec, UNIV_PAGE_SIZE));
}
