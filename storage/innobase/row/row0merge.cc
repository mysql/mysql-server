/*****************************************************************************

Copyright (c) 2005, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file row/row0merge.cc
New index creation routines using a merge sort

Created 12/4/2005 Jan Lindstrom
Completed by Sunny Bains and Marko Makela
*******************************************************/

#include "row0merge.h"
#include "row0ext.h"
#include "row0log.h"
#include "row0ins.h"
#include "row0sel.h"
#include "dict0crea.h"
#include "trx0purge.h"
#include "lock0lock.h"
#include "pars0pars.h"
#include "ut0sort.h"
#include "row0ftsort.h"
#include "row0import.h"
#include "handler0alter.h"
#include "ha_prototypes.h"

/* Ignore posix_fadvise() on those platforms where it does not exist */
#if defined __WIN__
# define posix_fadvise(fd, offset, len, advice) /* nothing */
#endif /* __WIN__ */

#ifdef UNIV_DEBUG
/** Set these in order ot enable debug printout. */
/* @{ */
/** Log each record read from temporary file. */
static ibool	row_merge_print_read;
/** Log each record write to temporary file. */
static ibool	row_merge_print_write;
/** Log each row_merge_blocks() call, merging two blocks of records to
a bigger one. */
static ibool	row_merge_print_block;
/** Log each block read from temporary file. */
static ibool	row_merge_print_block_read;
/** Log each block read from temporary file. */
static ibool	row_merge_print_block_write;
/* @} */
#endif /* UNIV_DEBUG */

/* Whether to disable file system cache */
UNIV_INTERN char	srv_disable_sort_file_cache;

#ifdef UNIV_DEBUG
/******************************************************//**
Display a merge tuple. */
static __attribute__((nonnull))
void
row_merge_tuple_print(
/*==================*/
	FILE*		f,	/*!< in: output stream */
	const mtuple_t*	entry,	/*!< in: tuple to print */
	ulint		n_fields)/*!< in: number of fields in the tuple */
{
	ulint	j;

	for (j = 0; j < n_fields; j++) {
		const dfield_t*	field = &entry->fields[j];

		if (dfield_is_null(field)) {
			fputs("\n NULL;", f);
		} else {
			ulint	field_len	= dfield_get_len(field);
			ulint	len		= ut_min(field_len, 20);
			if (dfield_is_ext(field)) {
				fputs("\nE", f);
			} else {
				fputs("\n ", f);
			}
			ut_print_buf(f, dfield_get_data(field), len);
			if (len != field_len) {
				fprintf(f, " (total %lu bytes)", field_len);
			}
		}
	}
	putc('\n', f);
}
#endif /* UNIV_DEBUG */

/******************************************************//**
Encode an index record. */
static __attribute__((nonnull))
void
row_merge_buf_encode(
/*=================*/
	byte**			b,		/*!< in/out: pointer to
						current end of output buffer */
	const dict_index_t*	index,		/*!< in: index */
	const mtuple_t*		entry,		/*!< in: index fields
						of the record to encode */
	ulint			n_fields)	/*!< in: number of fields
						in the entry */
{
	ulint	size;
	ulint	extra_size;

	size = rec_get_converted_size_comp_prefix(
		index, entry->fields, n_fields, &extra_size);
	ut_ad(size >= extra_size);
	ut_ad(extra_size >= REC_N_NEW_EXTRA_BYTES);

	extra_size -= REC_N_NEW_EXTRA_BYTES;
	size -= REC_N_NEW_EXTRA_BYTES;

	/* Encode extra_size + 1 */
	if (extra_size + 1 < 0x80) {
		*(*b)++ = (byte) (extra_size + 1);
	} else {
		ut_ad((extra_size + 1) < 0x8000);
		*(*b)++ = (byte) (0x80 | ((extra_size + 1) >> 8));
		*(*b)++ = (byte) (extra_size + 1);
	}

	rec_convert_dtuple_to_rec_comp(*b + extra_size, 0, index,
				       REC_STATUS_ORDINARY,
				       entry->fields, n_fields);

	*b += size;
}

/******************************************************//**
Allocate a sort buffer.
@return	own: sort buffer */
static __attribute__((malloc, nonnull))
row_merge_buf_t*
row_merge_buf_create_low(
/*=====================*/
	mem_heap_t*	heap,		/*!< in: heap where allocated */
	dict_index_t*	index,		/*!< in: secondary index */
	ulint		max_tuples,	/*!< in: maximum number of
					data tuples */
	ulint		buf_size)	/*!< in: size of the buffer,
					in bytes */
{
	row_merge_buf_t*	buf;

	ut_ad(max_tuples > 0);

	ut_ad(max_tuples <= srv_sort_buf_size);

	buf = static_cast<row_merge_buf_t*>(mem_heap_zalloc(heap, buf_size));
	buf->heap = heap;
	buf->index = index;
	buf->max_tuples = max_tuples;
	buf->tuples = static_cast<mtuple_t*>(
		ut_malloc(2 * max_tuples * sizeof *buf->tuples));
	buf->tmp_tuples = buf->tuples + max_tuples;

	return(buf);
}

/******************************************************//**
Allocate a sort buffer.
@return	own: sort buffer */
UNIV_INTERN
row_merge_buf_t*
row_merge_buf_create(
/*=================*/
	dict_index_t*	index)	/*!< in: secondary index */
{
	row_merge_buf_t*	buf;
	ulint			max_tuples;
	ulint			buf_size;
	mem_heap_t*		heap;

	max_tuples = srv_sort_buf_size
		/ ut_max(1, dict_index_get_min_size(index));

	buf_size = (sizeof *buf);

	heap = mem_heap_create(buf_size);

	buf = row_merge_buf_create_low(heap, index, max_tuples, buf_size);

	return(buf);
}

/******************************************************//**
Empty a sort buffer.
@return	sort buffer */
UNIV_INTERN
row_merge_buf_t*
row_merge_buf_empty(
/*================*/
	row_merge_buf_t*	buf)	/*!< in,own: sort buffer */
{
	ulint		buf_size	= sizeof *buf;
	ulint		max_tuples	= buf->max_tuples;
	mem_heap_t*	heap		= buf->heap;
	dict_index_t*	index		= buf->index;
	mtuple_t*	tuples		= buf->tuples;

	mem_heap_empty(heap);

	buf = static_cast<row_merge_buf_t*>(mem_heap_zalloc(heap, buf_size));
	buf->heap = heap;
	buf->index = index;
	buf->max_tuples = max_tuples;
	buf->tuples = tuples;
	buf->tmp_tuples = buf->tuples + max_tuples;

	return(buf);
}

/******************************************************//**
Deallocate a sort buffer. */
UNIV_INTERN
void
row_merge_buf_free(
/*===============*/
	row_merge_buf_t*	buf)	/*!< in,own: sort buffer to be freed */
{
	ut_free(buf->tuples);
	mem_heap_free(buf->heap);
}

/******************************************************//**
Insert a data tuple into a sort buffer.
@return	number of rows added, 0 if out of space */
static
ulint
row_merge_buf_add(
/*==============*/
	row_merge_buf_t*	buf,	/*!< in/out: sort buffer */
	dict_index_t*		fts_index,/*!< in: fts index to be created */
	const dict_table_t*	old_table,/*!< in: original table */
	fts_psort_t*		psort_info, /*!< in: parallel sort info */
	const dtuple_t*		row,	/*!< in: table row */
	const row_ext_t*	ext,	/*!< in: cache of externally stored
					column prefixes, or NULL */
	doc_id_t*		doc_id)	/*!< in/out: Doc ID if we are
					creating FTS index */
{
	ulint			i;
	const dict_index_t*	index;
	mtuple_t*		entry;
	dfield_t*		field;
	const dict_field_t*	ifield;
	ulint			n_fields;
	ulint			data_size;
	ulint			extra_size;
	ulint			bucket = 0;
	doc_id_t		write_doc_id;
	ulint			n_row_added = 0;

	if (buf->n_tuples >= buf->max_tuples) {
		return(0);
	}

	DBUG_EXECUTE_IF(
		"ib_row_merge_buf_add_two",
		if (buf->n_tuples >= 2) return(0););

	UNIV_PREFETCH_R(row->fields);

	/* If we are building FTS index, buf->index points to
	the 'fts_sort_idx', and real FTS index is stored in
	fts_index */
	index = (buf->index->type & DICT_FTS) ? fts_index : buf->index;

	n_fields = dict_index_get_n_fields(index);

	entry = &buf->tuples[buf->n_tuples];
	field = entry->fields = static_cast<dfield_t*>(
		mem_heap_alloc(buf->heap, n_fields * sizeof *entry->fields));

	data_size = 0;
	extra_size = UT_BITS_IN_BYTES(index->n_nullable);

	ifield = dict_index_get_nth_field(index, 0);

	for (i = 0; i < n_fields; i++, field++, ifield++) {
		ulint			len;
		const dict_col_t*	col;
		ulint			col_no;
		const dfield_t*		row_field;

		col = ifield->col;
		col_no = dict_col_get_no(col);

		/* Process the Doc ID column */
		if (*doc_id > 0
		    && col_no == index->table->fts->doc_col) {
			fts_write_doc_id((byte*) &write_doc_id, *doc_id);

			/* Note: field->data now points to a value on the
			stack: &write_doc_id after dfield_set_data(). Because
			there is only one doc_id per row, it shouldn't matter.
			We allocate a new buffer before we leave the function
			later below. */

			dfield_set_data(
				field, &write_doc_id, sizeof(write_doc_id));

			field->type.mtype = ifield->col->mtype;
			field->type.prtype = ifield->col->prtype;
			field->type.mbminmaxlen = DATA_MBMINMAXLEN(0, 0);
			field->type.len = ifield->col->len;
		} else {
			row_field = dtuple_get_nth_field(row, col_no);

			dfield_copy(field, row_field);

			/* Tokenize and process data for FTS */
			if (index->type & DICT_FTS) {
				fts_doc_item_t*	doc_item;
				byte*		value;

				if (dfield_is_null(field)) {
					n_row_added = 1;
					continue;
				}

				doc_item = static_cast<fts_doc_item_t*>(
					mem_heap_alloc(
						buf->heap,
						sizeof(fts_doc_item_t)));

				/* fetch Doc ID if it already exists
				in the row, and not supplied by the caller */
				if (*doc_id == 0) {
					const dfield_t*	doc_field;
					doc_field = dtuple_get_nth_field(
						row,
						index->table->fts->doc_col);
					*doc_id = (doc_id_t) mach_read_from_8(
						static_cast<byte*>(
						dfield_get_data(doc_field)));

					if (*doc_id == 0) {
						fprintf(stderr, "InnoDB FTS: "
							"User supplied Doc ID "
							"is zero. Record "
							"Skipped\n");
						return(0);
					}
				}

				value = static_cast<byte*>(
					ut_malloc(field->len));
				memcpy(value, field->data, field->len);
				field->data = value;

				doc_item->field = field;
				doc_item->doc_id = *doc_id;

				bucket = *doc_id % fts_sort_pll_degree;

				UT_LIST_ADD_LAST(
					doc_list,
					psort_info[bucket].fts_doc_list,
					doc_item);
				n_row_added = 1;
				continue;
			}
		}

		len = dfield_get_len(field);

		if (dfield_is_null(field)) {
			ut_ad(!(col->prtype & DATA_NOT_NULL));
			continue;
		} else if (!ext) {
		} else if (dict_index_is_clust(index)) {
			/* Flag externally stored fields. */
			const byte*	buf = row_ext_lookup(ext, col_no,
							     &len);
			if (UNIV_LIKELY_NULL(buf)) {
				ut_a(buf != field_ref_zero);
				if (i < dict_index_get_n_unique(index)) {
					dfield_set_data(field, buf, len);
				} else {
					dfield_set_ext(field);
					len = dfield_get_len(field);
				}
			}
		} else {
			const byte*	buf = row_ext_lookup(ext, col_no,
							     &len);
			if (UNIV_LIKELY_NULL(buf)) {
				ut_a(buf != field_ref_zero);
				dfield_set_data(field, buf, len);
			}
		}

		/* If a column prefix index, take only the prefix */

		if (ifield->prefix_len) {
			len = dtype_get_at_most_n_mbchars(
				col->prtype,
				col->mbminmaxlen,
				ifield->prefix_len,
				len,
				static_cast<char*>(dfield_get_data(field)));
			dfield_set_len(field, len);
		}

		ut_ad(len <= col->len || col->mtype == DATA_BLOB);

		if (ifield->fixed_len) {
			ut_ad(len == ifield->fixed_len);
			ut_ad(!dfield_is_ext(field));
		} else if (dfield_is_ext(field)) {
			extra_size += 2;
		} else if (len < 128
			   || (col->len < 256 && col->mtype != DATA_BLOB)) {
			extra_size++;
		} else {
			/* For variable-length columns, we look up the
			maximum length from the column itself.  If this
			is a prefix index column shorter than 256 bytes,
			this will waste one byte. */
			extra_size += 2;
		}
		data_size += len;
	}

	/* If this is FTS index, we already populated the sort buffer, return
	here */
	if (index->type & DICT_FTS) {
		return(n_row_added);
	}

#ifdef UNIV_DEBUG
	{
		ulint	size;
		ulint	extra;

		size = rec_get_converted_size_comp_prefix(
			index, entry->fields, n_fields, &extra);

		ut_ad(data_size + extra == size);
		ut_ad(extra_size + REC_N_NEW_EXTRA_BYTES == extra);
	}
#endif /* UNIV_DEBUG */

	/* Add to the total size of the record in row_merge_block_t
	the encoded length of extra_size and the extra bytes (extra_size).
	See row_merge_buf_write() for the variable-length encoding
	of extra_size. */
	data_size += (extra_size + 1) + ((extra_size + 1) >= 0x80);

	ut_ad(data_size < srv_sort_buf_size);

	/* Reserve one byte for the end marker of row_merge_block_t. */
	if (buf->total_size + data_size >= srv_sort_buf_size - 1) {
		return(0);
	}

	buf->total_size += data_size;
	buf->n_tuples++;
	n_row_added++;

	field = entry->fields;

	/* Copy the data fields. */

	do {
		dfield_dup(field++, buf->heap);
	} while (--n_fields);

	return(n_row_added);
}

