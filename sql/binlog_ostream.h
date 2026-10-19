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

#ifndef BINLOG_OSTREAM_INCLUDED
#define BINLOG_OSTREAM_INCLUDED

#include <openssl/evp.h>
#include <cassert>
#include <memory>
#include <string>
#include "my_io.h"              // FN_REFLEN
#include "mysql/my_loglevel.h"  // loglevel
#include "sql/basic_ostream.h"
#include "sql/binlog/cache_representation.h"
#include "sql/rpl_log_encryption.h"

// True if binlog cache is reset.
#ifndef NDEBUG
extern bool binlog_cache_is_reset;
#endif

/**
   Copy data from an input stream to an output stream.

   @param[in] istream   the input stream where data will be copied from
   @param[out] ostream  the output stream where data will be copied into
   @param[out] ostream_error It will be set to true if an error happens on
                             ostream and the pointer is not null. It is valid
                             only when the function returns true.

   @retval false Success
   @retval true Error happens in either the istream or ostream.
*/
template <class ISTREAM, class OSTREAM>
bool stream_copy(ISTREAM *istream, OSTREAM *ostream,
                 bool *ostream_error = nullptr) {
  DBUG_TRACE;
  unsigned char *buffer = nullptr;
  my_off_t length = 0;

  bool ret = istream->begin(&buffer, &length);
  while (!ret && length > 0) {
    if (ostream->write(buffer, length)) {
      if (ostream_error != nullptr) *ostream_error = true;
      return true;
    }

    ret = istream->next(&buffer, &length);
  }
  return ret;
}

/**
   A binlog cache implementation based on IO_CACHE.
*/
class IO_CACHE_binlog_cache_storage : public Truncatable_ostream {
 public:
  IO_CACHE_binlog_cache_storage();
  IO_CACHE_binlog_cache_storage &operator=(
      const IO_CACHE_binlog_cache_storage &) = delete;
  IO_CACHE_binlog_cache_storage(const IO_CACHE_binlog_cache_storage &) = delete;
  ~IO_CACHE_binlog_cache_storage() override;

  /**
     Opens the binlog cache. It creates a memory buffer as long as cache_size.
     The buffer will be extended up to max_cache_size when writing data. The
     data exceeding max_cache_size will be written into a temporary file.

     @param[in] dir  Where the temporary file will be created
     @param[in] prefix  Prefix of the temporary file name
     @param[in] cache_size  Size of the memory buffer.
     @param[in] max_cache_size  Maximum size of the memory buffer
     @param[in] reserved_bytes  Bytes reserved for header events
     @retval false  Success
     @retval true  Error
  */
  bool open(const char *dir, const char *prefix, my_off_t cache_size,
            my_off_t max_cache_size, my_off_t reserved_bytes);
  void close();

  bool write(const unsigned char *buffer, my_off_t length) override;
  bool truncate(my_off_t offset) override;
  /* purecov: inspected */
  /* binlog cache doesn't need seek operation. Setting true to return error */
  bool seek(my_off_t offset [[maybe_unused]]) override { return true; }
  /**
     Reset status and drop all data. A cache with a named spill file is
     closed instead, and is not usable afterwards: the caller replaces it.
     With kKeep, the caller has promoted the spilled file, and closing the
     cache does not delete that file.
  */
  bool reset(binlog::Spilled_file_on_reset spilled_file_action =
                 binlog::Spilled_file_on_reset::kDelete);
  /**
     Returns the file name if a temporary file is opened, otherwise nullptr is
     returned.
  */
  const char *tmp_file_name() const;
  /**
     Returns the path of the named spill file. Returns nullptr when the spill
     file is anonymous or not created yet.
  */
  const char *named_file_name() const { return m_io_cache.file_name; }
  /**
     Returns the count of calling temporary file's write()
  */
  size_t disk_writes() const;

