/*****************************************************************************

Copyright (c) 2008, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
#ifndef INNOBASE_API_H
#define INNOBASE_API_H

#include "db0err.h"

/* API_BEGIN_INCLUDE */
#include <stdio.h>

#ifdef _MSC_VER
#define strncasecmp		_strnicmp
#define strcasecmp		_stricmp
#endif

#if defined(__GNUC__) && (__GNUC__ > 2) && ! defined(__INTEL_COMPILER)
#define UNIV_NO_IGNORE		__attribute__ ((warn_unused_result))
#else
#define UNIV_NO_IGNORE
#endif /* __GNUC__ && __GNUC__ > 2 && !__INTEL_COMPILER */

/* See comment about ib_bool_t as to why the two macros are unsigned long. */
/** The boolean value of "true" used internally within InnoDB */
#define IB_TRUE			0x1UL
/** The boolean value of "false" used internally within InnoDB */
#define IB_FALSE		0x0UL


/* Basic types used by the InnoDB API. */
/** All InnoDB error codes are represented by ib_err_t */
typedef enum db_err		ib_err_t;
/** Representation of a byte within InnoDB */
typedef unsigned char		ib_byte_t;
/** Representation of an unsigned long int within InnoDB */
typedef unsigned long int	ib_ulint_t;

/* We assume C99 support except when using VisualStudio. */
#if !defined(_MSC_VER)
#include <stdint.h>
#endif /* _MSC_VER */

/* Integer types used by the API. Microsft VS defines its own types
and we use the Microsoft types when building with Visual Studio. */
#if defined(_MSC_VER)
/** A signed 8 bit integral type. */
typedef __int8			ib_i8_t;
#else
/** A signed 8 bit integral type. */
typedef int8_t                  ib_i8_t;
#endif

#if defined(_MSC_VER)
/** An unsigned 8 bit integral type. */
typedef unsigned __int8		ib_u8_t;
#else
/** An unsigned 8 bit integral type. */
typedef uint8_t                 ib_u8_t;
#endif

#if defined(_MSC_VER)
/** A signed 16 bit integral type. */
typedef __int16			ib_i16_t;
#else
/** A signed 16 bit integral type. */
typedef int16_t                 ib_i16_t;
#endif

#if defined(_MSC_VER)
/** An unsigned 16 bit integral type. */
typedef unsigned __int16	ib_u16_t;
#else
/** An unsigned 16 bit integral type. */
typedef uint16_t                ib_u16_t;
#endif

#if defined(_MSC_VER)
/** A signed 32 bit integral type. */
typedef __int32			ib_i32_t;
#else
/** A signed 32 bit integral type. */
typedef int32_t                 ib_i32_t;
#endif

#if defined(_MSC_VER)
/** An unsigned 32 bit integral type. */
typedef unsigned __int32	ib_u32_t;
#else
/** An unsigned 32 bit integral type. */
typedef uint32_t                ib_u32_t;
#endif

#if defined(_MSC_VER)
/** A signed 64 bit integral type. */
typedef __int64			ib_i64_t;
#else
/** A signed 64 bit integral type. */
typedef int64_t                 ib_i64_t;
#endif

#if defined(_MSC_VER)
/** An unsigned 64 bit integral type. */
typedef unsigned __int64	ib_u64_t;
#else
/** An unsigned 64 bit integral type. */
typedef uint64_t                ib_u64_t;
#endif

typedef void*			ib_opaque_t;
typedef ib_opaque_t		ib_charset_t;
typedef ib_ulint_t		ib_bool_t;
typedef ib_u64_t		ib_id_u64_t;

/** @enum ib_cfg_type_t Possible types for a configuration variable. */
typedef enum {
	IB_CFG_IBOOL,			/*!< The configuration parameter is
					of type ibool */

	/* XXX Can we avoid having different types for ulint and ulong?
	- On Win64 "unsigned long" is 32 bits
	- ulong is always defined as "unsigned long"
	- On Win64 ulint is defined as 64 bit integer
	=> On Win64 ulint != ulong.
	If we typecast all ulong and ulint variables to the smaller type
	ulong, then we will cut the range of the ulint variables.
	This is not a problem for most ulint variables because their max
	allowed values do not exceed 2^32-1 (e.g. log_groups is ulint
	but its max allowed value is 10). BUT buffer_pool_size and
	log_file_size allow up to 2^64-1. */

	IB_CFG_ULINT,			/*!< The configuration parameter is
					of type ulint */

	IB_CFG_ULONG,			/*!< The configuration parameter is
					of type ulong */

	IB_CFG_TEXT,			/*!< The configuration parameter is
					of type char* */

	IB_CFG_CB			/*!< The configuration parameter is
					a callback parameter */
} ib_cfg_type_t;

/** @enum ib_col_type_t  column types that are supported. */
typedef enum {
	IB_VARCHAR =	1,		/*!< Character varying length. The
					column is not padded. */

	IB_CHAR =	2,		/*!< Fixed length character string. The
					column is padded to the right. */

	IB_BINARY =	3,		/*!< Fixed length binary, similar to
					IB_CHAR but the column is not padded
					to the right. */

	IB_VARBINARY =	4,		/*!< Variable length binary */

	IB_BLOB	=	5,		/*!< Binary large object, or
					a TEXT type */

	IB_INT =	6,		/*!< Integer: can be any size
					from 1 - 8 bytes. If the size is
					1, 2, 4 and 8 bytes then you can use
					the typed read and write functions. For
					other sizes you will need to use the
					ib_col_get_value() function and do the
					conversion yourself. */

	IB_SYS =	8,		/*!< System column, this column can
					be one of DATA_TRX_ID, DATA_ROLL_PTR
					or DATA_ROW_ID. */

	IB_FLOAT =	9,		/*!< C (float)  floating point value. */

	IB_DOUBLE =	10,		/*!> C (double) floating point value. */

	IB_DECIMAL =	11,		/*!< Decimal stored as an ASCII
					string */

	IB_VARCHAR_ANYCHARSET =	12,	/*!< Any charset, varying length */

	IB_CHAR_ANYCHARSET =	13	/*!< Any charset, fixed length */

} ib_col_type_t;

