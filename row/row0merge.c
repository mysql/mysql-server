/******************************************************
New index creation routines using a merge sort

(c) 2005,2007 Innobase Oy

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

/* Block size for I/O operations in merge sort */

typedef byte	row_merge_block_t[1048576];

/* Secondary buffer for I/O operations of merge records */

typedef byte	mrec_buf_t[UNIV_PAGE_SIZE / 2];

/* Merge record in row_merge_block_t.  The format is the same as a
record in ROW_FORMAT=COMPACT with the exception that the
REC_N_NEW_EXTRA_BYTES are omitted. */
typedef byte	mrec_t;

/* Buffer for sorting in main memory. */
struct row_merge_buf_struct {
	mem_heap_t*	heap;		/* memory heap where allocated */
	dict_index_t*	index;		/* the index the tuples belong to */
	ulint		total_size;	/* total amount of data bytes */
	ulint		n_tuples;	/* number of data tuples */
	ulint		max_tuples;	/* maximum number of data tuples */
	const dfield_t**tuples;		/* array of pointers to
					arrays of fields that form
					the data tuples */
	const dfield_t**tmp_tuples;	/* temporary copy of tuples,
					for sorting */
};

typedef struct row_merge_buf_struct row_merge_buf_t;

/* Information about temporary files used in merge sort are stored
to this structure */

struct merge_file_struct {
	int	fd;		/* File descriptor */
	ulint	offset;		/* File offset */
};

typedef struct merge_file_struct merge_file_t;

/**********************************************************
Allocate a sort buffer. */
static
row_merge_buf_t*
row_merge_buf_create_low(
/*=====================*/
					/* out,own: sort buffer */
	mem_heap_t*	heap,		/* in: heap where allocated */
	dict_index_t*	index,		/* in: secondary index */
	ulint		buf_size,	/* in: size of the buffer, in bytes */
	ulint		max_tuples)	/* in: maximum number of data tuples */
{
	row_merge_buf_t*	buf;

	buf = mem_heap_calloc(heap, buf_size);
	buf->heap = heap;
	buf->index = index;
	buf->max_tuples = max_tuples;
	buf->tuples = mem_heap_alloc(heap,
				     2 * max_tuples * sizeof *buf->tuples);
	buf->tmp_tuples = buf->tuples + max_tuples;

	return(buf);
}

/**********************************************************
Allocate a sort buffer. */
static
row_merge_buf_t*
row_merge_buf_create(
/*=================*/
				/* out,own: sort buffer */
	dict_index_t*	index)	/* in: secondary index */
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

/**********************************************************
Empty a sort buffer. */
static
void
row_merge_buf_empty(
/*================*/
	row_merge_buf_t*	buf)	/* in/out: sort buffer */
{
	ulint		buf_size;
	ulint		max_tuples	= buf->max_tuples;
	mem_heap_t*	heap		= buf->heap;
	dict_index_t*	index		= buf->index;

	buf_size = (sizeof *buf) + (max_tuples - 1) * sizeof *buf->tuples;

	mem_heap_empty(heap);

	buf = row_merge_buf_create_low(heap, index, max_tuples, buf_size);
}

/**********************************************************
Deallocate a sort buffer. */
static
void
row_merge_buf_free(
/*===============*/
	row_merge_buf_t*	buf)	/* in,own: sort buffer, to be freed */
{
	mem_heap_free(buf->heap);
}

