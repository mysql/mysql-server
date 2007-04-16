/******************************************************
New index creation routines using a merge sort

(c) 2005 Innobase Oy

Created 12/4/2005 Jan Lindstrom
*******************************************************/

/******************************************************
TODO:

1. Run test with purify and valgrind and fix possible
   errors found.

2. Add more test cases and fix bugs founds.

3. If we are using variable length keys, then in
   some cases these keys do not fit into two empty blocks
   in a different order. Therefore, some empty space is
   left in every block. However, it has not been shown
   that this empty space is enough for all cases. Therefore,
   in the above case these overloaded records should be put
   on another block.

4. Run benchmarks.
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

/* Records are stored in the memory for main memory linked list
to this structure */

struct merge_rec_struct {
	struct merge_rec_struct *next;	/* Pointer to next record
					in the list */
	rec_t*		rec;		/* Record */
};

typedef struct merge_rec_struct merge_rec_t;

/* This structure is head element for main memory linked list
used for main memory linked list merge sort */

struct merge_rec_list_struct {
	merge_rec_t*	head;		/* Pointer to head of the
					list */
	merge_rec_t*	tail;		/* Pointer to tail of the
					list */
#ifdef UNIV_DEBUG
	ulint		n_records;	/* Number of records in
					the list */
#endif /* UNIV_DEBUG */
	ulint		total_size;	/* Total size of all records in
					the list */
	mem_heap_t*	heap;		/* Heap where memory for this
					list is allocated */
};

typedef struct merge_rec_list_struct merge_rec_list_t;

/* Block size for I/O operations in merge sort */

#define MERGE_BLOCK_SIZE	1048576	/* 1M */

/* Intentional free space on every block */
#define MERGE_BLOCK_SAFETY_MARGIN	128

/* Enable faster index creation debug code */
/* #define UNIV_DEBUG_INDEX_CREATE		1 */

/* This block header structure is used to create linked list of the
blocks to the disk. Every block contains one header.*/

struct merge_block_header_struct {
	ulint	n_records;		/* Number of records in the block. */
	ulint	offset;			/* Offset of this block */
	ulint	next;			/* Offset of next block */
};

typedef struct merge_block_header_struct merge_block_header_t;

/* This block structure is used to hold index records in the disk
and the memory */

struct merge_block_struct {
	merge_block_header_t	header;	/* Block header information */
	char			data[MERGE_BLOCK_SIZE - sizeof(merge_block_header_t)];/* Data area i.e. heap */
};

typedef struct merge_block_struct merge_block_t;

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
Creates and initializes a merge block */
static
merge_block_t*
row_merge_block_create(void)
/*========================*/
				/* out: pointer to block */
{
	merge_block_t*	mblock;

	mblock = mem_alloc(sizeof *mblock);

	memset(&mblock->header, 0, sizeof mblock->header);

	return(mblock);
}

/************************************************************************
Read a merge block from the file system. */
static
ibool
row_merge_read(
/*===========*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	os_file_t	file,	/* in: file handle */
	ulint		offset,	/* in: offset where to read */
	void*		buf,	/* out: data */
	ulint		size)	/* in: number of bytes to read */
{
	ib_uint64_t	ofs = ((ib_uint64_t) offset) * MERGE_BLOCK_SIZE;

	ut_ad(size <= MERGE_BLOCK_SIZE);

	return(UNIV_LIKELY(os_file_read(file, buf,
					(ulint) (ofs & 0xFFFFFFFF),
					(ulint) (ofs >> 32),
					size)));
}

/************************************************************************
Read a merge block from the file system. */
static
ibool
row_merge_block_read(
/*=================*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	os_file_t	file,	/* in: file handle */
	ulint		offset,	/* in: offset where to read */
	merge_block_t*	block)	/* out: merge block */
{
	return(row_merge_read(file, offset, block, sizeof *block));
}

/************************************************************************
Read a merge block header from the disk */
static
ibool
row_merge_block_header_read(
/*========================*/
					/* out: TRUE if request was
					successful, FALSE if fail */
	os_file_t		file,	/* in: handle to a file */
	ulint			offset,	/* in: offset where to read */
	merge_block_header_t*	header)	/* out: merge block header */
{
	return(row_merge_read(file, offset, header, sizeof *header));
}

/************************************************************************
Read a merge block from the file system. */
static
ibool
row_merge_write(
/*============*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	os_file_t	file,	/* in: file handle */
	ulint		offset,	/* in: offset where to write */
	const void*	buf,	/* in: data */
	ulint		size)	/* in: number of bytes to write */
{
	ib_uint64_t	ofs = ((ib_uint64_t) offset) * MERGE_BLOCK_SIZE;

	ut_ad(size <= MERGE_BLOCK_SIZE);

	return(UNIV_LIKELY(os_file_write("(merge)", file, buf,
					 (ulint) (ofs & 0xFFFFFFFF),
					 (ulint) (ofs >> 32),
					 size)));
}

/************************************************************************
Write a merge block header to the disk */
static
ibool
row_merge_block_header_write(
/*=========================*/
						/* out: TRUE if request was
						successful, FALSE if fail */
	os_file_t			file,	/* in: handle to a file */
	const merge_block_header_t*	header)	/* in: block header */
{
	return(row_merge_write(file, header->offset, header, sizeof *header));
}

/************************************************************************
Write a merge block to the disk */
static
ibool
row_merge_block_write(
/*==================*/
					/* out: TRUE if request was
					successful, FALSE if fail */
	os_file_t		file,	/* in: handle to a file */
	ulint			offset,	/* in: file offset */
	const merge_block_t*	block)	/* in: block header */
{
	ut_ad(offset == block->header.offset);

	return(row_merge_write(file, offset, block, sizeof *block));
}

/**************************************************************
Create a merge record and copy a index data tuple to the merge
record */
static
merge_rec_t*
row_merge_rec_create(
/*=================*/
				/* out: merge record */
	const dtuple_t*	dtuple,	/* in: data tuple */
	const ulint*	ext,	/* in: array of extern field numbers */
	ulint		n_ext,	/* in: number of elements in ext */
	dict_index_t*	index,	/* in: index record descriptor */
	mem_heap_t*	heap)	/* in: heap where memory is allocated */
{
	merge_rec_t*	m_rec;
	ulint		rec_size;
	byte*		buf;

	ut_ad(dtuple && index && heap);
	ut_ad(dtuple_validate(dtuple));

	m_rec = (merge_rec_t*) mem_heap_alloc(heap, sizeof(merge_rec_t));

	rec_size = rec_get_converted_size(index, dtuple, ext, n_ext);
	buf = mem_heap_alloc(heap, rec_size);

	m_rec->rec  = rec_convert_dtuple_to_rec(buf, index, dtuple,
						ext, n_ext);
	m_rec->next = NULL;

	return(m_rec);
}

