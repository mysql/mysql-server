/* Copyright (c) 2018, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/binlog_ostream.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "my_aes.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_rnd.h"
#include "my_sys.h"
#include "my_thread_local.h"  // my_errno
#include "mysql/components/services/log_builtins.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"
#include "sql/mysqld.h"
#include "sql/rpl_log_encryption.h"
#include "sql/sql_class.h"

#ifndef NDEBUG
bool binlog_cache_is_reset = false;
#endif

namespace {

constexpr char kBinlogTempFilePrefix[] = "bolt_";

/*
  Returns true if 'name' is a temp file created by this feature, i.e. matches
  the bolt_<lowercase-hex> pattern. Note: a true result does NOT mean the file
  was (or will be) promoted into the binary log sequence. Every binlog-cache
  spill file uses this name, including transactions that commit through the
  standard path (below the threshold, encrypted, compressed, etc.). This is
  purely an ownership check so startup cleanup only deletes files this feature
  created.
*/
bool is_bolt_temp_file(const char *name) {
  const size_t prefix_length = strlen(kBinlogTempFilePrefix);
  if (strncmp(name, kBinlogTempFilePrefix, prefix_length) != 0 ||
      name[prefix_length] == '\0')
    return false;

  for (const char *cursor = name + prefix_length; *cursor != '\0'; ++cursor) {
    if (!(*cursor >= 'a' && *cursor <= 'z') &&
        !(*cursor >= '0' && *cursor <= '9') && *cursor != '_')
      return false;
  }
  return true;
}

ulong binlog_temp_file_permissions() {
  ulong permissions = 0;
  if (my_umask & 0400) permissions |= USER_READ;
  if (my_umask & 0200) permissions |= USER_WRITE;
  if (my_umask & 0100) permissions |= USER_EXECUTE;
  if (my_umask & 0040) permissions |= GROUP_READ;
  if (my_umask & 0020) permissions |= GROUP_WRITE;
  if (my_umask & 0010) permissions |= GROUP_EXECUTE;
  if (my_umask & 0004) permissions |= OTHERS_READ;
  if (my_umask & 0002) permissions |= OTHERS_WRITE;
  if (my_umask & 0001) permissions |= OTHERS_EXECUTE;
  return permissions;
}

}  // namespace

IO_CACHE_binlog_cache_storage::IO_CACHE_binlog_cache_storage() = default;
IO_CACHE_binlog_cache_storage::~IO_CACHE_binlog_cache_storage() { close(); }

bool IO_CACHE_binlog_cache_storage::open(const char *dir, const char *prefix,
                                         my_off_t cache_size,
                                         my_off_t max_cache_size,
                                         my_off_t reserved_bytes) {
  DBUG_TRACE;
  if (open_cached_file(&m_io_cache, dir, prefix, cache_size, MYF(MY_WME)))
    return true;
  m_spilled_file_is_managed = false;

  /*
    Default to an anonymous spill file. Whether it is instead a named file that
    can be promoted into the binary log sequence is decided per transaction
    from the optimization knob, at transaction start, via set_named_file().
    A named file is kept in the filesystem namespace (promotable, and cleaned
    up at server startup if left behind); an anonymous file is unlinked at
    creation and is never visible. So when the optimization is disabled for a
    transaction, its spill file leaves no visible bolt_ file.
  */
  m_io_cache.named_file = false;
  /* Keep the arguments: reset re-opens the cache after a spill. */
  m_dir = dir;
  m_prefix = prefix;
  m_cache_size = cache_size;
  m_max_cache_size_arg = max_cache_size;

  /*
    The cache's content is placed after the reserved bytes: physical
    positions start there, and the first flush into the lazily created
    temporary file seeks there (the file's offset is 0 at creation).

    The reserved region is applied in all cases, even when the optimization is
    disabled for this transaction. A non-promoted transaction never fills it
    with header events and never copies it into the binary log (begin() starts
    the read cursor past it), so it costs only transient temp-file space and is
    invisible in the binary log. Only the file naming is gated on the knob.
  */
  m_reserved_bytes = reserved_bytes;
  m_io_cache.pos_in_file = reserved_bytes;
  m_io_cache.seek_not_done = true;

  if (rpl_encryption.is_enabled()) enable_encryption();

  m_max_cache_size = max_cache_size;
  /* The max cache size caps physical positions: shift it too. */
  if (m_max_cache_size <= ~(my_off_t)0 - reserved_bytes)
    m_max_cache_size += reserved_bytes;
  /* Set the max cache size for IO_CACHE */
  m_io_cache.end_of_file = m_max_cache_size;
  return false;
}