/**********************************************************
Insert a data tuple into a sort buffer. */
static
ibool
row_merge_buf_add(
/*==============*/
					/* out: TRUE if added,
					FALSE if out of space */
	row_merge_buf_t*	buf,	/* in/out: sort buffer */
	const dtuple_t*		row,	/* in: row in clustered index */
	row_ext_t*		ext)	/* in/out: cache of externally stored
					column prefixes, or NULL */
{
	ulint		i;
	ulint		j;
	ulint		n_fields;
	ulint		data_size;
	ulint		extra_size;
	dict_index_t*	index;
	dfield_t*	entry;
	dfield_t*	field;

	if (buf->n_tuples >= buf->max_tuples) {
		return(FALSE);
	}

	index = buf->index;

	n_fields = dict_index_get_n_fields(index);

	entry = mem_heap_alloc(buf->heap, n_fields * sizeof *entry);
	buf->tuples[buf->n_tuples] = entry;
	field = entry;

	data_size = 0;
	extra_size = UT_BITS_IN_BYTES(index->n_nullable);

	for (i = j = 0; i < n_fields; i++, field++) {
		dict_field_t*		ifield;
		const dict_col_t*	col;
		ulint			col_no;
		const dfield_t*		row_field;

		ifield = dict_index_get_nth_field(index, i);
		col = ifield->col;
		col_no = dict_col_get_no(col);
		row_field = dtuple_get_nth_field(row, col_no);
		dfield_copy(field, row_field);

		if (dfield_is_null(field)) {
			ut_ad(!(col->prtype & DATA_NOT_NULL));
			field->data = NULL;
			continue;
		} else if (UNIV_LIKELY(!ext)) {
		} else if (dict_index_is_clust(index)) {
			/* Flag externally stored fields. */
			if (j < ext->n_ext && col_no == ext->ext[j]) {
				j++;

				ut_a(field->len >= BTR_EXTERN_FIELD_REF_SIZE);
				dfield_set_ext(field);
			}
		} else {
			ulint	len = field->len;
			byte*	buf = row_ext_lookup(ext, col_no,
						     row_field->data,
						     row_field->len,
						     &len);
			if (UNIV_LIKELY_NULL(buf)) {
				dfield_set_data(field, buf, len);
			}
		}

		/* If a column prefix index, take only the prefix */

		if (ifield->prefix_len) {
			field->len = dtype_get_at_most_n_mbchars(
				col->prtype,
				col->mbminlen, col->mbmaxlen,
				ifield->prefix_len,
				field->len, field->data);
		}

		ut_ad(field->len <= col->len || col->mtype == DATA_BLOB);

		if (ifield->fixed_len) {
			ut_ad(field->len == ifield->fixed_len);
			ut_ad(!dfield_is_ext(field));
		} else if (dfield_is_ext(field)) {
			extra_size += 2;
		} else if (field->len < 128
			   || (col->len < 256 && col->mtype != DATA_BLOB)) {
			extra_size++;
		} else {
			extra_size += 2;
		}
		data_size += field->len;
	}

	ut_ad(!ext || !dict_index_is_clust(index) || j == ext->n_ext);

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

	/* Reserve one byte for the end marker of row_merge_block_t. */
	if (buf->total_size + data_size >= sizeof(row_merge_block_t) - 1) {
		return(FALSE);
	}

	buf->total_size += data_size;
	buf->n_tuples++;

	field = entry;

	/* Copy the data fields. */
	for (i = 0; i < n_fields; i++, field++) {
		if (!dfield_is_null(field)) {
			field->data = mem_heap_dup(buf->heap,
						   field->data, field->len);
		}
	}

	return(TRUE);
}

/*****************************************************************
Compare two tuples. */
static
int
row_merge_tuple_cmp(
/*================*/
					/* out: 1, 0, -1 if a is greater,
					equal, less, respectively, than b */
	ulint			n_field,/* in: number of fields */
	ulint*			n_dup,	/* in/out: number of duplicates */
	const dfield_t*		a,	/* in: first tuple to be compared */
	const dfield_t*		b)	/* in: second tuple to be compared */
{
	int	cmp;

	do {
		cmp = cmp_dfield_dfield(a++, b++);
	} while (!cmp && --n_field);

	if (!cmp) {
		(*n_dup)++;
	}

	return(cmp);
}

/**************************************************************************
Merge sort the tuple buffer in main memory. */
static
void
row_merge_tuple_sort(
/*=================*/
	ulint			n_field,/* in: number of fields */
	ulint*			n_dup,	/* in/out: number of duplicates */
	const dfield_t**	tuples,	/* in/out: tuples */
	const dfield_t**	aux,	/* in/out: work area */
	ulint			low,	/* in: lower bound of the
					sorting area, inclusive */
	ulint			high)	/* in: upper bound of the
					sorting area, exclusive */
{
#define row_merge_tuple_sort_ctx(a,b,c,d) \
	row_merge_tuple_sort(n_field, n_dup, a, b, c, d)
#define row_merge_tuple_cmp_ctx(a,b) row_merge_tuple_cmp(n_field, n_dup, a, b)

	UT_SORT_FUNCTION_BODY(row_merge_tuple_sort_ctx,
			      tuples, aux, low, high, row_merge_tuple_cmp_ctx);
}

/**********************************************************
Sort a buffer. */
static
ulint
row_merge_buf_sort(
/*===============*/
					/* out: number of duplicates
					encountered */
	row_merge_buf_t*	buf)	/* in/out: sort buffer */
{
	ulint	n_dup	= 0;

	row_merge_tuple_sort(dict_index_get_n_unique(buf->index), &n_dup,
			     buf->tuples, buf->tmp_tuples, 0, buf->n_tuples);

	return(n_dup);
}