/************************************************************************
Checks that a record fits to a block */
static
ibool
row_merge_rec_fits_to_block(
/*========================*/
				/* out: TRUE if record fits to merge block,
				FALSE if record does not fit to block */
	const ulint*	offsets,/* in: record offsets */
	ulint		offset) /* in: offset where to store in the block */
{
	ulint		rec_len;

	ut_ad(offsets);

	rec_len = mach_get_compressed_size(rec_offs_extra_size(offsets))
		+ rec_offs_size(offsets);

	/* Note that we intentionally leave free space on
	every block. This free space might be later needed when two
	blocks are merged and variable length keys are used. Variable
	length keys on two blocks might be interleaved on such a manner
	that they do not fit on two blocks if blocks are too full */

	return((offset + rec_len) < (MERGE_BLOCK_SIZE
				     - MERGE_BLOCK_SAFETY_MARGIN
				     - sizeof(merge_block_header_t)));
}

/************************************************************************
Store a record to a merge file block. Note that this function does
not check that the record fits to the block. */
static
ulint
row_merge_store_rec_to_block(
/*=========================*/
				/* out: offset for next data tuple */
	const rec_t*	rec,	/* in: record to be stored in the memory */
	const ulint*	offsets,/* in: record offsets */
	merge_block_t*	mblock, /* in: block where data tuple is stored */
	ulint		offset) /* in: offset where to store */
{
	char*		dest_data;
	ulint		rec_len;
	ulint		extra_len;
	ulint		storage_size;

	ut_ad(rec && mblock && offsets);
	ut_ad(rec_validate(rec, offsets));

	/* Find the position in the block where this data tuple is stored.
	If we are at the start of the block, remember to add size of header
	to the offset */

	if (offset == 0) {
		dest_data = mblock->data;
	} else {
		dest_data = ((char *)mblock + offset);
	}

	ut_ad(dest_data < (char*) &mblock[1]);

	extra_len = rec_offs_extra_size(offsets);
	rec_len = rec_offs_size(offsets);

	/* 1. Store the extra_len */
	storage_size = mach_write_compressed((byte *)dest_data, extra_len);
	dest_data+=storage_size;
	ut_ad(dest_data < (char*) &mblock[1]);

	/* 2. Store the record */
	memcpy(dest_data, rec - extra_len, rec_len);
	dest_data+=rec_len;
	ut_ad(dest_data < (char*) &mblock[1]);

	mblock->header.n_records++;

	/* Return next offset */
	return((char *)dest_data - (char *)mblock);
}

/************************************************************************
Read a record from the block */
static
merge_rec_t*
row_merge_read_rec_from_block(
/*==========================*/
				/* out: record or NULL*/
	merge_block_t*	mblock, /* in: memory block where to read */
	ulint*		offset,	/* in/out: offset where to read a record */
	mem_heap_t*	heap,	/* in: heap were this memory for this record
				is allocated */
	dict_index_t*	index)	/* in: index record desriptor */
{
	merge_rec_t*	mrec;
	char*		from_data;
	ulint		extra_len;
	ulint		data_len;
	ulint		tmp_offset;
	ulint		storage_len;
	rec_t*		rec;
	mem_heap_t*	offset_heap = NULL;
	ulint		sec_offsets_[REC_OFFS_SMALL_SIZE];
	ulint*		sec_offs	= sec_offsets_;

	*sec_offsets_ = (sizeof sec_offsets_) / sizeof *sec_offsets_;

	ut_ad(mblock && offset && heap);

	tmp_offset = *offset;

	/* Find the position in the block where this data tuple is stored.
	If we are at the start of the block, remember to add size of header
	to the offset */

	if (tmp_offset == 0) {
		from_data = mblock->data;
	} else {
		from_data = ((char *)mblock + tmp_offset);
	}

	ut_ad(from_data < (const char*) &mblock[1]);

	mrec = mem_heap_alloc(heap, sizeof(merge_rec_t));

	/* 1. Read the extra len and calculate its storage length */
	extra_len = mach_read_compressed((byte *)from_data);
	storage_len = mach_get_compressed_size(extra_len);
	from_data+=storage_len;
	ut_ad(from_data < (const char*) &mblock[1]);

	/* 2. Read the record */
	rec = (rec_t*)(from_data + extra_len);
	mrec->rec = rec;
	sec_offs = rec_get_offsets(mrec->rec, index, sec_offs, ULINT_UNDEFINED,
				   &offset_heap);
	data_len = rec_offs_size(sec_offs);
	ut_ad(rec_validate(rec, sec_offs));

	from_data+=data_len;
	ut_ad(from_data < (const char*) &mblock[1]);

	/* Return also start offset of the next data tuple */
	*offset = ((char *)from_data - (char *)mblock);

	if (offset_heap) {
		mem_heap_free(offset_heap);
	}

	return(mrec);
}

/*****************************************************************
Compare a merge record to another merge record. Returns
1) NULL if unique index is to be created and records are identical
2) first record if the fist record is smaller than the second record
3) first record if records are identical and index type is not UNIQUE
4) second record if the first record is largen than second record*/
static
int
row_merge_cmp(
/*==========*/
					/* out: 1, 0, -1 if mrec1 is
					greater, equal, less,
					respectively, than mrec2 */
	merge_rec_t*	mrec1,		/* in: first merge record to be
					compared */
	merge_rec_t*	mrec2,		/* in: second merge record to be
					compared */
	const ulint*	offsets1,	/* in: first record offsets */
	const ulint*	offsets2,	/* in: second record offsets */
	dict_index_t*	index)		/* in: index */
{
	ut_ad(mrec1 && mrec2 && offsets1 && offsets2 && index && selected);
	ut_ad(rec_validate(mrec1->rec, offsets1));
	ut_ad(rec_validate(mrec2->rec, offsets2));

	return(cmp_rec_rec(mrec1->rec, mrec2->rec, offsets1, offsets2, index));
}

