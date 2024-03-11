/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.
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

#include "my_dbug.h"

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
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0buf.h"
#include "log0chkp.h"
#include "log0recv.h"
#include "log0write.h"
#include "mem0mem.h"
#include "mtr0mtr.h"

#include "my_dbug.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_stage.h"
#include "mysqld.h"

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

#include <zlib.h>

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
#include "ut0new.h"

/** fil_space_t::flags for hard-coded tablespaces */
extern uint32_t predefined_flags;

/** true if a raw partition is in use */
bool srv_start_raw_disk_in_use = false;

/** true if the server is being started */
bool srv_is_being_started = false;
/** true if SYS_TABLESPACES is available for lookups */
bool srv_sys_tablespaces_open = false;
/** true if the server is being started, before rolling back any
incomplete transactions */
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
static dberr_t srv_undo_tablespace_create(undo::Tablespace &undo_space) {
  pfs_os_file_t fh;
  bool ret;
  dberr_t err = DB_SUCCESS;
  char *file_name = undo_space.file_name();
  space_id_t space_id = undo_space.id();

  ut_a(!srv_read_only_mode);
  ut_a(!srv_force_recovery);

  os_file_create_subdirs_if_needed(file_name);

  /* Until this undo tablespace can become active, keep a truncate log
  file around so that if a crash happens it can be rebuilt at startup. */
  err = undo::start_logging(&undo_space);
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_1070, undo_space.log_file_name(),
              undo_space.space_name());
  }
  ut_ad(err == DB_SUCCESS);

  fh = os_file_create(innodb_data_file_key, file_name,
                      (srv_read_only_mode ? OS_FILE_OPEN : OS_FILE_CREATE) |
                          OS_FILE_ON_ERROR_NO_EXIT,
                      OS_DATA_FILE, srv_read_only_mode, &ret);

  if (ret == false) {
    std::ostringstream stmt;

    if (os_file_get_last_error(false) == OS_FILE_ALREADY_EXISTS) {
      stmt << " since '" << file_name << "' already exists.";
    } else {
      stmt << ". os_file_create() returned " << ret << ".";
    }

    ib::error(ER_IB_MSG_1214, undo_space.space_name(), stmt.str().c_str());

    err = DB_ERROR;
  } else {
    ut_a(!srv_read_only_mode);

    /* We created the data file and now write it full of zeros */
    undo_space.set_new();

    ib::info(ER_IB_MSG_1071, file_name);

    ulint size_mb = UNDO_INITIAL_SIZE >> 20;

    ib::info(ER_IB_MSG_1072, file_name, ulonglong{size_mb});

    ib::info(ER_IB_MSG_1073);

    ret = os_file_set_size(file_name, fh, 0, UNDO_INITIAL_SIZE, true);

    DBUG_EXECUTE_IF("ib_undo_tablespace_create_fail", ret = false;);

    if (!ret) {
      ib::info(ER_IB_MSG_1074, file_name);
      err = DB_OUT_OF_FILE_SPACE;
    }

    os_file_close(fh);

    /* Add this space to the list of undo tablespaces to
    construct by creating header pages. If an old undo
    tablespace needed fixup before it is upgraded,
    there is no need to construct it.*/
    if (undo::is_reserved(space_id)) {
      undo::add_space_to_construction_list(space_id);
    }
  }

  return (err);
}

