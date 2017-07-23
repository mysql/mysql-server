/*****************************************************************************

Copyright (c) 1994, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

/********************************************************************//**
@file include/rem0rec.h
Record manager

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#ifndef rem0rec_h
#define rem0rec_h

#include "univ.i"
#include "data0data.h"
#include "rem0types.h"
#include "mtr0types.h"
#include "page0types.h"
#include "trx0types.h"
#include <ostream>
#include <sstream>

/* Info bit denoting the predefined minimum record: this bit is set
if and only if the record is the first user record on a non-leaf
B-tree page that is the leftmost page on its level
(PAGE_LEVEL is nonzero and FIL_PAGE_PREV is FIL_NULL). */
#define REC_INFO_MIN_REC_FLAG	0x10UL
/* The deleted flag in info bits */
#define REC_INFO_DELETED_FLAG	0x20UL	/* when bit is set to 1, it means the
					record has been delete marked */

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

/* The following four constants are needed in page0zip.cc in order to
efficiently compress and decompress pages. */

/* The offset of heap_no in a compact record */
#define REC_NEW_HEAP_NO		4
/* The shift of heap_no in a compact record.
The status is stored in the low-order bits. */
#define	REC_HEAP_NO_SHIFT	3

/* Length of a B-tree node pointer, in bytes */
#define REC_NODE_PTR_SIZE	4

/** SQL null flag in a 1-byte offset of ROW_FORMAT=REDUNDANT records */
#define REC_1BYTE_SQL_NULL_MASK	0x80UL
/** SQL null flag in a 2-byte offset of ROW_FORMAT=REDUNDANT records */
#define REC_2BYTE_SQL_NULL_MASK	0x8000UL

/** In a 2-byte offset of ROW_FORMAT=REDUNDANT records, the second most
significant bit denotes that the tail of a field is stored off-page. */
#define REC_2BYTE_EXTERN_MASK	0x4000UL

#ifdef UNIV_DEBUG
/* Length of the rec_get_offsets() header */
# define REC_OFFS_HEADER_SIZE	4
#else /* UNIV_DEBUG */
/* Length of the rec_get_offsets() header */
# define REC_OFFS_HEADER_SIZE	2
#endif /* UNIV_DEBUG */

/* Number of elements that should be initially allocated for the
offsets[] array, first passed to rec_get_offsets() */
#define REC_OFFS_NORMAL_SIZE	100
#define REC_OFFS_SMALL_SIZE	10

/******************************************************//**
The following function is used to get the pointer of the next chained record
on the same page.
@return pointer to the next chained record, or NULL if none */
UNIV_INLINE
const rec_t*
rec_get_next_ptr_const(
/*===================*/
	const rec_t*	rec,	/*!< in: physical record */
	ulint		comp)	/*!< in: nonzero=compact page format */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to get the pointer of the next chained record
on the same page.
@return pointer to the next chained record, or NULL if none */
UNIV_INLINE
rec_t*
rec_get_next_ptr(
/*=============*/
	rec_t*	rec,	/*!< in: physical record */
	ulint	comp)	/*!< in: nonzero=compact page format */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to get the offset of the
next chained record on the same page.
@return the page offset of the next chained record, or 0 if none */
UNIV_INLINE
ulint
rec_get_next_offs(
/*==============*/
	const rec_t*	rec,	/*!< in: physical record */
	ulint		comp)	/*!< in: nonzero=compact page format */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the next record offset field
of an old-style record. */
UNIV_INLINE
void
rec_set_next_offs_old(
/*==================*/
	rec_t*	rec,	/*!< in: old-style physical record */
	ulint	next);	/*!< in: offset of the next record */
/******************************************************//**
The following function is used to set the next record offset field
of a new-style record. */
UNIV_INLINE
void
rec_set_next_offs_new(
/*==================*/
	rec_t*	rec,	/*!< in/out: new-style physical record */
	ulint	next);	/*!< in: offset of the next record */
