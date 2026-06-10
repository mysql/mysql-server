/*****************************************************************************

Copyright (c) 1996, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/dict0boot.h
 Data dictionary creation and booting

 Created 4/18/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict0boot_h
#define dict0boot_h

#include "univ.i"

#include "buf0buf.h"
#include "dict0dict.h"
#include "fsp0fsp.h"
#include "mtr0log.h"
#include "mtr0mtr.h"
#include "ut0byte.h"

typedef byte dict_hdr_t;

/** Gets a pointer to the dictionary header and x-latches its page.
 @return pointer to the dictionary header, page x-latched */
dict_hdr_t *dict_hdr_get(mtr_t *mtr); /*!< in: mtr */

/** Returns a new table, index, or space id.
@param[out] table_id Table id (not assigned if null)
@param[out] index_id Index id (not assigned if null)
@param[out] space_id Space id (not assigned if null)
@param[in] table Table
@param[in] disable_redo If true and table object is null then disable-redo */
void dict_hdr_get_new_id(table_id_t *table_id, space_index_t *index_id,
                         space_id_t *space_id, const dict_table_t *table,
                         bool disable_redo);
#ifndef UNIV_HOTBACKUP
/** Returns a new row id.
 @return the new id */
static inline row_id_t dict_sys_get_new_row_id();
/** Reads a row id from a record or other 6-byte stored form.
 @return row id */
static inline row_id_t dict_sys_read_row_id(
    const byte *field); /*!< in: record field */

/** Writes a row id to a record or other 6-byte stored form.
@param[in]      field   record field
@param[in]      row_id  row id */
static inline void dict_sys_write_row_id(byte *field, row_id_t row_id);

#endif /* !UNIV_HOTBACKUP */

/** Initializes the data dictionary memory structures when the database is
 started. This function is also called when the data dictionary is created.
 @return DB_SUCCESS or error code. */
[[nodiscard]] dberr_t dict_boot();

/** Creates and initializes the data dictionary at the server bootstrap.
 @return DB_SUCCESS or error code. */
[[nodiscard]] dberr_t dict_create();

/** the ids for tables etc. start from this number, except for basic system
 tables and their above defined indexes; ibuf tables and indexes are assigned
 as the id the number DICT_IBUF_ID_MIN plus the space id */
constexpr uint32_t DICT_HDR_FIRST_ID = 10;

/* The offset of the dictionary header on the page */
constexpr uint32_t DICT_HDR = FSEG_PAGE_DATA;

/*-------------------------------------------------------------*/
/* Dictionary header offsets */
/** The latest assigned row id */
constexpr uint32_t DICT_HDR_ROW_ID = 0;
/** The latest assigned table id */
constexpr uint32_t DICT_HDR_TABLE_ID = 8;
/** The latest assigned index id */
constexpr uint32_t DICT_HDR_INDEX_ID = 16;
/** The latest assigned space id,or 0*/
constexpr uint32_t DICT_HDR_MAX_SPACE_ID = 24;
/** Obsolete,always DICT_HDR_FIRST_ID*/
constexpr uint32_t DICT_HDR_MIX_ID_LOW = 28;

/* Segment header for the tablespace segment into which the dictionary header is
 created */
constexpr uint32_t DICT_HDR_FSEG_HEADER = 56;
/*-------------------------------------------------------------*/

/* When a row id which is zero modulo this number (which must be a power of
two) is assigned, the field DICT_HDR_ROW_ID on the dictionary header page is
updated */
constexpr uint32_t DICT_HDR_ROW_ID_WRITE_MARGIN = 256;

#ifndef UNIV_HOTBACKUP
#include "dict0boot.ic"
#endif /* !UNIV_HOTBACKUP */

#endif
