/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file log/log0files_io.cc

 *******************************************************/

#ifndef WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#include <windows.h>
#endif /* WIN32 */

/* std::sort */
#include <algorithm>

/* strncpy */
#include <cstring>

/* std::numeric_limits */
#include <limits>

/* std::ostringstream */
#include <sstream>

/* log_block_set_checksum, ... */
#include "log0files_io.h"

/* log_pre_8_0_30::FILE_BASE_NAME, ... */
#include "log0pre_8_0_30.h"

/* LOG_HEADER_FORMAT, ... */
#include "log0types.h"

/* mach_write_to_4, ... */
#include "mach0data.h"

/* DBUG_EXECUTE_IF, ... */
#include "my_dbug.h"

/* os_file_scan_directory */
#include "os0file.h"

/* ut::vector */
#include "ut0new.h"

/* ut::random_from_interval */
#include "ut0rnd.h"

/* srv_redo_log_encrypt */
#include "srv0srv.h"

Log_checksum_algorithm_atomic_ptr log_checksum_algorithm_ptr;

bool log_header_checksum_is_ok(const byte *buf) {
  DBUG_EXECUTE_IF("log_header_checksum_disabled", return true;);
  return log_block_get_checksum(buf) == log_block_calc_checksum_crc32(buf);
}

/** Used for reads/writes to redo files within this module.
Note that OS_FILE_LOG_BLOCK_SIZE can be smaller than a sector size,
and indeed redo file is being accessed at offsets and sizes not aligned
to sector size, which is fine, as it always uses buffered IO. */
struct Log_file_block {
  byte data[OS_FILE_LOG_BLOCK_SIZE];
};

/** Asserts that the provided file header seems correct.
@param[in]  header    file header data to validate */
static void log_file_header_validate(const Log_file_header &header);

/** Computes offset from the beginning of the redo file to the checkpoint
header for provided checkpoint header number.
@param[in]  checkpoint_header_no  checkpoint header for which offset to compute
@return offset from the beginning of the redo file */
static os_offset_t log_checkpoint_header_offset(
    Log_checkpoint_header_no checkpoint_header_no);

/** Asserts that the provided <offset, size> defines one or more redo
data blocks within a log file.
@param[in]  offset   expected offset to the beginning of data block(s)
@param[in]  size     size in bytes of data blocks starting from the offset */
static void log_data_blocks_validate(os_offset_t offset, os_offset_t size);

/** Converts flag number to the mask with this flag turned on.
@param[in]  bit   flag bit number (but > 0)
@return bit mask value (2^(bit-1)) */
static Log_flags log_file_header_flag_bit(uint32_t bit);

#ifndef _WIN32

/** Fsyncs the given directory. Fails on assertion if the directory
could not be opened.
@param[in]  path  path to directory to fsync */
static void log_flush_directory_low(const std::string &path);

#endif /* !_WIN32 */

/** Renames the log file.
@param[in]  context        redo log files context
@param[in]  old_file_path  path to the existing log file to rename
@param[in]  new_file_path  path to the renamed file
@param[in]  err_msg_id     id of the error message to print (if needed)
@return DB_SUCCESS or DB_ERROR */
static dberr_t log_rename_file_low(const Log_files_context &context
                                   [[maybe_unused]],
                                   const std::string &old_file_path,
                                   const std::string &new_file_path,
                                   int err_msg_id);

/** Delete the log file at the provided file path. Asserts that the
file has been deleted or does not exist.
@param[in]  context     redo log files context
@param[in]  file_path   path to the log file that should be removed
@param[in]  err_msg_id  id of the error message to print (if needed)
@return DB_SUCCESS, DB_NOT_FOUND or DB_ERROR */
static dberr_t log_remove_file_low(const Log_files_context &context,
                                   const std::string &file_path,
                                   int err_msg_id);

/** Resizes the log file at the provided file path.
@param[in]  file_path       path to the log file that should be resized
@param[in]  size_in_bytes   size to which resize, in bytes
@param[in]  err_msg_id      id of the error message to print (if needed)
@return DB_SUCCESS, DB_NOT_FOUND, DB_OUT_OF_DISK_SPACE or DB_ERROR */
static dberr_t log_resize_file_low(const std::string &file_path,
                                   os_offset_t size_in_bytes, int err_msg_id);

/** Checks if a log file exists and can be opened with srv_read_only_mode mode.
If that is successful, reads the size of the file and provides it.
@param[in]  ctx            redo log files context
@param[in]  file_id        id of the file to check and open
@param[in]  read_only      true: check file permissions only for reading
                           false: check for both reading and writing
@param[out] size_in_bytes  size of the file, in bytes
@return DB_SUCCESS, DB_NOT_FOUND or DB_ERROR */
static dberr_t log_check_file(const Log_files_context &ctx, Log_file_id file_id,
                              bool read_only, os_offset_t &size_in_bytes);

/**************************************************/ /**

 @name Log_file_handle implementation

 *******************************************************/

/** @{ */

Log_file_io_callback Log_file_handle::s_on_before_read;
Log_file_io_callback Log_file_handle::s_on_before_write;
bool Log_file_handle::s_skip_fsyncs = false;

#ifdef UNIV_DEBUG
std::atomic<size_t> Log_file_handle::s_n_open{0};
#endif /* UNIV_DEBUG */