/*****************************************************************
Merge sort for linked list in memory.

Merge sort takes the input list and makes log N passes along
the list and in each pass it combines each adjacent pair of
small sorted lists into one larger sorted list. When only a one
pass is needed the whole output list must be sorted.

In each pass, two lists of size block_size are merged into lists of
size block_size*2. Initially block_size=1. Merge starts by pointing
a temporary pointer list1 at the head of the list and also preparing
an empty list list_tail where elements will be appended. Then:

	1) If list1 is NULL we terminate this pass.

	2) Otherwise, there is at least one element in the next
	pair of block_size lists therefore, increase the number of
	merges performed in this pass.

	3) Point another temporary pointer list2 as the same
	place as list1. Iterate list2 by block_size elements
	or until the end of the list. Let the list_size1 be the
	number of elements in the list2.

	4) Let list_size1=merge_size. Now we merge list starting at
	list1 of length list_size2 with a list starting at list2 of
	length at most list_size1.

	5) So, as long as either the list1 is non-empty (list_size1)
	or the list2 is non-empty (list_size2 and list2 pointing to
	a element):

		5.1) Select which list to take the next element from.
		If either lists is empty, we choose from the other one.
		If both lists are non-empty, compare the first element
		of each and choose the lower one.

		5.2) Remove that element, tmp, from the start of its
		lists, by advancing list1 or list2 to next element
		and decreasing list1_size or list2_size.

		5.3) Append tmp to list_tail

	6) At this point, we have advanced list1 until it is where
	list2 started out and we have advanced list2 until it is
	pointing at the next pair of block_size lists to merge.
	Thus, set list1 to the value of list2 and go back to the
	start of this loop.

As soon as a pass like this is performed with only one merge, the
algorithm terminates and output list list_head is sorted. Otherwise,
double the value of block_size and go back to the beginning. */
static
ibool
row_merge_sort_linked_list(
/*=======================*/
					/* out: FALSE on error */
	dict_index_t*		index,	/* in: index to be created */
	merge_rec_list_t*	list)	/* in: Pointer to head element */
{
	ibool		success;
	merge_rec_t*	list1;
	merge_rec_t*	list2;
	merge_rec_t*	list_head;
	merge_rec_t*	list_tail;
	ulint		block_size;
	ulint		num_of_merges;
	ulint		list1_size;
	ulint		list2_size;
	ulint		i;
	mem_heap_t*	heap = NULL;
	ulint		offsets1_[REC_OFFS_SMALL_SIZE];
	ulint*		offsets1 = offsets1_;
	ulint		offsets2_[REC_OFFS_SMALL_SIZE];
	ulint*		offsets2 = offsets2_;

	ut_ad(list && list->head && index);

	*offsets1_ = (sizeof offsets1_) / sizeof *offsets1_;
	*offsets2_ = (sizeof offsets2_) / sizeof *offsets2_;

	block_size = 1;	/* We start from block size 1 */

	list_head = list->head;

	for (;;) {
		list1 = list_head;
		list_head = NULL;
		list_tail = NULL;
		num_of_merges = 0;	/* We count number of merges we do in
					this pass */

		while (list1) {
			num_of_merges++;

			list2 = list1;
			list1_size = 0;

			/* Step at most block_size elements along from
			list2. */

			for (i = 0; i < block_size; i++) {
				list1_size++;
				list2 = list2->next;

				if (!list2) {
					break;
				}
			}

			list2_size = block_size;

			/* If list2 is not NULL, we have two lists to merge.
			Otherwice, we have a sorted list. */

			while (list1_size > 0 || (list2_size > 0 && list2)) {
				merge_rec_t*	tmp;
				/* Merge sort two lists by deciding whether
				next element of merge comes from list1 or
				list2. */

				if (list1_size == 0) {
					/* First list is empty, next element
					must come from the second list. */
					goto pick2;
				}

				if (list2_size == 0 || !list2) {
					/* Second list is empty, next element
					must come from the first list. */
					goto pick1;
				}

				offsets1 = rec_get_offsets(list1->rec, index,
							   offsets1,
							   ULINT_UNDEFINED,
							   &heap);
				offsets2 = rec_get_offsets(list2->rec, index,
							   offsets2,
							   ULINT_UNDEFINED,
							   &heap);

				switch (row_merge_cmp(list1, list2,
						      offsets1, offsets2,
						      index)) {
				case 0:
					if (UNIV_UNLIKELY
					    (dict_index_is_unique(index))) {
						success = FALSE;
						goto func_exit;
					}
					/* fall through */
				case -1:
pick1:
					tmp = list1;
					list1 = list1->next;
					list1_size--;
					break;
				case 1:
pick2:
					tmp = list2;
					list2 = list2->next;
					list2_size--;
					break;
				}

				/* Append the element to the merged list */

				if (list_tail) {
					list_tail->next = tmp;
				} else {
					list_head = tmp;
				}

				list_tail = tmp;
			}

			/* Now we have processed block_size items from list1. */

			list1 = list2;
		}

		list_tail->next = NULL;

		/* If we have done oly one merge, we have created a sorted
		list */

		if (num_of_merges <= 1) {
			list->head = list_head;
			success = TRUE;
			goto func_exit;
		} else {
			/* Otherwise merge lists twice the size */
			block_size *= 2;
		}
	}

func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	return(success);
}

/*****************************************************************
Create and initialize record list used for in-memory merge sort */
static
merge_rec_list_t*
row_merge_create_list(void)
/*=======================*/
				/* out: pointer to list */
{
	merge_rec_list_t*	list_header;
	mem_heap_t*		heap = NULL;

	/* Create list header */
	heap = mem_heap_create((MERGE_BLOCK_SIZE + sizeof(merge_rec_list_t)));

	list_header = mem_heap_alloc(heap, sizeof(merge_rec_list_t));

	list_header->head		= NULL;
	list_header->tail		= NULL;
	ut_d(list_header->n_records	= 0);
	list_header->total_size		= sizeof(merge_rec_list_t);
	list_header->heap		= heap;

	return(list_header);
}

/*****************************************************************
Add one record to the merge list */
static
void
row_merge_list_add(
/*===============*/
	merge_rec_t*		m_rec,		/* in: record to be
						inserted to the list */
	ulint			rec_len,	/* in: record length */
	merge_rec_list_t*	list_header)	/* in/out: list header */
{
	ut_ad(m_rec && list_header);

	m_rec->next = NULL;
	list_header->total_size+=rec_len;

	if (list_header->tail == NULL) {

		list_header->tail = list_header->head = m_rec;
	} else {
		list_header->tail->next = m_rec;
		list_header->tail = m_rec;
	}

	ut_d(list_header->n_records++);
}