/** @enum ib_tbl_fmt_t InnoDB table format types */
typedef enum {
	IB_TBL_REDUNDANT,		/*!< Redundant row format, the column
					type and length is stored in the row.*/

	IB_TBL_COMPACT,			/*!< Compact row format, the column
					type is not stored in the row. The
					length is stored in the row but the
					storage format uses a compact format
					to store the length of the column data
					and record data storage format also
					uses less storage. */

	IB_TBL_DYNAMIC,			/*!< Compact row format. BLOB prefixes
					are not stored in the clustered index */

	IB_TBL_COMPRESSED		/*!< Similar to dynamic format but
					with pages compressed */
} ib_tbl_fmt_t;

/** @enum ib_col_attr_t InnoDB column attributes */
typedef enum {
	IB_COL_NONE = 0,		/*!< No special attributes. */

	IB_COL_NOT_NULL = 1,		/*!< Column data can't be NULL. */

	IB_COL_UNSIGNED = 2,		/*!< Column is IB_INT and unsigned. */

	IB_COL_NOT_USED = 4,		/*!< Future use, reserved. */

	IB_COL_CUSTOM1 = 8,		/*!< Custom precision type, this is
					a bit that is ignored by InnoDB and so
					can be set and queried by users. */

	IB_COL_CUSTOM2 = 16,		/*!< Custom precision type, this is
					a bit that is ignored by InnoDB and so
					can be set and queried by users. */

	IB_COL_CUSTOM3 = 32		/*!< Custom precision type, this is
					a bit that is ignored by InnoDB and so
					can be set and queried by users. */
} ib_col_attr_t;

/* Note: must match lock0types.h */
/** @enum ib_lck_mode_t InnoDB lock modes. */
typedef enum {
	IB_LOCK_IS = 0,			/*!< Intention shared, an intention
					lock should be used to lock tables */

	IB_LOCK_IX,			/*!< Intention exclusive, an intention
					lock should be used to lock tables */

	IB_LOCK_S,			/*!< Shared locks should be used to
					lock rows */

	IB_LOCK_X,			/*!< Exclusive locks should be used to
					lock rows*/

	IB_LOCK_NOT_USED,		/*!< Future use, reserved */

	IB_LOCK_NONE,			/*!< This is used internally to note
					consistent read */

	IB_LOCK_NUM = IB_LOCK_NONE	/*!< number of lock modes */
} ib_lck_mode_t;

typedef enum {
	IB_CLUSTERED = 1,	/*!< clustered index */
	IB_UNIQUE = 2		/*!< unique index */
} ib_index_type_t;

/** @enum ib_srch_mode_t InnoDB cursor search modes for ib_cursor_moveto().
Note: Values must match those found in page0cur.h */
typedef enum {
	IB_CUR_G = 1,			/*!< If search key is not found then
					position the cursor on the row that
					is greater than the search key */

	IB_CUR_GE = 2,			/*!< If the search key not found then
					position the cursor on the row that
					is greater than or equal to the search
					key */

	IB_CUR_L = 3,			/*!< If search key is not found then
					position the cursor on the row that
					is less than the search key */

	IB_CUR_LE = 4			/*!< If search key is not found then
					position the cursor on the row that
					is less than or equal to the search
					key */
} ib_srch_mode_t;

/** @enum ib_match_mode_t Various match modes used by ib_cursor_moveto() */
typedef enum {
	IB_CLOSEST_MATCH,		/*!< Closest match possible */

	IB_EXACT_MATCH,			/*!< Search using a complete key
					value */

	IB_EXACT_PREFIX			/*!< Search using a key prefix which
					must match to rows: the prefix may
					contain an incomplete field (the
					last field in prefix may be just
					a prefix of a fixed length column) */
} ib_match_mode_t;

/** @struct ib_col_meta_t InnoDB column meta data. */
typedef struct {
	ib_col_type_t	type;		/*!< Type of the column */

	ib_col_attr_t	attr;		/*!< Column attributes */

	ib_u32_t	type_len;	/*!< Length of type */

	ib_u16_t	client_type;	/*!< 16 bits of data relevant only to
					the client. InnoDB doesn't care */

	ib_charset_t*	charset;	/*!< Column charset */
} ib_col_meta_t;

/* Note: Must be in sync with trx0trx.h */
/** @enum ib_trx_state_t The transaction state can be queried using the
ib_trx_state() function. The InnoDB deadlock monitor can roll back a
transaction and users should be prepared for this, especially where there
is high contention. The way to determine the state of the transaction is to
query it's state and check. */
typedef enum {
	IB_TRX_NOT_STARTED,		/*!< Has not started yet, the
					transaction has not ben started yet.*/

	IB_TRX_ACTIVE,			/*!< The transaction is currently
					active and needs to be either
					committed or rolled back. */

	IB_TRX_COMMITTED_IN_MEMORY,	/*!< Not committed to disk yet */

	IB_TRX_PREPARED			/*!< Support for 2PC/XA */
} ib_trx_state_t;