/*************************************************************//**
Report a duplicate key. */
UNIV_INTERN
void
row_merge_dup_report(
/*=================*/
	row_merge_dup_t*	dup,	/*!< in/out: for reporting duplicates */
	const dfield_t*		entry)	/*!< in: duplicate index entry */
{
	if (!dup->n_dup++) {
		/* Only report the first duplicate record,
		but count all duplicate records. */
		innobase_fields_to_mysql(dup->table, dup->index, entry);
	}
}

/*************************************************************//**
Compare two tuples.
@return	1, 0, -1 if a is greater, equal, less, respectively, than b */
static __attribute__((warn_unused_result))
int
row_merge_tuple_cmp(
/*================*/
	ulint			n_uniq,	/*!< in: number of unique fields */
	ulint			n_field,/*!< in: number of fields */
	const mtuple_t&		a,	/*!< in: first tuple to be compared */
	const mtuple_t&		b,	/*!< in: second tuple to be compared */
	row_merge_dup_t*	dup)	/*!< in/out: for reporting duplicates,
					NULL if non-unique index */
{
	int		cmp;
	const dfield_t*	af	= a.fields;
	const dfield_t*	bf	= b.fields;
	ulint		n	= n_uniq;

	ut_ad(n_uniq > 0);
	ut_ad(n_uniq <= n_field);

	/* Compare the fields of the tuples until a difference is
	found or we run out of fields to compare.  If !cmp at the
	end, the tuples are equal. */
	do {
		cmp = cmp_dfield_dfield(af++, bf++);
	} while (!cmp && --n);

	if (cmp) {
		return(cmp);
	}

	if (dup) {
		/* Report a duplicate value error if the tuples are
		logically equal.  NULL columns are logically inequal,
		although they are equal in the sorting order.  Find
		out if any of the fields are NULL. */
		for (const dfield_t* df = a.fields; df != af; df++) {
			if (dfield_is_null(df)) {
				goto no_report;
			}
		}

		row_merge_dup_report(dup, a.fields);
	}

no_report:
	/* The n_uniq fields were equal, but we compare all fields so
	that we will get the same order as in the B-tree. */
	for (n = n_field - n_uniq + 1; --n; ) {
		cmp = cmp_dfield_dfield(af++, bf++);
		if (cmp) {
			return(cmp);
		}
	}

	/* This should never be reached. Internally, an index must
	never contain duplicate entries. */
	ut_ad(0);
	return(cmp);
}

/** Wrapper for row_merge_tuple_sort() to inject some more context to
UT_SORT_FUNCTION_BODY().
@param tuples	array of tuples that being sorted
@param aux	work area, same size as tuples[]
@param low	lower bound of the sorting area, inclusive
@param high	upper bound of the sorting area, inclusive */
#define row_merge_tuple_sort_ctx(tuples, aux, low, high)		\
	row_merge_tuple_sort(n_uniq, n_field, dup, tuples, aux, low, high)
/** Wrapper for row_merge_tuple_cmp() to inject some more context to
UT_SORT_FUNCTION_BODY().
@param a	first tuple to be compared
@param b	second tuple to be compared
@return	1, 0, -1 if a is greater, equal, less, respectively, than b */
#define row_merge_tuple_cmp_ctx(a,b)			\
	row_merge_tuple_cmp(n_uniq, n_field, a, b, dup)

/**********************************************************************//**
Merge sort the tuple buffer in main memory. */
static __attribute__((nonnull(4,5)))
void
row_merge_tuple_sort(
/*=================*/
	ulint			n_uniq,	/*!< in: number of unique fields */
	ulint			n_field,/*!< in: number of fields */
	row_merge_dup_t*	dup,	/*!< in/out: reporter of duplicates
					(NULL if non-unique index) */
	mtuple_t*		tuples,	/*!< in/out: tuples */
	mtuple_t*		aux,	/*!< in/out: work area */
	ulint			low,	/*!< in: lower bound of the
					sorting area, inclusive */
	ulint			high)	/*!< in: upper bound of the
					sorting area, exclusive */
{
	ut_ad(n_field > 0);
	ut_ad(n_uniq <= n_field);

	UT_SORT_FUNCTION_BODY(row_merge_tuple_sort_ctx,
			      tuples, aux, low, high, row_merge_tuple_cmp_ctx);
}

/******************************************************//**
Sort a buffer. */
UNIV_INTERN
void
row_merge_buf_sort(
/*===============*/
	row_merge_buf_t*	buf,	/*!< in/out: sort buffer */
	row_merge_dup_t*	dup)	/*!< in/out: reporter of duplicates
					(NULL if non-unique index) */
{
	row_merge_tuple_sort(dict_index_get_n_unique(buf->index),
			     dict_index_get_n_fields(buf->index),
			     dup,
			     buf->tuples, buf->tmp_tuples, 0, buf->n_tuples);
}

/******************************************************//**
Write a buffer to a block. */
UNIV_INTERN
void
row_merge_buf_write(
/*================*/
	const row_merge_buf_t*	buf,	/*!< in: sorted buffer */
	const merge_file_t*	of UNIV_UNUSED,
					/*!< in: output file */
	row_merge_block_t*	block)	/*!< out: buffer for writing to file */
{
	const dict_index_t*	index	= buf->index;
	ulint			n_fields= dict_index_get_n_fields(index);
	byte*			b	= &block[0];

	for (ulint i = 0; i < buf->n_tuples; i++) {
		const mtuple_t*	entry	= &buf->tuples[i];

		row_merge_buf_encode(&b, index, entry, n_fields);
		ut_ad(b < &block[srv_sort_buf_size]);
#ifdef UNIV_DEBUG
		if (row_merge_print_write) {
			fprintf(stderr, "row_merge_buf_write %p,%d,%lu %lu",
				(void*) b, of->fd, (ulong) of->offset,
				(ulong) i);
			row_merge_tuple_print(stderr, entry, n_fields);
		}
#endif /* UNIV_DEBUG */
	}

	/* Write an "end-of-chunk" marker. */
	ut_a(b < &block[srv_sort_buf_size]);
	ut_a(b == &block[0] + buf->total_size);
	*b++ = 0;
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0xff, &block[srv_sort_buf_size] - b);
#endif /* UNIV_DEBUG_VALGRIND */
#ifdef UNIV_DEBUG
	if (row_merge_print_write) {
		fprintf(stderr, "row_merge_buf_write %p,%d,%lu EOF\n",
			(void*) b, of->fd, (ulong) of->offset);
	}
#endif /* UNIV_DEBUG */
}

/******************************************************//**
Create a memory heap and allocate space for row_merge_rec_offsets()
and mrec_buf_t[3].
@return	memory heap */
static
mem_heap_t*
row_merge_heap_create(
/*==================*/
	const dict_index_t*	index,		/*!< in: record descriptor */
	mrec_buf_t**		buf,		/*!< out: 3 buffers */
	ulint**			offsets1,	/*!< out: offsets */
	ulint**			offsets2)	/*!< out: offsets */
{
	ulint		i	= 1 + REC_OFFS_HEADER_SIZE
		+ dict_index_get_n_fields(index);
	mem_heap_t*	heap	= mem_heap_create(2 * i * sizeof **offsets1
						  + 3 * sizeof **buf);

	*buf = static_cast<mrec_buf_t*>(
		mem_heap_alloc(heap, 3 * sizeof **buf));
	*offsets1 = static_cast<ulint*>(
		mem_heap_alloc(heap, i * sizeof **offsets1));
	*offsets2 = static_cast<ulint*>(
		mem_heap_alloc(heap, i * sizeof **offsets2));

	(*offsets1)[0] = (*offsets2)[0] = i;
	(*offsets1)[1] = (*offsets2)[1] = dict_index_get_n_fields(index);

	return(heap);
}

/********************************************************************//**
Read a merge block from the file system.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
row_merge_read(
/*===========*/
	int			fd,	/*!< in: file descriptor */
	ulint			offset,	/*!< in: offset where to read
					in number of row_merge_block_t
					elements */
	row_merge_block_t*	buf)	/*!< out: data */
{
	os_offset_t	ofs = ((os_offset_t) offset) * srv_sort_buf_size;
	ibool		success;

#ifdef UNIV_DEBUG
	if (row_merge_print_block_read) {
		fprintf(stderr, "row_merge_read fd=%d ofs=%lu\n",
			fd, (ulong) offset);
	}
#endif /* UNIV_DEBUG */

#ifdef UNIV_DEBUG
	if (row_merge_print_block_read) {
		fprintf(stderr, "row_merge_read fd=%d ofs=%lu\n",
			fd, (ulong) offset);
	}
#endif /* UNIV_DEBUG */

	success = os_file_read_no_error_handling(OS_FILE_FROM_FD(fd), buf,
						 ofs, srv_sort_buf_size);
#ifdef POSIX_FADV_DONTNEED
	/* Each block is read exactly once.  Free up the file cache. */
	posix_fadvise(fd, ofs, srv_sort_buf_size, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

	if (UNIV_UNLIKELY(!success)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: failed to read merge block at "UINT64PF"\n",
			ofs);
	}

	return(UNIV_LIKELY(success));
}

