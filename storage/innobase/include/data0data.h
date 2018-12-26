/*****************************************************************************

Copyright (c) 1994, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/data0data.h
SQL data field and tuple

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#ifndef data0data_h
#define data0data_h

#include "univ.i"

#include "data0types.h"
#include "data0type.h"
#include "mem0mem.h"
#include "dict0types.h"

#include <ostream>

/** Storage for overflow data in a big record, that is, a clustered
index record which needs external storage of data fields */
struct big_rec_t;
struct upd_t;

#ifdef UNIV_DEBUG
/*********************************************************************//**
Gets pointer to the type struct of SQL data field.
@return pointer to the type struct */
UNIV_INLINE
dtype_t*
dfield_get_type(
/*============*/
	const dfield_t*	field)	/*!< in: SQL data field */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets pointer to the data in a field.
@return pointer to data */
UNIV_INLINE
void*
dfield_get_data(
/*============*/
	const dfield_t* field)	/*!< in: field */
	MY_ATTRIBUTE((warn_unused_result));
#else /* UNIV_DEBUG */
# define dfield_get_type(field) (&(field)->type)
# define dfield_get_data(field) ((field)->data)
#endif /* UNIV_DEBUG */
/*********************************************************************//**
Sets the type struct of SQL data field. */
UNIV_INLINE
void
dfield_set_type(
/*============*/
	dfield_t*	field,	/*!< in: SQL data field */
	const dtype_t*	type);	/*!< in: pointer to data type struct */

/*********************************************************************//**
Gets length of field data.
@return length of data; UNIV_SQL_NULL if SQL null data */
UNIV_INLINE
ulint
dfield_get_len(
/*===========*/
	const dfield_t* field)	/*!< in: field */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Sets length in a field. */
UNIV_INLINE
void
dfield_set_len(
/*===========*/
	dfield_t*	field,	/*!< in: field */
	ulint		len);	/*!< in: length or UNIV_SQL_NULL */
/*********************************************************************//**
Determines if a field is SQL NULL
@return nonzero if SQL null data */
UNIV_INLINE
ulint
dfield_is_null(
/*===========*/
	const dfield_t* field)	/*!< in: field */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Determines if a field is externally stored
@return nonzero if externally stored */
UNIV_INLINE
ulint
dfield_is_ext(
/*==========*/
	const dfield_t* field)	/*!< in: field */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Sets the "external storage" flag */
UNIV_INLINE
void
dfield_set_ext(
/*===========*/
	dfield_t*	field);	/*!< in/out: field */

/** Gets spatial status for "external storage"
@param[in,out]	field		field */
UNIV_INLINE
spatial_status_t
dfield_get_spatial_status(
	const dfield_t*	field);

/** Sets spatial status for "external storage"
@param[in,out]	field		field
@param[in]	spatial_status	spatial status */
UNIV_INLINE
void
dfield_set_spatial_status(
	dfield_t*		field,
	spatial_status_t	spatial_status);

/*********************************************************************//**
Sets pointer to the data and length in a field. */
UNIV_INLINE
void
dfield_set_data(
/*============*/
	dfield_t*	field,	/*!< in: field */
	const void*	data,	/*!< in: data */
	ulint		len);	/*!< in: length or UNIV_SQL_NULL */
/*********************************************************************//**
Sets pointer to the data and length in a field. */
UNIV_INLINE
void
dfield_write_mbr(
/*=============*/
	dfield_t*	field,	/*!< in: field */
	const double*	mbr);	/*!< in: data */

/*********************************************************************//**
Sets a data field to SQL NULL. */
UNIV_INLINE
void
dfield_set_null(
/*============*/
	dfield_t*	field);	/*!< in/out: field */

/**********************************************************************//**
Writes an SQL null field full of zeros. */
UNIV_INLINE
void
data_write_sql_null(
/*================*/
	byte*	data,	/*!< in: pointer to a buffer of size len */
	ulint	len);	/*!< in: SQL null size in bytes */