/******************************************************//**
The following function is used to get the number of fields
in an old-style record.
@return number of data fields */
UNIV_INLINE
ulint
rec_get_n_fields_old(
/*=================*/
	const rec_t*	rec)	/*!< in: physical record */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to get the number of fields
in a record.
@return number of data fields */
UNIV_INLINE
ulint
rec_get_n_fields(
/*=============*/
	const rec_t*		rec,	/*!< in: physical record */
	const dict_index_t*	index)	/*!< in: record descriptor */
	MY_ATTRIBUTE((warn_unused_result));

/** Confirms the n_fields of the entry is sane with comparing the other
record in the same page specified
@param[in]	index	index
@param[in]	rec	record of the same page
@param[in]	entry	index entry
@return	true if n_fields is sane */
UNIV_INLINE
bool
rec_n_fields_is_sane(
	dict_index_t*	index,
	const rec_t*	rec,
	const dtuple_t*	entry)
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
The following function is used to get the number of records owned by the
previous directory record.
@return number of owned records */
UNIV_INLINE
ulint
rec_get_n_owned_old(
/*================*/
	const rec_t*	rec)	/*!< in: old-style physical record */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the number of owned records. */
UNIV_INLINE
void
rec_set_n_owned_old(
/*================*/
	rec_t*	rec,		/*!< in: old-style physical record */
	ulint	n_owned);	/*!< in: the number of owned */

/******************************************************//**
The following function is used to get the number of records owned by the
previous directory record.
@return number of owned records */
UNIV_INLINE
ulint
rec_get_n_owned_new(
/*================*/
	const rec_t*	rec)	/*!< in: new-style physical record */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the number of owned records. */
UNIV_INLINE
void
rec_set_n_owned_new(
/*================*/
	rec_t*		rec,	/*!< in/out: new-style physical record */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	ulint		n_owned);/*!< in: the number of owned */

/******************************************************//**
The following function is used to retrieve the info bits of
a record.
@return info bits */
UNIV_INLINE
ulint
rec_get_info_bits(
/*==============*/
	const rec_t*	rec,	/*!< in: physical record */
	ulint		comp)	/*!< in: nonzero=compact page format */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the info bits of a record. */
UNIV_INLINE
void
rec_set_info_bits_old(
/*==================*/
	rec_t*	rec,	/*!< in: old-style physical record */
	ulint	bits);	/*!< in: info bits */

/******************************************************//**
The following function is used to set the info bits of a record. */
UNIV_INLINE
void
rec_set_info_bits_new(
/*==================*/
	rec_t*	rec,	/*!< in/out: new-style physical record */
	ulint	bits);	/*!< in: info bits */

/******************************************************//**
The following function retrieves the status bits of a new-style record.
@return status bits */
UNIV_INLINE
ulint
rec_get_status(
/*===========*/
	const rec_t*	rec)	/*!< in: physical record */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
The following function is used to set the status bits of a new-style record. */
UNIV_INLINE
void
rec_set_status(
/*===========*/
	rec_t*	rec,	/*!< in/out: physical record */
	ulint	bits);	/*!< in: info bits */

/******************************************************//**
The following function is used to retrieve the info and status
bits of a record.  (Only compact records have status bits.)
@return info bits */
UNIV_INLINE
ulint
rec_get_info_and_status_bits(
/*=========================*/
	const rec_t*	rec,	/*!< in: physical record */
	ulint		comp)	/*!< in: nonzero=compact page format */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the info and status
bits of a record.  (Only compact records have status bits.) */
UNIV_INLINE
void
rec_set_info_and_status_bits(
/*=========================*/
	rec_t*	rec,	/*!< in/out: compact physical record */
	ulint	bits);	/*!< in: info bits */

/******************************************************//**
The following function tells if record is delete marked.
@return nonzero if delete marked */
UNIV_INLINE
ulint
rec_get_deleted_flag(
/*=================*/
	const rec_t*	rec,	/*!< in: physical record */
	ulint		comp)	/*!< in: nonzero=compact page format */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the deleted bit. */
