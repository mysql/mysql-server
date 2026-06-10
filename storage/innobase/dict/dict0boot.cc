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

/** @file dict/dict0boot.cc
 Data dictionary creation and booting

 Created 4/18/1996 Heikki Tuuri
 *******************************************************/

#include "dict0boot.h"
#include "btr0btr.h"
#include "buf0flu.h"
#include "dict0crea.h"
#include "dict0load.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0recv.h"

#include "dict0dd.h"
#include "os0file.h"
#include "srv0srv.h"
#include "trx0trx.h"

/** Gets a pointer to the dictionary header and x-latches its page.
 @return pointer to the dictionary header, page x-latched */
dict_hdr_t *dict_hdr_get(mtr_t *mtr) /*!< in: mtr */
{
  buf_block_t *block;
  dict_hdr_t *header;

  block = buf_page_get(page_id_t(DICT_HDR_SPACE, DICT_HDR_PAGE_NO),
                       univ_page_size, RW_X_LATCH, UT_LOCATION_HERE, mtr);
  header = DICT_HDR + buf_block_get_frame(block);

  buf_block_dbg_add_level(block, SYNC_DICT_HEADER);

  return (header);
}

/** Returns a new table, index, or space id.
@param[out] table_id Table id (not assigned if null)
@param[out] index_id Index id (not assigned if null)
@param[out] space_id Space id (not assigned if null)
@param[in] table Table
@param[in] disable_redo If true and table object is null then disable-redo */
void dict_hdr_get_new_id(table_id_t *table_id, space_index_t *index_id,
                         space_id_t *space_id, const dict_table_t *table,
                         bool disable_redo) {
  dict_hdr_t *dict_hdr;
  ib_id_t id;
  mtr_t mtr;

  mtr_start(&mtr);

  if (table) {
    dict_disable_redo_if_temporary(table, &mtr);
  } else if (disable_redo) {
    /* In non-read-only mode we need to ensure that space-id header
    page is written to disk else if page is removed from buffer
    cache and re-loaded it would assign temporary tablespace id
    to another tablespace.
    This is not a case with read-only mode as there is no new object
    that is created except temporary tablespace. */
    mtr_set_log_mode(&mtr,
                     (srv_read_only_mode ? MTR_LOG_NONE : MTR_LOG_NO_REDO));
  }

  /* Server started and let's say space-id = x
  - table created with file-per-table
  - space-id = x + 1
  - crash
  Case 1: If it was redo logged then we know that it will be
          restored to x + 1
  Case 2: if not redo-logged
          Header will have the old space-id = x
          This is OK because on restart there is no object with
          space id = x + 1
  Case 3:
          space-id = x (on start)
          space-id = x+1 (temp-table allocation) - no redo logging
          space-id = x+2 (non-temp-table allocation), this gets
                     redo logged.
          If there is a crash there will be only 2 entries
          x (original) and x+2 (new) and disk hdr will be updated
          to reflect x + 2 entry.
          We cannot allocate the same space id to different objects. */
  dict_hdr = dict_hdr_get(&mtr);

  if (table_id) {
    id = mach_read_from_8(dict_hdr + DICT_HDR_TABLE_ID);
    id++;

    /* This means we are running out of table_ids and
    entering into reserved range of table_ids for SDI
    tables */
    if (id >= dict_sdi_get_table_id(0)) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_160)
          << "InnoDB is running out of table_ids"
          << " Please dump and reload the database";
    }

    mlog_write_ull(dict_hdr + DICT_HDR_TABLE_ID, id, &mtr);
    *table_id = id;
  }

  if (index_id) {
    id = mach_read_from_8(dict_hdr + DICT_HDR_INDEX_ID);
    id++;
    DBUG_EXECUTE_IF(
        "simulate_index_id_exceed_uint32",
        constexpr ib_id_t max_uint32 = 0xFFFFFFFF;
        if (id < max_uint32) { id = max_uint32; });
    mlog_write_ull(dict_hdr + DICT_HDR_INDEX_ID, id, &mtr);
    *index_id = id;
  }

  if (space_id) {
    *space_id =
        mtr_read_ulint(dict_hdr + DICT_HDR_MAX_SPACE_ID, MLOG_4BYTES, &mtr);
    if (fil_assign_new_space_id(space_id)) {
      mlog_write_ulint(dict_hdr + DICT_HDR_MAX_SPACE_ID, *space_id, MLOG_4BYTES,
                       &mtr);
    }
  }

  mtr_commit(&mtr);
}