void IO_CACHE_binlog_cache_storage::close() { close_cached_file(&m_io_cache); }

bool IO_CACHE_binlog_cache_storage::write(const unsigned char *buffer,
                                          my_off_t length) {
  /*
    Enable/disable binlog cache temporary file encryption according to the
    setting of global binlog_encryption if both binlog cache temporary
    file encryption and the setting of global binlog_encryption are not
    consistent on the first writing of binlog cache after changing the
    setting of global binlog_encryption.
  */
  if (unlikely((m_io_cache.m_encryptor == nullptr ||
                m_io_cache.m_decryptor == nullptr) &&
               rpl_encryption.is_enabled()) ||
      unlikely((m_io_cache.m_encryptor != nullptr ||
                m_io_cache.m_decryptor != nullptr) &&
               !rpl_encryption.is_enabled())) {
    /*
      Make sure the binlog cache temporary file is empty before enabling or
      disabling the binlog cache temporary file encryption.
    */
    if (m_io_cache.file == -1 ||
        my_seek(m_io_cache.file, 0L, MY_SEEK_END, MYF(MY_WME + MY_FAE)) == 0) {
      if (rpl_encryption.is_enabled()) {
        if (enable_encryption()) return true;
        if (setup_ciphers_password()) return true;
      } else
        disable_encryption();
    }
  }

  if (my_b_safe_write(&m_io_cache, buffer, length)) return true;

  /*
    open_cached_file creates the physical backing file lazily. For a named
    (promotable) spill file, immediately replace its generic mkstemp name with
    the managed bolt_ form before callers can observe or promote it. An
    anonymous spill file (optimization disabled for this transaction) has no
    name and is left as-is.
  */
  if (is_spilled() && m_io_cache.named_file && !m_spilled_file_is_managed) {
    if (rename_spilled_file()) return true;
    m_spilled_file_is_managed = true;
  }
  return false;
}

bool IO_CACHE_binlog_cache_storage::truncate(my_off_t offset) {
  /* Translate the zero-based data offset to a physical position. */
  offset += m_reserved_bytes;
  /*
     It is not really necessary to flush the data will be truncated into
     temporary file before truncating . And it may cause write failure. So set
     clear_cache to true if all data in cache will be truncated.
     It avoids flush data to the internal temporary file.
  */
  if (reinit_io_cache(&m_io_cache, WRITE_CACHE, offset, false,
                      offset < m_io_cache.pos_in_file /*clear_cache*/))
    return true;
  m_io_cache.end_of_file = m_max_cache_size;

  return false;
}

bool IO_CACHE_binlog_cache_storage::rename_spilled_file() {
  DBUG_TRACE;
  assert(is_spilled());
  if (m_io_cache.file_name == nullptr) return true;

  const char *const old_name = m_io_cache.file_name;

  /*
    The name is bolt_<server-start time>_<serial>, both in hex. It is unique
    within a server run: server_start_time is constant for the run and the
    serial is a monotonic atomic counter, so no probe or retry is needed.
    Leftover files from earlier runs are removed at startup; if that cleanup
    fails the optimization is disabled and no new files are created, so a
    name cannot collide with an earlier run's file either.
  */
  static std::atomic<uint64_t> serial_counter{0};
  const uint64_t serial =
      serial_counter.fetch_add(1, std::memory_order_relaxed);

  char new_name[FN_REFLEN];
  int length;
  if (m_dir != nullptr)
    length = snprintf(new_name, sizeof(new_name), "%s%c%s%llx_%llx", m_dir,
                      FN_LIBCHAR, kBinlogTempFilePrefix,
                      static_cast<unsigned long long>(server_start_time),
                      static_cast<unsigned long long>(serial));
  else
    length = snprintf(new_name, sizeof(new_name), "%s%llx_%llx",
                      kBinlogTempFilePrefix,
                      static_cast<unsigned long long>(server_start_time),
                      static_cast<unsigned long long>(serial));
  if (length < 0 || static_cast<size_t>(length) >= sizeof(new_name))
    return true;

  char *replacement = my_strdup(PSI_NOT_INSTRUMENTED, new_name, MYF(MY_WME));
  if (replacement == nullptr) return true;
  if (mysql_file_rename(m_io_cache.file_key, old_name, new_name,
                        MYF(MY_WME))) {
    my_free(replacement);
    return true;
  }

  my_free(m_io_cache.file_name);
  m_io_cache.file_name = replacement;
  return my_chmod(new_name, binlog_temp_file_permissions(), MYF(MY_WME));
}