/*********************************************************************//**
Copies the data and len fields. */
UNIV_INLINE
void
dfield_copy_data(
/*=============*/
	dfield_t*	field1,		/*!< out: field to copy to */
	const dfield_t*	field2);	/*!< in: field to copy from */

/*********************************************************************//**
Copies a data field to another. */
UNIV_INLINE
void
dfield_copy(
/*========*/
	dfield_t*	field1,		/*!< out: field to copy to */
	const dfield_t*	field2);	/*!< in: field to copy from */
/*********************************************************************//**
Copies the data pointed to by a data field. */
UNIV_INLINE
void
dfield_dup(
/*=======*/
	dfield_t*	field,	/*!< in/out: data field */
	mem_heap_t*	heap);	/*!< in: memory heap where allocated */

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Tests if two data fields are equal.
If len==0, tests the data length and content for equality.
If len>0, tests the first len bytes of the content for equality.
@return TRUE if both fields are NULL or if they are equal */
UNIV_INLINE
ibool
dfield_datas_are_binary_equal(
/*==========================*/
	const dfield_t*	field1,	/*!< in: field */
	const dfield_t*	field2,	/*!< in: field */
	ulint		len)	/*!< in: maximum prefix to compare,
				or 0 to compare the whole field length */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Tests if dfield data length and content is equal to the given.
@return TRUE if equal */
UNIV_INLINE
ibool
dfield_data_is_binary_equal(
/*========================*/
	const dfield_t*	field,	/*!< in: field */
	ulint		len,	/*!< in: data length or UNIV_SQL_NULL */
	const byte*	data)	/*!< in: data */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Gets number of fields in a data tuple.
@return number of fields */
UNIV_INLINE
ulint
dtuple_get_n_fields(
/*================*/
	const dtuple_t*	tuple)	/*!< in: tuple */
	MY_ATTRIBUTE((warn_unused_result));

/** Gets number of virtual fields in a data tuple.
@param[in]	tuple	dtuple to check
@return number of fields */
UNIV_INLINE
ulint
dtuple_get_n_v_fields(
	const dtuple_t*	tuple);

#ifdef UNIV_DEBUG
/** Gets nth field of a tuple.
@param[in]	tuple	tuple
@param[in]	n	index of field
@return nth field */
UNIV_INLINE
dfield_t*
dtuple_get_nth_field(
	const dtuple_t*	tuple,
	ulint		n);
/** Gets nth virtual field of a tuple.
@param[in]	tuple	tuple
@oaran[in]	n	the nth field to get
@return nth field */
UNIV_INLINE
dfield_t*
dtuple_get_nth_v_field(
	const dtuple_t*	tuple,
	ulint		n);
#else /* UNIV_DEBUG */
# define dtuple_get_nth_field(tuple, n) ((tuple)->fields + (n))
# define dtuple_get_nth_v_field(tuple, n) ((tuple)->fields + (tuple)->n_fields + (n))
#endif /* UNIV_DEBUG */
/*********************************************************************//**
Gets info bits in a data tuple.
@return info bits */
UNIV_INLINE
ulint
dtuple_get_info_bits(
/*=================*/
	const dtuple_t*	tuple)	/*!< in: tuple */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Sets info bits in a data tuple. */
UNIV_INLINE
void
dtuple_set_info_bits(
/*=================*/
	dtuple_t*	tuple,		/*!< in: tuple */
	ulint		info_bits);	/*!< in: info bits */

/*********************************************************************//**
Gets number of fields used in record comparisons.
@return number of fields used in comparisons in rem0cmp.* */
UNIV_INLINE
ulint
dtuple_get_n_fields_cmp(
/*====================*/
	const dtuple_t*	tuple)	/*!< in: tuple */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets number of fields used in record comparisons. */
UNIV_INLINE
void
dtuple_set_n_fields_cmp(
/*====================*/
	dtuple_t*	tuple,		/*!< in: tuple */
	ulint		n_fields_cmp);	/*!< in: number of fields used in
					comparisons in rem0cmp.* */

/* Estimate the number of bytes that are going to be allocated when
creating a new dtuple_t object */
#define DTUPLE_EST_ALLOC(n_fields)	\
	(sizeof(dtuple_t) + (n_fields) * sizeof(dfield_t))

