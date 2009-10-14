/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.

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

/************************************************************************
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

typedef struct big_rec_struct		big_rec_t;

#ifdef UNIV_DEBUG
/*************************************************************************
Gets pointer to the type struct of SQL data field. */
UNIV_INLINE
dtype_t*
dfield_get_type(
/*============*/
				/* out: pointer to the type struct */
	const dfield_t*	field);	/* in: SQL data field */
/*************************************************************************
Gets pointer to the data in a field. */
UNIV_INLINE
void*
dfield_get_data(
/*============*/
				/* out: pointer to data */
	const dfield_t* field);	/* in: field */
#else /* UNIV_DEBUG */
# define dfield_get_type(field) (&(field)->type)
# define dfield_get_data(field) ((field)->data)
#endif /* UNIV_DEBUG */
/*************************************************************************
Sets the type struct of SQL data field. */
UNIV_INLINE
void
dfield_set_type(
/*============*/
	dfield_t*	field,	/* in: SQL data field */
	dtype_t*	type);	/* in: pointer to data type struct */
/*************************************************************************
Gets length of field data. */
UNIV_INLINE
ulint
dfield_get_len(
/*===========*/
				/* out: length of data; UNIV_SQL_NULL if
				SQL null data */
	const dfield_t* field);	/* in: field */
