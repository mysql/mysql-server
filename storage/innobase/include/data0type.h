/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file include/data0type.h
Data types

Created 1/16/1996 Heikki Tuuri
*******************************************************/

#ifndef data0type_h
#define data0type_h

#include "univ.i"

extern ulint	data_mysql_default_charset_coll;
#define DATA_MYSQL_LATIN1_SWEDISH_CHARSET_COLL 8
#define DATA_MYSQL_BINARY_CHARSET_COLL 63

/* SQL data type struct */
struct dtype_t;

/* SQL Like operator comparison types */
enum ib_like_t {
	IB_LIKE_EXACT,                  /* e.g.  STRING */
	IB_LIKE_PREFIX,                 /* e.g., STRING% */
	IB_LIKE_SUFFIX,                 /* e.g., %STRING */
	IB_LIKE_SUBSTR,                 /* e.g., %STRING% */
	IB_LIKE_REGEXP                  /* Future */
};

/*-------------------------------------------*/
/* The 'MAIN TYPE' of a column */
#define DATA_MISSING	0	/* missing column */
#define	DATA_VARCHAR	1	/* character varying of the
				latin1_swedish_ci charset-collation; note
				that the MySQL format for this, DATA_BINARY,
				DATA_VARMYSQL, is also affected by whether the
				'precise type' contains
				DATA_MYSQL_TRUE_VARCHAR */
#define DATA_CHAR	2	/* fixed length character of the
				latin1_swedish_ci charset-collation */
#define DATA_FIXBINARY	3	/* binary string of fixed length */
#define DATA_BINARY	4	/* binary string */
#define DATA_BLOB	5	/* binary large object, or a TEXT type;
				if prtype & DATA_BINARY_TYPE == 0, then this is
				actually a TEXT column (or a BLOB created
				with < 4.0.14; since column prefix indexes
				came only in 4.0.14, the missing flag in BLOBs
				created before that does not cause any harm) */
#define	DATA_INT	6	/* integer: can be any size 1 - 8 bytes */
#define	DATA_SYS_CHILD	7	/* address of the child page in node pointer */
#define	DATA_SYS	8	/* system column */

/* Data types >= DATA_FLOAT must be compared using the whole field, not as
binary strings */

#define DATA_FLOAT	9
#define DATA_DOUBLE	10
#define DATA_DECIMAL	11	/* decimal number stored as an ASCII string */
#define	DATA_VARMYSQL	12	/* any charset varying length char */
#define	DATA_MYSQL	13	/* any charset fixed length char */
				/* NOTE that 4.1.1 used DATA_MYSQL and
				DATA_VARMYSQL for all character sets, and the
				charset-collation for tables created with it
				can also be latin1_swedish_ci */
#define DATA_MTYPE_MAX	63	/* dtype_store_for_order_and_null_size()
				requires the values are <= 63 */
/*-------------------------------------------*/
/* The 'PRECISE TYPE' of a column */
/*
Tables created by a MySQL user have the following convention:

- In the least significant byte in the precise type we store the MySQL type
code (not applicable for system columns).

- In the second least significant byte we OR flags DATA_NOT_NULL,
DATA_UNSIGNED, DATA_BINARY_TYPE.

- In the third least significant byte of the precise type of string types we
store the MySQL charset-collation code. In DATA_BLOB columns created with
< 4.0.14 we do not actually know if it is a BLOB or a TEXT column. Since there
are no indexes on prefixes of BLOB or TEXT columns in < 4.0.14, this is no
problem, though.

Note that versions < 4.1.2 or < 5.0.1 did not store the charset code to the
precise type, since the charset was always the default charset of the MySQL
installation. If the stored charset code is 0 in the system table SYS_COLUMNS
of InnoDB, that means that the default charset of this MySQL installation
should be used.

When loading a table definition from the system tables to the InnoDB data
dictionary cache in main memory, InnoDB versions >= 4.1.2 and >= 5.0.1 check
if the stored charset-collation is 0, and if that is the case and the type is
a non-binary string, replace that 0 by the default charset-collation code of
this MySQL installation. In short, in old tables, the charset-collation code
in the system tables on disk can be 0, but in in-memory data structures
(dtype_t), the charset-collation code is always != 0 for non-binary string
types.

In new tables, in binary string types, the charset-collation code is the
MySQL code for the 'binary charset', that is, != 0.

For binary string types and for DATA_CHAR, DATA_VARCHAR, and for those
DATA_BLOB which are binary or have the charset-collation latin1_swedish_ci,
InnoDB performs all comparisons internally, without resorting to the MySQL
comparison functions. This is to save CPU time.

InnoDB's own internal system tables have different precise types for their
columns, and for them the precise type is usually not used at all.
*/