bool IO_CACHE_binlog_cache_storage::reset(bool preserve_spilled_file) {
  assert(!preserve_spilled_file || is_spilled());
  if (is_spilled()) {
    /*
      A finished transaction leaves no trace in #binlog_temp_files unless BOLT
      has promoted the file into the binary log sequence.
    */
    disable_encryption();
    if (preserve_spilled_file) {
      /* Prevent close_cached_file from deleting the promoted file. */
      my_free(m_io_cache.file_name);
      m_io_cache.file_name = nullptr;
    }
    close();
    if (open(m_dir, m_prefix, m_cache_size, m_max_cache_size_arg,
             m_reserved_bytes))
      return true;
  } else if (truncate(0)) {
    return true;
  }

  DBUG_EXECUTE_IF("ensure_binlog_cache_temporary_file_is_encrypted", {
    /*
      Reset the binlog_cache_temporary_file_is_encrypted at resetting
      the binlog cache.
    */
    binlog_cache_temporary_file_is_encrypted = false;
  };);

  DBUG_EXECUTE_IF("ensure_binlog_cache_is_reset",
                  { binlog_cache_is_reset = true; };);

  if (rpl_encryption.is_enabled()) {
    if (enable_encryption()) return true;
    if (setup_ciphers_password()) return true;
  } else
    disable_encryption();

  m_io_cache.disk_writes = 0;
  return false;
}

size_t IO_CACHE_binlog_cache_storage::disk_writes() const {
  return m_io_cache.disk_writes;
}

bool IO_CACHE_binlog_cache_storage::is_spilled() const {
  return m_io_cache.file != -1;
}

bool IO_CACHE_binlog_cache_storage::flush_and_sync_spilled_file() {
  DBUG_TRACE;
  assert(is_spilled());
  if (flush_io_cache(&m_io_cache)) return true;
  /*
    A ROLLBACK TO SAVEPOINT truncation repositions the cache but does not
    shrink the file: cut any stale bytes past the logical end, so the
    promoted file ends exactly at the transaction's terminating event.
  */
  if (my_chsize(m_io_cache.file, my_b_tell(&m_io_cache), 0, MYF(MY_WME)))
    return true;
  return mysql_file_sync(m_io_cache.file, MYF(MY_WME)) != 0;
}

const char *IO_CACHE_binlog_cache_storage::tmp_file_name() const {
  return m_io_cache.file_name;
}

bool IO_CACHE_binlog_cache_storage::begin(unsigned char **buffer,
                                          my_off_t *length) {
  DBUG_EXECUTE_IF("simulate_tmpdir_partition_full",
                  { DBUG_SET("+d,simulate_file_write_error"); });

  DBUG_EXECUTE_IF("ensure_binlog_cache_temporary_file_is_encrypted", {
    /*
      Assert that the temporary file of binlog cache is encrypted before
      writing the content of binlog cache into binlog file.
    */
    assert(binlog_cache_temporary_file_is_encrypted);
  };);

  DBUG_EXECUTE_IF("ensure_binlog_cache_temp_file_encryption_is_disabled", {
    assert(m_io_cache.m_encryptor == nullptr &&
           m_io_cache.m_decryptor == nullptr);
  };);

  /* The data starts after the reserved bytes. */
  if (reinit_io_cache(&m_io_cache, READ_CACHE, m_reserved_bytes, false,
                      false)) {
    DBUG_EXECUTE_IF("simulate_tmpdir_partition_full",
                    { DBUG_SET("-d,simulate_file_write_error"); });

    char errbuf[MYSYS_STRERROR_SIZE];
    LogErr(ERROR_LEVEL, ER_FAILED_TO_WRITE_TO_FILE, tmp_file_name(), errno,
           my_strerror(errbuf, sizeof(errbuf), errno));

    if (current_thd->is_error()) current_thd->clear_error();
    my_error(ER_ERROR_ON_WRITE, MYF(MY_WME), tmp_file_name(), errno, errbuf);
    return true;
  }
  return next(buffer, length);
}

