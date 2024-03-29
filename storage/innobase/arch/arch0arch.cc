/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

/** @file arch/arch0arch.cc
 Common implementation for redo log and dirty page archiver system

 *******************************************************/

#include "arch0arch.h"
#include "os0thread-create.h"

/** Log Archiver system global */
Arch_Log_Sys *arch_log_sys = nullptr;

/** Page Archiver system global */
Arch_Page_Sys *arch_page_sys = nullptr;

/** Event to signal the log archiver thread. */
os_event_t log_archiver_thread_event;

/** Wakes up archiver threads.
@return true iff any thread was still alive */
bool arch_wake_threads() {
  bool found_alive = false;
  if (srv_thread_is_active(srv_threads.m_log_archiver)) {
    os_event_set(log_archiver_thread_event);
    found_alive = true;
  }
  if (srv_thread_is_active(srv_threads.m_page_archiver)) {
    os_event_set(page_archiver_thread_event);
    found_alive = true;
  }
  return (found_alive);
}

void arch_remove_file(const char *file_path, const char *file_name) {
  char path[MAX_ARCH_PAGE_FILE_NAME_LEN];

  static_assert(MAX_ARCH_LOG_FILE_NAME_LEN <= MAX_ARCH_PAGE_FILE_NAME_LEN);
  ut_ad(strlen(file_path) + 1 + strlen(file_name) <
        MAX_ARCH_PAGE_FILE_NAME_LEN);

  /* Remove only LOG and PAGE archival files. */
  if (0 != strncmp(file_name, ARCH_LOG_FILE, strlen(ARCH_LOG_FILE)) &&
      0 != strncmp(file_name, ARCH_PAGE_FILE, strlen(ARCH_PAGE_FILE)) &&
      0 != strncmp(file_name, ARCH_PAGE_GROUP_DURABLE_FILE_NAME,
                   strlen(ARCH_PAGE_GROUP_DURABLE_FILE_NAME))) {
    return;
  }

  snprintf(path, sizeof(path), "%s%c%s", file_path, OS_PATH_SEPARATOR,
           file_name);

#ifdef UNIV_DEBUG
  os_file_type_t type;
  bool exists;

  os_file_status(path, &exists, &type);
  ut_ad(exists);
  ut_ad(type == OS_FILE_TYPE_FILE);
#endif /* UNIV_DEBUG */

  os_file_delete(innodb_arch_file_key, path);
}

void arch_remove_dir(const char *dir_path, const char *dir_name) {
  char path[MAX_ARCH_DIR_NAME_LEN];

  static_assert(sizeof(ARCH_LOG_DIR) <= sizeof(ARCH_PAGE_DIR));
  ut_ad(strlen(dir_path) + 1 + strlen(dir_name) + 1 < sizeof(path));

  /* Remove only LOG and PAGE archival directories. */
  if (0 != strncmp(dir_name, ARCH_LOG_DIR, strlen(ARCH_LOG_DIR)) &&
      0 != strncmp(dir_name, ARCH_PAGE_DIR, strlen(ARCH_PAGE_DIR))) {
    return;
  }

  snprintf(path, sizeof(path), "%s%c%s", dir_path, OS_PATH_SEPARATOR, dir_name);

#ifdef UNIV_DEBUG
  os_file_type_t type;
  bool exists;

  os_file_status(path, &exists, &type);
  ut_ad(exists);
  ut_ad(type == OS_FILE_TYPE_DIR);
#endif /* UNIV_DEBUG */

  os_file_scan_directory(path, arch_remove_file, true);
}

