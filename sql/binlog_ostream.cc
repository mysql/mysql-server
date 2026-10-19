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
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include "my_aes.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_rnd.h"
#include "my_sys.h"
#include "my_thread_local.h"  // my_errno
#include "mysql/components/services/log_builtins.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"
#include "mysys_err.h"          // EE_OUTOFMEMORY, EE_FILENOTFOUND
#include "sql/error_handler.h"  // Dummy_error_handler, Internal_error_handler
#include "sql/mysqld.h"
#include "sql/rpl_log_encryption.h"
#include "sql/sql_class.h"

#ifndef NDEBUG
bool binlog_cache_is_reset = false;
#endif

bool is_bolt_temp_file(const char *name) {
  constexpr std::string_view prefix{kBinlogTempFilePrefix};
  const std::string_view file_name{name};

  return file_name.starts_with(prefix) && file_name.size() > prefix.size() &&
         std::ranges::all_of(file_name.substr(prefix.size()), [](char c) {
           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_';
         });
}

namespace {

/*
  Creates a named spill file. The name is <prefix><server-start
  time>_<serial>. A leftover file of an earlier run that started in the same
  second can have the same name. Then the name is skipped and the next serial
  is tried. The file gets the permissions of a binary log file, because the
  file may become a binary log file.
*/
File create_binlog_cache_file(IO_CACHE *cache, char *name) {
  static std::atomic<uint64_t> serial_counter{0};
  const char *const dir = cache->dir != nullptr ? cache->dir : "";
  const char separator[2] = {cache->dir != nullptr ? FN_LIBCHAR : '\0', '\0'};
  for (;;) {
    const uint64_t serial =
        serial_counter.fetch_add(1, std::memory_order_relaxed);
    const int length =
        snprintf(name, FN_REFLEN, "%s%s%s%llx_%llx", dir, separator,
                 cache->prefix != nullptr ? cache->prefix : "",
                 static_cast<unsigned long long>(server_start_time),
                 static_cast<unsigned long long>(serial));
    if (length < 0 || length >= FN_REFLEN) {
      set_my_errno(ENAMETOOLONG);
      MyOsError(ENAMETOOLONG, EE_FILENOTFOUND, MYF(0), name);
      return -1;
    }
    DBUG_EXECUTE_IF("fail_bolt_spill_file_create", {
      MyOsError(ENOSPC, EE_FILENOTFOUND, MYF(0), name);
      return -1;
    });
    /* A leftover file of an earlier run has this name. */
    DBUG_EXECUTE_IF("simulate_bolt_spill_file_name_taken", {
      (void)my_close(my_create(name, 0, O_RDWR, MYF(0)), MYF(0));
      DBUG_SET("-d,simulate_bolt_spill_file_name_taken");
    });
    /* No MY_WME: a name that a leftover file has is skipped silently. */
    const File file =
        mysql_file_open(cache->file_key, name,
                        O_CREAT | O_EXCL | O_RDWR | MY_FOPEN_BINARY, MYF(0));
    if (file >= 0) return file;
    if (my_errno() == EEXIST) continue;
    /* The error that the open raises with MY_WME. */
    MyOsError(my_errno(), EE_FILENOTFOUND, MYF(0), name);
    return -1;
  }
}

/*
  Keeps the message of the first error that the rewrite raises, and keeps
  every condition that the rewrite raises out of the statement. The statement
  then shows no error and no warning when the transaction falls back. A fatal
  error is not kept out, because the statement stops with it.
*/
class Rewrite_error_handler : public Internal_error_handler {
 public:
  bool handle_condition(THD *thd, uint, const char *,
                        Sql_condition::enum_severity_level *level,
                        const char *msg) override {
    if (thd->is_fatal_error()) return false;
    if (*level == Sql_condition::SL_ERROR && m_message.empty()) m_message = msg;
    return true;
  }

  /** Returns the first error message, or an empty string if there was none. */
  const std::string &message() const { return m_message; }

 private:
  std::string m_message;
};

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

  /*
    The spill file is anonymous by default. An anonymous spill file is
    unlinked when the cache creates it, and is never visible.
    set_named_file() sets a creator instead. Then the spill file is a named
    bolt_ file that can be promoted. Only a cache rewritten into binlog file
    form calls set_named_file().
  */
  m_io_cache.named_file_creator = nullptr;