/**********************************************************
Write a buffer to a block. */
static
void
row_merge_buf_write(
/*================*/
	const row_merge_buf_t*	buf,	/* in: sorted buffer */
	row_merge_block_t*	block)	/* out: buffer for writing to file */
{
	dict_index_t*	index	= buf->index;
	ulint		n_fields= dict_index_get_n_fields(index);
	byte*		b	= &(*block)[0];

	ulint		i;

	for (i = 0; i < buf->n_tuples; i++) {
		ulint		size;
		ulint		extra_size;
		const dfield_t*	entry		= buf->tuples[i];

		size = rec_get_converted_size_comp(buf->index,
						   REC_STATUS_ORDINARY,
						   entry, n_fields,
						   &extra_size);
		ut_ad(size > extra_size);
		ut_ad(extra_size >= REC_N_NEW_EXTRA_BYTES);
		extra_size -= REC_N_NEW_EXTRA_BYTES;
		size -= REC_N_NEW_EXTRA_BYTES;

		/* Encode extra_size + 1 */
		if (extra_size + 1 < 0x80) {
			*b++ = extra_size + 1;
		} else {
			ut_ad((extra_size + 1) < 0x8000);
			*b++ = 0x80 | ((extra_size + 1) >> 8);
			*b++ = (byte) (extra_size + 1);
		}

		ut_ad(b + size < block[1]);

		rec_convert_dtuple_to_rec_comp(b + extra_size, 0, index,
					       REC_STATUS_ORDINARY,
					       entry, n_fields);

		b += size;
	}

	/* Write an "end-of-chunk" marker. */
	ut_a(b < block[1]);
	ut_a(b == block[0] + buf->total_size);
	*b++ = 0;
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0, block[1] - b);
#endif /* UNIV_DEBUG_VALGRIND */
}

/**********************************************************
Create a memory heap and allocate space for row_merge_rec_offsets(). */
static
mem_heap_t*
row_merge_heap_create(
/*==================*/
					/* out: memory heap */
	dict_index_t*	index,		/* in: record descriptor */
	ulint**		offsets1,	/* out: offsets */
	ulint**		offsets2)	/* out: offsets */
{
	ulint		i	= 1 + REC_OFFS_HEADER_SIZE
		+ dict_index_get_n_fields(index);
	mem_heap_t*	heap	= mem_heap_create(2 * i * sizeof *offsets1);

	*offsets1 = mem_heap_alloc(heap, i * sizeof *offsets1);
	*offsets2 = mem_heap_alloc(heap, i * sizeof *offsets2);

	(*offsets1)[0] = (*offsets2)[0] = i;
	(*offsets1)[1] = (*offsets2)[1] = dict_index_get_n_fields(index);

	return(heap);
}

/**************************************************************************
Search an index object by name and column names.  If several indexes match,
return the index with the max id. */
static
dict_index_t*
row_merge_dict_table_get_index(
/*===========================*/
						/* out: matching index,
						NULL if not found */
	dict_table_t*		table,		/* in: table */
	const merge_index_def_t*index_def)	/* in: index definition */
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

	mem_free(column_names);

	return(index);
}

/************************************************************************
Read a merge block from the file system. */
static
ibool
row_merge_read(
/*===========*/
					/* out: TRUE if request was
					successful, FALSE if fail */
	int			fd,	/* in: file descriptor */
	ulint			offset,	/* in: offset where to read */
	row_merge_block_t*	buf)	/* out: data */
{
	ib_uint64_t	ofs = ((ib_uint64_t) offset) * sizeof *buf;

	return(UNIV_LIKELY(os_file_read(OS_FILE_FROM_FD(fd), buf,
					(ulint) (ofs & 0xFFFFFFFF),
					(ulint) (ofs >> 32),
					sizeof *buf)));
}

/************************************************************************
Read a merge block from the file system. */
static
ibool
row_merge_write(
/*============*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	int		fd,	/* in: file descriptor */
	ulint		offset,	/* in: offset where to write */
	const void*	buf)	/* in: data */
{
	ib_uint64_t	ofs = ((ib_uint64_t) offset)
		* sizeof(row_merge_block_t);

	return(UNIV_LIKELY(os_file_write("(merge)", OS_FILE_FROM_FD(fd), buf,
					 (ulint) (ofs & 0xFFFFFFFF),
					 (ulint) (ofs >> 32),
					 sizeof(row_merge_block_t))));
}

/************************************************************************
Read a merge record. */
static
const byte*
row_merge_read_rec(
/*===============*/
					/* out: pointer to next record,
					or NULL on I/O error
					or end of list */
	row_merge_block_t*	block,	/* in/out: file buffer */
	mrec_buf_t*		buf,	/* in/out: secondary buffer */
	const byte*		b,	/* in: pointer to record */
	dict_index_t*		index,	/* in: index of the record */
	int			fd,	/* in: file descriptor */
	ulint*			foffs,	/* in/out: file offset */
	const mrec_t**		mrec,	/* out: pointer to merge record,
					or NULL on end of list
					(non-NULL on I/O error) */
	ulint*			offsets)/* out: offsets of mrec */
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

	/* Normalize extra_size.  Above, value 0 signals "end of list. */
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

		return(b);
	}

	*mrec = b + extra_size;

	rec_init_offsets_comp_ordinary(*mrec, 0, index, offsets);

	data_size = rec_offs_data_size(offsets);
	ut_ad(extra_size + data_size < sizeof *buf);

	b += extra_size + data_size;

	if (UNIV_LIKELY(b < block[1])) {
		/* The record fits entirely in the block.
		This is the normal case. */
		return(b);
	}

	/* The record spans two blocks.  Copy it to buf. */

	avail_size = block[1] - b;
	memcpy(*buf, b, avail_size);
	*mrec = *buf + extra_size;
	rec_offs_make_valid(*mrec, index, offsets);

	if (!row_merge_read(fd, ++(*foffs), block)) {

		goto err_exit;
	}

	/* Wrap around to the beginning of the buffer. */
	b = block[0];

	/* Copy the rest of the record. */
	memcpy(*buf + avail_size, b, extra_size + data_size - avail_size);
	b += extra_size + data_size - avail_size;

	return(b);
}