/** Creates a data tuple from an already allocated chunk of memory.
The size of the chunk must be at least DTUPLE_EST_ALLOC(n_fields).
The default value for number of fields used in record comparisons
for this tuple is n_fields.
@param[in,out]	buf		buffer to use
@param[in]	buf_size	buffer size
@param[in]	n_fields	number of field
@param[in]	n_v_fields	number of fields on virtual columns
@return created tuple (inside buf) */
UNIV_INLINE
dtuple_t*
dtuple_create_from_mem(
	void*	buf,
	ulint	buf_size,
	ulint	n_fields,
	ulint	n_v_fields)
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Creates a data tuple to a memory heap. The default value for number
of fields used in record comparisons for this tuple is n_fields.
@return own: created tuple */
UNIV_INLINE
dtuple_t*
dtuple_create(
/*==========*/
	mem_heap_t*	heap,	/*!< in: memory heap where the tuple
				is created, DTUPLE_EST_ALLOC(n_fields)
				bytes will be allocated from this heap */
	ulint		n_fields)/*!< in: number of fields */
	MY_ATTRIBUTE((malloc));

/** Initialize the virtual field data in a dtuple_t
@param[in,out]		vrow	dtuple contains the virtual fields */
UNIV_INLINE
void
dtuple_init_v_fld(
	const dtuple_t*	vrow);

/** Duplicate the virtual field data in a dtuple_t
@param[in,out]		vrow	dtuple contains the virtual fields
@param[in]		heap	heap memory to use */
UNIV_INLINE
void
dtuple_dup_v_fld(
	const dtuple_t*	vrow,
	mem_heap_t*	heap);

/** Creates a data tuple with possible virtual columns to a memory heap.
@param[in]	heap		memory heap where the tuple is created
@param[in]	n_fields	number of fields
@param[in]	n_v_fields	number of fields on virtual col
@return own: created tuple */
UNIV_INLINE
dtuple_t*
dtuple_create_with_vcol(
	mem_heap_t*	heap,
	ulint		n_fields,
	ulint		n_v_fields);

/*********************************************************************//**
Sets number of fields used in a tuple. Normally this is set in
dtuple_create, but if you want later to set it smaller, you can use this. */
void
dtuple_set_n_fields(
/*================*/
	dtuple_t*	tuple,		/*!< in: tuple */
	ulint		n_fields);	/*!< in: number of fields */
/** Copies a data tuple's virtaul fields to another. This is a shallow copy;
@param[in,out]	d_tuple		destination tuple
@param[in]	s_tuple		source tuple */
UNIV_INLINE
void
dtuple_copy_v_fields(
	dtuple_t*	d_tuple,
	const dtuple_t*	s_tuple);
/*********************************************************************//**
Copies a data tuple to another.  This is a shallow copy; if a deep copy
is desired, dfield_dup() will have to be invoked on each field.
@return own: copy of tuple */
UNIV_INLINE
dtuple_t*
dtuple_copy(
/*========*/
	const dtuple_t*	tuple,	/*!< in: tuple to copy from */
	mem_heap_t*	heap)	/*!< in: memory heap
				where the tuple is created */
	MY_ATTRIBUTE((malloc));
/**********************************************************//**
The following function returns the sum of data lengths of a tuple. The space
occupied by the field structs or the tuple struct is not counted.
@return sum of data lens */
UNIV_INLINE
ulint
dtuple_get_data_size(
/*=================*/
	const dtuple_t*	tuple,	/*!< in: typed data tuple */
	ulint		comp);	/*!< in: nonzero=ROW_FORMAT=COMPACT  */
/*********************************************************************//**
Computes the number of externally stored fields in a data tuple.
@return number of fields */
UNIV_INLINE
ulint
dtuple_get_n_ext(
/*=============*/
	const dtuple_t*	tuple);	/*!< in: tuple */