  /*
    Only a cache in binlog file form reserves bytes at the front of its file.
    The cache's content starts after the reserved bytes. The first flush into
    the lazily created file seeks there, because a new file's offset is 0.
    begin() starts reading after the reserved bytes, so a transaction that
    falls back never copies them into the binary log.
  */
  m_reserved_bytes = reserved_bytes;
  if (reserved_bytes != 0) {
    m_io_cache.pos_in_file = reserved_bytes;
    m_io_cache.seek_not_done = true;
  }

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

  return my_b_safe_write(&m_io_cache, buffer, length);
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

void IO_CACHE_binlog_cache_storage::set_named_file() {
  m_io_cache.named_file_creator = create_binlog_cache_file;
}

void IO_CACHE_binlog_cache_storage::release_named_file(
    binlog::Spilled_file_on_reset spilled_file_action) {
  disable_encryption();
  if (spilled_file_action == binlog::Spilled_file_on_reset::kKeep) {
    my_free(m_io_cache.file_name);
    m_io_cache.file_name = nullptr;
  }
  close();
}

bool IO_CACHE_binlog_cache_storage::reset(
    binlog::Spilled_file_on_reset spilled_file_action) {
  assert(spilled_file_action != binlog::Spilled_file_on_reset::kKeep ||
         m_io_cache.file_name != nullptr);
  /* Binlog_cache_storage::reset() replaces a cache with a named file. */
  if (m_io_cache.file_name != nullptr) {
    release_named_file(spilled_file_action);
    DBUG_EXECUTE_IF("ensure_binlog_cache_is_reset",
                    { binlog_cache_is_reset = true; };);
    return false;
  }

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
  /* A cache whose named file was released is closed, and holds nothing. */
  if (!my_b_inited(&m_io_cache)) return 0;
  /* Physical positions include the reserved bytes; report the data
     length. */
  if (m_io_cache.type == WRITE_CACHE)
    return my_b_tell(&m_io_cache) - m_reserved_bytes;
  return m_io_cache.end_of_file - m_reserved_bytes;
}

bool IO_CACHE_binlog_cache_storage::write_would_spill(my_off_t length) const {
  if (m_io_cache.type != WRITE_CACHE ||            // cache not in write mode
      m_io_cache.pos_in_file != m_reserved_bytes)  // data already in the file
    return false;
  /* A write that fills the buffer exactly does not flush it. */
  return length >
         static_cast<my_off_t>(m_io_cache.write_end - m_io_cache.write_pos);
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

bool Binlog_cache_storage::create_checksumless_representation() {
  /* Fails as an allocation of the new cache's buffer fails. */
  DBUG_EXECUTE_IF("fail_bolt_standard_form_restore", {
    my_error(EE_OUTOFMEMORY, MYF(ME_FATALERROR),
             static_cast<size_t>(m_cache_size));
    return true;
  });
  // Anonymous spill file, no reserved header region (the standard form).
  auto representation = binlog::create_checksumless_cache_representation(
      m_dir, m_prefix, m_cache_size, m_max_cache_size);
  if (representation == nullptr) return true;
  m_representation = std::move(representation);
  return false;
}

bool Binlog_cache_storage::open(my_off_t cache_size, my_off_t max_cache_size) {
  const char *LOG_PREFIX = "ML";

  /* The standard form spills to tmpdir; only a cache rewritten into binlog
     file form is in the binary log's temp files directory. */
  m_dir = mysql_tmpdir;
  m_prefix = LOG_PREFIX;
  m_cache_size = cache_size;
  m_max_cache_size = max_cache_size;
  return create_checksumless_representation();
}

void Binlog_cache_storage::close() { m_representation.reset(); }

Binlog_cache_storage::~Binlog_cache_storage() { close(); }

bool Binlog_cache_storage::write(const unsigned char *buffer, my_off_t length) {
  assert(m_representation != nullptr);

  if (m_large_trx_rewrite_planned &&
      m_representation->write_would_spill(length)) {
    m_large_trx_rewrite_planned = false;
    /*
      If the rewrite fails, the cache stays in the standard form, and this
      write spills as without the optimization. A condition that the rewrite
      raises must not reach the statement. The error log gets the message of
      the rewrite's first error instead.
    */
    Rewrite_error_handler rewrite_error_handler;
    THD *const thd = current_thd;
    if (thd != nullptr) thd->push_internal_handler(&rewrite_error_handler);
    const bool rewrite_failed = rewrite_into_binlog_file_format(
        m_large_trx_planned_checksum_alg, m_large_trx_planned_reserved_bytes);
    if (thd != nullptr) thd->pop_internal_handler();
    /* A fatal error, such as running out of memory, stops the statement. */
    if (rewrite_failed && thd != nullptr && thd->is_fatal_error()) return true;
    if (rewrite_failed) {
      m_large_trx_rewrite_failed = true;
      LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_REWRITE_FAILED,
             rewrite_error_handler.message().empty()
                 ? "no error was reported"
                 : rewrite_error_handler.message().c_str());
    }
  }
  return m_representation->write(buffer, length);
}

bool Binlog_cache_storage::truncate(my_off_t offset) {
  assert(m_representation != nullptr);
  return m_representation->truncate(offset);
}

bool Binlog_cache_storage::truncate_at_event_boundary(
    my_off_t offset, size_t events_before_offset) {
  assert(m_representation != nullptr);
  return m_representation->truncate_at_event_boundary(offset,
                                                      events_before_offset);
}

bool Binlog_cache_storage::reset(
    binlog::Spilled_file_on_reset spilled_file_action) {
  assert(m_representation != nullptr);
  m_large_trx_rewrite_planned = false;
  m_large_trx_rewrite_failed = false;
  const bool was_rewritten = is_large_trx_rewritten();
  const bool error = m_representation->reset(spilled_file_action);
  if (!was_rewritten) return error;
  /*
    A rewritten cache is replaced by a new cache in the standard checksumless
    form. So an error from the rewritten cache's own reset leaves nothing
    behind. Destroying the rewritten cache does not delete a promoted file:
    the rewritten cache's reset already dropped the promoted file's name.

    A failed replacement does not raise an error in the session. The
    transaction has ended, or is rolled back to its start. The replacement
    fails only when memory runs out. The caller reports the failure, and the
    next transaction's first event retries the replacement (see
    restore_standard_form()). No message is needed from the error.
  */
  Dummy_error_handler no_session_error;
  THD *const thd = current_thd;
  if (thd != nullptr) thd->push_internal_handler(&no_session_error);
  const bool replacement_failed = create_checksumless_representation();
  if (thd != nullptr) thd->pop_internal_handler();
  return replacement_failed;
}

bool Binlog_cache_storage::restore_standard_form() {
  if (!is_large_trx_rewritten()) return false;
  return create_checksumless_representation();
}

size_t Binlog_cache_storage::disk_writes() const {
  assert(m_representation != nullptr);
  return m_representation->disk_writes();
}

my_off_t Binlog_cache_storage::reserved_bytes() const {
  assert(m_representation != nullptr);
  return m_representation->reserved_bytes();
}

const char *Binlog_cache_storage::tmp_file_name() const {
  assert(m_representation != nullptr);
  return m_representation->tmp_file_name();
}

const char *Binlog_cache_storage::named_file_name() const {
  assert(m_representation != nullptr);
  return m_representation->named_file_name();
}

bool Binlog_cache_storage::is_spilled() const {
  assert(m_representation != nullptr);
  return m_representation->is_spilled();
}

File Binlog_cache_storage::spilled_file() const {
  assert(m_representation != nullptr);
  return m_representation->spilled_file();
}

bool Binlog_cache_storage::is_encrypted() const {
  assert(m_representation != nullptr);
  return m_representation->is_encrypted();
}

bool Binlog_cache_storage::flush_and_sync_spilled_file() {
  assert(m_representation != nullptr);
  return m_representation->flush_and_sync_spilled_file();
}

bool Binlog_cache_storage::is_large_trx_rewritten() const {
  assert(m_representation != nullptr);
  return m_representation->is_large_trx_rewritten();
}

void Binlog_cache_storage::plan_large_trx_rewrite(
    mysql::binlog::event::enum_binlog_checksum_alg checksum_alg,
    my_off_t reserved_bytes) {
  assert(m_representation != nullptr);
  assert(!m_representation->is_large_trx_rewritten());
  m_large_trx_rewrite_planned = true;
  m_large_trx_planned_checksum_alg = checksum_alg;
  m_large_trx_planned_reserved_bytes = reserved_bytes;
}

mysql::binlog::event::enum_binlog_checksum_alg
Binlog_cache_storage::physical_checksum_alg() const {
  assert(m_representation != nullptr);
  return m_representation->physical_checksum_alg();
}

void Binlog_cache_storage::add_logical_length(my_off_t logical_event_length) {
  assert(m_representation != nullptr);
  m_representation->add_logical_length(logical_event_length);
}

bool Binlog_cache_storage::rewrite_into_binlog_file_format(
    mysql::binlog::event::enum_binlog_checksum_alg checksum_alg,
    my_off_t reserved_bytes) {
  DBUG_TRACE;
  assert(m_representation != nullptr);

  /*
    The rewritten cache is always a named bolt_ file with the reserved header
    region. The cache is rewritten at most once per transaction, always from
    the standard form, at its first spill (write()).
  */
  assert(!m_representation->is_large_trx_rewritten());
  auto rewritten = binlog::create_checksummed_cache_representation(
      *m_representation, checksum_alg, binlog_temp_files_dir.path(),
      kBinlogTempFilePrefix, m_cache_size, m_max_cache_size, reserved_bytes);
  if (rewritten == nullptr) return true;
  m_representation = std::move(rewritten);
  return false;
}

bool Binlog_cache_storage::copy_to(Basic_ostream *ostream,
                                   bool *ostream_error) {
  DBUG_TRACE;
  assert(m_representation != nullptr);
  assert(ostream != nullptr);
  return m_representation->copy_logical_to(*ostream, ostream_error);
}

my_off_t Binlog_cache_storage::length() const {
  assert(m_representation != nullptr);
  return m_representation->logical_length();
}

my_off_t Binlog_cache_storage::physical_length() const {
  assert(m_representation != nullptr);
  return m_representation->physical_length();
}

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

/*
  Deletes the bolt_* spill files that a previous run left in the binlog temp
  files directory. Binlog_temp_files_dir::init() calls this function during
  startup.

  Only regular files with the bolt_* name are deleted. Any other entry was not
  created by the binary log cache and is left in place with a warning. A
  bolt_* file that cannot be deleted is also left in place with a warning.
  The leftover file is never reused, because every new spill file gets a
  unique name.

  Returns true when the directory cannot be listed, having logged the reason
  at the given level. No result of this function stops the server startup.
*/
static bool temp_files_dir_clear_files(const char *path, loglevel level) {
  MY_DIR *dir_info = my_dir(path, MYF(MY_WANT_STAT));
  if (dir_info == nullptr) {
    LogErr(level, ER_BINLOG_BOLT_TEMP_FILES_DIR_FAILED, path, my_errno());
    return true;
  }

  uint removed = 0;
  for (uint i = 0; i < dir_info->number_off_files; i++) {
    const fileinfo *file = dir_info->dir_entry + i;
    /* Skip "." and "..". */
    if (file->name[0] == '.' &&
        (!file->name[1] || (file->name[1] == '.' && !file->name[2])))
      continue;
    char file_path[FN_REFLEN];
    if (snprintf(file_path, sizeof(file_path), "%s%c%s", path, FN_LIBCHAR,
                 file->name) >= static_cast<int>(sizeof(file_path))) {
      LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_SKIPPED_ENTRY,
             file->name, path);
      continue;
    }
    /* Do not follow links or delete entries the server did not create. */
    if (file->mystat == nullptr || !MY_S_ISREG(file->mystat->st_mode) ||
        my_is_symlink(file_path, nullptr) || !is_bolt_temp_file(file->name)) {
      LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_SKIPPED_ENTRY,
             file->name, path);
      continue;
    }
    if (my_delete(file_path, MYF(0))) {
      LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_UNDELETED_FILE,
             file_path, my_errno());
      continue;
    }
    removed++;
  }
  my_dirend(dir_info);

  if (removed > 0)
    LogErr(INFORMATION_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_CLEANED, removed,
           path);
  return false;
}