/** Try to enable encryption of an undo log tablespace.
@param[in]      space_id        undo tablespace id
@return DB_SUCCESS if success */
static dberr_t srv_undo_tablespace_enable_encryption(space_id_t space_id) {
  dberr_t err;

  ut_ad(Encryption::check_keyring());

  /* Set the space flag.  The encryption metadata
  will be generated in fsp_header_init later. */
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
@param[in]      fh              file handle of undo log file
@param[in]      file_name       file name
@param[in]      space           undo tablespace
@return DB_SUCCESS if success */
static dberr_t srv_undo_tablespace_read_encryption(pfs_os_file_t fh,
                                                   const char *file_name,
                                                   fil_space_t *space) {
  IORequest request;
  ulint n_read = 0;
  size_t page_size = UNIV_PAGE_SIZE_MAX;
  dberr_t err = DB_ERROR;

  /* Align the memory for a possible read from a raw device */
  byte *first_page = static_cast<byte *>(
      ut::aligned_alloc(UNIV_PAGE_SIZE_MAX, UNIV_PAGE_SIZE));

  /* Don't want unnecessary complaints about partial reads. */
  request.disable_partial_io_warnings();

  err = os_file_read_no_error_handling(request, file_name, fh, first_page, 0,
                                       page_size, &n_read);

  if (err != DB_SUCCESS) {
    ib::info(ER_IB_MSG_1076, space->name, ut_strerr(err));
    ut::aligned_free(first_page);
    return (err);
  }

  ulint offset;
  const page_size_t space_page_size(space->flags);

  offset = fsp_header_get_encryption_offset(space_page_size);
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

/** Fix up a v5.7 type undo tablespace that was being truncated.
The space_id is not a reserved undo space_id. We will just delete
the file since it will be replaced.
@param[in]  space_id  Tablespace ID
@return error code */
static dberr_t srv_undo_tablespace_fixup_57(space_id_t space_id) {
  space_id_t space_num = undo::id2num(space_id);
  ut_ad(space_num == space_id);
  if (undo::is_active_truncate_log_present(space_num)) {
    ib::info(ER_IB_MSG_1077, ulong{space_num});

    if (srv_read_only_mode) {
      ib::error(ER_IB_MSG_1078);
      return (DB_READ_ONLY);
    }

    undo::Tablespace undo_space(space_id);

    /* Flush any changes recovered in REDO */
    fil_flush(space_id);
    fil_space_close(space_id);

    os_file_delete_if_exists(innodb_data_file_key, undo_space.file_name(),
                             nullptr);

    return (DB_TABLESPACE_DELETED);
  }

  return (DB_SUCCESS);
}

/** Start the fix-up process on an undo tablespace if it was in the process
of being truncated when the server crashed. At this point, just delete the
old file if it exists.
We could do the whole reconstruction here for implicit undo spaces since we
know the space_id, space_name, and file_name implicitly.  But for explicit
undo spaces, we must wait for the DD to be scanned in boot_tablespaces()
in order to know the space_id, space_name, and file_name.
@param[in]  space_num  undo tablespace number
@return error code */
static dberr_t srv_undo_tablespace_fixup_num(space_id_t space_num) {
  if (!undo::is_active_truncate_log_present(space_num)) {
    return (DB_SUCCESS);
  }

  ib::info(ER_IB_MSG_1077, ulong{space_num});

  if (srv_read_only_mode) {
    ib::error(ER_IB_MSG_1078);
    return (DB_READ_ONLY);
  }

  /*
    Search for a file that is using any of the space IDs assigned to this
    undo number. The directory scan assured that there are no duplicate files
    with the same space_id or with the same undo space number.
   */
  space_id_t space_id = SPACE_UNKNOWN;
  std::string scanned_name;
  fil_system_get_file_by_space_num(space_num, space_id, scanned_name);

  /* If the previous file still exists, delete it. */
  if (scanned_name.length() > 0) {
    /* Flush any changes recovered in REDO */
    fil_flush(space_id);
    fil_space_close(space_id);
    os_file_delete_if_exists(innodb_data_file_key, scanned_name.c_str(),
                             nullptr);

  } else if (space_num < FSP_IMPLICIT_UNDO_TABLESPACES) {
    /* If there is any file with the implicit file name, delete it. */
    undo::Tablespace undo_space(undo::num2id(space_num, 0));
    os_file_delete_if_exists(innodb_data_file_key, undo_space.file_name(),
                             nullptr);
  }

  return (DB_SUCCESS);
}

/** Fix up an undo tablespace if it was in the process of being truncated
when the server crashed. This is the second call and is done after the DD
is available so now we know the space_name, file_name and previous space_id.
@param[in]  space_name  undo tablespace name
@param[in]  file_name   undo tablespace file name
@param[in]  space_id    undo tablespace ID
@return error code */
dberr_t srv_undo_tablespace_fixup(const char *space_name, const char *file_name,
                                  space_id_t space_id) {
  ut_ad(fsp_is_undo_tablespace(space_id));

  space_id_t space_num = undo::id2num(space_id);
  if (!undo::is_active_truncate_log_present(space_num)) {
    return (DB_SUCCESS);
  }

  if (srv_read_only_mode) {
    return (DB_READ_ONLY);
  }

  ib::info(ER_IB_MSG_1079, ulong{space_num});

  /* It is possible for an explicit undo tablespace to have been truncated and
  recreated but not yet written with a header page when a crash occurred.  In
  this case, the empty file would not have been scanned at startup and the
  first call to fixup did not know the filename.  Now that we know it, just
  delete any file with that name if it exists.  The dictionary claims it is
  an undo tablespace and there is a truncate log file present. */
  os_file_delete_if_exists(innodb_data_file_key, file_name, nullptr);

  /* Mark the space_id for this undo tablespace number as in-use. */
  undo::spaces->x_lock();
  undo::unuse_space_id(space_id);
  space_id_t new_space_id = undo::next_space_id(space_id);
  undo::use_space_id(new_space_id);
  undo::spaces->x_unlock();

  dberr_t err = srv_undo_tablespace_create(space_name, file_name, new_space_id);
  if (err != DB_SUCCESS) {
    return (err);
  }

  /* Update the DD with the new space ID and state. */
  undo::spaces->s_lock();
  undo::Tablespace *undo_space = undo::spaces->find(space_num);
  dd_space_states to_state;
  if (undo_space->is_inactive_explicit()) {
    to_state = DD_SPACE_STATE_EMPTY;
    undo_space->set_empty();
  } else {
    to_state = DD_SPACE_STATE_ACTIVE;
    undo_space->set_active();
  }
  undo::spaces->s_unlock();

  bool dd_result = dd_tablespace_get_mdl(space_name);
  if (dd_result == DD_SUCCESS) {
    dd_result =
        dd_tablespace_set_id_and_state(space_name, new_space_id, to_state);
  }
  if (dd_result != DD_SUCCESS) {
    err = DB_ERROR;
  }

  return (err);
}

/** Open an undo tablespace.
@param[in]  undo_space  Undo tablespace
@return DB_SUCCESS or error code */
dberr_t srv_undo_tablespace_open(undo::Tablespace &undo_space) {
  DBUG_EXECUTE_IF("ib_undo_tablespace_open_fail",
                  return (DB_CANNOT_OPEN_FILE););

  pfs_os_file_t fh;
  bool success;
  uint32_t flags;
  dberr_t err = DB_ERROR;
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

  if (!os_file_check_mode(file_name, srv_read_only_mode)) {
    ib::error(ER_IB_MSG_1081, file_name,
              srv_read_only_mode ? "readable!" : "writable!");

    return (DB_READ_ONLY);
  }

  /* Open a local handle. */
  fh = os_file_create(
      innodb_data_file_key, file_name,
      OS_FILE_OPEN_RETRY | OS_FILE_ON_ERROR_NO_EXIT | OS_FILE_ON_ERROR_SILENT,
      OS_DATA_FILE, srv_read_only_mode, &success);
  if (!success) {
    return (DB_CANNOT_OPEN_FILE);
  }

  if (space == nullptr) {
    /* Load the tablespace into InnoDB's internal data structures.
    Set the compressed page size to 0 (non-compressed) */
    flags = fsp_flags_init(univ_page_size, false, false, false, false);
    space = fil_space_create(undo_name, space_id, flags, FIL_TYPE_TABLESPACE);
    ut_a(space != nullptr);
    ut_ad(fil_validate());

    os_offset_t size = os_file_get_size(fh);
    ut_a(size != (os_offset_t)-1);
    page_no_t n_pages = static_cast<page_no_t>(size / UNIV_PAGE_SIZE);

    if (fil_node_create(file_name, n_pages, space, false) == nullptr) {
      os_file_close(fh);

      ib::error(ER_IB_MSG_1082, undo_name);

      return (DB_ERROR);
    }
  }

  /* Read the encryption metadata in this undo tablespace.
  If the encryption info in the first page cannot be decrypted
  by the master key, this table cannot be opened. */
  err = srv_undo_tablespace_read_encryption(fh, file_name, space);

  /* The file handle will no longer be needed. */
  success = os_file_close(fh);
  ut_ad(success);

  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_1083, undo_name);
    return (err);
  }

  /* Now that space and node exist, make sure this undo tablespace
  is open so that it stays open until shutdown.
  But if it is under construction, we cannot open it until the
  header page has been written. */
  if (!undo::is_under_construction(space_id)) {
    bool success = fil_space_open(space_id);
    ut_a(success);
  }

  if (undo::is_reserved(space_id)) {
    undo::spaces->add(undo_space);
  }

  return (DB_SUCCESS);
}

/** Open an undo tablespace with a specified space_id.
@param[in]      space_id        tablespace ID
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespace_open_by_id(space_id_t space_id) {
  undo::Tablespace undo_space(space_id);
  std::string scanned_name;

  /* If an undo tablespace with this space_id already exists,
  check if the name found in the file map for this undo space_id
  is the standard name.  The directory scan assured that there are
  no duplicates.  The filename found must match the standard name
  if this is an implicit undo tablespace. In other words, implicit
  undo tablespaces must be found in srv_undo_dir. */

  bool found = fil_system_get_file_by_space_id(space_id, scanned_name);

  if (found &&
      !Fil_path::is_same_as(undo_space.file_name(), scanned_name.c_str())) {
    ib::error(ER_IB_MSG_FOUND_WRONG_UNDO_SPACE, undo_space.file_name(),
              ulong{space_id}, scanned_name.c_str());
    return (DB_WRONG_FILE_NAME);
  }

  dberr_t err = srv_undo_tablespace_open(undo_space);

  if (err == DB_SUCCESS) {
    fil_space_set_undo_size(space_id, false);
  }

  return (err);
}