#define DATA_ENGLISH	4	/* English language character string: this
				is a relic from pre-MySQL time and only used
				for InnoDB's own system tables */
#define DATA_ERROR	111	/* another relic from pre-MySQL time */

#define DATA_MYSQL_TYPE_MASK 255 /* AND with this mask to extract the MySQL
				 type from the precise type */
#define DATA_MYSQL_TRUE_VARCHAR 15 /* MySQL type code for the >= 5.0.3
				   format true VARCHAR */

/* Precise data types for system columns and the length of those columns;
NOTE: the values must run from 0 up in the order given! All codes must
be less than 256 */
#define	DATA_ROW_ID	0	/* row id: a 48-bit integer */
#define DATA_ROW_ID_LEN	6	/* stored length for row id */

#define DATA_TRX_ID	1	/* transaction id: 6 bytes */
#define DATA_TRX_ID_LEN	6

#define	DATA_ROLL_PTR	2	/* rollback data pointer: 7 bytes */
#define DATA_ROLL_PTR_LEN 7

#define	DATA_N_SYS_COLS 3	/* number of system columns defined above */

#define DATA_FTS_DOC_ID	3	/* Used as FTS DOC ID column */

#define DATA_SYS_PRTYPE_MASK 0xF /* mask to extract the above from prtype */

/* Flags ORed to the precise data type */
#define DATA_NOT_NULL	256	/* this is ORed to the precise type when
				the column is declared as NOT NULL */
#define DATA_UNSIGNED	512	/* this id ORed to the precise type when
				we have an unsigned integer type */
#define	DATA_BINARY_TYPE 1024	/* if the data type is a binary character
				string, this is ORed to the precise type:
				this only holds for tables created with
				>= MySQL-4.0.14 */
/* #define	DATA_NONLATIN1	2048 This is a relic from < 4.1.2 and < 5.0.1.
				In earlier versions this was set for some
				BLOB columns.
*/
#define	DATA_LONG_TRUE_VARCHAR 4096	/* this is ORed to the precise data
				type when the column is true VARCHAR where
				MySQL uses 2 bytes to store the data len;
				for shorter VARCHARs MySQL uses only 1 byte */
/*-------------------------------------------*/

/* This many bytes we need to store the type information affecting the
alphabetical order for a single field and decide the storage size of an
SQL null*/
#define DATA_ORDER_NULL_TYPE_BUF_SIZE		4
/* In the >= 4.1.x storage format we add 2 bytes more so that we can also
store the charset-collation number; one byte is left unused, though */
#define DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE	6

/* Maximum multi-byte character length in bytes, plus 1 */
#define DATA_MBMAX	5

/* Pack mbminlen, mbmaxlen to mbminmaxlen. */
#define DATA_MBMINMAXLEN(mbminlen, mbmaxlen)	\
	((mbmaxlen) * DATA_MBMAX + (mbminlen))
/* Get mbminlen from mbminmaxlen. Cast the result of UNIV_EXPECT to ulint
because in GCC it returns a long. */
#define DATA_MBMINLEN(mbminmaxlen) ((ulint) \
                                    UNIV_EXPECT(((mbminmaxlen) % DATA_MBMAX), \
                                                1))
/* Get mbmaxlen from mbminmaxlen. */
#define DATA_MBMAXLEN(mbminmaxlen) ((ulint) ((mbminmaxlen) / DATA_MBMAX))