  /**
     Initializes binlog cache for reading and returns the data at the begin.
     buffer is controlled by binlog cache implementation, so caller should
     not release it. If the function sets *length to 0 and no error happens,
     it has reached the end of the cache.

     @param[out] buffer  It points to buffer where data is read.
     @param[out] length  Length of the data in the buffer.
     @retval false  Success
     @retval true  Error
  */
  bool begin(unsigned char **buffer, my_off_t *length);
  /**
     Returns next piece of data. buffer is controlled by binlog cache
     implementation, so caller should not release it. If the function sets
     *length to 0 and no error happens, it has reached the end of the cache.

     @param[out] buffer  It points to buffer where data is read.
     @param[out] length  Length of the data in the buffer.
     @retval false  Success
     @retval true  Error
  */
  bool next(unsigned char **buffer, my_off_t *length);
  my_off_t length() const;
  /** Returns the bytes reserved at the front of the file. */
  my_off_t reserved_bytes() const { return m_reserved_bytes; }
  bool flush() override { return false; }
  bool sync() override { return false; }
  /**
     Returns true once the cache overflowed into its temporary file.
  */
  bool is_spilled() const;
  /**
     Returns the spilled temporary file's descriptor (-1 when not
     spilled). The cache retains ownership until reset.
  */
  File spilled_file() const { return m_io_cache.file; }
  /**
     Returns true if the cache's events are encrypted. This reflects the
     temporary file's actual encryption, which is fixed at spill time, and is
     independent of the current global binlog_encryption setting (which may
     have changed since the transaction spilled).
  */
  bool is_encrypted() const { return m_io_cache.m_encryptor != nullptr; }
  /**
     Flushes buffered bytes into the spilled temporary file and syncs the
     file to disk. The cache must be spilled.

     @retval false  Success
     @retval true   Error
  */
  bool flush_and_sync_spilled_file();
  /**
     Creates the temporary file without writing to it.

     @retval false  Success
     @retval true   Error
  */
  bool create_spilled_file() { return real_open_cached_file(&m_io_cache); }
  /**
     Makes the next lazily-created spill file a named file, kept in the
     filesystem namespace and therefore promotable, instead of an anonymous
     file. Called on the cache that the rewrite creates, before the cache's
     first spill.
  */
  void set_named_file();

  /**
    True if writing length bytes would be the cache's first write to its file
    since its last reset, because length is more than the free part of the
    in-memory buffer. An open file does not count, because a reset truncates
    the file and keeps it open.
  */
  bool write_would_spill(my_off_t length) const;

 private:
  /**
     A named spill file may have been promoted to a real binary log file, and
     the cache keeps its descriptor across the promotion. Truncating through
     that descriptor would empty the promoted file, so the cache is closed
     instead. The cache is not opened again, because only a rewritten cache
     has a named file, and Binlog_cache_storage::reset() replaces the
     rewritten cache. With kKeep, the binary log already holds the promoted
     file through its own descriptor, and the cache only drops the file's
     name, so closing the cache does not delete the file. With kDelete,
     closing the cache deletes the file.
  */
  void release_named_file(binlog::Spilled_file_on_reset spilled_file_action);
  IO_CACHE m_io_cache;
  my_off_t m_max_cache_size = 0;
  my_off_t m_reserved_bytes = 0;
  /**
    Enable IO Cache temporary file encryption.

    @retval false Success.
    @retval true Error.
  */
  bool enable_encryption();
  /**
    Disable IO Cache temporary file encryption.
  */
  void disable_encryption();
  /**
    Generate a new password for the temporary file encryption.

    This function is called by reset() that is called every time a transaction
    commits to cleanup the binary log cache. The file password shall vary not
    only per temporary file, but also per transaction being committed within a
    single client connection.

    @retval false Success.
    @retval true Error.
  */
  bool setup_ciphers_password();
};

/**
   Byte container that provides a storage for serializing session
   binlog events. This way of arranging the classes separates storage layer
   and binlog layer, hides the implementation detail of low level storage.
*/
class Binlog_cache_storage : public Basic_ostream {
 public:
  ~Binlog_cache_storage() override;

  bool open(my_off_t cache_size, my_off_t max_cache_size);
  void close();