/** Open an undo tablespace with a specified undo number.
@param[in]  space_num  undo tablespace number
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespace_open_by_num(space_id_t space_num) {
  space_id_t space_id = SPACE_UNKNOWN;
  std::string scanned_name;

  /* Search for a file that is using any of the space IDs assigned to this
  undo number. The directory scan assured that there are no duplicate files
  with the same space_id or with the same undo space number. */
  if (!fil_system_get_file_by_space_num(space_num, space_id, scanned_name)) {
    return (DB_CANNOT_OPEN_FILE);
  }

  /* The first 2 undo space numbers must be implicit. */
  bool is_default = (space_num <= FSP_IMPLICIT_UNDO_TABLESPACES);

  /* v8.0.12 used innodb_undo_tablespaces to implicitly create undo
  spaces so there may be more than 2 implicit undo tablespaces.  They
  must match the default undo filename and must be found in
  srv_undo_directory. */
  undo::Tablespace undo_space(space_id);
  if (!Fil_path::is_same_as(undo_space.file_name(), scanned_name.c_str())) {
    if (is_default) {
      ib::info(ER_IB_MSG_1080, undo_space.file_name(), scanned_name.c_str(),
               ulong{space_id});

      return (DB_WRONG_FILE_NAME);
    }

    /* Explicit undo tablespaces must end with the suffix '.ibu'. */
    if (!Fil_path::has_suffix(IBU, scanned_name)) {
      ib::info(ER_IB_MSG_NOT_END_WITH_IBU, scanned_name.c_str());

      return (DB_WRONG_FILE_NAME);
    }

    /* Use the file name found in the scan. */
    undo_space.set_file_name(scanned_name.c_str());
  }

  /* Mark the space_id for this undo tablespace number as in-use. */
  undo::use_space_id(space_id);

  ib::info(ER_IB_MSG_USING_UNDO_SPACE, scanned_name.c_str());

  dberr_t err = srv_undo_tablespace_open(undo_space);

  if (err == DB_SUCCESS) {
    fil_space_set_undo_size(space_id, false);
  }

  return (err);
}

/* Open existing undo tablespaces up to the number in target_undo_tablespace.
If we are making a new database, these have been created.
If doing recovery, these should exist and may be needed for recovery.
If we fail to open any of these it is a fatal error.
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespaces_open() {
  dberr_t err;

  /* If upgrading from 5.7, build a list of existing undo tablespaces
  from the references in the TRX_SYS page. (not including the system
  tablespace) */
  trx_rseg_get_n_undo_tablespaces(trx_sys_undo_spaces);

  /* If undo tablespaces are being tracked in trx_sys then these
  will need to be replaced by independent undo tablespaces with
  reserved space_ids and RSEG_ARRAY pages. */
  if (trx_sys_undo_spaces->size() > 0) {
    /* Open each undo tablespace tracked in TRX_SYS. */
    for (const auto space_id : *trx_sys_undo_spaces) {
      fil_set_max_space_id_if_bigger(space_id);

      /* Check if this undo tablespace was in the process of being truncated.
      If so, just delete the file since it will be replaced. */
      if (DB_TABLESPACE_DELETED == srv_undo_tablespace_fixup_57(space_id)) {
        continue;
      }

      err = srv_undo_tablespace_open_by_id(space_id);
      if (err != DB_SUCCESS) {
        ib::error(ER_IB_MSG_CANNOT_OPEN_57_UNDO, ulong{space_id});
        return (err);
      }
    }
  }

  /* Open all existing implicit and explicit undo tablespaces.
  The tablespace scan has completed and the undo::space_id_bank has been
  filled with the space Ids that were found. */
  undo::spaces->x_lock();
  ut_ad(undo::spaces->size() == 0);

  for (space_id_t num = 1; num <= FSP_MAX_UNDO_TABLESPACES; ++num) {
    /* Check if this undo tablespace was in the process of being truncated.
    If so, recreate it and add it to the construction list. */
    dberr_t err = srv_undo_tablespace_fixup_num(num);
    if (err != DB_SUCCESS) {
      undo::spaces->x_unlock();
      return (err);
    }

    err = srv_undo_tablespace_open_by_num(num);
    switch (err) {
      case DB_WRONG_FILE_NAME:
        /* An Undo tablespace was found where the mapping
        file said it was.  Now we have a different filename
        for it. The undo directory must have changed and
        the the files were not moved. Cannot startup. */
      case DB_READ_ONLY:
        /* The undo tablespace was found where it should be
        but it cannot be opened in read/write mode. */
      default:
        /* The undo tablespace was found where it should be
        but it cannot be used. */
        undo::spaces->x_unlock();
        return (err);

      case DB_SUCCESS:

      case DB_CANNOT_OPEN_FILE:
        /* Doesn't exist, keep looking */
        break;
    }
  }

  ulint n_found_new = undo::spaces->size();
  ulint n_found_old = trx_sys_undo_spaces->size();
  undo::spaces->x_unlock();

  if (n_found_old != 0 || n_found_new < FSP_IMPLICIT_UNDO_TABLESPACES) {
    std::ostringstream msg;

    if (n_found_old != 0) {
      msg << "Found " << n_found_old << " undo tablespaces that"
          << " need to be upgraded. ";
    }

    if (n_found_new < FSP_IMPLICIT_UNDO_TABLESPACES) {
      msg << "Will create " << (FSP_IMPLICIT_UNDO_TABLESPACES - n_found_new)
          << " new undo tablespaces.";
    }

    ib::info(ER_IB_MSG_1215) << msg.str();
  }

  if (n_found_new + n_found_old) {
    ib::info(ER_IB_MSG_1085, ulonglong{n_found_new + n_found_old});
  }

  return (DB_SUCCESS);
}

/** Create the implicit undo tablespaces if we are creating a new instance
or if there was not enough implicit undo tablespaces previously existing.
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespaces_create() {
  dberr_t err = DB_SUCCESS;

  undo::spaces->x_lock();

  ulint initial_implicit_undo_spaces = 0;
  for (auto undo_space : undo::spaces->m_spaces) {
    if (undo_space->num() <= FSP_IMPLICIT_UNDO_TABLESPACES) {
      initial_implicit_undo_spaces++;
    }
  }

  if (initial_implicit_undo_spaces >= FSP_IMPLICIT_UNDO_TABLESPACES) {
    undo::spaces->x_unlock();
    return (DB_SUCCESS);
  }

  if (srv_read_only_mode || srv_force_recovery > 0) {
    const char *mode;

    mode = srv_read_only_mode ? "read_only" : "force_recovery",

    ib::warn(ER_IB_MSG_1086, mode, ulonglong{initial_implicit_undo_spaces});

    if (initial_implicit_undo_spaces == 0) {
      ib::error(ER_IB_MSG_1087, mode);

      undo::spaces->x_unlock();
      return (DB_ERROR);
    }

    undo::spaces->x_unlock();
    return (DB_SUCCESS);
  }

  /* Create all implicit undo tablespaces that are needed. */
  for (space_id_t num = 1; num <= FSP_IMPLICIT_UNDO_TABLESPACES; ++num) {
    /* If the trunc log file is present, the fixup process will be
    finished later. */
    if (undo::is_active_truncate_log_present(num)) {
      continue;
    }

    /* Check if an independent undo space for this space_id
    has already been found. */
    if (undo::spaces->contains(num)) {
      continue;
    }

    /* Mark this implicit undo space number as used and return the next
    available space_id. */
    space_id_t space_id = undo::use_next_space_id(num);

    /* Since it is not found, create it. */
    undo::Tablespace undo_space(space_id);
    undo_space.set_new();
    err = srv_undo_tablespace_create(undo_space);
    if (err != DB_SUCCESS) {
      ib::info(ER_IB_MSG_1088, undo_space.space_name());
      break;
    }

    /* Open this new undo tablespace. */
    err = srv_undo_tablespace_open(undo_space);
    if (err != DB_SUCCESS) {
      ib::info(ER_IB_MSG_1089, int{err}, ut_strerr(err),
               undo_space.space_name());

      break;
    }
  }

  undo::spaces->x_unlock();

  ulint new_spaces =
      FSP_IMPLICIT_UNDO_TABLESPACES - initial_implicit_undo_spaces;

  ib::info(ER_IB_MSG_1090, ulonglong{new_spaces});

  return (err);
}

