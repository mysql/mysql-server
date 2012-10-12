/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file row/row0merge.c
New index creation routines using a merge sort

Created 12/4/2005 Jan Lindstrom
Completed by Sunny Bains and Marko Makela
*******************************************************/

#include "row0merge.h"
#include "row0ext.h"
#include "row0row.h"
#include "row0upd.h"
#include "row0ins.h"
#include "row0sel.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0load.h"
#include "btr0btr.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "trx0undo.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "que0que.h"
#include "rem0cmp.h"
#include "read0read.h"
#include "os0file.h"
#include "lock0lock.h"
#include "data0data.h"
#include "data0type.h"
#include "que0que.h"
#include "pars0pars.h"
#include "mem0mem.h"
#include "log0log.h"
#include "ut0sort.h"
#include "handler0alter.h"

/* Ignore posix_fadvise() on those platforms where it does not exist */
#if defined __WIN__
# define posix_fadvise(fd, offset, len, advice) /* nothing */
#endif /* __WIN__ */

#ifdef UNIV_DEBUG
/** Set these in order ot enable debug printout. */
/* @{ */
/** Log the outcome of each row_merge_cmp() call, comparing records. */
static ibool	row_merge_print_cmp;
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

/** @brief Block size for I/O operations in merge sort.

The minimum is UNIV_PAGE_SIZE, or page_get_free_space_of_empty()
rounded to a power of 2.

When not creating a PRIMARY KEY that contains column prefixes, this
can be set as small as UNIV_PAGE_SIZE / 2.  See the comment above
ut_ad(data_size < sizeof(row_merge_block_t)). */
typedef byte	row_merge_block_t[1048576];

/** @brief Secondary buffer for I/O operations of merge records.

This buffer is used for writing or reading a record that spans two
row_merge_block_t.  Thus, it must be able to hold one merge record,
whose maximum size is the same as the minimum size of
row_merge_block_t. */
typedef byte	mrec_buf_t[UNIV_PAGE_SIZE];

/** @brief Merge record in row_merge_block_t.

The format is the same as a record in ROW_FORMAT=COMPACT with the
exception that the REC_N_NEW_EXTRA_BYTES are omitted. */
typedef byte	mrec_t;

/** Buffer for sorting in main memory. */
struct row_merge_buf_struct {
	mem_heap_t*	heap;		/*!< memory heap where allocated */
	dict_index_t*	index;		/*!< the index the tuples belong to */
	ulint		total_size;	/*!< total amount of data bytes */
	ulint		n_tuples;	/*!< number of data tuples */
	ulint		max_tuples;	/*!< maximum number of data tuples */
	const dfield_t**tuples;		/*!< array of pointers to
					arrays of fields that form
					the data tuples */
	const dfield_t**tmp_tuples;	/*!< temporary copy of tuples,
					for sorting */
};

/** Buffer for sorting in main memory. */
typedef struct row_merge_buf_struct row_merge_buf_t;

/** Information about temporary files used in merge sort */
struct merge_file_struct {
	int		fd;		/*!< file descriptor */
	ulint		offset;		/*!< file offset (end of file) */
	ib_uint64_t	n_rec;		/*!< number of records in the file */
};

/** Information about temporary files used in merge sort */
typedef struct merge_file_struct merge_file_t;