/********************************************************************//**
Write a merge block to the file system.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
row_merge_write(
/*============*/
	int		fd,	/*!< in: file descriptor */
	ulint		offset,	/*!< in: offset where to write,
				in number of row_merge_block_t elements */
	const void*	buf)	/*!< in: data */
{
	size_t		buf_len = srv_sort_buf_size;
	os_offset_t	ofs = buf_len * (os_offset_t) offset;
	ibool		ret;

	ret = os_file_write("(merge)", OS_FILE_FROM_FD(fd), buf, ofs, buf_len);

#ifdef UNIV_DEBUG
	if (row_merge_print_block_write) {
		fprintf(stderr, "row_merge_write fd=%d ofs=%lu\n",
			fd, (ulong) offset);
	}
#endif /* UNIV_DEBUG */

#ifdef POSIX_FADV_DONTNEED
	/* The block will be needed on the next merge pass,
	but it can be evicted from the file cache meanwhile. */
	posix_fadvise(fd, ofs, buf_len, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

	return(UNIV_LIKELY(ret));
}

/********************************************************************//**
Read a merge record.
@return	pointer to next record, or NULL on I/O error or end of list */
UNIV_INTERN
const byte*
row_merge_read_rec(
/*===============*/
	row_merge_block_t*	block,	/*!< in/out: file buffer */
	mrec_buf_t*		buf,	/*!< in/out: secondary buffer */
	const byte*		b,	/*!< in: pointer to record */
	const dict_index_t*	index,	/*!< in: index of the record */
	int			fd,	/*!< in: file descriptor */
	ulint*			foffs,	/*!< in/out: file offset */
	const mrec_t**		mrec,	/*!< out: pointer to merge record,
					or NULL on end of list
					(non-NULL on I/O error) */
	ulint*			offsets)/*!< out: offsets of mrec */
{
	ulint	extra_size;
	ulint	data_size;
	ulint	avail_size;

	ut_ad(block);
	ut_ad(buf);
	ut_ad(b >= &block[0]);
	ut_ad(b < &block[srv_sort_buf_size]);
	ut_ad(index);
	ut_ad(foffs);
	ut_ad(mrec);
	ut_ad(offsets);

	ut_ad(*offsets == 1 + REC_OFFS_HEADER_SIZE
	      + dict_index_get_n_fields(index));

	extra_size = *b++;

	if (UNIV_UNLIKELY(!extra_size)) {
		/* End of list */
		*mrec = NULL;
#ifdef UNIV_DEBUG
		if (row_merge_print_read) {
			fprintf(stderr, "row_merge_read %p,%p,%d,%lu EOF\n",
				(const void*) b, (const void*) block,
				fd, (ulong) *foffs);
		}
#endif /* UNIV_DEBUG */
		return(NULL);
	}

	if (extra_size >= 0x80) {
		/* Read another byte of extra_size. */

		if (UNIV_UNLIKELY(b >= &block[srv_sort_buf_size])) {
			if (!row_merge_read(fd, ++(*foffs), block)) {
err_exit:
				/* Signal I/O error. */
				*mrec = b;
				return(NULL);
			}

			/* Wrap around to the beginning of the buffer. */
			b = &block[0];
		}

		extra_size = (extra_size & 0x7f) << 8;
		extra_size |= *b++;
	}

	/* Normalize extra_size.  Above, value 0 signals "end of list". */
	extra_size--;

	/* Read the extra bytes. */

	if (UNIV_UNLIKELY(b + extra_size >= &block[srv_sort_buf_size])) {
		/* The record spans two blocks.  Copy the entire record
		to the auxiliary buffer and handle this as a special
		case. */

		avail_size = &block[srv_sort_buf_size] - b;

		memcpy(*buf, b, avail_size);

		if (!row_merge_read(fd, ++(*foffs), block)) {

			goto err_exit;
		}

		/* Wrap around to the beginning of the buffer. */
		b = &block[0];

		/* Copy the record. */
		memcpy(*buf + avail_size, b, extra_size - avail_size);
		b += extra_size - avail_size;

		*mrec = *buf + extra_size;

		rec_init_offsets_comp_ordinary(*mrec, 0, index, offsets);

		data_size = rec_offs_data_size(offsets);

		/* These overflows should be impossible given that
		records are much smaller than either buffer, and
		the record starts near the beginning of each buffer. */
		ut_a(extra_size + data_size < sizeof *buf);
		ut_a(b + data_size < &block[srv_sort_buf_size]);

		/* Copy the data bytes. */
		memcpy(*buf + extra_size, b, data_size);
		b += data_size;

		goto func_exit;
	}

	*mrec = b + extra_size;

	rec_init_offsets_comp_ordinary(*mrec, 0, index, offsets);

	data_size = rec_offs_data_size(offsets);
	ut_ad(extra_size + data_size < sizeof *buf);

	b += extra_size + data_size;

	if (UNIV_LIKELY(b < &block[srv_sort_buf_size])) {
		/* The record fits entirely in the block.
		This is the normal case. */
		goto func_exit;
	}

	/* The record spans two blocks.  Copy it to buf. */

	b -= extra_size + data_size;
	avail_size = &block[srv_sort_buf_size] - b;
	memcpy(*buf, b, avail_size);
	*mrec = *buf + extra_size;
#ifdef UNIV_DEBUG
	/* We cannot invoke rec_offs_make_valid() here, because there
	are no REC_N_NEW_EXTRA_BYTES between extra_size and data_size.
	Similarly, rec_offs_validate() would fail, because it invokes
	rec_get_status(). */
	offsets[2] = (ulint) *mrec;
	offsets[3] = (ulint) index;
#endif /* UNIV_DEBUG */

	if (!row_merge_read(fd, ++(*foffs), block)) {

		goto err_exit;
	}

	/* Wrap around to the beginning of the buffer. */
	b = &block[0];

	/* Copy the rest of the record. */
	memcpy(*buf + avail_size, b, extra_size + data_size - avail_size);
	b += extra_size + data_size - avail_size;

func_exit:
#ifdef UNIV_DEBUG
	if (row_merge_print_read) {
		fprintf(stderr, "row_merge_read %p,%p,%d,%lu ",
			(const void*) b, (const void*) block,
			fd, (ulong) *foffs);
		rec_print_comp(stderr, *mrec, offsets);
		putc('\n', stderr);
	}
#endif /* UNIV_DEBUG */

	return(b);
}

/********************************************************************//**
Write a merge record. */
static
void
row_merge_write_rec_low(
/*====================*/
	byte*		b,	/*!< out: buffer */
	ulint		e,	/*!< in: encoded extra_size */
#ifdef UNIV_DEBUG
	ulint		size,	/*!< in: total size to write */
	int		fd,	/*!< in: file descriptor */
	ulint		foffs,	/*!< in: file offset */
#endif /* UNIV_DEBUG */
	const mrec_t*	mrec,	/*!< in: record to write */
	const ulint*	offsets)/*!< in: offsets of mrec */
#ifndef UNIV_DEBUG
# define row_merge_write_rec_low(b, e, size, fd, foffs, mrec, offsets)	\
	row_merge_write_rec_low(b, e, mrec, offsets)
#endif /* !UNIV_DEBUG */
{
#ifdef UNIV_DEBUG
	const byte* const end = b + size;
	ut_ad(e == rec_offs_extra_size(offsets) + 1);

	if (row_merge_print_write) {
		fprintf(stderr, "row_merge_write %p,%d,%lu ",
			(void*) b, fd, (ulong) foffs);
		rec_print_comp(stderr, mrec, offsets);
		putc('\n', stderr);
	}
#endif /* UNIV_DEBUG */

	if (e < 0x80) {
		*b++ = (byte) e;
	} else {
		*b++ = (byte) (0x80 | (e >> 8));
		*b++ = (byte) e;
	}

	memcpy(b, mrec - rec_offs_extra_size(offsets), rec_offs_size(offsets));
	ut_ad(b + rec_offs_size(offsets) == end);
}

/********************************************************************//**
Write a merge record.
@return	pointer to end of block, or NULL on error */
static
byte*
row_merge_write_rec(
/*================*/
	row_merge_block_t*	block,	/*!< in/out: file buffer */
	mrec_buf_t*		buf,	/*!< in/out: secondary buffer */
	byte*			b,	/*!< in: pointer to end of block */
	int			fd,	/*!< in: file descriptor */
	ulint*			foffs,	/*!< in/out: file offset */
	const mrec_t*		mrec,	/*!< in: record to write */
	const ulint*		offsets)/*!< in: offsets of mrec */
{
	ulint	extra_size;
	ulint	size;
	ulint	avail_size;

	ut_ad(block);
	ut_ad(buf);
	ut_ad(b >= &block[0]);
	ut_ad(b < &block[srv_sort_buf_size]);
	ut_ad(mrec);
	ut_ad(foffs);
	ut_ad(mrec < &block[0] || mrec > &block[srv_sort_buf_size]);
	ut_ad(mrec < buf[0] || mrec > buf[1]);

	/* Normalize extra_size.  Value 0 signals "end of list". */
	extra_size = rec_offs_extra_size(offsets) + 1;

	size = extra_size + (extra_size >= 0x80)
		+ rec_offs_data_size(offsets);

	if (UNIV_UNLIKELY(b + size >= &block[srv_sort_buf_size])) {
		/* The record spans two blocks.
		Copy it to the temporary buffer first. */
		avail_size = &block[srv_sort_buf_size] - b;

		row_merge_write_rec_low(buf[0],
					extra_size, size, fd, *foffs,
					mrec, offsets);

		/* Copy the head of the temporary buffer, write
		the completed block, and copy the tail of the
		record to the head of the new block. */
		memcpy(b, buf[0], avail_size);

		if (!row_merge_write(fd, (*foffs)++, block)) {
			return(NULL);
		}

		UNIV_MEM_INVALID(&block[0], srv_sort_buf_size);

		/* Copy the rest. */
		b = &block[0];
		memcpy(b, buf[0] + avail_size, size - avail_size);
		b += size - avail_size;
	} else {
		row_merge_write_rec_low(b, extra_size, size, fd, *foffs,
					mrec, offsets);
		b += size;
	}

	return(b);
}

/********************************************************************//**
Write an end-of-list marker.
@return	pointer to end of block, or NULL on error */
static
byte*
row_merge_write_eof(
/*================*/
	row_merge_block_t*	block,	/*!< in/out: file buffer */
	byte*			b,	/*!< in: pointer to end of block */
	int			fd,	/*!< in: file descriptor */
	ulint*			foffs)	/*!< in/out: file offset */
{
	ut_ad(block);
	ut_ad(b >= &block[0]);
	ut_ad(b < &block[srv_sort_buf_size]);
	ut_ad(foffs);
#ifdef UNIV_DEBUG
	if (row_merge_print_write) {
		fprintf(stderr, "row_merge_write %p,%p,%d,%lu EOF\n",
			(void*) b, (void*) block, fd, (ulong) *foffs);
	}
#endif /* UNIV_DEBUG */

	*b++ = 0;
	UNIV_MEM_ASSERT_RW(&block[0], b - &block[0]);
	UNIV_MEM_ASSERT_W(&block[0], srv_sort_buf_size);
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0xff, &block[srv_sort_buf_size] - b);
#endif /* UNIV_DEBUG_VALGRIND */

	if (!row_merge_write(fd, (*foffs)++, block)) {
		return(NULL);
	}

	UNIV_MEM_INVALID(&block[0], srv_sort_buf_size);
	return(&block[0]);
}