UNIV_INLINE
void
rec_set_deleted_flag_old(
/*=====================*/
	rec_t*	rec,	/*!< in: old-style physical record */
	ulint	flag);	/*!< in: nonzero if delete marked */

/******************************************************//**
The following function is used to set the deleted bit. */
UNIV_INLINE
void
rec_set_deleted_flag_new(
/*=====================*/
	rec_t*		rec,	/*!< in/out: new-style physical record */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	ulint		flag);	/*!< in: nonzero if delete marked */

/******************************************************//**
The following function tells if a new-style record is a node pointer.
@return TRUE if node pointer */
UNIV_INLINE
ibool
rec_get_node_ptr_flag(
/*==================*/
	const rec_t*	rec)	/*!< in: physical record */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to get the order number
of an old-style record in the heap of the index page.
@return heap order number */
UNIV_INLINE
ulint
rec_get_heap_no_old(
/*================*/
	const rec_t*	rec)	/*!< in: physical record */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the heap number
field in an old-style record. */
UNIV_INLINE
void
rec_set_heap_no_old(
/*================*/
	rec_t*	rec,	/*!< in: physical record */
	ulint	heap_no);/*!< in: the heap number */

/******************************************************//**
The following function is used to get the order number
of a new-style record in the heap of the index page.
@return heap order number */
UNIV_INLINE
ulint
rec_get_heap_no_new(
/*================*/
	const rec_t*	rec)	/*!< in: physical record */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
The following function is used to set the heap number
field in a new-style record. */
UNIV_INLINE
void
rec_set_heap_no_new(
/*================*/
	rec_t*	rec,	/*!< in/out: physical record */
	ulint	heap_no);/*!< in: the heap number */

/******************************************************//**
The following function is used to test whether the data offsets
in the record are stored in one-byte or two-byte format.
@return TRUE if 1-byte form */
UNIV_INLINE
ibool
rec_get_1byte_offs_flag(
/*====================*/
	const rec_t*	rec)	/*!< in: physical record */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
The following function is used to set the 1-byte offsets flag. */
UNIV_INLINE
void
rec_set_1byte_offs_flag(
/*====================*/
	rec_t*	rec,	/*!< in: physical record */
	ibool	flag);	/*!< in: TRUE if 1byte form */

/******************************************************//**
Returns the offset of nth field end if the record is stored in the 1-byte
offsets form. If the field is SQL null, the flag is ORed in the returned
value.
@return offset of the start of the field, SQL null flag ORed */
UNIV_INLINE
ulint
rec_1_get_field_end_info(
/*=====================*/
	const rec_t*	rec,	/*!< in: record */
	ulint		n)	/*!< in: field index */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
Returns the offset of nth field end if the record is stored in the 2-byte
offsets form. If the field is SQL null, the flag is ORed in the returned
value.
@return offset of the start of the field, SQL null flag and extern
storage flag ORed */
UNIV_INLINE
ulint
rec_2_get_field_end_info(
/*=====================*/
	const rec_t*	rec,	/*!< in: record */
	ulint		n)	/*!< in: field index */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
Returns nonzero if the field is stored off-page.
@retval 0 if the field is stored in-page
@retval REC_2BYTE_EXTERN_MASK if the field is stored externally */
UNIV_INLINE
ulint
rec_2_is_field_extern(
/*==================*/
	const rec_t*	rec,	/*!< in: record */
	ulint		n)	/*!< in: field index */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
Determine how many of the first n columns in a compact
physical record are stored externally.
@return number of externally stored columns */
ulint
rec_get_n_extern_new(
/*=================*/
	const rec_t*		rec,	/*!< in: compact physical record */
	const dict_index_t*	index,	/*!< in: record descriptor */
	ulint			n)	/*!< in: number of columns to scan */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
