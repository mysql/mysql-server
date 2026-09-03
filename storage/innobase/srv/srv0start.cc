/*****************************************************************************

Copyright (c) 1996, 2026, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

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

/** @file srv/srv0start.cc
 Starts the InnoDB database server

 Created 2/16/1996 Heikki Tuuri
 *************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <zlib.h>

#include "btr0btr.h"
#include "btr0cur.h"
#include "buf0buf.h"
#include "buf0dump.h"
#include "current_thd.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "fil0fil.h"
#include "fil0pages_persistence_interface.h"
#include "fil0tablespace_scan.h"
#include "fil0tablespaces_nodes_interface.h"
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0chkp.h"
#include "log0encryption.h"
#include "log0handler.h"
#include "log0helpers.h"
#include "log0recv.h"
#include "log0write.h"
#include "mem0mem.h"
#include "mtr0mtr.h"

#include "my_dbug.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_stage.h"
#include "mysqld.h"
#include "scope_guard.h"

#include "ddl0fts.h"
#include "os0file.h"
#include "os0thread-create.h"
#include "os0thread.h"
#include "page0cur.h"
#include "page0page.h"
#include "rem0rec.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "ut0mem.h"

#include "arch0arch.h"
#include "arch0recv.h"
#include "btr0pcur.h"
#include "btr0sea.h"
#include "buf0flu.h"
#include "buf0rea.h"
#include "clone0api.h"
#include "clone0clone.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0load.h"
#include "dict0stats_bg.h"
#include "lock0lock.h"
#include "os0event.h"
#include "os0proc.h"
#include "pars0pars.h"
#include "que0que.h"
#include "rem0cmp.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0sel.h"
#include "row0upd.h"
#include "srv0tmp.h"
#include "trx0purge.h"
#include "trx0roll.h"
#include "trx0rseg.h"
#include "usr0sess.h"
#include "ut0crc32.h"
#include "ut0dbg.h"
#include "ut0new.h"

/** true if a raw partition is in use */
bool srv_start_raw_disk_in_use = false;

bool srv_is_being_started = false;
bool srv_sys_tablespaces_open = false;
bool srv_startup_is_before_trx_rollback_phase = false;

/** true if srv_start() has been called */
static bool srv_start_has_been_called = false;

/** Bit flags for tracking background thread creation. They are used to
determine which threads need to be stopped if we need to abort during
the initialisation step. */
enum srv_start_state_t {
  /** No thread started */
  SRV_START_STATE_NONE = 0,
  /** Started IO threads */
  SRV_START_STATE_IO = 1,
  /** Started purge thread(s) */
  SRV_START_STATE_PURGE = 2,
  /** Started bufdump + dict stat and FTS optimize thread. */
  SRV_START_STATE_STAT = 4
};

/** Track server thrd starting phases */
static uint64_t srv_start_state = SRV_START_STATE_NONE;

std::atomic<enum srv_shutdown_t> srv_shutdown_state{SRV_SHUTDOWN_NONE};

/** Name of srv_monitor_file */
static char *srv_monitor_file_name;

/** */
#define SRV_MAX_N_PENDING_SYNC_IOS 100

/* Keys to register InnoDB threads with performance schema */
#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t log_archiver_thread_key;
mysql_pfs_key_t page_archiver_thread_key;
mysql_pfs_key_t buf_pool_create_thread_key;
mysql_pfs_key_t buf_dump_thread_key;
mysql_pfs_key_t buf_resize_thread_key;
mysql_pfs_key_t clone_ddl_thread_key;
mysql_pfs_key_t clone_gtid_thread_key;
mysql_pfs_key_t ddl_thread_key;
mysql_pfs_key_t dict_stats_thread_key;
mysql_pfs_key_t fts_optimize_thread_key;
mysql_pfs_key_t fts_parallel_merge_thread_key;
mysql_pfs_key_t fts_parallel_tokenization_thread_key;
mysql_pfs_key_t srv_error_monitor_thread_key;
mysql_pfs_key_t srv_lock_timeout_thread_key;
mysql_pfs_key_t srv_master_thread_key;
mysql_pfs_key_t srv_monitor_thread_key;
mysql_pfs_key_t srv_purge_thread_key;
mysql_pfs_key_t srv_worker_thread_key;
mysql_pfs_key_t trx_recovery_rollback_thread_key;
mysql_pfs_key_t srv_ts_alter_encrypt_thread_key;
mysql_pfs_key_t parallel_rseg_init_thread_key;
mysql_pfs_key_t bulk_flusher_thread_key;
mysql_pfs_key_t bulk_alloc_thread_key;
#endif /* UNIV_PFS_THREAD */

#ifdef HAVE_PSI_STAGE_INTERFACE
/** Array of all InnoDB stage events for monitoring activities via
performance schema. */
static PSI_stage_info *srv_stages[] = {
    &srv_stage_alter_table_end,
    &srv_stage_alter_table_flush,
    &srv_stage_alter_table_insert,
    &srv_stage_alter_table_log_index,
    &srv_stage_alter_table_log_table,
    &srv_stage_alter_table_merge_sort,
    &srv_stage_alter_table_read_pk_internal_sort,
    &srv_stage_alter_tablespace_encryption,
    &srv_stage_buffer_pool_load,
    &srv_stage_clone_file_copy,
    &srv_stage_clone_redo_copy,
    &srv_stage_clone_page_copy,
};
#endif /* HAVE_PSI_STAGE_INTERFACE */

/** Sleep time in loops which wait for pending tasks during shutdown. */
static constexpr uint32_t SHUTDOWN_SLEEP_TIME_US = 100;

/** Number of wait rounds during shutdown, after which error is produced,
or other policy for timed out wait is applied. */
static constexpr uint32_t SHUTDOWN_SLEEP_ROUNDS =
    60 * 1000 * 1000 / SHUTDOWN_SLEEP_TIME_US;

/** Create undo tablespace.
@param[in]  undo_space  Undo Tablespace
@return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t srv_undo_tablespace_create(
    undo_truncate::Tablespace &undo_space) {
  space_id_t space_id = undo_space.id();

  ut_a(!srv_read_only_mode);
  ut_a(!srv_force_recovery);

  ut_d(undo_truncate::inject_crash(
      "create_crash_before_undo_tablespace_create"));

  auto flags = fsp_flags_init(univ_page_size, false, false, false, false);
  fsp_flags_set_undo_unusable(flags);

  /* Create the new UNDO tablespace. */
  const auto err = fil_undo_create(
      space_id, undo_space.space_name(), undo_space.file_name(), flags,
      UNDO_INITIAL_SIZE_IN_PAGES, undo_space.is_explicit());
  if (err != DB_SUCCESS) {
    return err;
  }

  ut_d(
      undo_truncate::inject_crash("create_crash_after_undo_tablespace_create"));

  undo_space.set_new();
  ut_a(undo_truncate::is_reserved(space_id));
  return err;
}

/** Try to enable encryption of an undo log tablespace.
@param[in]      space_id        undo tablespace id
@return DB_SUCCESS if success */
static dberr_t srv_undo_tablespace_enable_encryption(space_id_t space_id) {
  dberr_t err;

  ut_ad(Encryption::check_keyring());

  /* Set the space flag. The encryption metadata will be generated in
  fsp_header_init later. */
  fil_space_t *space = fil_space_get(space_id);
  if (!FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
    fsp_flags_set_encryption(space->flags);
    err = fil_set_encryption(space_id, Encryption::AES, nullptr, nullptr);
    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_1075, space->name);
      return (err);
    }
  }

  return (DB_SUCCESS);
}

/** Try to read encryption metadata from an undo tablespace.
@param[in]      file_handle     file handle of undo log file
@param[in]      space           undo tablespace
@return DB_SUCCESS if success */
static dberr_t srv_undo_tablespace_read_encryption(
    ib::fil::Tablespace_node_handle_interface *file_handle,
    fil_space_t *space) {
  dberr_t err = DB_ERROR;

  IORequest request{IORequest::Type::READ};
  /* Don't want unnecessary complaints about partial reads. */
  request.disable_partial_io_warnings();

  /* Align the memory for a possible read from a raw device */
  byte *first_page = static_cast<byte *>(
      ut::aligned_alloc(UNIV_PAGE_SIZE_MAX, UNIV_PAGE_SIZE));

  ib::fil::Tablespace_node_handle_interface::Status_IO status =
      file_handle->read_page(request, first_page, 0);

  if (status != ib::fil::Tablespace_node_handle_interface::Status_IO::SUCCESS) {
    ib::info(ER_IB_MSG_FIRST_PAGE_READ_FAILED, space->name, ut_strerr(err));
    ut::aligned_free(first_page);
    return (err);
  }

  const page_size_t space_page_size(space->flags);
  const auto offset = fsp_header_get_encryption_offset(space_page_size);
  ut_ad(offset);

  /* Return if the encryption metadata is empty. */
  if (!Encryption::is_encrypted_with_v3(first_page + offset)) {
    ut::aligned_free(first_page);
    return (DB_SUCCESS);
  }

  byte key[Encryption::KEY_LEN];
  byte iv[Encryption::KEY_LEN];
  Encryption_key e_key{key, iv};
  if (fsp_header_get_encryption_key(space->flags, e_key, first_page)) {
    fsp_flags_set_encryption(space->flags);
    err = fil_set_encryption(space->id, Encryption::AES, key, iv);
    ut_ad(err == DB_SUCCESS);
  } else {
    ut::aligned_free(first_page);
    return (DB_FAIL);
  }

  ut::aligned_free(first_page);
  ib::info(ER_IB_MSG_UNDO_ENCRYPTION_INFO_LOADED, space->name);

  return (DB_SUCCESS);
}

/** Handles the undo_$num_trunc.log marker generated by 9.x, by removing the
space with corresponding space_id, if it was found during tablespace scanning.
This ensures:
1. It will not be opened by subsequent srv_undo_tablespace_open_by_num(num)
2. It will clean up file created by CREATE UNDO TABLESPACE if it wasn't yet
   registered in DD (and thus wouldn't be found in DD and processed by
   srv_undo_tablespace_fixup)
@param[in]  space_num  undo tablespace number
@return error code */
static dberr_t srv_undo_tablespace_fixup_num(space_id_t space_num) {
  if (!undo_truncate::is_active_truncate_log_present(space_num)) {
    return (DB_SUCCESS);
  }

  ib::info(ER_IB_MSG_UNDO_TRUNCATE_DURING_SHUTDOWN_OR_CRASH, ulong{space_num});

  if (srv_read_only_mode) {
    ib::error(ER_IB_MSG_UNDO_RECOVER_FAILED_READ_ONLY_MODE);
    return (DB_READ_ONLY);
  }

  if (undo_truncate::num2id_map->contains(space_num)) {
    /* Delete the tablespace file found with this space number. */
    const auto space_id = undo_truncate::num2id_map->get(space_num);
    ut_ad(fsp_is_undo_tablespace(space_id));
    ut_ad(fil_space_get(space_id) == nullptr);
    ut_a(tablespace_scanning != nullptr);
    const auto scanned_name =
        tablespace_scanning->get_tablespace_file_by_id(space_id);
    mtr_t mtr;
    mtr.start();
    fil_op_write_log(MLOG_FILE_DELETE, space_id, scanned_name->c_str(), nullptr,
                     0, &mtr);
    mtr.commit();

    ib::redo::must_succeed(
        ib::redo::handler->persist_smaller_than(mtr.commit_lsn()),
        UT_LOCATION_HERE);

    /* We are sure here that the file exists, since the corresponding
    space_id is present in the map. */
    ut_ad(os_file_exists(scanned_name->c_str()));
    auto status = tablespaces_nodes->remove(space_id, 0,
                                            {.m_path = scanned_name->c_str()});
    if (status != ib::fil::Tablespaces_nodes_interface::Status::SUCCESS) {
      ib::error(ER_IB_FAILED_TO_DELETE_TABLESPACE_FILE, scanned_name->c_str());
      return DB_IO_ERROR;
    }
  }

  /* Remove the truncate log file */
  undo_truncate::remove_truncate_log_file(space_num);
  return DB_SUCCESS;
}