std::atomic<uint64_t> Log_file_handle::s_total_fsyncs{0};
std::atomic<uint64_t> Log_file_handle::s_fsyncs_in_progress{0};

Log_file_handle::Log_file_handle(Encryption_metadata &encryption_metadata)
    : m_file_id{},
      m_access_mode{},
      m_encryption_metadata{encryption_metadata},
      m_file_type{},
      m_is_open{},
      m_is_modified{},
      m_file_path{},
      m_raw_handle{},
      m_block_size{},
      m_file_size{} {}

Log_file_handle::Log_file_handle(Log_file_handle &&other)
    : m_file_id{other.m_file_id},
      m_access_mode{other.m_access_mode},
      m_encryption_metadata{other.m_encryption_metadata},
      m_file_type{other.m_file_type},
      m_is_open{other.m_is_open},
      m_is_modified{other.m_is_modified},
      m_file_path{other.m_file_path},
      m_raw_handle{other.m_raw_handle},
      m_block_size{other.m_block_size},
      m_file_size{other.m_file_size} {
  other.m_is_modified = false;
  other.m_is_open = false;
  other.m_raw_handle = {};
}

Log_file_handle &Log_file_handle::operator=(Log_file_handle &&rhs) {
  if (m_is_open) {
    close();
  }

  m_file_id = rhs.m_file_id;
  m_access_mode = rhs.m_access_mode;
  m_file_type = rhs.m_file_type;
  m_file_path = rhs.m_file_path;
  m_block_size = rhs.m_block_size;
  m_file_size = rhs.m_file_size;
  ut_a(&m_encryption_metadata == &rhs.m_encryption_metadata);

  m_is_modified = rhs.m_is_modified;
  rhs.m_is_modified = false;
  m_is_open = rhs.m_is_open;
  rhs.m_is_open = false;
  m_raw_handle = rhs.m_raw_handle;
  rhs.m_raw_handle = {};
  return *this;
}

Log_file_handle::Log_file_handle(const Log_files_context &ctx, Log_file_id id,
                                 Log_file_access_mode access_mode,
                                 Encryption_metadata &encryption_metadata,
                                 Log_file_type file_type)
    : m_file_id(id),
      m_access_mode(access_mode),
      m_encryption_metadata(encryption_metadata),
      m_file_type(file_type),
      m_is_open(false),
      m_is_modified(false),
      m_file_path(file_type == Log_file_type::UNUSED
                      ? log_file_path_for_unused_file(ctx, id)
                      : log_file_path(ctx, id)) {
  const dberr_t err = open();
  if (err != DB_SUCCESS) {
    ut_a(!m_is_open);
    ib::error(ER_IB_MSG_LOG_FILE_OPEN_FAILED, m_file_path.c_str(),
              static_cast<int>(err));
  }
}

Log_file_handle::~Log_file_handle() {
  if (m_is_open) {
    close();
  }
}

const std::string &Log_file_handle::file_path() const { return m_file_path; }

dberr_t Log_file_handle::open() {
  const bool read_only = m_access_mode == Log_file_access_mode::READ_ONLY;
  os_file_stat_t stat_info;
  dberr_t err;
  err = os_file_get_status(m_file_path.c_str(), &stat_info, false, read_only);
  if (err != DB_SUCCESS) {
    return err;
  }

  m_block_size = stat_info.block_size;
  ut_a(m_block_size > 0);

  m_file_size = stat_info.size;

  ut_ad(s_n_open.fetch_add(1) + 1 <= LOG_MAX_OPEN_FILES);

  m_raw_handle =
      os_file_create(innodb_log_file_key, m_file_path.c_str(), OS_FILE_OPEN,
                     OS_FILE_NORMAL, OS_LOG_FILE, read_only, &m_is_open);
  if (m_is_open) {
    return DB_SUCCESS;
  }

  ut_ad(s_n_open.fetch_sub(1) > 0);
  return DB_ERROR;
}

void Log_file_handle::close() {
  ut_ad(is_open());
  if (m_is_modified) {
    fsync();
    m_is_modified = false;
  }
  os_file_close(m_raw_handle);
  m_is_open = false;

  ut_ad(s_n_open.fetch_sub(1) > 0);
}

bool Log_file_handle::is_open() const { return m_is_open; }

void Log_file_handle::fsync() {
  ut_ad(is_open());
  ut_ad(m_access_mode != Log_file_access_mode::READ_ONLY);

  if (s_skip_fsyncs) {
    return;
  }

  s_total_fsyncs.fetch_add(1, std::memory_order_relaxed);
  s_fsyncs_in_progress.fetch_add(1);

  const bool success = os_file_flush(m_raw_handle);

  s_fsyncs_in_progress.fetch_sub(1);
  ut_a(success);
}

Log_file_id Log_file_handle::file_id() const { return m_file_id; }

os_offset_t Log_file_handle::file_size() const { return m_file_size; }