  /**
    Write to the cache. When a rewrite is planned and this write would make
    the cache spill, the cache is first rewritten into binlog file form; if
    the rewrite fails, the write spills in the standard form.

    @param buffer  The bytes.
    @param length  Their number.

    @retval false  Success.
    @retval true   Error.
  */
  bool write(const unsigned char *buffer, my_off_t length) override;
  /**
     Truncates some data at the end of the binlog cache.

     @param[in] offset  Where the binlog cache will be truncated to, as a
                        logical (checksum-free) offset.
     @retval false  Success
     @retval true  Error
  */
  bool truncate(my_off_t offset);
  /** @see binlog::Binlog_cache_representation::truncate_at_event_boundary */
  bool truncate_at_event_boundary(my_off_t offset, size_t events_before_offset);

  /**
     Reset status and drop all data. With kKeep, the cache closes without
     deleting a file that the binary log large transaction optimization (BOLT)
     has already promoted. A cache rewritten into binlog file form is replaced
     by a new one in the standard form.

     @retval false  Success.
     @retval true   Error. When the cache is still in binlog file form
                    afterwards, the replacement failed: no error was raised in
                    the session, and restore_standard_form() retries it.
  */
  bool reset(binlog::Spilled_file_on_reset spilled_file_action =
                 binlog::Spilled_file_on_reset::kDelete);
  /**
     Returns the count of disk writes
  */
  size_t disk_writes() const;
  /**
     Returns the name of the temporary file.
  */
  const char *tmp_file_name() const;
  /**
     Returns the bytes reserved at the beginning of the temp file.
  */
  my_off_t reserved_bytes() const;
  /** @see IO_CACHE_binlog_cache_storage::named_file_name */
  const char *named_file_name() const;
  /** @see IO_CACHE_binlog_cache_storage::is_spilled */
  bool is_spilled() const;
  /** @see IO_CACHE_binlog_cache_storage::spilled_file */
  File spilled_file() const;
  /** @see IO_CACHE_binlog_cache_storage::is_encrypted */
  bool is_encrypted() const;
  /** @see IO_CACHE_binlog_cache_storage::flush_and_sync_spilled_file */
  bool flush_and_sync_spilled_file();
  /** True if the cache was rewritten into binlog file form. */
  bool is_large_trx_rewritten() const;
  /**
    Plan the rewrite into binlog file form: write() runs it before the first
    write that would make the cache spill.

    @param checksum_alg    binlog_checksum when the plan is made.
    @param reserved_bytes  The header region of the bolt_ temp file.
  */
  void plan_large_trx_rewrite(
      mysql::binlog::event::enum_binlog_checksum_alg checksum_alg,
      my_off_t reserved_bytes);
  /** Drop the plan made by plan_large_trx_rewrite(), if any. */
  void clear_large_trx_rewrite_plan() { m_large_trx_rewrite_planned = false; }
  /**
    Drop the rewrite plan and the result of a failed rewrite.
    binlog_cache_data::cache_state_rollback() calls this function at a
    rollback to the start of the transaction, because the transaction's next
    event latches the optimization again.
  */
  void clear_large_trx_rewrite_state() {
    m_large_trx_rewrite_planned = false;
    m_large_trx_rewrite_failed = false;
  }
  /**
    True if the planned rewrite was tried and failed. The cache then stays
    in the standard form and spills as without the optimization. Cleared by
    reset() and clear_large_trx_rewrite_state().
  */
  bool large_trx_rewrite_failed() const { return m_large_trx_rewrite_failed; }
  /**
    Puts a cache that is still in binlog file form back in the standard form.
    That happens only when reset() could not replace a rewritten cache. A new
    transaction must not write into the old rewritten representation, so
    binlog_cache_data::write_event() calls this at each transaction's first
    event. It does nothing when the cache is in the standard form.

    @retval false  The cache is in the standard form.
    @retval true   The replacement failed, and an error was raised in the
                   session. The write must fail.
  */
  [[nodiscard]] bool restore_standard_form();
  /** Footer bytes per event in the physical form (4 with CRC32, else 0). */
  my_off_t footer_length() const {
    return physical_events_have_checksum() ? BINLOG_CHECKSUM_LEN : 0;
  }
  /**
     Returns the checksum algorithm used by the cache's physical event bytes.
  */
  mysql::binlog::event::enum_binlog_checksum_alg physical_checksum_alg() const;
  /**
     Returns true if the cache's physical event bytes include checksum footers.
  */
  bool physical_events_have_checksum() const {
    return binlog::is_checksum_alg_in_use(physical_checksum_alg());
  }
  /**
     Add the logical size of one event written in binlog file form. The
     logical size excludes the checksum footer.
  */
  void add_logical_length(my_off_t logical_event_length);
  /**
     Copy the cache contents to an output stream in the standard form (no
     checksum footers, log_pos 0), whatever form the cache stores them in.

     @param[out] ostream Where the data will be copied into
     @param[out] ostream_error It will be set to true if an error happens on
                               ostream and the pointer is not null. It is valid
                               only when the function returns true.
     @retval false  Success
     @retval true  Error happens in either the istream or ostream.
  */
  bool copy_to(Basic_ostream *ostream, bool *ostream_error = nullptr);