dberr_t srv_undo_tablespace_fixup(const char *space_name, const char *file_name,
                                  space_id_t space_id) {
  ut_ad(fsp_is_undo_tablespace(space_id));
  const space_id_t space_num = undo_truncate::id2num(space_id);

  /* If it was successfully opened during srv_undo_tablespaces_open() then the
  file exists and doesn't have FSP_FLAGS_MASK_UNDO_UNUSABLE, and there's
  nothing to do here.

  If it was not successfully opened, and srv_undo_tablespaces_open() itself did
  not report an error (as we've reached here), then that means that either:
  a) The srv_undo_tablespace_open_by_num(num) was not even called, because:
     the file with a space_id which maps to this num could not be found during
     the scan either because:
     i)  it simply doesn't exists, or
     ii) it was impossible to figure out its space_id by looking at its header
         as it was empty, corrupted or zeroed.
  b) It was called, but returned DB_UNDO_FILE_UNDER_TRUNCATION as the file had
     FSP_FLAGS_MASK_UNDO_UNUSABLE flag in its header. In this case the file was
     already removed by that function.
  c) It was called, but returned DB_CANNOT_OPEN_FILE as the file was missing,
     even though it was found during the scan, which means that
     srv_undo_tablespace_fixup_num() has removed it already seeing the legacy
     undo_{num}_trunc.log marker for it.

  In case a.ii) we want to clean up the file. We use the file_name from the DD
  as a hint to find it, even if its header is unreadable.

  In all a), b) and c) we treat the situation as a result of a failed truncate
  operation, so we proceed to re-create the space with next(space_id). */
  if (undo_truncate::spaces->contains(space_num)) {
    return DB_SUCCESS;
  }

  ut_ad(!srv_read_only_mode);
  if (srv_read_only_mode) {
    ib::error(ER_IB_MSG_UNDO_RECOVER_FAILED_READ_ONLY_MODE);
    return DB_READ_ONLY;
  }

  const auto node_info =
      tablespaces_nodes->get_node_info(space_id, 0, {.m_path = file_name}, 0);

  if (node_info) {
    /*  Check if the DD path is present in the known paths */
    if (tablespace_scanning &&
        (!tablespace_scanning->is_known_path(file_name))) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_UNPROTECTED_LOCATION_ALLOWED,
                file_name, space_name);
    }

    mtr_t mtr;
    mtr.start();

    fil_op_write_log(MLOG_FILE_DELETE, space_id, file_name, nullptr, 0, &mtr);

    mtr.commit();

    ib::redo::must_succeed(
        ib::redo::handler->persist_smaller_than(mtr.commit_lsn()),
        UT_LOCATION_HERE);

    /* This must be the case of (a.ii) as the node exists, so we delete the file
     */
    auto status = tablespaces_nodes->remove(space_id, 0, {file_name, false});
    if (status != ib::fil::Tablespaces_nodes_interface::Status::SUCCESS) {
      ib::error(ER_IB_FAILED_TO_DELETE_TABLESPACE_FILE, file_name);
      return DB_IO_ERROR;
    }
  }

  ib::info(ER_IB_MSG_UNDO_TABLESPACE_RECONSTRUCTING, ulong{space_num});

  /* Mark the space_id for this undo tablespace number as in-use. */
  undo_truncate::spaces->x_lock(UT_LOCATION_HERE);
  undo_truncate::unuse_space_id(space_id);
  space_id_t new_space_id = undo_truncate::next_space_id(space_id);
  undo_truncate::use_space_id(new_space_id);
  undo_truncate::spaces->x_unlock();

  ut_d(undo_truncate::inject_crash(
      "fixup_crash_before_updating_space_id_in_dd"));

  /* Update the DD with the new space id. */
  dd_space_states old_state = DD_SPACE_STATE__LAST;
  bool dd_result = dd_tablespace_get_mdl(space_name);
  if (dd_result == DD_SUCCESS) {
    dd_result = dd_tablespace_set_space_id_and_get_state(
        space_name, new_space_id, old_state);
  }
  if (dd_result != DD_SUCCESS) {
    return DB_ERROR;
  }

  ut_d(
      undo_truncate::inject_crash("fixup_crash_after_updating_space_id_in_dd"));

  /* Create the undo tablespace with new space id. */
  dberr_t err = srv_undo_tablespace_create(space_name, file_name, new_space_id);
  if (err != DB_SUCCESS) {
    return (err);
  }

  ut_d(undo_truncate::inject_crash("fixup_crash_after_creating_undo"));

  /* Update the DD with the new state. */
  undo_truncate::spaces->x_lock(UT_LOCATION_HERE);

  undo_truncate::Tablespace *undo_space =
      undo_truncate::spaces->find(space_num);

  dd_space_states to_state;
  ut_a(old_state != DD_SPACE_STATE__LAST);
  if (old_state == DD_SPACE_STATE_INACTIVE ||
      old_state == DD_SPACE_STATE_EMPTY) {
    to_state = DD_SPACE_STATE_EMPTY;
    undo_space->set_empty();
  } else {
    to_state = DD_SPACE_STATE_ACTIVE;
    undo_space->set_active();
  }
  undo_truncate::spaces->x_unlock();
  ut_d(undo_truncate::inject_crash("fixup_crash_before_updating_state_in_dd"));

  /* We need to acquire the MDL on space_name again because the MDL acquired
  earlier in this function was released as part of the commit called inside
  the function dd_tablespace_set_space_id_and_get_state(). */
  dd_result = dd_tablespace_get_mdl(space_name);
  if (dd_result == DD_SUCCESS) {
    dd_result =
        dd_tablespace_set_id_and_state(space_name, new_space_id, to_state);
  }
  if (dd_result != DD_SUCCESS) {
    err = DB_ERROR;
  }
  ut_d(undo_truncate::inject_crash("fixup_crash_after_updating_state_in_dd"));

  return (err);
}

dberr_t srv_undo_tablespace_open(undo_truncate::Tablespace &undo_space,
                                 bool expected_to_be_unusable) {
  DBUG_EXECUTE_IF("ib_undo_tablespace_open_fail",
                  return (DB_CANNOT_OPEN_FILE););

  space_id_t space_id = undo_space.id();
  char *undo_name = undo_space.space_name();
  char *file_name = undo_space.file_name();

  /* Check if it was already opened during redo recovery. */
  fil_space_t *space = fil_space_get(space_id);

  /* Flush and close any current file handle so we can open
  a local one below. */
  if (space != nullptr) {
    fil_flush(space_id);
    fil_space_close(space_id);
  }

  using Open_error = ib::fil::Tablespaces_nodes_interface::Open_error;

  /* Create in memory fil_space_t and fil_node_t structure */
  {
    /* Open a local handle. */
    uint32_t flags = fsp_flags_init(univ_page_size, false, false, false, false);
    if (expected_to_be_unusable) {
      fsp_flags_set_undo_unusable(flags);
    }
    const auto status =
        tablespaces_nodes->open(space_id, 0, {.m_path = file_name},
                                page_size_t(flags).physical(), true);
    if (!status) {
      switch (status.error()) {
        case Open_error::NO_ACCESS_PERMISSIONS:
          return DB_READ_ONLY;
        case Open_error::NODE_DOES_NOT_EXIST:
        case Open_error::IO_ERROR:
          return DB_CANNOT_OPEN_FILE;

        default:
          ut_d(ut_error);
          ut_o(return DB_CANNOT_OPEN_FILE);
      }
    }
    if (space == nullptr) {
      /* Load the tablespace into InnoDB's internal data structures.
      Set the compressed page size to 0 (non-compressed) */
      space = fil_space_create(undo_name, space_id, flags, FIL_TYPE_TABLESPACE);
      ut_a(space != nullptr);
      ut_ad(fil_validate());

      fil_node_create(file_name, space, false, PAGE_NO_MAX);
    }

    /* Read the encryption metadata in this undo tablespace.
    If the encryption info in the first page cannot be decrypted
    by the master key, this tablespace cannot be opened. */
    const dberr_t err =
        srv_undo_tablespace_read_encryption(status->get(), space);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_ENCRYPTION_READ_FAILED, undo_name);
      return err;
    }
  }

  /* Now that space and node exist, make sure this undo tablespace is open so
  that it stays open until shutdown. But if it is still marked unusable, we
  cannot open it until the header page has been written, because it would fill
  the FSP-related cache with invalid data. */
  if (!expected_to_be_unusable) {
    const auto open_res = fil_space_open(space_id);
    ut_a(open_res);
    fil_space_release(*open_res);
  }

  if (undo_truncate::is_reserved(space_id)) {
    undo_truncate::spaces->add(undo_space);
  }

  return DB_SUCCESS;
}

/** Open an undo tablespace with a specified undo number.
If the undo space is undergoing truncation, we will delete the undo file.
@param[in]  space_num  undo tablespace number
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespace_open_by_num(const space_id_t space_num) {
  ut_a(undo_truncate::num2id_map->contains(space_num));
  const auto space_id = undo_truncate::num2id_map->get(space_num);
  ut_a(fsp_is_undo_tablespace(space_id));
  undo_truncate::Tablespace undo_space(space_id);

  if (tablespace_scanning) {
    /* Search for a file that is using any of the space IDs assigned to this
    undo space_id. The directory scan assured that there are no duplicate files
    with the same space_id. */
    const auto scanned_name =
        tablespace_scanning->get_tablespace_file_by_id(space_id);
    if (!scanned_name) {
      return DB_CANNOT_OPEN_FILE;
    }

    /* The first 2 undo space numbers must be implicit. */
    const bool is_default = space_num <= FSP_IMPLICIT_UNDO_TABLESPACES;

    if (!Fil_path::is_same_as(undo_space.file_name(), scanned_name->c_str())) {
      if (is_default) {
        ib::info(ER_IB_MSG_UNDO_TABLESPACE_CREATE_FAILED_ALREADY_EXIST,
                 undo_space.file_name(), scanned_name->c_str(),
                 ulong{space_id});

        return DB_WRONG_FILE_NAME;
      }

      /* Explicit undo tablespaces must end with the suffix '.ibu'. */
      if (!Fil_path::has_suffix(IBU, *scanned_name)) {
        ib::info(ER_IB_MSG_NOT_END_WITH_IBU, scanned_name->c_str());

        return DB_WRONG_FILE_NAME;
      }

      /* Use the file name found in the scan. */
      undo_space.set_file_name(scanned_name->c_str());
    }
  }

  fil_space_t *space = fil_space_get(space_id);
  if (space == nullptr) {
    /* Try to open the undo file and read the header */
    using Open_error = ib::fil::Tablespaces_nodes_interface::Open_error;
    bool unusable_header;
    {
      const auto handle = tablespaces_nodes->open(
          space_id, 0, {.m_path = undo_space.file_name()}, srv_page_size, true);
      if (!handle) {
        switch (handle.error()) {
          case Open_error::NO_ACCESS_PERMISSIONS:
          case Open_error::NODE_DOES_NOT_EXIST:
          case Open_error::IO_ERROR:
            return DB_CANNOT_OPEN_FILE;

          default:
            ut_d(ut_error);
            ut_o(return DB_CANNOT_OPEN_FILE);
        }
      }

      IORequest request{IORequest::Type::READ};

      const auto first_page =
          ut::make_unique_aligned<byte[]>(srv_page_size, srv_page_size);

      ib::fil::Tablespace_node_handle_interface::Status_IO page_read_status =
          handle->get()->read_page(request, first_page.get(), 0);

      if (page_read_status !=
          ib::fil::Tablespace_node_handle_interface::Status_IO::SUCCESS) {
        ib::info(ER_IB_MSG_FIRST_PAGE_READ_FAILED, undo_space.file_name(),
                 ut_strerr(DB_ERROR));
        return DB_ERROR;
      }
      unusable_header =
          (fsp_header_get_field(first_page.get(), FSP_SIZE) == 0) ||
          FSP_FLAGS_GET_UNDO_UNUSABLE(fsp_header_get_flags(first_page.get()));
      /* Releasing the handle at the end of scope, which is crucial for
      Windows, where we have to close all handles before removing a file.*/
    }
    if (unusable_header) {
      if (srv_read_only_mode) {
        ib::error(ER_IB_MSG_UNDO_RECOVER_FAILED_READ_ONLY_MODE);
        return DB_READ_ONLY;
      }

      mtr_t mtr;
      mtr.start();

      fil_op_write_log(MLOG_FILE_DELETE, space_id, undo_space.file_name(),
                       nullptr, 0, &mtr);

      mtr.commit();

      ib::redo::must_succeed(
          ib::redo::handler->persist_smaller_than(mtr.commit_lsn()),
          UT_LOCATION_HERE);

      auto status = tablespaces_nodes->remove(space_id, 0,
                                              {undo_space.file_name(), false});
      if (status != ib::fil::Tablespaces_nodes_interface::Status::SUCCESS) {
        ib::error(ER_IB_FAILED_TO_DELETE_TABLESPACE_FILE,
                  undo_space.file_name());
        return DB_IO_ERROR;
      }

      return DB_UNDO_FILE_UNDER_TRUNCATION;
    }

  } else if (FSP_FLAGS_GET_UNDO_UNUSABLE(space->flags)) {
    if (srv_read_only_mode) {
      ib::error(ER_IB_MSG_UNDO_RECOVER_FAILED_READ_ONLY_MODE);
      return DB_READ_ONLY;
    }
    fil_flush(space_id);
    fil_space_close(space_id);
    auto err = fil_delete_tablespace(space_id);
    if (err != DB_SUCCESS) {
      ib::error(ER_IB_FAILED_TO_DELETE_TABLESPACE_FILE)
          << " with space id=" << space_id << ", file '"
          << undo_space.file_name() << "'!";

      return DB_ERROR;
    }
    return DB_UNDO_FILE_UNDER_TRUNCATION;
  }

  /* Mark the space_id for this undo tablespace number as in-use. */
  undo_truncate::use_space_id(space_id);
  ib::info(ER_IB_MSG_USING_UNDO_SPACE, undo_space.file_name());

  dberr_t err = srv_undo_tablespace_open(undo_space, false);

  if (err == DB_SUCCESS) {
    fil_space_set_undo_size(space_id, false);
  }

  return err;
}

