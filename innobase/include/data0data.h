/************************************************************************
SQL data field and tuple

(c) 1994-1996 Innobase Oy

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

/* Some non-inlined functions used in the MySQL interface: */
void 
dfield_set_data_noninline(
	dfield_t* 	field,	/* in: field */
	void*		data,	/* in: data */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
void* 
dfield_get_data_noninline(
	dfield_t* field);	/* in: field */
ulint
dfield_get_len_noninline(
	dfield_t* field);	/* in: field */
ulint 
dtuple_get_n_fields_noninline(
	dtuple_t* 	tuple);	/* in: tuple */
dfield_t* 
dtuple_get_nth_field_noninline(
	dtuple_t* 	tuple,	/* in: tuple */
	ulint		n);	/* in: index of field */

/*************************************************************************
Gets pointer to the type struct of SQL data field. */
UNIV_INLINE
dtype_t*
dfield_get_type(
/*============*/
				/* out: pointer to the type struct */
	dfield_t*	field);	/* in: SQL data field */
/*************************************************************************
Sets the type struct of SQL data field. */
UNIV_INLINE
void
dfield_set_type(
/*============*/
	dfield_t*	field,	/* in: SQL data field */
	dtype_t*	type);	/* in: pointer to data type struct */
/*************************************************************************
Gets pointer to the data in a field. */
UNIV_INLINE
void* 
dfield_get_data(
/*============*/
				/* out: pointer to data */
	dfield_t* field);	/* in: field */
/*************************************************************************
Gets length of field data. */
UNIV_INLINE
ulint
dfield_get_len(
/*===========*/
				/* out: length of data; UNIV_SQL_NULL if 
				SQL null data */
	dfield_t* field);	/* in: field */
/*************************************************************************
Sets length in a field. */
UNIV_INLINE
void 
dfield_set_len(
/*===========*/
	dfield_t* 	field,	/* in: field */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
/*************************************************************************
Sets pointer to the data and length in a field. */
UNIV_INLINE
void 
dfield_set_data(
/*============*/
	dfield_t* 	field,	/* in: field */
	void*		data,	/* in: data */
	ulint		len);	/* in: length or UNIV_SQL_NULL */
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
	dfield_t* 	field1,	/* in: field to copy to */
	dfield_t*	field2);/* in: field to copy from */
/*************************************************************************
Copies a data field to another. */
UNIV_INLINE
void
dfield_copy(
/*========*/
	dfield_t*	field1,	/* in: field to copy to */
	dfield_t*	field2);/* in: field to copy from */
/*************************************************************************
Tests if data length and content is equal for two dfields. */
UNIV_INLINE
ibool
dfield_datas_are_equal(
/*===================*/
				/* out: TRUE if equal */
	dfield_t*	field1,	/* in: field */
	dfield_t*	field2);/* in: field */
/*************************************************************************
Tests if dfield data length and content is equal to the given. */
UNIV_INLINE
ibool
dfield_data_is_equal(
/*=================*/
				/* out: TRUE if equal */
	dfield_t*	field,	/* in: field */
	ulint		len,	/* in: data length or UNIV_SQL_NULL */
	byte*		data);	/* in: data */
/*************************************************************************
Gets number of fields in a data tuple. */
UNIV_INLINE
ulint 
dtuple_get_n_fields(
/*================*/
				/* out: number of fields */
	dtuple_t* 	tuple);	/* in: tuple */
/*************************************************************************
Gets nth field of a tuple. */
UNIV_INLINE
dfield_t* 
dtuple_get_nth_field(
/*=================*/
				/* out: nth field */
	dtuple_t* 	tuple,	/* in: tuple */
	ulint		n);	/* in: index of field */
/*************************************************************************
Gets info bits in a data tuple. */
UNIV_INLINE
ulint
dtuple_get_info_bits(
/*=================*/
				/* out: info bits */
	dtuple_t* 	tuple);	/* in: tuple */
/*************************************************************************
Sets info bits in a data tuple. */
UNIV_INLINE
void
dtuple_set_info_bits(
/*=================*/
	dtuple_t* 	tuple,		/* in: tuple */
	ulint		info_bits);	/* in: info bits */
/*************************************************************************
Gets number of fields used in record comparisons. */
UNIV_INLINE
ulint
dtuple_get_n_fields_cmp(
/*====================*/
				/* out: number of fields used in comparisons
				in rem0cmp.* */
	dtuple_t*	tuple);	/* in: tuple */
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

/*************************************************************************
Creates a dtuple for use in MySQL. */

dtuple_t*
dtuple_create_for_mysql(
/*====================*/
			/* out, own created dtuple */
	void** heap,    /* out: created memory heap */
	ulint n_fields); /* in: number of fields */
/*************************************************************************
Frees a dtuple used in MySQL. */

void
dtuple_free_for_mysql(
/*==================*/
	void* heap);
/*************************************************************************
Sets number of fields used in a tuple. Normally this is set in
dtuple_create, but if you want later to set it smaller, you can use this. */ 

void
dtuple_set_n_fields(
/*================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		n_fields);	/* in: number of fields */
/**************************************************************
The following function returns the sum of data lengths of a tuple. The space
occupied by the field structs or the tuple struct is not counted. */
UNIV_INLINE
ulint
dtuple_get_data_size(
/*=================*/
				/* out: sum of data lens */
	dtuple_t*	tuple);	/* in: typed data tuple */
/****************************************************************
Returns TRUE if lengths of two dtuples are equal and respective data fields
in them are equal. */
UNIV_INLINE
ibool
dtuple_datas_are_equal(
/*===================*/
				/* out: TRUE if length and datas are equal */
	dtuple_t*	tuple1,	/* in: tuple 1 */
	dtuple_t*	tuple2);	/* in: tuple 2 */
/****************************************************************
Folds a prefix given as the number of fields of a tuple. */
UNIV_INLINE
ulint
dtuple_fold(
/*========*/
				/* out: the folded value */
	dtuple_t*	tuple,	/* in: the tuple */
	ulint		n_fields,/* in: number of complete fields to fold */
	ulint		n_bytes,/* in: number of bytes to fold in an
				incomplete last field */
	dulint		tree_id);/* in: index tree id */
/***********************************************************************
Sets types of fields binary in a tuple. */
UNIV_INLINE
void
dtuple_set_types_binary(
/*====================*/
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		n);	/* in: number of fields to set */
/**************************************************************
Checks that a data field is typed. Asserts an error if not. */

ibool
dfield_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dfield_t*	field);	/* in: data field */
/**************************************************************
Checks that a data tuple is typed. Asserts an error if not. */

ibool
dtuple_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple);	/* in: tuple */
/**************************************************************
Validates the consistency of a tuple which must be complete, i.e,
all fields must have been set. */