/* We now support 15 bits (up to 32767) collation number */
#define MAX_CHAR_COLL_NUM	32767

/* Mask to get the Charset Collation number (0x7fff) */
#define CHAR_COLL_MASK		MAX_CHAR_COLL_NUM

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Gets the MySQL type code from a dtype.
@return	MySQL type code; this is NOT an InnoDB type code! */
UNIV_INLINE
ulint
dtype_get_mysql_type(
/*=================*/
	const dtype_t*	type);	/*!< in: type struct */
/*********************************************************************//**
Determine how many bytes the first n characters of the given string occupy.
If the string is shorter than n characters, returns the number of bytes
the characters in the string occupy.
@return	length of the prefix, in bytes */
UNIV_INTERN
ulint
dtype_get_at_most_n_mbchars(
/*========================*/
	ulint		prtype,		/*!< in: precise type */
	ulint		mbminmaxlen,	/*!< in: minimum and maximum length of
					a multi-byte character */
	ulint		prefix_len,	/*!< in: length of the requested
					prefix, in characters, multiplied by
					dtype_get_mbmaxlen(dtype) */
	ulint		data_len,	/*!< in: length of str (in bytes) */
	const char*	str);		/*!< in: the string whose prefix
					length is being determined */
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Checks if a data main type is a string type. Also a BLOB is considered a
string type.
@return	TRUE if string type */
UNIV_INTERN
ibool
dtype_is_string_type(
/*=================*/
	ulint	mtype);	/*!< in: InnoDB main data type code: DATA_CHAR, ... */
/*********************************************************************//**
Checks if a type is a binary string type. Note that for tables created with
< 4.0.14, we do not know if a DATA_BLOB column is a BLOB or a TEXT column. For
those DATA_BLOB columns this function currently returns FALSE.
@return	TRUE if binary string type */
UNIV_INTERN
ibool
dtype_is_binary_string_type(
/*========================*/
	ulint	mtype,	/*!< in: main data type */
	ulint	prtype);/*!< in: precise type */
/*********************************************************************//**
Checks if a type is a non-binary string type. That is, dtype_is_string_type is
TRUE and dtype_is_binary_string_type is FALSE. Note that for tables created
with < 4.0.14, we do not know if a DATA_BLOB column is a BLOB or a TEXT column.
For those DATA_BLOB columns this function currently returns TRUE.
@return	TRUE if non-binary string type */
UNIV_INTERN
ibool
dtype_is_non_binary_string_type(
/*============================*/
	ulint	mtype,	/*!< in: main data type */
	ulint	prtype);/*!< in: precise type */
/*********************************************************************//**
Sets a data type structure. */
UNIV_INLINE
void
dtype_set(
/*======*/
	dtype_t*	type,	/*!< in: type struct to init */
	ulint		mtype,	/*!< in: main data type */
	ulint		prtype,	/*!< in: precise type */
	ulint		len);	/*!< in: precision of type */
/*********************************************************************//**
Copies a data type structure. */
UNIV_INLINE
void
dtype_copy(
/*=======*/
	dtype_t*	type1,	/*!< in: type struct to copy to */
	const dtype_t*	type2);	/*!< in: type struct to copy from */
/*********************************************************************//**
Gets the SQL main data type.
@return	SQL main data type */
UNIV_INLINE
ulint
dtype_get_mtype(
/*============*/
	const dtype_t*	type);	/*!< in: data type */
/*********************************************************************//**
Gets the precise data type.
@return	precise data type */
UNIV_INLINE
ulint
dtype_get_prtype(
/*=============*/
	const dtype_t*	type);	/*!< in: data type */
#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Compute the mbminlen and mbmaxlen members of a data type structure. */
UNIV_INLINE
void
dtype_get_mblen(
/*============*/
	ulint	mtype,		/*!< in: main type */
	ulint	prtype,		/*!< in: precise type (and collation) */
	ulint*	mbminlen,	/*!< out: minimum length of a
				multi-byte character */
	ulint*	mbmaxlen);	/*!< out: maximum length of a
				multi-byte character */