/* Open existing undo tablespaces up to the number FSP_MAX_UNDO_TABLESPACES.
We are attempting to open undo tablespaces before opening the DD; we must open
all undo tablespaces that are not undergoing truncation. If an undo tablespace
is undergoing truncation, it will be recreated in srv_undo_tablespace_fixup().
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespaces_open() {
  /* Open all existing implicit and explicit undo tablespaces.*/
  undo_truncate::spaces->x_lock(UT_LOCATION_HERE);

  ut_ad(undo_truncate::spaces->size() == 0);

  for (space_id_t num = 1; num <= FSP_MAX_UNDO_TABLESPACES; ++num) {
    dberr_t err = srv_undo_tablespace_fixup_num(num);
    if (err != DB_SUCCESS) {
      undo_truncate::spaces->x_unlock();
      return (err);
    }

    /* If no tablespace file is found with this space number, continue. */
    if (!undo_truncate::num2id_map->contains(num)) {
      /* No UNDO space with this number found. */
      continue;
    }

    err = srv_undo_tablespace_open_by_num(num);
    switch (err) {
      case DB_WRONG_FILE_NAME:
        /* An Undo tablespace was found where the mapping
        file said it was.  Now we have a different filename
        for it. The undo directory must have changed and
        the files were not moved. Cannot startup. */
      case DB_READ_ONLY:
        /* The undo tablespace was found where it should be
        but it cannot be opened in read/write mode. */
      default:
        /* The undo tablespace was found where it should be
        but it cannot be used. */
        undo_truncate::spaces->x_unlock();
        return (err);

      case DB_SUCCESS:
      case DB_UNDO_FILE_UNDER_TRUNCATION:
        /* Undergoing truncation, recreate it in srv_undo_tablespace_fixup(). */
      case DB_CANNOT_OPEN_FILE:
        /* Doesn't exist, possibly deleted by srv_undo_tablespace_fixup_num().
         */
        break;
    }
  }

  const size_t n_found = undo_truncate::spaces->size();
  undo_truncate::spaces->x_unlock();

  /* If no UNDO tablespaces found, abort the server startup. */
  if (n_found == 0) {
    ib::error(ER_IB_MSG_UNDO_TS_NOT_FOUND);
    return DB_ERROR;
  }

  if (n_found < FSP_IMPLICIT_UNDO_TABLESPACES) {
    ib::info(ER_IB_MSG_WILL_CREATE_N_UNDO_TS,
             FSP_IMPLICIT_UNDO_TABLESPACES - n_found);
  }

  if (n_found) {
    ib::info(ER_IB_MSG_OPENED_N_UNDO_TS, n_found);
  }

  return (DB_SUCCESS);
}

/** Create the implicit undo tablespaces for the new instance. It only ensures
the Undo Tablespace exists, has a minimal valid header and is open. It doesn't
create rseg arrays, nor rollback segments.
@return DB_SUCCESS or error code */
static dberr_t srv_undo_create_implicit_tablespaces() {
  dberr_t err = DB_SUCCESS;
  ut_a(!srv_read_only_mode);
  ut_a(srv_force_recovery == 0);
  undo_truncate::spaces->x_lock(UT_LOCATION_HERE);

  /* Create implicit undo tablespaces */
  for (space_id_t num = 1; num <= FSP_IMPLICIT_UNDO_TABLESPACES; ++num) {
    /* This part of the code is only executed during database creation, so no
    truncate log files should be present in the data directory. */
    ut_a(!undo_truncate::is_active_truncate_log_present(num));
    ut_a(!undo_truncate::spaces->contains(num));

    /* Mark this implicit undo space number as used and return the next
    available space_id. */
    space_id_t space_id = undo_truncate::use_next_space_id(num);

    undo_truncate::Tablespace undo_space(space_id);
    undo_space.set_new();
    err = srv_undo_tablespace_create(undo_space);
    if (err != DB_SUCCESS) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_UNDO_TABLESPACE_CREATE_FAILED,
                undo_space.space_name());
    }

    /* Open this new undo tablespace. */
    err = srv_undo_tablespace_open(undo_space, true);
    if (err != DB_SUCCESS) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_NEW_UNDO_TABLESPACE_OPEN_ERROR,
                int{err}, ut_strerr(err), undo_space.space_name());
    }
  }

  undo_truncate::spaces->x_unlock();

  ib::info(ER_IB_MSG_CREATED_N_UNDO_TABLESPACES, FSP_IMPLICIT_UNDO_TABLESPACES);

  return (err);
}

/** Initialize FSP structures (such as fragment and inode lists) and the (empty)
rseg array of an undo tablespace. Before the call this tablespace file should be
created and filled with zeros with a minimal tablespace header.
This function does not create any rollback segments.
@param[in]      space_id        undo tablespace id
@param[in]      enable_undo_encryption  whether to update global undo
                                        encryption metadata after construction
@return DB_SUCCESS or error code */
static dberr_t srv_undo_prepare_empty_structure(space_id_t space_id,
                                                bool enable_undo_encryption) {
  ut_a(!srv_read_only_mode);
  ut_a(!srv_force_recovery);

  if (srv_undo_log_encrypt && !Encryption::check_keyring()) {
    my_error(ER_CANNOT_FIND_KEY_IN_KEYRING, MYF(0));
    return (DB_ERROR);
  }

  /* Enable undo log encryption if it's ON. */
  if (srv_undo_log_encrypt) {
    dberr_t err = srv_undo_tablespace_enable_encryption(space_id);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_ENCRYPTED_UNDO_CREATE_FAILED,
                ulong{undo_truncate::id2num(space_id)});

      return (err);
    }
  }

  log_free_check();

  mtr_t mtr;
  mtr_start(&mtr);

  mtr_x_lock(fil_space_get_latch(space_id), &mtr, UT_LOCATION_HERE);

  if (!fsp_header_init(space_id, UNDO_INITIAL_SIZE_IN_PAGES, &mtr)) {
    ib::error(ER_IB_MSG_UNDO_HEADER_INITIALIZE_FAIL,
              ulong{undo_truncate::id2num(space_id)});

    mtr_commit(&mtr);
    return (DB_ERROR);
  }

  /* Add the RSEG_ARRAY page. */
  trx_rseg_array_create(space_id, &mtr);

  mtr_commit(&mtr);

  /* The rollback segments will get created later in
  trx_rseg_add_rollback_segments(). */

  if (srv_undo_log_encrypt && enable_undo_encryption) {
    ut_d(bool ret =) srv_enable_undo_encryption();
    ut_ad(!ret);
  }

  return (DB_SUCCESS);
}

/** Mark the point in which an undo tablespace is fully constructed and ready
to use.
@param[in]      space_id        undo tablespace id */
static void srv_undo_mark_tablespace_usable(space_id_t space_id) {
  const auto space = fil_space_get(space_id);

  if (space && FSP_FLAGS_GET_UNDO_UNUSABLE(space->flags)) {
    mtr_t mtr;
    mtr.start();
    undo_truncate::mark_undo_tablespace_usable(space_id, &mtr);
    mtr.commit();
  }
}

/** Mark any fully constructed undo tablespaces ready to use. */
static void srv_undo_mark_all_tablespaces_usable() {
  Space_Ids space_ids;
  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);
  for (auto undo_space : undo_truncate::spaces->m_spaces) {
    space_ids.push_back(undo_space->id());
  }
  undo_truncate::spaces->s_unlock();

  for (const auto space_id : space_ids) {
    srv_undo_mark_tablespace_usable(space_id);
  }
}

dberr_t srv_undo_tablespace_create(const char *space_name,
                                   const char *file_name, space_id_t space_id) {
  if (srv_undo_log_encrypt && !Encryption::check_keyring()) {
    my_error(ER_CANNOT_FIND_KEY_IN_KEYRING, MYF(0));
    return (DB_ERROR);
  }

  /* We need to x_lock the undo_truncate::spaces list until after this
  is created and added to it. */
  undo_truncate::spaces->x_lock(UT_LOCATION_HERE);

  undo_truncate::Tablespace undo_space(space_id);
  undo_space.set_space_name(space_name);
  undo_space.set_file_name(file_name);

  ut_ad(undo_truncate::spaces->find(undo_space.num()) == nullptr);

  if (const auto err = srv_undo_tablespace_create(undo_space);
      err != DB_SUCCESS) {
    undo_truncate::spaces->x_unlock();
    return err;
  }

  auto undo_space_create_guard = create_scope_guard([&undo_space]() {
    mtr_t mtr;
    mtr.start();
    fil_op_write_log(MLOG_FILE_DELETE, undo_space.id(), undo_space.file_name(),
                     nullptr, 0, &mtr);

    mtr.commit();

    ib::redo::must_succeed(
        ib::redo::handler->persist_smaller_than(mtr.commit_lsn()),
        UT_LOCATION_HERE);

    [[maybe_unused]] const auto status = tablespaces_nodes->remove(
        undo_space.id(), 0, {.m_path = undo_space.file_name()});
    ut_ad(status == ib::fil::Tablespaces_nodes_interface::Status::SUCCESS);
  });

  /* Open this new undo tablespace. */
  if (const auto err = srv_undo_tablespace_open(undo_space, true);
      err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_ERROR_OPENING_NEW_UNDO_SPACE, int{err}, space_name);
    undo_truncate::spaces->x_unlock();
    return err;
  }

  /* Unlock the undo_truncate::spaces list now that we are no longer changing
  it. This new undo space will not be used by new transactions until it becomes
  active. */
  undo_truncate::spaces->x_unlock();

  /* srv_undo_tablespace_open adds the undo space to the list, rollback this
  operation in case of errors. */
  auto undo_space_list_guard = create_scope_guard([&undo_space]() {
    undo_truncate::spaces->x_lock(UT_LOCATION_HERE);
    undo_truncate::spaces->drop(undo_space);
    undo_truncate::spaces->x_unlock();
  });

  /* Write header and RSEG_ARRAY pages to this undo tablespace. */
  if (const auto err = srv_undo_prepare_empty_structure(space_id, true);
      err != DB_SUCCESS) {
    return err;
  }

  /* Create the rollback segments in this tablespace and add an Rseg object
  for each one to the Rsegs list. */
  if (!trx_rseg_init_rollback_segments(space_id, srv_rollback_segments)) {
    return DB_ERROR;
  }

  undo_space_list_guard.release();
  undo_space_create_guard.release();

  srv_undo_mark_tablespace_usable(space_id);

  return DB_SUCCESS;
}

void undo_truncate_spaces_init() {
  ut_ad(undo_truncate::spaces == nullptr);

  undo_truncate::spaces = ut::new_withkey<undo_truncate::Tablespaces>(
      ut::make_psi_memory_key(mem_key_undo_spaces));
}

void undo_truncate_spaces_deinit() {
  if (undo_truncate::spaces != nullptr) {
    /* There can't be any active transactions. */
    undo_truncate::spaces->clear();

    ut::delete_(undo_truncate::spaces);
    undo_truncate::spaces = nullptr;
  }
}

/** Create the implicit undo tablespaces for the new instance, and create
their rseg arrrays, but not their rollback segments.
@return DB_SUCCESS or error code */
static dberr_t srv_undo_create_implicit_tablespaces_with_empty_structure() {
  dberr_t err = DB_SUCCESS;

  /* Create and open implicit undo tablespaces for the new DB. */
  mutex_enter(&undo_truncate::ddl_mutex);
  err = srv_undo_create_implicit_tablespaces();
  if (err != DB_SUCCESS) {
    mutex_exit(&undo_truncate::ddl_mutex);
    return (err);
  }

  Space_Ids new_space_ids;
  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);
  for (auto undo_space : undo_truncate::spaces->m_spaces) {
    if (undo_space->is_new()) {
      new_space_ids.push_back(undo_space->id());
    }
  }
  undo_truncate::spaces->s_unlock();

  for (const auto space_id : new_space_ids) {
    err = srv_undo_prepare_empty_structure(space_id, false);
    if (err != DB_SUCCESS) {
      mutex_exit(&undo_truncate::ddl_mutex);
      return (err);
    }
  }

  if (srv_undo_log_encrypt) {
    ut_d(bool ret =) srv_enable_undo_encryption();
    ut_ad(!ret);
  }

  /* We don't want to mark construction done here as we will do it later in
  srv_start() after doing trx_rseg_adjust_rollback_segments(), which will
  finalize construction of the undo tablespaces. */

  mutex_exit(&undo_truncate::ddl_mutex);
  return (DB_SUCCESS);
}

