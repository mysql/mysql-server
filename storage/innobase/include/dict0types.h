/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file include/dict0types.h
Data dictionary global types

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0types_h
#define dict0types_h

#include "ibuf0types.h" /* IBUF_SPACE_ID */
#include "ut0mutex.h"
#include "rem0types.h"

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

/** Table or partition identifier (unique within an InnoDB instance). */
typedef ib_id_t		table_id_t;
/** Index identifier (unique within a tablespace). */
typedef ib_id_t		space_index_t;

/** Globally unique index identifier */
class index_id_t {
public:
	/** Constructor.
	@param[in]	space_id	Tablespace identifier
	@param[in]	index_id	Index identifier */
	index_id_t(space_id_t space_id, space_index_t index_id) :
		m_space_id(space_id),
		m_index_id(index_id)
	{}

	/** Compare this to another index identifier.
	@param other	the other index identifier
	@return whether this is less than other */
	bool operator<(const index_id_t& other) const
	{
		return(m_space_id < other.m_space_id
		       || (m_space_id == other.m_space_id
			   && m_index_id < other.m_index_id));
	}
	/** Compare this to another index identifier.
	@param other	the other index identifier
	@return whether the identifiers are equal */
	bool operator==(const index_id_t& other) const
	{
		return(m_space_id == other.m_space_id
		       && m_index_id == other.m_index_id);
	}

	/** Convert an index_id to a 64 bit integer.
	@return a 64 bit integer */
	uint64_t
	conv_to_int() const
	{
		ut_ad((m_index_id & 0xFFFFFFFF00000000ULL) == 0);

		return(static_cast<uint64_t>(m_space_id) << 32 | m_index_id);
	}

	/** Check if the index belongs to the insert buffer.
	@return true if the index belongs to the insert buffer */
	bool
	is_ibuf() const
	{
		return(m_space_id == IBUF_SPACE_ID
		       && m_index_id == DICT_IBUF_ID_MIN + IBUF_SPACE_ID);
	}

	/** Tablespace identifier */
	space_id_t	m_space_id;
	/** Index identifier within the tablespace */
	space_index_t	m_index_id;
};

/** Display an index identifier.
@param[in,out]	out	the output stream
@param[in]	id	index identifier
@return the output stream */
inline
std::ostream&
operator<<(std::ostream& out, const index_id_t& id)
{
	return(out
	       << "[space=" << id.m_space_id
	       << ",index=" << id.m_index_id << "]");
}

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

#ifndef UNIV_HOTBACKUP
typedef ib_mutex_t DictSysMutex;
#endif /* !UNIV_HOTBACKUP */

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
