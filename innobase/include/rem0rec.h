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

/* Maximum values for various fields (for non-blob tuples) */
#define REC_MAX_N_FIELDS	(1024 - 1)
#define REC_MAX_HEAP_NO		(2 * 8192 - 1)
#define REC_MAX_N_OWNED		(16 - 1)

/* Flag denoting the predefined minimum record: this bit is ORed in the 4
info bits of a record */
#define REC_INFO_MIN_REC_FLAG	0x10

/* Number of extra bytes in a record, in addition to the data and the
offsets */
#define REC_N_EXTRA_BYTES	6

/**********************************************************
The following function is used to get the offset of the
next chained record on the same page. */
UNIV_INLINE
ulint 
rec_get_next_offs(
/*==============*/
			/* out: the page offset of the next 
			chained record */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the next record offset field
of the record. */
UNIV_INLINE
void
rec_set_next_offs(
/*==============*/
	rec_t*	rec,	/* in: physical record */
	ulint	next);	/* in: offset of the next record */
/**********************************************************
The following function is used to get the number of fields
in the record. */
UNIV_INLINE
ulint
rec_get_n_fields(
/*=============*/
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
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the number of owned
records. */
UNIV_INLINE
void
rec_set_n_owned(
/*============*/
	rec_t*	rec,		/* in: physical record */
	ulint	n_owned);	/* in: the number of owned */
/**********************************************************
The following function is used to retrieve the info bits of
a record. */
UNIV_INLINE
ulint
rec_get_info_bits(
/*==============*/
			/* out: info bits */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the info bits of a record. */
UNIV_INLINE
void
rec_set_info_bits(
/*==============*/
	rec_t*	rec,	/* in: physical record */
	ulint	bits);	/* in: info bits */
/**********************************************************
Gets the value of the deleted falg in info bits. */
UNIV_INLINE
ibool
rec_info_bits_get_deleted_flag(
/*===========================*/
				/* out: TRUE if deleted flag set */
	ulint	info_bits);	/* in: info bits from a record */
/**********************************************************
The following function tells if record is delete marked. */
UNIV_INLINE
ibool
rec_get_deleted_flag(
/*=================*/
			/* out: TRUE if delete marked */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the deleted bit. */
UNIV_INLINE
void
rec_set_deleted_flag(
/*=================*/
	rec_t*	rec,	/* in: physical record */
	ibool	flag);	/* in: TRUE if delete marked */
/**********************************************************
The following function is used to get the order number
of the record in the heap of the index page. */
UNIV_INLINE
ulint
rec_get_heap_no(
/*=============*/
			/* out: heap order number */
	rec_t*	rec);	/* in: physical record */
/**********************************************************
The following function is used to set the heap number
field in the record. */
UNIV_INLINE
void
rec_set_heap_no(
/*=============*/
	rec_t*	rec,	/* in: physical record */
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
/****************************************************************
The following function is used to get a pointer to the nth
data field in the record. */

byte*
rec_get_nth_field(
/*==============*/
 			/* out: pointer to the field, NULL if SQL null */
 	rec_t*	rec, 	/* in: record */
 	ulint	n,	/* in: index of the field */
	ulint*	len);	/* out: length of the field; UNIV_SQL_NULL 
			if SQL null */
/****************************************************************
Gets the physical size of a field. Also an SQL null may have a field of
size > 0, if the data type is of a fixed size. */
UNIV_INLINE
ulint
rec_get_nth_field_size(
/*===================*/
			/* out: field size in bytes */
 	rec_t*	rec, 	/* in: record */
 	ulint	n);	/* in: index of the field */
/****************************************************************
The following function is used to get a copy of the nth
data field in the record to a buffer. */
UNIV_INLINE
void
rec_copy_nth_field(
/*===============*/
 	void*	buf,	/* in: pointer to the buffer */
 	rec_t*	rec, 	/* in: record */
 	ulint	n,	/* in: index of the field */
	ulint*	len);	/* out: length of the field; UNIV_SQL_NULL if SQL 
			null */
/*************************************************************** 
This is used to modify the value of an already existing field in 
a physical record. The previous value must have exactly the same 
size as the new value. If len is UNIV_SQL_NULL then the field is 
treated as SQL null. */
UNIV_INLINE
void
rec_set_nth_field(
/*==============*/
	rec_t*	rec, 	/* in: record */
	ulint	n,	/* in: index of the field */
	void*	data,	/* in: pointer to the data if not SQL null */
	ulint	len);	/* in: length of the data or UNIV_SQL_NULL. 
			If not SQL null, must have the same length as the
			previous value. If SQL null, previous value must be
			SQL null. */
/************************************************************** 
The following function returns the data size of a physical
record, that is the sum of field lengths. SQL null fields
are counted as length 0 fields. The value returned by the function
is the distance from record origin to record end in bytes. */
UNIV_INLINE
ulint
rec_get_data_size(
/*==============*/
			/* out: size */
	rec_t*	rec);	/* in: physical record */
/************************************************************** 
Returns the total size of record minus data size of record.
The value returned by the function is the distance from record 
start to record origin in bytes. */
UNIV_INLINE
ulint
rec_get_extra_size(
/*===============*/
			/* out: size */
	rec_t*	rec);	/* in: physical record */
/************************************************************** 
Returns the total size of a physical record.  */
UNIV_INLINE
ulint
rec_get_size(
/*=========*/
			/* out: size */
	rec_t*	rec);	/* in: physical record */
/**************************************************************
Returns a pointer to the start of the record. */
UNIV_INLINE
byte*
rec_get_start(
/*==========*/
			/* out: pointer to start */
	rec_t*	rec);	/* in: pointer to record */
/**************************************************************
Returns a pointer to the end of the record. */
UNIV_INLINE
byte*
rec_get_end(
/*========*/
			/* out: pointer to end */
	rec_t*	rec);	/* in: pointer to record */
/*******************************************************************
Copies a physical record to a buffer. */
UNIV_INLINE
rec_t*
rec_copy(
/*=====*/
			/* out: pointer to the origin of the copied record */
	void*	buf,	/* in: buffer */
	rec_t*	rec);	/* in: physical record */
/******************************************************************
Copies the first n fields of a physical record to a new physical record in
a buffer. */

rec_t*
rec_copy_prefix_to_buf(
/*===================*/
				/* out, own: copied record */
	rec_t*	rec,		/* in: physical record */
	ulint	n_fields,	/* in: number of fields to copy */
	byte**	buf,		/* in/out: memory buffer for the copied prefix,
				or NULL */
	ulint*	buf_size);	/* in/out: buffer size */
/****************************************************************
Folds a prefix of a physical record to a ulint. */
UNIV_INLINE
ulint
rec_fold(
/*=====*/
				/* out: the folded value */
	rec_t*	rec,		/* in: the physical record */
	ulint	n_fields,	/* in: number of complete fields to fold */
	ulint	n_bytes,	/* in: number of bytes to fold in an
				incomplete last field */
	dulint	tree_id);	/* in: index tree id */
/*************************************************************
Builds a physical record out of a data tuple and stores it beginning from
address destination. */
UNIV_INLINE
rec_t* 	
rec_convert_dtuple_to_rec(
/*======================*/			
				/* out: pointer to the origin of physical
				record */
	byte*	destination,	/* in: start address of the physical record */
	dtuple_t* dtuple);	/* in: data tuple */
/*************************************************************
Builds a physical record out of a data tuple and stores it beginning from
address destination. */

rec_t* 	
rec_convert_dtuple_to_rec_low(
/*==========================*/			
				/* out: pointer to the origin of physical
				record */
	byte*	destination,	/* in: start address of the physical record */
	dtuple_t* dtuple,	/* in: data tuple */
	ulint	data_size);	/* in: data size of dtuple */
/**************************************************************
Returns the extra size of a physical record if we know its
data size and number of fields. */
UNIV_INLINE
ulint
rec_get_converted_extra_size(
/*=========================*/
				/* out: extra size */
	ulint	data_size,	/* in: data size */
	ulint	n_fields);	/* in: number of fields */
/**************************************************************
The following function returns the size of a data tuple when converted to
a physical record. */
UNIV_INLINE
ulint
rec_get_converted_size(
/*===================*/
				/* out: size */
	dtuple_t*	dtuple);/* in: data tuple */
/******************************************************************
Copies the first n fields of a physical record to a data tuple.
The fields are copied to the memory heap. */

void
rec_copy_prefix_to_dtuple(
/*======================*/
	dtuple_t*	tuple,		/* in: data tuple */
	rec_t*		rec,		/* in: physical record */
	ulint		n_fields,	/* in: number of fields to copy */
	mem_heap_t*	heap);		/* in: memory heap */
/*******************************************************************
Validates the consistency of a physical record. */

ibool
rec_validate(
/*=========*/
			/* out: TRUE if ok */
	rec_t*	rec);	/* in: physical record */
/*******************************************************************
Prints a physical record. */

void
rec_print(
/*======*/
	rec_t*	rec);	/* in: physical record */
/*******************************************************************
Prints a physical record to a buffer. */

ulint
rec_sprintf(
/*========*/
			/* out: printed length in bytes */
	char*	buf,	/* in: buffer to print to */
	ulint	buf_len,/* in: buffer length */
	rec_t*	rec);	/* in: physical record */

#define REC_INFO_BITS		6	/* This is single byte bit-field */

#ifndef UNIV_NONINL
#include "rem0rec.ic"
#endif

#endif
