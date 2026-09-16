/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
*/

#include "fil0innodb_tablespaces_nodes.h"
#include <limits>
#include "fil0innodb_tablespace_node_handle.h"
#include "fil0tablespace_scan.h"
#include "fsp0fsp.h"
#include "os0file.h"
#include "scope_guard.h"
#include "trx0purge.h"
#include "trx0undo_trunc.h"
#include "ut0dbg.h" /* ut_error */

#ifdef UNIV_PFS_IO
static const mysql_pfs_key_t &get_pfs_key(space_id_t id) {
  return fsp_is_system_temporary(id) ? innodb_temp_file_key
                                     : innodb_data_file_key;
}
#endif /* UNIV_PFS_IO */

namespace ib::fil {

Tablespaces_nodes::Capabilities Tablespaces_nodes::get_capabilities() {
  Capabilities caps;
  caps.supports_dblwr = true;
  caps.supports_import_tablespace = true;
  caps.supports_discard_tablespace = true;
  caps.supports_bulk_load = true;
  caps.supports_clone = true;
  caps.supports_transparent_data_encryption = true;
  caps.supports_transparent_page_compression = true;
  caps.supports_raw_devices = true;
  return caps;
}

ut::Expected<ut::unique_ptr<Tablespace_node_handle_interface>,
             Tablespaces_nodes_interface::Create_error>
Tablespaces_nodes::create(Tablespace_id space_id, size_t node_order,
                          const Create_node_hints &hints, uint32_t flags,
                          page_no_t size_in_pages) {
  const bool is_undo_tablespace = fsp_is_undo_tablespace(space_id);
  const bool is_temporary_tablespace = fsp_is_system_temporary(space_id);
  const auto &node_path = hints.m_base_hints.m_path.c_str();

  /* Create the subdirectories in the path, if they are not there already. */
  const bool is_shared_space = FSP_FLAGS_GET_SHARED(flags);
  if (!is_temporary_tablespace && (!is_shared_space || is_undo_tablespace)) {
    const auto err = os_file_create_subdirs_if_needed(node_path);

    if (err != DB_SUCCESS) {
      /* For UNDO tablespace it is expected to always pass? Because in trunk,
      return value is being ignored? Keeping following assert to confirm it. */
      ut_ad(!is_undo_tablespace);
      return ut::Unexpected(Create_error::IO_ERROR);
    }
  }

  const auto purpose = OS_DATA_FILE;
  const bool enforce_readonly_checks =
      srv_read_only_mode && !is_temporary_tablespace;
  auto create_mode =
      (hints.m_base_hints.m_is_raw_disk ? OS_FILE_OPEN_RAW : OS_FILE_CREATE) |
      OS_FILE_ON_ERROR_NO_EXIT;

  bool success = false;
  auto handle = os_file_create(get_pfs_key(space_id), node_path, create_mode,
                               purpose, enforce_readonly_checks, &success);

  if (!success) {
    /* The following call will print an error message */
    const auto error = os_file_get_and_log_last_error();

    switch (error) {
      case OS_FILE_ALREADY_EXISTS:
        return ut::Unexpected(Create_error::NODE_EXISTS);

      case OS_FILE_NAME_TOO_LONG:
        return ut::Unexpected(Create_error::FILE_NAME_TOO_LONG);

      case OS_FILE_DISK_FULL:
        return ut::Unexpected(Create_error::OUT_OF_DISK_SPACE);

      default:
        return ut::Unexpected(Create_error::IO_ERROR);
    }
  }

  auto delete_file_guard =
      create_scope_guard([&handle, &node_path, space_id]() {
        /* [[maybe_unused]] is not possible in lambda capture, and it can be
        unused if UNIV_PFS_IO is not defined. */
        (void)space_id;
        os_file_close(handle);
        os_file_delete(get_pfs_key(space_id), node_path);
      });

#ifndef UNIV_HOTBACKUP
  ut_d(undo_truncate::inject_crash("fixup_crash_after_creating_empty_file"));
#endif

  /* Special handling for UNDO tablespace */
  if (is_undo_tablespace) {
    ut_a(!srv_read_only_mode);

    /* We created the data file and now write it full of zeros */
    ib::info(ER_IB_MSG_CREATING_UNDO, node_path);

    ulint size_mb = UNDO_INITIAL_SIZE >> 20;

    ib::info(ER_IB_MSG_SETTING_FILE_SIZE, node_path, ulonglong{size_mb});

    ib::info(ER_IB_MSG_WRITING_FILE_FULL);
  }

  const auto physical_page_size = page_size_t{flags}.physical();

  if (size_in_pages > 0) {
    /* Writes NULLs to file pages */
    const auto initial_size_in_bytes = size_in_pages * physical_page_size;
    auto err = os_file_fill_range_with_zeros(
        node_path, handle, 0 /* offset */, initial_size_in_bytes,
        true /* flush */, tbsp_extend_and_initialize /* force_raw_writes */);

    /* Special handling for UNDO tablespace */
    DBUG_EXECUTE_IF(
        "ib_undo_tablespace_create_fail",
        if (is_undo_tablespace) { err = DB_ERROR; });

    if (err != DB_SUCCESS) {
      return ut::Unexpected(is_undo_tablespace ? Create_error::OUT_OF_DISK_SPACE
                                               : Create_error::IO_ERROR);
    }

    DBUG_EXECUTE_IF(
        "fixup_crash_after_init_with_zeros",
        ib::info(ER_IB_MSG_INJECT_CRASH, "fixup_crash_after_init_with_zeros");
        DBUG_SUICIDE(););

    if (node_order == 0) {
      /* Write few initial pages of tablespace */
      const auto write_err =
          fil_write_initial_pages(handle, node_path, nullptr, space_id, flags);
      if (write_err != DB_SUCCESS) {
        /* This writing of initial pages is specific to InnoDB implementation.
         */
        ib::error(ER_IB_MSG_WRITE_INITIAL_PAGES, node_path);
        return ut::Unexpected(write_err == DB_OUT_OF_FILE_SPACE
                                  ? Create_error::OUT_OF_DISK_SPACE
                                  : Create_error::IO_ERROR);
      }
    }
  }
  delete_file_guard.release();

  return ut::unique_ptr<Tablespace_node_handle_interface>{
      ut::new_<Tablespace_node_handle>(std::move(handle), node_path, space_id,
                                       physical_page_size, node_order)};
}

ut::Expected<ut::unique_ptr<Tablespace_node_handle_interface>,
             Tablespaces_nodes_interface::Open_error>
Tablespaces_nodes::open_undo_tablespace(const space_id_t space_id,
                                        const Node_hints &hints,
                                        const size_t page_size,
                                        bool for_read_only) {
  pfs_os_file_t handle;
  bool success = false;
  /* All the temporary undo logs are stored in the system temporary tablespace,
  so are not opened through this API */
  ut_ad(!fsp_is_system_temporary(space_id));
  handle = os_file_create(
      innodb_data_file_key, hints.m_path.c_str(),
      OS_FILE_OPEN_RETRY | OS_FILE_ON_ERROR_NO_EXIT | OS_FILE_ON_ERROR_SILENT,
      OS_DATA_FILE, for_read_only, &success);

  if (!success) {
    /* If the file does not exist, the `os_file_check_mode()` returns true. */
    if (!os_file_check_mode(
#ifdef UNIV_PFS_IO
            innodb_data_file_key,
#endif /* UNIV_PFS_IO */
            hints.m_path.c_str(), hints.m_is_raw_disk, for_read_only)) {
      ib::error(ER_IB_MSG_UNDO_TABLESPACE_WRONG_MODE, hints.m_path.c_str(),
                for_read_only ? "readable!" : "writable!");
      return ut::Unexpected(Open_error::NO_ACCESS_PERMISSIONS);
    }
    return ut::Unexpected(Open_error::NODE_DOES_NOT_EXIST);
  }

  return ut::unique_ptr<Tablespace_node_handle_interface>{
      ut::new_<Tablespace_node_handle>(std::move(handle), hints.m_path,
                                       space_id, page_size, 0)};
}

ut::Expected<ut::unique_ptr<Tablespace_node_handle_interface>,
             Tablespaces_nodes_interface::Open_error>
Tablespaces_nodes::open(Tablespace_id space_id, size_t node_order,
                        const Node_hints &hints, size_t page_size,
                        bool for_read_only) {
  if (fsp_is_undo_tablespace(space_id)) {
    return open_undo_tablespace(space_id, hints, page_size, for_read_only);
  }

  pfs_os_file_t handle;
  bool success = false;

#ifdef UNIV_PFS_IO
  const auto &file_key = get_pfs_key(space_id);
#endif

  /* Open the file on Windows in the unbuffered async I/O mode, though global
  variables may make os_file_create() to fall back to the normal file I/O
  mode. */
  if (hints.m_is_raw_disk) {
    ut_a(!for_read_only);
    handle = os_file_create(file_key, hints.m_path.c_str(), OS_FILE_OPEN_RAW,
                            OS_DATA_FILE, false, &success);
  } else if (for_read_only) {
    handle = os_file_create(file_key, hints.m_path.c_str(), OS_FILE_OPEN,
                            OS_DATA_FILE, true, &success);
  } else {
    handle = os_file_create(file_key, hints.m_path.c_str(),
                            (node_order == 0 && space_id == TRX_SYS_SPACE)
                                ? OS_FILE_OPEN_RETRY
                                : OS_FILE_OPEN,
                            OS_DATA_FILE, false, &success);
  }

  if (!success) {
    /* If the file does not exist, the `os_file_check_mode()` returns true. */
    if (!os_file_check_mode(
#ifdef UNIV_PFS_IO
            file_key,
#endif /* UNIV_PFS_IO */
            hints.m_path.c_str(), hints.m_is_raw_disk, for_read_only)) {
      return ut::Unexpected(Open_error::NO_ACCESS_PERMISSIONS);
    }
    return ut::Unexpected(Open_error::NODE_DOES_NOT_EXIST);
  }

  return ut::unique_ptr<Tablespace_node_handle_interface>{
      ut::new_<Tablespace_node_handle>(std::move(handle), hints.m_path,
                                       space_id, page_size, node_order)};
}

Tablespaces_nodes::Status Tablespaces_nodes::rename(
    space_id_t space_id, size_t /* node_order */, const std::string &old_path,
    const std::string &new_path) {
  const bool renamed_successfully =
      os_file_rename(get_pfs_key(space_id), old_path.c_str(), new_path.c_str());
  /* We can`t assert the rename succeeded as it may fail if we were to move the
  file between different file systems. */

  return renamed_successfully ? Tablespaces_nodes_interface::Status::SUCCESS
                              : Tablespaces_nodes_interface::Status::IO_ERROR;
}

[[nodiscard]] static Tablespaces_nodes_interface::Status remove_if_exists(
    const char *path
#ifdef UNIV_PFS_IO
    ,
    const mysql_pfs_key_t &file_key
#endif
) {
  if (!os_file_exists(path)) {
    return Tablespaces_nodes_interface::Status::SUCCESS;
  }
  /* TODO : Why do we try twice below? */
  if (!os_file_delete(file_key, path) &&
      !os_file_delete_if_exists(file_key, path, nullptr)) {
    /* TODO : What does the below 'old' comment mean? */
    /* Note: This is because we have removed the tablespace instance from the
    cache. */
    return Tablespaces_nodes_interface::Status::IO_ERROR;
  }
  return Tablespaces_nodes_interface::Status::SUCCESS;
}

Tablespaces_nodes::Status Tablespaces_nodes::remove(Tablespace_id space_id,
                                                    size_t /* node_order */,
                                                    const Node_hints &hints) {
#ifdef UNIV_PFS_IO
  const auto &file_key = get_pfs_key(space_id);
#endif
  /* Additionally delete any generated files, otherwise when we drop the
  database the remove directory will fail. */
  for (auto func : {Fil_path::make_cfg, Fil_path::make_cfp}) {
    if (char *name = func(hints.m_path.c_str()); name) {
      os_file_delete_if_exists(file_key, name, nullptr);
      ut::free(name);
    }
  }

  return remove_if_exists(hints.m_path.c_str()
#ifdef UNIV_PFS_IO
                              ,
                          file_key
#endif
  );
}

ut::Expected<Tablespaces_nodes_interface::Node_info,
             Tablespaces_nodes_interface::Node_error>
Tablespaces_nodes::get_node_info(Tablespace_id space_id,
                                 size_t /* node_order */,
                                 const Node_hints &hints, size_t page_size) {
  os_file_stat_t stat{};

  /* As per specification, SPACE_UNKNOWN means we are not interested in the
  node storage size and page_size must be 0. */
  ut_a(space_id != SPACE_UNKNOWN || page_size == 0);
  /* As per specification, SPACE_UNKNOWN means we are not interested in the
  access_permissions and thus m_check_permissions must be false. This part of
  the contract is important as checking permissions requires opening a file,
  which in turn registers a path in the PFS under specified pfs key, and not
  knowing space_id we don't know which of the keys to use. */
  ut_a(space_id != SPACE_UNKNOWN || !hints.m_check_permissions);

  switch (os_file_get_status(hints.m_path.c_str(), &stat)) {
    case DB_FAIL:
      return ut::Unexpected(Node_error::IO_ERROR);

    case DB_SUCCESS:
      if (stat.type == OS_FILE_TYPE_FILE) {
        ut_a(page_size == 0 ||
             stat.size / page_size <= std::numeric_limits<page_no_t>::max());
        const auto access_permissions =
            hints.m_check_permissions
                ? os_file_check_access(
#ifdef UNIV_PFS_IO
                      get_pfs_key(space_id),
#endif /* UNIV_PFS_IO */
                      hints.m_path.c_str(), hints.m_is_raw_disk)
                : Access_permissions{};
        return Node_info{(page_size != 0)
                             ? static_cast<page_no_t>(stat.size / page_size)
                             : 0,
                         stat.block_size, stat.alloc_size, access_permissions};
      } else {
        return ut::Unexpected(Node_error::NOT_A_NODE);
      }

    case DB_NOT_FOUND:
      return ut::Unexpected(Node_error::NODE_DOES_NOT_EXIST);

    default:
      ut_error;
  }
}
} /* namespace ib::fil */