/************************************************************************
Write a merge record. */
static
void
row_merge_write_rec_low(
/*====================*/
	byte*		b,	/* out: buffer */
	ulint		e,	/* in: encoded extra_size */
	const mrec_t*	mrec,	/* in: record to write */
	const ulint*	offsets)/* in: offsets of mrec */
{
	if (e < 0x80) {
		*b++ = e;
	} else {
		*b++ = 0x80 | (e >> 8);
		*b++ = (byte) e;
	}

	memcpy(b, mrec - rec_offs_extra_size(offsets), rec_offs_size(offsets));
}

/************************************************************************
Write a merge record. */
static
byte*
row_merge_write_rec(
/*================*/
					/* out: pointer to end of block,
					or NULL on error */
	row_merge_block_t*	block,	/* in/out: file buffer */
	mrec_buf_t*		buf,	/* in/out: secondary buffer */
	byte*			b,	/* in: pointer to end of block */
	int			fd,	/* in: file descriptor */
	ulint*			foffs,	/* in/out: file offset */
	const mrec_t*		mrec,	/* in: record to write */
	const ulint*		offsets)/* in: offsets of mrec */
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

		row_merge_write_rec_low(buf[0], extra_size, mrec, offsets);

		/* Copy the head of the temporary buffer, write
		the completed block, and copy the tail of the
		record to the head of the new block. */
		memcpy(b, buf[0], avail_size);

		if (!row_merge_write(fd, (*foffs)++, block)) {
			return(NULL);
		}

		/* Copy the rest. */
		b = block[0];
		memcpy(b, buf[0] + avail_size, size - avail_size);
		b += size - avail_size;
	} else {
		row_merge_write_rec_low(b, extra_size, mrec, offsets);
		b += rec_offs_size(offsets);
	}

	return(b);
}

/************************************************************************
Write an end-of-list marker. */
static
byte*
row_merge_write_eof(
/*================*/
					/* out: pointer to end of block,
					or NULL on error */
	row_merge_block_t*	block,	/* in/out: file buffer */
	byte*			b,	/* in: pointer to end of block */
	int			fd,	/* in: file descriptor */
	ulint*			foffs)	/* in/out: file offset */
{
	ut_ad(block);
	ut_ad(b >= block[0]);
	ut_ad(b < block[1]);
	ut_ad(foffs);

	*b++ = 0;
#ifdef UNIV_DEBUG_VALGRIND
	/* The rest of the block is uninitialized.  Initialize it
	to avoid bogus warnings. */
	memset(b, 0, block[1] - b);
#endif /* UNIV_DEBUG_VALGRIND */

	if (!row_merge_write(fd, (*foffs)++, block)) {
		return(NULL);
	}

	return(block[0]);
}

/*****************************************************************
Compare two merge records. */
static
int
row_merge_cmp(
/*==========*/
					/* out: 1, 0, -1 if mrec1 is
					greater, equal, less,
					respectively, than mrec2 */
	const mrec_t*	mrec1,		/* in: first merge record to be
					compared */
	const mrec_t*	mrec2,		/* in: second merge record to be
					compared */
	const ulint*	offsets1,	/* in: first record offsets */
	const ulint*	offsets2,	/* in: second record offsets */
	dict_index_t*	index)		/* in: index */
{
	return(cmp_rec_rec_simple(mrec1, mrec2, offsets1, offsets2, index));
}