/*************************************************************************
Sets length in a field. */
UNIV_INLINE
void
dfield_set_len(
/*===========*/
	dfield_t*	field,	/* in: field */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
/*************************************************************************
Determines if a field is SQL NULL */
UNIV_INLINE
ulint
dfield_is_null(
/*===========*/
				/* out: nonzero if SQL null data */
	const dfield_t* field);	/* in: field */
/*************************************************************************
Determines if a field is externally stored */
UNIV_INLINE
ulint
dfield_is_ext(
/*==========*/
				/* out: nonzero if externally stored */
	const dfield_t* field);	/* in: field */
/*************************************************************************
Sets the "external storage" flag */
UNIV_INLINE
void
dfield_set_ext(
/*===========*/
	dfield_t*	field);	/* in/out: field */
/*************************************************************************
Sets pointer to the data and length in a field. */
UNIV_INLINE
void
dfield_set_data(
/*============*/
	dfield_t*	field,	/* in: field */
	const void*	data,	/* in: data */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
/*************************************************************************
Sets a data field to SQL NULL. */
UNIV_INLINE
void
dfield_set_null(
/*============*/
	dfield_t*	field);	/* in/out: field */
/**************************************************************************
Writes an SQL null field full of zeros. */
UNIV_INLINE
void
data_write_sql_null(
/*================*/
	byte*	data,	/* in: pointer to a buffer of size len */
	ulint	len);	/* in: SQL null size in bytes */
/*************************************************************************
Copies the data and len fields. */
UNIV_INLINE
void
dfield_copy_data(
/*=============*/
	dfield_t*	field1,	/* out: field to copy to */
	const dfield_t*	field2);/* in: field to copy from */
/*************************************************************************
Copies a data field to another. */
UNIV_INLINE
void
dfield_copy(
/*========*/
	dfield_t*	field1,	/* out: field to copy to */
	const dfield_t*	field2);/* in: field to copy from */
/*************************************************************************
Copies the data pointed to by a data field. */
UNIV_INLINE
void
dfield_dup(
/*=======*/
	dfield_t*	field,	/* in/out: data field */
	mem_heap_t*	heap);	/* in: memory heap where allocated */
/*************************************************************************
Tests if data length and content is equal for two dfields. */
UNIV_INLINE
ibool
dfield_datas_are_binary_equal(
/*==========================*/
				/* out: TRUE if equal */
	const dfield_t*	field1,	/* in: field */
	const dfield_t*	field2);/* in: field */
/*************************************************************************
Tests if dfield data length and content is equal to the given. */
UNIV_INTERN
ibool
dfield_data_is_binary_equal(
/*========================*/
				/* out: TRUE if equal */
	const dfield_t*	field,	/* in: field */
	ulint		len,	/* in: data length or UNIV_SQL_NULL */
	const byte*	data);	/* in: data */
/*************************************************************************
Gets number of fields in a data tuple. */
UNIV_INLINE
ulint
dtuple_get_n_fields(
/*================*/
				/* out: number of fields */
	const dtuple_t*	tuple);	/* in: tuple */
#ifdef UNIV_DEBUG
/*************************************************************************
Gets nth field of a tuple. */
UNIV_INLINE
dfield_t*
dtuple_get_nth_field(
/*=================*/
				/* out: nth field */
	const dtuple_t*	tuple,	/* in: tuple */
	ulint		n);	/* in: index of field */
#else /* UNIV_DEBUG */
# define dtuple_get_nth_field(tuple, n) ((tuple)->fields + (n))
#endif /* UNIV_DEBUG */
/*************************************************************************
Gets info bits in a data tuple. */
UNIV_INLINE
ulint
dtuple_get_info_bits(
/*=================*/
				/* out: info bits */
	const dtuple_t*	tuple);	/* in: tuple */
/*************************************************************************
Sets info bits in a data tuple. */
UNIV_INLINE
void
dtuple_set_info_bits(
/*=================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		info_bits);	/* in: info bits */
/*************************************************************************
Gets number of fields used in record comparisons. */
UNIV_INLINE
ulint
dtuple_get_n_fields_cmp(
/*====================*/
				/* out: number of fields used in comparisons
				in rem0cmp.* */
	const dtuple_t*	tuple);	/* in: tuple */
/*************************************************************************
Gets number of fields used in record comparisons. */
UNIV_INLINE
void
dtuple_set_n_fields_cmp(
/*====================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		n_fields_cmp);	/* in: number of fields used in
					comparisons in rem0cmp.* */
/**************************************************************
Creates a data tuple to a memory heap. The default value for number
of fields used in record comparisons for this tuple is n_fields. */
UNIV_INLINE
dtuple_t*
dtuple_create(
/*==========*/
				/* out, own: created tuple */
	mem_heap_t*	heap,	/* in: memory heap where the tuple
				is created */
	ulint		n_fields); /* in: number of fields */

/**************************************************************
Wrap data fields in a tuple. The default value for number
of fields used in record comparisons for this tuple is n_fields. */
UNIV_INLINE
const dtuple_t*
dtuple_from_fields(
/*===============*/
					/* out: data tuple */
	dtuple_t*	tuple,		/* in: storage for data tuple */
	const dfield_t*	fields,		/* in: fields */
	ulint		n_fields);	/* in: number of fields */

/*************************************************************************
Sets number of fields used in a tuple. Normally this is set in
dtuple_create, but if you want later to set it smaller, you can use this. */
UNIV_INTERN
void
dtuple_set_n_fields(
/*================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		n_fields);	/* in: number of fields */
/*************************************************************************
Copies a data tuple to another.  This is a shallow copy; if a deep copy
is desired, dfield_dup() will have to be invoked on each field. */
UNIV_INLINE
dtuple_t*
dtuple_copy(
/*========*/
				/* out, own: copy of tuple */
	const dtuple_t*	tuple,	/* in: tuple to copy from */
	mem_heap_t*	heap);	/* in: memory heap
				where the tuple is created */
/**************************************************************
The following function returns the sum of data lengths of a tuple. The space
occupied by the field structs or the tuple struct is not counted. */
UNIV_INLINE
ulint
dtuple_get_data_size(
/*=================*/
				/* out: sum of data lens */
	const dtuple_t*	tuple);	/* in: typed data tuple */
/*************************************************************************
Computes the number of externally stored fields in a data tuple. */
UNIV_INLINE
ulint
dtuple_get_n_ext(
/*=============*/
				/* out: number of fields */
	const dtuple_t*	tuple);	/* in: tuple */
/****************************************************************
Compare two data tuples, respecting the collation of character fields. */
UNIV_INTERN
int
dtuple_coll_cmp(
/*============*/
				/* out: 1, 0 , -1 if tuple1 is greater, equal,
				less, respectively, than tuple2 */
	const dtuple_t*	tuple1,	/* in: tuple 1 */
	const dtuple_t*	tuple2);/* in: tuple 2 */
/****************************************************************
Folds a prefix given as the number of fields of a tuple. */
UNIV_INLINE
ulint
dtuple_fold(
/*========*/
				/* out: the folded value */
	const dtuple_t*	tuple,	/* in: the tuple */
	ulint		n_fields,/* in: number of complete fields to fold */
	ulint		n_bytes,/* in: number of bytes to fold in an
				incomplete last field */
	dulint		tree_id)/* in: index tree id */
	__attribute__((pure));
/***********************************************************************
Sets types of fields binary in a tuple. */
UNIV_INLINE
void
dtuple_set_types_binary(
/*====================*/
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		n);	/* in: number of fields to set */
/**************************************************************************
Checks if a dtuple contains an SQL null value. */
UNIV_INLINE
ibool
dtuple_contains_null(
/*=================*/
				/* out: TRUE if some field is SQL null */
	const dtuple_t*	tuple);	/* in: dtuple */
/**************************************************************
Checks that a data field is typed. Asserts an error if not. */
UNIV_INTERN
ibool
dfield_check_typed(
/*===============*/
				/* out: TRUE if ok */
	const dfield_t*	field);	/* in: data field */
/**************************************************************
Checks that a data tuple is typed. Asserts an error if not. */
UNIV_INTERN
ibool
dtuple_check_typed(
/*===============*/
				/* out: TRUE if ok */
	const dtuple_t*	tuple);	/* in: tuple */
/**************************************************************
Checks that a data tuple is typed. */
UNIV_INTERN
ibool
dtuple_check_typed_no_assert(
/*=========================*/
				/* out: TRUE if ok */
	const dtuple_t*	tuple);	/* in: tuple */
#ifdef UNIV_DEBUG
/**************************************************************
Validates the consistency of a tuple which must be complete, i.e,
all fields must have been set. */
UNIV_INTERN
ibool
dtuple_validate(
/*============*/
				/* out: TRUE if ok */
	const dtuple_t*	tuple);	/* in: tuple */
#endif /* UNIV_DEBUG */
/*****************************************************************
Pretty prints a dfield value according to its data type. */
UNIV_INTERN
void
dfield_print(
/*=========*/
	const dfield_t*	dfield);/* in: dfield */
/*****************************************************************
Pretty prints a dfield value according to its data type. Also the hex string
is printed if a string contains non-printable characters. */
UNIV_INTERN
void
dfield_print_also_hex(
/*==================*/
	const dfield_t*	dfield);	 /* in: dfield */
/**************************************************************
The following function prints the contents of a tuple. */
UNIV_INTERN
void
dtuple_print(
/*=========*/
	FILE*		f,	/* in: output stream */
	const dtuple_t*	tuple);	/* in: tuple */
/******************************************************************
Moves parts of long fields in entry to the big record vector so that
the size of tuple drops below the maximum record size allowed in the
database. Moves data only from those fields which are not necessary
to determine uniquely the insertion place of the tuple in the index. */
UNIV_INTERN
big_rec_t*
dtuple_convert_big_rec(
/*===================*/
				/* out, own: created big record vector,
				NULL if we are not able to shorten
				the entry enough, i.e., if there are
				too many fixed-length or short fields
				in entry or the index is clustered */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in/out: index entry */
	ulint*		n_ext);	/* in/out: number of
				externally stored columns */
/******************************************************************
Puts back to entry the data stored in vector. Note that to ensure the
fields in entry can accommodate the data, vector must have been created
from entry with dtuple_convert_big_rec. */
UNIV_INTERN
void
dtuple_convert_back_big_rec(
/*========================*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: entry whose data was put to vector */
	big_rec_t*	vector);/* in, own: big rec vector; it is
				freed in this function */
/******************************************************************
Frees the memory in a big rec vector. */
UNIV_INLINE
void
dtuple_big_rec_free(
/*================*/
	big_rec_t*	vector);	/* in, own: big rec vector; it is
				freed in this function */

/*######################################################################*/

/* Structure for an SQL data field */
struct dfield_struct{
	void*		data;	/* pointer to data */
	unsigned	ext:1;	/* TRUE=externally stored, FALSE=local */
	unsigned	len:32;	/* data length; UNIV_SQL_NULL if SQL null */
	dtype_t		type;	/* type of data */
};

struct dtuple_struct {
	ulint		info_bits;	/* info bits of an index record:
					the default is 0; this field is used
					if an index record is built from
					a data tuple */
	ulint		n_fields;	/* number of fields in dtuple */
	ulint		n_fields_cmp;	/* number of fields which should
					be used in comparison services
					of rem0cmp.*; the index search
					is performed by comparing only these
					fields, others are ignored; the
					default value in dtuple creation is
					the same value as n_fields */
	dfield_t*	fields;		/* fields */
	UT_LIST_NODE_T(dtuple_t) tuple_list;
					/* data tuples can be linked into a
					list using this field */
#ifdef UNIV_DEBUG
	ulint		magic_n;
# define		DATA_TUPLE_MAGIC_N	65478679
#endif /* UNIV_DEBUG */
};

/* A slot for a field in a big rec vector */

typedef struct big_rec_field_struct	big_rec_field_t;
struct big_rec_field_struct {
	ulint		field_no;	/* field number in record */
	ulint		len;		/* stored data len */
	const void*	data;		/* stored data */
};

/* Storage format for overflow data in a big record, that is, a record
which needs external storage of data fields */

struct big_rec_struct {
	mem_heap_t*	heap;		/* memory heap from which allocated */
	ulint		n_fields;	/* number of stored fields */
	big_rec_field_t* fields;	/* stored fields */
};

#ifndef UNIV_NONINL
#include "data0data.ic"
#endif

#endif