/********************************************************************//**
Reads clustered index of the table and create temporary files
containing the index entries for the indexes to be built.
@return	DB_SUCCESS or error */
static __attribute__((nonnull(1,2,3,4,6,9,10,16), warn_unused_result))
dberr_t
row_merge_read_clustered_index(
/*===========================*/
	trx_t*			trx,	/*!< in: transaction */
	struct TABLE*		table,	/*!< in/out: MySQL table object,
					for reporting erroneous records */
	const dict_table_t*	old_table,/*!< in: table where rows are
					read from */
	const dict_table_t*	new_table,/*!< in: table where indexes are
					created; identical to old_table
					unless creating a PRIMARY KEY */
	bool			online,	/*!< in: true if creating indexes
					online */
	dict_index_t**		index,	/*!< in: indexes to be created */
	dict_index_t*		fts_sort_idx,
					/*!< in: full-text index to be created,
					or NULL */
	fts_psort_t*		psort_info,
					/*!< in: parallel sort info for
					fts_sort_idx creation, or NULL */
	merge_file_t*		files,	/*!< in: temporary files */
	const ulint*		key_numbers,
					/*!< in: MySQL key numbers to create */
	ulint			n_index,/*!< in: number of indexes to create */
	const dtuple_t*		add_cols,
					/*!< in: default values of
					added columns, or NULL */
	const ulint*		col_map,/*!< in: mapping of old column
					numbers to new ones, or NULL
					if old_table == new_table */
	ulint			add_autoinc,
					/*!< in: number of added
					AUTO_INCREMENT column, or
					ULINT_UNDEFINED if none is added */
	ib_sequence_t&		sequence,/*!< in/out: autoinc sequence */
	row_merge_block_t*	block)	/*!< in/out: file buffer */
{
	dict_index_t*		clust_index;	/* Clustered index */
	mem_heap_t*		row_heap;	/* Heap memory to create
						clustered index tuples */
	row_merge_buf_t**	merge_buf;	/* Temporary list for records*/
	btr_pcur_t		pcur;		/* Cursor on the clustered
						index */
	mtr_t			mtr;		/* Mini transaction */
	dberr_t			err = DB_SUCCESS;/* Return code */
	ulint			n_nonnull = 0;	/* number of columns
						changed to NOT NULL */
	ulint*			nonnull = NULL;	/* NOT NULL columns */
	dict_index_t*		fts_index = NULL;/* FTS index */
	doc_id_t		doc_id = 0;
	doc_id_t		max_doc_id = 0;
	ibool			add_doc_id = FALSE;
	os_event_t		fts_parallel_sort_event = NULL;
	ibool			fts_pll_sort = FALSE;
	ib_int64_t		sig_count = 0;

	ut_ad((old_table == new_table) == !col_map);
	ut_ad(!add_cols || col_map);

	trx->op_info = "reading clustered index";

#ifdef FTS_INTERNAL_DIAG_PRINT
	DEBUG_FTS_SORT_PRINT("FTS_SORT: Start Create Index\n");
#endif

	/* Create and initialize memory for record buffers */

	merge_buf = static_cast<row_merge_buf_t**>(
		mem_alloc(n_index * sizeof *merge_buf));

	for (ulint i = 0; i < n_index; i++) {
		if (index[i]->type & DICT_FTS) {

			/* We are building a FT index, make sure
			we have the temporary 'fts_sort_idx' */
			ut_a(fts_sort_idx);

			fts_index = index[i];

			merge_buf[i] = row_merge_buf_create(fts_sort_idx);

			add_doc_id = DICT_TF2_FLAG_IS_SET(
				new_table, DICT_TF2_FTS_ADD_DOC_ID);

			/* If Doc ID does not exist in the table itself,
			fetch the first FTS Doc ID */
			if (add_doc_id) {
				fts_get_next_doc_id(
					(dict_table_t*) new_table,
					&doc_id);
				ut_ad(doc_id > 0);
			}

			fts_pll_sort = TRUE;
			row_fts_start_psort(psort_info);
			fts_parallel_sort_event =
				 psort_info[0].psort_common->sort_event;
		} else {
			merge_buf[i] = row_merge_buf_create(index[i]);
		}
	}

	mtr_start(&mtr);

	/* Find the clustered index and create a persistent cursor
	based on that. */

	clust_index = dict_table_get_first_index(old_table);

	btr_pcur_open_at_index_side(
		true, clust_index, BTR_SEARCH_LEAF, &pcur, true, 0, &mtr);

	if (old_table != new_table) {
		/* The table is being rebuilt.  Identify the columns
		that were flagged NOT NULL in the new table, so that
		we can quickly check that the records in the old table
		do not violate the added NOT NULL constraints. */

		nonnull = static_cast<ulint*>(
			mem_alloc(dict_table_get_n_cols(new_table)
				  * sizeof *nonnull));

		for (ulint i = 0; i < dict_table_get_n_cols(old_table); i++) {
			if (dict_table_get_nth_col(old_table, i)->prtype
			    & DATA_NOT_NULL) {
				continue;
			}

			const ulint j = col_map[i];

			if (j == ULINT_UNDEFINED) {
				/* The column was dropped. */
				continue;
			}

			if (dict_table_get_nth_col(new_table, j)->prtype
			    & DATA_NOT_NULL) {
				nonnull[n_nonnull++] = j;
			}
		}

		if (!n_nonnull) {
			mem_free(nonnull);
			nonnull = NULL;
		}
	}

	row_heap = mem_heap_create(sizeof(mrec_buf_t));

	/* Scan the clustered index. */
	for (;;) {
		const rec_t*	rec;
		ulint*		offsets;
		const dtuple_t*	row;
		row_ext_t*	ext;
		page_cur_t*	cur	= btr_pcur_get_page_cur(&pcur);

		page_cur_move_to_next(cur);

		if (page_cur_is_after_last(cur)) {
			if (UNIV_UNLIKELY(trx_is_interrupted(trx))) {
				err = DB_INTERRUPTED;
				trx->error_key_num = 0;
				goto func_exit;
			}

			if (online && old_table != new_table) {
				err = row_log_table_get_error(clust_index);
				if (err != DB_SUCCESS) {
					trx->error_key_num = 0;
					goto func_exit;
				}
			}
#ifdef DBUG_OFF
# define dbug_run_purge	false
#else /* DBUG_OFF */
			bool	dbug_run_purge = false;
#endif /* DBUG_OFF */
			DBUG_EXECUTE_IF(
				"ib_purge_on_create_index_page_switch",
				dbug_run_purge = true;);

			if (dbug_run_purge
			    || rw_lock_get_waiters(
				    dict_index_get_lock(clust_index))) {
				/* There are waiters on the clustered
				index tree lock, likely the purge
				thread. Store and restore the cursor
				position, and yield so that scanning a
				large table will not starve other
				threads. */

				/* Store the cursor position on the last user
				record on the page. */
				btr_pcur_move_to_prev_on_page(&pcur);
				/* Leaf pages must never be empty, unless
				this is the only page in the index tree. */
				ut_ad(btr_pcur_is_on_user_rec(&pcur)
				      || buf_block_get_page_no(
					      btr_pcur_get_block(&pcur))
				      == clust_index->page);

				btr_pcur_store_position(&pcur, &mtr);
				mtr_commit(&mtr);

				if (dbug_run_purge) {
					/* This is for testing
					purposes only (see
					DBUG_EXECUTE_IF above).  We
					signal the purge thread and
					hope that the purge batch will
					complete before we execute
					btr_pcur_restore_position(). */
					trx_purge_run();
					os_thread_sleep(1000000);
				}

				/* Give the waiters a chance to proceed. */
				os_thread_yield();

				mtr_start(&mtr);
				/* Restore position on the record, or its
				predecessor if the record was purged
				meanwhile. */
				btr_pcur_restore_position(
					BTR_SEARCH_LEAF, &pcur, &mtr);
				/* Move to the successor of the
				original record. */
				if (!btr_pcur_move_to_next_user_rec(
					    &pcur, &mtr)) {
end_of_index:
					row = NULL;
					mtr_commit(&mtr);
					mem_heap_free(row_heap);
					if (nonnull) {
						mem_free(nonnull);
					}
					goto write_buffers;
				}
			} else {
				ulint		next_page_no;
				buf_block_t*	block;

				next_page_no = btr_page_get_next(
					page_cur_get_page(cur), &mtr);

				if (next_page_no == FIL_NULL) {
					goto end_of_index;
				}

				block = page_cur_get_block(cur);
				block = btr_block_get(
					buf_block_get_space(block),
					buf_block_get_zip_size(block),
					next_page_no, BTR_SEARCH_LEAF,
					clust_index, &mtr);

				btr_leaf_page_release(page_cur_get_block(cur),
						      BTR_SEARCH_LEAF, &mtr);
				page_cur_set_before_first(block, cur);
				page_cur_move_to_next(cur);

				ut_ad(!page_cur_is_after_last(cur));
			}
		}

		rec = page_cur_get_rec(cur);

		offsets = rec_get_offsets(rec, clust_index, NULL,
					  ULINT_UNDEFINED, &row_heap);

		if (online && new_table != old_table) {
			/* When rebuilding the table online, perform a
			REPEATABLE READ, so that row_log_table_apply()
			will not see a newer state of the table when
			applying the log.  This is mainly to prevent
			false duplicate key errors, because the log
			will identify records by the PRIMARY KEY. */
			ut_ad(trx->read_view);

			if (!read_view_sees_trx_id(
				    trx->read_view,
				    row_get_rec_trx_id(
					    rec, clust_index, offsets))) {
				rec_t*	old_vers;

				row_vers_build_for_consistent_read(
					rec, &mtr, clust_index, &offsets,
					trx->read_view, &row_heap,
					row_heap, &old_vers);

				rec = old_vers;

				if (!rec) {
					continue;
				}
			}

			if (rec_get_deleted_flag(
				    rec,
				    dict_table_is_comp(old_table))) {
				/* This record was deleted in the latest
				committed version, or it was deleted and
				then reinserted-by-update before purge
				kicked in. Skip it. */
				continue;
			}

			ut_ad(!rec_offs_any_null_extern(rec, offsets));
		} else if (rec_get_deleted_flag(
				   rec, dict_table_is_comp(old_table))) {
			/* Skip delete-marked records.

			Skipping delete-marked records will make the
			created indexes unuseable for transactions
			whose read views were created before the index
			creation completed, but preserving the history
			would make it tricky to detect duplicate
			keys. */
			continue;
		} else if (UNIV_LIKELY_NULL(rec_offs_any_null_extern(
						    rec, offsets))) {
			/* This is essentially a READ UNCOMMITTED to
			fetch the most recent version of the record. */
#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
			trx_id_t	trx_id;
			ulint		trx_id_offset;

			/* It is possible that the record was
			just inserted and the off-page columns
			have not yet been written. We will
			ignore the record if this is the case,
			because it should be covered by the
			index->info.online log in that case. */

			trx_id_offset = clust_index->trx_id_offset;
			if (!trx_id_offset) {
				trx_id_offset = row_get_trx_id_offset(
					clust_index, offsets);
			}

			trx_id = trx_read_trx_id(rec + trx_id_offset);
			ut_a(trx_rw_is_active(trx_id, NULL));
			ut_a(trx_undo_trx_id_is_insert(rec + trx_id_offset));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

			/* When !online, we are holding an X-lock on
			old_table, preventing any inserts. */
			ut_ad(online);
			continue;
		}

		/* Build a row based on the clustered index. */

		row = row_build(ROW_COPY_POINTERS, clust_index,
				rec, offsets, new_table,
				add_cols, col_map, &ext, row_heap);
		ut_ad(row);

		for (ulint i = 0; i < n_nonnull; i++) {
			const dfield_t*	field	= &row->fields[nonnull[i]];

			ut_ad(dfield_get_type(field)->prtype & DATA_NOT_NULL);

			if (dfield_is_null(field)) {
				err = DB_INVALID_NULL;
				trx->error_key_num = 0;
				goto func_exit;
			}
		}

		/* Get the next Doc ID */
		if (add_doc_id) {
			doc_id++;
		} else {
			doc_id = 0;
		}

		if (add_autoinc != ULINT_UNDEFINED) {

			ut_ad(add_autoinc
			      < dict_table_get_n_user_cols(new_table));

			const dfield_t*	dfield;

			dfield = dtuple_get_nth_field(row, add_autoinc);
			if (dfield_is_null(dfield)) {
				goto write_buffers;
			}

			const dtype_t*  dtype = dfield_get_type(dfield);
			byte*	b = static_cast<byte*>(dfield_get_data(dfield));

			if (sequence.eof()) {
				err = DB_ERROR;
				trx->error_key_num = 0;

				ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
					ER_AUTOINC_READ_FAILED, "[NULL]");

				goto func_exit;
			}

			ulonglong	value = sequence++;

			switch (dtype_get_mtype(dtype)) {
			case DATA_INT: {
				ibool	usign;
				ulint	len = dfield_get_len(dfield);

				usign = dtype_get_prtype(dtype) & DATA_UNSIGNED;
				mach_write_ulonglong(b, value, len, usign);

				break;
				}

			case DATA_FLOAT:
				mach_float_write(
					b, static_cast<float>(value));
				break;

			case DATA_DOUBLE:
				mach_double_write(
					b, static_cast<double>(value));
				break;

			default:
				ut_ad(0);
			}
		}

write_buffers:
		/* Build all entries for all the indexes to be created
		in a single scan of the clustered index. */

		for (ulint i = 0; i < n_index; i++) {
			row_merge_buf_t*	buf	= merge_buf[i];
			merge_file_t*		file	= &files[i];
			ulint			rows_added = 0;

			if (UNIV_LIKELY
			    (row && (rows_added = row_merge_buf_add(
					buf, fts_index, old_table,
					psort_info, row, ext, &doc_id)))) {

				/* If we are creating FTS index,
				a single row can generate more
				records for tokenized word */
				file->n_rec += rows_added;
				if (doc_id > max_doc_id) {
					max_doc_id = doc_id;
				}

				continue;
			}

			if ((buf->index->type & DICT_FTS)
			    && (!row || !doc_id)) {
				continue;
			}

			/* The buffer must be sufficiently large
			to hold at least one record. It may only
			be empty when we reach the end of the
			clustered index. row_merge_buf_add()
			must not have been called in this loop. */
			ut_ad(buf->n_tuples || row == NULL);

			/* We have enough data tuples to form a block.
			Sort them and write to disk. */

			if (buf->n_tuples) {
				if (dict_index_is_unique(buf->index)) {
					row_merge_dup_t	dup = {
						buf->index, table, col_map, 0};

					row_merge_buf_sort(buf, &dup);

					if (dup.n_dup) {
						err = DB_DUPLICATE_KEY;
						trx->error_key_num
							= key_numbers[i];
						break;
					}
				} else {
					row_merge_buf_sort(buf, NULL);
				}
			} else if (online && new_table == old_table) {
				/* Note the newest transaction that
				modified this index when the scan was
				completed. We prevent older readers
				from accessing this index, to ensure
				read consistency. */

				trx_id_t	max_trx_id;

				ut_a(row == NULL);
				rw_lock_x_lock(
					dict_index_get_lock(buf->index));
				ut_a(dict_index_get_online_status(buf->index)
				     == ONLINE_INDEX_CREATION);

				max_trx_id = row_log_get_max_trx(buf->index);

				if (max_trx_id > buf->index->trx_id) {
					buf->index->trx_id = max_trx_id;
				}

				rw_lock_x_unlock(
					dict_index_get_lock(buf->index));
			}

			row_merge_buf_write(buf, file, block);

			if (!row_merge_write(file->fd, file->offset++,
					     block)) {
				err = DB_OUT_OF_FILE_SPACE;
				trx->error_key_num = i;
				break;
			}

			UNIV_MEM_INVALID(&block[0], srv_sort_buf_size);
			merge_buf[i] = row_merge_buf_empty(buf);

			if (UNIV_LIKELY(row != NULL)) {
				/* Try writing the record again, now
				that the buffer has been written out
				and emptied. */

				if (UNIV_UNLIKELY
				    (!(rows_added = row_merge_buf_add(
						buf, fts_index, old_table,
						psort_info, row, ext,
						&doc_id)))) {
					/* An empty buffer should have enough
					room for at least one record. */
					ut_error;
				}

				file->n_rec += rows_added;
			}
		}

		if (row == NULL) {
			goto all_done;
		}

		if (err != DB_SUCCESS) {
			goto func_exit;
		}

		mem_heap_empty(row_heap);
	}

func_exit:
	mtr_commit(&mtr);
	mem_heap_free(row_heap);

	if (nonnull) {
		mem_free(nonnull);
	}

all_done:
#ifdef FTS_INTERNAL_DIAG_PRINT
	DEBUG_FTS_SORT_PRINT("FTS_SORT: Complete Scan Table\n");
#endif
	if (fts_pll_sort) {
		for (ulint i = 0; i < fts_sort_pll_degree; i++) {
			psort_info[i].state = FTS_PARENT_COMPLETE;
		}
wait_again:
		os_event_wait_time_low(fts_parallel_sort_event,
				       1000000, sig_count);

		for (ulint i = 0; i < fts_sort_pll_degree; i++) {
			if (psort_info[i].child_status != FTS_CHILD_COMPLETE
			    && psort_info[i].child_status != FTS_CHILD_EXITING) {
				sig_count = os_event_reset(
					fts_parallel_sort_event);
				goto wait_again;
			}
		}
	}

#ifdef FTS_INTERNAL_DIAG_PRINT
	DEBUG_FTS_SORT_PRINT("FTS_SORT: Complete Tokenization\n");
#endif
	for (ulint i = 0; i < n_index; i++) {
		row_merge_buf_free(merge_buf[i]);
	}

	row_fts_free_pll_merge_buf(psort_info);

	mem_free(merge_buf);

	btr_pcur_close(&pcur);

	/* Update the next Doc ID we used. Table should be locked, so
	no concurrent DML */
	if (max_doc_id) {
		fts_update_next_doc_id(
			0, new_table, old_table->name, max_doc_id);
	}

	trx->op_info = "";

	return(err);
}