/** Wait for the purge thread(s) to start up. */
static void srv_start_wait_for_purge_to_start() {
  /* Wait for the purge coordinator and master thread to startup. */

  purge_state_t state = trx_purge_state();

  ut_a(state != PURGE_STATE_DISABLED);

  while (srv_shutdown_state.load() < SRV_SHUTDOWN_PURGE &&
         srv_force_recovery < SRV_FORCE_NO_BACKGROUND &&
         state == PURGE_STATE_INIT) {
    switch (state = trx_purge_state()) {
      case PURGE_STATE_RUN:
      case PURGE_STATE_STOP:
        break;

      case PURGE_STATE_INIT:
        ib::info(ER_IB_MSG_1097);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        break;

      case PURGE_STATE_EXIT:
      case PURGE_STATE_DISABLED:
        ut_error;
    }
  }
}

/** Deletes all files used by the System Temporary Tablespace. */
static void srv_delete_temporary_space_files(
    ib::fsp::SysTablespace &tmp_space) {
  for (const auto node_order : srv_tmp_space.delete_files()) {
    ib::info(ER_IB_REMOVED_TEMPORARY_TABLESPACE_FILE,
             tmp_space.node(node_order).name().c_str());
  }
}

/** Create the temporary file tablespace.
@param[in,out]  tmp_space       Shared Temporary SysTablespace
@return DB_SUCCESS or error code. */
static dberr_t srv_open_tmp_tablespace(ib::fsp::SysTablespace &tmp_space) {
  /* Will try to remove if there is existing file left-over by last unclean
  shutdown */
  srv_delete_temporary_space_files(tmp_space);

  ib::info(ER_IB_MSG_1098);

  srv_recovery_crash(100);

  const auto err = tmp_space.check_file_spec(true, 12 * 1024 * 1024, false);

  if (err != DB_SUCCESS) {
    return err;
  }

  const auto prepare_err = tmp_space.prepare_nodes();

  if (prepare_err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_1101, tmp_space.name());
    return err;
  }

  const auto size = tmp_space.get_sum_of_expected_sizes_in_pages();
  ut_ad_eq(size, fil_space_get_size(tmp_space.space_id()));

  /* Open this shared temp tablespace in the fil_system so that
  it stays open until shutdown. */
  if (const auto space = fil_space_open(tmp_space.space_id()); !space) {
    ib::error(ER_IB_MSG_1102, tmp_space.name());
    return space.error();
  } else {
    fil_space_release(*space);
  }

  /* Initialize the header page */
  mtr_t mtr;
  mtr_start(&mtr);
  mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

  /* Could the write_initial_pages() be called in the prepare_nodes() above?
  This would cause the if statement in the `fil_space_t::validate_first_page()`
  to not be needed anymore. */
  fsp_header_init(tmp_space.space_id(), size, &mtr);

  mtr_commit(&mtr);

  return DB_SUCCESS;
}

/** Create SDI Indexes in system tablespace. */
static void srv_create_sdi_indexes() {
  btr_sdi_create_index(SYSTEM_TABLE_SPACE, false);
}

/** Set state to indicate start of particular group of threads in InnoDB. */
static inline void srv_start_state_set(
    srv_start_state_t state) /*!< in: indicate current
                             state of thread startup */
{
  srv_start_state |= state;
}

/** Check if following group of threads is started.
 @return true if started */
static inline bool srv_start_state_is_set(
    srv_start_state_t state) /*!< in: state to check for */
{
  return (srv_start_state & state);
}

struct Thread_to_stop {
  /** Name of the thread, printed to the error log if we waited too
  long (after 60 seconds and then every 60 seconds). */
  const char *m_name;

  /** Future which allows to check if given task is completed. */
  const IB_thread &m_thread;

  /** Function which can be called any number of times to wake
  the possibly waiting thread, so it could exit. */
  std::function<void()> m_notify;

  /** Shutdown state in which we are waiting until thread is exited
  (earlier we keep notifying but we don't require it to exit before
  we may switch to the next state). */
  srv_shutdown_t m_wait_on_state;
};

static const Thread_to_stop threads_to_stop[]{
    {"lock_wait_timeout", srv_threads.m_lock_wait_timeout,
     lock_set_timeout_event, SRV_SHUTDOWN_CLEANUP},

    {"error_monitor", srv_threads.m_error_monitor,
     []() { os_event_set(srv_error_event); }, SRV_SHUTDOWN_CLEANUP},

    {"monitor", srv_threads.m_monitor,
     []() { os_event_set(srv_monitor_event); }, SRV_SHUTDOWN_CLEANUP},

    {"buf_dump", srv_threads.m_buf_dump,
     []() { os_event_set(srv_buf_dump_event); }, SRV_SHUTDOWN_CLEANUP},

    {"buf_resize", srv_threads.m_buf_resize,
     []() { os_event_set(srv_buf_resize_event); }, SRV_SHUTDOWN_CLEANUP},

    {"master", srv_threads.m_master, srv_wake_master_thread,
     SRV_SHUTDOWN_MASTER_STOP}};

void srv_shutdown_exit_threads() {
  if (srv_thread_is_active(srv_threads.m_log_checkpointer)) {
    (void)ib::redo::handler->persist_available();
  }
  srv_shutdown_state.store(SRV_SHUTDOWN_EXIT_THREADS);

  if (srv_start_state == SRV_START_STATE_NONE) {
    return;
  }

  uint32_t i;

  /* All threads end up waiting for certain events. Put those events
  to the signaled state. Then the threads will exit themselves after
  os_event_wait(). */
  for (i = 0; i < SHUTDOWN_SLEEP_ROUNDS; i++) {
    /* NOTE: IF YOU CREATE THREADS IN INNODB, YOU MUST EXIT THEM
    HERE OR EARLIER */

    /* These threads normally finish when reaching SRV_SHUTDOWN_CLEANUP or
    SRV_SHUTDOWN_MASTER_STOP state, which we might have jumped over. */
    for (const auto &thread_info : threads_to_stop) {
      if (srv_thread_is_active(thread_info.m_thread)) {
        thread_info.m_notify();
      }
    }

    if (!srv_read_only_mode) {
      if (srv_start_state_is_set(SRV_START_STATE_PURGE)) {
        /* Wakeup purge threads. */
        srv_purge_wakeup();
      }
    }

    if (srv_start_state_is_set(SRV_START_STATE_IO)) {
      /* Exit the i/o threads */
      if (!srv_read_only_mode) {
        if (recv_sys->flush_start != nullptr) {
          os_event_set(recv_sys->flush_start);
        }
        if (recv_sys->flush_end != nullptr) {
          os_event_set(recv_sys->flush_end);
        }
      }

      os_event_set(buf_flush_event);

      if (!buf_flush_page_cleaner_is_active() && os_aio_all_slots_free()) {
        os_aio_wake_all_threads_at_shutdown();
      }
    }

    if (srv_thread_is_active(srv_threads.m_dict_stats)) {
      os_event_set(dict_stats_event);
    }

    /* Try to stop archiver threads. */
    arch_wake_threads();

    if (log_sys != nullptr) {
      /* Preserve the log threads for the 75% of the total
      time we are waiting here until all threads are stopped.
      This is because log threads are normally shut down at
      the very end and we might need their help to stop other
      threads. */
      if (!buf_flush_page_cleaner_is_active() ||
          i >= SHUTDOWN_SLEEP_ROUNDS * 0.75) {
        log_stop_background_threads_nowait(*log_sys);
      } else {
        /* Ensure log threads are working. The redo log is
        like a blood, we need it for a lot of other systems
        to work. Ensure the blood flows. */
        log_wake_threads(*log_sys);
      }
    }
    /* Stop the checkpointer only once writer and flusher aren't active, as it
    asserts all mtrs which did write_mtr() are already persisted to disc, and
    that therefore it can recognize that dirty pages were added to flush list by
    comparing buf_flush_list_added->smallest_not_added_lsn() to
    peek_first_nonpersisted_lsn(). */
    if (srv_thread_is_active(srv_threads.m_log_checkpointer) &&
        (!log_sys || (!srv_thread_is_active(srv_threads.m_log_flusher) &&
                      !srv_thread_is_active(srv_threads.m_log_writer)))) {
      ut_a(log_checkpointing != nullptr);
      log_checkpointing->stop_thread_no_wait();
    }

    bool active = os_thread_any_active();

    std::this_thread::sleep_for(
        std::chrono::microseconds(SHUTDOWN_SLEEP_TIME_US));

    if (!active) {
      break;
    }
  }

  if (i == SHUTDOWN_SLEEP_ROUNDS) {
    ib::warn(ER_IB_MSG_1103, os_thread_count.load());

#ifdef UNIV_DEBUG
    os_aio_print_pending_io(stderr);
    ut_d(ut_error);
#endif /* UNIV_DEBUG */
  } else {
    /* Reset the start state. */
    srv_start_state = SRV_START_STATE_NONE;
  }
}

#ifdef UNIV_DEBUG
#define srv_init_abort(_db_err) \
  srv_init_abort_low(create_new_db, __FILE__, __LINE__, _db_err)
#else
#define srv_init_abort(_db_err) srv_init_abort_low(create_new_db, _db_err)
#endif /* UNIV_DEBUG */

/** Innobase start-up aborted. Perform cleanup actions.
@param[in]      create_new_db   true if new db is  being created
@param[in]      file            File name
@param[in]      line            Line number
@param[in]      err             Reason for aborting InnoDB startup
@return DB_SUCCESS or error code. */
static dberr_t srv_init_abort_low(bool create_new_db,
                                  IF_DEBUG(const char *file, ulint line, )
                                      dberr_t err) {
  std::ostringstream msg;

#ifdef UNIV_DEBUG
  msg << "at " << innobase_basename(file) << "[" << line << "] ";
#endif /* UNIV_DEBUG */

  if (create_new_db) {
    ib::error(ER_IB_MSG_1104, msg.str().c_str(), ut_strerr(err));
  } else {
    ib::error(ER_IB_MSG_1105, msg.str().c_str(), ut_strerr(err));
  }

  clone_files_error();
  srv_shutdown_exit_threads();

  return (err);
}

/** Check the page type, if there is a mismatch then throw
fatal error. It may so happen that data file before 5.7 GA version
may contain uninitialized bytes in the FIL_PAGE_TYPE field.
@param[in]  page_id         Page id to verify
@param[in]  type            Expected page type */
static void verify_page_type(page_id_t page_id, page_type_t type) {
  mtr_t mtr;
  mtr_start(&mtr);
  /* We should not write to redo log before checkpointing is enabled as it risks
  running out of space, and we don't expect to write anything in this mtr.
  It should be read only */
  mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

  const auto *block =
      buf_page_get(page_id, univ_page_size, RW_S_LATCH, UT_LOCATION_HERE, &mtr);

  const auto page_type = fil_page_get_type(block->frame);
  if (page_type != type) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_INVALID_PAGE_TYPE, unsigned{type},
              unsigned{page_type}, ulong{page_id.space()},
              ulong{page_id.page_no()});
  }
  mtr_commit(&mtr);
}

