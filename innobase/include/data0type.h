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

/* Data main types of SQL data */
#define	DATA_VARCHAR	1	/* character varying */
#define DATA_CHAR	2	/* fixed length character */
#define DATA_FIXBINARY	3	/* binary string of fixed length */
#define DATA_BINARY	4	/* binary string */
#define DATA_BLOB	5	/* binary large object, or a TEXT type; if
				prtype & DATA_NONLATIN1 != 0 the data must
				be compared by MySQL as a whole field; if
				prtype & DATA_BINARY_TYPE == 0, then this is
				actually a TEXT column */
#define	DATA_INT	6	/* integer: can be any size 1 - 8 bytes */
#define	DATA_SYS_CHILD	7	/* address of the child page in node pointer */
#define	DATA_SYS	8	/* system column */
/* Data types >= DATA_FLOAT must be compared using the whole field, not as
binary strings */
#define DATA_FLOAT	9
#define DATA_DOUBLE	10
#define DATA_DECIMAL	11	/* decimal number stored as an ASCII string */
#define	DATA_VARMYSQL	12	/* non-latin1 varying length char */
#define	DATA_MYSQL	13	/* non-latin1 fixed length char */
#define DATA_MTYPE_MAX	63	/* dtype_store_for_order_and_null_size()
				requires the values are <= 63 */
/*-------------------------------------------*/
/* In the lowest byte in the precise type we store the MySQL type code
(not applicable for system columns). */

#define DATA_ENGLISH    4       /* English language character string: this
				is a relic from pre-MySQL time and only used
				for InnoDB's own system tables */
#define DATA_ERROR	111	/* another relic from pre-MySQL time */

#define DATA_MYSQL_TYPE_MASK 255 /* AND with this mask to extract the MySQL
				 type from the precise type */

/* Precise data types for system columns and the length of those columns;
NOTE: the values must run from 0 up in the order given! All codes must
be less than 256 */
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
/*-------------------------------------------*/
/* Flags ORed to the precise data type */
#define DATA_NOT_NULL	256	/* this is ORed to the precise type when
				the column is declared as NOT NULL */
#define DATA_UNSIGNED	512	/* this id ORed to the precise type when
				we have an unsigned integer type */
#define	DATA_BINARY_TYPE 1024	/* if the data type is a binary character
				string, this is ORed to the precise type:
				this only holds for tables created with
				>= MySQL-4.0.14 */
#define	DATA_NONLATIN1 2048	/* if the data type is a DATA_BLOB (actually
				TEXT) of a non-latin1 type, this is ORed to
				the precise type: this only holds for tables
				created with >= MySQL-4.0.14 */
/*-------------------------------------------*/

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
	dtype_t*	type);	/* in: type */
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
Stores for a type the information which determines its alphabetical ordering
and the storage size of an SQL NULL value. */
UNIV_INLINE
void
dtype_store_for_order_and_null_size(
/*================================*/
	byte*		buf,	/* in: buffer for DATA_ORDER_NULL_TYPE_BUF_SIZE
				bytes where we store the info */
	dtype_t*	type);	/* in: type struct */
/**************************************************************************
Reads to a type the stored information which determines its alphabetical
ordering and the storage size of an SQL NULL value. */
UNIV_INLINE
void
dtype_read_for_order_and_null_size(
/*===============================*/
	dtype_t*	type,	/* in: type struct */
	byte*		buf);	/* in: buffer for the stored order info */
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