  /**
     Returns logical data length.
  */
  my_off_t length() const;
  /**
     Returns physical data length.
  */
  my_off_t physical_length() const;
  /**
     Returns true if binlog cache is empty.
  */
  bool is_empty() const { return length() == 0; }

 private:
  /**
    Replace the representation with a new one in the standard form.

    @retval false  Success.
    @retval true   Error.
  */
  [[nodiscard]] bool create_checksumless_representation();
  /**
    Rewrite the cache, in the standard form, into binlog file form (see
    binlog::create_checksummed_cache_representation()).

    @param checksum_alg    binlog_checksum for the rewritten events.
    @param reserved_bytes  The header region of the bolt_ temp file.

    @retval false  Success.
    @retval true   Error: the cache stays in the standard form.
  */
  bool rewrite_into_binlog_file_format(
      mysql::binlog::event::enum_binlog_checksum_alg checksum_alg,
      my_off_t reserved_bytes);

  /** The cache's bytes, in the standard or the binlog file form. */
  std::unique_ptr<binlog::Binlog_cache_representation> m_representation;
  /** @see plan_large_trx_rewrite() */
  bool m_large_trx_rewrite_planned = false;
  /** @see large_trx_rewrite_failed() */
  bool m_large_trx_rewrite_failed = false;
  /** The checksum algorithm of the planned rewrite. */
  mysql::binlog::event::enum_binlog_checksum_alg
      m_large_trx_planned_checksum_alg =
          mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
  /** The reserved header region of the planned rewrite. */
  my_off_t m_large_trx_planned_reserved_bytes = 0;
  /* The open() arguments, kept for creating replacement representations. */
  const char *m_dir = nullptr;
  const char *m_prefix = nullptr;
  my_off_t m_cache_size = 0;
  my_off_t m_max_cache_size = 0;
};

/**
  It is an Truncatable_ostream which provides encryption feature. It can be
  setup into an stream pipeline. In the pipeline, it encrypts the data
  from up stream and then feeds the encrypted data into down stream.
*/
class Binlog_encryption_ostream : public Truncatable_ostream {
 public:
  ~Binlog_encryption_ostream() override;

  /**
    Initialize the context used in the encryption stream and write encryption
    header into down stream.

    @param[in] down_ostream The stream for storing encrypted data.

    @retval false Success
    @retval true Error.
  */
  bool open(std::unique_ptr<Truncatable_ostream> down_ostream);

  /**
    Initialize the context used in the encryption stream based on the
    header passed as parameter. It shall be used when opening an ostream for
    a stream that was already encrypted (the cypher password already exists).

    @param[in] down_ostream the stream for storing encrypted data.
    @param[in] header the encryption header to setup the cypher.

    @retval false Success.
    @retval true Error.
  */
  bool open(std::unique_ptr<Truncatable_ostream> down_ostream,
            std::unique_ptr<Rpl_encryption_header> header);

  /**
    Re-encrypt the encrypted binary/relay log file header by replacing its
    binlog encryption key id with the current one and its encrypted file
    password with the new one, which is got by encrypting its file password
    with the current binlog encryption key.

    @retval false Success with an empty error message.
    @retval true Error with an error message.
  */
  std::pair<bool, std::string> reencrypt();