bool IO_CACHE_binlog_cache_storage::next(unsigned char **buffer,
                                         my_off_t *length) {
  my_b_fill(&m_io_cache);

  *buffer = m_io_cache.read_pos;
  *length = my_b_bytes_in_cache(&m_io_cache);

  m_io_cache.read_pos = m_io_cache.read_end;

  return m_io_cache.error;
}

my_off_t IO_CACHE_binlog_cache_storage::length() const {
  /* Physical positions include the reserved bytes; report the data
     length. */
  if (m_io_cache.type == WRITE_CACHE)
    return my_b_tell(&m_io_cache) - m_reserved_bytes;
  return m_io_cache.end_of_file - m_reserved_bytes;
}

bool IO_CACHE_binlog_cache_storage::enable_encryption() {
  /* Return earlier if already enabled */
  if (m_io_cache.m_encryptor != nullptr && m_io_cache.m_decryptor != nullptr)
    return false;

  if (rpl_encryption.is_enabled()) {
    std::unique_ptr<Rpl_encryption_header> header =
        Rpl_encryption_header::get_new_default_header();
    const Key_string password_str = header->generate_new_file_password();

    std::unique_ptr<Stream_cipher> encryptor = header->get_encryptor();
    if (encryptor->open(password_str, 0)) return true;

    std::unique_ptr<Stream_cipher> decryptor = header->get_decryptor();
    if (decryptor->open(password_str, 0)) return true;

    m_io_cache.m_encryptor = encryptor.release();
    m_io_cache.m_decryptor = decryptor.release();
  }
  return false;
}

void IO_CACHE_binlog_cache_storage::disable_encryption() {
  if (m_io_cache.m_encryptor != nullptr) {
    delete m_io_cache.m_encryptor;
    m_io_cache.m_encryptor = nullptr;
  }
  if (m_io_cache.m_decryptor != nullptr) {
    delete m_io_cache.m_decryptor;
    m_io_cache.m_decryptor = nullptr;
  }
}

bool IO_CACHE_binlog_cache_storage::setup_ciphers_password() {
  assert(m_io_cache.m_encryptor != nullptr &&
         m_io_cache.m_decryptor != nullptr);

  unsigned char password[Aes_ctr_encryptor::PASSWORD_LENGTH];
  Key_string password_str;

  /* Generate password, it is a random string. */
  if (my_rand_buffer(password, sizeof(password))) return true;
  password_str.append(password, sizeof(password));

  m_io_cache.m_encryptor->close();
  m_io_cache.m_decryptor->close();

  if (m_io_cache.m_encryptor->open(password_str, 0)) return true;
  if (m_io_cache.m_decryptor->open(password_str, 0)) return true;
  return false;
}

bool Binlog_cache_storage::open(my_off_t cache_size, my_off_t max_cache_size,
                                my_off_t reserved_bytes) {
  if (m_file.open(binlog_temp_files_dir.path(), kBinlogTempFilePrefix,
                  cache_size, max_cache_size, reserved_bytes))
    return true;
  m_pipeline_head = &m_file;
  return false;
}

void Binlog_cache_storage::close() {
  m_pipeline_head = nullptr;
  m_file.close();
}

Binlog_cache_storage::~Binlog_cache_storage() { close(); }

Binlog_encryption_ostream::~Binlog_encryption_ostream() { close(); }

#define THROW_RPL_ENCRYPTION_FAILED_TO_ENCRYPT_ERROR                        \
  char err_msg[MYSQL_ERRMSG_SIZE];                                          \
  ERR_error_string_n(ERR_get_error(), err_msg, MYSQL_ERRMSG_SIZE);          \
  LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_FAILED_TO_ENCRYPT, err_msg); \
  if (current_thd) {                                                        \
    if (current_thd->is_error()) current_thd->clear_error();                \
    my_error(ER_RPL_ENCRYPTION_FAILED_TO_ENCRYPT, MYF(0), err_msg);         \
  }