/** Finish building an undo tablespace. So far these tablespace files in
the construction list should be created and filled with zeros.
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespaces_construct() {
  mtr_t mtr;

  if (undo::s_under_construction.empty()) {
    return (DB_SUCCESS);
  }

  ut_a(!srv_read_only_mode);
  ut_a(!srv_force_recovery);

  if (srv_undo_log_encrypt && Encryption::check_keyring() == false) {
    my_error(ER_CANNOT_FIND_KEY_IN_KEYRING, MYF(0));
    return (DB_ERROR);
  }

  for (auto space_id : undo::s_under_construction) {
    /* Enable undo log encryption if it's ON. */
    if (srv_undo_log_encrypt) {
      dberr_t err = srv_undo_tablespace_enable_encryption(space_id);

      if (err != DB_SUCCESS) {
        ib::error(ER_IB_MSG_1091, ulong{undo::id2num(space_id)});

        return (err);
      }
    }

    log_free_check();

    mtr_start(&mtr);

    mtr_x_lock(fil_space_get_latch(space_id), &mtr, UT_LOCATION_HERE);

    if (!fsp_header_init(space_id, UNDO_INITIAL_SIZE_IN_PAGES, &mtr)) {
      ib::error(ER_IB_MSG_1093, ulong{undo::id2num(space_id)});

      mtr_commit(&mtr);
      return (DB_ERROR);
    }

    /* Add the RSEG_ARRAY page. */
    trx_rseg_array_create(space_id, &mtr);

    mtr_commit(&mtr);

    /* The rollback segments will get created later in
    trx_rseg_add_rollback_segments(). */
  }

  if (srv_undo_log_encrypt) {
    ut_d(bool ret =) srv_enable_undo_encryption();
    ut_ad(!ret);
  }

  return (DB_SUCCESS);
}

/** Mark the point in which the undo tablespaces in the construction list
are fully constructed and ready to use. */
static void srv_undo_tablespaces_mark_construction_done() {
  /* Remove the truncate log files if they exist. */
  for (auto space_id : undo::s_under_construction) {
    /* Flush these pages to disk since they were not redo logged. */
    auto flush_observer = ut::new_withkey<Flush_observer>(
        UT_NEW_THIS_FILE_PSI_KEY, space_id, nullptr, nullptr);

    flush_observer->flush();
    ut::delete_(flush_observer);

    space_id_t space_num = undo::id2num(space_id);
    if (undo::is_active_truncate_log_present(space_num)) {
      undo::done_logging(space_num);
    }
  }

  undo::clear_construction_list();
}

/** Upgrade undo tablespaces by deleting the old undo tablespaces
referenced by the TRX_SYS page.
@return error code */
dberr_t srv_undo_tablespaces_upgrade() {
  if (trx_sys_undo_spaces->empty()) {
    goto cleanup;
  }

  /* Recovered transactions in the prepared state prevent the old
  rsegs and undo tablespaces they are in from being deleted.
  These transactions must be either committed or rolled back by
  the mysql server.*/
  if (trx_sys->n_prepared_trx > 0) {
    ib::warn(ER_IB_MSG_1094);
    return (DB_SUCCESS);
  }

  ib::info(ER_IB_MSG_1095, trx_sys_undo_spaces->size(),
           ulong{FSP_IMPLICIT_UNDO_TABLESPACES});

  /* All Undo Tablespaces found in the TRX_SYS page need to be
  deleted. The new independent undo tablespaces were created in
  in srv_undo_tablespaces_create() */
  for (const auto space_id : *trx_sys_undo_spaces) {
    undo::Tablespace undo_space(space_id);

    fil_space_close(undo_space.id());

    auto err = fil_delete_tablespace(undo_space.id(), BUF_REMOVE_ALL_NO_WRITE);

    if (err != DB_SUCCESS) {
      ib::warn(ER_IB_MSG_57_UNDO_SPACE_DELETE_FAIL, undo_space.space_name());
    }
  }

  /* All pages should be removed from the spaces we deleted. We just collect
  them now, so that the space_id -> shard mapping is correct - it will be
  changed the second the trx_sys_undo_spaces is cleared.*/
  fil_purge();

  /* Remove the tracking of these undo tablespaces from TRX_SYS page and
  trx_sys->rsegs. */
  trx_rseg_upgrade_undo_tablespaces();

  /* Since we now have new format undo tablespaces, we will no longer
  look for undo tablespaces or rollback segments in the TRX_SYS page
  or the trx_sys->rsegs vector. */
  trx_sys_undo_spaces->clear();

cleanup:
  /* Post 5.7 undo tablespaces track their own rsegs.
  Clear the list of rsegs in old undo tablespaces. */
  trx_sys->rsegs.clear();

  return (DB_SUCCESS);
}

/** Downgrade undo tablespaces by deleting the new undo tablespaces which
are not referenced by the TRX_SYS page. */
static void srv_undo_tablespaces_downgrade() {
  ut_ad(srv_downgrade_logs);

  ib::info(ER_IB_MSG_1096, ulonglong{undo::spaces->size()});

  /* All the new independent undo tablespaces that were created in
  in srv_undo_tablespaces_create() need to be deleted. */
  for (const auto undo_space : undo::spaces->m_spaces) {
    fil_space_close(undo_space->id());

    os_file_delete(innodb_data_file_key, undo_space->file_name());
  }
}

/** Create an undo tablespace with an explicit file name
This is called during CREATE UNDO TABLESPACE.
@param[in]  space_name  tablespace name
@param[in]  file_name   file name
@param[in]  space_id    Tablespace ID
@return DB_SUCCESS or error code */
dberr_t srv_undo_tablespace_create(const char *space_name,
                                   const char *file_name, space_id_t space_id) {
  if (srv_undo_log_encrypt && Encryption::check_keyring() == false) {
    my_error(ER_CANNOT_FIND_KEY_IN_KEYRING, MYF(0));
    return (DB_ERROR);
  }

  /* We need to x_lock the undo::spaces list until after this
  is created and added to it. */
  undo::spaces->x_lock();

  ut_ad(undo::spaces->find(undo::id2num(space_id)) == nullptr);

  undo::Tablespace undo_space(space_id);
  undo_space.set_space_name(space_name);
  undo_space.set_file_name(file_name);

  dberr_t err = srv_undo_tablespace_create(undo_space);
  if (err != DB_SUCCESS) {
    undo::spaces->x_unlock();
    goto cleanup_and_exit;
  }

  /* Open this new undo tablespace. */
  err = srv_undo_tablespace_open(undo_space);
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_ERROR_OPENING_NEW_UNDO_SPACE, int{err}, space_name);
    undo::spaces->x_unlock();
    goto cleanup_and_exit;
  }

  /* Unlock the undo::spaces list now that we are no longer changing it.
  This new undo space will not be used by new transactions until it
  becomes active. */
  undo::spaces->x_unlock();

  /* Write header and RSEG_ARRAY pages to this undo tablespace. */
  err = srv_undo_tablespaces_construct();
  if (err != DB_SUCCESS) {
    goto cleanup_and_exit;
  }

  /* Create the rollback segments in this tablespace and add an Rseg object
  for each one to the Rsegs list. */
  if (!trx_rseg_init_rollback_segments(space_id, srv_rollback_segments)) {
    err = DB_ERROR;
    goto cleanup_and_exit;
  }