/** Write a record via buffer 2 and read the next record to buffer N.
@param N	number of the buffer (0 or 1)
@param INDEX	record descriptor
@param AT_END	statement to execute at end of input */
#define ROW_MERGE_WRITE_GET_NEXT(N, INDEX, AT_END)			\
	do {								\
		b2 = row_merge_write_rec(&block[2 * srv_sort_buf_size], \
					 &buf[2], b2,			\
					 of->fd, &of->offset,		\
					 mrec##N, offsets##N);		\
		if (UNIV_UNLIKELY(!b2 || ++of->n_rec > file->n_rec)) {	\
			goto corrupt;					\
		}							\
		b##N = row_merge_read_rec(&block[N * srv_sort_buf_size],\
					  &buf[N], b##N, INDEX,		\
					  file->fd, foffs##N,		\
					  &mrec##N, offsets##N);	\
		if (UNIV_UNLIKELY(!b##N)) {				\
			if (mrec##N) {					\
				goto corrupt;				\
			}						\
			AT_END;						\
		}							\
	} while (0)

/*************************************************************//**
Merge two blocks of records on disk and write a bigger block.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_merge_blocks(
/*=============*/
	const row_merge_dup_t*	dup,	/*!< in: descriptor of
					index being created */
	const merge_file_t*	file,	/*!< in: file containing
					index entries */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	ulint*			foffs0,	/*!< in/out: offset of first
					source list in the file */
	ulint*			foffs1,	/*!< in/out: offset of second
					source list in the file */
	merge_file_t*		of)	/*!< in/out: output file */
{
	mem_heap_t*	heap;	/*!< memory heap for offsets0, offsets1 */

	mrec_buf_t*	buf;	/*!< buffer for handling
				split mrec in block[] */
	const byte*	b0;	/*!< pointer to block[0] */
	const byte*	b1;	/*!< pointer to block[srv_sort_buf_size] */
	byte*		b2;	/*!< pointer to block[2 * srv_sort_buf_size] */
	const mrec_t*	mrec0;	/*!< merge rec, points to block[0] or buf[0] */
	const mrec_t*	mrec1;	/*!< merge rec, points to
				block[srv_sort_buf_size] or buf[1] */
	ulint*		offsets0;/* offsets of mrec0 */
	ulint*		offsets1;/* offsets of mrec1 */

#ifdef UNIV_DEBUG
	if (row_merge_print_block) {
		fprintf(stderr,
			"row_merge_blocks fd=%d ofs=%lu + fd=%d ofs=%lu"
			" = fd=%d ofs=%lu\n",
			file->fd, (ulong) *foffs0,
			file->fd, (ulong) *foffs1,
			of->fd, (ulong) of->offset);
	}
#endif /* UNIV_DEBUG */

	heap = row_merge_heap_create(dup->index, &buf, &offsets0, &offsets1);

	/* Write a record and read the next record.  Split the output
	file in two halves, which can be merged on the following pass. */

	if (!row_merge_read(file->fd, *foffs0, &block[0])
	    || !row_merge_read(file->fd, *foffs1, &block[srv_sort_buf_size])) {
corrupt:
		mem_heap_free(heap);
		return(DB_CORRUPTION);
	}

	b0 = &block[0];
	b1 = &block[srv_sort_buf_size];
	b2 = &block[2 * srv_sort_buf_size];

	b0 = row_merge_read_rec(
		&block[0], &buf[0], b0, dup->index,
		file->fd, foffs0, &mrec0, offsets0);
	b1 = row_merge_read_rec(
		&block[srv_sort_buf_size],
		&buf[srv_sort_buf_size], b1, dup->index,
		file->fd, foffs1, &mrec1, offsets1);
	if (UNIV_UNLIKELY(!b0 && mrec0)
	    || UNIV_UNLIKELY(!b1 && mrec1)) {

		goto corrupt;
	}

	while (mrec0 && mrec1) {
		switch (cmp_rec_rec_simple(
				mrec0, mrec1, offsets0, offsets1,
				dup->index, dup->table)) {
		case 0:
			mem_heap_free(heap);
			return(DB_DUPLICATE_KEY);
		case -1:
			ROW_MERGE_WRITE_GET_NEXT(0, dup->index, goto merged);
			break;
		case 1:
			ROW_MERGE_WRITE_GET_NEXT(1, dup->index, goto merged);
			break;
		default:
			ut_error;
		}
	}

merged:
	if (mrec0) {
		/* append all mrec0 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(0, dup->index, goto done0);
		}
	}
done0:
	if (mrec1) {
		/* append all mrec1 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(1, dup->index, goto done1);
		}
	}
done1:

	mem_heap_free(heap);
	b2 = row_merge_write_eof(&block[2 * srv_sort_buf_size],
				 b2, of->fd, &of->offset);
	return(b2 ? DB_SUCCESS : DB_CORRUPTION);
}

/*************************************************************//**
Copy a block of index entries.
@return	TRUE on success, FALSE on failure */
static __attribute__((nonnull, warn_unused_result))
ibool
row_merge_blocks_copy(
/*==================*/
	const dict_index_t*	index,	/*!< in: index being created */
	const merge_file_t*	file,	/*!< in: input file */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	ulint*			foffs0,	/*!< in/out: input file offset */
	merge_file_t*		of)	/*!< in/out: output file */
{
	mem_heap_t*	heap;	/*!< memory heap for offsets0, offsets1 */

	mrec_buf_t*	buf;	/*!< buffer for handling
				split mrec in block[] */
	const byte*	b0;	/*!< pointer to block[0] */
	byte*		b2;	/*!< pointer to block[2 * srv_sort_buf_size] */
	const mrec_t*	mrec0;	/*!< merge rec, points to block[0] */
	ulint*		offsets0;/* offsets of mrec0 */
	ulint*		offsets1;/* dummy offsets */

#ifdef UNIV_DEBUG
	if (row_merge_print_block) {
		fprintf(stderr,
			"row_merge_blocks_copy fd=%d ofs=%lu"
			" = fd=%d ofs=%lu\n",
			file->fd, (ulong) foffs0,
			of->fd, (ulong) of->offset);
	}
#endif /* UNIV_DEBUG */

	heap = row_merge_heap_create(index, &buf, &offsets0, &offsets1);

	/* Write a record and read the next record.  Split the output
	file in two halves, which can be merged on the following pass. */

	if (!row_merge_read(file->fd, *foffs0, &block[0])) {
corrupt:
		mem_heap_free(heap);
		return(FALSE);
	}

	b0 = &block[0];

	b2 = &block[2 * srv_sort_buf_size];

	b0 = row_merge_read_rec(&block[0], &buf[0], b0, index,
				file->fd, foffs0, &mrec0, offsets0);
	if (UNIV_UNLIKELY(!b0 && mrec0)) {

		goto corrupt;
	}

	if (mrec0) {
		/* append all mrec0 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(0, index, goto done0);
		}
	}
done0:

	/* The file offset points to the beginning of the last page
	that has been read.  Update it to point to the next block. */
	(*foffs0)++;

	mem_heap_free(heap);
	return(row_merge_write_eof(&block[2 * srv_sort_buf_size],
				   b2, of->fd, &of->offset)
	       != NULL);
}

/*************************************************************//**
Merge disk files.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull))
dberr_t
row_merge(
/*======*/
	trx_t*			trx,	/*!< in: transaction */
	const row_merge_dup_t*	dup,	/*!< in: descriptor of
					index being created */
	merge_file_t*		file,	/*!< in/out: file containing
					index entries */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	int*			tmpfd,	/*!< in/out: temporary file handle */
	ulint*			num_run,/*!< in/out: Number of runs remain
					to be merged */
	ulint*			run_offset) /*!< in/out: Array contains the
					first offset number for each merge
					run */
{
	ulint		foffs0;	/*!< first input offset */
	ulint		foffs1;	/*!< second input offset */
	dberr_t		error;	/*!< error code */
	merge_file_t	of;	/*!< output file */
	const ulint	ihalf	= run_offset[*num_run / 2];
				/*!< half the input file */
	ulint		n_run	= 0;
				/*!< num of runs generated from this merge */

	UNIV_MEM_ASSERT_W(&block[0], 3 * srv_sort_buf_size);

	ut_ad(ihalf < file->offset);

	of.fd = *tmpfd;
	of.offset = 0;
	of.n_rec = 0;

#ifdef POSIX_FADV_SEQUENTIAL
	/* The input file will be read sequentially, starting from the
	beginning and the middle.  In Linux, the POSIX_FADV_SEQUENTIAL
	affects the entire file.  Each block will be read exactly once. */
	posix_fadvise(file->fd, 0, 0,
		      POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE);
#endif /* POSIX_FADV_SEQUENTIAL */

	/* Merge blocks to the output file. */
	foffs0 = 0;
	foffs1 = ihalf;

	UNIV_MEM_INVALID(run_offset, *num_run * sizeof *run_offset);

	for (; foffs0 < ihalf && foffs1 < file->offset; foffs0++, foffs1++) {

		if (trx_is_interrupted(trx)) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		error = row_merge_blocks(dup, file, block,
					 &foffs0, &foffs1, &of);

		if (error != DB_SUCCESS) {
			return(error);
		}

	}

	/* Copy the last blocks, if there are any. */

	while (foffs0 < ihalf) {
		if (UNIV_UNLIKELY(trx_is_interrupted(trx))) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		if (!row_merge_blocks_copy(dup->index, file, block,
					   &foffs0, &of)) {
			return(DB_CORRUPTION);
		}
	}

	ut_ad(foffs0 == ihalf);

	while (foffs1 < file->offset) {
		if (trx_is_interrupted(trx)) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		if (!row_merge_blocks_copy(dup->index, file, block,
					   &foffs1, &of)) {
			return(DB_CORRUPTION);
		}
	}

	ut_ad(foffs1 == file->offset);

	if (UNIV_UNLIKELY(of.n_rec != file->n_rec)) {
		return(DB_CORRUPTION);
	}

	ut_ad(n_run <= *num_run);

	*num_run = n_run;

	/* Each run can contain one or more offsets. As merge goes on,
	the number of runs (to merge) will reduce until we have one
	single run. So the number of runs will always be smaller than
	the number of offsets in file */
	ut_ad((*num_run) <= file->offset);

	/* The number of offsets in output file is always equal or
	smaller than input file */
	ut_ad(of.offset <= file->offset);

	/* Swap file descriptors for the next pass. */
	*tmpfd = file->fd;
	*file = of;

	UNIV_MEM_INVALID(&block[0], 3 * srv_sort_buf_size);

	return(DB_SUCCESS);
}

/*************************************************************//**
Merge disk files.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_merge_sort(
/*===========*/
	trx_t*			trx,	/*!< in: transaction */
	const row_merge_dup_t*	dup,	/*!< in: descriptor of
					index being created */
	merge_file_t*		file,	/*!< in/out: file containing
					index entries */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	int*			tmpfd)	/*!< in/out: temporary file handle */
{
	const ulint	half	= file->offset / 2;
	ulint		num_runs;
	ulint*		run_offset;
	dberr_t		error	= DB_SUCCESS;

	/* Record the number of merge runs we need to perform */
	num_runs = file->offset;

	/* If num_runs are less than 1, nothing to merge */
	if (num_runs <= 1) {
		return(error);
	}

	/* "run_offset" records each run's first offset number */
	run_offset = (ulint*) mem_alloc(file->offset * sizeof(ulint));

	/* This tells row_merge() where to start for the first round
	of merge. */
	run_offset[half] = half;

	/* The file should always contain at least one byte (the end
	of file marker).  Thus, it must be at least one block. */
	ut_ad(file->offset > 0);

	/* Merge the runs until we have one big run */
	do {
		error = row_merge(trx, dup, file, block, tmpfd,
				  &num_runs, run_offset);

		UNIV_MEM_ASSERT_RW(run_offset, num_runs * sizeof *run_offset);

		if (error != DB_SUCCESS) {
			break;
		}
	} while (num_runs > 1);

	mem_free(run_offset);

	return(error);
}

/*************************************************************//**
Set blob fields empty */
static __attribute__((nonnull))
void
row_merge_set_blob_empty(
/*=====================*/
	dtuple_t*	tuple)	/*!< in/out: data tuple */
{
	for (ulint i = 0; i < dtuple_get_n_fields(tuple); i++) {
		dfield_t*	field = dtuple_get_nth_field(tuple, i);

		if (dfield_is_ext(field)) {
			dfield_set_data(field, NULL, 0);
		}
	}
}

/*************************************************************//**
Copy externally stored columns to the data tuple. */
static __attribute__((nonnull))
void
row_merge_copy_blobs(
/*=================*/
	const mrec_t*	mrec,	/*!< in: merge record */
	const ulint*	offsets,/*!< in: offsets of mrec */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	dtuple_t*	tuple,	/*!< in/out: data tuple */
	mem_heap_t*	heap)	/*!< in/out: memory heap */
{
	ut_ad(rec_offs_any_extern(offsets));

	for (ulint i = 0; i < dtuple_get_n_fields(tuple); i++) {
		ulint		len;
		const void*	data;
		dfield_t*	field = dtuple_get_nth_field(tuple, i);

		if (!dfield_is_ext(field)) {
			continue;
		}

		ut_ad(!dfield_is_null(field));

		/* During the creation of a PRIMARY KEY, the table is
		X-locked, and we skip copying records that have been
		marked for deletion. Therefore, externally stored
		columns cannot possibly be freed between the time the
		BLOB pointers are read (row_merge_read_clustered_index())
		and dereferenced (below). */
		data = btr_rec_copy_externally_stored_field(
			mrec, offsets, zip_size, i, &len, heap);
		/* Because we have locked the table, any records
		written by incomplete transactions must have been
		rolled back already. There must not be any incomplete
		BLOB columns. */
		ut_a(data);

		dfield_set_data(field, data, len);
	}
}

/********************************************************************//**
Read sorted file containing index data tuples and insert these data
tuples to the index
@return	DB_SUCCESS or error number */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_merge_insert_index_tuples(
/*==========================*/
	trx_id_t		trx_id,	/*!< in: transaction identifier */
	dict_index_t*		index,	/*!< in: index */
	const dict_table_t*	old_table,/*!< in: old table */
	int			fd,	/*!< in: file descriptor */
	row_merge_block_t*	block)	/*!< in/out: file buffer */
{
	const byte*		b;
	mem_heap_t*		heap;
	mem_heap_t*		tuple_heap;
	mem_heap_t*		ins_heap;
	dberr_t			error = DB_SUCCESS;
	ulint			foffs = 0;
	ulint*			offsets;
	mrec_buf_t*		buf;

	ut_ad(!srv_read_only_mode);
	ut_ad(!(index->type & DICT_FTS));
	ut_ad(trx_id);

	tuple_heap = mem_heap_create(1000);

	{
		ulint i	= 1 + REC_OFFS_HEADER_SIZE
			+ dict_index_get_n_fields(index);
		heap = mem_heap_create(sizeof *buf + i * sizeof *offsets);
		ins_heap = mem_heap_create(sizeof *buf + i * sizeof *offsets);
		offsets = static_cast<ulint*>(
			mem_heap_alloc(heap, i * sizeof *offsets));
		offsets[0] = i;
		offsets[1] = dict_index_get_n_fields(index);
	}

	b = block;

	if (!row_merge_read(fd, foffs, block)) {
		error = DB_CORRUPTION;
	} else {
		buf = static_cast<mrec_buf_t*>(
			mem_heap_alloc(heap, sizeof *buf));

		for (;;) {
			const mrec_t*	mrec;
			dtuple_t*	dtuple;
			ulint		n_ext;
			big_rec_t*	big_rec;
			rec_t*		rec;
			btr_cur_t	cursor;
			mtr_t		mtr;

			b = row_merge_read_rec(block, buf, b, index,
					       fd, &foffs, &mrec, offsets);
			if (UNIV_UNLIKELY(!b)) {
				/* End of list, or I/O error */
				if (mrec) {
					error = DB_CORRUPTION;
				}
				break;
			}

			dict_index_t*	old_index
				= dict_table_get_first_index(old_table);

			if (dict_index_is_clust(index)
			    && dict_index_is_online_ddl(old_index)) {
				error = row_log_table_get_error(old_index);
				if (error != DB_SUCCESS) {
					break;
				}
			}

			dtuple = row_rec_to_index_entry_low(
				mrec, index, offsets, &n_ext, tuple_heap);

			if (!n_ext) {
				/* There are no externally stored columns. */
			} else if (!dict_index_is_online_ddl(old_index)) {
				ut_ad(dict_index_is_clust(index));
				/* Modifications to the table are
				blocked while we are not rebuilding it
				or creating indexes. Off-page columns
				can be fetched safely. */
				row_merge_copy_blobs(
					mrec, offsets,
					dict_table_zip_size(old_table),
					dtuple, tuple_heap);
			} else {
				ut_ad(dict_index_is_clust(index));

				ulint	offset = index->trx_id_offset;

				if (!offset) {
					offset = row_get_trx_id_offset(
						index, offsets);
				}

				/* Copy the off-page columns while
				holding old_index->lock, so
				that they cannot be freed by
				a rollback of a fresh insert. */
				rw_lock_s_lock(&old_index->lock);

				if (row_log_table_is_rollback(
					    old_index,
					    trx_read_trx_id(mrec + offset))) {
					/* The row and BLOB could
					already be freed. They
					will be deleted by
					row_undo_ins_remove_clust_rec
					when rolling back a fresh
					insert. So, no need to retrieve
					the off-page column. */
					row_merge_set_blob_empty(
						dtuple);
				} else {
					row_merge_copy_blobs(
						mrec, offsets,
						dict_table_zip_size(old_table),
						dtuple, tuple_heap);
				}

				rw_lock_s_unlock(&old_index->lock);
			}

			ut_ad(dtuple_validate(dtuple));
			log_free_check();

			mtr_start(&mtr);
			/* Insert after the last user record. */
			btr_cur_open_at_index_side(
				false, index, BTR_MODIFY_LEAF,
				&cursor, 0, &mtr);
			page_cur_position(
				page_rec_get_prev(btr_cur_get_rec(&cursor)),
				btr_cur_get_block(&cursor),
				btr_cur_get_page_cur(&cursor));
			cursor.flag = BTR_CUR_BINARY;
#ifdef UNIV_DEBUG
			/* Check that the records are inserted in order. */
			rec = btr_cur_get_rec(&cursor);

			if (!page_rec_is_infimum(rec)) {
				ulint*	rec_offsets = rec_get_offsets(
					rec, index, offsets,
					ULINT_UNDEFINED, &tuple_heap);
				ut_ad(cmp_dtuple_rec(dtuple, rec, rec_offsets)
				      > 0);
			}
#endif /* UNIV_DEBUG */
			ulint*	ins_offsets = NULL;

			error = btr_cur_optimistic_insert(
				BTR_NO_UNDO_LOG_FLAG | BTR_NO_LOCKING_FLAG
				| BTR_KEEP_SYS_FLAG | BTR_CREATE_FLAG,
				&cursor, &ins_offsets, &ins_heap,
				dtuple, &rec, &big_rec, 0, NULL, &mtr);

			if (error == DB_FAIL) {
				ut_ad(!big_rec);
				mtr_commit(&mtr);
				mtr_start(&mtr);
				btr_cur_open_at_index_side(
					false, index, BTR_MODIFY_TREE,
					&cursor, 0, &mtr);
				page_cur_position(
					page_rec_get_prev(btr_cur_get_rec(
								  &cursor)),
					btr_cur_get_block(&cursor),
					btr_cur_get_page_cur(&cursor));

				error = btr_cur_pessimistic_insert(
					BTR_NO_UNDO_LOG_FLAG
					| BTR_NO_LOCKING_FLAG
					| BTR_KEEP_SYS_FLAG | BTR_CREATE_FLAG,
					&cursor, &ins_offsets, &ins_heap,
					dtuple, &rec, &big_rec, 0, NULL, &mtr);
			}

			if (!dict_index_is_clust(index)) {
				page_update_max_trx_id(
					btr_cur_get_block(&cursor),
					btr_cur_get_page_zip(&cursor),
					trx_id, &mtr);
			}

			mtr_commit(&mtr);

			if (UNIV_LIKELY_NULL(big_rec)) {
				/* If the system crashes at this
				point, the clustered index record will
				contain a null BLOB pointer. This
				should not matter, because the copied
				table will be dropped on crash
				recovery anyway. */

				ut_ad(dict_index_is_clust(index));
				ut_ad(error == DB_SUCCESS);
				error = row_ins_index_entry_big_rec(
					dtuple, big_rec,
					ins_offsets, &ins_heap,
					index, NULL, __FILE__, __LINE__);
				dtuple_convert_back_big_rec(
					index, dtuple, big_rec);
			}

			if (error != DB_SUCCESS) {
				goto err_exit;
			}

			mem_heap_empty(tuple_heap);
			mem_heap_empty(ins_heap);
		}
	}

err_exit:
	mem_heap_free(tuple_heap);
	mem_heap_free(ins_heap);
	mem_heap_free(heap);

	return(error);
}

/*********************************************************************//**
Sets an exclusive lock on a table, for the duration of creating indexes.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_merge_lock_table(
/*=================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode)		/*!< in: LOCK_X or LOCK_S */
{
	mem_heap_t*	heap;
	que_thr_t*	thr;
	dberr_t		err;
	sel_node_t*	node;

	ut_ad(!srv_read_only_mode);
	ut_ad(mode == LOCK_X || mode == LOCK_S);

	heap = mem_heap_create(512);

	trx->op_info = "setting table lock for creating or dropping index";

	node = sel_node_create(heap);
	thr = pars_complete_graph_for_exec(node, trx, heap);
	thr->graph->state = QUE_FORK_ACTIVE;

	/* We use the select query graph as the dummy graph needed
	in the lock module call */

	thr = static_cast<que_thr_t*>(
		que_fork_get_first_thr(
			static_cast<que_fork_t*>(que_node_get_parent(thr))));

	que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
	thr->run_node = thr;
	thr->prev_node = thr->common.parent;

	err = lock_table(0, table, mode, thr);

	trx->error_state = err;

	if (UNIV_LIKELY(err == DB_SUCCESS)) {
		que_thr_stop_for_mysql_no_error(thr, trx);
	} else {
		que_thr_stop_for_mysql(thr);

		if (err != DB_QUE_THR_SUSPENDED) {
			bool	was_lock_wait;

			was_lock_wait = row_mysql_handle_errors(
				&err, trx, thr, NULL);

			if (was_lock_wait) {
				goto run_again;
			}
		} else {
			que_thr_t*	run_thr;
			que_node_t*	parent;

			parent = que_node_get_parent(thr);

			run_thr = que_fork_start_command(
				static_cast<que_fork_t*>(parent));

			ut_a(run_thr == thr);

			/* There was a lock wait but the thread was not
			in a ready to run or running state. */
			trx->error_state = DB_LOCK_WAIT;

			goto run_again;
		}
	}

	que_graph_free(thr->graph);
	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Drop an index that was created before an error occurred.
The data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed. */
static
void
row_merge_drop_index_dict(
/*======================*/
	trx_t*		trx,	/*!< in/out: dictionary transaction */
	index_id_t	index_id)/*!< in: index identifier */
{
	static const char sql[] =
		"PROCEDURE DROP_INDEX_PROC () IS\n"
		"BEGIN\n"
		"DELETE FROM SYS_FIELDS WHERE INDEX_ID=:indexid;\n"
		"DELETE FROM SYS_INDEXES WHERE ID=:indexid;\n"
		"END;\n";
	dberr_t		error;
	pars_info_t*	info;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	info = pars_info_create();
	pars_info_add_ull_literal(info, "indexid", index_id);
	trx->op_info = "dropping index from dictionary";
	error = que_eval_sql(info, sql, FALSE, trx);

	if (error != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: row_merge_drop_index_dict "
			"failed with error code: %u.\n", (unsigned) error);
	}

	trx->op_info = "";
}

/*********************************************************************//**
Drop indexes that were created before an error occurred.
The data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed. */
UNIV_INTERN
void
row_merge_drop_indexes_dict(
/*========================*/
	trx_t*		trx,	/*!< in/out: dictionary transaction */
	table_id_t	table_id)/*!< in: table identifier */
{
	static const char sql[] =
		"PROCEDURE DROP_INDEXES_PROC () IS\n"
		"ixid CHAR;\n"
		"found INT;\n"

		"DECLARE CURSOR index_cur IS\n"
		" SELECT ID FROM SYS_INDEXES\n"
		" WHERE TABLE_ID=:tableid AND\n"
		" SUBSTR(NAME,0,1)='" TEMP_INDEX_PREFIX_STR "'\n"
		"FOR UPDATE;\n"

		"BEGIN\n"
		"found := 1;\n"
		"OPEN index_cur;\n"
		"WHILE found = 1 LOOP\n"
		"  FETCH index_cur INTO ixid;\n"
		"  IF (SQL % NOTFOUND) THEN\n"
		"    found := 0;\n"
		"  ELSE\n"
		"    DELETE FROM SYS_FIELDS WHERE INDEX_ID=ixid;\n"
		"    DELETE FROM SYS_INDEXES WHERE CURRENT OF index_cur;\n"
		"  END IF;\n"
		"END LOOP;\n"
		"CLOSE index_cur;\n"

		"END;\n";
	dberr_t		error;
	pars_info_t*	info;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	/* It is possible that table->n_ref_count > 1 when
	locked=TRUE. In this case, all code that should have an open
	handle to the table be waiting for the next statement to execute,
	or waiting for a meta-data lock.

	A concurrent purge will be prevented by dict_operation_lock. */

	info = pars_info_create();
	pars_info_add_ull_literal(info, "tableid", table_id);
	trx->op_info = "dropping indexes";
	error = que_eval_sql(info, sql, FALSE, trx);

	if (error != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: row_merge_drop_indexes_dict "
			"failed with error code: %u.\n", (unsigned) error);
	}

	trx->op_info = "";
}

/*********************************************************************//**
Drop indexes that were created before an error occurred.
The data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed. */
UNIV_INTERN
void
row_merge_drop_indexes(
/*===================*/
	trx_t*		trx,	/*!< in/out: dictionary transaction */
	dict_table_t*	table,	/*!< in/out: table containing the indexes */
	ibool		locked)	/*!< in: TRUE=table locked,
				FALSE=may need to do a lazy drop */
{
	dict_index_t*	index;
	dict_index_t*	next_index;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	index = dict_table_get_first_index(table);
	ut_ad(dict_index_is_clust(index));
	ut_ad(dict_index_get_online_status(index) == ONLINE_INDEX_COMPLETE);

	/* the caller should have an open handle to the table */
	ut_ad(table->n_ref_count >= 1);

	/* It is possible that table->n_ref_count > 1 when
	locked=TRUE. In this case, all code that should have an open
	handle to the table be waiting for the next statement to execute,
	or waiting for a meta-data lock.

	A concurrent purge will be prevented by dict_operation_lock. */

	if (!locked && table->n_ref_count > 1) {
		/* We will have to drop the indexes later, when the
		table is guaranteed to be no longer in use.  Mark the
		indexes as incomplete and corrupted, so that other
		threads will stop using them.  Let dict_table_close()
		or crash recovery or the next invocation of
		prepare_inplace_alter_table() take care of dropping
		the indexes. */

		while ((index = dict_table_get_next_index(index)) != NULL) {
			ut_ad(!dict_index_is_clust(index));

			switch (dict_index_get_online_status(index)) {
			case ONLINE_INDEX_ABORTED_DROPPED:
				continue;
			case ONLINE_INDEX_COMPLETE:
				if (*index->name != TEMP_INDEX_PREFIX) {
					/* Do nothing to already
					published indexes. */
				} else if (index->type & DICT_FTS) {
					/* Drop a completed FULLTEXT
					index, due to a timeout during
					MDL upgrade for
					commit_inplace_alter_table().
					Because only concurrent reads
					are allowed (and they are not
					seeing this index yet) we
					are safe to drop the index. */
					dict_index_t* prev = UT_LIST_GET_PREV(
						indexes, index);
					/* At least there should be
					the clustered index before
					this one. */
					ut_ad(prev);
					ut_a(table->fts);
					fts_drop_index(table, index, trx);
					/* Since
					INNOBASE_SHARE::idx_trans_tbl
					is shared between all open
					ha_innobase handles to this
					table, no thread should be
					accessing this dict_index_t
					object. Also, we should be
					holding LOCK=SHARED MDL on the
					table even after the MDL
					upgrade timeout. */
					dict_index_remove_from_cache(
						table, index);
					index = prev;
				} else {
					rw_lock_x_lock(
						dict_index_get_lock(index));
					dict_index_set_online_status(
						index, ONLINE_INDEX_ABORTED);
					index->type |= DICT_CORRUPT;
					table->drop_aborted = TRUE;
					goto drop_aborted;
				}
				continue;
			case ONLINE_INDEX_CREATION:
				rw_lock_x_lock(dict_index_get_lock(index));
				ut_ad(*index->name == TEMP_INDEX_PREFIX);
				row_log_abort_sec(index);
			drop_aborted:
				rw_lock_x_unlock(dict_index_get_lock(index));

				DEBUG_SYNC_C("merge_drop_index_after_abort");
				/* covered by dict_sys->mutex */
				MONITOR_INC(MONITOR_BACKGROUND_DROP_INDEX);
				/* fall through */
			case ONLINE_INDEX_ABORTED:
				/* Drop the index tree from the
				data dictionary and free it from
				the tablespace, but keep the object
				in the data dictionary cache. */
				row_merge_drop_index_dict(trx, index->id);
				rw_lock_x_lock(dict_index_get_lock(index));
				dict_index_set_online_status(
					index, ONLINE_INDEX_ABORTED_DROPPED);
				rw_lock_x_unlock(dict_index_get_lock(index));
				table->drop_aborted = TRUE;
				continue;
			}
			ut_error;
		}

		return;
	}

	row_merge_drop_indexes_dict(trx, table->id);

	next_index = dict_table_get_next_index(index);

	while ((index = next_index) != NULL) {
		/* read the next pointer before freeing the index */
		next_index = dict_table_get_next_index(index);

		ut_ad(!dict_index_is_clust(index));

		if (*index->name == TEMP_INDEX_PREFIX) {
			/* If it is FTS index, drop from table->fts
			and also drop its auxiliary tables */
			if (index->type & DICT_FTS) {
				ut_a(table->fts);
				fts_drop_index(table, index, trx);
			}

			switch (dict_index_get_online_status(index)) {
			case ONLINE_INDEX_CREATION:
				/* This state should only be possible
				when prepare_inplace_alter_table() fails
				after invoking row_merge_create_index().
				In inplace_alter_table(),
				row_merge_build_indexes()
				should never leave the index in this state.
				It would invoke row_log_abort_sec() on
				failure. */
			case ONLINE_INDEX_COMPLETE:
				/* In these cases, we are able to drop
				the index straight. The DROP INDEX was
				never deferred. */
				break;
			case ONLINE_INDEX_ABORTED:
			case ONLINE_INDEX_ABORTED_DROPPED:
				/* covered by dict_sys->mutex */
				MONITOR_DEC(MONITOR_BACKGROUND_DROP_INDEX);
			}

			dict_index_remove_from_cache(table, index);
		}
	}

	table->drop_aborted = FALSE;
	ut_d(dict_table_check_for_dup_indexes(table, CHECK_ALL_COMPLETE));
}

/*********************************************************************//**
Drop all partially created indexes during crash recovery. */
UNIV_INTERN
void
row_merge_drop_temp_indexes(void)
/*=============================*/
{
	static const char sql[] =
		"PROCEDURE DROP_TEMP_INDEXES_PROC () IS\n"
		"ixid CHAR;\n"
		"found INT;\n"

		"DECLARE CURSOR index_cur IS\n"
		" SELECT ID FROM SYS_INDEXES\n"
		" WHERE SUBSTR(NAME,0,1)='" TEMP_INDEX_PREFIX_STR "'\n"
		"FOR UPDATE;\n"

		"BEGIN\n"
		"found := 1;\n"
		"OPEN index_cur;\n"
		"WHILE found = 1 LOOP\n"
		"  FETCH index_cur INTO ixid;\n"
		"  IF (SQL % NOTFOUND) THEN\n"
		"    found := 0;\n"
		"  ELSE\n"
		"    DELETE FROM SYS_FIELDS WHERE INDEX_ID=ixid;\n"
		"    DELETE FROM SYS_INDEXES WHERE CURRENT OF index_cur;\n"
		"  END IF;\n"
		"END LOOP;\n"
		"CLOSE index_cur;\n"
		"END;\n";
	trx_t*	trx;
	dberr_t	error;

	/* Load the table definitions that contain partially defined
	indexes, so that the data dictionary information can be checked
	when accessing the tablename.ibd files. */
	trx = trx_allocate_for_background();
	trx->op_info = "dropping partially created indexes";
	row_mysql_lock_data_dictionary(trx);
	/* Ensure that this transaction will be rolled back and locks
	will be released, if the server gets killed before the commit
	gets written to the redo log. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	trx->op_info = "dropping indexes";
	error = que_eval_sql(NULL, sql, FALSE, trx);

	if (error != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: row_merge_drop_temp_indexes "
			"failed with error code: %u.\n", (unsigned) error);
	}

	trx_commit_for_mysql(trx);
	row_mysql_unlock_data_dictionary(trx);
	trx_free_for_background(trx);
}

/*********************************************************************//**
Creates temporary merge files, and if UNIV_PFS_IO defined, register
the file descriptor with Performance Schema.
@return File descriptor */
UNIV_INTERN
int
row_merge_file_create_low(void)
/*===========================*/
{
	int	fd;
#ifdef UNIV_PFS_IO
	/* This temp file open does not go through normal
	file APIs, add instrumentation to register with
	performance schema */
	struct PSI_file_locker*	locker = NULL;
	PSI_file_locker_state	state;
	register_pfs_file_open_begin(&state, locker, innodb_file_temp_key,
				     PSI_FILE_OPEN,
				     "Innodb Merge Temp File",
				     __FILE__, __LINE__);
#endif
	fd = innobase_mysql_tmpfile();
#ifdef UNIV_PFS_IO
        register_pfs_file_open_end(locker, fd);
#endif
	return(fd);
}

/*********************************************************************//**
Create a merge file. */
UNIV_INTERN
void
row_merge_file_create(
/*==================*/
	merge_file_t*	merge_file)	/*!< out: merge file structure */
{
	merge_file->fd = row_merge_file_create_low();
	if (srv_disable_sort_file_cache) {
		os_file_set_nocache(merge_file->fd, "row0merge.c", "sort");
	}
	merge_file->offset = 0;
	merge_file->n_rec = 0;
}

/*********************************************************************//**
Destroy a merge file. And de-register the file from Performance Schema
if UNIV_PFS_IO is defined. */
UNIV_INTERN
void
row_merge_file_destroy_low(
/*=======================*/
	int		fd)	/*!< in: merge file descriptor */
{
#ifdef UNIV_PFS_IO
	struct PSI_file_locker*	locker = NULL;
	PSI_file_locker_state	state;
	register_pfs_file_io_begin(&state, locker,
				   fd, 0, PSI_FILE_CLOSE,
				   __FILE__, __LINE__);
#endif
	close(fd);
#ifdef UNIV_PFS_IO
	register_pfs_file_io_end(locker, 0);
#endif
}
/*********************************************************************//**
Destroy a merge file. */
UNIV_INTERN
void
row_merge_file_destroy(
/*===================*/
	merge_file_t*	merge_file)	/*!< in/out: merge file structure */
{
	ut_ad(!srv_read_only_mode);

	if (merge_file->fd != -1) {
		row_merge_file_destroy_low(merge_file->fd);
		merge_file->fd = -1;
	}
}

/*********************************************************************//**
Rename an index in the dictionary that was created. The data
dictionary must have been locked exclusively by the caller, because
the transaction will not be committed.
@return	DB_SUCCESS if all OK */
UNIV_INTERN
dberr_t
row_merge_rename_index_to_add(
/*==========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	table_id_t	table_id,	/*!< in: table identifier */
	index_id_t	index_id)	/*!< in: index identifier */
{
	dberr_t		err = DB_SUCCESS;
	pars_info_t*	info = pars_info_create();

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in renaming indexes. */

	static const char rename_index[] =
		"PROCEDURE RENAME_INDEX_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES SET NAME=SUBSTR(NAME,1,LENGTH(NAME)-1)\n"
		"WHERE TABLE_ID = :tableid AND ID = :indexid;\n"
		"END;\n";

	ut_ad(trx);
	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);

	trx->op_info = "renaming index to add";

	pars_info_add_ull_literal(info, "tableid", table_id);
	pars_info_add_ull_literal(info, "indexid", index_id);

	err = que_eval_sql(info, rename_index, FALSE, trx);

	if (err != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: row_merge_rename_index_to_add "
			 "failed with error code: %u.\n", (unsigned) err);
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Rename an index in the dictionary that is to be dropped. The data
dictionary must have been locked exclusively by the caller, because
the transaction will not be committed.
@return	DB_SUCCESS if all OK */
UNIV_INTERN
dberr_t
row_merge_rename_index_to_drop(
/*===========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	table_id_t	table_id,	/*!< in: table identifier */
	index_id_t	index_id)	/*!< in: index identifier */
{
	dberr_t		err;
	pars_info_t*	info = pars_info_create();

	ut_ad(!srv_read_only_mode);

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in renaming indexes. */

	static const char rename_index[] =
		"PROCEDURE RENAME_INDEX_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES SET NAME=CONCAT('"
		TEMP_INDEX_PREFIX_STR "',NAME)\n"
		"WHERE TABLE_ID = :tableid AND ID = :indexid;\n"
		"END;\n";

	ut_ad(trx);
	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);

	trx->op_info = "renaming index to drop";

	pars_info_add_ull_literal(info, "tableid", table_id);
	pars_info_add_ull_literal(info, "indexid", index_id);

	err = que_eval_sql(info, rename_index, FALSE, trx);

	if (err != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_CONCURRENT_TRXS. */
		trx->error_state = DB_SUCCESS;

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: row_merge_rename_index_to_drop "
			 "failed with error code: %u.\n", (unsigned) err);
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Provide a new pathname for a table that is being renamed if it belongs to
a file-per-table tablespace.  The caller is responsible for freeing the
memory allocated for the return value.
@return	new pathname of tablespace file, or NULL if space = 0 */
UNIV_INTERN
char*
row_make_new_pathname(
/*==================*/
	dict_table_t*	table,		/*!< in: table to be renamed */
	const char*	new_name)	/*!< in: new name */
{
	char*	new_path;
	char*	old_path;

	ut_ad(table->space != TRX_SYS_SPACE);

	old_path = fil_space_get_first_path(table->space);
	ut_a(old_path);

	new_path = os_file_make_new_pathname(old_path, new_name);

	mem_free(old_path);

	return(new_path);
}

/*********************************************************************//**
Rename the tables in the data dictionary.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_merge_rename_tables(
/*====================*/
	dict_table_t*	old_table,	/*!< in/out: old table, renamed to
					tmp_name */
	dict_table_t*	new_table,	/*!< in/out: new table, renamed to
					old_table->name */
	const char*	tmp_name,	/*!< in: new name for old_table */
	trx_t*		trx)		/*!< in: transaction handle */
{
	dberr_t		err	= DB_ERROR;
	pars_info_t*	info;
	char		old_name[MAX_FULL_NAME_LEN + 1];

	ut_ad(!srv_read_only_mode);
	ut_ad(old_table != new_table);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_TABLE);

	/* store the old/current name to an automatic variable */
	if (strlen(old_table->name) + 1 <= sizeof(old_name)) {
		memcpy(old_name, old_table->name, strlen(old_table->name) + 1);
	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Too long table name: '%s', max length is %d",
			old_table->name, MAX_FULL_NAME_LEN);
		ut_error;
	}

	trx->op_info = "renaming tables";

	DBUG_EXECUTE_IF(
		"ib_rebuild_cannot_rename",
		err = DB_ERROR; goto err_exit;);

	/* We use the private SQL parser of Innobase to generate the query
	graphs needed in updating the dictionary data in system tables. */

	info = pars_info_create();

	pars_info_add_str_literal(info, "new_name", new_table->name);
	pars_info_add_str_literal(info, "old_name", old_name);
	pars_info_add_str_literal(info, "tmp_name", tmp_name);

	err = que_eval_sql(info,
			   "PROCEDURE RENAME_TABLES () IS\n"
			   "BEGIN\n"
			   "UPDATE SYS_TABLES SET NAME = :tmp_name\n"
			   " WHERE NAME = :old_name;\n"
			   "UPDATE SYS_TABLES SET NAME = :old_name\n"
			   " WHERE NAME = :new_name;\n"
			   "END;\n", FALSE, trx);

	/* Update SYS_TABLESPACES and SYS_DATAFILES if the old
	table is in a non-system tablespace where space > 0. */
	if (err == DB_SUCCESS
	    && old_table->space != TRX_SYS_SPACE
	    && !old_table->ibd_file_missing) {
		/* Make pathname to update SYS_DATAFILES. */
		char* tmp_path = row_make_new_pathname(old_table, tmp_name);

		info = pars_info_create();

		pars_info_add_str_literal(info, "tmp_name", tmp_name);
		pars_info_add_str_literal(info, "tmp_path", tmp_path);
		pars_info_add_int4_literal(info, "old_space",
					   (lint) old_table->space);

		err = que_eval_sql(info,
				   "PROCEDURE RENAME_OLD_SPACE () IS\n"
				   "BEGIN\n"
				   "UPDATE SYS_TABLESPACES"
				   " SET NAME = :tmp_name\n"
				   " WHERE SPACE = :old_space;\n"
				   "UPDATE SYS_DATAFILES"
				   " SET PATH = :tmp_path\n"
				   " WHERE SPACE = :old_space;\n"
				   "END;\n", FALSE, trx);

		mem_free(tmp_path);
	}

	/* Update SYS_TABLESPACES and SYS_DATAFILES if the new
	table is in a non-system tablespace where space > 0. */
	if (err == DB_SUCCESS && new_table->space != TRX_SYS_SPACE) {
		/* Make pathname to update SYS_DATAFILES. */
		char* old_path = row_make_new_pathname(new_table, old_name);

		info = pars_info_create();

		pars_info_add_str_literal(info, "old_name", old_name);
		pars_info_add_str_literal(info, "old_path", old_path);
		pars_info_add_int4_literal(info, "new_space",
					   (lint) new_table->space);

		err = que_eval_sql(info,
				   "PROCEDURE RENAME_NEW_SPACE () IS\n"
				   "BEGIN\n"
				   "UPDATE SYS_TABLESPACES"
				   " SET NAME = :old_name\n"
				   " WHERE SPACE = :new_space;\n"
				   "UPDATE SYS_DATAFILES"
				   " SET PATH = :old_path\n"
				   " WHERE SPACE = :new_space;\n"
				   "END;\n", FALSE, trx);

		mem_free(old_path);
	}

	if (err != DB_SUCCESS) {
		goto err_exit;
	}

	/* Generate the redo logs for file operations */
	fil_mtr_rename_log(old_table->space, old_name,
			   new_table->space, new_table->name, tmp_name);

	/* What if the redo logs are flushed to disk here?  This is
	tested with following crash point */
	DBUG_EXECUTE_IF("bug14669848_precommit", log_buffer_flush_to_disk();
			DBUG_SUICIDE(););

	/* File operations cannot be rolled back.  So, before proceeding
	with file operations, commit the dictionary changes.*/
	trx_commit_for_mysql(trx);

	/* If server crashes here, the dictionary in InnoDB and MySQL
	will differ.  The .ibd files and the .frm files must be swapped
	manually by the administrator. No loss of data. */
	DBUG_EXECUTE_IF("bug14669848", DBUG_SUICIDE(););

	/* Ensure that the redo logs are flushed to disk.  The config
	innodb_flush_log_at_trx_commit must not affect this. */
	log_buffer_flush_to_disk();

	/* The following calls will also rename the .ibd data files if
	the tables are stored in a single-table tablespace */

	err = dict_table_rename_in_cache(old_table, tmp_name, FALSE);

	if (err == DB_SUCCESS) {

		ut_ad(dict_table_is_discarded(old_table)
		      == dict_table_is_discarded(new_table));

		err = dict_table_rename_in_cache(new_table, old_name, FALSE);

		if (err != DB_SUCCESS) {

			if (dict_table_rename_in_cache(
					old_table, old_name, FALSE)
			    != DB_SUCCESS) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"Cannot undo the rename in cache "
					"from %s to %s", old_name, tmp_name);
			}

			goto err_exit;
		}

		if (dict_table_is_discarded(new_table)) {

			err = row_import_update_discarded_flag(
				trx, new_table->id, true, true);
		}
	}

	DBUG_EXECUTE_IF("ib_rebuild_cannot_load_fk",
			err = DB_ERROR; goto err_exit;);

	err = dict_load_foreigns(old_name, FALSE, TRUE);

	if (err != DB_SUCCESS) {
err_exit:
		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);
		trx->error_state = DB_SUCCESS;
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Create and execute a query graph for creating an index.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_merge_create_index_graph(
/*=========================*/
	trx_t*		trx,		/*!< in: trx */
	dict_table_t*	table,		/*!< in: table */
	dict_index_t*	index)		/*!< in: index */
{
	ind_node_t*	node;		/*!< Index creation node */
	mem_heap_t*	heap;		/*!< Memory heap */
	que_thr_t*	thr;		/*!< Query thread */
	dberr_t		err;

	ut_ad(trx);
	ut_ad(table);
	ut_ad(index);

	heap = mem_heap_create(512);

	index->table = table;
	node = ind_create_graph_create(index, heap, false);
	thr = pars_complete_graph_for_exec(node, trx, heap);

	ut_a(thr == que_fork_start_command(
			static_cast<que_fork_t*>(que_node_get_parent(thr))));

	que_run_threads(thr);

	err = trx->error_state;

	que_graph_free((que_t*) que_node_get_parent(thr));

	return(err);
}