bool Binlog_encryption_ostream::open(
    std::unique_ptr<Truncatable_ostream> down_ostream) {
  assert(down_ostream != nullptr);
  m_header = Rpl_encryption_header::get_new_default_header();
  const Key_string password_str = m_header->generate_new_file_password();
  if (password_str.empty()) return true;
  m_encryptor.reset(nullptr);
  m_encryptor = m_header->get_encryptor();
  if (m_encryptor->open(password_str, m_header->get_header_size())) {
    THROW_RPL_ENCRYPTION_FAILED_TO_ENCRYPT_ERROR;
    m_encryptor.reset(nullptr);
    return true;
  }
  m_down_ostream = std::move(down_ostream);
  return m_header->serialize(m_down_ostream.get());
}

bool Binlog_encryption_ostream::open(
    std::unique_ptr<Truncatable_ostream> down_ostream,
    std::unique_ptr<Rpl_encryption_header> header) {
  assert(down_ostream != nullptr);

  m_down_ostream = std::move(down_ostream);
  m_header = std::move(header);
  m_encryptor.reset(nullptr);
  m_encryptor = m_header->get_encryptor();
  if (m_encryptor->open(m_header->decrypt_file_password(),
                        m_header->get_header_size())) {
    THROW_RPL_ENCRYPTION_FAILED_TO_ENCRYPT_ERROR;
    m_encryptor.reset(nullptr);
    return true;
  }

  return seek(0);
}

std::pair<bool, std::string> Binlog_encryption_ostream::reencrypt() {
  DBUG_TRACE;
  assert(m_header != nullptr);
  assert(m_down_ostream != nullptr);
  std::string error_message;

  /* Get the file password */
  Key_string password_str = m_header->decrypt_file_password();
  if (password_str.empty() ||
      DBUG_EVALUATE_IF("fail_to_decrypt_file_password", true, false)) {
    error_message.assign("failed to decrypt the file password");
    return std::make_pair(true, error_message);
  }
  if (m_down_ostream->seek(0) ||
      DBUG_EVALUATE_IF("fail_to_reset_file_stream", true, false)) {
    error_message.assign("failed to reset the file out stream");
    return std::make_pair(true, error_message);
  }
  m_header.reset(nullptr);
  m_header = Rpl_encryption_header::get_new_default_header();
  if (m_header->encrypt_file_password(password_str) ||
      DBUG_EVALUATE_IF("fail_to_encrypt_file_password", true, false)) {
    error_message.assign(
        "failed to encrypt the file password with current encryption key");
    return std::make_pair(true, error_message);
  }
  if (m_header->serialize(m_down_ostream.get()) ||
      DBUG_EVALUATE_IF("fail_to_write_reencrypted_header", true, false)) {
    error_message.assign("failed to write the new reencrypted file header");
    return std::make_pair(true, error_message);
  }
  if (flush() ||
      DBUG_EVALUATE_IF("fail_to_flush_reencrypted_header", true, false)) {
    error_message.assign("failed to flush the new reencrypted file header");
    return std::make_pair(true, error_message);
  }
  if (sync() ||
      DBUG_EVALUATE_IF("fail_to_sync_reencrypted_header", true, false)) {
    error_message.assign(
        "failed to synchronize the new reencrypted file header");
    return std::make_pair(true, error_message);
  }
  close();

  return std::make_pair(false, error_message);
}

void Binlog_encryption_ostream::close() {
  m_encryptor.reset(nullptr);
  m_header.reset(nullptr);
  m_down_ostream.reset(nullptr);
}

bool Binlog_encryption_ostream::write(const unsigned char *buffer,
                                      my_off_t length) {
  const int ENCRYPT_BUFFER_SIZE = 2048;
  unsigned char encrypt_buffer[ENCRYPT_BUFFER_SIZE];
  const unsigned char *ptr = buffer;

  /*
    Split the data in 'buffer' to ENCRYPT_BUFFER_SIZE bytes chunks and
    encrypt them one by one.
  */
  while (length > 0) {
    int encrypt_len =
        std::min(length, static_cast<my_off_t>(ENCRYPT_BUFFER_SIZE));

    if (m_encryptor->encrypt(encrypt_buffer, ptr, encrypt_len)) {
      THROW_RPL_ENCRYPTION_FAILED_TO_ENCRYPT_ERROR;
      return true;
    }

    if (m_down_ostream->write(encrypt_buffer, encrypt_len)) return true;

    ptr += encrypt_len;
    length -= encrypt_len;
  }
  return false;
}