/*****************************************************************
Write records from the list to the merge block */
static
merge_rec_list_t*
row_merge_write_list_to_block(
/*==========================*/
					/* out: pointer to a new list
					where rest of the items are stored */
	merge_rec_list_t*	list,	/* in: Record list */
	merge_block_t*		output,	/* in: Pointer to block */
	dict_index_t*		index)	/* in: Record descriptor */
{
	ulint		offset		= 0;
	merge_rec_t*	m_rec		= NULL;
	merge_rec_list_t* new_list	= NULL;
	mem_heap_t*	heap		= NULL;
	ulint		sec_offsets_[REC_OFFS_SMALL_SIZE];
	ulint*		sec_offs	= sec_offsets_;

	ut_ad(list && output && index);

	*sec_offsets_ = (sizeof sec_offsets_) / sizeof *sec_offsets_;
	output->header.n_records = 0;

	/* Write every record which fits to block to the block */

	m_rec = list->head;

	while (m_rec) {

		sec_offs = rec_get_offsets(m_rec->rec, index, sec_offs,
					   ULINT_UNDEFINED, &heap);

		if (!row_merge_rec_fits_to_block(sec_offs, offset)) {
			break;
		}

		offset = row_merge_store_rec_to_block(m_rec->rec,
						      sec_offs, output, offset);

		m_rec = m_rec->next;
		ut_d(list->n_records--);
	}

	/* Now create a new list and store rest of the records there.
	Note that records must be copied because we deallocate memory
	allocated for the original list. */

	new_list = row_merge_create_list();

	while (m_rec) {
		rec_t*	rec;
		merge_rec_t*	n_rec;
		void*	buff;

		*sec_offsets_ = (sizeof sec_offsets_) / sizeof *sec_offsets_;

		sec_offs = rec_get_offsets(m_rec->rec, index, sec_offs,
					   ULINT_UNDEFINED, &heap);

		buff = mem_heap_alloc(new_list->heap,
				      rec_offs_size(sec_offs));

		n_rec = mem_heap_alloc(new_list->heap, sizeof(merge_rec_t));
		rec = rec_copy(buff, m_rec->rec, sec_offs);
		n_rec->rec = rec;
		row_merge_list_add(n_rec, rec_offs_size(sec_offs), new_list);
		m_rec = m_rec->next;
	}

	/* We can now free original list */
	mem_heap_free(list->heap);

	if (heap) {
		mem_heap_free(heap);
	}

	return(new_list);
}

#ifdef UNIV_DEBUG
/*************************************************************************
Validate contents of the block */
static
ibool
row_merge_block_validate(
/*=====================*/
	merge_block_t*	block,	/* in: block to be printed */
	dict_index_t*	index)	/* in: record descriptor */
{
	merge_rec_t*	mrec;
	ulint		offset	= 0;
	ulint		n_recs	= 0;
	mem_heap_t*	heap;
	ulint		sec_offsets1_[REC_OFFS_SMALL_SIZE];
	ulint*		sec_offs1 = sec_offsets1_;
	*sec_offsets1_ = (sizeof sec_offsets1_) / sizeof *sec_offsets1_;

	ut_a(block && index);

	heap = mem_heap_create(1024);

	fprintf(stderr,
		"Block validate %lu records, "
		"offset %lu, next %lu\n",
		block->header.n_records,
		block->header.offset, block->header.next);

	ut_a(block->header.n_records > 0);

	for (n_recs = 0; n_recs < block->header.n_records; n_recs++) {

		mrec = row_merge_read_rec_from_block(block, &offset, heap,
						     index);

		sec_offs1 = rec_get_offsets(mrec->rec, index, sec_offs1,
					    ULINT_UNDEFINED, &heap);

		ut_a(rec_validate(mrec->rec, sec_offs1));

		mem_heap_empty(heap);
	}

	mem_heap_free(heap);

	return(TRUE);
}
#endif /* UNIV_DEBUG */

/*************************************************************************
Merge two blocks resulting a two sorted blocks. */
static
merge_block_t*
row_merge_block_merge(
/*==================*/
					/* out: Pointer to first sorted block
					or NULL in case of error */
	merge_block_t*	block1,		/* in: First block to be merged */
	merge_block_t**	block2,		/* in/out: Second block to be merged.
					Note that contents of the second sorted
					block is returned with this parameter.*/
	dict_index_t*	index)		/* in: Index to be created */
{
	merge_block_t*	new_block1;
	merge_block_t*	new_block2;
	merge_block_t*	tmp;
	merge_rec_t*	mrec1;
	merge_rec_t*	mrec2;
	ulint		nth_rec1 = 0;
	ulint		nth_rec2 = 0;
	ulint		offset1 = 0;
	ulint		offset2 = 0;
	ulint		offset3 = 0;
	ulint		offset4 = 0;
	ibool		fits_to_new = TRUE;
	int		selected = 0;
	mem_heap_t*	heap;
	mem_heap_t*	offset_heap = NULL;
	ulint		sec_offsets1_[REC_OFFS_SMALL_SIZE];
	ulint*		sec_offs1 = sec_offsets1_;
	ulint		sec_offsets2_[REC_OFFS_SMALL_SIZE];
	ulint*		sec_offs2 = sec_offsets2_;

	ut_ad(block1 && block2 && *block2 && index);
	ut_ad(row_merge_block_validate(block1, index));
	ut_ad(row_merge_block_validate(*block2, index));

	*sec_offsets1_ = (sizeof sec_offsets1_) / sizeof *sec_offsets1_;
	*sec_offsets2_ = (sizeof sec_offsets2_) / sizeof *sec_offsets2_;

	new_block1 = row_merge_block_create();
	new_block2 = row_merge_block_create();
	tmp = *block2;
	heap = mem_heap_create(256);

	/* Copy block offset and next block offset to new blocks */

	new_block1->header = block1->header;
	new_block2->header = tmp->header;

	/* Merge all records from both blocks */

	while (nth_rec1 < block1->header.n_records ||
	       nth_rec2 < tmp->header.n_records) {
		const ulint*	rec_offsets;

		mrec1 = mrec2 = NULL;
		selected = 0;
		mem_heap_empty(heap);

		if (nth_rec1 < block1->header.n_records &&
		    nth_rec2 >= tmp->header.n_records) {

			/* If the second block is empty read record from
			the first block */

			mrec1 = row_merge_read_rec_from_block(
				block1, &offset1, heap, index);

			sec_offs1 = rec_get_offsets(
				mrec1->rec, index, sec_offs1, ULINT_UNDEFINED,
				&offset_heap);

			rec_offsets = sec_offs1;

			ut_ad(rec_validate(mrec1->rec, sec_offs1));

			nth_rec1++;

		} else if (nth_rec2 < tmp->header.n_records &&
			   nth_rec1 >= block1->header.n_records) {

			/* If the first block is empty read data tuple from
			the second block */

			mrec1 = row_merge_read_rec_from_block(
				tmp, &offset2, heap, index);

			sec_offs1 = rec_get_offsets(
				mrec1->rec, index, sec_offs1, ULINT_UNDEFINED,
				&offset_heap);

			rec_offsets = sec_offs1;

			ut_ad(rec_validate(mrec1->rec, sec_offs1));

			nth_rec2++;
		} else {
			ulint tmp_offset1 = offset1;
			ulint tmp_offset2 = offset2;

			/* Both blocks contain record and thus they must
			be compared */

			mrec1 = row_merge_read_rec_from_block(
				block1, &offset1, heap, index);

			sec_offs1 = rec_get_offsets(
				mrec1->rec, index, sec_offs1, ULINT_UNDEFINED,
				&offset_heap);

			ut_ad(rec_validate(mrec1->rec, sec_offs1));

			mrec2 = row_merge_read_rec_from_block(
				tmp, &offset2, heap, index);

			sec_offs2 = rec_get_offsets(
				mrec2->rec, index, sec_offs2, ULINT_UNDEFINED,
				&offset_heap);

			ut_ad(rec_validate(mrec2->rec, sec_offs2));

			switch (row_merge_cmp(mrec1, mrec2,
					      sec_offs1, sec_offs2, index)) {
			case 0:
				if (UNIV_UNLIKELY
				    (dict_index_is_unique(index))) {
					goto error_handling;
				}
				/* fall through */
			case 1:
				rec_offsets = sec_offs1;
				nth_rec1++;
				offset2 = tmp_offset2;
				break;
			case 2:
				mrec1 = mrec2;
				rec_offsets = sec_offs2;
				nth_rec2++;
				offset1 = tmp_offset1;
				break;
			}
		}

		ut_ad(mrec1);
		ut_ad(rec_validate(mrec1->rec, rec_offsets));

		/* If the first output block is not yet full test whether this
		new data tuple fits to block. If not this new data tuple must
		be inserted to second output block */

		if (fits_to_new) {
			fits_to_new = row_merge_rec_fits_to_block(
				rec_offsets, offset3);
		}

		if (fits_to_new) {
			offset3 = row_merge_store_rec_to_block(
				mrec1->rec, rec_offsets, new_block1, offset3);
		} else {
			offset4 = row_merge_store_rec_to_block(
				mrec1->rec, rec_offsets, new_block2, offset4);
		}

		/* TODO: If we are using variable length keys, then in
		some cases these keys do not fit to two empty blocks
		in a different order. Therefore, some empty space is
		left to every block. However, it has not been prooven
		that this empty space is enough in all cases. Therefore,
		here these overloaded records should be put on another
		block. */
	}

	/* Free memory from old blocks and return pointers to new blocks */

	if (offset_heap) {
		mem_heap_free(offset_heap);
	}

	mem_heap_free(heap);
	mem_free(block1);
	mem_free(tmp);

	ut_ad(row_merge_block_validate(new_block1, index));
	ut_ad(row_merge_block_validate(new_block2, index));

	*block2 = new_block2;

	return(new_block1);

error_handling:
	/* Duplicate key was found and unique key was requested. Free all
	allocated memory and return NULL */

	if (offset_heap) {
		mem_heap_free(offset_heap);
	}

	mem_heap_free(heap);
	mem_free(block1);
	mem_free(tmp);
	mem_free(new_block1);
	mem_free(new_block2);

	return(NULL);
}

