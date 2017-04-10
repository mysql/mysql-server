/*****************************************************************************

Copyright (c) 1994, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/rem0types.h
Record manager global types

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#ifndef rem0types_h
#define rem0types_h

/* We define the physical record simply as an array of bytes */
typedef byte	rec_t;

/* Maximum values for various fields (for non-blob tuples) */
#define REC_MAX_N_FIELDS	(1024 - 1)
#define REC_MAX_HEAP_NO		(2 * 8192 - 1)
#define REC_MAX_N_OWNED		(16 - 1)

/* Maximum number of user defined fields/columns. The reserved columns
are the ones InnoDB adds internally: DB_ROW_ID, DB_TRX_ID, DB_ROLL_PTR.
We need "* 2" because mlog_parse_index() creates a dummy table object
possibly, with some of the system columns in it, and then adds the 3
system columns (again) using dict_table_add_system_columns(). The problem
is that mlog_parse_index() cannot recognize the system columns by
just having n_fields, n_uniq and the lengths of the columns. */
#define REC_MAX_N_USER_FIELDS	(REC_MAX_N_FIELDS - DATA_N_SYS_COLS * 2)

/* REC_ANTELOPE_MAX_INDEX_COL_LEN is measured in bytes and is the maximum
indexed field length (or indexed prefix length) for indexes on tables of
ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT format.
Before we support UTF-8 encodings with mbmaxlen = 4, a UTF-8 character
may take at most 3 bytes.  So the limit was set to 3*256, so that one
can create a column prefix index on 256 characters of a TEXT or VARCHAR
column also in the UTF-8 charset.
This constant MUST NOT BE CHANGED, or the compatibility of InnoDB data
files would be at risk! */
#define REC_ANTELOPE_MAX_INDEX_COL_LEN		768

/** Maximum indexed field length for tables that have atomic BLOBs.
This (3072) is the maximum index row length allowed, so we cannot create index
prefix column longer than that. */
#define REC_VERSION_56_MAX_INDEX_COL_LEN	3072

/** Innodb row types are a subset of the MySQL global enum row_type.
They are made into their own enum so that switch statements can account
for each of them. */
enum rec_format_enum {
	REC_FORMAT_REDUNDANT	= 0,	/*!< REDUNDANT row format */
	REC_FORMAT_COMPACT	= 1,	/*!< COMPACT row format */
	REC_FORMAT_COMPRESSED	= 2,	/*!< COMPRESSED row format */
	REC_FORMAT_DYNAMIC	= 3	/*!< DYNAMIC row format */
};
typedef enum rec_format_enum rec_format_t;

#endif
