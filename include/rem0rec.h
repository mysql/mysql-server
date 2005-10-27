/************************************************************************
Record manager

(c) 1994-1996 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#ifndef rem0rec_h
#define rem0rec_h

#include "univ.i"
#include "data0data.h"
#include "rem0types.h"
#include "mtr0types.h"
#include "page0types.h"

/* Maximum values for various fields (for non-blob tuples) */
#define REC_MAX_N_FIELDS	(1024 - 1)

/* Flag denoting the predefined minimum record: this bit is ORed in the 4
info bits of a record */
#define REC_INFO_MIN_REC_FLAG	0x10UL

/* Number of extra bytes in an old-style record,
in addition to the data and the offsets */
#define REC_N_OLD_EXTRA_BYTES	6
/* Number of extra bytes in a new-style record,
in addition to the data and the offsets */
#define REC_N_NEW_EXTRA_BYTES	5

/* Record status values */
#define REC_STATUS_ORDINARY	0
#define REC_STATUS_NODE_PTR	1
#define REC_STATUS_INFIMUM	2
#define REC_STATUS_SUPREMUM	3

/* Number of elements that should be initially allocated for the
offsets[] array, first passed to rec_get_offsets() */
#define REC_OFFS_NORMAL_SIZE	100
#define REC_OFFS_SMALL_SIZE	10

/**********************************************************
The following function is used to get the pointer of the next chained record
on the same page. */
UNIV_INLINE
rec_t*
rec_get_next_ptr(
/*=============*/
			/* out: pointer to the next chained record, or
			NULL if none */
	rec_t*	rec,	/* in: physical record */
	ulint	comp);	/* in: nonzero=compact page format */
/**********************************************************
The following function is used to get the offset of the
next chained record on the same page. */
UNIV_INLINE
ulint 
rec_get_next_offs(
/*==============*/
			/* out: the page offset of the next 
			chained record */
	rec_t*	rec,	/* in: physical record */
	ulint	comp);	/* in: nonzero=compact page format */
/**********************************************************
The following function is used to set the next record offset field
of an old-style record. */
UNIV_INLINE
void
rec_set_next_offs_old(
/*==================*/
	rec_t*	rec,	/* in: old-style physical record */
	ulint	next);	/* in: offset of the next record */
/**********************************************************
The following function is used to set the next record offset field
of a new-style record. */
UNIV_INLINE
void
rec_set_next_offs_new(
/*==================*/
	rec_t*		rec,	/* in/out: new-style physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				6 bytes available, or NULL */
	ulint		next);	/* in: offset of the next record */