/*****************************************************************
Merge sort for linked list in the disk.

Merge sort takes the input list and makes log N passes along
the list and in each pass it combines each adjacent pair of
small sorted lists into one larger sorted list. When only a one
pass is needed the whole output list must be sorted.

The linked list is stored in the file system.  File blocks represent
items of linked list.  The list is singly linked by the next offset
stored in block header. Offset is calculated from the start of the
file. Thus whenever next item in the list is requested this item is
read from the disk. Similarly every item is witten back to the disk
when we have sorted two blocks in the memory.

In each pass, two lists of size block_size are merged into lists of
size block_size*2. Initially block_size=1. Merge starts by pointing
a temporary pointer list1 at the head of the list and also preparing
an empty list list_tail where elements will be appended. Then:

	1) If block1 is NULL we terminate this pass.

	2) Otherwise, there is at least one element in the next
	pair of block_size lists therefore, increase the number of
	merges performed in this pass.

	3) Point another temporary pointer list2 as the same
	place as list1. Iterate list2 by block_size elements
	or until the end of the list. Let the list_size1 be the
	number of elements in the list2.

	4) Let list_size1=merge_size. Now we merge list starting at
	list1 of length list_size2 with a list starting at list2 of
	length at most list_size1.

	5) So, as long as either the list1 is non-empty (list_size1)
	or the list2 is non-empty (list_size2 and list2 pointing to
	a element):

		5.1) Select which list to take the next element from.
		If either lists is empty, we choose from the other one.
		If both lists are non-empty, compare the first element
		of each and choose the lower one.

		5.2) Remove that element, tmp, from the start of its
		lists, by advancing list1 or list2 to next element
		and decreasing list1_size or list2_size.

		5.3) Append tmp to list_tail

	6) At this point, we have advanced list1 until it is where
	list2 started out and we have advanced list2 until it is
	pointing at the next pair of block_size lists to merge.
	Thus, set list1 to the value of list2 and go back to the
	start of this loop.

As soon as a pass like this is performed with only one merge, the
algorithm terminates. Otherwise, double the value of block_size
and go back to the beginning. */

