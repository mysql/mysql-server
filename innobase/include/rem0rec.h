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

/* Maximum values for various fields (for non-blob tuples) */
#define REC_MAX_N_FIELDS	(1024 - 1)
#define REC_MAX_HEAP_NO		(2 * 8192 - 1)
#define REC_MAX_N_OWNED		(16 - 1)

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
	ibool	comp);	/* in: TRUE=compact page format */
/**********************************************************
The following function is used to set the next record offset field
of the record. */
UNIV_INLINE
void
rec_set_next_offs(
/*==============*/
	rec_t*	rec,	/* in: physical record */
	ibool	comp,	/* in: TRUE=compact page format */
	ulint	next);	/* in: offset of the next record */
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
The following function is used to get the number of records
owned by the previous directory record. */
UNIV_INLINE
ulint
rec_get_n_owned(
/*============*/
			/* out: number of owned records */
	rec_t*	rec,	/* in: physical record */
	ibool	comp);	/* in: TRUE=compact page format */
/**********************************************************
The following function is used to set the number of owned
records. */
UNIV_INLINE
void
rec_set_n_owned(
/*============*/
	rec_t*	rec,		/* in: physical record */
	ibool	comp,		/* in: TRUE=compact page format */
	ulint	n_owned);	/* in: the number of owned */
/**********************************************************
The following function is used to retrieve the info bits of
a record. */
UNIV_INLINE
ulint
rec_get_info_bits(
/*==============*/
			/* out: info bits */
	rec_t*	rec,	/* in: physical record */
	ibool	comp);	/* in: TRUE=compact page format */
/**********************************************************
The following function is used to set the info bits of a record. */
UNIV_INLINE
void
rec_set_info_bits(
/*==============*/
	rec_t*	rec,	/* in: physical record */
	ibool	comp,	/* in: TRUE=compact page format */
	ulint	bits);	/* in: info bits */
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
	rec_t*	rec,	/* in: physical record */
	ulint	bits);	/* in: info bits */

/**********************************************************
The following function tells if record is delete marked. */
UNIV_INLINE
ibool
rec_get_deleted_flag(
/*=================*/
			/* out: TRUE if delete marked */
	rec_t*	rec,	/* in: physical record */
	ibool	comp);	/* in: TRUE=compact page format */
/**********************************************************
The following function is used to set the deleted bit. */
UNIV_INLINE
void
rec_set_deleted_flag(
/*=================*/
	rec_t*	rec,	/* in: physical record */
	ibool	comp,	/* in: TRUE=compact page format */
	ibool	flag);	/* in: TRUE if delete marked */
/**********************************************************
The following function tells if a new-style record is a node pointer. */
UNIV_INLINE
ibool
rec_get_node_ptr_flag(
/*=================*/
			/* out: TRUE if node pointer */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to flag a record as a node pointer. */
UNIV_INLINE
void
rec_set_node_ptr_flag(
/*=================*/
	rec_t*	rec,	/* in: physical record */
	ibool	flag);	/* in: TRUE if the record is a node pointer */
/**********************************************************
The following function is used to get the order number
of the record in the heap of the index page. */
UNIV_INLINE
ulint
rec_get_heap_no(
/*=============*/
			/* out: heap order number */
	rec_t*	rec,	/* in: physical record */
	ibool	comp);	/* in: TRUE=compact page format */
/**********************************************************
The following function is used to set the heap number
field in the record. */
UNIV_INLINE
void
rec_set_heap_no(
/*=============*/
	rec_t*	rec,	/* in: physical record */
	ibool	comp,	/* in: TRUE=compact page format */
	ulint	heap_no);/* in: the heap number */
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
in the record.  The offsets are returned in an array of
ulint, with [0] being the number of fields (n), [1] being the
extra size (if REC_OFFS_COMPACT is set, the record is in the new
format), and [2]..[n+1] being the offsets past the end of
fields 0..n, or to the beginning of fields 1..n+1.  When the
high-order bit of the offset at [n+1] is set (REC_OFFS_SQL_NULL),
the field n is NULL.  When the second high-order bit of the offset
at [n+1] is set (REC_OFFS_EXTERNAL), the field n is being stored
externally. */

ulint*
rec_get_offsets(
/*============*/
				/* out: the offsets */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint		n_fields,/* in: maximum number of initialized fields
				(ULINT_UNDEFINED if all fields) */
	mem_heap_t*	heap);	/* in: memory heap */
/**********************************************************
The following function determines the offsets to each field
in the record.  It differs from rec_get_offsets() by trying to
reuse a previously returned array. */

ulint*
rec_reget_offsets(
/*==============*/
				/* out: the new offsets */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint*		offsets,/* in: array of offsets
				from rec_get_offsets()
				or rec_reget_offsets(), or NULL */
	ulint		n_fields,/* in: maximum number of initialized fields
				(ULINT_UNDEFINED if all fields) */
	mem_heap_t*	heap);	/* in: memory heap */

/****************************************************************
Validates offsets returned by rec_get_offsets() or rec_reget_offsets(). */
UNIV_INLINE
ibool
rec_offs_validate(
/*==============*/
				/* out: TRUE if valid */
	rec_t*		rec,	/* in: record or NULL */
	dict_index_t*	index,	/* in: record descriptor or NULL */
	const ulint*	offsets);/* in: array returned by rec_get_offsets()
				or rec_reget_offsets() */
/****************************************************************
Updates debug data in offsets, in order to avoid bogus
rec_offs_validate() failures. */
UNIV_INLINE
void
rec_offs_make_valid(
/*================*/
	const rec_t*	rec,	/* in: record */
	const dict_index_t* index,/* in: record descriptor */
	ulint*		offsets);/* in: array returned by rec_get_offsets()
				or rec_reget_offsets() */

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
data field in an old-style record. */
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
ibool
rec_offs_comp(
/*==========*/
				/* out: TRUE if compact format */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/**********************************************************
Returns TRUE if the nth field of rec is SQL NULL. */
UNIV_INLINE
ibool
rec_offs_nth_null(
/*==============*/
				/* out: TRUE if SQL NULL */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		n);	/* in: nth field */
/**********************************************************
Returns TRUE if the extern bit is set in nth field of rec. */
UNIV_INLINE
ibool
rec_offs_nth_extern(
/*================*/
				/* out: TRUE if externally stored */
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
Sets the value of the ith field extern storage bit. */
UNIV_INLINE
void
rec_set_nth_field_extern_bit(
/*=========================*/
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: record descriptor */
	ulint		i,	/* in: ith field */
	ibool		val,	/* in: value to set */
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
Returns the total size of a physical record.  */

ulint
rec_get_size(
/*=========*/
				/* out: size */
	rec_t*		rec,	/* in: physical record */
	dict_index_t*	index);	/* in: record descriptor */
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
Builds a physical record out of a data tuple and stores it beginning from
address destination. */

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
rec_print(
/*======*/
	FILE*		file,	/* in: file where to print */
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */

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