The following function determines the offsets to each field
in the record.	It can reuse a previously allocated array.
@return the new offsets */
ulint*
rec_get_offsets_func(
/*=================*/
	const rec_t*		rec,	/*!< in: physical record */
	const dict_index_t*	index,	/*!< in: record descriptor */
	ulint*			offsets,/*!< in/out: array consisting of
					offsets[0] allocated elements,
					or an array from rec_get_offsets(),
					or NULL */
	ulint			n_fields,/*!< in: maximum number of
					initialized fields
					 (ULINT_UNDEFINED if all fields) */
#ifdef UNIV_DEBUG
	const char*		file,	/*!< in: file name where called */
	ulint			line,	/*!< in: line number where called */
#endif /* UNIV_DEBUG */
	mem_heap_t**		heap)	/*!< in/out: memory heap */
	MY_ATTRIBUTE((warn_unused_result));

#ifdef UNIV_DEBUG
# define rec_get_offsets(rec,index,offsets,n,heap)			\
	rec_get_offsets_func(rec,index,offsets,n,__FILE__,__LINE__,heap)
#else /* UNIV_DEBUG */
# define rec_get_offsets(rec, index, offsets, n, heap)	\
	rec_get_offsets_func(rec, index, offsets, n, heap)
#endif /* UNIV_DEBUG */

/******************************************************//**
The following function determines the offsets to each field
in the record.  It can reuse a previously allocated array. */
void
rec_get_offsets_reverse(
/*====================*/
	const byte*		extra,	/*!< in: the extra bytes of a
					compact record in reverse order,
					excluding the fixed-size
					REC_N_NEW_EXTRA_BYTES */
	const dict_index_t*	index,	/*!< in: record descriptor */
	ulint			node_ptr,/*!< in: nonzero=node pointer,
					0=leaf node */
	ulint*			offsets);/*!< in/out: array consisting of
					offsets[0] allocated elements */
#ifdef UNIV_DEBUG
/************************************************************//**
Validates offsets returned by rec_get_offsets().
@return TRUE if valid */
UNIV_INLINE
ibool
rec_offs_validate(
/*==============*/
	const rec_t*		rec,	/*!< in: record or NULL */
	const dict_index_t*	index,	/*!< in: record descriptor or NULL */
	const ulint*		offsets)/*!< in: array returned by
					rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/************************************************************//**
Updates debug data in offsets, in order to avoid bogus
rec_offs_validate() failures. */
UNIV_INLINE
void
rec_offs_make_valid(
/*================*/
	const rec_t*		rec,	/*!< in: record */
	const dict_index_t*	index,	/*!< in: record descriptor */
	ulint*			offsets);/*!< in: array returned by
					rec_get_offsets() */
#else
# define rec_offs_make_valid(rec, index, offsets) ((void) 0)
#endif /* UNIV_DEBUG */

/************************************************************//**
The following function is used to get the offset to the nth
data field in an old-style record.
@return offset to the field */
ulint
rec_get_nth_field_offs_old(
/*=======================*/
	const rec_t*	rec,	/*!< in: record */
	ulint		n,	/*!< in: index of the field */
	ulint*		len);	/*!< out: length of the field; UNIV_SQL_NULL
				if SQL null */
#define rec_get_nth_field_old(rec, n, len) \
((rec) + rec_get_nth_field_offs_old(rec, n, len))
/************************************************************//**
Gets the physical size of an old-style field.
Also an SQL null may have a field of size > 0,
if the data type is of a fixed size.
@return field size in bytes */
UNIV_INLINE
ulint
rec_get_nth_field_size(
/*===================*/
	const rec_t*	rec,	/*!< in: record */
	ulint		n)	/*!< in: index of the field */
	MY_ATTRIBUTE((warn_unused_result));
/************************************************************//**
The following function is used to get an offset to the nth
data field in a record.
@return offset from the origin of rec */
UNIV_INLINE
ulint
rec_get_nth_field_offs(
/*===================*/
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		n,	/*!< in: index of the field */
	ulint*		len);	/*!< out: length of the field; UNIV_SQL_NULL
				if SQL null */