/*********************************************************************//**
Create the index and load in to the dictionary.
@return	index, or NULL on error */
UNIV_INTERN
dict_index_t*
row_merge_create_index(
/*===================*/
	trx_t*			trx,	/*!< in/out: trx (sets error_state) */
	dict_table_t*		table,	/*!< in: the index is on this table */
	const index_def_t*	index_def)
					/*!< in: the index definition */
{
	dict_index_t*	index;
	dberr_t		err;
	ulint		n_fields = index_def->n_fields;
	ulint		i;

	ut_ad(!srv_read_only_mode);

	/* Create the index prototype, using the passed in def, this is not
	a persistent operation. We pass 0 as the space id, and determine at
	a lower level the space id where to store the table. */

	index = dict_mem_index_create(table->name, index_def->name,
				      0, index_def->ind_type, n_fields);

	ut_a(index);

	for (i = 0; i < n_fields; i++) {
		index_field_t*	ifield = &index_def->fields[i];

		dict_mem_index_add_field(
			index, dict_table_get_col_name(table, ifield->col_no),
			ifield->prefix_len);
	}

	/* Add the index to SYS_INDEXES, using the index prototype. */
	err = row_merge_create_index_graph(trx, table, index);

	if (err == DB_SUCCESS) {

		index = dict_table_get_index_on_name(table, index_def->name);

		ut_a(index);

		/* Note the id of the transaction that created this
		index, we use it to restrict readers from accessing
		this index, to ensure read consistency. */
		index->trx_id = trx->id;
	} else {
		index = NULL;
	}

	return(index);
}