bool Binlog_encryption_ostream::seek(my_off_t offset) {
  if (m_down_ostream->seek(m_header->get_header_size() + offset)) return true;
  return m_encryptor->set_stream_offset(offset);
}

bool Binlog_encryption_ostream::truncate(my_off_t offset) {
  if (m_down_ostream->truncate(m_header->get_header_size() + offset))
    return true;
  return m_encryptor->set_stream_offset(offset);
}

bool Binlog_encryption_ostream::flush() { return m_down_ostream->flush(); }

bool Binlog_encryption_ostream::sync() { return m_down_ostream->sync(); }

int Binlog_encryption_ostream::get_header_size() {
  return m_header->get_header_size();
}

Binlog_temp_files_dir binlog_temp_files_dir;

// Clears the temp files directory.
static bool temp_files_dir_clear_files(const char *path) {
  MY_DIR *dir_info = my_dir(path, MYF(MY_WANT_STAT));
  if (dir_info == nullptr) {
    LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_FAILED, path,
           my_errno());
    return true;
  }

  uint removed = 0;
  bool failed = false;
  for (uint i = 0; i < dir_info->number_off_files && !failed; i++) {
    const fileinfo *file = dir_info->dir_entry + i;
    /* Skip "." and "..". */
    if (file->name[0] == '.' &&
        (!file->name[1] || (file->name[1] == '.' && !file->name[2])))
      continue;
    char file_path[FN_REFLEN];
    if (snprintf(file_path, sizeof(file_path), "%s%c%s", path, FN_LIBCHAR,
                 file->name) >= static_cast<int>(sizeof(file_path))) {
      LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_INVALID, path);
      failed = true;
      break;
    }
    /* Do not follow links or delete entries the server did not create. */
    if (file->mystat == nullptr || !MY_S_ISREG(file->mystat->st_mode) ||
        my_is_symlink(file_path, nullptr) || !is_bolt_temp_file(file->name)) {
      LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_UNSAFE_ENTRY, path,
             file->name);
      failed = true;
      break;
    }
    if (my_delete(file_path, MYF(0))) {
      LogErr(ERROR_LEVEL, ER_BINLOG_CANT_DELETE_FILE, file_path);
      failed = true;
      break;
    }
    removed++;
  }
  my_dirend(dir_info);
  if (failed) return true;

  if (removed > 0)
    LogErr(INFORMATION_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_CLEANED, removed,
           path);
  return false;
}

bool Binlog_temp_files_dir::init(const char *log_basename) {
  DBUG_TRACE;
  assert(log_basename != nullptr && !m_initialized);

  // Build <log basename>/#binlog_temp_files
  char dir_part[FN_REFLEN];
  size_t dir_len;
  dirname_part(dir_part, log_basename, &dir_len);
  if (dir_len + strlen(kBinlogTempFilesDirName) + 1 > sizeof(m_path)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_FAILED, log_basename,
           ENAMETOOLONG);
    return true;
  }
  snprintf(m_path, sizeof(m_path), "%s%s", dir_part, kBinlogTempFilesDirName);
  const char *path = m_path;

  /* A symlink is rejected even if it points to a directory: files in
     this directory must be on the same filesystem as the binlog files. */
  if (my_is_symlink(path, nullptr)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_INVALID, path);
    return true;
  }

  MY_STAT stat_area;
  if (my_stat(path, &stat_area, MYF(0)) != nullptr) {
    if (!MY_S_ISDIR(stat_area.st_mode)) {
      LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_INVALID, path);
      return true;
    }
    if (temp_files_dir_clear_files(path)) return true;
  } else if (my_mkdir(path, my_umask_dir, MYF(0)) != 0) {
    LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_FAILED, path,
           my_errno());
    return true;
  }

  m_initialized = true;
  return false;
}