/************************************************************************
Reads clustered index of the table and create temporary files
containing index entries for indexes to be built. */
static
ulint
row_merge_read_clustered_index(
/*===========================*/
					/* out: DB_SUCCESS or error */
	trx_t*			trx,	/* in: transaction */
	dict_table_t*		table,	/* in: table where index is created */
	dict_index_t**		index,	/* in: indexes to be created */
	merge_file_t*		files,	/* in: temporary files */
	ulint			n_index,/* in: number of indexes to create */
	row_merge_block_t*	block)	/* in/out: file buffer */
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

	trx->op_info = "reading clustered index";

	ut_ad(trx);
	ut_ad(table);
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

	clust_index = dict_table_get_first_index(table);

	btr_pcur_open_at_index_side(
		TRUE, clust_index, BTR_SEARCH_LEAF, &pcur, TRUE, &mtr);

	row_heap = mem_heap_create(UNIV_PAGE_SIZE);

	/* Scan the clustered index. */
	for (;;) {
		const rec_t*	rec;
		dtuple_t*	row		= NULL;
		row_ext_t*	ext;
		ibool		has_next	= TRUE;

		btr_pcur_move_to_next_on_page(&pcur, &mtr);

		/* When switching pages, commit the mini-transaction
		in order to release the latch on the old page. */

		if (btr_pcur_is_after_last_on_page(&pcur, &mtr)) {
			btr_pcur_store_position(&pcur, &mtr);
			mtr_commit(&mtr);
			mtr_start(&mtr);
			btr_pcur_restore_position(BTR_SEARCH_LEAF,
						  &pcur, &mtr);
			has_next = btr_pcur_move_to_next_user_rec(&pcur, &mtr);
		}

		if (UNIV_LIKELY(has_next)) {
			rec = btr_pcur_get_rec(&pcur);

			/* Skip delete marked records. */
			if (rec_get_deleted_flag(rec,
						 dict_table_is_comp(table))) {
				continue;
			}

			srv_n_rows_inserted++;

			/* Build row based on clustered index */

			row = row_build(ROW_COPY_POINTERS, clust_index,
					rec, NULL, &ext, row_heap);

			/* Build all entries for all the indexes to be created
			in a single scan of the clustered index. */
		}

		for (i = 0; i < n_index; i++) {
			row_merge_buf_t*	buf	= merge_buf[i];
			merge_file_t*		file	= &files[i];

			if (UNIV_LIKELY
			    (row && row_merge_buf_add(buf, row, ext))) {
				continue;
			}

			ut_ad(buf->n_tuples || !has_next);

			/* We have enough data tuples to form a block.
			Sort them and write to disk. */

			if (buf->n_tuples
			    && row_merge_buf_sort(buf)
			    && dict_index_is_unique(buf->index)) {
				err = DB_DUPLICATE_KEY;
				goto func_exit;
			}

			row_merge_buf_write(buf, block);

			if (!row_merge_write(file->fd, file->offset++,
					     block)) {
				trx->error_key_num = i;
				err = DB_OUT_OF_FILE_SPACE;
				goto func_exit;
			}

			row_merge_buf_empty(buf);
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

	for (i = 0; i < n_index; i++) {
		row_merge_buf_free(merge_buf[i]);
	}

	mem_free(merge_buf);

	trx->op_info = "";

	return(err);
}

/*****************************************************************
Merge two blocks of linked lists on disk and write a bigger block. */
static
ulint
row_merge_blocks(
/*=============*/
					/* out: DB_SUCCESS or error code */
	dict_index_t*		index,	/* in: index being created */
	merge_file_t*		file,	/* in/out: file containing
					index entries */
	row_merge_block_t*	block,	/* in/out: 3 buffers */
	ulint*			foffs0,	/* in/out: offset of first
					source list in the file */
	ulint*			foffs1,	/* in/out: offset of second
					source list in the file */
	merge_file_t*		of)	/* in/out: output file */
{
	mem_heap_t*	heap;	/* memory heap for offsets0, offsets1 */

	mrec_buf_t	buf[3];	/* buffer for handling split mrec in block[] */
	const byte*	b0;	/* pointer to block[0] */
	const byte*	b1;	/* pointer to block[1] */
	byte*		b2;	/* pointer to block[2] */
	const mrec_t*	mrec0;	/* merge rec, points to block[0] or buf[0] */
	const mrec_t*	mrec1;	/* merge rec, points to block[1] or buf[1] */
	ulint*		offsets0;/* offsets of mrec0 */
	ulint*		offsets1;/* offsets of mrec1 */

	heap = row_merge_heap_create(index, &offsets0, &offsets1);

	/* Write a record and read the next record.  Split the output
	file in two halves, which can be merged on the following pass. */
#define ROW_MERGE_WRITE_GET_NEXT(N, AT_END)				\
	do {								\
		b2 = row_merge_write_rec(&block[2], &buf[2], b2,	\
					 of->fd, &of->offset,		\
					 mrec##N, offsets##N);		\
		if (UNIV_UNLIKELY(!b2)) {				\
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
		switch (row_merge_cmp(mrec0, mrec1,
				      offsets0, offsets1, index)) {
		case 0:
			if (UNIV_UNLIKELY
			    (dict_index_is_unique(index))) {
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

/*****************************************************************
Merge disk files. */
static
ulint
row_merge(
/*======*/
						/* out: DB_SUCCESS
						or error code */
	dict_index_t*		index,		/* in: index being created */
	merge_file_t*		file,		/* in/out: file containing
						index entries */
	row_merge_block_t*	block,		/* in/out: 3 buffers */
	int*			tmpfd)		/* in/out: temporary file
						handle */
{
	ulint		foffs0;	/* first input offset */
	ulint		foffs1;	/* second input offset */
	ulint		half;	/* upper limit of foffs1 */
	ulint		error;	/* error code */
	merge_file_t	of;	/* output file */

	of.fd = *tmpfd;
	of.offset = 0;

	/* Split the input file in two halves. */
	half = file->offset / 2;

	/* Merge blocks to the output file. */
	foffs0 = 0;
	foffs1 = half;

	for (; foffs0 < half; foffs0++, foffs1++) {
		error = row_merge_blocks(index, file, block,
					 &foffs0, &foffs1, &of);

		if (error != DB_SUCCESS) {
			return(error);
		}
	}

	/* Copy the last block, if there is one. */
	while (foffs1 < file->offset) {
		if (!row_merge_read(file->fd, foffs1++, block)
		    || !row_merge_write(of.fd, of.offset++, block)) {
			return(DB_CORRUPTION);
		}
	}

	/* Swap file descriptors for the next pass. */
	*tmpfd = file->fd;
	*file = of;

	return(DB_SUCCESS);
}

/*****************************************************************
Merge disk files. */
static
ulint
row_merge_sort(
/*===========*/
						/* out: DB_SUCCESS
						or error code */
	dict_index_t*		index,		/* in: index being created */
	merge_file_t*		file,		/* in/out: file containing
						index entries */
	row_merge_block_t*	block,		/* in/out: 3 buffers */
	int*			tmpfd)		/* in/out: temporary file
						handle */
{
	ulint	blksz;	/* block size */

	blksz = 1;

	for (;; blksz *= 2) {
		ulint	error = row_merge(index, file, block, tmpfd);
		if (error != DB_SUCCESS) {
			return(error);
		}

		if (blksz >= file->offset) {
			/* everything is in a single block */
			break;
		}

		/* Round up the file size to a multiple of blksz. */
		file->offset = ut_2pow_round(file->offset - 1, blksz) + blksz;
	}

	return(DB_SUCCESS);
}

/*****************************************************************
Copy externally stored columns to the data tuple. */
static
void
row_merge_copy_blobs(
/*=================*/
	const mrec_t*	mrec,	/* in: merge record */
	const ulint*	offsets,/* in: offsets of mrec */
	ulint		zip_size,/* in: compressed page size in bytes, or 0 */
	dtuple_t*	tuple,	/* in/out: data tuple */
	mem_heap_t*	heap)	/* in/out: memory heap */
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

		data = btr_rec_copy_externally_stored_field(
			mrec, offsets, zip_size, i, &len, heap);

		dfield_set_data(field, data, len);
	}
}

/************************************************************************
Read sorted file containing index data tuples and insert these data
tuples to the index */
static
ulint
row_merge_insert_index_tuples(
/*==========================*/
					/* out: DB_SUCCESS or error number */
	trx_t*			trx,	/* in: transaction */
	dict_index_t*		index,	/* in: index */
	dict_table_t*		table,	/* in: new table */
	ulint			zip_size,/* in: compressed page size of
					 the old table, or 0 if uncompressed */
	int			fd,	/* in: file descriptor */
	row_merge_block_t*	block)	/* in/out: file buffer */
{
	mrec_buf_t		buf;
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

	graph_heap = mem_heap_create(500);
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
		for (;;) {
			const mrec_t*	mrec;
			dtuple_t*	dtuple;
			ulint		n_ext;

			b = row_merge_read_rec(block, &buf, b, index,
					       fd, &foffs, &mrec, offsets);
			if (UNIV_UNLIKELY(!b)) {
				/* End of list, or I/O error */
				if (mrec) {
					error = DB_CORRUPTION;
				}
				break;
			}

			n_ext = 0;
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

/*************************************************************************
Drop an index from the InnoDB system tables. */

void
row_merge_drop_index(
/*=================*/
	dict_index_t*	index,	/* in: index to be removed */
	dict_table_t*	table,	/* in: table */
	trx_t*		trx)	/* in: transaction handle */
{
	ulint		err;
	ibool		dict_lock = FALSE;
	pars_info_t*	info = pars_info_create();

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in deleting the dictionary data from system
	tables in Innobase. Deleting a row from SYS_INDEXES table also
	frees the file segments of the B-tree associated with the index. */

	static const char str1[] =
		"PROCEDURE DROP_INDEX_PROC () IS\n"
		"BEGIN\n"
		"DELETE FROM SYS_FIELDS WHERE INDEX_ID = :indexid;\n"
		"DELETE FROM SYS_INDEXES WHERE ID = :indexid\n"
		"		AND TABLE_ID = :tableid;\n"
		"END;\n";

	ut_ad(index && table && trx);

	pars_info_add_dulint_literal(info, "indexid", index->id);
	pars_info_add_dulint_literal(info, "tableid", table->id);

	trx_start_if_not_started(trx);
	trx->op_info = "dropping index";

	if (trx->dict_operation_lock_mode == 0) {
		row_mysql_lock_data_dictionary(trx);
		dict_lock = TRUE;
	}

	err = que_eval_sql(info, str1, FALSE, trx);

	ut_a(err == DB_SUCCESS);

	/* Replace this index with another equivalent index for all
	foreign key constraints on this table where this index is used */

	dict_table_replace_index_in_foreign_list(table, index);

	if (trx->dict_redo_list) {
		dict_redo_remove_index(trx, index);
	}

	dict_index_remove_from_cache(table, index);

	if (dict_lock) {
		row_mysql_unlock_data_dictionary(trx);
	}

	trx->op_info = "";
}

/*************************************************************************
Drop those indexes which were created before an error occurred
when building an index. */

void
row_merge_drop_indexes(
/*===================*/
	trx_t*		trx,		/* in: transaction */
	dict_table_t*	table,		/* in: table containing the indexes */
	dict_index_t**	index,		/* in: indexes to drop */
	ulint		num_created)	/* in: number of elements in index[] */
{
	ulint	key_num;

	for (key_num = 0; key_num < num_created; key_num++) {
		row_merge_drop_index(index[key_num], table, trx);
	}
}

/*************************************************************************
Create a merge file. */
static
void
row_merge_file_create(
/*==================*/
	merge_file_t*	merge_file)	/* out: merge file structure */
{
	merge_file->fd = innobase_mysql_tmpfile();
	merge_file->offset = 0;
}

/*************************************************************************
Destroy a merge file. */
static
void
row_merge_file_destroy(
/*===================*/
	merge_file_t*	merge_file)	/* out: merge file structure */
{
	if (merge_file->fd != -1) {
		close(merge_file->fd);
		merge_file->fd = -1;
	}
}

/*************************************************************************
Create a temporary table using a definition of the old table. You must
lock data dictionary before calling this function. */

dict_table_t*
row_merge_create_temporary_table(
/*=============================*/
					/* out: table, or NULL on error */
	const char*	table_name,	/* in: new table name */
	dict_table_t*	table,		/* in: old table definition */
	trx_t*		trx)		/* in/out: trx (sets error_state) */
{
	ulint		i;
	dict_table_t*	new_table = NULL;
	ulint		n_cols = dict_table_get_n_user_cols(table);
	ulint		error;

	ut_ad(table_name && table && error);
	ut_ad(mutex_own(&dict_sys->mutex));

	error = row_undo_report_create_table_dict_operation(trx, table_name);

	if (error == DB_SUCCESS) {

		mem_heap_t*	heap = mem_heap_create(1000);
		log_buffer_flush_to_disk();

		new_table = dict_mem_table_create(
			table_name, 0, n_cols, table->flags);

		for (i = 0; i < n_cols; i++) {
			const dict_col_t*	col;

			col = dict_table_get_nth_col(table, i);

			dict_mem_table_add_col(
				new_table, heap,
				dict_table_get_col_name(table, i),
				col->mtype, col->prtype, col->len);
		}

		error = row_create_table_for_mysql(new_table, trx);
		mem_heap_free(heap);

		if (error != DB_SUCCESS) {
			dict_mem_table_free(new_table);
			new_table = NULL;
		}
	}

	if (error != DB_SUCCESS) {
		trx->error_state = error;
	}

	return(new_table);
}

/*************************************************************************
Rename the indexes in the dictionary. */

ulint
row_merge_rename_index(
/*===================*/
					/* out: DB_SUCCESS if all OK */
	trx_t*		trx,		/* in: Transaction */
	dict_table_t*	table,		/* in: Table for index */
	dict_index_t*	index)		/* in: Index to rename */
{
	ibool		dict_lock = FALSE;
	ulint		err = DB_SUCCESS;
	pars_info_t*	info = pars_info_create();

	/* Only rename from temp names */
	ut_a(*index->name == TEMP_TABLE_PREFIX);

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in renaming index. */

	static const char str1[] =
		"PROCEDURE RENAME_INDEX_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES SET NAME = :name\n"
		" WHERE ID = :indexid AND TABLE_ID = :tableid;\n"
		"END;\n";

	table = index->table;

	ut_ad(index && table && trx);

	trx_start_if_not_started(trx);
	trx->op_info = "renaming index";

	pars_info_add_str_literal(info, "name", index->name + 1);
	pars_info_add_dulint_literal(info, "indexid", index->id);
	pars_info_add_dulint_literal(info, "tableid", table->id);

	if (trx->dict_operation_lock_mode == 0) {
		row_mysql_lock_data_dictionary(trx);
		dict_lock = TRUE;
	}

	err = que_eval_sql(info, str1, FALSE, trx);

	if (err == DB_SUCCESS) {
		index->name++;
	}

	if (dict_lock) {
		row_mysql_unlock_data_dictionary(trx);
	}

	trx->op_info = "";

	return(err);
}

/*************************************************************************
Create the index and load in to the dictionary. */

dict_index_t*
row_merge_create_index(
/*===================*/
					/* out: index, or NULL on error */
	trx_t*		trx,		/* in/out: trx (sets error_state) */
	dict_table_t*	table,		/* in: the index is on this table */
	const merge_index_def_t*	/* in: the index definition */
			index_def)
{
	dict_index_t*	index;
	ulint		err = DB_SUCCESS;
	ulint		n_fields = index_def->n_fields;

	/* Create the index prototype, using the passed in def, this is not
	a persistent operation. We pass 0 as the space id, and determine at
	a lower level the space id where to store the table. */

	index = dict_mem_index_create(table->name, index_def->name,
				      0, index_def->ind_type, n_fields);

	ut_a(index);

	/* Create the index id, as it will be required when we build
	the index. We assign the id here because we want to write an
	UNDO record before we insert the entry into SYS_INDEXES. */
	ut_a(ut_dulint_is_zero(index->id));

	index->id = dict_hdr_get_new_id(DICT_HDR_INDEX_ID);
	index->table = table;

	/* Write the UNDO record for the create index */
	err = row_undo_report_create_index_dict_operation(trx, index);

	if (err == DB_SUCCESS) {
		ulint		i;

		/* Make sure the UNDO record gets to disk */
		log_buffer_flush_to_disk();

		for (i = 0; i < n_fields; i++) {
			merge_index_field_t* ifield;

			ifield = &index_def->fields[i];

			dict_mem_index_add_field(index,
						 ifield->field_name,
						 ifield->prefix_len);
		}

		/* Add the index to SYS_INDEXES, this will use the prototype
		to create an entry in SYS_INDEXES. */
		err = row_create_index_graph_for_mysql(trx, table, index);

		if (err == DB_SUCCESS) {

			index = row_merge_dict_table_get_index(
				table, index_def);

			ut_a(index);

			/* Note the id of the transaction that created this
			index, we use it to restrict readers from accessing
			this index, to ensure read consistency. */
			index->trx_id = trx->id;

			/* Create element and append to list in trx. So that
			we can rename from temp name to real name. */
			if (trx->dict_redo_list) {
				dict_redo_t*	dict_redo;

				dict_redo = dict_redo_create_element(trx);
				dict_redo->index = index;
			}
		}
	}

	if (err != DB_SUCCESS) {
		trx->error_state = err;
	}

	return(index);
}

/*************************************************************************
Check if a transaction can use an index. */

ibool
row_merge_is_index_usable(
/*======================*/
	const trx_t*		trx,	/* in: transaction */
	const dict_index_t*	index)	/* in: index to check */
{
	if (!trx->read_view) {
		return(TRUE);
	}

	return(ut_dulint_cmp(index->trx_id, trx->read_view->low_limit_id) < 0);
}

/*************************************************************************
Drop the old table. */

ulint
row_merge_drop_table(
/*=================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	dict_table_t*	table)		/* in: table to drop */
{
	ulint		err = DB_SUCCESS;
	ibool		dict_locked = FALSE;

	if (trx->dict_operation_lock_mode == 0) {
		row_mysql_lock_data_dictionary(trx);
		dict_locked = TRUE;
	}

	ut_a(table->to_be_dropped);
	ut_a(*table->name == TEMP_TABLE_PREFIX);

	/* Drop the table immediately iff it is not references by MySQL */
	if (table->n_mysql_handles_opened == 0) {
		/* Copy table->name, because table will have been
		freed when row_drop_table_for_mysql_no_commit()
		checks with dict_load_table() that the table was
		indeed dropped. */
		char* table_name = mem_strdup(table->name);
		/* Set the commit flag to FALSE. */
		err = row_drop_table_for_mysql(table_name, trx, FALSE);
		mem_free(table_name);
	}

	if (dict_locked) {
		row_mysql_unlock_data_dictionary(trx);
	}

	return(err);
}

/*************************************************************************
Build indexes on a table by reading a clustered index,
creating a temporary file containing index entries, merge sorting
these index entries and inserting sorted index entries to indexes. */

ulint
row_merge_build_indexes(
/*====================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	dict_table_t*	old_table,	/* in: Table where rows are
					read from */
	dict_table_t*	new_table,	/* in: Table where indexes are
					created. Note that old_table ==
					new_table if we are creating a
					secondary keys. */
	dict_index_t**	indexes,	/* in: indexes to be created */
	ulint		n_indexes)	/* in: size of indexes[] */
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

	tmpfd = innobase_mysql_tmpfile();

	/* Read clustered index of the table and create files for
	secondary index entries for merge sort */

	error = row_merge_read_clustered_index(
		trx, old_table, indexes, merge_files, n_indexes, block);

	if (error != DB_SUCCESS) {

		goto func_exit;
	}

	trx_start_if_not_started(trx);

	/* Now we have files containing index entries ready for
	sorting and inserting. */

	for (i = 0; i < n_indexes; i++) {
		error = row_merge_sort(indexes[i], &merge_files[i],
				       block, &tmpfd);

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
	close(tmpfd);

	for (i = 0; i < n_indexes; i++) {
		row_merge_file_destroy(&merge_files[i]);
	}

	mem_free(merge_files);
	os_mem_free_large(block, block_size);

	return(error);
}