/*********************************************************************//**
Gets the MySQL charset-collation code for MySQL string types.
@return	MySQL charset-collation code */
UNIV_INLINE
ulint
dtype_get_charset_coll(
/*===================*/
	ulint	prtype);/*!< in: precise data type */
/*********************************************************************//**
Forms a precise type from the < 4.1.2 format precise type plus the
charset-collation code.
@return precise type, including the charset-collation code */
UNIV_INTERN
ulint
dtype_form_prtype(
/*==============*/
	ulint	old_prtype,	/*!< in: the MySQL type code and the flags
				DATA_BINARY_TYPE etc. */
	ulint	charset_coll);	/*!< in: MySQL charset-collation code */
/*********************************************************************//**
Determines if a MySQL string type is a subset of UTF-8.  This function
may return false negatives, in case further character-set collation
codes are introduced in MySQL later.
@return	TRUE if a subset of UTF-8 */
UNIV_INLINE
ibool
dtype_is_utf8(
/*==========*/
	ulint	prtype);/*!< in: precise data type */
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Gets the type length.
@return	fixed length of the type, in bytes, or 0 if variable-length */
UNIV_INLINE
ulint
dtype_get_len(
/*==========*/
	const dtype_t*	type);	/*!< in: data type */
#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Gets the minimum length of a character, in bytes.
@return minimum length of a char, in bytes, or 0 if this is not a
character type */
UNIV_INLINE
ulint
dtype_get_mbminlen(
/*===============*/
	const dtype_t*	type);	/*!< in: type */
/*********************************************************************//**
Gets the maximum length of a character, in bytes.
@return maximum length of a char, in bytes, or 0 if this is not a
character type */
UNIV_INLINE
ulint
dtype_get_mbmaxlen(
/*===============*/
	const dtype_t*	type);	/*!< in: type */
/*********************************************************************//**
Sets the minimum and maximum length of a character, in bytes. */
UNIV_INLINE
void
dtype_set_mbminmaxlen(
/*==================*/
	dtype_t*	type,		/*!< in/out: type */
	ulint		mbminlen,	/*!< in: minimum length of a char,
					in bytes, or 0 if this is not
					a character type */
	ulint		mbmaxlen);	/*!< in: maximum length of a char,
					in bytes, or 0 if this is not
					a character type */
/*********************************************************************//**
Gets the padding character code for the type.
@return	padding character code, or ULINT_UNDEFINED if no padding specified */
UNIV_INLINE
ulint
dtype_get_pad_char(
/*===============*/
	ulint	mtype,		/*!< in: main type */
	ulint	prtype);	/*!< in: precise type */
#endif /* !UNIV_HOTBACKUP */
/***********************************************************************//**
Returns the size of a fixed size data type, 0 if not a fixed size type.
@return	fixed size, or 0 */
UNIV_INLINE
ulint
dtype_get_fixed_size_low(
/*=====================*/
	ulint	mtype,		/*!< in: main type */
	ulint	prtype,		/*!< in: precise type */
	ulint	len,		/*!< in: length */
	ulint	mbminmaxlen,	/*!< in: minimum and maximum length of a
				multibyte character, in bytes */
	ulint	comp);		/*!< in: nonzero=ROW_FORMAT=COMPACT  */
#ifndef UNIV_HOTBACKUP
/***********************************************************************//**
Returns the minimum size of a data type.
@return	minimum size */
UNIV_INLINE
ulint
dtype_get_min_size_low(
/*===================*/
	ulint	mtype,		/*!< in: main type */
	ulint	prtype,		/*!< in: precise type */
	ulint	len,		/*!< in: length */
	ulint	mbminmaxlen);	/*!< in: minimum and maximum length of a
				multibyte character */
/***********************************************************************//**
Returns the maximum size of a data type. Note: types in system tables may be
incomplete and return incorrect information.
@return	maximum size */
UNIV_INLINE
ulint
dtype_get_max_size_low(
/*===================*/
	ulint	mtype,		/*!< in: main type */
	ulint	len);		/*!< in: length */
