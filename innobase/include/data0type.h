/******************************************************
Data types

(c) 1996 Innobase Oy

Created 1/16/1996 Heikki Tuuri
*******************************************************/

#ifndef data0type_h
#define data0type_h

#include "univ.i"

/* SQL data type struct */
typedef struct dtype_struct		dtype_t;

/* This variable is initialized as the standard binary variable length
data type */
extern dtype_t* 	dtype_binary;

/* Data main types of SQL data; NOTE! character data types requiring
collation transformation must have the smallest codes! All codes must be
less than 256! */
#define	DATA_VARCHAR	1	/* character varying */
#define DATA_CHAR	2	/* fixed length character */
#define DATA_FIXBINARY	3	/* binary string of fixed length */
#define DATA_BINARY	4	/* binary string */
#define DATA_BLOB	5	/* binary large object */
#define	DATA_INT	6	/* integer: can be any size 1 - 8 bytes */
#define	DATA_SYS_CHILD	7	/* address of the child page in node pointer */
#define	DATA_SYS	8	/* system column */
/* Data types >= DATA_FLOAT must be compared using the whole field, not as
binary strings */
#define DATA_FLOAT	9
#define DATA_DOUBLE	10
#define DATA_DECIMAL	11	/* decimal number stored as an ASCII string */
#define	DATA_VARMYSQL	12	/* data types for which comparisons must be */
#define	DATA_MYSQL	13	/* made by MySQL */
#define DATA_ERROR	111	/* error value */
#define DATA_MTYPE_MAX	255
/*-------------------------------------------*/
/* Precise data types for system columns; NOTE: the values must run
from 0 up in the order given! All codes must be less than 256! */
#define	DATA_ROW_ID	0	/* row id: a dulint */
#define DATA_ROW_ID_LEN	6	/* stored length for row id */
#define DATA_TRX_ID	1	/* transaction id: 6 bytes */
#define DATA_TRX_ID_LEN	6
#define	DATA_ROLL_PTR	2	/* rollback data pointer: 7 bytes */
#define DATA_ROLL_PTR_LEN 7
#define DATA_MIX_ID	3	/* mixed index label: a dulint, stored in
				a row in a compressed form */
#define DATA_MIX_ID_LEN	9	/* maximum stored length for mix id (in a
				compressed dulint form) */
#define	DATA_N_SYS_COLS 4 	/* number of system columns defined above */
#define DATA_NOT_NULL	256	/* this is ORed to the precise type when
				the column is declared as NOT NULL */
#define DATA_UNSIGNED	512	/* this id ORed to the precise type when
				we have an unsigned integer type */
/*-------------------------------------------*/

/* Precise types of a char or varchar data. All codes must be less than 256! */
#define DATA_ENGLISH	4	/* English language character string */
#define	DATA_FINNISH	5	/* Finnish */
#define DATA_PRTYPE_MAX	255

/* This many bytes we need to store the type information affecting the
alphabetical order for a single field and decide the storage size of an
SQL null*/
#define DATA_ORDER_NULL_TYPE_BUF_SIZE	4

/*************************************************************************
Sets a data type structure. */
UNIV_INLINE
void
dtype_set(
/*======*/
	dtype_t*	type,	/* in: type struct to init */
	ulint		mtype,	/* in: main data type */
	ulint		prtype,	/* in: precise type */
	ulint		len,	/* in: length of type */
	ulint		prec);	/* in: precision of type */
/*************************************************************************
Copies a data type structure. */
UNIV_INLINE
void
dtype_copy(
/*=======*/
	dtype_t*	type1,	/* in: type struct to copy to */
	dtype_t*	type2);	/* in: type struct to copy from */
/*************************************************************************
Gets the SQL main data type. */
UNIV_INLINE
ulint
dtype_get_mtype(
/*============*/
	dtype_t*	type);
/*************************************************************************
Gets the precise data type. */
UNIV_INLINE
ulint
dtype_get_prtype(
/*=============*/
	dtype_t*	type);
/*************************************************************************
Gets the type length. */
UNIV_INLINE
ulint
dtype_get_len(
/*==========*/
	dtype_t*	type);
/*************************************************************************
Gets the type precision. */
UNIV_INLINE
ulint
dtype_get_prec(
/*===========*/
	dtype_t*	type);
/*************************************************************************
Gets the padding character code for the type. */
UNIV_INLINE
ulint
dtype_get_pad_char(
/*===============*/
				/* out: padding character code, or
				ULINT_UNDEFINED if no padding specified */
	dtype_t*	type);	/* in: typeumn */
/***************************************************************************
Returns the size of a fixed size data type, 0 if not a fixed size type. */
UNIV_INLINE
ulint
dtype_get_fixed_size(
/*=================*/
				/* out: fixed size, or 0 */
	dtype_t*	type);	/* in: type */
/***************************************************************************
Returns a stored SQL NULL size for a type. For fixed length types it is
the fixed length of the type, otherwise 0. */
UNIV_INLINE
ulint
dtype_get_sql_null_size(
/*====================*/
				/* out: SQL null storage size */
	dtype_t*	type);	/* in: type */
/***************************************************************************
Returns TRUE if a type is of a fixed size. */
UNIV_INLINE
ibool
dtype_is_fixed_size(
/*================*/
				/* out: TRUE if fixed size */
	dtype_t*	type);	/* in: type */
/**************************************************************************
Stores to a type the information which determines its alphabetical
ordering. */
UNIV_INLINE
void
dtype_store_for_order_and_null_size(
/*================================*/
	byte*		buf,	/* in: buffer for DATA_ORDER_NULL_TYPE_BUF_SIZE
				bytes */
	dtype_t*	type);	/* in: type struct */
/**************************************************************************
Reads of a type the stored information which determines its alphabetical
ordering. */
UNIV_INLINE
void
dtype_read_for_order_and_null_size(
/*===============================*/
	dtype_t*	type,	/* in: type struct */
	byte*		buf);	/* in: buffer for type order info */
/*************************************************************************
Validates a data type structure. */

ibool
dtype_validate(
/*===========*/
				/* out: TRUE if ok */
	dtype_t*	type);	/* in: type struct to validate */
/*************************************************************************
Prints a data type structure. */

void
dtype_print(
/*========*/
	dtype_t*	type);	/* in: type */

/* Structure for an SQL data type */

struct dtype_struct{
	ulint	mtype;		/* main data type */
	ulint	prtype;		/* precise type; MySQL data type */

	/* remaining two fields do not affect alphabetical ordering: */

	ulint	len;		/* length */
	ulint	prec;		/* precision */
};

#ifndef UNIV_NONINL
#include "data0type.ic"
#endif

#endif
