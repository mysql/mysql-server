/*****************************************************************************

Copyright (c) 1996, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/dict0types.h
Data dictionary global types

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0types_h
#define dict0types_h

#include <ut0mutex.h>

struct dict_sys_t;
struct dict_col_t;
struct dict_field_t;
struct dict_index_t;
struct dict_table_t;
struct dict_foreign_t;
struct dict_v_col_t;

struct ind_node_t;
struct tab_node_t;
struct dict_add_v_col_t;

/* Space id and page no where the dictionary header resides */
#define	DICT_HDR_SPACE		0	/* the SYSTEM tablespace */
#define	DICT_HDR_PAGE_NO	FSP_DICT_HDR_PAGE_NO

/* The ibuf table and indexes's ID are assigned as the number
DICT_IBUF_ID_MIN plus the space id */
#define DICT_IBUF_ID_MIN	0xFFFFFFFF00000000ULL

typedef ib_id_t		table_id_t;
typedef ib_id_t		index_id_t;

/** Error to ignore when we load table dictionary into memory. However,
the table and index will be marked as "corrupted", and caller will
be responsible to deal with corrupted table or index.
Note: please define the IGNORE_ERR_* as bits, so their value can
be or-ed together */
enum dict_err_ignore_t {
	DICT_ERR_IGNORE_NONE = 0,	/*!< no error to ignore */
	DICT_ERR_IGNORE_INDEX_ROOT = 1,	/*!< ignore error if index root
					page is FIL_NULL or incorrect value */
	DICT_ERR_IGNORE_CORRUPT = 2,	/*!< skip corrupted indexes */
	DICT_ERR_IGNORE_FK_NOKEY = 4,	/*!< ignore error if any foreign
					key is missing */
	DICT_ERR_IGNORE_RECOVER_LOCK = 8,
					/*!< Used when recovering table locks
					for resurrected transactions.
					Silently load a missing
					tablespace, and do not load
					incomplete index definitions. */
	DICT_ERR_IGNORE_ALL = 0xFFFF	/*!< ignore all errors */
};

/** Quiescing states for flushing tables to disk. */
enum ib_quiesce_t {
	QUIESCE_NONE,
	QUIESCE_START,			/*!< Initialise, prepare to start */
	QUIESCE_COMPLETE		/*!< All done */
};

#ifndef UNIV_INNOCHECKSUM
typedef ib_mutex_t DictSysMutex;
#endif /* !UNIV_INNOCHECKSUM */

/** Prefix for tmp tables, adopted from sql/table.h */
#define TEMP_FILE_PREFIX		"#sql"
#define TEMP_FILE_PREFIX_LENGTH		4
#define TEMP_FILE_PREFIX_INNODB		"#sql-ib"

#define TEMP_TABLE_PREFIX                "#sql"
#define TEMP_TABLE_PATH_PREFIX           "/" TEMP_TABLE_PREFIX

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
extern uint		ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** Shift for spatial status */
#define SPATIAL_STATUS_SHIFT	12

/** Mask to encode/decode spatial status. */
#define SPATIAL_STATUS_MASK	(3 << SPATIAL_STATUS_SHIFT)

#if SPATIAL_STATUS_MASK < REC_VERSION_56_MAX_INDEX_COL_LEN
# error SPATIAL_STATUS_MASK < REC_VERSION_56_MAX_INDEX_COL_LEN
#endif

/** whether a col is used in spatial index or regular index
Note: the spatial status is part of persistent undo log,
so we should not modify the values in MySQL 5.7 */
enum spatial_status_t {
	/* Unkown status (undo format in 5.7.9) */
	SPATIAL_UNKNOWN = 0,

	/** Not used in gis index. */
	SPATIAL_NONE	= 1,

	/** Used in both spatial index and regular index. */
	SPATIAL_MIXED	= 2,

	/** Only used in spatial index. */
	SPATIAL_ONLY	= 3
};

#endif