namespace {

/** What a binlog temp files directory path refers to. */
enum class Temp_files_dir_state { kMissing, kDirectory, kInvalid };

/*
  Checks what path is: it must be a directory and not a symlink, so that the
  files in it are on the same filesystem as the binary log files. Logs the
  reason at the given level when it is invalid.
*/
Temp_files_dir_state temp_files_dir_state(const char *path, loglevel level) {
  if (my_is_symlink(path, nullptr)) {
    LogErr(level, ER_BINLOG_BOLT_TEMP_FILES_DIR_INVALID, path);
    return Temp_files_dir_state::kInvalid;
  }
  MY_STAT stat_area;
  if (my_stat(path, &stat_area, MYF(0)) == nullptr)
    return Temp_files_dir_state::kMissing;
  if (!MY_S_ISDIR(stat_area.st_mode)) {
    LogErr(level, ER_BINLOG_BOLT_TEMP_FILES_DIR_INVALID, path);
    return Temp_files_dir_state::kInvalid;
  }
  return Temp_files_dir_state::kDirectory;
}

/*
  Creates the directory at path unless it exists. Another session or another
  server sharing the binlog directory may create it at the same time: an
  EEXIST that leaves a directory is success.

  Returns true on failure, having logged the reason as an error.
*/
bool temp_files_dir_create(const char *path) {
  switch (temp_files_dir_state(path, ERROR_LEVEL)) {
    case Temp_files_dir_state::kDirectory:
      return false;
    case Temp_files_dir_state::kInvalid:
      return true;
    case Temp_files_dir_state::kMissing:
      break;
  }
  if (my_mkdir(path, my_umask_dir, MYF(0)) == 0) return false;
  if (my_errno() == EEXIST && temp_files_dir_state(path, ERROR_LEVEL) ==
                                  Temp_files_dir_state::kDirectory)
    return false;
  LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_FAILED, path, my_errno());
  return true;
}

}  // namespace