/* Note: Must be in sync with trx0trx.h */
/** @enum ib_trx_level_t Transaction isolation levels */
typedef enum {
	IB_TRX_READ_UNCOMMITTED = 0,	/*!< Dirty read: non-locking SELECTs are
					performed so that we do not look at a
					possible earlier version of a record;
					thus they are not 'consistent' reads
					under this isolation level; otherwise
					like level 2 */

	IB_TRX_READ_COMMITTED = 1,	/*!< Somewhat Oracle-like isolation,
					except that in range UPDATE and DELETE
					we must block phantom rows with
					next-key locks; SELECT ... FOR UPDATE
					and ...  LOCK IN SHARE MODE only lock
					the index records, NOT the gaps before
					them, and thus allow free inserting;
					each consistent read reads its own
					snapshot */

	IB_TRX_REPEATABLE_READ = 2,	/*!< All consistent reads in the same
					trx read the same snapshot; full
					next-key locking used in locking reads
					to block insertions into gaps */

	IB_TRX_SERIALIZABLE = 3		/*!< All plain SELECTs are converted to
					LOCK IN SHARE MODE reads */
} ib_trx_level_t;

/** @enum ib_shutdown_t When ib_shutdown() is called InnoDB may take a long
time to shutdown because of background tasks e.g., purging deleted records.
The following flags allow the user to control the shutdown behavior. */
typedef enum {
	IB_SHUTDOWN_NORMAL,		/*!< Normal shutdown, do insert buffer
					merge and purge before complete
					shutdown. */

	IB_SHUTDOWN_NO_IBUFMERGE_PURGE,	/*!< Do not do a purge and index buffer
					merge at shutdown. */

	IB_SHUTDOWN_NO_BUFPOOL_FLUSH	/*!< Same as NO_IBUFMERGE_PURGE
					and in addition do not even flush the
				       	buffer pool to data files. No committed
				       	transactions are lost */
} ib_shutdown_t;

/** Generical InnoDB callback prototype. */
typedef void (*ib_cb_t)(void);

/** The first argument to the InnoDB message logging function. By default
it's set to stderr. You should treat ib_msg_stream_t as a void*, since
it will probably change in the future. */
typedef FILE* ib_msg_stream_t;

/** All log messages are written to this function.It should have the same
behavior as fprintf(3). */
typedef int (*ib_msg_log_t)(ib_msg_stream_t, const char*, ...);

/* Note: This is to make it easy for API users to have type
checking for arguments to our functions. Making it ib_opaque_t
by itself will result in pointer decay resulting in subverting
of the compiler's type checking. */

/** InnoDB tuple handle. This handle can refer to either a cluster index
tuple or a secondary index tuple. There are two types of tuples for each
type of index, making a total of four types of tuple handles. There
is a tuple for reading the entire row contents and another for searching
on the index key. */
typedef struct ib_tpl_struct* ib_tpl_t;

/** InnoDB transaction handle, all database operations need to be covered
by transactions. This handle represents a transaction. The handle can be
created with ib_trx_begin(), you commit your changes with ib_trx_commit()
and undo your changes using ib_trx_rollback(). If the InnoDB deadlock
monitor rolls back the transaction then you need to free the transaction
using the function ib_trx_release(). You can query the state of an InnoDB
transaction by calling ib_trx_state(). */
typedef struct ib_trx_struct* ib_trx_t;

/** InnoDB cursor handle */
typedef struct ib_crsr_struct* ib_crsr_t;

/** InnoDB table schema handle */
typedef struct ib_tbl_sch_struct* ib_tbl_sch_t;

/** InnoDB index schema handle */
typedef struct ib_idx_sch_struct* ib_idx_sch_t;

/** Currently, this is also the number of callback functions in the struct. */
typedef enum {
	IB_SCHEMA_VISITOR_TABLE = 1,
	IB_SCHEMA_VISITOR_TABLE_COL = 2,
	IB_SCHEMA_VISITOR_TABLE_AND_INDEX = 3,
	IB_SCHEMA_VISITOR_TABLE_AND_INDEX_COL = 4
} ib_schema_visitor_version_t;

/** Visit all tables in the InnoDB schem. */
typedef int (*ib_schema_visitor_table_all_t) (
					/*!< return 0 on success, nonzero
					on failure (abort traversal) */
	void*		arg,		/*!< User callback arg */
	const char*	name,		/*!< Table name */
	int		name_len);	/*!< Length of name in bytes */

/** Table visitor */
typedef int (*ib_schema_visitor_table_t) (
					/*!< return 0 on success, nonzero
					on failure (abort traversal) */
	void*		arg,		/*!< User callback arg */
	const char*	name,		/*!< Table name */
	ib_tbl_fmt_t	tbl_fmt,	/*!< Table type */
	ib_ulint_t	page_size,	/*!< Table page size */
	int		n_cols,		/*!< No. of cols defined */
	int		n_indexes);	/*!< No. of indexes defined */

/** Table column visitor */
typedef int (*ib_schema_visitor_table_col_t) (
					/*!< return 0 on success, nonzero
					on failure (abort traversal) */
	void*		arg,		/*!< User callback arg */
	const char*	name,		/*!< Column name */
	ib_col_type_t	col_type,	/*!< Column type */
	ib_ulint_t	len,		/*!< Column len */
	ib_col_attr_t	attr);		/*!< Column attributes */

/** Index visitor */
typedef int (*ib_schema_visitor_index_t) (
					/*!< return 0 on success, nonzero
					on failure (abort traversal) */
	void*		arg,		/*!< User callback arg */
	const char*	name,		/*!< Index name */
	ib_bool_t	clustered,	/*!< True if clustered */
	ib_bool_t	unique,		/*!< True if unique */
	int		n_cols);	/*!< No. of cols defined */

/** Index column visitor */
typedef int (*ib_schema_visitor_index_col_t) (
					/*!< return 0 on success, nonzero
					on failure (abort traversal) */
	void*		arg,		/*!< User callback arg */
	const char*	name,		/*!< Column name */
	ib_ulint_t	prefix_len);	/*!< Prefix length */