IORequest Log_file_handle::prepare_io_request(int req_type, os_offset_t offset,
                                              os_offset_t size,
                                              bool can_use_encryption) {
  ut_a(size > 0);
  ut_a(size % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(offset % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(req_type == IORequest::READ || req_type == IORequest::WRITE);
  ut_a(m_block_size > 0);

  IORequest io_request{IORequest::LOG | req_type};
  io_request.block_size(m_block_size);

  // Finally, set up encryption related fields, if needed

  if (!(can_use_encryption && m_encryption_metadata.can_encrypt())) {
    return io_request;  // There is no ecryption involved
  }

  if (offset + size <= LOG_FILE_HDR_SIZE) {
    return io_request;  // Never use encryption in the header
  }

  // Assume the whole encrypted region is in the body, none of it in the header
  ut_a(offset >= LOG_FILE_HDR_SIZE);
  io_request.get_encryption_info().set(m_encryption_metadata);

  return io_request;
}

dberr_t Log_file_handle::read(os_offset_t read_offset, os_offset_t read_size,
                              byte *buf) {
  if (!is_open()) return DB_ERROR;

  auto io_request =
      prepare_io_request(IORequest::READ, read_offset, read_size, true);

  ut_ad(m_access_mode != Log_file_access_mode::WRITE_ONLY);

  if (s_on_before_read) {
    s_on_before_read(m_file_id, m_file_type, read_offset, read_size);
  }

  return os_file_read(io_request, m_file_path.c_str(), m_raw_handle, buf,
                      read_offset, static_cast<ulint>(read_size));
}

dberr_t Log_file_handle::write(os_offset_t write_offset, os_offset_t write_size,
                               const byte *buf) {
  if (!is_open()) return DB_ERROR;

  auto io_request = prepare_io_request(IORequest::WRITE, write_offset,
                                       write_size, srv_redo_log_encrypt);

  ut_ad(m_access_mode != Log_file_access_mode::READ_ONLY);

  if (s_on_before_write) {
    s_on_before_write(m_file_id, m_file_type, write_offset, write_size);
  }

  m_is_modified = true;

  return os_file_write(io_request, m_file_path.c_str(), m_raw_handle, buf,
                       write_offset, static_cast<ulint>(write_size));
}

Log_file_handle Log_file::open(Log_file_access_mode access_mode) const {
  return open(m_files_ctx, m_id, access_mode, m_encryption_metadata,
              Log_file_type::NORMAL);
}

Log_file_handle Log_file::open(const Log_files_context &files_ctx,
                               Log_file_id file_id,
                               Log_file_access_mode access_mode,
                               Encryption_metadata &encryption_metadata,
                               Log_file_type file_type) {
  return Log_file_handle{files_ctx, file_id, access_mode, encryption_metadata,
                         file_type};
}

Log_file::Log_file(const Log_files_context &files_ctx,
                   Encryption_metadata &encryption_metadata)
    : m_files_ctx{files_ctx},
      m_id{},
      m_consumed{},
      m_full{},
      m_size_in_bytes{},
      m_start_lsn{},
      m_end_lsn{},
      m_encryption_metadata{encryption_metadata} {}

Log_file::Log_file(const Log_files_context &files_ctx, Log_file_id id,
                   bool consumed, bool full, os_offset_t size_in_bytes,
                   lsn_t start_lsn, lsn_t end_lsn,
                   Encryption_metadata &encryption_metadata)
    : m_files_ctx{files_ctx},
      m_id{id},
      m_consumed{consumed},
      m_full{full},
      m_size_in_bytes{size_in_bytes},
      m_start_lsn{start_lsn},
      m_end_lsn{end_lsn},
      m_encryption_metadata{encryption_metadata} {}

Log_file &Log_file::operator=(const Log_file &other) {
  m_id = other.m_id;
  m_consumed = other.m_consumed;
  m_full = other.m_full;
  m_size_in_bytes = other.m_size_in_bytes;
  m_start_lsn = other.m_start_lsn;
  m_end_lsn = other.m_end_lsn;
  ut_a(&m_files_ctx == &other.m_files_ctx);
  ut_a(&m_encryption_metadata == &other.m_encryption_metadata);
  return *this;
}

/** @} */

/**************************************************/ /**

 @name Log - file header read/write.

 *******************************************************/

/** @{ */

static void log_file_header_validate(const Log_file_header &header) {
  ut_a(header.m_start_lsn >= LOG_START_LSN);
  ut_a(header.m_start_lsn < LSN_MAX);
  ut_a(header.m_start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  ut_a(!header.m_creator_name.empty());
  ut_a(header.m_creator_name.size() <= LOG_HEADER_CREATOR_MAX_LENGTH);

  ut_a(header.m_format <= to_int(Log_format::CURRENT));
}

void log_file_header_serialize(const Log_file_header &header, byte *buf) {
  log_file_header_validate(header);

  std::memset(buf, 0x00, OS_FILE_LOG_BLOCK_SIZE);

  mach_write_to_4(buf + LOG_HEADER_FORMAT, header.m_format);

  mach_write_to_8(buf + LOG_HEADER_START_LSN, header.m_start_lsn);

  strncpy(reinterpret_cast<char *>(buf) + LOG_HEADER_CREATOR,
          header.m_creator_name.c_str(), LOG_HEADER_CREATOR_MAX_LENGTH);

  mach_write_to_4(buf + LOG_HEADER_FLAGS, header.m_log_flags);

  mach_write_to_4(buf + LOG_HEADER_LOG_UUID, header.m_log_uuid);

  log_block_set_checksum(buf, log_block_calc_checksum_crc32(buf));
}

bool log_file_header_deserialize(const byte *buf, Log_file_header &header) {
  header.m_format = mach_read_from_4(buf + LOG_HEADER_FORMAT);

  header.m_start_lsn = mach_read_from_8(buf + LOG_HEADER_START_LSN);

  /* Do not assume there is any null terminator after buf + LOG_HEADER_CREATOR
  because there could be no and it would then read outside bytes dedicated for
  the creator name. */
  auto creator_name =
      std::string{reinterpret_cast<const char *>(buf + LOG_HEADER_CREATOR),
                  LOG_HEADER_CREATOR_MAX_LENGTH};

  /* Copy only up to the first null terminator. */
  header.m_creator_name = std::string{creator_name.c_str()};

  header.m_log_flags = mach_read_from_4(buf + LOG_HEADER_FLAGS);

  header.m_log_uuid = mach_read_from_4(buf + LOG_HEADER_LOG_UUID);

  /* Check the header page checksum. There was no
  checksum in the first redo log format (version 0). */
  return header.m_format == to_int(Log_format::LEGACY) ||
         log_header_checksum_is_ok(buf);
}

dberr_t log_file_header_write(Log_file_handle &file_handle,
                              const Log_file_header &header) {
  Log_file_block buf;
  log_file_header_serialize(header, buf.data);
  return log_file_header_write(file_handle, buf.data);
}

dberr_t log_file_header_write(Log_file_handle &file_handle, const byte *buf) {
  return file_handle.write(0, OS_FILE_LOG_BLOCK_SIZE, buf);
}

dberr_t log_file_header_read(Log_file_handle &file_handle, byte *buf) {
  return file_handle.read(0, OS_FILE_LOG_BLOCK_SIZE, buf);
}

dberr_t log_file_header_read(Log_file_handle &file_handle,
                             Log_file_header &header) {
  Log_file_block buf;

  const dberr_t err = log_file_header_read(file_handle, buf.data);
  if (err != DB_SUCCESS) {
    return err;
  }

  if (!log_file_header_deserialize(buf.data, header)) {
    ib::error(ER_IB_MSG_LOG_FILE_HEADER_INVALID_CHECKSUM);
    return DB_CORRUPTION;
  }

  return DB_SUCCESS;
}

static Log_flags log_file_header_flag_bit(uint32_t bit) {
  ut_a(bit > 0);
  ut_a(bit <= LOG_HEADER_FLAG_MAX);
  return 1UL << (bit - 1);
}

void log_file_header_set_flag(Log_flags &log_flags, uint32_t bit) {
  log_flags |= log_file_header_flag_bit(bit);
}

void log_file_header_reset_flag(Log_flags &log_flags, uint32_t bit) {
  log_flags &= ~log_file_header_flag_bit(bit);
}

bool log_file_header_check_flag(Log_flags log_flags, uint32_t bit) {
  return log_flags & log_file_header_flag_bit(bit);
}

/** @} */

/**************************************************/ /**

 @name Log - encryption header read/write.

 *******************************************************/

/** @{ */

dberr_t log_encryption_header_write(Log_file_handle &file_handle,
                                    const byte *buf) {
  return file_handle.write(LOG_ENCRYPTION, OS_FILE_LOG_BLOCK_SIZE, buf);
}

dberr_t log_encryption_header_read(Log_file_handle &file_handle, byte *buf) {
  return file_handle.read(LOG_ENCRYPTION, OS_FILE_LOG_BLOCK_SIZE, buf);
}

/** @} */

/**************************************************/ /**

 @name Log - checkpoint header read/write.

 *******************************************************/

/** @{ */

void log_checkpoint_header_serialize(const Log_checkpoint_header &header,
                                     byte *buf) {
  memset(buf, 0x00, OS_FILE_LOG_BLOCK_SIZE);

  mach_write_to_8(buf + LOG_CHECKPOINT_LSN, header.m_checkpoint_lsn);

  log_block_set_checksum(buf, log_block_calc_checksum_crc32(buf));
}

bool log_checkpoint_header_deserialize(const byte *buf,
                                       Log_checkpoint_header &header) {
  header.m_checkpoint_lsn = mach_read_from_8(buf + LOG_CHECKPOINT_LSN);

  return log_header_checksum_is_ok(buf);
}

dberr_t log_checkpoint_header_write(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    const Log_checkpoint_header &header) {
  Log_file_block buf;
  log_checkpoint_header_serialize(header, buf.data);
  return log_checkpoint_header_write(file_handle, checkpoint_header_no,
                                     buf.data);
}

static os_offset_t log_checkpoint_header_offset(
    Log_checkpoint_header_no checkpoint_header_no) {
  switch (checkpoint_header_no) {
    case Log_checkpoint_header_no::HEADER_1:
      return LOG_CHECKPOINT_1;
    case Log_checkpoint_header_no::HEADER_2:
      return LOG_CHECKPOINT_2;
    default:
      ut_error;
  }
}

dberr_t log_checkpoint_header_write(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    const byte *buf) {
  const os_offset_t checkpoint_header_offset =
      log_checkpoint_header_offset(checkpoint_header_no);
  return file_handle.write(checkpoint_header_offset, OS_FILE_LOG_BLOCK_SIZE,
                           buf);
}

dberr_t log_checkpoint_header_read(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    byte *buf) {
  const os_offset_t checkpoint_header_offset =
      log_checkpoint_header_offset(checkpoint_header_no);
  return file_handle.read(checkpoint_header_offset, OS_FILE_LOG_BLOCK_SIZE,
                          buf);
}

dberr_t log_checkpoint_header_read(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    Log_checkpoint_header &header) {
  Log_file_block buf;

  const dberr_t err =
      log_checkpoint_header_read(file_handle, checkpoint_header_no, buf.data);
  if (err != DB_SUCCESS) {
    return err;
  }

  if (!log_checkpoint_header_deserialize(buf.data, header)) {
    DBUG_PRINT("ib_log", ("invalid checkpoint " UINT32PF " checksum %lx",
                          uint32_t{to_int(checkpoint_header_no)},
                          ulong{log_block_get_checksum(buf.data)}));
    return DB_CORRUPTION;
  }

  return DB_SUCCESS;
}

/** @} */

/**************************************************/ /**

 @name Log data blocks

 *******************************************************/

/** @{ */

static void log_data_blocks_validate(os_offset_t offset, os_offset_t size) {
  ut_a(offset >= LOG_FILE_HDR_SIZE);
  ut_a(offset % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(size % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(size > 0);
}

dberr_t log_data_blocks_write(Log_file_handle &file_handle,
                              os_offset_t write_offset, size_t write_size,
                              const byte *buf) {
  log_data_blocks_validate(write_offset, write_size);
  return file_handle.write(write_offset, write_size, buf);
}

dberr_t log_data_blocks_read(Log_file_handle &file_handle,
                             os_offset_t read_offset, size_t read_size,
                             byte *buf) {
  log_data_blocks_validate(read_offset, read_size);
  return file_handle.read(read_offset, read_size, buf);
}

/** @} */

/**************************************************/ /**

 @name Log - file names and paths.

 *******************************************************/

/** @{ */

std::string log_directory_path(const Log_files_context &ctx) {
  std::ostringstream str;
  if (!ctx.m_root_path.empty()) {
    str << ctx.m_root_path;
    /* Add a path separator if needed. */
    if (ctx.m_root_path.back() != OS_PATH_SEPARATOR) {
      str << OS_PATH_SEPARATOR;
    }
  } else {
    str << "." << OS_PATH_SEPARATOR;
  }
  switch (ctx.m_files_ruleset) {
    case Log_files_ruleset::CURRENT:
      str << LOG_DIRECTORY_NAME << OS_PATH_SEPARATOR;
      break;
    case Log_files_ruleset::PRE_8_0_30:
      break;
    default:
      ut_error;
  }
  return str.str();
}

std::string log_file_name(const Log_files_context &ctx, Log_file_id file_id) {
  switch (ctx.m_files_ruleset) {
    case Log_files_ruleset::CURRENT:
      break;
    case Log_files_ruleset::PRE_8_0_30:
      return log_pre_8_0_30::file_name(file_id);
    default:
      ut_error;
  }
  std::ostringstream str;
  str << LOG_FILE_BASE_NAME << file_id;
  return str.str();
}

std::string log_file_path(const Log_files_context &ctx, Log_file_id file_id) {
  return log_directory_path(ctx) + log_file_name(ctx, file_id);
}

std::string log_file_path_for_unused_file(const Log_files_context &ctx,
                                          Log_file_id file_id) {
  return log_file_path(ctx, file_id) + "_tmp";
}

/** Extracts identifier of the redo log file from its file name.
Function might be called for non-redo log files in which case it
should return false instead of extracting the identifier.
@param[in]   ctx              redo log files context
@param[in]   file_name        file name (can be wrong)
@param[in]   expected_suffix  suffix that should exist after id
@param[out]  extracted_id     extracted redo file identifier
@return true iff extracted the identifier properly */
static bool log_extract_id_from_file_name(const Log_files_context &ctx,
                                          const char *file_name,
                                          const char *expected_suffix,
                                          Log_file_id &extracted_id) {
  std::string file_base_name;
  switch (ctx.m_files_ruleset) {
    case Log_files_ruleset::PRE_8_0_30:
      file_base_name = log_pre_8_0_30::FILE_BASE_NAME;
      break;
    case Log_files_ruleset::CURRENT:
      file_base_name = LOG_FILE_BASE_NAME;
      break;
    default:
      ut_error;
  }

  const size_t file_name_len = strlen(file_name);
  const size_t expected_suffix_len = strlen(expected_suffix);

  if (file_name_len <= file_base_name.size() + expected_suffix_len) {
    return false;
  }
  if (memcmp(file_base_name.c_str(), file_name, file_base_name.size()) != 0) {
    return false;
  }
  size_t n_processed = 0;
  try {
    if (!isdigit(file_name[file_base_name.size()])) {
      return false;
    }
    auto id = std::stoll(file_name + file_base_name.size(), &n_processed);
    if (file_base_name.size() + n_processed + expected_suffix_len !=
        file_name_len) {
      return false;
    }
    if (memcmp(file_name + file_base_name.size() + n_processed, expected_suffix,
               expected_suffix_len) != 0) {
      return false;
    }
    extracted_id = id;
    return true;
  } catch (const std::invalid_argument &) {
    return false;
  } catch (const std::out_of_range &) {
    return false;
  } catch (...) {
    ut_error;
  }
}

/** Lists existing redo files in the configured redo log directory, selecting
redo log files which have a given suffix in their name. Produces list of
identifiers of the listed and selected files. If a system error occurs when
listing the redo directory, error is emitted to the error log and DB_ERROR
is returned.
@param[in]   ctx      redo log files context
@param[in]   suffix   suffix used for the selection of files (could be "")
@param[out]  ret      identifiers of the listed and selected files
@return DB_SUCCESS or DB_ERROR */
static dberr_t log_list_existing_files_low(const Log_files_context &ctx,
                                           const char *suffix,
                                           ut::vector<Log_file_id> &ret) {
  ret.clear();

  const auto dir_path = log_directory_path(ctx);

  const bool success = os_file_scan_directory(
      dir_path.c_str(),
      [&](const char *, const char *file_name) {
        Log_file_id file_id;
        if (log_extract_id_from_file_name(ctx, file_name, suffix, file_id)) {
          ret.push_back(file_id);
        }
      },
      false);

  std::sort(ret.begin(), ret.end());

  return success ? DB_SUCCESS : DB_ERROR;
}

dberr_t log_list_existing_unused_files(const Log_files_context &ctx,
                                       ut::vector<Log_file_id> &ret) {
  /* Possible error is emitted to the log inside function called below. */
  return log_list_existing_files_low(ctx, "_tmp", ret);
}

dberr_t log_list_existing_files(const Log_files_context &ctx,
                                ut::vector<Log_file_id> &ret) {
  /* Possible error is emitted to the log inside function called below. */
  return log_list_existing_files_low(ctx, "", ret);
}

/** @} */

/**************************************************/ /**

 @name Log - file creation / deletion

 *******************************************************/

/** @{ */

#ifndef _WIN32

static void log_flush_directory_low(const std::string &path) {
  const auto dir_path = 0 < path.length()
                            ? path.back() == OS_PATH_SEPARATOR
                                  ? path.substr(0, path.length() - 1)
                                  : path
                            : ".";

  bool ret{false};
  const auto dir =
      os_file_create(innodb_log_file_key, dir_path.c_str(), OS_FILE_OPEN,
                     OS_FILE_NORMAL, OS_LOG_FILE, true, &ret);
  ut_a(ret);
  os_file_flush(dir);
  os_file_close(dir);
}

#endif /* !_WIN32 */

static dberr_t log_rename_file_low(const Log_files_context &ctx
                                   [[maybe_unused]],
                                   const std::string &old_file_path,
                                   const std::string &new_file_path,
                                   int err_msg_id) {
  const bool success = os_file_rename(
      innodb_log_file_key, old_file_path.c_str(), new_file_path.c_str());

  /* On Windows, os_file_rename() uses MoveFileEx
  and provides MOVEFILE_WRITE_THROUGH. */

#ifndef _WIN32
  if (success) {
    log_flush_directory_low(ctx.m_root_path.c_str());
  }
#endif /* !_WIN32 */

  if (!success) {
    ib::error(err_msg_id, old_file_path.c_str(), new_file_path.c_str());
    return DB_ERROR;
  }

  return DB_SUCCESS;
}

dberr_t log_rename_unused_file(const Log_files_context &ctx,
                               Log_file_id old_unused_file_id,
                               Log_file_id new_unused_file_id) {
  ut_a(old_unused_file_id != new_unused_file_id);
  return log_rename_file_low(
      ctx, log_file_path_for_unused_file(ctx, old_unused_file_id),
      log_file_path_for_unused_file(ctx, new_unused_file_id),
      ER_IB_MSG_LOG_FILE_UNUSED_RENAME_FAILED);
}

dberr_t log_mark_file_as_in_use(const Log_files_context &ctx,
                                Log_file_id file_id) {
  return log_rename_file_low(ctx, log_file_path_for_unused_file(ctx, file_id),
                             log_file_path(ctx, file_id),
                             ER_IB_MSG_LOG_FILE_UNUSED_MARK_AS_IN_USE_FAILED);
}

dberr_t log_mark_file_as_unused(const Log_files_context &ctx,
                                Log_file_id file_id) {
  return log_rename_file_low(ctx, log_file_path(ctx, file_id),
                             log_file_path_for_unused_file(ctx, file_id),
                             ER_IB_MSG_LOG_FILE_MARK_AS_UNUSED_FAILED);
}

static dberr_t log_remove_file_low(const Log_files_context &,
                                   const std::string &file_path,
                                   int err_msg_id) {
  os_file_type_t file_type;
  os_file_status(file_path.c_str(), nullptr, &file_type);
  if (file_type == OS_FILE_TYPE_MISSING) {
    /* File does not exist (note: there is no reason to use the "exists"
    argument of the os_file_status(), because the "file_type" argument
    provides information about the missing file by OS_FILE_TYPE_MISSING. */
    return DB_NOT_FOUND;
  }
  ut_a(file_type == OS_FILE_TYPE_FILE);
  const bool deleted =
      os_file_delete_if_exists(innodb_log_file_key, file_path.c_str(), nullptr);
  if (!deleted) {
    ib::error(err_msg_id, file_path.c_str());
    return DB_ERROR;
  }
  return DB_SUCCESS;
}

dberr_t log_remove_unused_file(const Log_files_context &ctx,
                               Log_file_id file_id) {
  return log_remove_file_low(ctx, log_file_path_for_unused_file(ctx, file_id),
                             ER_IB_MSG_LOG_FILE_UNUSED_REMOVE_FAILED);
}

std::pair<dberr_t, ut::vector<Log_file_id>> log_remove_unused_files(
    const Log_files_context &ctx) {
  ut::vector<Log_file_id> listed_files, removed_files;

  dberr_t err = log_list_existing_unused_files(ctx, listed_files);
  if (err != DB_SUCCESS) {
    ut_a(err != DB_NOT_FOUND);
    return {err, {}};
  }

  for (Log_file_id id : listed_files) {
    removed_files.push_back(id);
    err = log_remove_unused_file(ctx, id);
    if (err != DB_SUCCESS) {
      if (err == DB_NOT_FOUND) {
        err = DB_ERROR;
      }
      return {err, removed_files};
    }
  }

  /* In older versions of format, ib_logfile101 was used as
  a temporary file, which marked non-finished initialization. */
  if (ctx.m_files_ruleset <= Log_files_ruleset::PRE_8_0_30) {
    removed_files.push_back(101);
    err = log_remove_file_low(ctx, log_directory_path(ctx) + "ib_logfile101",
                              ER_IB_MSG_LOG_FILE_UNUSED_REMOVE_FAILED);
    if (err == DB_NOT_FOUND) {
      removed_files.pop_back();
    } else if (err != DB_SUCCESS) {
      return {err, removed_files};
    }
  }

  return {DB_SUCCESS, removed_files};
}

dberr_t log_remove_file(const Log_files_context &ctx, Log_file_id file_id) {
  return log_remove_file_low(ctx, log_file_path(ctx, file_id),
                             ER_IB_MSG_LOG_FILE_REMOVE_FAILED);
}

std::pair<dberr_t, Log_file_id> log_remove_file(const Log_files_context &ctx) {
  ut::vector<Log_file_id> listed_files;

  dberr_t err = log_list_existing_files(ctx, listed_files);
  if (err != DB_SUCCESS) {
    return {err, Log_file_id{}};
  }

  if (listed_files.empty()) {
    return {DB_NOT_FOUND, Log_file_id{}};
  } else {
    return {log_remove_file(ctx, listed_files.front()), listed_files.front()};
  }
}

std::pair<dberr_t, ut::vector<Log_file_id>> log_remove_files(
    const Log_files_context &ctx) {
  ut::vector<Log_file_id> listed_files, removed_files;

  dberr_t err = log_list_existing_files(ctx, listed_files);
  if (err != DB_SUCCESS) {
    return {err, {}};
  }

  for (Log_file_id id : listed_files) {
    removed_files.push_back(id);
    err = log_remove_file(ctx, id);
    if (err != DB_SUCCESS) {
      if (err == DB_NOT_FOUND) {
        err = DB_ERROR;
      }
      return {err, removed_files};
    }
  }
  return {DB_SUCCESS, removed_files};
}

dberr_t log_create_unused_file(const Log_files_context &ctx,
                               Log_file_id file_id, os_offset_t size_in_bytes) {
  ut_a(size_in_bytes >= LOG_FILE_HDR_SIZE);

  const auto file_path = log_file_path_for_unused_file(ctx, file_id);

  bool ret;
  auto file = os_file_create(innodb_log_file_key, file_path.c_str(),
                             OS_FILE_CREATE | OS_FILE_ON_ERROR_NO_EXIT,
                             OS_FILE_NORMAL, OS_LOG_FILE, false, &ret);

  if (!ret) {
    ib::error(ER_IB_MSG_LOG_FILE_OS_CREATE_FAILED, file_path.c_str());
    return DB_ERROR;
  }

  ret = os_file_set_size_fast(file_path.c_str(), file, 0, size_in_bytes, true);

  if (!ret) {
    ib::error(ER_IB_MSG_LOG_FILE_RESIZE_FAILED, file_path.c_str(),
              ulonglong{size_in_bytes / (1024 * 1024UL)}, "Failed to set size");

    /* Delete incomplete file if OOM */
    if (os_has_said_disk_full) {
      ret = os_file_close(file);
      ut_a(ret);
      os_file_delete(innodb_log_file_key, file_path.c_str());
    }

    return DB_ERROR;
  }

  ret = os_file_close(file);
  ut_a(ret);

  return DB_SUCCESS;
}

static dberr_t log_resize_file_low(const std::string &file_path,
                                   os_offset_t size_in_bytes, int err_msg_id) {
  os_file_stat_t stat_info;
  const dberr_t err =
      os_file_get_status(file_path.c_str(), &stat_info, false, false);
  if (err != DB_SUCCESS) {
    ib::error(err_msg_id, file_path.c_str(),
              ulonglong{size_in_bytes / (1024 * 1024UL)},
              err == DB_NOT_FOUND ? "Failed to find the file"
                                  : "Failed to retrieve status of the file");
    return err == DB_NOT_FOUND ? DB_NOT_FOUND : DB_ERROR;
  }

  if (size_in_bytes == stat_info.size) {
    return DB_SUCCESS;
  }

  bool ret;
  auto file = os_file_create(innodb_log_file_key, file_path.c_str(),
                             OS_FILE_OPEN | OS_FILE_ON_ERROR_NO_EXIT,
                             OS_FILE_NORMAL, OS_LOG_FILE, false, &ret);
  if (!ret) {
    ib::error(err_msg_id, file_path.c_str(),
              ulonglong{size_in_bytes / (1024 * 1024UL)},
              "Failed to open the file");
    return DB_ERROR;
  }

  if (size_in_bytes > stat_info.size) {
    ret =
        os_file_set_size_fast(file_path.c_str(), file, 0, size_in_bytes, true);

  } else if (size_in_bytes < stat_info.size) {
    ret = os_file_truncate(file_path.c_str(), file, size_in_bytes);
    os_file_flush(file);

  } else {
    ret = true;
  }

  const bool close_ret = os_file_close(file);
  ut_a(close_ret);

  if (!ret) {
    if (os_has_said_disk_full) {
      ib::error(err_msg_id, file_path.c_str(),
                ulonglong{size_in_bytes / (1024 * 1024UL)},
                "Missing space on disk");
      return DB_OUT_OF_DISK_SPACE;
    }
    ib::error(err_msg_id, file_path.c_str(),
              ulonglong{size_in_bytes / (1024 * 1024UL)},
              "Failed to resize the file");
    return DB_ERROR;
  }
  return DB_SUCCESS;
}

dberr_t log_resize_unused_file(const Log_files_context &ctx,
                               Log_file_id file_id, os_offset_t size_in_bytes) {
  return log_resize_file_low(log_file_path_for_unused_file(ctx, file_id),
                             size_in_bytes,
                             ER_IB_MSG_LOG_FILE_UNUSED_RESIZE_FAILED);
}

dberr_t log_resize_file(const Log_files_context &ctx, Log_file_id file_id,
                        os_offset_t size_in_bytes) {
  return log_resize_file_low(log_file_path(ctx, file_id), size_in_bytes,
                             ER_IB_MSG_LOG_FILE_RESIZE_FAILED);
}

static dberr_t log_check_file(const Log_files_context &ctx, Log_file_id file_id,
                              bool read_only, os_offset_t &size_in_bytes) {
  const auto file_path = log_file_path(ctx, file_id);

  if (!os_file_exists(file_path.c_str())) {
    return DB_NOT_FOUND;
  }

  if (!os_file_check_mode(file_path.c_str(), read_only)) {
    /* Error has been emitted in os_file_check_mode */
    return DB_ERROR;
  }

  bool ret;
  auto file =
      os_file_create(innodb_log_file_key, file_path.c_str(), OS_FILE_OPEN,
                     OS_FILE_NORMAL, OS_LOG_FILE, read_only, &ret);
  if (!ret) {
    ib::error(ER_IB_MSG_LOG_FILE_OPEN_FAILED, file_path.c_str(),
              static_cast<int>(DB_ERROR));
    return DB_ERROR;
  }

  size_in_bytes = os_file_get_size(file);

  ret = os_file_close(file);
  ut_a(ret);

  if (size_in_bytes == 0) {
    ib::error(ER_IB_MSG_LOG_FILE_IS_EMPTY, file_path.c_str());
    return DB_ERROR;
  }
  if (size_in_bytes < LOG_FILE_MIN_SIZE) {
    ib::error(ER_IB_MSG_LOG_FILE_TOO_SMALL, file_path.c_str(),
              ulonglong{LOG_FILE_MIN_SIZE});
    return DB_ERROR;
  }
  if (ctx.m_files_ruleset > Log_files_ruleset::PRE_8_0_30) {
    if (size_in_bytes > LOG_FILE_MAX_SIZE) {
      ib::error(ER_IB_MSG_LOG_FILE_TOO_BIG, file_path.c_str(),
                ulonglong{LOG_FILE_MAX_SIZE});
      return DB_ERROR;
    }
  }
  if (size_in_bytes % UNIV_PAGE_SIZE != 0) {
    /* Even though we tolerate different sizes of log files, still
    we require that each of them has size divisible by page size. */
    ib::error(ER_IB_MSG_LOG_FILE_SIZE_INVALID, file_path.c_str(),
              ulonglong{size_in_bytes});
    return DB_ERROR;
  }

  return DB_SUCCESS;
}

dberr_t log_collect_existing_files(const Log_files_context &ctx, bool read_only,
                                   ut::vector<Log_file_id_and_size> &found) {
  ut::vector<Log_file_id> listed_files;
  dberr_t err = log_list_existing_files(ctx, listed_files);
  if (err != DB_SUCCESS) {
    /* Error emitted in log_list_existing_files */
    return err;
  }
  ut::vector<Log_file_id_and_size> result;
  for (Log_file_id id : listed_files) {
    os_offset_t file_size_in_bytes;
    err = log_check_file(ctx, id, read_only, file_size_in_bytes);
    switch (err) {
      case DB_SUCCESS:
        result.emplace_back(id, file_size_in_bytes);
        break;
      case DB_ERROR:
        /* Error has been emitted in log_check_file */
        return DB_ERROR;
      case DB_NOT_FOUND:
        continue;
      default:
        ut_error;
    }
  }
  found = std::move(result);
  return found.empty() ? DB_NOT_FOUND : DB_SUCCESS;
}

Log_uuid log_generate_uuid() {
  return ut::random_from_interval(1, std::numeric_limits<Log_uuid>::max());
}

/** @} */

/**************************************************/ /**

 @name Log - files context

 *******************************************************/

/** @{ */

Log_files_context::Log_files_context(const std::string &root_path,
                                     Log_files_ruleset files_ruleset)
    : m_root_path{root_path}, m_files_ruleset{files_ruleset} {}

/** @} */

void Log_data_block_header::set_lsn(lsn_t lsn) {
  m_epoch_no = log_block_convert_lsn_to_epoch_no(lsn);
  m_hdr_no = log_block_convert_lsn_to_hdr_no(lsn);
}