  void close();
  bool write(const unsigned char *buffer, my_off_t length) override;
  bool truncate(my_off_t offset) override;
  bool seek(my_off_t offset) override;
  bool flush() override;
  bool sync() override;
  /**
    Return the encrypted file header size.

    @return the encrypted file header size.
  */
  int get_header_size();

 private:
  std::unique_ptr<Truncatable_ostream> m_down_ostream;
  std::unique_ptr<Rpl_encryption_header> m_header;
  std::unique_ptr<Stream_cipher> m_encryptor;
};

/**
  Name of the directory, in the binary log directory, that holds one
  subdirectory per server for the spilled files of caches in binlog file form.
  The subdirectory is named after the server's binary log base name.
*/
inline constexpr const char *kBinlogTempFilesDirName = "#binlog_temp_files";

/**
  Allocation quantum for the header region reserved at the front of the
  spilled file of a cache rewritten into binlog file form.
*/
inline constexpr my_off_t kBinlogTempFileReservedBytes = 64 * 1024;

/** Minimum space left after the binary log header in the header region. */
inline constexpr my_off_t kBinlogTempFileHeaderHeadroomBytes = 32 * 1024;

/**
  Prefix of the spilled files of caches rewritten into binlog file form. The
  files are named bolt_<server-start time>_<serial>, both in hex. Startup
  cleanup deletes only files with this name pattern.
*/
inline constexpr char kBinlogTempFilePrefix[] = "bolt_";

/**
  Returns true if 'name' is the name of a BOLT spill file, that is, it matches
  the bolt_<id> pattern.

  A true result does NOT mean the file was, or will be, promoted to a binary
  log file: a transaction whose cache was rewritten may still fall back to
  the standard commit path. This is purely an ownership check, so that
  startup cleanup of \#binlog_temp_files only deletes files with BOLT's name
  pattern.

  @param name  Base name of a directory entry, without any directory part.

  @retval true   The name matches BOLT's spill file pattern.
  @retval false  It does not, so cleanup must leave the file alone.
*/
bool is_bolt_temp_file(const char *name);

/**
  The directory of the spilled files that the binary log large
  transaction optimization (BOLT) may promote. The directory exists once the
  optimization is enabled, and is never removed: a transaction that started
  with the optimization enabled may still write to its spilled file after
  the optimization is disabled.
*/
class Binlog_temp_files_dir {
 public:
  /**
    At server startup, with the binary log on: computes the directory's path
    and, if the directory exists, deletes the files a previous run left in it.
    Does not create it (see create()).

    @param log_basename  The log basename. The directory is
                         <directory part>/\#binlog_temp_files/<base name>.
    @param level         The level a failure is logged at: an error when the
                         optimization is enabled, otherwise a warning.

    @retval false  Success.
    @retval true   Failure; the reason has been logged at level.
  */
  bool init(const char *log_basename, loglevel level);

  /**
    Creates the directory if it does not exist. The server calls create() at
    startup when the optimization is enabled. The server also calls create()
    when binlog_large_transaction_optimization_enabled is set to ON.
    Concurrent calls are safe.

    When the binary log is off, init() is never called. Then create() does
    nothing and returns false. When init() failed, create() returns true.
    Then the optimization cannot be enabled without the directory.

    @retval false  Success.
    @retval true   Failure; an error has been logged.
  */
  bool create();

  /** @return The full path of the directory. */
  const char *path() const {
    assert(m_initialized);
    return m_path;
  }

 private:
  /** <binlog directory>/#binlog_temp_files, shared by servers. */
  char m_parent[FN_REFLEN];
  /** <binlog directory>/#binlog_temp_files/<binlog base name>. */
  char m_path[FN_REFLEN];
  /** @see init() */
  bool m_initialized{false};
  /** Set when init() failed. @see create() */
  bool m_init_failed{false};
  /** The log basename init() was called with. */
  const char *m_log_basename{nullptr};
};

/** The binary log's temp files directory (\#binlog_temp_files/<base name>). */
extern Binlog_temp_files_dir binlog_temp_files_dir;

#endif  // BINLOG_OSTREAM_INCLUDED