/** Callback functions to traverse the schema of a table. */
typedef struct {
	ib_schema_visitor_version_t	version;
					/*!< Visitor version */
	ib_schema_visitor_table_t	table;
					/*!< For travesing table info */
	ib_schema_visitor_table_col_t	table_col;
					/*!< For travesing table column info */
	ib_schema_visitor_index_t	index;
					/*!< For travesing index info */
	ib_schema_visitor_index_col_t	index_col;
					/*!< For travesing index column info */
} ib_schema_visitor_t;

/*************************************************************//**
This function is used to compare two data fields for which the data type
is such that we must use the client code to compare them. */
typedef int (*ib_client_cmp_t)(
					/*!< out: 1, 0, -1, if a is greater,
					equal, less than b, respectively */
	const ib_col_meta_t*
			col_meta,	/*!< in: column meta data */
	const ib_byte_t*p1,		/*!< in: key */
	ib_ulint_t	p1_len,		/*!< in: key length */
	const ib_byte_t*p2,		/*!< in: key */
	ib_ulint_t	p2_len);	/*!< in: key length */

/* This should be the same as univ.i */
/** Represents SQL_NULL length */
#define	IB_SQL_NULL		0xFFFFFFFF
/** The number of system columns in a row. */
#define IB_N_SYS_COLS		3

/** The maximum length of a text column. */
#define MAX_TEXT_LEN		4096

/* MySQL uses 3 byte UTF-8 encoding. */
/** The maximum length of a column name in a table schema. */
#define IB_MAX_COL_NAME_LEN	(64 * 3)

/** The maximum length of a table name (plus database name). */
#define IB_MAX_TABLE_NAME_LEN	(64 * 3)

/*! @def ib_tbl_sch_add_blob_col(s, n)
Add a BLOB column to a table schema.
@param s is the the schema handle
@param n is the column name  */
#define ib_tbl_sch_add_blob_col(s, n) \
	ib_table_schema_add_col(s, n, IB_BLOB, IB_COL_NONE, 0, 0)

/*! @def ib_tbl_sch_add_text_col(s, n)
Add a BLOB column to a table schema.
@param s is the the schema handle
@param n is the column name
Add a TEXT column to a table schema. */
#define ib_tbl_sch_add_text_col(s, n) \
	ib_table_schema_add_col(s, n, IB_VARCHAR, IB_COL_NONE, 0, MAX_TEXT_LEN)

/*! @def ib_tbl_sch_add_varchar_col(s, n, l)
Add a VARCHAR column to a table schema.
@param s is the schema handle
@param n is the column name
@param l the max length of the VARCHAR column
@return DB_SUCCESS or error code */
#define ib_tbl_sch_add_varchar_col(s, n, l) \
	ib_table_schema_add_col(s, n, IB_VARCHAR, IB_COL_NONE, 0, l)

/*! @def ib_tbl_sch_add_u32_col(s, n)
Add an UNSIGNED INT column to a table schema.
@param s is the schema handle
@param n is the column name
@return DB_SUCCESS or error code */
#define ib_tbl_sch_add_u32_col(s, n) \
	ib_table_schema_add_col(s, n, IB_INT, IB_COL_UNSIGNED, 0, 4)

/*! @def ib_tbl_sch_add_u64_col(s, n)
Add an UNSIGNED BIGINT column to a table schema.
@param s is the schema handle
@param n is the column name
@return DB_SUCCESS or error code */
#define ib_tbl_sch_add_u64_col(s, n) \
	ib_table_schema_add_col(s, n, IB_INT, IB_COL_UNSIGNED, 0, 8)

/*! @def ib_tbl_sch_add_u64_notnull_col(s, n)
Add an UNSIGNED BIGINT NOT NULL column to a table schema.
@param s is the schema handle
@param n is the column name
@return DB_SUCCESS or error code */
#define ib_tbl_sch_add_u64_notnull_col(s, n) \
	ib_table_schema_add_col(s, n, IB_INT, \
			        IB_COL_NOT_NULL | IB_COL_UNSIGNED,0,\
				8)
/*! @def ib_cfg_set_int(name, value)
Set an int configuration variable.
@param name is the config variable name
@param value is the integer value of the variable
@return DB_SUCCESS or error code */
#define ib_cfg_set_int(name, value)	ib_cfg_set(name, value)

/*! @def ib_cfg_set_text(name, value)
Set a text configuration variable.
@param name is the config variable name
@param value is the char* value of the variable
@return DB_SUCCESS or error code */
#define ib_cfg_set_text(name, value)	ib_cfg_set(name, value)

/*! @def ib_cfg_set_bool_on(name)
Set a boolean configuration variable to IB_TRUE.
@param name is the config variable name
@return DB_SUCCESS or error code */
#define ib_cfg_set_bool_on(name)	ib_cfg_set(name, IB_TRUE)

/*! @def ib_cfg_set_bool_off(name)
Set a boolean configuration variable to IB_FALSE.
@param name is the config variable name
@return DB_SUCCESS or error code */
#define ib_cfg_set_bool_off(name)	ib_cfg_set(name, IB_FALSE)

/*! @def ib_cfg_set_callback(name, value)
Set a generic ib_cb_t callback function.
@param name is the config variable name
@param value is a pointer to a callback function
@return DB_SUCCESS or error code */
#define ib_cfg_set_callback(name, value) ib_cfg_set(name, value)

/** Callback function to compare InnoDB key columns in an index. */
//extern ib_client_cmp_t	ib_client_compare;