cleanup_and_exit:
  /* If UNDO tablespace couldn't initialize completely, remove it from
  undo tablespace list */
  if (err != DB_SUCCESS) {
    undo::spaces->x_lock();
    undo::spaces->drop(undo_space);
    undo::spaces->x_unlock();

    /* Remove undo tablespace file (if created) */
    os_file_delete_if_exists(innodb_data_file_key, undo_space.file_name(),
                             nullptr);
  }

  srv_undo_tablespaces_mark_construction_done();
  return (err);
}

/** Initialize undo::spaces and trx_sys_undo_spaces,
called once during srv_start(). */
void undo_spaces_init() {
  ut_ad(undo::spaces == nullptr);

  undo::spaces = ut::new_withkey<undo::Tablespaces>(
      ut::make_psi_memory_key(mem_key_undo_spaces));

  trx_sys_undo_spaces_init();

  undo::init_space_id_bank();
}

/** Free the resources occupied by undo::spaces and trx_sys_undo_spaces,
called once during thread de-initialization. */
void undo_spaces_deinit() {
  if (srv_downgrade_logs) {
    srv_undo_tablespaces_downgrade();
  }

  if (undo::spaces != nullptr) {
    /* There can't be any active transactions. */
    undo::spaces->clear();

    ut::delete_(undo::spaces);
    undo::spaces = nullptr;
  }

  trx_sys_undo_spaces_deinit();

  if (undo::space_id_bank != nullptr) {
    ut::delete_arr(undo::space_id_bank);
    undo::space_id_bank = nullptr;
  }
}

/** Open the configured number of implicit undo tablespaces.
@param[in]      create_new_db   true if new db being created
@return DB_SUCCESS or error code */
static dberr_t srv_undo_tablespaces_init(bool create_new_db) {
  dberr_t err = DB_SUCCESS;

  /* Open any existing implicit undo tablespaces. */
  if (!create_new_db) {
    err = srv_undo_tablespaces_open();
    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  /* If this is opening an existing database, create and open any
  undo tablespaces that are still needed. For a new DB, create
  them all. */
  mutex_enter(&undo::ddl_mutex);
  err = srv_undo_tablespaces_create();
  if (err != DB_SUCCESS) {
    mutex_exit(&undo::ddl_mutex);
    return (err);
  }

  /* Finish building any undo tablespaces just created by adding
  header pages, rseg_array pages, and rollback segments. Then delete
  any undo truncation log files and clear the construction list.
  This list includes any tablespace newly created or fixed-up. */
  err = srv_undo_tablespaces_construct();
  if (err != DB_SUCCESS) {
    mutex_exit(&undo::ddl_mutex);
    return (err);
  }

  mutex_exit(&undo::ddl_mutex);
  return (DB_SUCCESS);
}

/********************************************************************
Wait for the purge thread(s) to start up. */
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

/** Create the temporary file tablespace.
@param[in]      create_new_db   whether we are creating a new database
@param[in,out]  tmp_space       Shared Temporary SysTablespace
@return DB_SUCCESS or error code. */
static dberr_t srv_open_tmp_tablespace(bool create_new_db,
                                       SysTablespace *tmp_space) {
  page_no_t sum_of_new_sizes;

  /* Will try to remove if there is existing file left-over by last
  unclean shutdown */
  tmp_space->set_sanity_check_status(true);
  tmp_space->delete_files();
  tmp_space->set_ignore_read_only(true);

  ib::info(ER_IB_MSG_1098);

  bool create_new_temp_space = true;

  tmp_space->set_space_id(dict_sys_t::s_temp_space_id);

  RECOVERY_CRASH(100);

  dberr_t err =
      tmp_space->check_file_spec(create_new_temp_space, 12 * 1024 * 1024);

  if (err == DB_FAIL) {
    ib::error(ER_IB_MSG_1099, tmp_space->name());

    err = DB_ERROR;

  } else if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_1100, tmp_space->name());

  } else if ((err = tmp_space->open_or_create(true, create_new_db,
                                              &sum_of_new_sizes, nullptr)) !=
             DB_SUCCESS) {
    ib::error(ER_IB_MSG_1101, tmp_space->name());

  } else {
    mtr_t mtr;
    page_no_t size = tmp_space->get_sum_of_sizes();

    /* Open this shared temp tablespace in the fil_system so that
    it stays open until shutdown. */
    if (fil_space_open(tmp_space->space_id())) {
      /* Initialize the header page */
      mtr_start(&mtr);
      mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

      fsp_header_init(tmp_space->space_id(), size, &mtr);

      mtr_commit(&mtr);
    } else {
      /* This file was just opened in the code above! */
      ib::error(ER_IB_MSG_1102, tmp_space->name());

      err = DB_ERROR;
    }
  }

  return (err);
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

/** Recreate REDO log files.
@param[in,out] flushed_lsn flushed_lsn
@return DB_SUCCESS or error code */
static dberr_t recreate_redo_files(lsn_t &flushed_lsn) {
  ut_d(log_sys->disable_redo_writes = true);

  /* Emit a message to the error log. */
  const auto target_size = log_sys->m_capacity.target_physical_capacity();
  const auto target_size_in_M = target_size / (1024 * 1024UL);
  ib::info(ER_IB_MSG_LOG_FILES_UPGRADE, ulonglong{target_size_in_M},
           ulonglong{flushed_lsn});

  RECOVERY_CRASH(5);
  RECOVERY_CRASH(6);
  ib::info(ER_IB_MSG_LOG_FILES_REWRITING);

  /* Remove all existing log files. */
  log_files_remove(*log_sys);

  log_sys_close();
  ut_a(log_sys == nullptr);

  /* The checkpoint_lsn found could be larger than flushed_lsn in the system
  tables space in case the shutdown wasn't slow. In such case we should start
  from an lsn at least equal to checkpoint_lsn as pages in the tablespace will
  have lsns larger than flushed_lsn. */
  if (recv_sys->checkpoint_lsn != 0) {
    ut_ad(flushed_lsn <= recv_sys->checkpoint_lsn);
    flushed_lsn = std::max(flushed_lsn, recv_sys->checkpoint_lsn);
  }

  /* This is to provide the property that data byte at given lsn never
  changes and avoid the need to rewrite the block with flushed_lsn. */
  flushed_lsn = ut_uint64_align_up(flushed_lsn, OS_FILE_LOG_BLOCK_SIZE) +
                LOG_BLOCK_HDR_SIZE;

  /* `true` parameter makes sure new files are created */
  dberr_t err = log_sys_init(true, flushed_lsn, flushed_lsn);
  if (err != DB_SUCCESS) {
    return err;
  }

  ut_d(log_sys->disable_redo_writes = false);

  fil_open_system_tablespace_files();

  return DB_SUCCESS;
}