dberr_t srv_start(bool create_new_db) {
  /* Reset the start state. */
  srv_start_state = SRV_START_STATE_NONE;

#ifdef UNIV_LINUX
#ifdef HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
  ib::info(ER_IB_MSG_1107);
#else
  ib::info(ER_IB_MSG_1108);
#endif /* HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE */
#endif /* UNIV_LINUX */

  static_assert(sizeof(ulint) == sizeof(void *),
                "Size of InnoDB's ulint is not the same as size of void*. The "
                "sizes should be the same so that on a 64-bit platforms you "
                "can allocate more than 4 GB of memory.");

#ifdef UNIV_DEBUG
  ib::info(ER_IB_MSG_1112) << "!!!!!!!! UNIV_DEBUG switched on !!!!!!!!!";
#endif

#ifdef UNIV_IBUF_DEBUG
  ib::info(ER_IB_MSG_1113) << "!!!!!!!! UNIV_IBUF_DEBUG switched on !!!!!!!!!";
#ifdef UNIV_IBUF_COUNT_DEBUG
  ib::info(ER_IB_MSG_1114)
      << "!!!!!!!! UNIV_IBUF_COUNT_DEBUG switched on !!!!!!!!!";
  ib::error(ER_IB_MSG_1115)
      << "Crash recovery will fail with UNIV_IBUF_COUNT_DEBUG";
#endif
#endif

#if defined(COMPILER_HINTS_ENABLED)
  ib::info(ER_IB_MSG_1117) << "Compiler hints enabled.";
#endif /* defined(COMPILER_HINTS_ENABLED) */

  ib::info(ER_IB_MSG_1119) << MUTEX_TYPE;
  ib::info(ER_IB_MSG_1120) << IB_MEMORY_BARRIER_STARTUP_MSG;

  if (srv_force_recovery > 0) {
    ib::info(ER_IB_MSG_1121) << "!!! innodb_force_recovery is set to "
                             << srv_force_recovery << " !!!";
  }

#ifndef HAVE_MEMORY_BARRIER
#if defined __i386__ || defined __x86_64__ || defined _M_IX86 || \
    defined _M_X64 || defined _WIN32
#else
  ib::warn(ER_IB_MSG_1122);
#endif /* IA32 or AMD64 */
#endif /* HAVE_MEMORY_BARRIER */

#ifdef UNIV_ZIP_DEBUG
  ib::info(ER_IB_MSG_1123, ZLIB_VERSION) << " with validation";
#else
  ib::info(ER_IB_MSG_1123, ZLIB_VERSION);
#endif /* UNIV_ZIP_DEBUG */

#ifdef UNIV_ZIP_COPY
  ib::info(ER_IB_MSG_1124) << "and extra copying";
#endif /* UNIV_ZIP_COPY */

  /* Since InnoDB does not currently clean up all its internal data
  structures in MySQL Embedded Server Library server_end(), we
  print an error message if someone tries to start up InnoDB a
  second time during the process lifetime. */

  if (srv_start_has_been_called) {
    ib::error(ER_IB_MSG_1125);
  }

  srv_start_has_been_called = true;

  srv_is_being_started = true;

#ifdef HAVE_PSI_STAGE_INTERFACE
  /* Register performance schema stages before any real work has been
  started which may need to be instrumented. */
  mysql_stage_register("innodb", srv_stages, UT_ARR_SIZE(srv_stages));
#endif /* HAVE_PSI_STAGE_INTERFACE */

  /* Switch latching order checks on in sync0debug.cc, if
  --innodb-sync-debug=false (default) */
  ut_d(sync_check_enable());

  srv_boot();

  ib::info(ER_IB_MSG_1126)
      << "Using "
      << (ut_crc32_cpu_enabled ? (ut_poly_mul_cpu_enabled
                                      ? "hardware accelerated crc32 and "
                                        "polynomial multiplication."
                                      : "hardware accelerated crc32 and "
                                        "software polynomial multiplication.")
                               : "software crc32.");

  os_create_block_cache();

  fil_init(innobase_get_open_files_limit());

  /** Minimum expected tablespace size. TBD Why is it 5MB exactly? */
  constexpr auto MIN_EXPECTED_TABLESPACE_SIZE = 5 * 1024 * 1024;

  /* Check if the data files exist or not and if their sizes are correct. */
  if (const auto err = srv_sys_space.check_file_spec(
          create_new_db, MIN_EXPECTED_TABLESPACE_SIZE,
          tablespaces_nodes->get_capabilities().supports_raw_devices);
      err != DB_SUCCESS) {
    return srv_init_abort(err);
  }

  /* Must replace clone files before scanning directories. When
  clone replaces current database, cloned files are moved to data files
  at this stage. */
  if (const auto err = clone_init(); err != DB_SUCCESS) {
    return srv_init_abort(err);
  }

  if (!srv_read_only_mode) {
    mutex_create(LATCH_ID_SRV_MONITOR_FILE, &srv_monitor_file_mutex);

    if (srv_innodb_status) {
      srv_monitor_file_name = static_cast<char *>(ut::malloc_withkey(
          UT_NEW_THIS_FILE_PSI_KEY,
          MySQL_datadir_path.len() + 20 + sizeof "/innodb_status."));

      sprintf(srv_monitor_file_name, "%s/innodb_status." ULINTPF,
              static_cast<const char *>(MySQL_datadir_path),
              os_proc_get_number());

      srv_monitor_file = fopen(srv_monitor_file_name, "w+");

      if (!srv_monitor_file) {
        ib::error(ER_IB_MSG_1127, srv_monitor_file_name, strerror(errno));

        return srv_init_abort(DB_ERROR);
      }
    } else {
      srv_monitor_file_name = nullptr;
      srv_monitor_file = os_file_create_tmpfile();

      if (!srv_monitor_file) {
        return srv_init_abort(DB_ERROR);
      }
    }

    mutex_create(LATCH_ID_SRV_MISC_TMPFILE, &srv_misc_tmpfile_mutex);

    srv_misc_tmpfile = os_file_create_tmpfile();

    if (!srv_misc_tmpfile) {
      return srv_init_abort(DB_ERROR);
    }
  }

  if (!os_aio_init(srv_n_read_io_threads, srv_n_write_io_threads)) {
    ib::error(ER_IB_MSG_1129);

    return srv_init_abort(DB_ERROR);
  }

  double size;
  char unit;

  if (srv_buf_pool_size >= 1024 * 1024 * 1024) {
    size = ((double)srv_buf_pool_size) / (1024 * 1024 * 1024);
    unit = 'G';
  } else {
    size = ((double)srv_buf_pool_size) / (1024 * 1024);
    unit = 'M';
  }

  double chunk_size;
  char chunk_unit;

  if (srv_buf_pool_chunk_unit >= 1024 * 1024 * 1024) {
    chunk_size = srv_buf_pool_chunk_unit / 1024.0 / 1024 / 1024;
    chunk_unit = 'G';
  } else {
    chunk_size = srv_buf_pool_chunk_unit / 1024.0 / 1024;
    chunk_unit = 'M';
  }

  ib::info(ER_IB_MSG_1130, size, unit, srv_buf_pool_instances, chunk_size,
           chunk_unit);

  if (const auto err = buf_pool_init(srv_buf_pool_size, srv_buf_pool_instances);
      err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_1131);

    return srv_init_abort(DB_ERROR);
  }

  ib::info(ER_IB_MSG_1132);

#ifdef UNIV_DEBUG
  /* We have observed deadlocks with a 5MB buffer pool but
  the actual lower limit could very well be a little higher. */

  if (srv_buf_pool_size <= 5 * 1024 * 1024) {
    ib::info(ER_IB_MSG_1133, ulonglong{srv_buf_pool_size / 1024 / 1024});
  }