/* Define the Doxygen groups:
   @defgroup init Startup/Shutdown functions
   @defgroup cursor Cursor functions
   @defgroup trx Transaction functions
   @defgroup conf Configuration functions
   @defgroup ddl DDL functions
   @defgroup misc Miscellaneous functions
   @defgroup tuple Tuple functions
   @defgroup sql SQL functions
   @defgroup dml DML functions
*/
/*****************************************************************//**
Return the API version number, the version number format is:
| 16 bits future use | 16 bits current | 16 bits revision | 16 bits age |

- If the library source code has changed at all since the last release,
  then revision will be incremented (`c:r:a' becomes `c:r+1:a').
- If any interfaces have been added, removed, or changed since the last
  update, current will be incremented, and revision will be set to 0.
- If any interfaces have been added (but not changed or removed) since
  the last release, then age will be incremented.
- If any interfaces have been changed or removed since the last release,
  then age will be set to 0.

@ingroup misc
@return	API version number */

/*****************************************************************//**
Start a transaction that's been rolled back. This special function
exists for the case when InnoDB's deadlock detector has rolledack
a transaction. While the transaction has been rolled back the handle
is still valid and can be reused by calling this function. If you
don't want to reuse the transaction handle then you can free the handle
by calling ib_trx_release().

@ingroup trx
@param ib_trx is the transaction to restart
@param ib_trx_level is the transaction isolation level
@return	innobase txn handle */

ib_err_t
ib_trx_start(
/*=========*/
	ib_trx_t	ib_trx,
	ib_trx_level_t	ib_trx_level,
	void*		thd);

/*****************************************************************//**
Begin a transaction. This will allocate a new transaction handle and
put the transaction in the active state.
@ingroup trx
@param ib_trx_level is the transaction isolation level
@return	innobase txn handle */

ib_trx_t
ib_trx_begin(
/*=========*/
	ib_trx_level_t	ib_trx_level) UNIV_NO_IGNORE;

/*****************************************************************//**
Query the transaction's state. This function can be used to check for
the state of the transaction in case it has been rolled back by the
InnoDB deadlock detector. Note that when a transaction is selected as
a victim for rollback, InnoDB will always return an appropriate error
code indicating this. @see DB_DEADLOCK, @see DB_LOCK_TABLE_FULL and
@see DB_LOCK_WAIT_TIMEOUT

@ingroup trx
@param ib_trx is the transaction handle
@return	transaction state */