ulint
row_merge_sort_linked_list_in_disk(
/*===============================*/
					/* out: offset to first block in
					the list or ULINT_UNDEFINED in
					case of error */
	dict_index_t*	index,		/* in: index to be created */
	os_file_t	file,		/* in: File handle */
	int*		error)		/* out: 0 or error */
{
	merge_block_t*		block1;
	merge_block_t*		block2;
	merge_block_t*		backup1;
	merge_block_t*		backup2;
	merge_file_t		output;
	ulint			block_size;
	ulint			list_head	= 0;

	ut_ad(index);

	/* Allocate memory for blocks */
	backup1 = block1 = row_merge_block_create();
	backup2 = block2 = row_merge_block_create();

	output.file = file;

	for (block_size = 1;; block_size *= 2) {
		ibool	sorted		= TRUE;
		ibool	list_is_empty	= TRUE;

		block1 = backup1;

		if (!row_merge_block_read(file, list_head, block1)) {
file_error:
			*error = DB_CORRUPTION;
			goto err_exit;
		}
		ut_ad(row_merge_block_validate(block1, index));

		for (;;) {
			ulint	offset		= block1->header.offset;
			ulint	list1_size	= 0;
			ulint	list2_size	= block_size;
			ulint	i;

			/* Count how many list elements we have in the list. */

			for (i = 0; i < block_size; i++) {
				merge_block_header_t	header;

				list1_size++;

				/* Here read only the header to iterate the
				list in the disk. */

				if (!row_merge_block_header_read(file, offset,
								 &header)) {
					goto file_error;
				}

				offset = header.next;

				/* If the offset is zero we have arrived to the
				end of disk list */

				if (!offset) {
					break;
				}
			}

			/* If offset is zero we have reached end of the list in
			the disk. */

			if (!offset) {
				block2 = NULL;
			} else {
				block2 = backup2;
				if (!row_merge_block_read(
					    file, offset, block2)) {
					goto file_error;
				}
				ut_ad(row_merge_block_validate(block2, index));
			}

			/* If list2 is not empty, we have two lists to merge.
			Otherwice, we have a sorted list. */

			while (list1_size > 0 || (list2_size > 0 && block2)) {
				/* Merge sort two lists by deciding whether
				next element of merge comes from list1 or
				list2. */
				merge_block_t*	tmp;

				if (list1_size == 0) {
					/* First list is empty, next element
					must come from the second list. */

					tmp = block2;

					if (!block2->header.next) {
						block2 = NULL;
						list2_size = 0;
					} else {
						list2_size--;
					}
				} else if (list2_size == 0 || !block2) {
					/* Second list is empty, next record
					must come from the first list. */

					tmp = block1;
					list1_size--;
				} else {
					/* Both lists contain a block and we
					need to merge records on these block */

					tmp = row_merge_block_merge(
						block1, &block2, index);

					if (tmp == NULL) {
						*error = DB_DUPLICATE_KEY;
						goto err_exit;
					}

					block1 = backup1 = tmp;
					backup2 = block2;

					list1_size--;
				}

				/* Store the head offset of the disk
				list.  Note that only records in the
				blocks are changed not the order of
				the blocks in the disk. */

				if (list_is_empty) {
					list_is_empty = FALSE;
					list_head = tmp->header.offset;
				}

				ut_ad(row_merge_block_validate(tmp, index));

				if (!row_merge_block_write(
					    file, tmp->header.offset, tmp)) {
					goto file_error;
				}

				/* Now we can read the next record from the
				selected list if it contains more records */

				if (tmp->header.next
				    && !row_merge_block_read(file,
							     tmp->header.next,
							     tmp)) {
					goto file_error;
				}
			}

			/* Now we have processed block_size items from
			the disk.  Swap blocks using pointers. */

			if (!block2) {
				break;
			}

			block2 = backup1;
			block1 = backup2;
			backup2 = block2;
			backup1 = block1;

			sorted = FALSE;
		}

		if (sorted) {

			mem_free(backup1);
			mem_free(backup2);

			return(list_head);
		}
	}

err_exit:
	mem_free(backup1);
	mem_free(backup2);
	return(ULINT_UNDEFINED);
}

/************************************************************************
Merge sort linked list in the memory and store part of the linked
list into a block and write this block to the disk. */
static
ibool
row_merge_sort_and_store(
/*=====================*/
					/* out: FALSE on error */
	dict_index_t*		index,	/* in: Index */
	merge_file_t*		file,	/* in: File where to write index
					entries */
	merge_block_t*		block,	/* in/out: Block where to store
					the list */
	merge_rec_list_t**	list)	/* in/out: Pointer to the list */
{
	ut_ad(index && file && block && list);

	/* Firstly, merge sort linked list in the memory */
	if (!row_merge_sort_linked_list(index, *list)) {
		return(FALSE);
	}

	/* Secondly, write part of the linked list to the block */
	*list = row_merge_write_list_to_block(*list, block, index);

	ut_ad(row_merge_block_validate(block, index));

	/* Next block will be written directly behind this one. This will
	create a 'linked list' of blocks to the disk. */

	block->header.offset = file->offset;
	block->header.next = ++file->offset;

	/* Thirdly, write block to the disk */
	return(row_merge_block_write(file->file, block->header.offset, block));
}

#ifdef UNIV_DEBUG_INDEX_CREATE
/************************************************************************
Pretty print data tuple */
static
void
row_merge_dtuple_print(
/*===================*/
	FILE*		f,	/* in: output stream */
	dtuple_t*	dtuple)	/* in: data tuple */
{
	ulint		n_fields;
	ulint		i;

	ut_ad(f && dtuple);

	n_fields = dtuple_get_n_fields(dtuple);

	fprintf(f, "DATA TUPLE: %lu fields;\n", (ulong) n_fields);

	for (i = 0; i < n_fields; i++) {
		dfield_t*	dfield;

		dfield = dtuple_get_nth_field(dtuple, i);

		fprintf(f, "%lu: ", (ulong) i);

		if (dfield->len != UNIV_SQL_NULL) {
			dfield_print_also_hex(dfield);
		} else {
			fputs(" SQL NULL", f);
		}

		putc(';', f);
	}

	putc('\n', f);
	ut_ad(dtuple_validate(dtuple));
}
#endif /* UNIV_DEBUG_INDEX_CREATE */

/************************************************************************
Reads clustered index of the table and create temporary files
containing index entries for indexes to be built. */