#endif /* UNIV_DEBUG */

  fsp_init();
  pars_init();
  recv_sys_create();
  recv_sys_init();
  trx_sys_create();
  lock_sys_create(srv_lock_table_size);
  dict_persist_init();

  /* Create i/o-handler threads: */
  os_aio_start_threads();

  if (create_new_db) {
    recv_sys_free();
  }

  srv_startup_is_before_trx_rollback_phase = !create_new_db;

  /* Open or create the data files for the System Tablespace. */
  switch (const auto err = srv_sys_space.prepare_nodes(); err) {
    case DB_SUCCESS:
      break;
    case DB_CANNOT_OPEN_FILE:
      ib::error(ER_IB_MSG_1134);
      [[fallthrough]];
    default:
      return srv_init_abort(err);
  }

  /* Load Double-write buffer pages before we use them to recover broken pages.
  We will start with recovering the System Tablespace right away. */
  if (const auto err = recv_sys->dblwr->load(); err != DB_SUCCESS) {
    return srv_init_abort(err);
  }

  /* Check if the System Tablespace has to be recovered from the Double-write
  buffer. Other tablespaces will attempt to be recovered on calls to
  Fil_system::open_for_recovery(). For all tablespaces that don't have any redo
  log changes to be applied, we don't have to run the Double-write buffer
  recovery, as they have all pages written out before the checkpoint, so no
  writes could be torn when attempting to do a checkpoint past any of such
  writes. */
  const auto sys_space = fil_space_get_sys_space();
  ut_a(sys_space != nullptr);

  recv_sys->dblwr->recover(*sys_space);

  lsn_t flushed_lsn;
  if (create_new_db) {
    /* The data files are empty, so we assign the initial value to flush_lsn
    instead of reading it from disk. */
    flushed_lsn = LOG_START_LSN + LOG_BLOCK_HDR_SIZE;
  } else {
    /* Validate the header page in the first datafile in the system tablespace
    and read flush_lsn from the validated header page. */
    const auto res = srv_sys_space.read_lsn_and_check_flags();
    if (!res) {
      return srv_init_abort(res.error());
    }
    flushed_lsn = *res;
  }

  if (flushed_lsn < LOG_START_LSN) {
    ut_ad(!create_new_db);
    /* Data directory hasn't been initialized yet. */
    ib::error(ER_IB_MSG_DATA_DIRECTORY_NOT_INITIALIZED_OR_CORRUPTED);
    return srv_init_abort(DB_ERROR);
  }

  mtr_t::s_logging.init();

  if (dblwr::is_enabled()) {
    if (const auto err = dblwr::open(); err != DB_SUCCESS) {
      return srv_init_abort(err);
    }
  }
  if (ib::redo::handler == nullptr) {
    ib::redo::set_handler(new ib::redo::Handler{});
  }

  if (!srv_reconfigure_log_handler()) {
    return srv_init_abort(DB_ERROR);
  }

  if (srv_redo_log_encrypt &&
      !ib::redo::handler->get_capabilities().supports_encryption) {
    ib::error(ER_IB_REDO_HANDLER_NO_ENCRYPTION_SUPPORT);
    return srv_init_abort(DB_ERROR);
  }

  if (pages_persistence->init() !=
      ib::fil::Pages_persistence_interface::Status::SUCCESS) {
    return srv_init_abort(DB_ERROR);
  }

  srv_start_state_set(SRV_START_STATE_IO);

  if (create_new_db) {
    /* There are no pages to be recovered. */
    ut_a(buf_are_flush_lists_empty_validate());

    ut_a(!srv_read_only_mode);

    ut_a(flushed_lsn == LOG_START_LSN + LOG_BLOCK_HDR_SIZE);
    if (ib::redo::handler->create(flushed_lsn) != ib::redo::Status::SUCCESS) {
      return srv_init_abort(DB_ERROR);
    }

    if (ib::redo::handler->get_capabilities().supports_clone) {
      /* arch_init() should be called before recovery, but after
      log_sys_create() */
      ut_a(log_sys != nullptr);
      arch_init();
    }

    if (pages_persistence->assume_checkpoint_lsn(flushed_lsn) !=
        ib::fil::Pages_persistence_interface::Status::SUCCESS) {
      return srv_init_abort(DB_ERROR);
    }

    if (ib::redo::handler->start_writing(flushed_lsn) !=
        ib::redo::Status::SUCCESS) {
      return srv_init_abort(DB_ERROR);
    }
    pages_persistence->enable_checkpointing();

    ut_a(ib::redo::handler->peek_first_unassigned_lsn() == flushed_lsn);
    ut_a(ib::redo::handler->peek_first_nonpersisted_lsn() == flushed_lsn);

    if (const auto err =
            srv_undo_create_implicit_tablespaces_with_empty_structure();
        err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

    {
      const auto size = srv_sys_space.get_sum_of_expected_sizes_in_pages();
      ut_ad_eq(size, fil_space_get_size(TRX_SYS_SPACE));
      /* Initialize header for system tablespace */
      mtr_t mtr;
      mtr_start(&mtr);

      const auto ret = fsp_header_init(TRX_SYS_SPACE, size, &mtr);

      mtr_commit(&mtr);
      if (!ret) {
        return srv_init_abort(DB_ERROR);
      }
    }

    /* To maintain backward compatibility we create only
    the first rollback segment before the double write buffer.
    All the remaining rollback segments will be created later,
    after the double write buffers haves been created. */
    trx_sys_create_sys_pages();

    trx_purge_sys_mem_create();

    const auto purge_queue = trx_sys_init_at_db_start();

    /* The purge system needs to create the purge view and
    therefore requires that the trx_sys is initialized. */

    trx_purge_sys_initialize(srv_threads.m_purge_workers_n, purge_queue);

    if (const auto err = dict_create(); err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

    srv_create_sdi_indexes();

    /* We always create the legacy double write buffer to preserve the
    expected page ordering of the system tablespace.
    FIXME: Try and remove this requirement. */
    if (const auto err = dblwr::v1::create(); err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

  } else {
    /* Load the reserved boundaries of the legacy dblwr buffer, this is
    required to check for stray reads and writes trying to access this
    reserved region in the sys tablespace.
    FIXME: Try and remove this requirement. */
    if (const auto err = dblwr::v1::init(); err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

    /* Invalidate the buffer pool to ensure that we reread
    the page that we read above, during recovery.
    Note that this is not as heavy weight as it seems. At
    this point there will be only ONE page in the buf_LRU
    and there must be no page in the buf_flush list. */
    buf_pool_invalidate();

    auto recovered_lsn = flushed_lsn;
    /* Do the recovery and persist all the changes to tablespace pages found in
    REDO. Also create undo number to space id mapping for UNDO tablespaces. */
    {
      const auto space_ids = pages_persistence->recover_pages(recovered_lsn);
      if (!space_ids.has_value()) {
        return srv_init_abort(DB_ERROR);
      }

      /* Tablespaces should have been found */
      ut_a(!space_ids.value().empty());

      undo_truncate::num2id_map =
          ut::make_unique<undo_truncate::Undo_num2id_map>(space_ids.value());
    }

    if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
      /* We need to start log threads now, because will write recovered updates
      to actual B-tree pages during pages_persistence->recover_tables() -
      an operation on pages which itself needs to be redo logged.
      Even in srv_read_only_mode we call start_writing(), because many places
      in innodb assume that log_start() was called - in particular, that we can
      call peek_unassigned_lsn() in buf_page_lsn_check after reads. However in
      srv_read_only_mode, the background threads will not be started. */
      const auto start_result = ib::redo::handler->start_writing(recovered_lsn);
      if (start_result != ib::redo::Status::SUCCESS) {
        ut_ad(start_result == ib::redo::Status::WRITE_ERROR);
        return srv_init_abort(DB_ERROR);
      }

      if (!srv_read_only_mode &&
          srv_recovered_redo_block_was_encrypted != srv_redo_log_encrypt) {
        /* Seal the recovered partial block using its physical encryption mode
        before dictionary startup can generate new redo. */
        const bool configured_encryption = srv_redo_log_encrypt;

        srv_redo_log_encrypt = srv_recovered_redo_block_was_encrypted;
        log_encryption_write_dummy_barrier();
        srv_redo_log_encrypt = configured_encryption;
      }

      /* Validate a few system page types that were left uninitialized
      by older versions of MySQL. */
      verify_page_type({IBUF_SPACE_ID, FSP_IBUF_HEADER_PAGE_NO},
                       FIL_PAGE_TYPE_SYS);
      verify_page_type({TRX_SYS_SPACE, FSP_FIRST_RSEG_PAGE_NO},
                       FIL_PAGE_TYPE_SYS);
      verify_page_type({TRX_SYS_SPACE, TRX_SYS_PAGE_NO}, FIL_PAGE_TYPE_TRX_SYS);
      verify_page_type({TRX_SYS_SPACE, FSP_DICT_HDR_PAGE_NO},
                       FIL_PAGE_TYPE_SYS);
    }

    /* We should not start checkpointer before persisted metadata is stored. */
    ut_a(!log_checkpointer_is_active());

    /* We have to call dict_boot() either before setting recv_lsn_checks_on,
    or after ib::redo::handler->start_writing(), as it will read pages from
    disc. dict_boot() also initializes the change buffer which is needed for any
    disk i/o. We need to call dict_boot() so pages_persistence->recover_tables()
    can access dict_table_t and dict_index_t objects. */
    if (const auto err = dict_boot(); err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

    DBUG_EXECUTE_IF("log_first_rec_group_test", {
      const lsn_t end_lsn = mtr_commit_mlog_test();
      log_write_up_to(*log_sys, end_lsn, true);
      DBUG_SUICIDE();
    });

    if (pages_persistence->recover_tables() !=
        ib::fil::Pages_persistence_interface::Status::SUCCESS) {
      return srv_init_abort(DB_ERROR);
    }

    srv_recovery_crash(4);

    if (!srv_read_only_mode) {
      pages_persistence->enable_checkpointing();
    }

    /* for a restored database we reset creator for log. To do this we stop
    background log processing for unknown reason, possibly just in case. */
    if (recv_sys->is_cloned_db || recv_sys->is_meb_db) {
      ut_a(!recv_sys->is_cloned_db ||
           ib::redo::handler->get_capabilities().supports_clone);
      ut_a(!recv_sys->is_meb_db ||
           ib::redo::handler->get_capabilities().supports_meb);
      buf_pool_wait_for_no_pending_io();

      ut_a(!srv_read_only_mode);
      ib::redo::must_succeed(ib::redo::handler->persist_available(),
                             UT_LOCATION_HERE);
      const auto end_lsn = ib::redo::handler->peek_first_unassigned_lsn();
      pages_persistence->disable_checkpointing();
      ib::redo::handler->stop_writing();

      ut_ad(buf_pool_pending_io_reads_count() == 0);

      if (const auto err = log_files_reset_creator_and_set_full(*log_sys);
          err != DB_SUCCESS) {
        return srv_init_abort(err);
      }
      ut_a(end_lsn == buf_flush_list_added->smallest_not_added_lsn());
      if (ib::redo::handler->start_writing(end_lsn) !=
          ib::redo::Status::SUCCESS) {
        return srv_init_abort(DB_ERROR);
      }
      pages_persistence->enable_checkpointing();
    }

    /* We will add sizes of all newly created files. It may happen that old size
    plus the sum_of_new_sizes is not the actual size, because some files might
    be missing or truncated. We will detect such situation later. */
    if (srv_sys_space.get_sum_of_new_sizes_in_pages() > 0) {
      ut_a(!srv_read_only_mode);

      /* New data file(s) were added */
      mtr_t mtr;
      mtr_start(&mtr);

      fsp_header_inc_size(TRX_SYS_SPACE,
                          srv_sys_space.get_sum_of_new_sizes_in_pages(), &mtr);

      mtr_commit(&mtr);

      /* Immediately write the log record about
      increased tablespace size to disk, so that it
      is durable even if mysqld would crash
      quickly */

      ib::redo::must_persist_all(UT_LOCATION_HERE);
    }

    /* Open implicit UNDO tablespaces */
    if (const auto err = srv_undo_tablespaces_open();
        err != DB_SUCCESS && srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {
      return srv_init_abort(err);
    }

    /* Clear the mapping as it is not needed anymore. */
    undo_truncate::num2id_map.reset();

    trx_purge_sys_mem_create();

    /* The purge system needs to create the purge view and
    therefore requires that the trx_sys is initialized. */
    const auto purge_queue = trx_sys_init_at_db_start();

    /* The purge system needs to create the purge view and
    therefore requires that the trx_sys and trx lists were
    initialized in trx_sys_init_at_db_start(). */
    trx_purge_sys_initialize(srv_threads.m_purge_workers_n, purge_queue);
  }

  /* Open temp-tablespace and keep it open until shutdown. */
  if (const auto err = srv_open_tmp_tablespace(srv_tmp_space);
      err != DB_SUCCESS) {
    return srv_init_abort(err);
  }

  if (const auto err = ibt::open_or_create(create_new_db); err != DB_SUCCESS) {
    return srv_init_abort(err);
  }

  /* Here the double write buffer has already been created and so
  any new rollback segments will be allocated after the double
  write buffer. The default segment should already exist.
  We create the new segments only if it's a new database or
  the database was shutdown cleanly. */

  /* Note: When creating the extra rollback segments during an upgrade
  we violate the latching order, even if the change buffer is empty.
  We make an exception in sync0sync.cc and check srv_is_being_started
  for that violation. It cannot create a deadlock because we are still
  running in single threaded mode essentially. Only the IO threads
  should be running at this stage. */

  ut_a(srv_rollback_segments > 0);
  ut_a_le(srv_rollback_segments, FSP_MAX_ROLLBACK_SEGMENTS);

  /* Make sure there are enough rollback segments in each tablespace
  and that each rollback segment has an associated memory object.
  If any of these rollback segments contain undo logs, load them into
  the purge queue */
  if (!trx_rseg_adjust_rollback_segments(srv_rollback_segments)) {
    return srv_init_abort(DB_ERROR);
  }

  /* Any undo tablespaces under construction are now fully built
  with all needed rsegs. */
  srv_undo_mark_all_tablespaces_usable();

  /* Now that all rsegs are ready for use, make them active. */
  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);
  for (auto undo_space : undo_truncate::spaces->m_spaces) {
    if (!undo_space->is_empty()) {
      undo_space->set_active();
    }
  }
  undo_truncate::spaces->s_unlock();

  /* Undo Tablespaces and Rollback Segments are ready. */
  srv_startup_is_before_trx_rollback_phase = false;

  if (!srv_read_only_mode) {
    if (create_new_db) {
      srv_buffer_pool_load_at_startup = false;
    }

    /* Create the thread which watches the timeouts
    for lock waits */
    srv_threads.m_lock_wait_timeout = os_thread_create(
        srv_lock_timeout_thread_key, 0, lock_wait_timeout_thread);

    srv_threads.m_lock_wait_timeout.start();

    /* Create the thread which warns of long semaphore waits */
    srv_threads.m_error_monitor = os_thread_create(srv_error_monitor_thread_key,
                                                   0, srv_error_monitor_thread);

    srv_threads.m_error_monitor.start();

    /* Create the thread which prints InnoDB monitor info */
    srv_threads.m_monitor =
        os_thread_create(srv_monitor_thread_key, 0, srv_monitor_thread);

    srv_threads.m_monitor.start();
  }

  srv_sys_tablespaces_open = true;

  /* Rotate the encryption key for recovery. It's because
  server could crash in middle of key rotation. Some tablespace
  didn't complete key rotation. Here, we will resume the
  rotation. */
  if (!srv_read_only_mode && !create_new_db &&
      srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
    size_t fail_count = fil_encryption_rotate();
    if (fail_count > 0) {
      ib::info(ER_IB_MSG_1146)
          << "During recovery, fil_encryption_rotate() failed for "
          << fail_count << " tablespace(s).";
    }
  }

  srv_is_being_started = false;

  ut_a(trx_purge_state() == PURGE_STATE_INIT);

  /* wake main loop of page cleaner up */
  os_event_set(buf_flush_event);

  const auto sum_of_data_file_sizes_in_pages =
      fil_space_get_size(TRX_SYS_SPACE);

  const auto tablespace_size_in_header = fsp_header_get_tablespace_size();

  if (!srv_read_only_mode && !srv_sys_space.can_auto_extend_last_file() &&
      sum_of_data_file_sizes_in_pages != tablespace_size_in_header) {
    ib::error(ER_IB_MSG_1147, ulong{tablespace_size_in_header},
              ulong{sum_of_data_file_sizes_in_pages});

    if (srv_force_recovery == 0 &&
        sum_of_data_file_sizes_in_pages < tablespace_size_in_header) {
      /* This is a fatal error, the tail of a tablespace is
      missing */

      ib::error(ER_IB_MSG_1148);

      return srv_init_abort(DB_ERROR);
    }
  }

  if (!srv_read_only_mode && srv_sys_space.can_auto_extend_last_file() &&
      sum_of_data_file_sizes_in_pages < tablespace_size_in_header) {
    ib::error(ER_IB_MSG_1149, ulong{tablespace_size_in_header},
              ulong{sum_of_data_file_sizes_in_pages});

    if (srv_force_recovery == 0) {
      ib::error(ER_IB_MSG_1150);

      return srv_init_abort(DB_ERROR);
    }
  }

  /* Finish clone files recovery. */
  clone_files_recovery(true);

  ib::info(ER_IB_MSG_1151, INNODB_VERSION_STR,
           ulonglong{srv_force_recovery < SRV_FORCE_NO_LOG_REDO
                         ? ib::redo::handler->peek_first_unassigned_lsn()
                         : 0});

  return DB_SUCCESS;
}

/** Applier of dynamic metadata */
struct metadata_applier {
  /** Default constructor */
  metadata_applier() = default;
  /** Visitor.
  @param[in]      table   table to visit */
  void operator()(dict_table_t *table) const {
    ut_ad(dict_sys->dynamic_metadata != nullptr);
    uint64_t autoinc = table->autoinc;
    dict_table_load_dynamic_metadata(table);
    /* For those tables which were not opened by
    ha_innobase::open() and not initialized by
    innobase_initialize_autoinc(), the next counter should be
    advanced properly */
    if (autoinc != table->autoinc && table->autoinc != ~0ULL) {
      ++table->autoinc;
    }
  }
};

/** Apply the dynamic metadata to all tables */
static void apply_dynamic_metadata() {
  const metadata_applier applier;

  dict_sys->for_each_table(applier);
}

/** On a restart, initialize the remaining InnoDB subsystems so that
any tables (including data dictionary tables) can be accessed. */
void srv_dict_recover_on_restart() {
  /* Resurrect locks for dictionary transactions */
  trx_resurrect_locks(false);

  /* Roll back any recovered data dictionary transactions, so
  that the data dictionary tables will be free of any locks.
  The data dictionary latch should guarantee that there is at
  most one data dictionary transaction active at a time. */
  if (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO && trx_sys_need_rollback()) {
    trx_rollback_or_clean_recovered(false);
  }

  /* Resurrect locks for non-dictionary transactions only after rolling back all
  dictionary transactions. This is required as of today since we read
  uncommitted data while constructing table object in dd_table_open_on_id_low.
  This is done only while looking for the DD space object
  client->acquire_uncached_uncommitted<dd::Tablespace>().

  TODO-1: dd_table_open_on_id_low : Reading uncommitted data doesn't seem
  correct and needs to be analyzed and possibly fixed.

  Till that time we let all DD transactions to rollback to avoid reading dirty
  data from incomplete DDL commands while resurrecting locks. It essentially
  fixes two independent issues.

  1. Not able to resurrect table locks for uncommitted transaction.

  2. Not able to load innodb dict_* object for the table involved in the DDL.
     This could result in much more serious issue when binary log is enabled
     and crash happens after the transaction is prepared. Currently in binlog
     transaction recovery path no session THD is created and we rely on cached
     dict_* object to find out if a table is dropped. If the dict_table_t
     object is not already loaded, the table is considered dropped and undo
     apply is skipped. This would further result in uncommitted but prepared
     transaction data being committed and persisted.

  TODO-2: Have session (THD) while doing binary log recovery. The lack of
  THD seems not correct since rollback requires DD metadata. This alone would
  have prevented transaction inconsistency between innodb and binlog even if we
  failed to resurrect the table locks.
  binlog_recover->ha_recover->xarecover_handlerton->innobase_rollback_by_xid
  ->innobase_rollback_trx

  Note: The current work around fixes both issues but ideally should not be
  required if base issues [TODOs] are fixed. */
  trx_resurrect_locks(true);

  trx_clear_resurrected_table_ids();

  /* Do after all DD transactions recovery, to get consistent metadata */
  apply_dynamic_metadata();

  if (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE) {
    srv_sys_tablespaces_open = true;
  }
}

/** Start purge threads. During upgrade we start
purge threads early to apply purge. */
void srv_start_purge_threads() {
  /* Start purge threads only if they are not started earlier. */
  if (srv_start_state_is_set(SRV_START_STATE_PURGE)) {
    return;
  }

  srv_threads.m_purge_coordinator =
      os_thread_create(srv_purge_thread_key, 0, srv_purge_coordinator_thread);

  srv_threads.m_purge_workers[0] = srv_threads.m_purge_coordinator;

  /* We've already created the purge coordinator thread above. */
  for (size_t i = 1; i < srv_threads.m_purge_workers_n; ++i) {
    srv_threads.m_purge_workers[i] =
        os_thread_create(srv_worker_thread_key, i, srv_worker_thread);
  }

  for (size_t i = 0; i < srv_threads.m_purge_workers_n; ++i) {
    srv_threads.m_purge_workers[i].start();
  }

  srv_start_wait_for_purge_to_start();

  srv_start_state_set(SRV_START_STATE_PURGE);
}

/** Start up the InnoDB service threads which are independent of DDL recovery.
 */
void srv_start_threads() {
  if (!srv_read_only_mode) {
    /* Before 8.0, it was master thread that was doing periodical
    checkpoints (every 7s). Since 8.0, it is the log checkpointer
    thread, which is owned by log_sys, that is responsible for
    periodical checkpoints (every innodb_log_checkpoint_every ms).
    Note that the log checkpointer thread was created earlier and
    is already active, but the periodical checkpoints were disabled.
    Only the required checkpoints were allowed, which includes:
            - checkpoints because of too old last_checkpoint_lsn,
            - checkpoints explicitly requested (because of call to
              pages_persistence->request_sharp_checkpoint()).
    The reason was to make the situation more deterministic during
    the startup, because then:
            - it is easier to write mtr tests,
            - there are less possible flows - smaller risk of bug.
    Now we start allowing periodical checkpoints! Since now, it's
    hard to predict when checkpoints are written! */
    pages_persistence->enable_periodical_checkpoints();
  }

  srv_threads.m_buf_resize =
      os_thread_create(buf_resize_thread_key, 0, buf_resize_thread);

  srv_threads.m_buf_resize.start();

  if (srv_read_only_mode) {
    purge_sys->state = PURGE_STATE_DISABLED;
    return;
  }

  /* Create the master thread which does purge and other utility
  operations */
  srv_threads.m_master =
      os_thread_create(srv_master_thread_key, 0, srv_master_thread);

  srv_threads.m_master.start();

  if (srv_force_recovery == 0) {
    /* In the insert buffer we may have even bigger tablespace
    id's, because we may have dropped those tablespaces, but
    insert buffer merge has not had time to clean the records from
    the ibuf tree. */

    ibuf_update_max_tablespace_id();
  }

  /* Create the dict stats gathering thread */
  srv_threads.m_dict_stats =
      os_thread_create(dict_stats_thread_key, 0, dict_stats_thread);

  dict_stats_thread_init();

  srv_threads.m_dict_stats.start();

  /* Create the thread that will optimize the FTS sub-system. */
  fts_optimize_init();

  srv_start_state_set(SRV_START_STATE_STAT);
}

void srv_start_threads_after_ddl_recovery() {
  if (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO && trx_sys_need_rollback()) {
    /* Rollback all recovered transactions that are
    not in committed nor in XA PREPARE state. */
    srv_threads.m_trx_recovery_rollback = os_thread_create(
        trx_recovery_rollback_thread_key, 0, trx_recovery_rollback_thread);

    srv_threads.m_trx_recovery_rollback.start();
    /* Wait till shared MDL is taken by background thread for all tables,
    for which rollback is to be performed. */
    os_event_wait(recovery_lock_taken);
  }

  /* Start the buffer pool dump/load thread, which will access spaces thus
        must wait for DDL recovery */
  srv_threads.m_buf_dump =
      os_thread_create(buf_dump_thread_key, 0, buf_dump_thread);

  srv_threads.m_buf_dump.start();

  /* Resume unfinished (un)encryption process in background thread. */
  if (!ts_encrypt_ddl_records.empty()) {
    srv_threads.m_ts_alter_encrypt =
        os_thread_create(srv_ts_alter_encrypt_thread_key, 0,
                         fsp_init_resume_alter_encrypt_tablespace);

    mysql_mutex_lock(&resume_encryption_cond_m);
    srv_threads.m_ts_alter_encrypt.start();
    /* Wait till shared MDL is taken by background thread for all tablespaces,
    for which (un)encryption is to be rolled forward. */
    mysql_cond_wait(&resume_encryption_cond, &resume_encryption_cond_m);
    mysql_mutex_unlock(&resume_encryption_cond_m);
  }

  /* Start and consume all GTIDs for recovered transactions. */
  auto &gtid_persistor = clone_sys->get_gtid_persistor();
  gtid_persistor.start();

  DBUG_EXECUTE_IF("crash_before_purge_thread", DBUG_SUICIDE(););

  /* Now the InnoDB Metadata and file system should be consistent.
  Start the Purge thread */
  srv_start_purge_threads();

  /* If recovered, should do write back the dynamic metadata. */
  dict_persist_to_dd_table_buffer();
}

/** Set srv_shutdown_state to a given state and validate change is proper.
@remarks This function is used only from the main thread, and only during
startup or shutdown.
@param[in]  new_state   new state to set */
static void srv_shutdown_set_state(srv_shutdown_t new_state) {
  ut_a(static_cast<int>(srv_shutdown_state.load()) + 1 ==
       static_cast<int>(new_state));

  srv_shutdown_state.store(new_state);
}

static void srv_shutdown_cleanup_and_master_stop();

bool srv_shutdown_waits_for_rollback_of_recovered_transactions() {
  return (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO && srv_fast_shutdown == 0);
}

/** Shut down all InnoDB background tasks that may look up objects in
the data dictionary. */
void srv_pre_dd_shutdown() {
  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_NONE);

  /* Warn and wait if there are still some query threads alive.
  If all is correct, then all user threads should already be gone,
  because before clean_up() -> srv_pre_dd_shutdown() is called,
  we are joining signal_hand thread, which before exiting waits
  for all connections to be closed (close_connections()). */
  for (size_t count = 0; count < 10; ++count) {
    const auto threads_count = srv_conc_get_active_threads();
    if (threads_count == 0) {
      break;
    }
    ib::warn(ER_IB_MSG_1154, threads_count);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  /* Crash if some query threads are still alive. */
  ut_a(srv_conc_get_active_threads() == 0);

  ut_a(!srv_thread_is_active(srv_threads.m_recv_writer));

  /* Avoid fast shutdown, if redo logging is disabled. Otherwise, we won't be
  able to recover. */
  if (mtr_t::s_logging.is_disabled() && srv_fast_shutdown == 2) {
    ib::warn(ER_IB_WRN_FAST_SHUTDOWN_REDO_DISABLED);
    srv_fast_shutdown = 1;
  }

  /* Stop service for persisting GTID */
  auto &gtid_persistor = clone_sys->get_gtid_persistor();
  gtid_persistor.stop();

  if (srv_read_only_mode) {
    /* Check that goal of SRV_SHUTDOWN_RECOVERY_ROLLBACK is reached:
    1. In read-only mode, no rollbacks should be executed.
    2. The trx_recovery_rollback thread should not be started. */
    ut_ad(trx_sys_recovered_active_trxs_count() == 0);
    ut_a(!srv_thread_is_active(srv_threads.m_trx_recovery_rollback));

    /* Check the goal of SRV_SHUTDOWN_PRE_DD_AND_SYSTEM_TRANSACTIONS,
    the following threads should not be started in read-only mode: */
    ut_a(!srv_thread_is_active(srv_threads.m_dict_stats));
    ut_a(!srv_thread_is_active(srv_threads.m_fts_optimize));
    ut_a(!srv_thread_is_active(srv_threads.m_ts_alter_encrypt));

    /* In read-only mode, there is no master thread. */
    ut_a(!srv_thread_is_active(srv_threads.m_master));

    /* In read-only mode, no purge should be done, so goal of the
    SRV_SHUTDOWN_PURGE is already satisfied (no purge threads). */
    ut_a(!srv_purge_threads_active());

    /* Advance quickly through all states to SRV_SHUTDOWN_DD. */
    srv_shutdown_set_state(SRV_SHUTDOWN_RECOVERY_ROLLBACK);
    srv_shutdown_set_state(SRV_SHUTDOWN_PRE_DD_AND_SYSTEM_TRANSACTIONS);
    srv_shutdown_set_state(SRV_SHUTDOWN_PURGE);
    srv_shutdown_set_state(SRV_SHUTDOWN_DD);
    return;
  }

  srv_shutdown_set_state(SRV_SHUTDOWN_RECOVERY_ROLLBACK);

  if (srv_shutdown_waits_for_rollback_of_recovered_transactions()) {
    /* We need to wait for rollback of recovered transactions. */
    for (uint32_t count = 0;; ++count) {
      /* Should not loop and wait if rollback thread isn't there. */
      if (!srv_thread_is_active(srv_threads.m_trx_recovery_rollback)) {
        break;
      }
      const auto total_trx = trx_sys_recovered_active_trxs_count();
      if (total_trx == 0) {
        break;
      }
      if (count >= SHUTDOWN_SLEEP_ROUNDS) {
        ib::info(ER_IB_MSG_1249, total_trx);
        count = 0;
      }
      std::this_thread::sleep_for(
          std::chrono::microseconds(SHUTDOWN_SLEEP_TIME_US));
    }
  }

  if (srv_thread_is_active(srv_threads.m_trx_recovery_rollback)) {
    /* We should wait until rollback after recovery end to avoid
    adding more for purge and to avoid touching transaction objects
    since this point. */
    srv_threads.m_trx_recovery_rollback.wait();
  }

  srv_shutdown_set_state(SRV_SHUTDOWN_PRE_DD_AND_SYSTEM_TRANSACTIONS);

  if (srv_start_state_is_set(SRV_START_STATE_STAT)) {
    fts_optimize_shutdown();
    dict_stats_shutdown();
    dict_stats_thread_deinit();
  }
  ut_a(!srv_thread_is_active(srv_threads.m_fts_optimize));
  ut_a(!srv_thread_is_active(srv_threads.m_dict_stats));

  for (uint32_t count = 1; srv_thread_is_active(srv_threads.m_ts_alter_encrypt);
       ++count) {
    if (count % SHUTDOWN_SLEEP_ROUNDS == 0) {
      ib::info(ER_IB_MSG_WAIT_FOR_ENCRYPT_THREAD);
    }
    std::this_thread::sleep_for(
        std::chrono::microseconds(SHUTDOWN_SLEEP_TIME_US));
  }

  /* Wait until the master thread exits its main loop and notices that:
    - it should do shutdown-cleanup,
    - and still is allowed to access DD objects. */
  if (srv_thread_is_active(srv_threads.m_master)) {
    srv_wake_master_thread();
    os_event_wait(srv_threads.m_master_ready_for_dd_shutdown);
  }

  /* Since this point we do not expect accesses to DD coming from InnoDB. */
  ut_d(trx_sys_before_pre_dd_shutdown_validate());

  srv_shutdown_set_state(SRV_SHUTDOWN_PURGE);

  for (uint32_t count = 1; srv_purge_threads_active(); ++count) {
    srv_purge_wakeup();
    if (count % SHUTDOWN_SLEEP_ROUNDS == 0) {
      ib::info(ER_IB_MSG_1152);
    }
    std::this_thread::sleep_for(
        std::chrono::microseconds(SHUTDOWN_SLEEP_TIME_US));
  }
  switch (trx_purge_state()) {
    case PURGE_STATE_INIT:
    case PURGE_STATE_EXIT:
    case PURGE_STATE_DISABLED:
      srv_start_state &= ~SRV_START_STATE_PURGE;
      break;
    case PURGE_STATE_RUN:
    case PURGE_STATE_STOP:
      ut_d(ut_error);
  }

  /* After this phase plugins are asked to be shut down, in which case they
  will be marked as DELETED. Note: we cannot leave any transaction in the THD,
  because the mechanism which cleans resources in THD would not be able to
  unregister those transactions from mysql_trx_list, because the handler
  of close_connection in InnoDB handlerton would not be called, because
  InnoDB has already been marked as DELETED. You should close your thread
  here, in the srv_pre_dd_shutdown, if it might do lookups in DD objects.
  No other transactions should be useful, so for sake of simplicity we
  require to have no transactions at all here, except transactions:
    - with state = TRX_STATE_PREPARED,
    - with state = TRX_STATE_ACTIVE and with is_recovered == true */

  ut_d(trx_sys_after_pre_dd_shutdown_validate());

  srv_shutdown_set_state(SRV_SHUTDOWN_DD);

  DBUG_EXECUTE_IF("wait_for_threads_in_pre_dd_shutdown",
                  srv_shutdown_cleanup_and_master_stop(););
}

/** Shutdown background threads of InnoDB at the start of the shutdown phase.
Handles shutdown phases: SRV_SHUTDOWN_CLEANUP and SRV_SHUTDOWN_MASTER_STOP. */
static void srv_shutdown_cleanup_and_master_stop() {
  DBUG_EXECUTE_IF("threads_wait_on_cleanup",
                  os_event_set(srv_threads.m_shutdown_cleanup_dbg););

  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_DD);

  srv_shutdown_set_state(SRV_SHUTDOWN_CLEANUP);

  const srv_shutdown_t max_wait_on_state{SRV_SHUTDOWN_MASTER_STOP};

  uint32_t count = 0;

  for (;;) {
    /* Print messages every 60 seconds when we are waiting for any
    of those threads to exit. */
    bool print;
    if (count >= SHUTDOWN_SLEEP_ROUNDS) {
      print = true;
      count = 0;
    } else {
      print = false;
    }

    size_t active_found = 0;
    for (const auto &thread_info : threads_to_stop) {
      ut_a(thread_info.m_wait_on_state <= max_wait_on_state);
      if (thread_info.m_wait_on_state == srv_shutdown_state.load() &&
          srv_thread_is_active(thread_info.m_thread)) {
        ++active_found;
        if (print) {
          ib::info(ER_IB_MSG_1248, thread_info.m_name);
        }
        thread_info.m_notify();
      }
    }

    if (active_found == 0) {
      if (srv_shutdown_state.load() == max_wait_on_state) {
        break;
      }
      srv_shutdown_set_state(static_cast<srv_shutdown_t>(
          static_cast<int>(srv_shutdown_state.load()) + 1));
    }

    std::this_thread::sleep_for(
        std::chrono::microseconds(SHUTDOWN_SLEEP_TIME_US));
    ++count;
  }

  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_MASTER_STOP);

  ut_d(trx_sys_after_background_threads_shutdown_validate());
}

/** Waits for page cleaners exit. */
static void srv_shutdown_page_cleaners() {
  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_MASTER_STOP);
  ut_a(!srv_master_thread_is_active());

  srv_shutdown_set_state(SRV_SHUTDOWN_FLUSH_PHASE);

  buf_pool_wait_for_no_pending_io();

  /* At this point only page_cleaner should be active. We wait
  here to let it complete the flushing of the buffer pools
  before proceeding further. */

  for (uint32_t count = 0; buf_flush_page_cleaner_is_active(); ++count) {
    if (count >= SHUTDOWN_SLEEP_ROUNDS) {
      ib::info(ER_IB_MSG_1251);
      count = 0;
    }
    os_event_set(buf_flush_event);
    std::this_thread::sleep_for(
        std::chrono::microseconds(SHUTDOWN_SLEEP_TIME_US));
  }

  ut_ad(buf_pool_pending_io_reads_count() == 0);
  ut_ad(buf_pool_pending_io_writes_count() == 0);
}