#endif /* !UNIV_HOTBACKUP */
/***********************************************************************//**
Returns the ROW_FORMAT=REDUNDANT stored SQL NULL size of a type.
For fixed length types it is the fixed length of the type, otherwise 0.
@return	SQL null storage size in ROW_FORMAT=REDUNDANT */
UNIV_INLINE
ulint
dtype_get_sql_null_size(
/*====================*/
	const dtype_t*	type,	/*!< in: type */
	ulint		comp);	/*!< in: nonzero=ROW_FORMAT=COMPACT  */
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Reads to a type the stored information which determines its alphabetical
ordering and the storage size of an SQL NULL value. */
UNIV_INLINE
void
dtype_read_for_order_and_null_size(
/*===============================*/
	dtype_t*	type,	/*!< in: type struct */
	const byte*	buf);	/*!< in: buffer for the stored order info */
/**********************************************************************//**
Stores for a type the information which determines its alphabetical ordering
and the storage size of an SQL NULL value. This is the >= 4.1.x storage
format. */
UNIV_INLINE
void
dtype_new_store_for_order_and_null_size(
/*====================================*/
	byte*		buf,	/*!< in: buffer for
				DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE
				bytes where we store the info */
	const dtype_t*	type,	/*!< in: type struct */
	ulint		prefix_len);/*!< in: prefix length to
				replace type->len, or 0 */
/**********************************************************************//**
Reads to a type the stored information which determines its alphabetical
ordering and the storage size of an SQL NULL value. This is the 4.1.x storage
format. */
UNIV_INLINE
void
dtype_new_read_for_order_and_null_size(
/*===================================*/
	dtype_t*	type,	/*!< in: type struct */
	const byte*	buf);	/*!< in: buffer for stored type order info */

/*********************************************************************//**
Returns the type's SQL name (e.g. BIGINT UNSIGNED) from mtype,prtype,len
@return the SQL type name */
UNIV_INLINE
char*
dtype_sql_name(
/*===========*/
	unsigned	mtype,	/*!< in: mtype */
	unsigned	prtype,	/*!< in: prtype */
	unsigned	len,	/*!< in: len */
	char*		name,	/*!< out: SQL name */
	unsigned	name_sz);/*!< in: size of the name buffer */

#endif /* !UNIV_HOTBACKUP */

/*********************************************************************//**
Validates a data type structure.
@return	TRUE if ok */
UNIV_INTERN
ibool
dtype_validate(
/*===========*/
	const dtype_t*	type);	/*!< in: type struct to validate */
/*********************************************************************//**
Prints a data type structure. */
UNIV_INTERN
void
dtype_print(
/*========*/
	const dtype_t*	type);	/*!< in: type */

/* Structure for an SQL data type.
If you add fields to this structure, be sure to initialize them everywhere.
This structure is initialized in the following functions:
dtype_set()
dtype_read_for_order_and_null_size()
dtype_new_read_for_order_and_null_size()
sym_tab_add_null_lit() */

struct dtype_t{
	unsigned	prtype:32;	/*!< precise type; MySQL data
					type, charset code, flags to
					indicate nullability,
					signedness, whether this is a
					binary string, whether this is
					a true VARCHAR where MySQL
					uses 2 bytes to store the length */
	unsigned	mtype:8;	/*!< main data type */

	/* the remaining fields do not affect alphabetical ordering: */

	unsigned	len:16;		/*!< length; for MySQL data this
					is field->pack_length(),
					except that for a >= 5.0.3
					type true VARCHAR this is the
					maximum byte length of the
					string data (in addition to
					the string, MySQL uses 1 or 2
					bytes to store the string length) */
#ifndef UNIV_HOTBACKUP
	unsigned	mbminmaxlen:5;	/*!< minimum and maximum length of a
					character, in bytes;
					DATA_MBMINMAXLEN(mbminlen,mbmaxlen);
					mbminlen=DATA_MBMINLEN(mbminmaxlen);
					mbmaxlen=DATA_MBMINLEN(mbminmaxlen) */
#endif /* !UNIV_HOTBACKUP */
};

#ifndef UNIV_NONINL
#include "data0type.ic"
#endif

#endif