ulint
row_merge_read_clustered_index(
/*===========================*/
					/* out: DB_SUCCESS if successfull,
					or ERROR code */
	trx_t*		trx,		/* in: transaction */
	dict_table_t*	table,		/* in: table where index is created */
	dict_index_t**	index,		/* in: indexes to be created */
	merge_file_t*	files,		/* in: Files where to write index
					entries */
	ulint		num_of_idx)	/* in: number of indexes to be
					created */
{
	dict_index_t*	clust_index;		/* Cluster index */
	merge_rec_t*	new_mrec;		/* New merge record */
	mem_heap_t*	row_heap;		/* Heap memory to create
						clustered index records */
	mem_heap_t*	heap;			/* Memory heap for
						record lists and offsets */
	merge_block_t*	block;			/* Merge block where records
						are stored for memory sort and
						then written to the disk */
	merge_rec_list_t**	merge_list;	/* Temporary list for records*/
	rec_t*		rec;			/* Record in the persistent
						cursor*/
	btr_pcur_t	pcur;			/* Persistent cursor on the
						cluster index */
	mtr_t		mtr;			/* Mini transaction */
	ibool		more_records_exists;	/* TRUE if we reached end of
						the cluster index */
	ulint		err = DB_SUCCESS;	/* Return code */
	ulint		idx_num = 0;		/* Index number */
	ulint		n_blocks = 0;		/* Number of blocks written
						to disk */
	ulint		sec_offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		sec_offs	= sec_offsets_;

	*sec_offsets_ = (sizeof sec_offsets_) / sizeof *sec_offsets_;

	trx->op_info="reading cluster index";

	ut_a(trx && table && index && files);

	/* Create block where index entries are stored */
	block = row_merge_block_create();

	/* Create and initialize memory for record lists */

	heap = mem_heap_create(256);
	merge_list = mem_heap_alloc(heap, num_of_idx * sizeof *merge_list);

	for (idx_num = 0; idx_num < num_of_idx; idx_num++) {
		merge_list[idx_num] = row_merge_create_list();
	}

	mtr_start(&mtr);

	/* Find the clustered index and create a persistent cursor
	based on that. */

	clust_index = dict_table_get_first_index(table);

	btr_pcur_open_at_index_side(
		TRUE, clust_index, BTR_SEARCH_LEAF, &pcur, TRUE, &mtr);

	row_heap = mem_heap_create(512);

	/* Get first record from the clustered index */
	rec = btr_pcur_get_rec(&pcur);

	/* Iterate all records in the clustered index */
	while (rec) {
		dtuple_t*	row;
		row_ext_t*	ext;

		/* Infimum and supremum records are skipped */

		if (!page_rec_is_user_rec(rec)) {

			goto next_record;

		/* We don't count the delete marked records as "Inserted" */
		} else if (!rec_get_deleted_flag(rec, page_rec_is_comp(rec))) {

			srv_n_rows_inserted++;
		}

		/* Build row based on clustered index */
		mem_heap_empty(row_heap);

		row = row_build(ROW_COPY_POINTERS,
				clust_index, rec, NULL, &ext, row_heap);

		/* If the user has requested the creation of several indexes
		for the same table. We build all index entries in a single
		pass over the cluster index. */

		for (idx_num = 0; idx_num < num_of_idx; idx_num++) {

			dtuple_t*	index_tuple;

			index_tuple = row_build_index_entry(
				row, ext,
				index[idx_num], merge_list[idx_num]->heap);

#ifdef UNIV_DEBUG_INDEX_CREATE
			row_merge_dtuple_print(stderr, index_tuple);
#endif

			new_mrec = row_merge_rec_create(
				index_tuple,
				ext ? ext->ext : NULL, ext ? ext->n_ext : 0,
				index[idx_num], merge_list[idx_num]->heap);

			sec_offs = rec_get_offsets(
				new_mrec->rec, index[idx_num], sec_offs,
				ULINT_UNDEFINED, &heap);

			/* Add data tuple to linked list of data tuples */

			row_merge_list_add(
				new_mrec, rec_offs_size(sec_offs),
				merge_list[idx_num]);

			/* If we have enough data tuples to form a block
			sort linked list and store it to the block and
			write this block to the disk. Note that not all
			data tuples in the list fit to the block.*/

			if (merge_list[idx_num]->total_size >=
			    MERGE_BLOCK_SIZE) {

				if (!row_merge_sort_and_store(
					    index[idx_num],
					    &files[idx_num],
					    block,
					    &(merge_list[idx_num]))) {

					trx->error_key_num = idx_num;
					err = DB_DUPLICATE_KEY;
					goto error_handling;
				}

				n_blocks++;
				files[idx_num].num_of_blocks++;
			}
		}


next_record:
		/* Persistent cursor has to be stored and mtr committed
		if we move to a new page in cluster index. */

		if (btr_pcur_is_after_last_on_page(&pcur, &mtr)) {
			btr_pcur_store_position(&pcur, &mtr);
			mtr_commit(&mtr);
			mtr_start(&mtr);
			btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
		}

		more_records_exists = btr_pcur_move_to_next(&pcur, &mtr);

		/* If no records are left we have created file for merge
		sort */

		if (more_records_exists == TRUE) {
			rec = btr_pcur_get_rec(&pcur);
		} else {
			rec = NULL;
		}
	}

	/* Now we have to write all remaining items in the list to
	blocks and write these blocks to the disk */

	for (idx_num = 0; idx_num < num_of_idx; idx_num++) {

		/* While we have items in the list write them
		to the block */
		ut_ad(!merge_list[idx_num]->head
		      == !merge_list[idx_num]->tail);
		ut_ad(!merge_list[idx_num]->n_records
		      == !merge_list[idx_num]->head);

		if (merge_list[idx_num]->head) {

			/* Next block will be written directly
			behind this one. This will create a
			'linked list' of blocks to the disk. */

			block->header.offset = files[idx_num].offset;
			block->header.next = files[idx_num].offset + 1;

			if (!row_merge_sort_and_store(
				    index[idx_num],
				    &files[idx_num],
				    block,
				    &(merge_list[idx_num]))) {

				trx->error_key_num = idx_num;
				err = DB_DUPLICATE_KEY;
				goto error_handling;
			}

			files[idx_num].num_of_blocks++;
			n_blocks++;
		}

		/* Write the last block. */
		block->header.next = 0; /* end-of-list marker */

		if (!row_merge_block_header_write(
			    files[idx_num].file, &block->header)) {
			err = DB_CORRUPTION;
			goto error_handling;
		}
	}

#ifdef UNIV_DEBUG_INDEX_CREATE
	fprintf(stderr, "Stored %lu blocks\n", n_blocks);
#endif

error_handling:

	/* Cleanup resources */

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(row_heap);
	mem_free(block);

	for (idx_num = 0; idx_num < num_of_idx; idx_num++) {
		mem_heap_free(merge_list[idx_num]->heap);
	}

	mem_heap_free(heap);

	trx->op_info="";

	return(err);
}

/************************************************************************
Read sorted file containing index data tuples and insert these data
tuples to the index */

ulint
row_merge_insert_index_tuples(
/*==========================*/
					/* out: 0 or error number */
	trx_t*		trx,		/* in: transaction */
	dict_index_t*	index,		/* in: index */
	dict_table_t*	table,		/* in: table */
	os_file_t	file,		/* in: file handle */
	ulint		offset)		/* in: offset where to start
					reading */
{
	merge_block_t*	block;
	que_thr_t*	thr;
	ins_node_t*	node;
	mem_heap_t*	heap;
	mem_heap_t*	graph_heap;
	ulint		error = DB_SUCCESS;

	ut_ad(trx && index && table);

	/* We use the insert query graph as the dummy graph
	needed in the row module call */

	trx->op_info = "inserting index entries";

	graph_heap = mem_heap_create(500);
	node = ins_node_create(INS_DIRECT, table, graph_heap);

	thr = pars_complete_graph_for_exec(node, trx, graph_heap);

	que_thr_move_to_run_state_for_mysql(thr, trx);

	block = row_merge_block_create();
	heap = mem_heap_create(1000);

	do {
		ulint	n_rec;
		ulint	tuple_offset = 0;

		if (!row_merge_block_read(file, offset, block)) {
			error = DB_CORRUPTION;
			break;
		}

		ut_ad(row_merge_block_validate(block, index));

		for (n_rec = 0; n_rec < block->header.n_records; n_rec++) {
			merge_rec_t*	mrec = row_merge_read_rec_from_block(
				block, &tuple_offset, heap, index);

			if (!rec_get_deleted_flag(mrec->rec, 0)) {

				dtuple_t* dtuple = row_rec_to_index_entry(
					ROW_COPY_POINTERS,
					index, mrec->rec, heap);

				node->row = dtuple;
				node->table = table;
				node->trx_id = trx->id;

				ut_ad(dtuple_validate(dtuple));

#ifdef UNIV_DEBUG_INDEX_CREATE
				row_merge_dtuple_print(stderr, dtuple);
#endif

				do {
					thr->run_node = thr;
					thr->prev_node = thr->common.parent;

					error = row_ins_index_entry(
						index, dtuple, NULL, 0, thr);

					if (error == DB_SUCCESS) {
						goto next_rec;
					}

					thr->lock_state = QUE_THR_LOCK_ROW;
					trx->error_state = error;
					que_thr_stop_for_mysql(thr);
					thr->lock_state = QUE_THR_LOCK_NOLOCK;
				} while (row_mysql_handle_errors(&error, trx,
								 thr, NULL));

				goto err_exit;
			}

next_rec:
			mem_heap_empty(heap);
		}

		offset = block->header.next;

		/* If we have reached the end of the disk list we have
		inserted all of the index entries to the index. */
	} while (offset);

	que_thr_stop_for_mysql_no_error(thr, trx);
err_exit:
	que_graph_free(thr->graph);

	trx->op_info = "";

	mem_free(block);
	mem_heap_free(heap);

	return(error);
}