dberr_t srv_start(bool create_new_db) {
  page_no_t sum_of_data_file_sizes;
  page_no_t tablespace_size_in_header;
  dberr_t err;
  mtr_t mtr;
  purge_pq_t *purge_queue;

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

  if (srv_is_upgrade_mode) {
    if (srv_force_recovery != 0) {
      ib::error(ER_IB_MSG_1111);
      return (srv_init_abort(DB_ERROR));
    }
    if (srv_read_only_mode) {
      ib::error(ER_IB_MSG_1110);
      return (srv_init_abort(DB_ERROR));
    }
  }

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

#ifdef UNIV_LOG_LSN_DEBUG
  ib::info(ER_IB_MSG_1116)
      << "!!!!!!!! UNIV_LOG_LSN_DEBUG switched on !!!!!!!!!";
#endif /* UNIV_LOG_LSN_DEBUG */

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

  /* This is the default directory for IBD and IBU files. Put it first
  in the list of known directories. */
  fil_set_scan_dir(MySQL_datadir_path.path());

  /* Add --innodb-data-home-dir as a known location for IBD and IBU files
  if it is not already there. */
  ut_ad(srv_data_home != nullptr && *srv_data_home != '\0');
  fil_set_scan_dir(Fil_path::remove_quotes(srv_data_home));

  /* Add --innodb-directories as known locations for IBD and IBU files. */
  if (srv_innodb_directories != nullptr && *srv_innodb_directories != 0) {
    fil_set_scan_dirs(Fil_path::remove_quotes(srv_innodb_directories));
  }

  /* Note whether the undo path is different (not the same or under)
  from all other known directories. If so, this will allow us to keep
  IBD files out of this unique undo location.*/
  MySQL_undo_path_is_unique = !fil_path_is_known(MySQL_undo_path.path());

  /* For the purpose of file discovery at startup, we need to scan
  --innodb-undo-directory also if it is different from the locations above. */
  if (MySQL_undo_path_is_unique) {
    fil_set_scan_dir(Fil_path::remove_quotes(MySQL_undo_path));
  }

  ib::info(ER_IB_MSG_378) << "Directories to scan '" << fil_get_dirs() << "'";

  /* Must replace clone files before scanning directories. When
  clone replaces current database, cloned files are moved to data files
  at this stage. */
  err = clone_init();

  if (err != DB_SUCCESS) {
    return (srv_init_abort(err));
  }

  err = fil_scan_for_tablespaces();

  if (err != DB_SUCCESS) {
    return (srv_init_abort(err));
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

        return (srv_init_abort(DB_ERROR));
      }
    } else {
      srv_monitor_file_name = nullptr;
      srv_monitor_file = os_file_create_tmpfile();

      if (!srv_monitor_file) {
        return (srv_init_abort(DB_ERROR));
      }
    }

    mutex_create(LATCH_ID_SRV_MISC_TMPFILE, &srv_misc_tmpfile_mutex);

    srv_misc_tmpfile = os_file_create_tmpfile();

    if (!srv_misc_tmpfile) {
      return (srv_init_abort(DB_ERROR));
    }
  }

  if (!os_aio_init(srv_n_read_io_threads, srv_n_write_io_threads)) {
    ib::error(ER_IB_MSG_1129);

    return (srv_init_abort(DB_ERROR));
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

  err = buf_pool_init(srv_buf_pool_size, srv_buf_pool_instances);

  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_1131);

    return (srv_init_abort(DB_ERROR));
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

  /* Create i/o-handler threads: */
  os_aio_start_threads();

  /* Even in read-only mode there could be flush job generated by
  intrinsic table operations. */
  buf_flush_page_cleaner_init();

  srv_start_state_set(SRV_START_STATE_IO);

  srv_startup_is_before_trx_rollback_phase = !create_new_db;

  if (create_new_db) {
    recv_sys_free();
  }

  /* Open or create the data files. */
  page_no_t sum_of_new_sizes;
  lsn_t flushed_lsn;

  err = srv_sys_space.open_or_create(false, create_new_db, &sum_of_new_sizes,
                                     &flushed_lsn);

  switch (err) {
    case DB_SUCCESS:
      break;
    case DB_CANNOT_OPEN_FILE:
      ib::error(ER_IB_MSG_1134);
      [[fallthrough]];
    default:

      /* Other errors might come from
      Datafile::validate_first_page() */

      return (srv_init_abort(err));
  }

  if (flushed_lsn < LOG_START_LSN) {
    ut_ad(!create_new_db);
    /* Data directory hasn't been initialized yet. */
    ib::error(ER_IB_MSG_DATA_DIRECTORY_NOT_INITIALIZED_OR_CORRUPTED);
    return srv_init_abort(DB_ERROR);
  }

  /* FIXME: This can be done earlier, but we now have to wait for
  checking of system tablespace. */
  dict_persist_init();

  mtr_t::s_logging.init();

  if (dblwr::is_enabled() && ((err = dblwr::open()) != DB_SUCCESS)) {
    return srv_init_abort(err);
  }

  lsn_t new_files_lsn;

  err = log_sys_init(create_new_db, flushed_lsn, new_files_lsn);

  if (err != DB_SUCCESS) {
    return srv_init_abort(err);
  }

  ut_a(log_sys != nullptr);

  arch_init();

  if (create_new_db) {
    ut_a(buf_are_flush_lists_empty_validate());

    ut_a(!srv_read_only_mode);

    ut_a(log_sys->last_checkpoint_lsn.load() ==
         LOG_START_LSN + LOG_BLOCK_HDR_SIZE);

    ut_a(new_files_lsn == LOG_START_LSN + LOG_BLOCK_HDR_SIZE);

    err = log_start(*log_sys, new_files_lsn, new_files_lsn);

    if (err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

    log_start_background_threads(*log_sys);

    err = srv_undo_tablespaces_init(true);

    if (err != DB_SUCCESS) {
      return (srv_init_abort(err));
    }

    mtr_start(&mtr);

    bool ret = fsp_header_init(0, sum_of_new_sizes, &mtr);

    mtr_commit(&mtr);

    if (!ret) {
      return (srv_init_abort(DB_ERROR));
    }

    /* To maintain backward compatibility we create only
    the first rollback segment before the double write buffer.
    All the remaining rollback segments will be created later,
    after the double write buffers haves been created. */
    trx_sys_create_sys_pages();

    trx_purge_sys_mem_create();

    purge_queue = trx_sys_init_at_db_start();

    /* The purge system needs to create the purge view and
    therefore requires that the trx_sys is inited. */

    trx_purge_sys_initialize(srv_threads.m_purge_workers_n, purge_queue);

    err = dict_create();

    if (err != DB_SUCCESS) {
      return (srv_init_abort(err));
    }

    srv_create_sdi_indexes();

    /* We always create the legacy double write buffer to preserve the
    expected page ordering of the system tablespace.
    FIXME: Try and remove this requirement. */
    err = dblwr::v1::create();

    if (err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

  } else {
    /* Load the reserved boundaries of the legacy dblwr buffer, this is
    required to check for stray reads and writes trying to access this
    reserved region in the sys tablespace.
    FIXME: Try and remove this requirement. */
    err = dblwr::v1::init();

    if (err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

    /* Invalidate the buffer pool to ensure that we reread
    the page that we read above, during recovery.
    Note that this is not as heavy weight as it seems. At
    this point there will be only ONE page in the buf_LRU
    and there must be no page in the buf_flush list. */
    buf_pool_invalidate();

    /* Open all data files in the system tablespace:
    we keep them open until database shutdown. */
    fil_open_system_tablespace_files();

    /* We always try to do a recovery, even if the database had
    been shut down normally: this is the normal startup path */
    RECOVERY_CRASH(1);

    if (new_files_lsn != 0) {
      /* This means that either no log files have been found
      or the existing log files were marked as uninitialized. */
      flushed_lsn = new_files_lsn;
    }

    ut_a(log_sys->m_format <= Log_format::CURRENT);

    const bool log_upgrade = log_sys->m_format < Log_format::CURRENT;

    if (log_upgrade) {
      if (srv_read_only_mode) {
        ib::error(ER_IB_MSG_LOG_UPGRADE_IN_READ_ONLY_MODE,
                  ulong{to_int(log_sys->m_format)});
        return srv_init_abort(DB_ERROR);
      }

      /* Check if the redo log from an older known redo log
      version is from a clean shutdown. */
      err = recv_verify_log_is_clean_pre_8_0_30(*log_sys);
      if (err != DB_SUCCESS) {
        return srv_init_abort(err);
      }

      /* Redo logs are clean. We need to recreate REDO files */
      err = recreate_redo_files(flushed_lsn);
      if (err != DB_SUCCESS) {
        return srv_init_abort(err);
      }
    }

    err = recv_recovery_from_checkpoint_start(*log_sys, flushed_lsn);
    if (err != DB_SUCCESS) {
      return srv_init_abort(err);
    }

    if (err == DB_SUCCESS) {
      arch_page_sys->post_recovery_init();

      /* Initialize the change buffer. */
      err = dict_boot();
    }

    if (err != DB_SUCCESS) {
      return (srv_init_abort(err));
    }

    ut_ad(clone_check_recovery_crashpoint(recv_sys->is_cloned_db));

    const bool redo_writes_allowed = !srv_read_only_mode;

    ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO || !redo_writes_allowed);

    if (redo_writes_allowed) {
      /* We need to start log threads now, because recovery
      could result in execution of ibuf merges. These merges
      could result in new redo records. In the read-only mode
      we do not need log threads, because we disallow new redo
      records in such mode. If upgrade was forced, or the data
      directory was cloned, we will start redo threads later. */
      log_start_background_threads(*log_sys);
    }

    if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
      /* Apply the hashed log records to the
      respective file pages, for the last batch of
      recv_group_scan_log_recs(). */

      RECOVERY_CRASH(2);

      /* Don't allow IBUF operations for cloned database
      recovery as it would add extra redo log and we may
      not have enough margin.

      Don't allow IBUF operations when redo is written
      in the older format than the current, because we
      would write new redo records in the current fmt,
      and end up with file in both formats = invalid. */

      recv_apply_hashed_log_recs(*log_sys,
                                 !recv_sys->is_cloned_db && !log_upgrade);

      if (recv_sys->found_corrupt_log) {
        err = DB_ERROR;
        return (srv_init_abort(err));
      }

      DBUG_PRINT("ib_log", ("apply completed"));

      /* Check and print if there were any tablespaces
      which had redo log records but we couldn't apply
      them because the filenames were missing. */

      /* Recovery complete, start verifying the
      page LSN on read. */
      recv_lsn_checks_on = true;
    }

    /* We have gone through the redo log, now check if all the
    tablespaces were found and recovered. */

    if (srv_force_recovery == 0 && fil_check_missing_tablespaces()) {
      ib::error(ER_IB_MSG_1139);
      RECOVERY_CRASH(3);

      /* Set the abort flag to true. */
      auto p = recv_recovery_from_checkpoint_finish(true);

      ut_a(p == nullptr);

      return (srv_init_abort(DB_ERROR));
    }

    /* We have successfully recovered from the redo log. The
    data dictionary should now be readable. */

    if (recv_sys->found_corrupt_log) {
      ib::warn(ER_IB_MSG_RECOVERY_CORRUPT);
    }

    if (!srv_force_recovery && !srv_read_only_mode) {
      buf_flush_sync_all_buf_pools();
    }

    RECOVERY_CRASH(3);

    auto *dict_metadata = recv_recovery_from_checkpoint_finish(false);
    ut_a(dict_metadata != nullptr);

    /* We need to save the dynamic metadata collected from redo log to DD
    buffer table here. This is to make sure that the dynamic metadata is not
    lost by any future checkpoint. Since DD and data dictionary in memory
    objects are not fully initialized at this point, the usual mechanism to
    persist dynamic metadata at checkpoint wouldn't work. */

    DBUG_EXECUTE_IF("log_first_rec_group_test", {
      const lsn_t end_lsn = mtr_commit_mlog_test();
      log_write_up_to(*log_sys, end_lsn, true);
      DBUG_SUICIDE();
    });

    if (!recv_sys->is_cloned_db && !dict_metadata->empty()) {
      ut_a(redo_writes_allowed);

      /* Open this table in case dict_metadata should be applied to this
      table before checkpoint. And because DD is not fully up yet, the table
      can be opened by internal APIs. */

      fil_space_t *space =
          fil_space_acquire_silent(dict_sys_t::s_dict_space_id);
      if (space == nullptr) {
        dberr_t error =
            fil_ibd_open(true, FIL_TYPE_TABLESPACE, dict_sys_t::s_dict_space_id,
                         predefined_flags, dict_sys_t::s_dd_space_name,
                         dict_sys_t::s_dd_space_file_name, true, false);
        if (error != DB_SUCCESS) {
          ib::error(ER_IB_MSG_1142);
          return (srv_init_abort(DB_ERROR));
        }
      } else {
        fil_space_release(space);
      }

      dict_persist->table_buffer =
          ut::new_withkey<DDTableBuffer>(UT_NEW_THIS_FILE_PSI_KEY);
      /* We write redo log here. We assume that there should be enough room in
      log files, supposing log_free_check() works fine before crash. */
      dict_metadata->store();

      /* Flush logs to persist the changes. */
      log_buffer_flush_to_disk(*log_sys);
    }
    ut::delete_(dict_metadata);

    RECOVERY_CRASH(4);

    log_sys->m_allow_checkpoints.store(true, std::memory_order_release);

    if (recv_sys->is_cloned_db || recv_sys->is_meb_db) {
      buf_pool_wait_for_no_pending_io();

      /* Reset creator for log */

      if (redo_writes_allowed) {
        log_stop_background_threads(*log_sys);
      }

      ut_ad(buf_pool_pending_io_reads_count() == 0);

      err = log_files_reset_creator_and_set_full(*log_sys);
      if (err != DB_SUCCESS) {
        return srv_init_abort(err);
      }

      log_start_background_threads(*log_sys);

    } else {
      ut_a(redo_writes_allowed || srv_read_only_mode);
    }

    if (sum_of_new_sizes > 0) {
      ut_a(!srv_read_only_mode);

      /* New data file(s) were added */
      mtr_start(&mtr);

      fsp_header_inc_size(0, sum_of_new_sizes, &mtr);

      mtr_commit(&mtr);

      /* Immediately write the log record about
      increased tablespace size to disk, so that it
      is durable even if mysqld would crash
      quickly */

      log_buffer_flush_to_disk(*log_sys);
    }

    err = srv_undo_tablespaces_init(false);

    if (err != DB_SUCCESS && srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {
      return (srv_init_abort(err));
    }

    trx_purge_sys_mem_create();

    /* The purge system needs to create the purge view and
    therefore requires that the trx_sys is inited. */
    purge_queue = trx_sys_init_at_db_start();

    if (srv_is_upgrade_mode) {
      if (!purge_queue->empty()) {
        ib::info(ER_IB_MSG_1144);
        srv_upgrade_old_undo_found = true;
      }
      /* Either the old or new undo tablespaces will
      be deleted later depending on the value of
      'failed_upgrade' in dd_upgrade_finish(). */
    } else {
      /* New undo tablespaces have been created.
      Delete the old undo tablespaces and the references
      to them in the TRX_SYS page. */
      srv_undo_tablespaces_upgrade();
    }

    DBUG_EXECUTE_IF("check_no_undo", ut_ad(purge_queue->empty()););

    /* The purge system needs to create the purge view and
    therefore requires that the trx_sys and trx lists were
    initialized in trx_sys_init_at_db_start(). */
    trx_purge_sys_initialize(srv_threads.m_purge_workers_n, purge_queue);
  }

  /* Open temp-tablespace and keep it open until shutdown. */
  err = srv_open_tmp_tablespace(create_new_db, &srv_tmp_space);
  if (err != DB_SUCCESS) {
    return (srv_init_abort(err));
  }

  err = ibt::open_or_create(create_new_db);
  if (err != DB_SUCCESS) {
    return (srv_init_abort(err));
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
  ut_a(srv_rollback_segments <= TRX_SYS_N_RSEGS);

  /* Make sure there are enough rollback segments in each tablespace
  and that each rollback segment has an associated memory object.
  If any of these rollback segments contain undo logs, load them into
  the purge queue */
  if (!trx_rseg_adjust_rollback_segments(srv_rollback_segments)) {
    return (srv_init_abort(DB_ERROR));
  }

  /* Any undo tablespaces under construction are now fully built
  with all needed rsegs. Delete the trunc.log files and clear the
  construction list. */
  srv_undo_tablespaces_mark_construction_done();

  /* Now that all rsegs are ready for use, make them active. */
  undo::spaces->s_lock();
  for (auto undo_space : undo::spaces->m_spaces) {
    if (!undo_space->is_empty()) {
      undo_space->set_active();
    }
  }
  undo::spaces->s_unlock();

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

  sum_of_data_file_sizes = srv_sys_space.get_sum_of_sizes();
  ut_a(sum_of_new_sizes != FIL_NULL);

  tablespace_size_in_header = fsp_header_get_tablespace_size();

  if (!srv_read_only_mode && !srv_sys_space.can_auto_extend_last_file() &&
      sum_of_data_file_sizes != tablespace_size_in_header) {
    ib::error(ER_IB_MSG_1147, ulong{tablespace_size_in_header},
              ulong{sum_of_data_file_sizes});

    if (srv_force_recovery == 0 &&
        sum_of_data_file_sizes < tablespace_size_in_header) {
      /* This is a fatal error, the tail of a tablespace is
      missing */

      ib::error(ER_IB_MSG_1148);

      return (srv_init_abort(DB_ERROR));
    }
  }

  if (!srv_read_only_mode && srv_sys_space.can_auto_extend_last_file() &&
      sum_of_data_file_sizes < tablespace_size_in_header) {
    ib::error(ER_IB_MSG_1149, ulong{tablespace_size_in_header},
              ulong{sum_of_data_file_sizes});

    if (srv_force_recovery == 0) {
      ib::error(ER_IB_MSG_1150);

      return (srv_init_abort(DB_ERROR));
    }
  }

  /* Finish clone files recovery. This call is idempotent and is no op
  if it is already done before creating new log files. */
  clone_files_recovery(true);

  ib::info(ER_IB_MSG_1151, INNODB_VERSION_STR,
           ulonglong{log_get_lsn(*log_sys)});

  return (DB_SUCCESS);
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
              log_make_latest_checkpoint()).
    The reason was to make the situation more deterministic during
    the startup, because then:
            - it is easier to write mtr tests,
            - there are less possible flows - smaller risk of bug.
    Now we start allowing periodical checkpoints! Since now, it's
    hard to predict when checkpoints are written! */
    log_limits_mutex_enter(*log_sys);
    log_sys->periodical_checkpoints_enabled = true;
    log_limits_mutex_exit(*log_sys);
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

      log_stop_background_threads(*log_sys);
    }

    /* No redo log might be generated since now. */
    log_background_threads_inactive_validate();

    srv_shutdown_set_state(SRV_SHUTDOWN_LAST_PHASE);

    return (log_get_lsn(*log_sys));
  }

  if (!srv_read_only_mode) {
    log_make_empty_and_stop_background_threads(*log_sys);
  }

  /* No redo log might be generated since now. */
  log_background_threads_inactive_validate();
  buf_assert_all_are_replaceable();

  const lsn_t lsn = log_get_lsn(*log_sys);

  if (!srv_read_only_mode) {
    /* Redo log has been flushed at the log_flusher's exit. */
    fil_flush_file_spaces();
  }

  srv_shutdown_set_state(SRV_SHUTDOWN_LAST_PHASE);

  /* Validate lsn and write it down. */
  ut_a(log_is_data_lsn(lsn) || srv_force_recovery >= SRV_FORCE_NO_LOG_REDO);

  ut_a(lsn == log_sys->last_checkpoint_lsn.load() ||
       srv_force_recovery >= SRV_FORCE_NO_LOG_REDO);

  ut_a(lsn == log_get_lsn(*log_sys));

  if (!srv_read_only_mode) {
    ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);

    auto err = fil_write_flushed_lsn(lsn);
    ut_a(err == DB_SUCCESS);
  }

  buf_assert_all_are_replaceable();
  ut_a(lsn == log_get_lsn(*log_sys));

  if (srv_downgrade_logs) {
    ut_a(!srv_read_only_mode);

    /* InnoDB in any version is able to start on empty set of redo files. */
    log_files_remove(*log_sys);
  }

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

  /* 3. Close all opened files. */
  ibt::close_files();
  fil_close_all_files();
  if (srv_monitor_file) {
    fclose(srv_monitor_file);
  }
  if (srv_misc_tmpfile) {
    fclose(srv_misc_tmpfile);
  }

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
  log_sys_close();
  recv_sys_close();
  trx_sys_close();
  lock_sys_close();
  trx_pool_close();

  dict_close();
  dict_persist_close();
  undo_spaces_deinit();
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

/** Call std::_Exit(3) */
void srv_fatal_error() {
  ib::error(ER_IB_MSG_1156);

  fflush(stderr);

  ut_d(innodb_calling_exit = true);

  flush_error_log_messages();

  std::_Exit(3);
}