bool Binlog_temp_files_dir::init(const char *log_basename, loglevel level) {
  DBUG_TRACE;
  assert(log_basename != nullptr && !m_initialized);
  m_log_basename = log_basename;

  /*
    Build <binlog directory>/#binlog_temp_files/<binlog base name>. Servers
    that share a binlog directory have different base names, so each one
    creates and cleans only its own directory.
  */
  char dir_part[FN_REFLEN];
  size_t dir_part_len;  // Required out-param of dirname_part(); value unused.
  const char *const base_name =
      log_basename + dirname_part(dir_part, log_basename, &dir_part_len);

  const int parent_len = snprintf(m_parent, sizeof(m_parent), "%s%s", dir_part,
                                  kBinlogTempFilesDirName);
  const int path_len = snprintf(m_path, sizeof(m_path), "%s%c%s", m_parent,
                                FN_LIBCHAR, base_name);
  if (parent_len < 0 || static_cast<size_t>(parent_len) >= sizeof(m_parent) ||
      path_len < 0 || static_cast<size_t>(path_len) >= sizeof(m_path) ||
      base_name[0] == '\0') {
    LogErr(level, ER_BINLOG_BOLT_TEMP_FILES_DIR_PATH_INVALID, log_basename,
           base_name[0] == '\0' ? EINVAL : ENAMETOOLONG);
    m_init_failed = true;
    return true;
  }

  /* Files left by a previous run are deleted whatever the optimization's
     setting: it may have been ON then. */
  if (temp_files_dir_state(m_parent, level) == Temp_files_dir_state::kInvalid) {
    m_init_failed = true;
    return true;
  }
  switch (temp_files_dir_state(m_path, level)) {
    case Temp_files_dir_state::kInvalid:
      m_init_failed = true;
      return true;
    case Temp_files_dir_state::kDirectory:
      if (temp_files_dir_clear_files(m_path, level)) {
        m_init_failed = true;
        return true;
      }
      break;
    case Temp_files_dir_state::kMissing:
      break;
  }

  m_initialized = true;
  return false;
}

bool Binlog_temp_files_dir::create() {
  DBUG_TRACE;
  /* The reason was logged when init() failed at startup. */
  if (m_init_failed) {
    LogErr(ERROR_LEVEL, ER_BINLOG_BOLT_TEMP_FILES_DIR_UNUSABLE, m_log_basename);
    return true;
  }
  /* The binary log is off, so init() was never called. */
  if (!m_initialized) return false;
  return temp_files_dir_create(m_parent) || temp_files_dir_create(m_path);
}