ib_trx_state_t
ib_trx_state(
/*=========*/
	ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Release the resources of the transaction. If the transaction was
selected as a victim by InnoDB and rolled back then use this function
to free the transaction handle.

@ingroup trx
@param ib_trx is the transaction handle
@return	DB_SUCCESS or err code */

ib_err_t
ib_trx_release(
/*===========*/
	ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Commit a transaction. This function will release the schema latches too.
It will also free the transaction handle.

@ingroup trx
@param ib_trx is thr transaction handle
@return	DB_SUCCESS or err code */

ib_err_t
ib_trx_commit(
/*==========*/
	ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Rollback a transaction. This function will release the schema latches too.
It will also free the transaction handle.

@ingroup trx
@param ib_trx is the transaction handle
@return	DB_SUCCESS or err code */

ib_err_t
ib_trx_rollback(
/*============*/
	ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Set to true if it's a simple select.

@ingroup sql
@param[in, out] ib_crsr is the cursor to update */

void
ib_cursor_set_simple_select(
/*========================*/
	ib_crsr_t	ib_crsr);

/*****************************************************************//**
Open an InnoDB table and return a cursor handle to it.

@ingroup cursor
@param table_id is the id of the table to open
@param ib_trx is the current transaction handle, can be NULL
@param[out] ib_crsr is the new cursor
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_open_table_using_id(
/*==========================*/
	ib_id_u64_t	table_id,
	ib_trx_t	ib_trx,
	ib_crsr_t*	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Open an InnoDB index and return a cursor handle to it.

@ingroup cursor
@param index_id is the id of the index to open
@param ib_trx is the current transaction handlem can be NULL
@param[out] ib_crsr is the new cursor
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_open_index_using_id(
/*==========================*/
	ib_id_u64_t	index_id,
	ib_trx_t	ib_trx,
	ib_crsr_t*	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Open an InnoDB secondary index cursor and return a cursor handle to it.
@return DB_SUCCESS or err code */

ib_err_t
ib_cursor_open_index_using_name(
/*============================*/
	ib_crsr_t	ib_open_crsr,	/*!< in: open/active cursor */
	const char*	index_name,	/*!< in: secondary index name */
	ib_crsr_t*	ib_crsr,	/*!< out,own: InnoDB index cursor */
	int*		idx_type,	/*!< out: index is cluster index */
	ib_id_u64_t*	idx_id);	/*!< out: index id */

/*****************************************************************//**
Open an InnoDB table by name and return a cursor handle to it.

@ingroup cursor
@param name is the table name to open
@param ib_trx is the current transactionm, can be NULL
@param ib_crsr is the new cursor
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_open_table(
/*=================*/
	const char*	name,
	ib_trx_t	ib_trx,
	ib_crsr_t*	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Reset the cursor.
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_reset(
/*============*/
	ib_crsr_t	ib_crsr);	/*!< in/out: InnoDB cursor */

/*****************************************************************//**
Close an InnoDB table and free the cursor.
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_close(
/*============*/
	ib_crsr_t	ib_crsr);	/*!< in,own: InnoDB cursor */

/*****************************************************************//**
Close the table, decrement n_ref_count count.
@return DB_SUCCESS or err code */

ib_err_t
ib_cursor_close_table(
/*==================*/
	ib_crsr_t	ib_crsr);	/*!< in,own: InnoDB cursor */

/*****************************************************************//**
update the cursor with new transactions and also reset the cursor
@return DB_SUCCESS or err code */

ib_err_t
ib_cursor_new_trx(
/*==============*/
	ib_crsr_t	ib_crsr,	/*!< out: InnoDB cursor */
	ib_trx_t	ib_trx);		/*!< in: transaction */

/*****************************************************************//**
Commit the transaction in a cursor
@return DB_SUCCESS or err code */

ib_err_t
ib_cursor_commit_trx(
/*=================*/
	ib_crsr_t	ib_crsr,	/*!< out: InnoDB cursor */
	ib_trx_t	ib_trx);		/*!< in: transaction */

/********************************************************************//**
Open a table using the table name, if found then increment table ref count.
@return table instance if found */

void*
ib_open_table_by_name(
/*==================*/
	const char*	name);	/*!< in: table name to lookup */

/*****************************************************************//**
Insert a row to a table.

@ingroup dml
@param ib_crsr is an open cursor
@param ib_tpl is the tuple to insert
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_insert_row(
/*=================*/
	ib_crsr_t	ib_crsr,
	const ib_tpl_t	ib_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Update a row in a table.

@ingroup dml
@param ib_crsr is the cursor instance
@param ib_old_tpl is the old tuple in the table
@param ib_new_tpl is the new tuple with the updated values
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_update_row(
/*=================*/
	ib_crsr_t	ib_crsr,
	const ib_tpl_t	ib_old_tpl,
	const ib_tpl_t	ib_new_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Delete a row in a table.

@ingroup dml
@param ib_crsr is the cursor instance
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_delete_row(
/*=================*/
	ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Read current row.

@ingroup dml
@param ib_crsr is the cursor instance
@param[out] ib_tpl is the tuple to read the column values
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_read_row(
/*===============*/
	ib_crsr_t	ib_crsr,
	ib_tpl_t	ib_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Move cursor to the first record in the table.

@ingroup cursor
@param ib_crsr is the cursor instance
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_first(
/*============*/
	ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Move cursor to the last record in the table.

@ingroup cursor
@param ib_crsr is the cursor instance
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_last(
/*===========*/
	ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Search for key.

@ingroup cursor
@param ib_crsr is an open cursor instance
@param ib_tpl is a key to search for
@param ib_srch_mode is the search mode
@param[out] result is -1, 0 or 1 depending on tuple eq or gt than
       the current row
@return	DB_SUCCESS or err code */

ib_err_t
ib_cursor_moveto(
/*=============*/
	ib_crsr_t	ib_crsr,
	ib_tpl_t	ib_tpl,
	ib_srch_mode_t	ib_srch_mode);

/*****************************************************************//**
Attach the cursor to the transaction. The cursor must not already be
attached to another transaction.

@ingroup cursor
@param ib_crsr is the cursor instance
@param ib_trx is the transaction to attach to the cursor */

void
ib_cursor_attach_trx(
/*=================*/
	ib_crsr_t	ib_crsr,
	ib_trx_t	ib_trx);

/*****************************************************************//**
Set the client comparison function for BLOBs and client types.

@ingroup misc
@param client_cmp_func is the index key compare callback function */

void
ib_set_client_compare(
/*==================*/
	ib_client_cmp_t	client_cmp_func);

/*****************************************************************//**
Set the match mode for ib_cursor_move().

@ingroup cursor
@param ib_crsr is the cursor instance
@param match_mode is the match mode to set */

void
ib_cursor_set_match_mode(
/*=====================*/
	ib_crsr_t	ib_crsr,
	ib_match_mode_t	match_mode);

/*****************************************************************//**
Set a column of the tuple. Make a copy using the tuple's heap.

@ingroup dml
@param ib_tpl is the tuple instance
@param col_no is the column index in the tuple
@param src is the data value to set
@param len is the data value (src) length in bytes
@return	DB_SUCCESS or error code */

ib_err_t
ib_col_set_value(
/*=============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	col_no,
	const void*	src,
	ib_ulint_t	len) UNIV_NO_IGNORE;

/*****************************************************************//**
Get the size of the data available in the column the tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@return	bytes avail or IB_SQL_NULL */

ib_ulint_t
ib_col_get_len(
/*===========*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i) UNIV_NO_IGNORE;

/*****************************************************************//**
Copy a column value from the tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] dst is where the data will be copied
@param len is the maximum number of bytes that can be copied to dst
@return	bytes copied or IB_SQL_NULL */

ib_ulint_t
ib_col_copy_value(
/*==============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	void*		dst,
	ib_ulint_t	len);

/*************************************************************//**
Read a signed int 8 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_i8(
/*=============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i8_t*	ival) UNIV_NO_IGNORE;

/*************************************************************//**
Read an unsigned int 8 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_u8(
/*=============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u8_t*	ival) UNIV_NO_IGNORE;

/*************************************************************//**
Read a signed int 16 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_i16(
/*==============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i16_t*	ival) UNIV_NO_IGNORE;

/*************************************************************//**
Read an unsigned int 16 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_u16(
/*==============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u16_t*	ival) UNIV_NO_IGNORE;

/*************************************************************//**
Read a signed int 32 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_i32(
/*==============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i32_t*	ival) UNIV_NO_IGNORE;

/*************************************************************//**
Read an unsigned int 32 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_u32(
/*==============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u32_t*	ival) UNIV_NO_IGNORE;

/*************************************************************//**
Read a signed int 64 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_i64(
/*==============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i64_t*	ival) UNIV_NO_IGNORE;

/*************************************************************//**
Read an unsigned int 64 bit column from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ival is the integer value
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_u64(
/*==============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u64_t*	ival) UNIV_NO_IGNORE;

/*****************************************************************//**
Get a column value pointer from the tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@return	NULL or pointer to buffer */

const void*
ib_col_get_value(
/*=============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i) UNIV_NO_IGNORE;

/*****************************************************************//**
Get a column type, length and attributes from the tuple.

@ingroup dml
@param ib_tpl is the tuple instance
@param i is the index (ordinal position) of the column within the tuple
@param[out] ib_col_meta the column meta data
@return	len of column data */

ib_ulint_t
ib_col_get_meta(
/*============*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_col_meta_t*	ib_col_meta);

/*****************************************************************//**
"Clear" or reset an InnoDB tuple. We free the heap and recreate the tuple.

@ingroup tuple
@param ib_tpl is the tuple to be freed
@return	new tuple, or NULL */

ib_tpl_t
ib_tuple_clear(
/*============*/
	ib_tpl_t	ib_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Create a new cluster key search tuple and copy the contents of  the
secondary index key tuple columns that refer to the cluster index record
to the cluster key. It does a deep copy of the column data.

@ingroup tuple
@param ib_crsr is a cursor opened on a secondary index
@param[out] ib_dst_tpl is the tuple where the key data will be copied
@param ib_src_tpl is the source secondary index tuple to copy from
@return	DB_SUCCESS or error code */

ib_err_t
ib_tuple_get_cluster_key(
/*=====================*/
	ib_crsr_t	ib_crsr,
	ib_tpl_t*	ib_dst_tpl,
	const ib_tpl_t	ib_src_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Copy the contents of  source tuple to destination tuple. The tuples
must be of the same type and belong to the same table/index.

@ingroup tuple
@param ib_dst_tpl is the destination tuple
@param ib_src_tpl is the source tuple
@return	DB_SUCCESS or error code */

ib_err_t
ib_tuple_copy(
/*==========*/
	ib_tpl_t	ib_dst_tpl,
	const ib_tpl_t	ib_src_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Create an InnoDB tuple used for index/table search.

@ingroup tuple
@param ib_crsr is the cursor instance
@return tuple for current index */

ib_tpl_t
ib_sec_search_tuple_create(
/*=======================*/
	ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Create an InnoDB tuple used for index/table search.

@ingroup tuple
@param ib_crsr is the cursor instance
@return	tuple for current index */

ib_tpl_t
ib_sec_read_tuple_create(
/*=====================*/
	ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Create an InnoDB tuple used for table key operations.

@ingroup tuple
@param ib_crsr is the cursor instance
@return	tuple for current table */

ib_tpl_t
ib_clust_search_tuple_create(
/*=========================*/
	ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Create an InnoDB tuple for table row operations.

@ingroup tuple
@param ib_crsr is the cursor instance
@return	tTuple for current table */

ib_tpl_t
ib_clust_read_tuple_create(
/*=======================*/
	ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Return the number of user columns in the tuple definition.

@ingroup tuple
@param ib_tpl is a tuple
@return	number of user columns */

ib_ulint_t
ib_tuple_get_n_user_cols(
/*=====================*/
	const ib_tpl_t	ib_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Return the number of columns in the tuple definition.

@ingroup tuple
@param ib_tpl is a tuple
@return	number of columns */

ib_ulint_t
ib_tuple_get_n_cols(
/*================*/
	const ib_tpl_t	ib_tpl) UNIV_NO_IGNORE;

/*****************************************************************//**
Destroy an InnoDB tuple.

@ingroup tuple
@param ib_tpl is the tuple instance to delete */

void
ib_tuple_delete(
/*============*/
	ib_tpl_t	ib_tpl);

/*****************************************************************//**
Truncate a table. The cursor handle will be closed and set to NULL
on success.

@ingroup ddl
@param[out] ib_crsr is the cursor for table to truncate
@param[out] table_id is the new table id
@return	DB_SUCCESS or error code */

ib_err_t
ib_cursor_truncate(
/*===============*/
	ib_crsr_t*	ib_crsr,
	ib_id_u64_t*	table_id) UNIV_NO_IGNORE;

/*****************************************************************//**
Get a table id.

@ingroup ddl
@param table_name is the name of the table to lookup
@param[out] table_id is the new table id if found
@return	DB_SUCCESS if found */

ib_err_t
ib_table_get_id(
/*============*/
	const char*	table_name,
	ib_id_u64_t*	table_id) UNIV_NO_IGNORE;

/*****************************************************************//**
Get an index id.

@ingroup ddl
@param table_name is the name of the table that contains the index
@param index_name is the name of the index to lookup
@param[out] index_id contains the index id if found
@return	DB_SUCCESS if found */

ib_err_t
ib_index_get_id(
/*============*/
	const char*	table_name,
	const char*	index_name,
	ib_id_u64_t*	index_id) UNIV_NO_IGNORE;

/*****************************************************************//**
Check if cursor is positioned.

@ingroup cursor
@param ib_crsr is the cursor instance to check
@return	IB_TRUE if positioned */

ib_bool_t
ib_cursor_is_positioned(
/*====================*/
	const ib_crsr_t	ib_crsr) UNIV_NO_IGNORE;

/*****************************************************************//**
Latches the data dictionary in shared mode.

@ingroup ddl
@param ib_trx is the transaction instance
@return	DB_SUCCESS or error code */

ib_err_t
ib_schema_lock_shared(
/*==================*/
	ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Latches the data dictionary in exclusive mode.

@ingroup ddl
@param ib_trx is the transaction instance
@return	DB_SUCCESS or error code */

ib_err_t
ib_schema_lock_exclusive(
/*======================*/
	ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Checks if the data dictionary is latched in exclusive mode by a
user transaction.

@ingroup ddl
@param ib_trx is a transaction instance
@return	TRUE if exclusive latch */

ib_bool_t
ib_schema_lock_is_exclusive(
/*========================*/
	const ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Checks if the data dictionary is latched in shared mode.
@param ib_trx is a transaction instance
@return	TRUE if shared latch */

ib_bool_t
ib_schema_lock_is_shared(
/*=====================*/
	const ib_trx_t	ib_trx) UNIV_NO_IGNORE;

/*****************************************************************//**
Lock an InnoDB cursor/table.

@ingroup trx
@param ib_crsr is the cursor instance
@param ib_lck_mode is the lock mode
@return	DB_SUCCESS or error code */

ib_err_t
ib_cursor_lock(
/*===========*/
	ib_crsr_t	ib_crsr,
	ib_lck_mode_t	ib_lck_mode) UNIV_NO_IGNORE;

/*****************************************************************//**
Set the Lock an InnoDB table using the table id.

@ingroup trx
@param ib_trx is a transaction instance
@param table_id is the table to lock
@param ib_lck_mode is the lock mode
@return	DB_SUCCESS or error code */

ib_err_t
ib_table_lock(
/*===========*/
	ib_trx_t	ib_trx,
	ib_id_u64_t	table_id,
	ib_lck_mode_t	ib_lck_mode) UNIV_NO_IGNORE;

/*****************************************************************//**
Set the Lock mode of the cursor.

@ingroup trx
@param ib_crsr is the cursor instance for which we want to set the lock mode
@param  ib_lck_mode is the lock mode
@return	DB_SUCCESS or error code */

ib_err_t
ib_cursor_set_lock_mode(
/*====================*/
	ib_crsr_t	ib_crsr,
	ib_lck_mode_t	ib_lck_mode) UNIV_NO_IGNORE;

/*****************************************************************//**
Set need to access clustered index record flag.

@ingroup dml
@param ib_crsr is the cursor instance for which we want to set the flag */

void
ib_cursor_set_cluster_access(
/*=========================*/
	ib_crsr_t	ib_crsr);

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_i8(
/*==============*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i8_t		val) UNIV_NO_IGNORE;

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_i16(
/*=================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i16_t	val) UNIV_NO_IGNORE;

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_i32(
/*===============*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i32_t	val) UNIV_NO_IGNORE;

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_i64(
/*===============*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i64_t	val) UNIV_NO_IGNORE;

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_u8(
/*==============*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u8_t		val) UNIV_NO_IGNORE;

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_u16(
/*===============*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u16_t	val) UNIV_NO_IGNORE;

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_u32(
/*=================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u32_t	val) UNIV_NO_IGNORE;

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCESS or error */

ib_err_t
ib_tuple_write_u64(
/*===============*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u64_t	val) UNIV_NO_IGNORE;

/*****************************************************************//**
Inform the cursor that it's the start of an SQL statement.

@ingroup cursor
@param ib_crsr is the cursor instance */

void
ib_cursor_stmt_begin(
/*=================*/
	ib_crsr_t	ib_crsr);

/*****************************************************************//**
Write a double value to a column.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_write_double(
/*==================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	double		val);

/*************************************************************//**
Read a double column value from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple to read from
@param col_no is the column number to read
@param[out] dval is where the value is copied
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_double(
/*=================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	col_no,
	double*		dval);

/*****************************************************************//**
Write a float value to a column.

@ingroup dml
@param[in,out] ib_tpl is the tuple to write to
@param col_no is the column number to update
@param val is the value to write
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_write_float(
/*=================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	float		val);

/*************************************************************//**
Read a float value from an InnoDB tuple.

@ingroup dml
@param ib_tpl is the tuple to read from
@param col_no is the column number to read
@param[out] fval is where the value is copied
@return	DB_SUCCESS or error */

ib_err_t
ib_tuple_read_float(
/*================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	col_no,
	float*		fval);

/*************************************************************//**
Set the message logging function.

@ingroup misc
@param ib_msg_log is the message logging function
@param ib_msg_stream is the message stream, this is the first argument
       to the loggingfunction */

void
ib_logger_set(
/*==========*/
	ib_msg_log_t	ib_msg_log,
	ib_msg_stream_t	ib_msg_stream);

/*************************************************************//**
Convert an error number to a human readable text message. The
returned string is static and should not be freed or modified.

@ingroup misc
@param db_errno is the error number
@return	string, describing the error */

const char*
ib_strerror(
/*========*/
	ib_err_t	db_errno);

/*******************************************************************//**
Get the value of an INT status variable. 

@ingroup misc
@param name is the status variable name
@param[out] dst is where the output value is copied if name is found
@return	DB_SUCCESS if found and type is INT,
	DB_DATA_MISMATCH if found but type is not INT,
	DB_NOT_FOUND otherwise. */

ib_err_t
ib_status_get_i64(
/*==============*/
	const char*	name,
	ib_i64_t*	dst);

/*****************************************************************//**
Get a column type, length and attributes from the tuple.
@return len of column data */

const char*
ib_col_get_name(
/*============*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	ib_ulint_t	i);		/*!< in: column index in tuple */


/*****************************************************************//**
Truncate a table.
@return DB_SUCCESS or error code */

ib_err_t
ib_table_truncate(
/*==============*/
	const char*	table_name,	/*!< in: table name */
	ib_id_u64_t*	table_id);	/*!< out: new table id */

/*****************************************************************//**
Frees a possible InnoDB trx object associated with the current THD.
@return 0 or error number */

ib_err_t
ib_close_thd(
/*=========*/
	void*		thd);	/*!< in: handle to the MySQL thread of the user
				whose resources should be free'd */

/*****************************************************************//**
Check whether binlog is enabled (innodb_direct_access_enable_binlog
is set to TRUE)
@return TRUE if enabled */

int
ib_is_binlog_enabled();
/*==================*/
/*****************************************************************//**
Check whether the table name conforms to our requirements. Currently
we only do a simple check for the presence of a '/'.
@return DB_SUCCESS or err code */

ib_err_t
ib_table_name_check(
/*================*/
	const char*	name);		/*!< in: table name to check */
/*****************************************************************//**
Return isolation configuration set by "innodb_api_trx_level"
@return trx isolation level*/

ib_trx_state_t
ib_cfg_trx_level();
/*==============*/

/* API_END_INCLUDE */

#endif /* INNOBASE_API_H */
