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
#include "my_aes.h"
#include "my_inttypes.h"
#include "my_rnd.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"
#include "sql/mysqld.h"
#include "sql/rpl_log_encryption.h"
#include "sql/sql_class.h"

/*
  Globals owned by the Binlog_commit_by_rotate feature (defined in
  sql/binlog.cc). They are declared here rather than pulling in the whole header
  to keep this low level file free of binlog layer dependencies.

  - binlog_cache_dir        The #binlog_cache_files directory where the reserved
                            (KEEP) temporary files are created.
  - binlog_cache_reserved_size()  The space (in bytes) to reserve at the begin
                            of the transactional cache temporary file.
  - binlog_commit_by_rotate_enabled()  Whether the feature is switched on.
*/
extern char binlog_cache_dir[FN_REFLEN];
extern uint32 binlog_cache_reserved_size();
extern bool binlog_commit_by_rotate_enabled();

#ifndef NDEBUG
bool binlog_cache_is_reset = false;
#endif

IO_CACHE_binlog_cache_storage::IO_CACHE_binlog_cache_storage() = default;
IO_CACHE_binlog_cache_storage::~IO_CACHE_binlog_cache_storage() { close(); }

bool IO_CACHE_binlog_cache_storage::open(const char *dir, const char *prefix,
                                         my_off_t cache_size,
                                         my_off_t max_cache_size) {
  DBUG_TRACE;
  if (open_cached_file(&m_io_cache, dir, prefix, cache_size, MYF(MY_WME)))
    return true;

  if (rpl_encryption.is_enabled()) enable_encryption();

  m_max_cache_size = max_cache_size;
  /* Set the max cache size for IO_CACHE */
  m_io_cache.end_of_file = max_cache_size;
  return false;
}

void IO_CACHE_binlog_cache_storage::close() {
  /*
    The binlog cache temporary file is a normal (KEEP) file, so it must be
    unlinked explicitly here. It is not unlinked when it has been detached
    (renamed to a binary log file), in which case m_io_cache.file is -1.
  */
  if (m_io_cache.file != -1) unlink(tmp_file_name());

  close_cached_file(&m_io_cache);
}

bool IO_CACHE_binlog_cache_storage::write(const unsigned char *buffer,
                                          my_off_t length) {
  /*
    The binlog cache always uses a normal (KEEP) file in the #binlog_cache_files
    directory as its temporary file, so that it can be renamed to a binary log
    file when space is reserved (see Binlog_commit_by_rotate). Create it here,
    before the IO_CACHE would create its own unlinked file on buffer overflow.
  */
  if (m_io_cache.file == -1 &&
      m_io_cache.write_pos + length > m_io_cache.write_end) {
    char name_buff[FN_REFLEN];
    generate_tmp_file_name(name_buff);
    if ((m_io_cache.file = mysql_file_open(m_io_cache.file_key, name_buff,
                                           O_CREAT | O_RDWR, MYF(MY_WME))) <
        0) {
      LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
             "Failed to open a binlog cache temporary file.");
      return true;
    }
  }

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

  return my_b_safe_write(&m_io_cache, buffer, length);
}