/** Initialize Page and Log archiver system
@return error code */
dberr_t arch_init() {
  if (arch_log_sys == nullptr) {
    arch_log_sys =
        ut::new_withkey<Arch_Log_Sys>(ut::make_psi_memory_key(mem_key_archive));

    if (arch_log_sys == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    log_archiver_thread_event = os_event_create();
  }

  if (arch_page_sys == nullptr) {
    arch_page_sys = ut::new_withkey<Arch_Page_Sys>(
        ut::make_psi_memory_key(mem_key_archive));

    if (arch_page_sys == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    page_archiver_thread_event = os_event_create();
  }

  if (srv_read_only_mode) {
    arch_page_sys->set_read_only_mode();
    return DB_SUCCESS;
  }

  arch_page_sys->recover();

  return DB_SUCCESS;
}

/** Free Page and Log archiver system */
void arch_free() {
  if (arch_log_sys != nullptr) {
    ut::delete_(arch_log_sys);
    arch_log_sys = nullptr;

    os_event_destroy(log_archiver_thread_event);
  }

  if (arch_page_sys != nullptr) {
    ut::delete_(arch_page_sys);
    arch_page_sys = nullptr;

    os_event_destroy(page_archiver_thread_event);
  }
}

dberr_t Arch_Group::prepare_file_with_header(
    uint64_t start_offset, Get_file_header_callback &get_header) {
  ut::aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE> header{};
  header.alloc(ut::Count{m_header_len});

  dberr_t err = get_header(start_offset, header);
  if (err != DB_SUCCESS) {
    return err;
  }

  err = m_file_ctx.open_new(m_begin_lsn, m_file_size, 0);

  if (err != DB_SUCCESS) {
    return err;
  }

  return m_file_ctx.write(nullptr, header, m_header_len);
}

dberr_t Arch_Group::write_to_file(Arch_File_Ctx *from_file, byte *from_buffer,
                                  uint length, bool partial_write,
                                  bool do_persist,
                                  Get_file_header_callback get_header) {
  dberr_t err = DB_SUCCESS;
  uint write_size;

  if (m_file_ctx.is_closed()) {
    /* First file in the archive group. */
    ut_ad(m_file_ctx.get_count() == 0);

    DBUG_EXECUTE_IF("crash_before_archive_file_creation", DBUG_SUICIDE(););

    err = prepare_file_with_header(0, get_header);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  auto len_left = m_file_ctx.bytes_left();

  uint64_t start_offset = 0;

  /* New file is immediately opened when current file is over. */
  ut_ad(len_left != 0);

  while (length > 0) {
    auto len_copy = static_cast<uint64_t>(length);

    /* Write as much as possible in current file. */
    if (len_left < len_copy) {
      ut_ad(len_left <= std::numeric_limits<uint>::max());
      write_size = static_cast<uint>(len_left);
    } else {
      write_size = length;
    }

    if (do_persist) {
      Arch_Page_Dblwr_Offset dblwr_offset =
          (partial_write ? ARCH_PAGE_DBLWR_PARTIAL_FLUSH_PAGE
                         : ARCH_PAGE_DBLWR_FULL_FLUSH_PAGE);

      Arch_Group::write_to_doublewrite_file(from_file, from_buffer, write_size,
                                            dblwr_offset);
    }

    if (partial_write) {
      DBUG_EXECUTE_IF("crash_after_partial_block_dblwr_flush", DBUG_SUICIDE(););
      err = m_file_ctx.write(from_file, from_buffer,
                             static_cast<uint>(m_file_ctx.get_offset()),
                             write_size);
    } else {
      DBUG_EXECUTE_IF("crash_after_full_block_dblwr_flush", DBUG_SUICIDE(););
      err = m_file_ctx.write(from_file, from_buffer, write_size);
    }

    if (err != DB_SUCCESS) {
      return (err);
    }

    if (do_persist) {
      /* Flush the file to make sure the changes are made persistent as there
      would be no way to recover the data otherwise in case of a crash. */
      m_file_ctx.flush();
    }

    ut_ad(length >= write_size);
    length -= write_size;
    start_offset += write_size;

    len_left = m_file_ctx.bytes_left();

    /* Current file is over, switch to next file. */
    if (len_left == 0) {
      m_file_ctx.close();

      err = prepare_file_with_header(start_offset, get_header);

      if (err != DB_SUCCESS) {
        return (err);
      }

      DBUG_EXECUTE_IF("crash_after_archive_file_creation", DBUG_SUICIDE(););

      len_left = m_file_ctx.bytes_left();
    }
  }

  return (DB_SUCCESS);
}

bool Arch_File_Ctx::delete_file(uint file_index, lsn_t begin_lsn) {
  bool success;
  char file_name[MAX_ARCH_PAGE_FILE_NAME_LEN];

  build_name(file_index, begin_lsn, file_name, MAX_ARCH_PAGE_FILE_NAME_LEN);

  os_file_type_t type;
  bool exists;

  success = os_file_status(file_name, &exists, &type);

  if (!success || !exists) {
    return (false);
  }

  ut_ad(type == OS_FILE_TYPE_FILE);

  success = os_file_delete(innodb_arch_file_key, file_name);

  return (success);
}

void Arch_File_Ctx::delete_files(lsn_t begin_lsn) {
  bool exists;
  os_file_type_t type;
  char dir_name[MAX_ARCH_DIR_NAME_LEN];

  build_dir_name(begin_lsn, dir_name, MAX_ARCH_DIR_NAME_LEN);
  os_file_status(dir_name, &exists, &type);

  if (exists) {
    ut_ad(type == OS_FILE_TYPE_DIR);
    os_file_scan_directory(dir_name, arch_remove_file, true);
  }
}

dberr_t Arch_File_Ctx::init(const char *path, const char *base_dir,
                            const char *base_file, uint num_files) {
  m_base_len = static_cast<uint>(strlen(path));

  m_name_len =
      m_base_len + static_cast<uint>(strlen(base_file)) + MAX_LSN_DECIMAL_DIGIT;

  if (base_dir != nullptr) {
    m_name_len += static_cast<uint>(strlen(base_dir));
    m_name_len += MAX_LSN_DECIMAL_DIGIT;
  }

  /* Add some extra buffer. */
  m_name_len += MAX_LSN_DECIMAL_DIGIT;

  /* In case of reinitialise. */
  if (m_name_buf != nullptr) {
    ut::free(m_name_buf);
    m_name_buf = nullptr;
  }

  m_name_buf = static_cast<char *>(
      ut::malloc_withkey(ut::make_psi_memory_key(mem_key_archive), m_name_len));

  if (m_name_buf == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  m_path_name = path;
  m_dir_name = base_dir;
  m_file_name = base_file;

  strcpy(m_name_buf, path);

  if (m_name_buf[m_base_len - 1] != OS_PATH_SEPARATOR) {
    m_name_buf[m_base_len] = OS_PATH_SEPARATOR;
    ++m_base_len;
    m_name_buf[m_base_len] = '\0';
  }

  m_file.m_file = OS_FILE_CLOSED;

  m_index = 0;
  m_count = num_files;

  m_offset = 0;

  m_reset.clear();
  m_stop_points.clear();

  return (DB_SUCCESS);
}

dberr_t Arch_File_Ctx::open(bool read_only, lsn_t start_lsn, uint file_index,
                            uint64_t file_offset, uint64_t file_size) {
  /* Close current file, if open. */
  close();

  m_index = file_index;
  m_offset = file_offset;

  build_name(m_index, start_lsn, nullptr, 0);

  bool exists;
  os_file_type_t type;

  bool success = os_file_status(m_name_buf, &exists, &type);

  if (!success) {
    return (DB_CANNOT_OPEN_FILE);
  }

  os_file_create_t option;

  if (read_only) {
    if (!exists) {
      return (DB_CANNOT_OPEN_FILE);
    }

    option = OS_FILE_OPEN;
  } else {
    option = exists ? OS_FILE_OPEN : OS_FILE_CREATE_PATH;
  }

  m_file =
      os_file_create(innodb_arch_file_key, m_name_buf, option, OS_FILE_NORMAL,
                     OS_CLONE_LOG_FILE, read_only, &success);

  if (!success) {
    return (DB_CANNOT_OPEN_FILE);
  }

  /* For newly created file, zero fill the header section. This is required
  for archived redo files that are just created. Clone expects the header
  length to be written. */
  if (!exists && file_offset != 0 && !read_only) {
    /* This call would extend the length by multiple of UNIV_PAGE_SIZE. This is
    not an issue but we need to lseek to keep the current position at offset. */
    success = os_file_set_size(m_name_buf, m_file, 0, file_offset, false);

    exists = success;
  }

  if (success) {
    success = os_file_seek(m_name_buf, m_file.m_file, file_offset);
  }

  m_size = file_size == 0
               ? (exists ? os_file_get_size(m_name_buf).m_total_size : 0)
               : file_size;
  ut_ad(m_offset <= m_size);

  if (!success) {
    close();
  }

  return (success ? DB_SUCCESS : DB_IO_ERROR);
}

dberr_t Arch_File_Ctx::open_new(lsn_t start_lsn, uint64_t new_file_size,
                                uint64_t initial_file_size) {
  auto err = open(false, start_lsn, m_count, initial_file_size, new_file_size);
  if (err != DB_SUCCESS) {
    return err;
  }
  ++m_count;
  return DB_SUCCESS;
}

dberr_t Arch_File_Ctx::open_next(lsn_t start_lsn, uint64_t file_offset,
                                 uint64_t file_size) {
  dberr_t error;

  /* Get next file index. */
  ++m_index;

  /* Open next file. */
  error = open(true, start_lsn, m_index, file_offset, file_size);

  return (error);
}

dberr_t Arch_File_Ctx::read(byte *to_buffer, uint64_t offset, uint size) {
  ut_ad(offset + size <= m_size);
  ut_ad(!is_closed());

  IORequest request(IORequest::READ);
  request.disable_compression();
  request.clear_encrypted();

  auto err =
      os_file_read(request, m_path_name, m_file, to_buffer, offset, size);

  return (err);
}

dberr_t Arch_File_Ctx::resize_and_overwrite_with_zeros(uint64_t file_size) {
  ut_ad(m_size <= file_size);

  m_size = file_size;

  byte *buf = static_cast<byte *>(
      ut::zalloc_withkey(ut::make_psi_memory_key(mem_key_archive), file_size));

  /* Make sure that the physical file size is the same as logical by filling
  the file with all-zeroes. Page archiver recovery expects that the physical
  file size is the same as logical file size. */
  const dberr_t err = write(nullptr, buf, 0, file_size);

  ut::free(buf);

  if (err != DB_SUCCESS) {
    return err;
  }

  flush();
  return DB_SUCCESS;
}

dberr_t Arch_File_Ctx::write(Arch_File_Ctx *from_file, byte *from_buffer,
                             uint size) {
  dberr_t err;

  if (from_buffer == nullptr) {
    /* write from File */
    err = os_file_copy(from_file->m_file, from_file->m_offset, m_file, m_offset,
                       size);

    if (err == DB_SUCCESS) {
      from_file->m_offset += size;
      ut_ad(from_file->m_offset <= from_file->m_size);
    }

  } else {
    /* write from buffer */
    IORequest request(IORequest::WRITE);
    request.disable_compression();
    request.clear_encrypted();

    err = os_file_write(request, "Track file", m_file, from_buffer, m_offset,
                        size);
  }

  if (err != DB_SUCCESS) {
    return (err);
  }

  m_offset += size;
  ut_ad(m_offset <= m_size);

  return (DB_SUCCESS);
}

void Arch_File_Ctx::build_name(uint idx, lsn_t dir_lsn, char *buffer,
                               uint length) {
  char *buf_ptr;
  uint buf_len;

  /* If user has passed NULL, use pre-allocated buffer. */
  if (buffer == nullptr) {
    buf_ptr = m_name_buf;
    buf_len = m_name_len;
  } else {
    buf_ptr = buffer;
    buf_len = length;

    strncpy(buf_ptr, m_name_buf, buf_len);
  }

  buf_ptr += m_base_len;
  buf_len -= m_base_len;

  if (m_dir_name == nullptr) {
    snprintf(buf_ptr, buf_len, "%s%u", m_file_name, idx);

  } else if (dir_lsn == LSN_MAX) {
    snprintf(buf_ptr, buf_len, "%s%c%s%u", m_dir_name, OS_PATH_SEPARATOR,
             m_file_name, idx);

  } else {
    snprintf(buf_ptr, buf_len, "%s" UINT64PF "%c%s%u", m_dir_name, dir_lsn,
             OS_PATH_SEPARATOR, m_file_name, idx);
  }
}

void Arch_File_Ctx::build_dir_name(lsn_t dir_lsn, char *buffer, uint length) {
  ut_ad(buffer != nullptr);

  if (m_dir_name != nullptr) {
    snprintf(buffer, length, "%s%c%s" UINT64PF, m_path_name, OS_PATH_SEPARATOR,
             m_dir_name, dir_lsn);
  } else {
    snprintf(buffer, length, "%s", m_path_name);
  }
}

int start_log_archiver_background() {
  bool ret;
  char errbuf[MYSYS_STRERROR_SIZE];

  ret = os_file_create_directory(ARCH_DIR, false);

  if (ret) {
    srv_threads.m_log_archiver =
        os_thread_create(log_archiver_thread_key, 0, log_archiver_thread);

    srv_threads.m_log_archiver.start();

  } else {
    my_error(ER_CANT_CREATE_FILE, MYF(0), ARCH_DIR, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (ER_CANT_CREATE_FILE);
  }

  return (0);
}

int start_page_archiver_background() {
  bool ret;
  char errbuf[MYSYS_STRERROR_SIZE];

  ret = os_file_create_directory(ARCH_DIR, false);

  if (ret) {
    srv_threads.m_page_archiver =
        os_thread_create(page_archiver_thread_key, 0, page_archiver_thread);

    srv_threads.m_page_archiver.start();

  } else {
    my_error(ER_CANT_CREATE_FILE, MYF(0), ARCH_DIR, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (ER_CANT_CREATE_FILE);
  }

  return (0);
}

/** Archiver background thread */
void log_archiver_thread() {
  Arch_File_Ctx log_file_ctx;
  lsn_t log_arch_lsn = LSN_MAX;

  bool log_abort = false;
  bool log_wait = false;
  bool log_init = true;

  while (true) {
    /* Archive available redo log data. */
    log_abort = arch_log_sys->archive(log_init, &log_file_ctx, &log_arch_lsn,
                                      &log_wait);

    if (log_abort) {
      ib::info(ER_IB_MSG_13) << "Exiting Log Archiver";
      break;
    }

    log_init = false;

    if (log_wait) {
      /* Nothing to archive. Wait until next trigger. */
      os_event_wait(log_archiver_thread_event);
      os_event_reset(log_archiver_thread_event);
    }
  }
}