/**********************************************************
The following function is used to get the number of fields
in an old-style record. */
UNIV_INLINE
ulint
rec_get_n_fields_old(
/*=================*/
			/* out: number of data fields */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to get the number of fields
in a record. */
UNIV_INLINE
ulint
rec_get_n_fields(
/*=============*/
				/* out: number of data fields */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index);	/* in: record descriptor */
/**********************************************************
The following function is used to get the number of records owned by the
previous directory record. */
UNIV_INLINE
ulint
rec_get_n_owned_old(
/*================*/
			/* out: number of owned records */
	rec_t*	rec);	/* in: old-style physical record */
/**********************************************************
The following function is used to set the number of owned records. */
UNIV_INLINE
void
rec_set_n_owned_old(
/*================*/
				/* out: TRUE on success */
	rec_t*	rec,		/* in: old-style physical record */
	ulint	n_owned);	/* in: the number of owned */
/**********************************************************
The following function is used to get the number of records owned by the
previous directory record. */
UNIV_INLINE
ulint
rec_get_n_owned_new(
/*================*/
			/* out: number of owned records */
	rec_t*	rec);	/* in: new-style physical record */
/**********************************************************
The following function is used to set the number of owned records. */
UNIV_INLINE
void
rec_set_n_owned_new(
/*================*/
				/* out: TRUE on success */
	rec_t*		rec,	/* in/out: new-style physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				5 bytes available, or NULL */
	ulint		n_owned);/* in: the number of owned */
/**********************************************************
The following function is used to retrieve the info bits of
a record. */
UNIV_INLINE
ulint
rec_get_info_bits(
/*==============*/
			/* out: info bits */
	rec_t*	rec,	/* in: physical record */
	ulint	comp);	/* in: nonzero=compact page format */
/**********************************************************
The following function is used to set the info bits of a record. */
UNIV_INLINE
void
rec_set_info_bits_old(
/*==================*/
	rec_t*	rec,	/* in: old-style physical record */
	ulint	bits);	/* in: info bits */
/**********************************************************
The following function is used to set the info bits of a record. */
UNIV_INLINE
void
rec_set_info_bits_new(
/*==================*/
	rec_t*		rec,	/* in/out: new-style physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with
				at least 5 bytes available, or NULL */
	ulint		bits);	/* in: info bits */
/**********************************************************
The following function retrieves the status bits of a new-style record. */
UNIV_INLINE
ulint
rec_get_status(
/*===========*/
			/* out: status bits */
	rec_t*	rec);	/* in: physical record */

/**********************************************************
The following function is used to set the status bits of a new-style record. */
UNIV_INLINE
void
rec_set_status(
/*===========*/
	rec_t*		rec,	/* in/out: physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with
				at least 5 bytes available, or NULL */
	ulint		bits);	/* in: info bits */

/**********************************************************
The following function is used to retrieve the info and status
bits of a record.  (Only compact records have status bits.) */
UNIV_INLINE
ulint
rec_get_info_and_status_bits(
/*=========================*/
			/* out: info bits */
	rec_t*	rec,	/* in: physical record */
	ulint	comp);	/* in: nonzero=compact page format */
/**********************************************************
The following function is used to set the info and status
bits of a record.  (Only compact records have status bits.) */
UNIV_INLINE
void
rec_set_info_and_status_bits(
/*=========================*/
	rec_t*		rec,	/* in/out: compact physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with
				at least 5 bytes available, or NULL */
	ulint		bits);	/* in: info bits */

/**********************************************************
The following function tells if record is delete marked. */
UNIV_INLINE
ulint
rec_get_deleted_flag(
/*=================*/
			/* out: nonzero if delete marked */
	rec_t*	rec,	/* in: physical record */
	ulint	comp);	/* in: nonzero=compact page format */
/**********************************************************
The following function is used to set the deleted bit. */
UNIV_INLINE
void
rec_set_deleted_flag_old(
/*=====================*/
	rec_t*	rec,	/* in: old-style physical record */
	ulint	flag);	/* in: nonzero if delete marked */
/**********************************************************
The following function is used to set the deleted bit. */
UNIV_INLINE
void
rec_set_deleted_flag_new(
/*=====================*/
	rec_t*		rec,	/* in/out: new-style physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with
				at least 5 bytes available, or NULL */
	ulint		flag);	/* in: nonzero if delete marked */
/**********************************************************
The following function tells if a new-style record is a node pointer. */
UNIV_INLINE
ibool
rec_get_node_ptr_flag(
/*==================*/
			/* out: TRUE if node pointer */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to get the order number
of an old-style record in the heap of the index page. */
UNIV_INLINE
ulint
rec_get_heap_no_old(
/*================*/
			/* out: heap order number */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the heap number
field in an old-style record. */
UNIV_INLINE
void
rec_set_heap_no_old(
/*================*/
	rec_t*	rec,	/* in: physical record */
	ulint	heap_no);/* in: the heap number */
/**********************************************************
The following function is used to get the order number
of a new-style record in the heap of the index page. */
UNIV_INLINE
ulint
rec_get_heap_no_new(
/*================*/
			/* out: heap order number */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the heap number
field in a new-style record. */
UNIV_INLINE
void
rec_set_heap_no_new(
/*================*/
	rec_t*		rec,	/* in/out: physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with
				at least 6 bytes available, or NULL */
	ulint		heap_no);/* in: the heap number */
/**********************************************************
The following function is used to test whether the data offsets
in the record are stored in one-byte or two-byte format. */
UNIV_INLINE
ibool
rec_get_1byte_offs_flag(
/*====================*/
			/* out: TRUE if 1-byte form */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function determines the offsets to each field
in the record.  It can reuse a previously allocated array. */

ulint*
rec_get_offsets_func(
/*=================*/
				/* out: the new offsets */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint*		offsets,/* in: array consisting of offsets[0]
				allocated elements, or an array from
				rec_get_offsets(), or NULL */
	ulint		n_fields,/* in: maximum number of initialized fields
				(ULINT_UNDEFINED if all fields) */
	mem_heap_t**	heap,	/* in/out: memory heap */
	const char*	file,	/* in: file name where called */
	ulint		line);	/* in: line number where called */

#define rec_get_offsets(rec,index,offsets,n,heap)	\
	rec_get_offsets_func(rec,index,offsets,n,heap,__FILE__,__LINE__)

/****************************************************************
Validates offsets returned by rec_get_offsets(). */
UNIV_INLINE
ibool
rec_offs_validate(
/*==============*/
				/* out: TRUE if valid */
	rec_t*		rec,	/* in: record or NULL */
	dict_index_t*	index,	/* in: record descriptor or NULL */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/****************************************************************
Updates debug data in offsets, in order to avoid bogus
rec_offs_validate() failures. */
UNIV_INLINE
void
rec_offs_make_valid(
/*================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,/* in: record descriptor */
	ulint*		offsets);/* in: array returned by rec_get_offsets() */

/****************************************************************
The following function is used to get a pointer to the nth
data field in an old-style record. */

byte*
rec_get_nth_field_old(
/*==================*/
 			/* out: pointer to the field */
 	rec_t*	rec, 	/* in: record */
 	ulint	n,	/* in: index of the field */
	ulint*	len);	/* out: length of the field; UNIV_SQL_NULL 
			if SQL null */
/****************************************************************
Gets the physical size of an old-style field.
Also an SQL null may have a field of size > 0,
if the data type is of a fixed size. */
UNIV_INLINE
ulint
rec_get_nth_field_size(
/*===================*/
			/* out: field size in bytes */
 	rec_t*	rec, 	/* in: record */
 	ulint	n);	/* in: index of the field */
/****************************************************************
The following function is used to get a pointer to the nth
data field in a record. */
UNIV_INLINE
byte*
rec_get_nth_field(
/*==============*/
	 			/* out: pointer to the field */
	rec_t*		rec, 	/* in: record */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		n,	/* in: index of the field */
	ulint*		len);	/* out: length of the field; UNIV_SQL_NULL
				if SQL null */
/**********************************************************
Determine if the offsets are for a record in the new
compact format. */
UNIV_INLINE
ulint
rec_offs_comp(
/*==========*/
				/* out: nonzero if compact format */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/**********************************************************
Returns nonzero if the extern bit is set in nth field of rec. */
UNIV_INLINE
ulint
rec_offs_nth_extern(
/*================*/
				/* out: nonzero if externally stored */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		n);	/* in: nth field */
/**********************************************************
Returns nonzero if the SQL NULL bit is set in nth field of rec. */
UNIV_INLINE
ulint
rec_offs_nth_sql_null(
/*==================*/
				/* out: nonzero if SQL NULL */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		n);	/* in: nth field */
/**********************************************************
Gets the physical size of a field. */
UNIV_INLINE
ulint
rec_offs_nth_size(
/*==============*/
				/* out: length of field */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		n);	/* in: nth field */

/**********************************************************
Returns TRUE if the extern bit is set in any of the fields
of rec. */
UNIV_INLINE
ibool
rec_offs_any_extern(
/*================*/
				/* out: TRUE if a field is stored externally */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/***************************************************************
Sets the ith field extern storage bit. */
UNIV_INLINE
void
rec_set_nth_field_extern_bit(
/*=========================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint		i,	/* in: ith field */
	mtr_t*		mtr);	/* in: mtr holding an X-latch to the page
				where rec is, or NULL; in the NULL case
				we do not write to log about the change */
/***************************************************************
Sets TRUE the extern storage bits of fields mentioned in an array. */

void
rec_set_field_extern_bits(
/*======================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: record descriptor */
	const ulint*	vec,	/* in: array of field numbers */
	ulint		n_fields,/* in: number of fields numbers */
	mtr_t*		mtr);	/* in: mtr holding an X-latch to the page
				where rec is, or NULL; in the NULL case
				we do not write to log about the change */
/*************************************************************** 
This is used to modify the value of an already existing field in a record.
The previous value must have exactly the same size as the new value. If len
is UNIV_SQL_NULL then the field is treated as an SQL null for old-style
records. For new-style records, len must not be UNIV_SQL_NULL. */
UNIV_INLINE
void
rec_set_nth_field(
/*==============*/
	rec_t*		rec, 	/* in: record */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		n,	/* in: index number of the field */
	const void*	data,	/* in: pointer to the data if not SQL null */
	ulint		len);	/* in: length of the data or UNIV_SQL_NULL.
				If not SQL null, must have the same
				length as the previous value.
				If SQL null, previous value must be
				SQL null. */
/************************************************************** 
The following function returns the data size of an old-style physical
record, that is the sum of field lengths. SQL null fields
are counted as length 0 fields. The value returned by the function
is the distance from record origin to record end in bytes. */
UNIV_INLINE
ulint
rec_get_data_size_old(
/*==================*/
				/* out: size */
	rec_t*	rec);	/* in: physical record */
/************************************************************** 
The following function returns the number of fields in a record. */
UNIV_INLINE
ulint
rec_offs_n_fields(
/*===============*/
				/* out: number of fields */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/**************************************************************
The following function returns the data size of a physical
record, that is the sum of field lengths. SQL null fields
are counted as length 0 fields. The value returned by the function
is the distance from record origin to record end in bytes. */
UNIV_INLINE
ulint
rec_offs_data_size(
/*===============*/
				/* out: size */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/**************************************************************
Returns the total size of record minus data size of record.
The value returned by the function is the distance from record 
start to record origin in bytes. */
UNIV_INLINE
ulint
rec_offs_extra_size(
/*================*/
				/* out: size */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/**************************************************************
Returns the total size of a physical record.  */
UNIV_INLINE
ulint
rec_offs_size(
/*==========*/
				/* out: size */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/**************************************************************
Returns a pointer to the start of the record. */
UNIV_INLINE
byte*
rec_get_start(
/*==========*/
				/* out: pointer to start */
	rec_t*		rec,	/* in: pointer to record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/**************************************************************
Returns a pointer to the end of the record. */
UNIV_INLINE
byte*
rec_get_end(
/*========*/
				/* out: pointer to end */
	rec_t*		rec,	/* in: pointer to record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/*******************************************************************
Copies a physical record to a buffer. */
UNIV_INLINE
rec_t*
rec_copy(
/*=====*/
				/* out: pointer to the origin of the copy */
	void*		buf,	/* in: buffer */
	const rec_t*	rec,	/* in: physical record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/******************************************************************
Copies the first n fields of a physical record to a new physical record in
a buffer. */

rec_t*
rec_copy_prefix_to_buf(
/*===================*/
					/* out, own: copied record */
	rec_t*		rec,		/* in: physical record */
	dict_index_t*	index,		/* in: record descriptor */
	ulint		n_fields,	/* in: number of fields to copy */
	byte**		buf,		/* in/out: memory buffer
					for the copied prefix, or NULL */
	ulint*		buf_size);	/* in/out: buffer size */
/****************************************************************
Folds a prefix of a physical record to a ulint. */
UNIV_INLINE
ulint
rec_fold(
/*=====*/
					/* out: the folded value */
	rec_t*		rec,		/* in: the physical record */
	const ulint*	offsets,	/* in: array returned by
					rec_get_offsets() */
	ulint		n_fields,	/* in: number of complete
					fields to fold */
	ulint		n_bytes,	/* in: number of bytes to fold
					in an incomplete last field */
	dulint		tree_id);	/* in: index tree id */
/*************************************************************
Builds a physical record out of a data tuple and
stores it into the given buffer. */

rec_t* 	
rec_convert_dtuple_to_rec(
/*======================*/			
				/* out: pointer to the origin
				of physical record */
	byte*		buf,	/* in: start address of the
				physical record */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	dtuple);/* in: data tuple */
/**************************************************************
Returns the extra size of an old-style physical record if we know its
data size and number of fields. */
UNIV_INLINE
ulint
rec_get_converted_extra_size(
/*=========================*/
				/* out: extra size */
	ulint	data_size,	/* in: data size */
	ulint	n_fields)	/* in: number of fields */
		__attribute__((const));
/**************************************************************
The following function returns the size of a data tuple when converted to
a physical record. */
UNIV_INLINE
ulint
rec_get_converted_size(
/*===================*/
				/* out: size */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	dtuple);/* in: data tuple */
/******************************************************************
Copies the first n fields of a physical record to a data tuple.
The fields are copied to the memory heap. */

void
rec_copy_prefix_to_dtuple(
/*======================*/
	dtuple_t*	tuple,		/* in: data tuple */
	rec_t*		rec,		/* in: physical record */
	dict_index_t*	index,		/* in: record descriptor */
	ulint		n_fields,	/* in: number of fields to copy */
	mem_heap_t*	heap);		/* in: memory heap */
/*******************************************************************
Validates the consistency of a physical record. */

ibool
rec_validate(
/*=========*/
				/* out: TRUE if ok */
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/*******************************************************************
Prints an old-style physical record. */

void
rec_print_old(
/*==========*/
	FILE*		file,	/* in: file where to print */
	rec_t*		rec);	/* in: physical record */
/*******************************************************************
Prints a physical record. */

void
rec_print_new(
/*==========*/
	FILE*		file,	/* in: file where to print */
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/*******************************************************************
Prints a physical record. */

void
rec_print(
/*======*/
	FILE*		file,	/* in: file where to print */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index);	/* in: record descriptor */

#define REC_INFO_BITS		6	/* This is single byte bit-field */

/* Maximum lengths for the data in a physical record if the offsets
are given in one byte (resp. two byte) format. */
#define REC_1BYTE_OFFS_LIMIT	0x7FUL
#define REC_2BYTE_OFFS_LIMIT	0x7FFFUL

/* The data size of record must be smaller than this because we reserve
two upmost bits in a two byte offset for special purposes */
#define REC_MAX_DATA_SIZE	(16 * 1024)

#ifndef UNIV_NONINL
#include "rem0rec.ic"
#endif

#endif