bool IO_CACHE_binlog_cache_storage::truncate(my_off_t offset) {
  /*
    Skip the reserved space at the beginning of the temporary file. It is hidden
    from callers, so truncate(0) truncates the file to m_file_reserved_bytes,
    not to 0.
  */
  offset += m_file_reserved_bytes;
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

bool IO_CACHE_binlog_cache_storage::reset() {
  /* m_file_reserved_bytes must be reset to 0 before truncate. */
  m_file_reserved_bytes = 0;
  if (truncate(0)) return true;

  /* Truncate the temporary file if there is one. */
  if (m_io_cache.file != -1) {
    if (my_chsize(m_io_cache.file, 0, 0, MYF(MY_WME))) return true;

    DBUG_EXECUTE_IF("show_io_cache_size", {
      my_off_t file_size =
          my_seek(m_io_cache.file, 0L, MY_SEEK_END, MYF(MY_WME + MY_FAE));
      assert(file_size == 0);
    });
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

const char *IO_CACHE_binlog_cache_storage::tmp_file_name() const {
  return my_filename(m_io_cache.file);
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

  /*
    Start reading after the reserved space at the beginning of the temporary
    file. m_file_reserved_bytes is 0 unless the file reserves space for being
    renamed to a binary log file (see Binlog_commit_by_rotate).
  */
  if (reinit_io_cache(&m_io_cache, READ_CACHE, m_file_reserved_bytes, false,
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

my_off_t IO_CACHE_binlog_cache_storage::raw_length() const {
  if (m_io_cache.type == WRITE_CACHE) return my_b_tell(&m_io_cache);
  return m_io_cache.end_of_file;
}

my_off_t IO_CACHE_binlog_cache_storage::length() const {
  /*
    Hide the reserved space at the beginning of the temporary file. So length()
    still returns the length of the binlog data written into the cache, not the
    file length. m_file_reserved_bytes is 0 unless space is reserved.
  */
  return raw_length() - m_file_reserved_bytes;
}

my_off_t IO_CACHE_binlog_cache_storage::get_file_end_pos() const {
  return raw_length();
}

void IO_CACHE_binlog_cache_storage::generate_tmp_file_name(char *name) {
  /*
    The temporary file is named with the cache prefix and the memory address of
    the IO_CACHE which guarantees it is unique. The file is created in the
    #binlog_cache_files directory, next to the binary log files, so that it can
    be renamed to a binary log file at commit time. init_binlog_cache_dir()
    guarantees the full name fits in FN_REFLEN; the return value is checked so
    the compiler does not warn about a (here impossible) truncation.
  */
  if (snprintf(name, FN_REFLEN, "%s/%s_%llu", binlog_cache_dir,
               m_io_cache.prefix, (ulonglong)&m_io_cache) >= FN_REFLEN)
    name[FN_REFLEN - 1] = '\0';
}

void IO_CACHE_binlog_cache_storage::init_file_reserved_bytes() {
  /*
    Space is reserved only while the binlog_large_commit_threshold feature is on
    and the cache is not encrypted (the reserved file becomes a binary log file,
    which uses a different encryption key than the cache temporary file).
  */
  const bool should_enable =
      binlog_commit_by_rotate_enabled() && !is_encryption_enabled();

  /*
    binlog_cache_reserved_size() is already aligned to IO_SIZE (see
    Binlog_commit_by_rotate::set_reserved_bytes()), which keeps the reserved
    region from reducing the cache buffer in reinit_io_cache().
  */
  my_off_t reserved = should_enable ? binlog_cache_reserved_size() : 0;

  DBUG_EXECUTE_IF("simulate_small_binlog_cache_reserved_space",
                  reserved = 100;);

  m_file_reserved_bytes = reserved;

  /*
    Seek past the reserved space at the beginning of the temporary file. This
    sets pos_in_file to m_file_reserved_bytes and seek_not_done to true. The
    file is created when the buffer is full, and is sought to pos_in_file before
    writing into it.
  */
  if (reserved != 0) {
    reinit_io_cache(&m_io_cache, WRITE_CACHE, reserved, false, true);
    m_io_cache.end_of_file = m_max_cache_size;
  }
}

my_off_t IO_CACHE_binlog_cache_storage::get_file_reserved_size() {
  /* Reserve space on the first write, while nothing is written yet. */
  if (raw_length() == 0) init_file_reserved_bytes();
  return m_file_reserved_bytes;
}

void IO_CACHE_binlog_cache_storage::detach_temp_file() {
  /*
    If a rollback to savepoint happened before, the real length of the
    temporary file can be greater than the binlog data end position. Truncate
    the file to its end position so it becomes a valid binary log file.
  */
  my_chsize(m_io_cache.file, get_file_end_pos(), 0, MYF(MY_WME));

  mysql_file_close(m_io_cache.file, MYF(0));
  /* Reset the fd so that the cache no longer owns the (now binary log) file. */
  m_io_cache.file = -1;
}

bool IO_CACHE_binlog_cache_storage::sync_temp_file() {
  assert(m_io_cache.file != -1);

  if (my_b_flush_io_cache(&m_io_cache, 1)) return true;
  if (mysql_file_sync(m_io_cache.file, MYF(MY_WME))) return true;
  return false;
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

bool Binlog_cache_storage::open(my_off_t cache_size, my_off_t max_cache_size) {
  const char *LOG_PREFIX = "ML";

  if (m_file.open(mysql_tmpdir, LOG_PREFIX, cache_size, max_cache_size))
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