/** Closes redo log. If this is not fast shutdown, it forces to write a
checkpoint which should be written for logically empty redo log. Note that we
forced to flush all dirty pages in the last stages of page cleaners activity
(unless it was fast shutdown). After checkpoint is written, the flushed_lsn is
updated within header of the system tablespace. This is lsn of the last clean
shutdown. */
static lsn_t srv_shutdown_log() {
  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_FLUSH_PHASE);
  ut_a(!buf_flush_page_cleaner_is_active());
  ut_ad(buf_pool_pending_io_reads_count() == 0);
  ut_ad(buf_pool_pending_io_writes_count() == 0);

  lsn_t lsn{srv_force_recovery < SRV_FORCE_NO_LOG_REDO
                ? ib::redo::handler->peek_first_unassigned_lsn()
                : 0};
  const auto persist_available_and_stop = [&lsn] {
    ib::redo::must_succeed(ib::redo::handler->persist_available(),
                           UT_LOCATION_HERE);
    pages_persistence->disable_checkpointing();
    lsn = ib::redo::handler->peek_first_unassigned_lsn();
    ut_a(lsn == ib::redo::handler->peek_first_nonpersisted_lsn());
    ib::redo::handler->stop_writing();
  };

  if (srv_fast_shutdown == 2) {
    if (!srv_read_only_mode) {
      ib::info(ER_IB_MSG_1253);

      /* In this fastest shutdown we do not flush the
      buffer pool:

      it is essentially a 'crash' of the InnoDB server.
      Make sure that the log is all flushed to disk, so
      that we can recover all committed transactions in
      a crash recovery. We must not write the lsn stamps
      to the data files, since at a startup InnoDB deduces
      from the stamps if the previous shutdown was clean. */
      persist_available_and_stop();
    }

    /* No redo log might be generated since now. */
    log_background_threads_inactive_validate();

    srv_shutdown_set_state(SRV_SHUTDOWN_LAST_PHASE);

    return lsn;
  }

  if (!srv_read_only_mode) {
    pages_persistence->request_sharp_checkpoint();
    ut_ad_eq(pages_persistence->get_checkpoint_lsn(),
             ib::redo::handler->peek_first_unassigned_lsn());
    persist_available_and_stop();
  }

  /* No redo log might be generated since now. */
  log_background_threads_inactive_validate();
  buf_assert_all_are_replaceable();

  if (!srv_read_only_mode) {
    /* Redo log has been flushed at the log_flusher's exit. */
    fil_flush_file_spaces();
  }

  srv_shutdown_set_state(SRV_SHUTDOWN_LAST_PHASE);

  /* Validate lsn and write it down. */
  ut_a(log_is_data_lsn(lsn) || srv_force_recovery >= SRV_FORCE_NO_LOG_REDO);

  ut_a(lsn == pages_persistence->get_checkpoint_lsn() ||
       srv_force_recovery >= SRV_FORCE_NO_LOG_REDO);

  if (!srv_read_only_mode) {
    ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);

    auto err = fil_write_flushed_lsn(lsn);
    ut_a(err == DB_SUCCESS);
  }

  buf_assert_all_are_replaceable();

  return (lsn);
}