ibool
dtuple_validate(
/*============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple);	/* in: tuple */
/*****************************************************************
Pretty prints a dfield value according to its data type. */

void
dfield_print(
/*=========*/
	dfield_t*	dfield);/* in: dfield */
/*****************************************************************
Pretty prints a dfield value according to its data type. Also the hex string
is printed if a string contains non-printable characters. */ 

void
dfield_print_also_hex(
/*==================*/
	dfield_t*	dfield);	 /* in: dfield */
/**************************************************************
The following function prints the contents of a tuple. */

void
dtuple_print(
/*=========*/
	dtuple_t*	tuple);	/* in: tuple */
/**************************************************************
The following function prints the contents of a tuple to a buffer. */

ulint
dtuple_sprintf(
/*===========*/
				/* out: printed length in bytes */
	char*		buf,	/* in: print buffer */
	ulint		buf_len,/* in: buf length in bytes */
	dtuple_t*	tuple);	/* in: tuple */
/******************************************************************
Moves parts of long fields in entry to the big record vector so that
the size of tuple drops below the maximum record size allowed in the
database. Moves data only from those fields which are not necessary
to determine uniquely the insertion place of the tuple in the index. */

big_rec_t*
dtuple_convert_big_rec(
/*===================*/
				/* out, own: created big record vector,
				NULL if we are not able to shorten
				the entry enough, i.e., if there are
				too many short fields in entry */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry);	/* in: index entry */
/******************************************************************
Puts back to entry the data stored in vector. Note that to ensure the
fields in entry can accommodate the data, vector must have been created
from entry with dtuple_convert_big_rec. */

void
dtuple_convert_back_big_rec(
/*========================*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: entry whose data was put to vector */
	big_rec_t*	vector);/* in, own: big rec vector; it is
				freed in this function */
/******************************************************************
Frees the memory in a big rec vector. */

void
dtuple_big_rec_free(
/*================*/
	big_rec_t*	vector);	/* in, own: big rec vector; it is
				freed in this function */
/***************************************************************
Generates a random tuple. */

dtuple_t*
dtuple_gen_rnd_tuple(
/*=================*/
				/* out: pointer to the tuple */
	mem_heap_t*	heap);	/* in: memory heap where generated */
/*******************************************************************
Generates a test tuple for sort and comparison tests. */

void
dtuple_gen_test_tuple(
/*==================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 3 fields */
	ulint		i);	/* in: a number, 0 <= i < 512 */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_test_tuple3(
/*===================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 3 fields */
	ulint		i,	/* in: a number < 1000000 */
	ulint		type,	/* in: DTUPLE_TEST_FIXED30, ... */
	byte*		buf);	/* in: a buffer of size >= 8 bytes */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_search_tuple3(
/*=====================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 or 2 fields */
	ulint		i,	/* in: a number < 1000000 */
	byte*		buf);	/* in: a buffer of size >= 8 bytes */
/*******************************************************************
Generates a test tuple for TPC-A speed test. */

void
dtuple_gen_test_tuple_TPC_A(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 3 fields */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_search_tuple_TPC_A(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */
/*******************************************************************
Generates a test tuple for TPC-C speed test. */

void
dtuple_gen_test_tuple_TPC_C(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 12 fields */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */
/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_search_tuple_TPC_C(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf);	/* in: a buffer of size >= 16 bytes */

/* Types of the third field in dtuple_gen_test_tuple3 */	
#define DTUPLE_TEST_FIXED30	1
#define DTUPLE_TEST_RND30	2
#define DTUPLE_TEST_RND3500	3
#define DTUPLE_TEST_FIXED2000	4
#define DTUPLE_TEST_FIXED3	5

/*######################################################################*/

/* Structure for an SQL data field */
struct dfield_struct{
	void*		data;	/* pointer to data */
	ulint		len;	/* data length; UNIV_SQL_NULL if SQL null; */
	dtype_t		type;	/* type of data */
	ulint		col_no;	/* when building index entries, the column
				number can be stored here */
};

struct dtuple_struct {
	ulint		info_bits;	/* info bits of an index record:
					default is 0; this field is used
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
	ulint		magic_n;	
};
#define	DATA_TUPLE_MAGIC_N	65478679

/* A slot for a field in a big rec vector */

typedef struct big_rec_field_struct 	big_rec_field_t;
struct big_rec_field_struct {
	ulint		field_no;	/* field number in record */
	ulint		len;		/* stored data len */
	byte*		data;		/* stored data */
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