/*********************************************************************//**
Check if a transaction can use an index. */
UNIV_INTERN
ibool
row_merge_is_index_usable(
/*======================*/
	const trx_t*		trx,	/*!< in: transaction */
	const dict_index_t*	index)	/*!< in: index to check */
{
	if (!dict_index_is_clust(index)
	    && dict_index_is_online_ddl(index)) {
		/* Indexes that are being created are not useable. */
		return(FALSE);
	}

	return(!dict_index_is_corrupted(index)
	       && (dict_table_is_temporary(index->table)
		   || !trx->read_view
		   || read_view_sees_trx_id(trx->read_view, index->trx_id)));
}

/*********************************************************************//**
Drop the old table.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_merge_drop_table(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table,		/*!< in: table to drop */
	bool		nonatomic)	/*!< in: whether it is permitted
					to release and reacquire
					dict_operation_lock */
{
	ut_ad(!srv_read_only_mode);

	/* There must be no open transactions on the table. */
	ut_a(table->n_ref_count == 0);

	return(row_drop_table_for_mysql(table->name, trx, false, nonatomic));
}


/*********************************************************************//**
Build indexes on a table by reading a clustered index,
creating a temporary file containing index entries, merge sorting
these index entries and inserting sorted index entries to indexes.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_merge_build_indexes(
/*====================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	old_table,	/*!< in: table where rows are
					read from */
	dict_table_t*	new_table,	/*!< in: table where indexes are
					created; identical to old_table
					unless creating a PRIMARY KEY */
	bool		online,		/*!< in: true if creating indexes
					online */
	dict_index_t**	indexes,	/*!< in: indexes to be created */
	const ulint*	key_numbers,	/*!< in: MySQL key numbers */
	ulint		n_indexes,	/*!< in: size of indexes[] */
	struct TABLE*	table,		/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
	const dtuple_t*	add_cols,	/*!< in: default values of
					added columns, or NULL */
	const ulint*	col_map,	/*!< in: mapping of old column
					numbers to new ones, or NULL
					if old_table == new_table */
	ulint		add_autoinc,	/*!< in: number of added
					AUTO_INCREMENT column, or
					ULINT_UNDEFINED if none is added */
	ib_sequence_t&	sequence)	/*!< in: autoinc instance if
					add_autoinc != ULINT_UNDEFINED */
{
	merge_file_t*		merge_files;
	row_merge_block_t*	block;
	ulint			block_size;
	ulint			i;
	ulint			j;
	dberr_t			error;
	int			tmpfd;
	dict_index_t*		fts_sort_idx = NULL;
	fts_psort_t*		psort_info = NULL;
	fts_psort_t*		merge_info = NULL;
	ib_int64_t		sig_count = 0;

	ut_ad(!srv_read_only_mode);
	ut_ad((old_table == new_table) == !col_map);
	ut_ad(!add_cols || col_map);

	/* Allocate memory for merge file data structure and initialize
	fields */

	block_size = 3 * srv_sort_buf_size;
	block = static_cast<row_merge_block_t*>(
		os_mem_alloc_large(&block_size));

	if (block == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	trx_start_if_not_started_xa(trx);

	merge_files = static_cast<merge_file_t*>(
		mem_alloc(n_indexes * sizeof *merge_files));

	for (i = 0; i < n_indexes; i++) {
		row_merge_file_create(&merge_files[i]);

		if (indexes[i]->type & DICT_FTS) {
			ibool	opt_doc_id_size = FALSE;

			/* To build FTS index, we would need to extract
			doc's word, Doc ID, and word's position, so
			we need to build a "fts sort index" indexing
			on above three 'fields' */
			fts_sort_idx = row_merge_create_fts_sort_index(
				indexes[i], old_table, &opt_doc_id_size);

			row_merge_dup_t* dup = static_cast<row_merge_dup_t*>(
				ut_malloc(sizeof *dup));
			dup->index = fts_sort_idx;
			dup->table = table;
			dup->col_map = col_map;
			dup->n_dup = 0;

			row_fts_psort_info_init(
				trx, dup, new_table, opt_doc_id_size,
				&psort_info, &merge_info);
		}
	}

	tmpfd = row_merge_file_create_low();

	/* Reset the MySQL row buffer that is used when reporting
	duplicate keys. */
	innobase_rec_reset(table);

	/* Read clustered index of the table and create files for
	secondary index entries for merge sort */

	error = row_merge_read_clustered_index(
		trx, table, old_table, new_table, online, indexes,
		fts_sort_idx, psort_info, merge_files, key_numbers,
		n_indexes, add_cols, col_map,
		add_autoinc, sequence, block);

	if (error != DB_SUCCESS) {

		goto func_exit;
	}

	DEBUG_SYNC_C("row_merge_after_scan");

	/* Now we have files containing index entries ready for
	sorting and inserting. */

	for (i = 0; i < n_indexes; i++) {
		dict_index_t*	sort_idx;

		sort_idx = (indexes[i]->type & DICT_FTS)
			? fts_sort_idx
			: indexes[i];

		if (indexes[i]->type & DICT_FTS) {
			os_event_t	fts_parallel_merge_event;
			bool		all_exit = false;
			ulint		trial_count = 0;

			/* Now all children should complete, wait
			a bit until they all finish using event */
			while (!all_exit && trial_count < 10000) {
				all_exit = true;

				for (j = 0; j < fts_sort_pll_degree;
				     j++) {
					if (psort_info[j].child_status
					    != FTS_CHILD_EXITING) {
						all_exit = false;
						os_thread_sleep(1000);
						break;
					}
				}
				trial_count++;
			}

			if (!all_exit) {
				ib_logf(IB_LOG_LEVEL_ERROR,
					"Not all child sort threads exited"
					" when creating FTS index '%s'",
					indexes[i]->name);
					
			}

			fts_parallel_merge_event
				= merge_info[0].psort_common->merge_event;

			if (FTS_PLL_MERGE) {
				trial_count = 0;
				all_exit = false;
				os_event_reset(fts_parallel_merge_event);
				row_fts_start_parallel_merge(merge_info);
wait_again:
				os_event_wait_time_low(
					fts_parallel_merge_event, 1000000,
					sig_count);

				for (j = 0; j < FTS_NUM_AUX_INDEX; j++) {
					if (merge_info[j].child_status
					    != FTS_CHILD_COMPLETE
					    && merge_info[j].child_status
					    != FTS_CHILD_EXITING) {
						sig_count = os_event_reset(
						fts_parallel_merge_event);

						goto wait_again;
					}
				}

				/* Now all children should complete, wait
				a bit until they all finish using event */
				while (!all_exit && trial_count < 10000) {
					all_exit = true;

					for (j = 0; j < FTS_NUM_AUX_INDEX;
					     j++) {
						if (merge_info[j].child_status
						    != FTS_CHILD_EXITING) {
							all_exit = false;
							os_thread_sleep(1000);
							break;
						}
					}
					trial_count++;
				}

				if (!all_exit) {
					ib_logf(IB_LOG_LEVEL_ERROR,
						"Not all child merge threads"
						" exited when creating FTS"
						" index '%s'",
						indexes[i]->name);
						
				}
			} else {
				/* This cannot report duplicates; an
				assertion would fail in that case. */
				error = row_fts_merge_insert(
					sort_idx, new_table,
					psort_info, 0);
			}

#ifdef FTS_INTERNAL_DIAG_PRINT
			DEBUG_FTS_SORT_PRINT("FTS_SORT: Complete Insert\n");
#endif
		} else {
			row_merge_dup_t	dup = {
				sort_idx, table, col_map, 0};

			error = row_merge_sort(
				trx, &dup, &merge_files[i],
				block, &tmpfd);

			if (error == DB_SUCCESS) {
				error = row_merge_insert_index_tuples(
					trx->id, sort_idx, old_table,
					merge_files[i].fd, block);
			}
		}

		/* Close the temporary file to free up space. */
		row_merge_file_destroy(&merge_files[i]);

		if (indexes[i]->type & DICT_FTS) {
			row_fts_psort_info_destroy(psort_info, merge_info);
		} else if (error != DB_SUCCESS || !online) {
			/* Do not apply any online log. */
		} else if (old_table != new_table) {
			ut_ad(!sort_idx->online_log);
			ut_ad(sort_idx->online_status
			      == ONLINE_INDEX_COMPLETE);
		} else {
			DEBUG_SYNC_C("row_log_apply_before");
			error = row_log_apply(trx, sort_idx, table);
			DEBUG_SYNC_C("row_log_apply_after");
		}

		if (error != DB_SUCCESS) {
			trx->error_key_num = key_numbers[i];
			goto func_exit;
		}

		if (indexes[i]->type & DICT_FTS && fts_enable_diag_print) {
			char*	name = (char*) indexes[i]->name;

			if (*name == TEMP_INDEX_PREFIX)  {
				name++;
			}

			ut_print_timestamp(stderr);
			fprintf(stderr, " InnoDB: Finished building "
				"full-text index %s\n", name);
		}
	}

func_exit:
	DBUG_EXECUTE_IF(
		"ib_build_indexes_too_many_concurrent_trxs",
		error = DB_TOO_MANY_CONCURRENT_TRXS;
		trx->error_state = error;);

	row_merge_file_destroy_low(tmpfd);

	for (i = 0; i < n_indexes; i++) {
		row_merge_file_destroy(&merge_files[i]);
	}

	if (fts_sort_idx) {
		dict_mem_index_free(fts_sort_idx);
	}

	mem_free(merge_files);
	os_mem_free_large(block, block_size);

	DICT_TF2_FLAG_UNSET(new_table, DICT_TF2_FTS_ADD_DOC_ID);

	if (online && old_table == new_table && error != DB_SUCCESS) {
		/* On error, flag all online secondary index creation
		as aborted. */
		for (i = 0; i < n_indexes; i++) {
			ut_ad(!(indexes[i]->type & DICT_FTS));
			ut_ad(*indexes[i]->name == TEMP_INDEX_PREFIX);
			ut_ad(!dict_index_is_clust(indexes[i]));

			/* Completed indexes should be dropped as
			well, and indexes whose creation was aborted
			should be dropped from the persistent
			storage. However, at this point we can only
			set some flags in the not-yet-published
			indexes. These indexes will be dropped later
			in row_merge_drop_indexes(), called by
			rollback_inplace_alter_table(). */

			switch (dict_index_get_online_status(indexes[i])) {
			case ONLINE_INDEX_COMPLETE:
				break;
			case ONLINE_INDEX_CREATION:
				rw_lock_x_lock(
					dict_index_get_lock(indexes[i]));
				row_log_abort_sec(indexes[i]);
				indexes[i]->type |= DICT_CORRUPT;
				rw_lock_x_unlock(
					dict_index_get_lock(indexes[i]));
				new_table->drop_aborted = TRUE;
				/* fall through */
			case ONLINE_INDEX_ABORTED_DROPPED:
			case ONLINE_INDEX_ABORTED:
				MONITOR_MUTEX_INC(
					&dict_sys->mutex,
					MONITOR_BACKGROUND_DROP_INDEX);
			}
		}
	}

	return(error);
}