/** Compare two data tuples.
@param[in] tuple1 first data tuple
@param[in] tuple2 second data tuple
@return positive, 0, negative if tuple1 is greater, equal, less, than tuple2,
respectively */
int
dtuple_coll_cmp(
	const dtuple_t*	tuple1,
	const dtuple_t*	tuple2)
	MY_ATTRIBUTE((warn_unused_result));
/** Fold a prefix given as the number of fields of a tuple.
@param[in]	tuple		index record
@param[in]	n_fields	number of complete fields to fold
@param[in]	n_bytes		number of bytes to fold in the last field
@param[in]	index_id	index tree ID
@return the folded value */
UNIV_INLINE
ulint
dtuple_fold(
	const dtuple_t*	tuple,
	ulint		n_fields,
	ulint		n_bytes,
	index_id_t	tree_id)
	MY_ATTRIBUTE((warn_unused_result));
/*******************************************************************//**
Sets types of fields binary in a tuple. */
UNIV_INLINE
void
dtuple_set_types_binary(
/*====================*/
	dtuple_t*	tuple,	/*!< in: data tuple */
	ulint		n);	/*!< in: number of fields to set */

/**********************************************************************//**
Checks if a dtuple contains an SQL null value.
@return TRUE if some field is SQL null */
UNIV_INLINE
ibool
dtuple_contains_null(
/*=================*/
	const dtuple_t*	tuple)	/*!< in: dtuple */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Checks that a data field is typed. Asserts an error if not.
@return TRUE if ok */
ibool
dfield_check_typed(
/*===============*/
	const dfield_t*	field)	/*!< in: data field */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Checks that a data tuple is typed. Asserts an error if not.
@return TRUE if ok */
ibool
dtuple_check_typed(
/*===============*/
	const dtuple_t*	tuple)	/*!< in: tuple */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************//**
Checks that a data tuple is typed.
@return TRUE if ok */
ibool
dtuple_check_typed_no_assert(
/*=========================*/
	const dtuple_t*	tuple)	/*!< in: tuple */
	MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
/**********************************************************//**
Validates the consistency of a tuple which must be complete, i.e,
all fields must have been set.
@return TRUE if ok */
ibool
dtuple_validate(
/*============*/
	const dtuple_t*	tuple)	/*!< in: tuple */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */
/*************************************************************//**
Pretty prints a dfield value according to its data type. */
void
dfield_print(
/*=========*/
	const dfield_t*	dfield);	/*!< in: dfield */
/*************************************************************//**
Pretty prints a dfield value according to its data type. Also the hex string
is printed if a string contains non-printable characters. */
void
dfield_print_also_hex(
/*==================*/
	const dfield_t*	dfield);	 /*!< in: dfield */
/**********************************************************//**
The following function prints the contents of a tuple. */
void
dtuple_print(
/*=========*/
	FILE*		f,	/*!< in: output stream */
	const dtuple_t*	tuple);	/*!< in: tuple */

/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	field	array of data fields
@param[in]	n	number of data fields */
void
dfield_print(
	std::ostream&	o,
	const dfield_t*	field,
	ulint		n);
/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	tuple	data tuple */
void
dtuple_print(
	std::ostream&	o,
	const dtuple_t*	tuple);

/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	tuple	data tuple */
inline
std::ostream&
operator<<(std::ostream& o, const dtuple_t& tuple)
{
	dtuple_print(o, &tuple);
	return(o);
}

/**************************************************************//**
Moves parts of long fields in entry to the big record vector so that
the size of tuple drops below the maximum record size allowed in the
database. Moves data only from those fields which are not necessary
to determine uniquely the insertion place of the tuple in the index.
@return own: created big record vector, NULL if we are not able to
shorten the entry enough, i.e., if there are too many fixed-length or
short fields in entry or the index is clustered */
big_rec_t*
dtuple_convert_big_rec(
/*===================*/
	dict_index_t*	index,	/*!< in: index */
	upd_t*		upd,	/*!< in/out: update vector */
	dtuple_t*	entry,	/*!< in/out: index entry */
	ulint*		n_ext)	/*!< in/out: number of
				externally stored columns */
	MY_ATTRIBUTE((malloc, warn_unused_result));