/** Writes the current value of the row id counter to the dictionary header file
 page. */
void dict_sys_t::dict_hdr_flush_row_id() {
  dict_hdr_t *dict_hdr;
  row_id_t id;
  mtr_t mtr;

  ut_ad(dict_sys_mutex_own());

  id = dict_sys->row_id.load();

  mtr_start(&mtr);

  dict_hdr = dict_hdr_get(&mtr);

  mlog_write_ull(dict_hdr + DICT_HDR_ROW_ID, id, &mtr);

  mtr_commit(&mtr);
}

/** Creates the file page for the dictionary header. This function is
 called only at the database creation.
 @return true if succeed */
static bool dict_hdr_create(mtr_t *mtr) /*!< in: mtr */
{
  buf_block_t *block;
  dict_hdr_t *dict_header;

  ut_ad(mtr);

  /* Create the dictionary header file block in a new, allocated file
  segment in the system tablespace */
  block = fseg_create(DICT_HDR_SPACE, 0, DICT_HDR + DICT_HDR_FSEG_HEADER, mtr);

  ut_a(DICT_HDR_PAGE_NO == block->page.id.page_no());

  dict_header = dict_hdr_get(mtr);

  /* Start counting row, table, index, and tree ids from 0 */
  mlog_write_ull(dict_header + DICT_HDR_ROW_ID, 0, mtr);

  mlog_write_ull(dict_header + DICT_HDR_TABLE_ID, DICT_MAX_DD_TABLES, mtr);

  mlog_write_ull(dict_header + DICT_HDR_INDEX_ID, 0, mtr);

  mlog_write_ulint(dict_header + DICT_HDR_MAX_SPACE_ID, 0, MLOG_4BYTES, mtr);

  /* Obsolete, but we must initialize it anyway. */
  mlog_write_ulint(dict_header + DICT_HDR_MIX_ID_LOW, 0, MLOG_4BYTES, mtr);

  return true;
}

/** Initializes the data dictionary memory structures when the database is
 started. This function is also called when the data dictionary is created.
 @return DB_SUCCESS or error code. */
dberr_t dict_boot() {
  dict_hdr_t *dict_hdr;
  mtr_t mtr;
  dberr_t err = DB_SUCCESS;

  mtr_start(&mtr);

  /* Create the hash tables etc. */
  dict_init();

  /* Get the dictionary header */
  dict_hdr = dict_hdr_get(&mtr);

  /* Because we only write new row ids to disk-based data structure
  (dictionary header) when it is divisible by
  DICT_HDR_ROW_ID_WRITE_MARGIN, in recovery we will not recover
  the latest value of the row id counter. Therefore we advance
  the counter at the database startup to avoid overlapping values.
  Note that when a user after database startup first time asks for
  a new row id, then because the counter is now divisible by
  ..._MARGIN, it will immediately be updated to the disk-based
  header. */

  dict_sys->row_id.store(
      DICT_HDR_ROW_ID_WRITE_MARGIN +
      ut_uint64_align_up(mach_read_from_8(dict_hdr + DICT_HDR_ROW_ID),
                         DICT_HDR_ROW_ID_WRITE_MARGIN));

  mtr_commit(&mtr);

  /*-------------------------*/

  /* Initialize the insert buffer table, table buffer and indexes */

  ibuf_init_at_db_start();

  if (srv_force_recovery != SRV_FORCE_NO_LOG_REDO && srv_read_only_mode &&
      !ibuf_is_empty()) {
    ib::error(ER_IB_MSG_161) << "Change buffer must be empty when"
                                " --innodb-read-only is set!";

    err = DB_ERROR;
  }

  return (err);
}

/** Creates and initializes the data dictionary at the server bootstrap.
 @return DB_SUCCESS or error code. */
dberr_t dict_create() {
  mtr_t mtr;

  mtr_start(&mtr);

  dict_hdr_create(&mtr);

  mtr_commit(&mtr);

  dberr_t err = dict_boot();

  return (err);
}