#ifdef UNIV_DEBUG
/******************************************************//**
Display a merge tuple. */
static
void
row_merge_tuple_print(
/*==================*/
	FILE*		f,	/*!< in: output stream */
	const dfield_t*	entry,	/*!< in: tuple to print */
	ulint		n_fields)/*!< in: number of fields in the tuple */
{
	ulint	j;

	for (j = 0; j < n_fields; j++) {
		const dfield_t*	field = &entry[j];

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
Allocate a sort buffer.
@return	own: sort buffer */
static
row_merge_buf_t*
row_merge_buf_create_low(
/*=====================*/
	mem_heap_t*	heap,		/*!< in: heap where allocated */
	dict_index_t*	index,		/*!< in: secondary index */
	ulint		max_tuples,	/*!< in: maximum number of data tuples */
	ulint		buf_size)	/*!< in: size of the buffer, in bytes */
{
	row_merge_buf_t*	buf;

	ut_ad(max_tuples > 0);
	ut_ad(max_tuples <= sizeof(row_merge_block_t));
	ut_ad(max_tuples < buf_size);

	buf = mem_heap_zalloc(heap, buf_size);
	buf->heap = heap;
	buf->index = index;
	buf->max_tuples = max_tuples;
	buf->tuples = mem_heap_alloc(heap,
				     2 * max_tuples * sizeof *buf->tuples);
	buf->tmp_tuples = buf->tuples + max_tuples;

	return(buf);
}

/******************************************************//**
Allocate a sort buffer.
@return	own: sort buffer */
static
row_merge_buf_t*
row_merge_buf_create(
/*=================*/
	dict_index_t*	index)	/*!< in: secondary index */
{
	row_merge_buf_t*	buf;
	ulint			max_tuples;
	ulint			buf_size;
	mem_heap_t*		heap;

	max_tuples = sizeof(row_merge_block_t)
		/ ut_max(1, dict_index_get_min_size(index));

	buf_size = (sizeof *buf) + (max_tuples - 1) * sizeof *buf->tuples;

	heap = mem_heap_create(buf_size + sizeof(row_merge_block_t));

	buf = row_merge_buf_create_low(heap, index, max_tuples, buf_size);

	return(buf);
}

/******************************************************//**
Empty a sort buffer.
@return	sort buffer */
static
row_merge_buf_t*
row_merge_buf_empty(
/*================*/
	row_merge_buf_t*	buf)	/*!< in,own: sort buffer */
{
	ulint		buf_size;
	ulint		max_tuples	= buf->max_tuples;
	mem_heap_t*	heap		= buf->heap;
	dict_index_t*	index		= buf->index;

	buf_size = (sizeof *buf) + (max_tuples - 1) * sizeof *buf->tuples;

	mem_heap_empty(heap);

	return(row_merge_buf_create_low(heap, index, max_tuples, buf_size));
}

/******************************************************//**
Deallocate a sort buffer. */
static
void
row_merge_buf_free(
/*===============*/
	row_merge_buf_t*	buf)	/*!< in,own: sort buffer, to be freed */
{
	mem_heap_free(buf->heap);
}

/******************************************************//**
Insert a data tuple into a sort buffer.
@return	TRUE if added, FALSE if out of space */
static
ibool
row_merge_buf_add(
/*==============*/
	row_merge_buf_t*	buf,	/*!< in/out: sort buffer */
	const dtuple_t*		row,	/*!< in: row in clustered index */
	const row_ext_t*	ext)	/*!< in: cache of externally stored
					column prefixes, or NULL */
{
	ulint			i;
	ulint			n_fields;
	ulint			data_size;
	ulint			extra_size;
	const dict_index_t*	index;
	dfield_t*		entry;
	dfield_t*		field;
	const dict_field_t*	ifield;

	if (buf->n_tuples >= buf->max_tuples) {
		return(FALSE);
	}

	UNIV_PREFETCH_R(row->fields);

	index = buf->index;

	n_fields = dict_index_get_n_fields(index);

	entry = mem_heap_alloc(buf->heap, n_fields * sizeof *entry);
	buf->tuples[buf->n_tuples] = entry;
	field = entry;

	data_size = 0;
	extra_size = UT_BITS_IN_BYTES(index->n_nullable);

	ifield = dict_index_get_nth_field(index, 0);

	for (i = 0; i < n_fields; i++, field++, ifield++) {
		const dict_col_t*	col;
		ulint			col_no;
		const dfield_t*		row_field;
		ulint			len;

		col = ifield->col;
		col_no = dict_col_get_no(col);
		row_field = dtuple_get_nth_field(row, col_no);
		dfield_copy(field, row_field);
		len = dfield_get_len(field);

		if (dfield_is_null(field)) {
			ut_ad(!(col->prtype & DATA_NOT_NULL));
			continue;
		} else if (UNIV_LIKELY(!ext)) {
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
				len, dfield_get_data(field));
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

#ifdef UNIV_DEBUG
	{
		ulint	size;
		ulint	extra;

		size = rec_get_converted_size_comp(index,
						   REC_STATUS_ORDINARY,
						   entry, n_fields, &extra);

		ut_ad(data_size + extra_size + REC_N_NEW_EXTRA_BYTES == size);
		ut_ad(extra_size + REC_N_NEW_EXTRA_BYTES == extra);
	}
#endif /* UNIV_DEBUG */

	/* Add to the total size of the record in row_merge_block_t
	the encoded length of extra_size and the extra bytes (extra_size).
	See row_merge_buf_write() for the variable-length encoding
	of extra_size. */
	data_size += (extra_size + 1) + ((extra_size + 1) >= 0x80);

	/* The following assertion may fail if row_merge_block_t is
	declared very small and a PRIMARY KEY is being created with
	many prefix columns.  In that case, the record may exceed the
	page_zip_rec_needs_ext() limit.  However, no further columns
	will be moved to external storage until the record is inserted
	to the clustered index B-tree. */
	ut_ad(data_size < sizeof(row_merge_block_t));

	/* Reserve one byte for the end marker of row_merge_block_t. */
	if (buf->total_size + data_size >= sizeof(row_merge_block_t) - 1) {
		return(FALSE);
	}

	buf->total_size += data_size;
	buf->n_tuples++;

	field = entry;

	/* Copy the data fields. */

	do {
		dfield_dup(field++, buf->heap);
	} while (--n_fields);

	return(TRUE);
}

/** Structure for reporting duplicate records. */
struct row_merge_dup_struct {
	const dict_index_t*	index;		/*!< index being sorted */
	struct TABLE*		table;		/*!< MySQL table object */
	ulint			n_dup;		/*!< number of duplicates */
};

/** Structure for reporting duplicate records. */
typedef struct row_merge_dup_struct row_merge_dup_t;

/*************************************************************//**
Report a duplicate key. */
static
void
row_merge_dup_report(
/*=================*/
	row_merge_dup_t*	dup,	/*!< in/out: for reporting duplicates */
	const dfield_t*		entry)	/*!< in: duplicate index entry */
{
	mrec_buf_t* 		buf;
	const dtuple_t*		tuple;
	dtuple_t		tuple_store;
	const rec_t*		rec;
	const dict_index_t*	index	= dup->index;
	ulint			n_fields= dict_index_get_n_fields(index);
	mem_heap_t*		heap;
	ulint*			offsets;
	ulint			n_ext;

	if (dup->n_dup++) {
		/* Only report the first duplicate record,
		but count all duplicate records. */
		return;
	}

	/* Convert the tuple to a record and then to MySQL format. */
	heap = mem_heap_create((1 + REC_OFFS_HEADER_SIZE + n_fields)
			       * sizeof *offsets
			       + sizeof *buf);

	buf = mem_heap_alloc(heap, sizeof *buf);

	tuple = dtuple_from_fields(&tuple_store, entry, n_fields);
	n_ext = dict_index_is_clust(index) ? dtuple_get_n_ext(tuple) : 0;

	rec = rec_convert_dtuple_to_rec(*buf, index, tuple, n_ext);
	offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

	innobase_rec_to_mysql(dup->table, rec, index, offsets);

	mem_heap_free(heap);
}

/*************************************************************//**
Compare two tuples.
@return	1, 0, -1 if a is greater, equal, less, respectively, than b */
static
int
row_merge_tuple_cmp(
/*================*/
	ulint			n_field,/*!< in: number of fields */
	const dfield_t*		a,	/*!< in: first tuple to be compared */
	const dfield_t*		b,	/*!< in: second tuple to be compared */
	row_merge_dup_t*	dup)	/*!< in/out: for reporting duplicates */
{
	int		cmp;
	const dfield_t*	field	= a;

	/* Compare the fields of the tuples until a difference is
	found or we run out of fields to compare.  If !cmp at the
	end, the tuples are equal. */
	do {
		cmp = cmp_dfield_dfield(a++, b++);
	} while (!cmp && --n_field);

	if (UNIV_UNLIKELY(!cmp) && UNIV_LIKELY_NULL(dup)) {
		/* Report a duplicate value error if the tuples are
		logically equal.  NULL columns are logically inequal,
		although they are equal in the sorting order.  Find
		out if any of the fields are NULL. */
		for (b = field; b != a; b++) {
			if (dfield_is_null(b)) {

				goto func_exit;
			}
		}

		row_merge_dup_report(dup, field);
	}

func_exit:
	return(cmp);
}

/** Wrapper for row_merge_tuple_sort() to inject some more context to
UT_SORT_FUNCTION_BODY().
@param a	array of tuples that being sorted
@param b	aux (work area), same size as tuples[]
@param c	lower bound of the sorting area, inclusive
@param d	upper bound of the sorting area, inclusive */
#define row_merge_tuple_sort_ctx(a,b,c,d) \
	row_merge_tuple_sort(n_field, dup, a, b, c, d)
/** Wrapper for row_merge_tuple_cmp() to inject some more context to
UT_SORT_FUNCTION_BODY().
@param a	first tuple to be compared
@param b	second tuple to be compared
@return	1, 0, -1 if a is greater, equal, less, respectively, than b */
#define row_merge_tuple_cmp_ctx(a,b) row_merge_tuple_cmp(n_field, a, b, dup)

/**********************************************************************//**
Merge sort the tuple buffer in main memory. */
static
void
row_merge_tuple_sort(
/*=================*/
	ulint			n_field,/*!< in: number of fields */
	row_merge_dup_t*	dup,	/*!< in/out: for reporting duplicates */
	const dfield_t**	tuples,	/*!< in/out: tuples */
	const dfield_t**	aux,	/*!< in/out: work area */
	ulint			low,	/*!< in: lower bound of the
					sorting area, inclusive */
	ulint			high)	/*!< in: upper bound of the
					sorting area, exclusive */
{
	UT_SORT_FUNCTION_BODY(row_merge_tuple_sort_ctx,
			      tuples, aux, low, high, row_merge_tuple_cmp_ctx);
}

/******************************************************//**
Sort a buffer. */
static
void
row_merge_buf_sort(
/*===============*/
	row_merge_buf_t*	buf,	/*!< in/out: sort buffer */
	row_merge_dup_t*	dup)	/*!< in/out: for reporting duplicates */
{
	row_merge_tuple_sort(dict_index_get_n_unique(buf->index), dup,
			     buf->tuples, buf->tmp_tuples, 0, buf->n_tuples);
}

/******************************************************//**
Write a buffer to a block. */
static
void
row_merge_buf_write(
/*================*/
	const row_merge_buf_t*	buf,	/*!< in: sorted buffer */
#ifdef UNIV_DEBUG
	const merge_file_t*	of,	/*!< in: output file */
#endif /* UNIV_DEBUG */
	row_merge_block_t*	block)	/*!< out: buffer for writing to file */
#ifndef UNIV_DEBUG
# define row_merge_buf_write(buf, of, block) row_merge_buf_write(buf, block)
#endif /* !UNIV_DEBUG */
{
	const dict_index_t*	index	= buf->index;
	ulint			n_fields= dict_index_get_n_fields(index);
	byte*			b	= &(*block)[0];

	ulint		i;

	for (i = 0; i < buf->n_tuples; i++) {
		ulint		size;
		ulint		extra_size;
		const dfield_t*	entry		= buf->tuples[i];

		size = rec_get_converted_size_comp(index,
						   REC_STATUS_ORDINARY,
						   entry, n_fields,
						   &extra_size);
		ut_ad(size >= extra_size);
		ut_ad(extra_size >= REC_N_NEW_EXTRA_BYTES);
		extra_size -= REC_N_NEW_EXTRA_BYTES;
		size -= REC_N_NEW_EXTRA_BYTES;

		/* Encode extra_size + 1 */
		if (extra_size + 1 < 0x80) {
			*b++ = (byte) (extra_size + 1);
		} else {
			ut_ad((extra_size + 1) < 0x8000);
			*b++ = (byte) (0x80 | ((extra_size + 1) >> 8));
			*b++ = (byte) (extra_size + 1);
		}

		ut_ad(b + size < block[1]);

		rec_convert_dtuple_to_rec_comp(b + extra_size, 0, index,
					       REC_STATUS_ORDINARY,
					       entry, n_fields);

		b += size;

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
	ut_a(b < block[1]);
	ut_a(b == block[0] + buf->total_size);
	*b++ = 0;
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0xff, block[1] - b);
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

	*buf = mem_heap_alloc(heap, 3 * sizeof **buf);
	*offsets1 = mem_heap_alloc(heap, i * sizeof **offsets1);
	*offsets2 = mem_heap_alloc(heap, i * sizeof **offsets2);

	(*offsets1)[0] = (*offsets2)[0] = i;
	(*offsets1)[1] = (*offsets2)[1] = dict_index_get_n_fields(index);

	return(heap);
}

/**********************************************************************//**
Search an index object by name and column names.  If several indexes match,
return the index with the max id.
@return	matching index, NULL if not found */
static
dict_index_t*
row_merge_dict_table_get_index(
/*===========================*/
	dict_table_t*		table,		/*!< in: table */
	const merge_index_def_t*index_def)	/*!< in: index definition */
{
	ulint		i;
	dict_index_t*	index;
	const char**	column_names;

	column_names = mem_alloc(index_def->n_fields * sizeof *column_names);

	for (i = 0; i < index_def->n_fields; ++i) {
		column_names[i] = index_def->fields[i].field_name;
	}

	index = dict_table_get_index_by_max_id(
		table, index_def->name, column_names, index_def->n_fields);

	mem_free((void*) column_names);

	return(index);
}

/********************************************************************//**
Read a merge block from the file system.
@return	TRUE if request was successful, FALSE if fail */
static
ibool
row_merge_read(
/*===========*/
	int			fd,	/*!< in: file descriptor */
	ulint			offset,	/*!< in: offset where to read
					in number of row_merge_block_t
					elements */
	row_merge_block_t*	buf)	/*!< out: data */
{
	ib_uint64_t	ofs = ((ib_uint64_t) offset) * sizeof *buf;
	ibool		success;

#ifdef UNIV_DEBUG
	if (row_merge_print_block_read) {
		fprintf(stderr, "row_merge_read fd=%d ofs=%lu\n",
			fd, (ulong) offset);
	}
#endif /* UNIV_DEBUG */

	success = os_file_read_no_error_handling(OS_FILE_FROM_FD(fd), buf,
						 (ulint) (ofs & 0xFFFFFFFF),
						 (ulint) (ofs >> 32),
						 sizeof *buf);
#ifdef POSIX_FADV_DONTNEED
	/* Each block is read exactly once.  Free up the file cache. */
	posix_fadvise(fd, ofs, sizeof *buf, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

	if (UNIV_UNLIKELY(!success)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: failed to read merge block at %llu\n", ofs);
	}

	return(UNIV_LIKELY(success));
}

/********************************************************************//**
Write a merge block to the file system.
@return	TRUE if request was successful, FALSE if fail */
static
ibool
row_merge_write(
/*============*/
	int		fd,	/*!< in: file descriptor */
	ulint		offset,	/*!< in: offset where to write,
				in number of row_merge_block_t elements */
	const void*	buf)	/*!< in: data */
{
	size_t		buf_len = sizeof(row_merge_block_t);
	ib_uint64_t	ofs = buf_len * (ib_uint64_t) offset;
	ibool		ret;

	ret = os_file_write("(merge)", OS_FILE_FROM_FD(fd), buf,
			    (ulint) (ofs & 0xFFFFFFFF),
			    (ulint) (ofs >> 32),
			    buf_len);

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
static __attribute__((nonnull))
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
	ut_ad(b >= block[0]);
	ut_ad(b < block[1]);
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

		if (UNIV_UNLIKELY(b >= block[1])) {
			if (!row_merge_read(fd, ++(*foffs), block)) {
err_exit:
				/* Signal I/O error. */
				*mrec = b;
				return(NULL);
			}

			/* Wrap around to the beginning of the buffer. */
			b = block[0];
		}

		extra_size = (extra_size & 0x7f) << 8;
		extra_size |= *b++;
	}

	/* Normalize extra_size.  Above, value 0 signals "end of list". */
	extra_size--;

	/* Read the extra bytes. */

	if (UNIV_UNLIKELY(b + extra_size >= block[1])) {
		/* The record spans two blocks.  Copy the entire record
		to the auxiliary buffer and handle this as a special
		case. */

		avail_size = block[1] - b;

		memcpy(*buf, b, avail_size);

		if (!row_merge_read(fd, ++(*foffs), block)) {

			goto err_exit;
		}

		/* Wrap around to the beginning of the buffer. */
		b = block[0];

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
		ut_a(b + data_size < block[1]);

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

	if (UNIV_LIKELY(b < block[1])) {
		/* The record fits entirely in the block.
		This is the normal case. */
		goto func_exit;
	}

	/* The record spans two blocks.  Copy it to buf. */

	b -= extra_size + data_size;
	avail_size = block[1] - b;
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
	b = block[0];

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
	ut_ad(b >= block[0]);
	ut_ad(b < block[1]);
	ut_ad(mrec);
	ut_ad(foffs);
	ut_ad(mrec < block[0] || mrec > block[1]);
	ut_ad(mrec < buf[0] || mrec > buf[1]);

	/* Normalize extra_size.  Value 0 signals "end of list". */
	extra_size = rec_offs_extra_size(offsets) + 1;

	size = extra_size + (extra_size >= 0x80)
		+ rec_offs_data_size(offsets);

	if (UNIV_UNLIKELY(b + size >= block[1])) {
		/* The record spans two blocks.
		Copy it to the temporary buffer first. */
		avail_size = block[1] - b;

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

		UNIV_MEM_INVALID(block[0], sizeof block[0]);

		/* Copy the rest. */
		b = block[0];
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
	ut_ad(b >= block[0]);
	ut_ad(b < block[1]);
	ut_ad(foffs);
#ifdef UNIV_DEBUG
	if (row_merge_print_write) {
		fprintf(stderr, "row_merge_write %p,%p,%d,%lu EOF\n",
			(void*) b, (void*) block, fd, (ulong) *foffs);
	}
#endif /* UNIV_DEBUG */

	*b++ = 0;
	UNIV_MEM_ASSERT_RW(block[0], b - block[0]);
	UNIV_MEM_ASSERT_W(block[0], sizeof block[0]);
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0xff, block[1] - b);
#endif /* UNIV_DEBUG_VALGRIND */

	if (!row_merge_write(fd, (*foffs)++, block)) {
		return(NULL);
	}

	UNIV_MEM_INVALID(block[0], sizeof block[0]);
	return(block[0]);
}

/*************************************************************//**
Compare two merge records.
@return	1, 0, -1 if mrec1 is greater, equal, less, respectively, than mrec2 */
static
int
row_merge_cmp(
/*==========*/
	const mrec_t*		mrec1,		/*!< in: first merge
						record to be compared */
	const mrec_t*		mrec2,		/*!< in: second merge
						record to be compared */
	const ulint*		offsets1,	/*!< in: first record offsets */
	const ulint*		offsets2,	/*!< in: second record offsets */
	const dict_index_t*	index,		/*!< in: index */
	ibool*			null_eq)	/*!< out: set to TRUE if
						found matching null values */
{
	int	cmp;

	cmp = cmp_rec_rec_simple(mrec1, mrec2, offsets1, offsets2, index,
				 null_eq);

#ifdef UNIV_DEBUG
	if (row_merge_print_cmp) {
		fputs("row_merge_cmp1 ", stderr);
		rec_print_comp(stderr, mrec1, offsets1);
		fputs("\nrow_merge_cmp2 ", stderr);
		rec_print_comp(stderr, mrec2, offsets2);
		fprintf(stderr, "\nrow_merge_cmp=%d\n", cmp);
	}
#endif /* UNIV_DEBUG */

	return(cmp);
}

/********************************************************************//**
Reads clustered index of the table and create temporary files
containing the index entries for the indexes to be built.
@return	DB_SUCCESS or error */
static __attribute__((nonnull))
ulint
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
	dict_index_t**		index,	/*!< in: indexes to be created */
	merge_file_t*		files,	/*!< in: temporary files */
	ulint			n_index,/*!< in: number of indexes to create */
	row_merge_block_t*	block)	/*!< in/out: file buffer */
{
	dict_index_t*		clust_index;	/* Clustered index */
	mem_heap_t*		row_heap;	/* Heap memory to create
						clustered index records */
	row_merge_buf_t**	merge_buf;	/* Temporary list for records*/
	btr_pcur_t		pcur;		/* Persistent cursor on the
						clustered index */
	mtr_t			mtr;		/* Mini transaction */
	ulint			err = DB_SUCCESS;/* Return code */
	ulint			i;
	ulint			n_nonnull = 0;	/* number of columns
						changed to NOT NULL */
	ulint*			nonnull = NULL;	/* NOT NULL columns */

	trx->op_info = "reading clustered index";

	ut_ad(trx);
	ut_ad(old_table);
	ut_ad(new_table);
	ut_ad(index);
	ut_ad(files);

	/* Create and initialize memory for record buffers */

	merge_buf = mem_alloc(n_index * sizeof *merge_buf);

	for (i = 0; i < n_index; i++) {
		merge_buf[i] = row_merge_buf_create(index[i]);
	}

	mtr_start(&mtr);

	/* Find the clustered index and create a persistent cursor
	based on that. */

	clust_index = dict_table_get_first_index(old_table);

	btr_pcur_open_at_index_side(
		TRUE, clust_index, BTR_SEARCH_LEAF, &pcur, TRUE, &mtr);

	if (UNIV_UNLIKELY(old_table != new_table)) {
		ulint	n_cols = dict_table_get_n_cols(old_table);

		/* A primary key will be created.  Identify the
		columns that were flagged NOT NULL in the new table,
		so that we can quickly check that the records in the
		(old) clustered index do not violate the added NOT
		NULL constraints. */

		ut_a(n_cols == dict_table_get_n_cols(new_table));

		nonnull = mem_alloc(n_cols * sizeof *nonnull);

		for (i = 0; i < n_cols; i++) {
			if (dict_table_get_nth_col(old_table, i)->prtype
			    & DATA_NOT_NULL) {

				continue;
			}

			if (dict_table_get_nth_col(new_table, i)->prtype
			    & DATA_NOT_NULL) {

				nonnull[n_nonnull++] = i;
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
		dtuple_t*	row		= NULL;
		row_ext_t*	ext;
		ibool		has_next	= TRUE;

		btr_pcur_move_to_next_on_page(&pcur);

		/* When switching pages, commit the mini-transaction
		in order to release the latch on the old page. */

		if (btr_pcur_is_after_last_on_page(&pcur)) {
			if (UNIV_UNLIKELY(trx_is_interrupted(trx))) {
				i = 0;
				err = DB_INTERRUPTED;
				goto err_exit;
			}

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
			mtr_start(&mtr);
			/* Restore position on the record, or its
			predecessor if the record was purged
			meanwhile. */
			btr_pcur_restore_position(BTR_SEARCH_LEAF,
						  &pcur, &mtr);
			/* Move to the successor of the original record. */
			has_next = btr_pcur_move_to_next_user_rec(&pcur, &mtr);
		}

		if (UNIV_LIKELY(has_next)) {
			rec = btr_pcur_get_rec(&pcur);
			offsets = rec_get_offsets(rec, clust_index, NULL,
						  ULINT_UNDEFINED, &row_heap);

			/* Skip delete marked records. */
			if (rec_get_deleted_flag(
				    rec, dict_table_is_comp(old_table))) {
				continue;
			}

			srv_n_rows_inserted++;

			/* Build a row based on the clustered index. */

			row = row_build(ROW_COPY_POINTERS, clust_index,
					rec, offsets,
					new_table, &ext, row_heap);

			if (UNIV_LIKELY_NULL(nonnull)) {
				for (i = 0; i < n_nonnull; i++) {
					dfield_t*	field
						= &row->fields[nonnull[i]];
					dtype_t*	field_type
						= dfield_get_type(field);

					ut_a(!(field_type->prtype
					       & DATA_NOT_NULL));

					if (dfield_is_null(field)) {
						err = DB_PRIMARY_KEY_IS_NULL;
						i = 0;
						goto err_exit;
					}

					field_type->prtype |= DATA_NOT_NULL;
				}
			}
		}

		/* Build all entries for all the indexes to be created
		in a single scan of the clustered index. */

		for (i = 0; i < n_index; i++) {
			row_merge_buf_t*	buf	= merge_buf[i];
			merge_file_t*		file	= &files[i];
			const dict_index_t*	index	= buf->index;

			if (UNIV_LIKELY
			    (row && row_merge_buf_add(buf, row, ext))) {
				file->n_rec++;
				continue;
			}

			/* The buffer must be sufficiently large
			to hold at least one record. */
			ut_ad(buf->n_tuples || !has_next);

			/* We have enough data tuples to form a block.
			Sort them and write to disk. */

			if (buf->n_tuples) {
				if (dict_index_is_unique(index)) {
					row_merge_dup_t	dup;
					dup.index = buf->index;
					dup.table = table;
					dup.n_dup = 0;

					row_merge_buf_sort(buf, &dup);

					if (dup.n_dup) {
						err = DB_DUPLICATE_KEY;
err_exit:
						trx->error_key_num = i;
						goto func_exit;
					}
				} else {
					row_merge_buf_sort(buf, NULL);
				}
			}

			row_merge_buf_write(buf, file, block);

			if (!row_merge_write(file->fd, file->offset++,
					     block)) {
				err = DB_OUT_OF_FILE_SPACE;
				goto err_exit;
			}

			UNIV_MEM_INVALID(block[0], sizeof block[0]);
			merge_buf[i] = row_merge_buf_empty(buf);

			if (UNIV_LIKELY(row != NULL)) {
				/* Try writing the record again, now
				that the buffer has been written out
				and emptied. */

				if (UNIV_UNLIKELY
				    (!row_merge_buf_add(buf, row, ext))) {
					/* An empty buffer should have enough
					room for at least one record. */
					ut_error;
				}

				file->n_rec++;
			}
		}

		mem_heap_empty(row_heap);

		if (UNIV_UNLIKELY(!has_next)) {
			goto func_exit;
		}
	}

func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(row_heap);

	if (UNIV_LIKELY_NULL(nonnull)) {
		mem_free(nonnull);
	}

	for (i = 0; i < n_index; i++) {
		row_merge_buf_free(merge_buf[i]);
	}

	mem_free(merge_buf);

	trx->op_info = "";

	return(err);
}

/** Write a record via buffer 2 and read the next record to buffer N.
@param N	number of the buffer (0 or 1)
@param AT_END	statement to execute at end of input */
#define ROW_MERGE_WRITE_GET_NEXT(N, AT_END)				\
	do {								\
		b2 = row_merge_write_rec(&block[2], &buf[2], b2,	\
					 of->fd, &of->offset,		\
					 mrec##N, offsets##N);		\
		if (UNIV_UNLIKELY(!b2 || ++of->n_rec > file->n_rec)) {	\
			goto corrupt;					\
		}							\
		b##N = row_merge_read_rec(&block[N], &buf[N],		\
					  b##N, index,			\
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
static
ulint
row_merge_blocks(
/*=============*/
	const dict_index_t*	index,	/*!< in: index being created */
	const merge_file_t*	file,	/*!< in: file containing
					index entries */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	ulint*			foffs0,	/*!< in/out: offset of first
					source list in the file */
	ulint*			foffs1,	/*!< in/out: offset of second
					source list in the file */
	merge_file_t*		of,	/*!< in/out: output file */
	struct TABLE*		table)	/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
{
	mem_heap_t*	heap;	/*!< memory heap for offsets0, offsets1 */

	mrec_buf_t*	buf;	/*!< buffer for handling
				split mrec in block[] */
	const byte*	b0;	/*!< pointer to block[0] */
	const byte*	b1;	/*!< pointer to block[1] */
	byte*		b2;	/*!< pointer to block[2] */
	const mrec_t*	mrec0;	/*!< merge rec, points to block[0] or buf[0] */
	const mrec_t*	mrec1;	/*!< merge rec, points to block[1] or buf[1] */
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

	heap = row_merge_heap_create(index, &buf, &offsets0, &offsets1);

	/* Write a record and read the next record.  Split the output
	file in two halves, which can be merged on the following pass. */

	if (!row_merge_read(file->fd, *foffs0, &block[0])
	    || !row_merge_read(file->fd, *foffs1, &block[1])) {
corrupt:
		mem_heap_free(heap);
		return(DB_CORRUPTION);
	}

	b0 = block[0];
	b1 = block[1];
	b2 = block[2];

	b0 = row_merge_read_rec(&block[0], &buf[0], b0, index, file->fd,
				foffs0, &mrec0, offsets0);
	b1 = row_merge_read_rec(&block[1], &buf[1], b1, index, file->fd,
				foffs1, &mrec1, offsets1);
	if (UNIV_UNLIKELY(!b0 && mrec0)
	    || UNIV_UNLIKELY(!b1 && mrec1)) {

		goto corrupt;
	}

	while (mrec0 && mrec1) {
		ibool	null_eq = FALSE;
		switch (row_merge_cmp(mrec0, mrec1,
				      offsets0, offsets1, index,
				      &null_eq)) {
		case 0:
			if (UNIV_UNLIKELY
			    (dict_index_is_unique(index) && !null_eq)) {
				innobase_rec_to_mysql(table, mrec0,
						      index, offsets0);
				mem_heap_free(heap);
				return(DB_DUPLICATE_KEY);
			}
			/* fall through */
		case -1:
			ROW_MERGE_WRITE_GET_NEXT(0, goto merged);
			break;
		case 1:
			ROW_MERGE_WRITE_GET_NEXT(1, goto merged);
			break;
		default:
			ut_error;
		}

	}

merged:
	if (mrec0) {
		/* append all mrec0 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(0, goto done0);
		}
	}
done0:
	if (mrec1) {
		/* append all mrec1 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(1, goto done1);
		}
	}
done1:

	mem_heap_free(heap);
	b2 = row_merge_write_eof(&block[2], b2, of->fd, &of->offset);
	return(b2 ? DB_SUCCESS : DB_CORRUPTION);
}

/*************************************************************//**
Copy a block of index entries.
@return	TRUE on success, FALSE on failure */
static __attribute__((nonnull))
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
	byte*		b2;	/*!< pointer to block[2] */
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

	b0 = block[0];
	b2 = block[2];

	b0 = row_merge_read_rec(&block[0], &buf[0], b0, index, file->fd,
				foffs0, &mrec0, offsets0);
	if (UNIV_UNLIKELY(!b0 && mrec0)) {

		goto corrupt;
	}

	if (mrec0) {
		/* append all mrec0 to output */
		for (;;) {
			ROW_MERGE_WRITE_GET_NEXT(0, goto done0);
		}
	}
done0:

	/* The file offset points to the beginning of the last page
	that has been read.  Update it to point to the next block. */
	(*foffs0)++;

	mem_heap_free(heap);
	return(row_merge_write_eof(&block[2], b2, of->fd, &of->offset)
	       != NULL);
}

/*************************************************************//**
Merge disk files.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull))
ulint
row_merge(
/*======*/
	trx_t*			trx,	/*!< in: transaction */
	const dict_index_t*	index,	/*!< in: index being created */
	merge_file_t*		file,	/*!< in/out: file containing
					index entries */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	int*			tmpfd,	/*!< in/out: temporary file handle */
	struct TABLE*		table,	/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
	ulint*			num_run,/*!< in/out: Number of runs remain
					to be merged */
	ulint*			run_offset) /*!< in/out: Array contains the
					first offset number for each merge
					run */
{
	ulint		foffs0;	/*!< first input offset */
	ulint		foffs1;	/*!< second input offset */
	ulint		error;	/*!< error code */
	merge_file_t	of;	/*!< output file */
	const ulint	ihalf	= run_offset[*num_run / 2];
				/*!< half the input file */
	ulint		n_run	= 0;
				/*!< num of runs generated from this merge */


	UNIV_MEM_ASSERT_W(block[0], 3 * sizeof block[0]);

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

		if (UNIV_UNLIKELY(trx_is_interrupted(trx))) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		error = row_merge_blocks(index, file, block,
					 &foffs0, &foffs1, &of, table);

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

		if (!row_merge_blocks_copy(index, file, block, &foffs0, &of)) {
			return(DB_CORRUPTION);
		}
	}

	ut_ad(foffs0 == ihalf);

	while (foffs1 < file->offset) {
		if (UNIV_UNLIKELY(trx_is_interrupted(trx))) {
			return(DB_INTERRUPTED);
		}

		/* Remember the offset number for this run */
		run_offset[n_run++] = of.offset;

		if (!row_merge_blocks_copy(index, file, block, &foffs1, &of)) {
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

	UNIV_MEM_INVALID(block[0], 3 * sizeof block[0]);

	return(DB_SUCCESS);
}

/*************************************************************//**
Merge disk files.
@return	DB_SUCCESS or error code */
static
ulint
row_merge_sort(
/*===========*/
	trx_t*			trx,	/*!< in: transaction */
	const dict_index_t*	index,	/*!< in: index being created */
	merge_file_t*		file,	/*!< in/out: file containing
					index entries */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	int*			tmpfd,	/*!< in/out: temporary file handle */
	struct TABLE*		table)	/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
{
	ulint	half = file->offset / 2;
	ulint	num_runs;
	ulint*	run_offset;
	ulint	error = DB_SUCCESS;

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
		error = row_merge(trx, index, file, block, tmpfd,
				  table, &num_runs, run_offset);

		UNIV_MEM_ASSERT_RW(run_offset, num_runs * sizeof *run_offset);

		if (error != DB_SUCCESS) {
			break;
		}
	} while (num_runs > 1);

	mem_free(run_offset);

	return(error);
}

/*************************************************************//**
Copy externally stored columns to the data tuple. */
static
void
row_merge_copy_blobs(
/*=================*/
	const mrec_t*	mrec,	/*!< in: merge record */
	const ulint*	offsets,/*!< in: offsets of mrec */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	dtuple_t*	tuple,	/*!< in/out: data tuple */
	mem_heap_t*	heap)	/*!< in/out: memory heap */
{
	ulint	i;
	ulint	n_fields = dtuple_get_n_fields(tuple);

	for (i = 0; i < n_fields; i++) {
		ulint		len;
		const void*	data;
		dfield_t*	field = dtuple_get_nth_field(tuple, i);

		if (!dfield_is_ext(field)) {
			continue;
		}

		ut_ad(!dfield_is_null(field));

		/* The table is locked during index creation.
		Therefore, externally stored columns cannot possibly
		be freed between the time the BLOB pointers are read
		(row_merge_read_clustered_index()) and dereferenced
		(below). */
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
static
ulint
row_merge_insert_index_tuples(
/*==========================*/
	trx_t*			trx,	/*!< in: transaction */
	dict_index_t*		index,	/*!< in: index */
	dict_table_t*		table,	/*!< in: new table */
	ulint			zip_size,/*!< in: compressed page size of
					 the old table, or 0 if uncompressed */
	int			fd,	/*!< in: file descriptor */
	row_merge_block_t*	block)	/*!< in/out: file buffer */
{
	const byte*		b;
	que_thr_t*		thr;
	ins_node_t*		node;
	mem_heap_t*		tuple_heap;
	mem_heap_t*		graph_heap;
	ulint			error = DB_SUCCESS;
	ulint			foffs = 0;
	ulint*			offsets;

	ut_ad(trx);
	ut_ad(index);
	ut_ad(table);

	/* We use the insert query graph as the dummy graph
	needed in the row module call */

	trx->op_info = "inserting index entries";

	graph_heap = mem_heap_create(500 + sizeof(mrec_buf_t));
	node = ins_node_create(INS_DIRECT, table, graph_heap);

	thr = pars_complete_graph_for_exec(node, trx, graph_heap);

	que_thr_move_to_run_state_for_mysql(thr, trx);

	tuple_heap = mem_heap_create(1000);

	{
		ulint i	= 1 + REC_OFFS_HEADER_SIZE
			+ dict_index_get_n_fields(index);
		offsets = mem_heap_alloc(graph_heap, i * sizeof *offsets);
		offsets[0] = i;
		offsets[1] = dict_index_get_n_fields(index);
	}

	b = *block;

	if (!row_merge_read(fd, foffs, block)) {
		error = DB_CORRUPTION;
	} else {
		mrec_buf_t*	buf = mem_heap_alloc(graph_heap, sizeof *buf);

		for (;;) {
			const mrec_t*	mrec;
			dtuple_t*	dtuple;
			ulint		n_ext;

			b = row_merge_read_rec(block, buf, b, index,
					       fd, &foffs, &mrec, offsets);
			if (UNIV_UNLIKELY(!b)) {
				/* End of list, or I/O error */
				if (mrec) {
					error = DB_CORRUPTION;
				}
				break;
			}

			dtuple = row_rec_to_index_entry_low(
				mrec, index, offsets, &n_ext, tuple_heap);

			if (UNIV_UNLIKELY(n_ext)) {
				row_merge_copy_blobs(mrec, offsets, zip_size,
						     dtuple, tuple_heap);
			}

			node->row = dtuple;
			node->table = table;
			node->trx_id = trx->id;

			ut_ad(dtuple_validate(dtuple));

			do {
				thr->run_node = thr;
				thr->prev_node = thr->common.parent;

				error = row_ins_index_entry(index, dtuple,
							    0, FALSE, thr);

				if (UNIV_LIKELY(error == DB_SUCCESS)) {

					goto next_rec;
				}

				thr->lock_state = QUE_THR_LOCK_ROW;
				trx->error_state = error;
				que_thr_stop_for_mysql(thr);
				thr->lock_state = QUE_THR_LOCK_NOLOCK;
			} while (row_mysql_handle_errors(&error, trx,
							 thr, NULL));

			goto err_exit;
next_rec:
			mem_heap_empty(tuple_heap);
		}
	}

	que_thr_stop_for_mysql_no_error(thr, trx);
err_exit:
	que_graph_free(thr->graph);

	trx->op_info = "";

	mem_heap_free(tuple_heap);

	return(error);
}

/*********************************************************************//**
Sets an exclusive lock on a table, for the duration of creating indexes.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_merge_lock_table(
/*=================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode)		/*!< in: LOCK_X or LOCK_S */
{
	mem_heap_t*	heap;
	que_thr_t*	thr;
	ulint		err;
	sel_node_t*	node;

	ut_ad(trx);
	ut_ad(mode == LOCK_X || mode == LOCK_S);

	heap = mem_heap_create(512);

	trx->op_info = "setting table lock for creating or dropping index";

	node = sel_node_create(heap);
	thr = pars_complete_graph_for_exec(node, trx, heap);
	thr->graph->state = QUE_FORK_ACTIVE;

	/* We use the select query graph as the dummy graph needed
	in the lock module call */

	thr = que_fork_get_first_thr(que_node_get_parent(thr));
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
			ibool	was_lock_wait;

			was_lock_wait = row_mysql_handle_errors(
				&err, trx, thr, NULL);

			if (was_lock_wait) {
				goto run_again;
			}
		} else {
			que_thr_t*	run_thr;
			que_node_t*	parent;

			parent = que_node_get_parent(thr);
			run_thr = que_fork_start_command(parent);

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
Drop an index from the InnoDB system tables.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed. */
UNIV_INTERN
void
row_merge_drop_index(
/*=================*/
	dict_index_t*	index,	/*!< in: index to be removed */
	dict_table_t*	table,	/*!< in: table */
	trx_t*		trx)	/*!< in: transaction handle */
{
	ulint		err;
	pars_info_t*	info = pars_info_create();

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in deleting the dictionary data from system
	tables in Innobase. Deleting a row from SYS_INDEXES table also
	frees the file segments of the B-tree associated with the index. */

	static const char sql[] =
		"PROCEDURE DROP_INDEX_PROC () IS\n"
		"BEGIN\n"
		/* Rename the index, so that it will be dropped by
		row_merge_drop_temp_indexes() at crash recovery
		if the server crashes before this trx is committed. */
		"UPDATE SYS_INDEXES SET NAME=CONCAT('"
		TEMP_INDEX_PREFIX_STR "', NAME) WHERE ID = :indexid;\n"
		"COMMIT WORK;\n"
		/* Drop the field definitions of the index. */
		"DELETE FROM SYS_FIELDS WHERE INDEX_ID = :indexid;\n"
		/* Drop the index definition and the B-tree. */
		"DELETE FROM SYS_INDEXES WHERE ID = :indexid;\n"
		"END;\n";

	ut_ad(index && table && trx);

	pars_info_add_ull_literal(info, "indexid", index->id);

	trx_start_if_not_started(trx);
	trx->op_info = "dropping index";

	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

	err = que_eval_sql(info, sql, FALSE, trx);


	if (err != DB_SUCCESS) {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_TRANSACTIONS. */
		trx->error_state = DB_SUCCESS;

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: row_merge_drop_index failed "
			"with error code: %lu.\n", (ulint) err);
	}

	/* Replace this index with another equivalent index for all
	foreign key constraints on this table where this index is used */

	dict_table_replace_index_in_foreign_list(table, index, trx);
	dict_index_remove_from_cache(table, index);

	trx->op_info = "";
}

/*********************************************************************//**
Drop those indexes which were created before an error occurred when
building an index.  The data dictionary must have been locked
exclusively by the caller, because the transaction will not be
committed. */
UNIV_INTERN
void
row_merge_drop_indexes(
/*===================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table,		/*!< in: table containing the indexes */
	dict_index_t**	index,		/*!< in: indexes to drop */
	ulint		num_created)	/*!< in: number of elements in index[] */
{
	ulint	key_num;

	for (key_num = 0; key_num < num_created; key_num++) {
		row_merge_drop_index(index[key_num], table, trx);
	}
}

/*********************************************************************//**
Drop all partially created indexes during crash recovery. */
UNIV_INTERN
void
row_merge_drop_temp_indexes(void)
/*=============================*/
{
	trx_t*		trx;
	btr_pcur_t	pcur;
	mtr_t		mtr;

	/* Load the table definitions that contain partially defined
	indexes, so that the data dictionary information can be checked
	when accessing the tablename.ibd files. */
	trx = trx_allocate_for_background();
	trx->op_info = "dropping partially created indexes";
	row_mysql_lock_data_dictionary(trx);

	mtr_start(&mtr);

	btr_pcur_open_at_index_side(
		TRUE,
		dict_table_get_first_index(dict_sys->sys_indexes),
		BTR_SEARCH_LEAF, &pcur, TRUE, &mtr);

	for (;;) {
		const rec_t*	rec;
		const byte*	field;
		ulint		len;
		table_id_t	table_id;
		dict_table_t*	table;

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);

		if (!btr_pcur_is_on_user_rec(&pcur)) {
			break;
		}

		rec = btr_pcur_get_rec(&pcur);
		field = rec_get_nth_field_old(rec, DICT_SYS_INDEXES_NAME_FIELD,
					      &len);
		if (len == UNIV_SQL_NULL || len == 0
		    || (char) *field != TEMP_INDEX_PREFIX) {
			continue;
		}

		/* This is a temporary index. */

		field = rec_get_nth_field_old(rec, 0/*TABLE_ID*/, &len);
		if (len != 8) {
			/* Corrupted TABLE_ID */
			continue;
		}

		table_id = mach_read_from_8(field);

		btr_pcur_store_position(&pcur, &mtr);
		btr_pcur_commit_specify_mtr(&pcur, &mtr);

		table = dict_table_get_on_id_low(table_id);

		if (table) {
			dict_index_t*	index;
			dict_index_t*	next_index;

			for (index = dict_table_get_first_index(table);
			     index; index = next_index) {

				next_index = dict_table_get_next_index(index);

				if (*index->name == TEMP_INDEX_PREFIX) {
					row_merge_drop_index(index, table, trx);
					trx_commit_for_mysql(trx);
				}
			}
		}

		mtr_start(&mtr);
		btr_pcur_restore_position(BTR_SEARCH_LEAF,
					  &pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	row_mysql_unlock_data_dictionary(trx);
	trx_free_for_background(trx);
}

/*********************************************************************//**
Creates temperary merge files, and if UNIV_PFS_IO defined, register
the file descriptor with Performance Schema.
@return File descriptor */
UNIV_INLINE
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
static
void
row_merge_file_create(
/*==================*/
	merge_file_t*	merge_file)	/*!< out: merge file structure */
{
	merge_file->fd = row_merge_file_create_low();
	merge_file->offset = 0;
	merge_file->n_rec = 0;
}

/*********************************************************************//**
Destroy a merge file. And de-register the file from Performance Schema
if UNIV_PFS_IO is defined. */
UNIV_INLINE
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
static
void
row_merge_file_destroy(
/*===================*/
	merge_file_t*	merge_file)	/*!< out: merge file structure */
{
	if (merge_file->fd != -1) {
		row_merge_file_destroy_low(merge_file->fd);
		merge_file->fd = -1;
	}
}

/*********************************************************************//**
Determine the precise type of a column that is added to a tem
if a column must be constrained NOT NULL.
@return	col->prtype, possibly ORed with DATA_NOT_NULL */
UNIV_INLINE
ulint
row_merge_col_prtype(
/*=================*/
	const dict_col_t*	col,		/*!< in: column */
	const char*		col_name,	/*!< in: name of the column */
	const merge_index_def_t*index_def)	/*!< in: the index definition
						of the primary key */
{
	ulint	prtype = col->prtype;
	ulint	i;

	ut_ad(index_def->ind_type & DICT_CLUSTERED);

	if (prtype & DATA_NOT_NULL) {

		return(prtype);
	}

	/* All columns that are included
	in the PRIMARY KEY must be NOT NULL. */

	for (i = 0; i < index_def->n_fields; i++) {
		if (!strcmp(col_name, index_def->fields[i].field_name)) {
			return(prtype | DATA_NOT_NULL);
		}
	}

	return(prtype);
}

/*********************************************************************//**
Create a temporary table for creating a primary key, using the definition
of an existing table.
@return	table, or NULL on error */
UNIV_INTERN
dict_table_t*
row_merge_create_temporary_table(
/*=============================*/
	const char*		table_name,	/*!< in: new table name */
	const merge_index_def_t*index_def,	/*!< in: the index definition
						of the primary key */
	const dict_table_t*	table,		/*!< in: old table definition */
	trx_t*			trx)		/*!< in/out: transaction
						(sets error_state) */
{
	ulint		i;
	dict_table_t*	new_table = NULL;
	ulint		n_cols = dict_table_get_n_user_cols(table);
	ulint		error;
	mem_heap_t*	heap = mem_heap_create(1000);

	ut_ad(table_name);
	ut_ad(index_def);
	ut_ad(table);
	ut_ad(mutex_own(&dict_sys->mutex));

	new_table = dict_mem_table_create(table_name, 0, n_cols, table->flags);

	for (i = 0; i < n_cols; i++) {
		const dict_col_t*	col;
		const char*		col_name;

		col = dict_table_get_nth_col(table, i);
		col_name = dict_table_get_col_name(table, i);

		dict_mem_table_add_col(new_table, heap, col_name, col->mtype,
				       row_merge_col_prtype(col, col_name,
							    index_def),
				       col->len);
	}

	error = row_create_table_for_mysql(new_table, trx);
	mem_heap_free(heap);

	if (error != DB_SUCCESS) {
		trx->error_state = error;
		new_table = NULL;
	}

	return(new_table);
}

/*********************************************************************//**
Rename the temporary indexes in the dictionary to permanent ones.  The
data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed.
@return	DB_SUCCESS if all OK */
UNIV_INTERN
ulint
row_merge_rename_indexes(
/*=====================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table)		/*!< in/out: table with new indexes */
{
	ulint		err = DB_SUCCESS;
	pars_info_t*	info = pars_info_create();

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in renaming indexes. */

	static const char sql[] =
		"PROCEDURE RENAME_INDEXES_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES SET NAME=SUBSTR(NAME,1,LENGTH(NAME)-1)\n"
		"WHERE TABLE_ID = :tableid AND SUBSTR(NAME,0,1)='"
		TEMP_INDEX_PREFIX_STR "';\n"
		"END;\n";

	ut_ad(table);
	ut_ad(trx);
	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

	trx->op_info = "renaming indexes";

	pars_info_add_ull_literal(info, "tableid", table->id);

	err = que_eval_sql(info, sql, FALSE, trx);

	if (err == DB_SUCCESS) {
		dict_index_t*	index = dict_table_get_first_index(table);
		do {
			if (*index->name == TEMP_INDEX_PREFIX) {
				index->name++;
			}
			index = dict_table_get_next_index(index);
		} while (index);
	} else {
		/* Even though we ensure that DDL transactions are WAIT
		and DEADLOCK free, we could encounter other errors e.g.,
		DB_TOO_MANY_TRANSACTIONS. */
		trx->error_state = DB_SUCCESS;

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: row_merge_rename_indexes "
			"failed with error code: %lu.\n", (ulint) err);
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Rename the tables in the data dictionary.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_merge_rename_tables(
/*====================*/
	dict_table_t*	old_table,	/*!< in/out: old table, renamed to
					tmp_name */
	dict_table_t*	new_table,	/*!< in/out: new table, renamed to
					old_table->name */
	const char*	tmp_name,	/*!< in: new name for old_table */
	trx_t*		trx)		/*!< in: transaction handle */
{
	ulint		err	= DB_ERROR;
	pars_info_t*	info;
	char		old_name[MAX_FULL_NAME_LEN + 1];

	ut_ad(old_table != new_table);
	ut_ad(mutex_own(&dict_sys->mutex));

	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

	/* store the old/current name to an automatic variable */
	if (strlen(old_table->name) + 1 <= sizeof(old_name)) {
		memcpy(old_name, old_table->name, strlen(old_table->name) + 1);
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: too long table name: '%s', "
			"max length is %d\n", old_table->name,
			MAX_FULL_NAME_LEN);
		ut_error;
	}

	trx->op_info = "renaming tables";

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

	if (err != DB_SUCCESS) {

		goto err_exit;
	}

	/* The following calls will also rename the .ibd data files if
	the tables are stored in a single-table tablespace */

	if (!dict_table_rename_in_cache(old_table, tmp_name, FALSE)
	    || !dict_table_rename_in_cache(new_table, old_name, FALSE)) {

		err = DB_ERROR;
		goto err_exit;
	}

	err = dict_load_foreigns(old_name, FALSE, TRUE);

	if (err != DB_SUCCESS) {
err_exit:
		trx->error_state = DB_SUCCESS;
		trx_general_rollback_for_mysql(trx, NULL);
		trx->error_state = DB_SUCCESS;
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Create and execute a query graph for creating an index.
@return	DB_SUCCESS or error code */
static
ulint
row_merge_create_index_graph(
/*=========================*/
	trx_t*		trx,		/*!< in: trx */
	dict_table_t*	table,		/*!< in: table */
	dict_index_t*	index)		/*!< in: index */
{
	ind_node_t*	node;		/*!< Index creation node */
	mem_heap_t*	heap;		/*!< Memory heap */
	que_thr_t*	thr;		/*!< Query thread */
	ulint		err;

	ut_ad(trx);
	ut_ad(table);
	ut_ad(index);

	heap = mem_heap_create(512);

	index->table = table;
	node = ind_create_graph_create(index, heap);
	thr = pars_complete_graph_for_exec(node, trx, heap);

	ut_a(thr == que_fork_start_command(que_node_get_parent(thr)));

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
	const merge_index_def_t*index_def)
					/*!< in: the index definition */
{
	dict_index_t*	index;
	ulint		err;
	ulint		n_fields = index_def->n_fields;
	ulint		i;

	/* Create the index prototype, using the passed in def, this is not
	a persistent operation. We pass 0 as the space id, and determine at
	a lower level the space id where to store the table. */

	index = dict_mem_index_create(table->name, index_def->name,
				      0, index_def->ind_type, n_fields);

	ut_a(index);

	for (i = 0; i < n_fields; i++) {
		merge_index_field_t*	ifield = &index_def->fields[i];

		dict_mem_index_add_field(index, ifield->field_name,
					 ifield->prefix_len);
	}

	/* Add the index to SYS_INDEXES, using the index prototype. */
	err = row_merge_create_index_graph(trx, table, index);

	if (err == DB_SUCCESS) {

		index = row_merge_dict_table_get_index(
			table, index_def);

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
	return(!dict_index_is_corrupted(index)
	       && (!trx->read_view
	           || read_view_sees_trx_id(trx->read_view, index->trx_id)));
}

/*********************************************************************//**
Drop the old table.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_merge_drop_table(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table)		/*!< in: table to drop */
{
	/* There must be no open transactions on the table. */
	ut_a(table->n_mysql_handles_opened == 0);

	return(row_drop_table_for_mysql(table->name, trx, FALSE));
}

/*********************************************************************//**
Build indexes on a table by reading a clustered index,
creating a temporary file containing index entries, merge sorting
these index entries and inserting sorted index entries to indexes.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_merge_build_indexes(
/*====================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	old_table,	/*!< in: table where rows are
					read from */
	dict_table_t*	new_table,	/*!< in: table where indexes are
					created; identical to old_table
					unless creating a PRIMARY KEY */
	dict_index_t**	indexes,	/*!< in: indexes to be created */
	ulint		n_indexes,	/*!< in: size of indexes[] */
	struct TABLE*	table)		/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
{
	merge_file_t*		merge_files;
	row_merge_block_t*	block;
	ulint			block_size;
	ulint			i;
	ulint			error;
	int			tmpfd;

	ut_ad(trx);
	ut_ad(old_table);
	ut_ad(new_table);
	ut_ad(indexes);
	ut_ad(n_indexes);

	trx_start_if_not_started(trx);

	/* Allocate memory for merge file data structure and initialize
	fields */

	merge_files = mem_alloc(n_indexes * sizeof *merge_files);
	block_size = 3 * sizeof *block;
	block = os_mem_alloc_large(&block_size);

	for (i = 0; i < n_indexes; i++) {

		row_merge_file_create(&merge_files[i]);
	}

	tmpfd = row_merge_file_create_low();

	/* Reset the MySQL row buffer that is used when reporting
	duplicate keys. */
	innobase_rec_reset(table);

	/* Read clustered index of the table and create files for
	secondary index entries for merge sort */

	error = row_merge_read_clustered_index(
		trx, table, old_table, new_table, indexes,
		merge_files, n_indexes, block);

	if (error != DB_SUCCESS) {

		goto func_exit;
	}

	/* Now we have files containing index entries ready for
	sorting and inserting. */

	for (i = 0; i < n_indexes; i++) {
		error = row_merge_sort(trx, indexes[i], &merge_files[i],
				       block, &tmpfd, table);

		if (error == DB_SUCCESS) {
			error = row_merge_insert_index_tuples(
				trx, indexes[i], new_table,
				dict_table_zip_size(old_table),
				merge_files[i].fd, block);
		}

		/* Close the temporary file to free up space. */
		row_merge_file_destroy(&merge_files[i]);

		if (error != DB_SUCCESS) {
			trx->error_key_num = i;
			goto func_exit;
		}
	}

func_exit:
	row_merge_file_destroy_low(tmpfd);

	for (i = 0; i < n_indexes; i++) {
		row_merge_file_destroy(&merge_files[i]);
	}

	mem_free(merge_files);
	os_mem_free_large(block, block_size);

	return(error);
}