#define rec_get_nth_field(rec, offsets, n, len) \
((rec) + rec_get_nth_field_offs(offsets, n, len))
/******************************************************//**
Determine if the offsets are for a record in the new
compact format.
@return nonzero if compact format */
UNIV_INLINE
ulint
rec_offs_comp(
/*==========*/
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
Determine if the offsets are for a record containing
externally stored columns.
@return nonzero if externally stored */
UNIV_INLINE
ulint
rec_offs_any_extern(
/*================*/
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
Determine if the offsets are for a record containing null BLOB pointers.
@return first field containing a null BLOB pointer, or NULL if none found */
UNIV_INLINE
const byte*
rec_offs_any_null_extern(
/*=====================*/
	const rec_t*	rec,		/*!< in: record */
	const ulint*	offsets)	/*!< in: rec_get_offsets(rec) */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
Returns nonzero if the extern bit is set in nth field of rec.
@return nonzero if externally stored */
UNIV_INLINE
ulint
rec_offs_nth_extern(
/*================*/
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		n)	/*!< in: nth field */
	MY_ATTRIBUTE((warn_unused_result));

/** Mark the nth field as externally stored.
@param[in]	offsets		array returned by rec_get_offsets()
@param[in]	n		nth field */
void
rec_offs_make_nth_extern(
        ulint*		offsets,
        const ulint     n);
/******************************************************//**
Returns nonzero if the SQL NULL bit is set in nth field of rec.
@return nonzero if SQL NULL */
UNIV_INLINE
ulint
rec_offs_nth_sql_null(
/*==================*/
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		n)	/*!< in: nth field */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************//**
Gets the physical size of a field.
@return length of field */
UNIV_INLINE
ulint
rec_offs_nth_size(
/*==============*/
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		n)	/*!< in: nth field */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
Returns the number of extern bits set in a record.
@return number of externally stored fields */
UNIV_INLINE
ulint
rec_offs_n_extern(
/*==============*/
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/***********************************************************//**
This is used to modify the value of an already existing field in a record.
The previous value must have exactly the same size as the new value. If len
is UNIV_SQL_NULL then the field is treated as an SQL null.
For records in ROW_FORMAT=COMPACT (new-style records), len must not be
UNIV_SQL_NULL unless the field already is SQL null. */
UNIV_INLINE
void
rec_set_nth_field(
/*==============*/
	rec_t*		rec,	/*!< in: record */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		n,	/*!< in: index number of the field */
	const void*	data,	/*!< in: pointer to the data if not SQL null */
	ulint		len);	/*!< in: length of the data or UNIV_SQL_NULL.
				If not SQL null, must have the same
				length as the previous value.
				If SQL null, previous value must be
				SQL null. */

/**********************************************************//**
The following function returns the data size of an old-style physical
record, that is the sum of field lengths. SQL null fields
are counted as length 0 fields. The value returned by the function
is the distance from record origin to record end in bytes.
@return size */
UNIV_INLINE
ulint
rec_get_data_size_old(
/*==================*/
	const rec_t*	rec)	/*!< in: physical record */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
The following function returns the number of allocated elements
for an array of offsets.
@return number of elements */
UNIV_INLINE
ulint
rec_offs_get_n_alloc(
/*=================*/
	const ulint*	offsets)/*!< in: array for rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
The following function sets the number of allocated elements
for an array of offsets. */
UNIV_INLINE
void
rec_offs_set_n_alloc(
/*=================*/
	ulint*	offsets,	/*!< out: array for rec_get_offsets(),
				must be allocated */
	ulint	n_alloc);	/*!< in: number of elements */
#define rec_offs_init(offsets) \
	rec_offs_set_n_alloc(offsets, (sizeof offsets) / sizeof *offsets)
/**********************************************************//**
The following function returns the number of fields in a record.
@return number of fields */
UNIV_INLINE
ulint
rec_offs_n_fields(
/*==============*/
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
The following function returns the data size of a physical
record, that is the sum of field lengths. SQL null fields
are counted as length 0 fields. The value returned by the function
is the distance from record origin to record end in bytes.
@return size */
UNIV_INLINE
ulint
rec_offs_data_size(
/*===============*/
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Returns the total size of record minus data size of record.
The value returned by the function is the distance from record
start to record origin in bytes.
@return size */
UNIV_INLINE
ulint
rec_offs_extra_size(
/*================*/
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Returns the total size of a physical record.
@return size */
UNIV_INLINE
ulint
rec_offs_size(
/*==========*/
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
/**********************************************************//**
Returns a pointer to the start of the record.
@return pointer to start */
UNIV_INLINE
byte*
rec_get_start(
/*==========*/
	const rec_t*	rec,	/*!< in: pointer to record */
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Returns a pointer to the end of the record.
@return pointer to end */
UNIV_INLINE
byte*
rec_get_end(
/*========*/
	const rec_t*	rec,	/*!< in: pointer to record */
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	MY_ATTRIBUTE((warn_unused_result));
#else /* UNIV_DEBUG */
# define rec_get_start(rec, offsets) ((rec) - rec_offs_extra_size(offsets))
# define rec_get_end(rec, offsets) ((rec) + rec_offs_data_size(offsets))
#endif /* UNIV_DEBUG */

/** Copy a physical record to a buffer.
@param[in]	buf	buffer
@param[in]	rec	physical record
@param[in]	offsets	array returned by rec_get_offsets()
@return pointer to the origin of the copy */
UNIV_INLINE
rec_t*
rec_copy(
	void*		buf,
	const rec_t*	rec,
	const ulint*	offsets);

#ifndef UNIV_HOTBACKUP
/**********************************************************//**
Determines the size of a data tuple prefix in a temporary file.
@return total size */
ulint
rec_get_converted_size_temp(
/*========================*/
	const dict_index_t*	index,	/*!< in: record descriptor */
	const dfield_t*		fields,	/*!< in: array of data fields */
	ulint			n_fields,/*!< in: number of data fields */
	const dtuple_t*		v_entry,/*!< in: dtuple contains virtual column
					data */
	ulint*			extra)	/*!< out: extra size */
	MY_ATTRIBUTE((warn_unused_result));

/******************************************************//**
Determine the offset to each field in temporary file.
@see rec_convert_dtuple_to_temp() */
void
rec_init_offsets_temp(
/*==================*/
	const rec_t*		rec,	/*!< in: temporary file record */
	const dict_index_t*	index,	/*!< in: record descriptor */
	ulint*			offsets);/*!< in/out: array of offsets;
					in: n=rec_offs_n_fields(offsets) */

/*********************************************************//**
Builds a temporary file record out of a data tuple.
@see rec_init_offsets_temp() */
void
rec_convert_dtuple_to_temp(
/*=======================*/
	rec_t*			rec,		/*!< out: record */
	const dict_index_t*	index,		/*!< in: record descriptor */
	const dfield_t*		fields,		/*!< in: array of data fields */
	ulint			n_fields,	/*!< in: number of fields */
	const dtuple_t*		v_entry);	/*!< in: dtuple contains
						virtual column data */


/**************************************************************//**
Copies the first n fields of a physical record to a new physical record in
a buffer.
@return own: copied record */
rec_t*
rec_copy_prefix_to_buf(
/*===================*/
	const rec_t*		rec,		/*!< in: physical record */
	const dict_index_t*	index,		/*!< in: record descriptor */
	ulint			n_fields,	/*!< in: number of fields
						to copy */
	byte**			buf,		/*!< in/out: memory buffer
						for the copied prefix,
						or NULL */
	ulint*			buf_size);	/*!< in/out: buffer size */
/** Fold a prefix of a physical record.
@param[in]	rec		index record
@param[in]	offsets		return value of rec_get_offsets()
@param[in]	n_fields	number of complete fields to fold
@param[in]	n_bytes		number of bytes to fold in the last field
@param[in]	index_id	index tree ID
@return the folded value */
UNIV_INLINE
ulint
rec_fold(
	const rec_t*	rec,
	const ulint*	offsets,
	ulint		n_fields,
	ulint		n_bytes,
	index_id_t	tree_id)
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
/*********************************************************//**
Builds a physical record out of a data tuple and
stores it into the given buffer.
@return pointer to the origin of physical record */
rec_t*
rec_convert_dtuple_to_rec(
/*======================*/
	byte*			buf,	/*!< in: start address of the
					physical record */
	const dict_index_t*	index,	/*!< in: record descriptor */
	const dtuple_t*		dtuple,	/*!< in: data tuple */
	ulint			n_ext)	/*!< in: number of
					externally stored columns */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Returns the extra size of an old-style physical record if we know its
data size and number of fields.
@return extra size */
UNIV_INLINE
ulint
rec_get_converted_extra_size(
/*=========================*/
	ulint	data_size,	/*!< in: data size */
	ulint	n_fields,	/*!< in: number of fields */
	ulint	n_ext)		/*!< in: number of externally stored columns */
	MY_ATTRIBUTE((const));
/**********************************************************//**
Determines the size of a data tuple prefix in ROW_FORMAT=COMPACT.
@return total size */
ulint
rec_get_converted_size_comp_prefix(
/*===============================*/
	const dict_index_t*	index,	/*!< in: record descriptor */
	const dfield_t*		fields,	/*!< in: array of data fields */
	ulint			n_fields,/*!< in: number of data fields */
	ulint*			extra)	/*!< out: extra size */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Determines the size of a data tuple in ROW_FORMAT=COMPACT.
@return total size */
ulint
rec_get_converted_size_comp(
/*========================*/
	const dict_index_t*	index,	/*!< in: record descriptor;
					dict_table_is_comp() is
					assumed to hold, even if
					it does not */
	ulint			status,	/*!< in: status bits of the record */
	const dfield_t*		fields,	/*!< in: array of data fields */
	ulint			n_fields,/*!< in: number of data fields */
	ulint*			extra);	/*!< out: extra size */
/**********************************************************//**
The following function returns the size of a data tuple when converted to
a physical record.
@return size */
UNIV_INLINE
ulint
rec_get_converted_size(
/*===================*/
	dict_index_t*	index,	/*!< in: record descriptor */
	const dtuple_t*	dtuple,	/*!< in: data tuple */
	ulint		n_ext)	/*!< in: number of externally stored columns */
	MY_ATTRIBUTE((warn_unused_result));
#ifndef UNIV_HOTBACKUP
/**************************************************************//**
Copies the first n fields of a physical record to a data tuple.
The fields are copied to the memory heap. */
void
rec_copy_prefix_to_dtuple(
/*======================*/
	dtuple_t*		tuple,		/*!< out: data tuple */
	const rec_t*		rec,		/*!< in: physical record */
	const dict_index_t*	index,		/*!< in: record descriptor */
	ulint			n_fields,	/*!< in: number of fields
						to copy */
	mem_heap_t*		heap);		/*!< in: memory heap */
#endif /* !UNIV_HOTBACKUP */
/***************************************************************//**
Validates the consistency of a physical record.
@return TRUE if ok */
ibool
rec_validate(
/*=========*/
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/***************************************************************//**
Prints an old-style physical record. */
void
rec_print_old(
/*==========*/
	FILE*		file,	/*!< in: file where to print */
	const rec_t*	rec);	/*!< in: physical record */
#ifndef UNIV_HOTBACKUP
/***************************************************************//**
Prints a physical record in ROW_FORMAT=COMPACT.  Ignores the
record header. */
void
rec_print_comp(
/*===========*/
	FILE*		file,	/*!< in: file where to print */
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/***************************************************************//**
Prints a spatial index record. */
void
rec_print_mbr_rec(
/*==========*/
	FILE*		file,	/*!< in: file where to print */
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/***************************************************************//**
Prints a physical record. */
void
rec_print_new(
/*==========*/
	FILE*		file,	/*!< in: file where to print */
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/***************************************************************//**
Prints a physical record. */
void
rec_print(
/*======*/
	FILE*			file,	/*!< in: file where to print */
	const rec_t*		rec,	/*!< in: physical record */
	const dict_index_t*	index);	/*!< in: record descriptor */

/** Pretty-print a record.
@param[in,out]	o	output stream
@param[in]	rec	physical record
@param[in]	info	rec_get_info_bits(rec)
@param[in]	offsets	rec_get_offsets(rec) */
void
rec_print(
	std::ostream&	o,
	const rec_t*	rec,
	ulint		info,
	const ulint*	offsets);

/** Wrapper for pretty-printing a record */
struct rec_index_print
{
	/** Constructor */
	rec_index_print(const rec_t* rec, const dict_index_t* index) :
		m_rec(rec), m_index(index)
	{}

	/** Record */
	const rec_t*		m_rec;
	/** Index */
	const dict_index_t*	m_index;
};

/** Display a record.
@param[in,out]	o	output stream
@param[in]	r	record to display
@return	the output stream */
std::ostream&
operator<<(std::ostream& o, const rec_index_print& r);

/** Wrapper for pretty-printing a record */
struct rec_offsets_print
{
	/** Constructor */
	rec_offsets_print(const rec_t* rec, const ulint* offsets) :
		m_rec(rec), m_offsets(offsets)
	{}

	/** Record */
	const rec_t*		m_rec;
	/** Offsets to each field */
	const ulint*		m_offsets;
};

/** Display a record.
@param[in,out]	o	output stream
@param[in]	r	record to display
@return	the output stream */
std::ostream&
operator<<(std::ostream& o, const rec_offsets_print& r);

# ifndef DBUG_OFF
/** Pretty-printer of records and tuples */
class rec_printer : public std::ostringstream {
public:
	/** Construct a pretty-printed record.
	@param rec	record with header
	@param offsets	rec_get_offsets(rec, ...) */
	rec_printer(const rec_t* rec, const ulint* offsets)
		:
		std::ostringstream ()
	{
		rec_print(*this, rec,
			  rec_get_info_bits(rec, rec_offs_comp(offsets)),
			  offsets);
	}

	/** Construct a pretty-printed record.
	@param rec record, possibly lacking header
	@param info rec_get_info_bits(rec)
	@param offsets rec_get_offsets(rec, ...) */
	rec_printer(const rec_t* rec, ulint info, const ulint* offsets)
		:
		std::ostringstream ()
	{
		rec_print(*this, rec, info, offsets);
	}

	/** Construct a pretty-printed tuple.
	@param tuple	data tuple */
	rec_printer(const dtuple_t* tuple)
		:
		std::ostringstream ()
	{
		dtuple_print(*this, tuple);
	}

	/** Construct a pretty-printed tuple.
	@param field	array of data tuple fields
	@param n	number of fields */
	rec_printer(const dfield_t* field, ulint n)
		:
		std::ostringstream ()
	{
		dfield_print(*this, field, n);
	}

	/** Destructor */
	virtual ~rec_printer() {}

private:
	/** Copy constructor */
	rec_printer(const rec_printer& other);
	/** Assignment operator */
	rec_printer& operator=(const rec_printer& other);
};
# endif /* !DBUG_OFF */

# ifdef UNIV_DEBUG
/************************************************************//**
Reads the DB_TRX_ID of a clustered index record.
@return the value of DB_TRX_ID */
trx_id_t
rec_get_trx_id(
/*===========*/
	const rec_t*		rec,	/*!< in: record */
	const dict_index_t*	index)	/*!< in: clustered index */
	MY_ATTRIBUTE((warn_unused_result));
# endif /* UNIV_DEBUG */
#endif /* UNIV_HOTBACKUP */

/* Maximum lengths for the data in a physical record if the offsets
are given in one byte (resp. two byte) format. */
#define REC_1BYTE_OFFS_LIMIT	0x7FUL
#define REC_2BYTE_OFFS_LIMIT	0x7FFFUL

/* The data size of record must be smaller than this because we reserve
two upmost bits in a two byte offset for special purposes */
#define REC_MAX_DATA_SIZE	16384

#ifndef UNIV_NONINL
#include "rem0rec.ic"
#endif

#endif