/*************************************************************************
Remove a index from system tables */

ulint
row_merge_remove_index(
/*===================*/
				/* out: error code or DB_SUCCESS */
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

	return(err);
}

/*************************************************************************
Allocate and initialize memory for a merge file structure */

void
row_merge_file_create(
/*==================*/
	merge_file_t*	merge_file)	/* out: merge file structure */
{
	merge_file->file = innobase_mysql_tmpfile();

	merge_file->offset = 0;
	merge_file->num_of_blocks = 0;
}

#ifdef UNIV_DEBUG_INDEX_CREATE
/*************************************************************************
Print definition of a table in the dictionary */

void
row_merge_print_table(
/*==================*/
	dict_table_t*	table)	/* in: table */
{
	dict_table_print(table);
}
#endif

/*************************************************************************
Mark all prebuilts using the table obsolete. These prebuilts are
rebuilt later. */

void
row_merge_mark_prebuilt_obsolete(
/*=============================*/

	trx_t*		trx,		/* in: trx */
	dict_table_t*	table)		/* in: table */
{
	row_prebuilt_t*	prebuilt;

	row_mysql_lock_data_dictionary(trx);

	prebuilt = UT_LIST_GET_FIRST(table->prebuilts);

	while (prebuilt) {
		prebuilt->magic_n = ROW_PREBUILT_OBSOLETE;
		prebuilt->magic_n2 = ROW_PREBUILT_OBSOLETE;

		prebuilt = UT_LIST_GET_NEXT(prebuilts, prebuilt);
	}

	/* This table will be dropped when there are no more references
	to it */
	table->to_be_dropped = 1;

	row_mysql_unlock_data_dictionary(trx);
}

/*************************************************************************
Create a temporary table using a definition of the old table. You must
lock data dictionary before calling this function. */

dict_table_t*
row_merge_create_temporary_table(
/*=============================*/
					/* out: new temporary table
					definition */
	const char*	table_name,	/* in: new table name */
	dict_table_t*	table,		/* in: old table definition */
	trx_t*		trx,		/* in: trx */
	ulint*		error)		/* in:out/ error code or DB_SUCCESS */
{
	ulint		i;
	dict_table_t*	new_table = NULL;
	ulint		n_cols = dict_table_get_n_user_cols(table);

	ut_ad(table_name && table && error);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&dict_sys->mutex));
#endif /* UNIV_SYNC_DEBUG */

	*error = row_undo_report_create_table_dict_operation(trx, table_name);

	if (*error == DB_SUCCESS) {

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

		*error = row_create_table_for_mysql(new_table, trx);
		mem_heap_free(heap);
	}

	return(new_table);
}

/*************************************************************************
Rename the indexes in the dicitionary. */

ulint
row_merge_rename_index(
/*===================*/
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

	pars_info_add_str_literal(info, "name", index->name);
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
Create the index and load in to the dicitionary. */

ulint
row_merge_create_index(
/*===================*/
	trx_t*		trx,		/* in: transaction */
	dict_index_t**	index,		/* out: the instance of the index */
	dict_table_t*	table,		/* in: the index is on this table */
	const merge_index_def_t*	/* in: the index definition */
			index_def)
{
	ulint		err = DB_SUCCESS;
	ulint		n_fields = index_def->n_fields;

	/* Create the index prototype, using the passed in def, this is not
	a persistent operation. We pass 0 as the space id, and determine at
	a lower level the space id where to store the table.*/

	*index = dict_mem_index_create(
		table->name, index_def->name, 0, index_def->ind_type, n_fields);

	ut_a(*index);

	/* Create the index id, as it will be required when we build
	the index. We assign the id here because we want to write an
	UNDO record before we insert the entry into SYS_INDEXES.*/
	ut_a(ut_dulint_is_zero((*index)->id));

	(*index)->id = dict_hdr_get_new_id(DICT_HDR_INDEX_ID);
	(*index)->table = table;

	/* Write the UNDO record for the create index */
	err = row_undo_report_create_index_dict_operation(trx, *index);

	if (err == DB_SUCCESS) {
		ulint		i;

		/* Make sure the UNDO record gets to disk */
		log_buffer_flush_to_disk();

		for (i = 0; i < n_fields; i++) {
			merge_index_field_t* ifield;

			ifield = &index_def->fields[i];

			dict_mem_index_add_field(*index,
						 ifield->field_name,
						 ifield->prefix_len);
		}

		/* Add the index to SYS_INDEXES, this will use the prototype
		to create an entry in SYS_INDEXES.*/
		err = row_create_index_graph_for_mysql(trx, table, *index);

		if (err == DB_SUCCESS) {

			*index = row_merge_dict_table_get_index(
				table, index_def);

			ut_a(*index);

			/* Note the id of the transaction that created this
			index, we use it to restrict readers from accessing
			this index, to ensure read consistency.*/
			(*index)->trx_id = trx->id;

			/* Create element and append to list in trx. So that
			we can rename from temp name to real name.*/
			if (trx->dict_redo_list) {
				dict_redo_t*	dict_redo;

				dict_redo = dict_redo_create_element(trx);
				dict_redo->index = *index;
			}
		}
	}

	return(err);
}

/*************************************************************************
Check if a transaction can use an index.*/

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
Drop the old table.*/

ulint
row_merge_drop_table(
/*=================*/
					/* out: DB_SUCCESS if all OK else
					error code.*/
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
		/* Set the commit flag to FALSE.*/
		err = row_drop_table_for_mysql(table->name, trx, FALSE);
	}

	if (dict_locked) {
		row_mysql_unlock_data_dictionary(trx);
	}

	return(err);
}