/**************************************************************//**
Puts back to entry the data stored in vector. Note that to ensure the
fields in entry can accommodate the data, vector must have been created
from entry with dtuple_convert_big_rec. */
void
dtuple_convert_back_big_rec(
/*========================*/
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: entry whose data was put to vector */
	big_rec_t*	vector);	/*!< in, own: big rec vector; it is
				freed in this function */
/**************************************************************//**
Frees the memory in a big rec vector. */
UNIV_INLINE
void
dtuple_big_rec_free(
/*================*/
	big_rec_t*	vector);	/*!< in, own: big rec vector; it is
				freed in this function */

/*######################################################################*/

/** Structure for an SQL data field */
struct dfield_t{
	void*		data;	/*!< pointer to data */
	unsigned	ext:1;	/*!< TRUE=externally stored, FALSE=local */
	unsigned	spatial_status:2;
				/*!< spatial status of externally stored field
				in undo log for purge */
	unsigned	len;	/*!< data length; UNIV_SQL_NULL if SQL null */
	dtype_t		type;	/*!< type of data */

	/** Create a deep copy of this object
	@param[in]	heap	the memory heap in which the clone will be
				created.
	@return	the cloned object. */
	dfield_t* clone(mem_heap_t* heap);
};

/** Structure for an SQL data tuple of fields (logical record) */
struct dtuple_t {
	ulint		info_bits;	/*!< info bits of an index record:
					the default is 0; this field is used
					if an index record is built from
					a data tuple */
	ulint		n_fields;	/*!< number of fields in dtuple */
	ulint		n_fields_cmp;	/*!< number of fields which should
					be used in comparison services
					of rem0cmp.*; the index search
					is performed by comparing only these
					fields, others are ignored; the
					default value in dtuple creation is
					the same value as n_fields */
	dfield_t*	fields;		/*!< fields */
	ulint		n_v_fields;	/*!< number of virtual fields */
	dfield_t*	v_fields;	/*!< fields on virtual column */
	UT_LIST_NODE_T(dtuple_t) tuple_list;
					/*!< data tuples can be linked into a
					list using this field */
#ifdef UNIV_DEBUG
	ulint		magic_n;	/*!< magic number, used in
					debug assertions */
/** Value of dtuple_t::magic_n */
# define		DATA_TUPLE_MAGIC_N	65478679
#endif /* UNIV_DEBUG */
};


/** A slot for a field in a big rec vector */
struct big_rec_field_t {

	/** Constructor.
	@param[in]	field_no_	the field number
	@param[in]	len_		the data length
	@param[in]	data_		the data */
	big_rec_field_t(ulint field_no_, ulint len_, const void* data_)
		: field_no(field_no_),
		  len(len_),
		  data(data_)
	{}

	ulint		field_no;	/*!< field number in record */
	ulint		len;		/*!< stored data length, in bytes */
	const void*	data;		/*!< stored data */
};

/** Storage format for overflow data in a big record, that is, a
clustered index record which needs external storage of data fields */
struct big_rec_t {
	mem_heap_t*	heap;		/*!< memory heap from which
					allocated */
	const ulint	capacity;	/*!< fields array size */
	ulint		n_fields;	/*!< number of stored fields */
	big_rec_field_t*fields;		/*!< stored fields */

	/** Constructor.
	@param[in]	max	the capacity of the array of fields. */
	explicit big_rec_t(const ulint max)
		: heap(0),
		  capacity(max),
		  n_fields(0),
		  fields(0)
	{}

	/** Append one big_rec_field_t object to the end of array of fields */
	void append(const big_rec_field_t& field)
	{
		ut_ad(n_fields < capacity);
		fields[n_fields] = field;
		n_fields++;
	}

	/** Allocate a big_rec_t object in the given memory heap, and for
	storing n_fld number of fields.
	@param[in]	heap	memory heap in which this object is allocated
	@param[in]	n_fld	maximum number of fields that can be stored in
			this object
	@return the allocated object */
	static big_rec_t* alloc(
		mem_heap_t*	heap,
		ulint		n_fld);
};

#ifndef UNIV_NONINL
#include "data0data.ic"
#endif

#endif