/** Copy all remaining data and shutdown archiver threads. */
static void srv_shutdown_arch() {
  uint32_t count = 0;

  while (arch_wake_threads()) {
    ++count;
    std::this_thread::sleep_for(
        std::chrono::microseconds(SHUTDOWN_SLEEP_TIME_US));

    if (count > SHUTDOWN_SLEEP_ROUNDS) {
      ib::info(ER_IB_MSG_1246);
      count = 0;
    }
  }
}

void srv_thread_delay_cleanup_if_needed(bool wait_for_signal) {
  DBUG_EXECUTE_IF("threads_wait_on_cleanup", {
    if (wait_for_signal) {
      os_event_wait(srv_threads.m_shutdown_cleanup_dbg);
    } else {
      /* In some cases we cannot wait for the signal, because we would otherwise
      never reach the end of pre_dd_shutdown, because pre_dd_shutdown is waiting
      for this thread before it ends. Then we would never reach shutdown phase
      in which the signal becomes signalled. Still we would like to have a way
      to detect situation in which someone broke the code and pre_dd_shutdown
      no longer waits for this thread. */
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
}

/** Shut down the InnoDB database. */
void srv_shutdown() {
  ut_d(trx_sys_after_pre_dd_shutdown_validate());

  /* Need to revert partition file names if minor upgrade fails. */
  uint data_version = MYSQL_VERSION_ID;

  if (!fsp_header_dict_get_server_version(&data_version) &&
      data_version != MYSQL_VERSION_ID) {
    srv_downgrade_partition_files = true;
  }

  ib::info(ER_IB_MSG_1247);

  ut_a(!srv_is_being_started);

  /* Ensure threads below have been stopped. */
  const auto threads_stopped_before_shutdown = {
      std::cref(srv_threads.m_purge_coordinator),
      std::cref(srv_threads.m_ts_alter_encrypt),
      std::cref(srv_threads.m_fts_optimize),
      std::cref(srv_threads.m_recv_writer),
      std::cref(srv_threads.m_dict_stats)};

  for (const auto &thread : threads_stopped_before_shutdown) {
    ut_a(!srv_thread_is_active(thread));
  }

#ifdef UNIV_DEBUG
  /* In DEBUG we might be testing scenario in which we forced to
  call srv_shutdown_cleanup_and_master_stop() to stop all threads
  at the end of the srv_pre_dd_shutdown(). */
  DBUG_EXECUTE_IF("wait_for_threads_in_pre_dd_shutdown",
                  srv_shutdown_state.store(SRV_SHUTDOWN_DD););
#endif /* UNIV_DEBUG */

  /* The SRV_SHUTDOWN_DD state was set during pre_dd_shutdown phase. */
  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_DD);

  /* Write dynamic metadata to DD buffer table. */
  dict_persist_to_dd_table_buffer();

  /* 0. Stop remaining background threads except:
    - page-cleaners - we are shutting down page cleaners in step 1
    - redo-log-threads - these need to be shutdown after page cleaners,
    - archiver threads - these need to be shutdown after redo threads.
  After this call the state of shutdown is advanced to SRV_SHUTDOWN_MASTER_STOP.
  */
  srv_shutdown_cleanup_and_master_stop();

  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_MASTER_STOP);

  /* Check again and write dynamic metadata to DD buffer table. Ideally we
  would not have dynamic metadata written so late in shutdown phase but
  currently we have certain operations done in master thread which could
  generate metadata. It is safe to check and write it here before we flush
  buffer pool to disk. */
  dict_persist_to_dd_table_buffer();

  /* The steps 1-4 is the real InnoDB shutdown.
  All before was to stop activity which could produce new changes.
  All after is just cleaning up (freeing memory). */

  /* 1. Flush the buffer pool to disk. */
  srv_shutdown_page_cleaners();

  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_FLUSH_PHASE);

  /* 2. Write the current lsn to the tablespace header(s). */
  const lsn_t shutdown_lsn = srv_shutdown_log();

  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_LAST_PHASE);

  /* 3. Close all opened files and delete System Temporary space files. */
  ibt::close_files();
  fil_close_all_files();

  /* Call reset here because Tablespace_scanning destructor calls
  Tablespace_scanning::rename_partition_files() in case of failed upgrade
  which logs the info in error log. For which error log mechanism should
  still be initialized */
  tablespace_scanning.reset(nullptr);

  if (srv_monitor_file) {
    fclose(srv_monitor_file);
  }
  if (srv_misc_tmpfile) {
    fclose(srv_misc_tmpfile);
  }
  srv_delete_temporary_space_files(srv_tmp_space);

  /* 4. Copy all log data to archive and stop archiver threads. */
  srv_shutdown_arch();

  /* This is to preserve the old style, we should finally get rid of the call
  here. For that, we need to ensure we have already effectively closed all
  threads. */
  srv_shutdown_exit_threads();

  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS);
  ut_ad(!os_thread_any_active());

  /* 5. Free all the resources acquired by InnoDB (mutexes, events, memory). */
  ibt::delete_pool_manager();

  if (srv_monitor_file) {
    srv_monitor_file = nullptr;
    if (srv_monitor_file_name) {
      unlink(srv_monitor_file_name);
      ut::free(srv_monitor_file_name);
    }
    mutex_free(&srv_monitor_file_mutex);
  }

  if (srv_misc_tmpfile) {
    srv_misc_tmpfile = nullptr;
    mutex_free(&srv_misc_tmpfile_mutex);
  }

  /* This must be disabled before closing the buffer pool
  and closing the data dictionary.  */
  btr_search_disable();

  ibuf_close();
  ddl_log_close();
  delete ib::redo::handler;
  pages_persistence->deinit();
  recv_sys_close();
  trx_sys_close();
  lock_sys_close();
  trx_pool_close();

  dict_close();
  dict_persist_close();
  undo_truncate_spaces_deinit();
  os_aio_free();
  que_close();
  row_mysql_close();
  srv_free();
  fil_close();
  pars_close();

  pars_lexer_close();
  buf_pool_free_all();

  /* 6. Free the thread management resources. */
  clone_free();
  arch_free();

  dblwr::close();
  os_thread_close();

  /* 6. Free the synchronisation infrastructure. */
  sync_check_close();

  ib::info(ER_IB_MSG_1155, ulonglong{shutdown_lsn});

  srv_start_has_been_called = false;
  srv_start_state = SRV_START_STATE_NONE;
}

void srv_get_encryption_data_filename(dict_table_t *table, char *filename,
                                      ulint max_len) {
  /* Make sure the data_dir_path is set. */
  dd_get_and_save_data_dir_path<dd::Table>(table, nullptr, false);

  std::string path = dict_table_get_datadir(table);

  auto filepath = Fil_path::make(path, table->name.m_name, CFP, true);

  size_t len = strlen(filepath);
  ut_a(max_len >= len);

  strcpy(filename, filepath);

  ut::free(filepath);
}
