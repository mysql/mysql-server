// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef BINLOG_CACHE_REPRESENTATION_H
#define BINLOG_CACHE_REPRESENTATION_H

/**
  @file

  @brief The interface of the binlog cache representations, which store a
  session's binlog cache in the standard form or in the binary log file form
  of the binary log large transaction optimization (BOLT).
*/

#include <memory>
#include <utility>

#include "my_io.h"
#include "mysql/binlog/event/binlog_event.h"
#include "sql/basic_ostream.h"

namespace binlog {

/** What a reset does with a binlog cache's spilled file. */
enum class Spilled_file_on_reset {
  /** Delete the spilled file, as for any binlog cache. */
  kDelete,
  /** Keep the spilled file: it was promoted and is now a binary log file. */
  kKeep,
};

/**
  @class Binlog_cache_representation

  @brief How the bytes of a THD binlog cache are stored.

  Binlog_cache_representation is a layer between Binlog_cache_storage (the
  byte-container owned by binlog_cache_data) and
  IO_CACHE_binlog_cache_storage (the IO_CACHE with its temporary file).
  Binlog_cache_storage owns exactly one representation, through a pointer,
  and forwards every read and write to it; only the representation knows the
  byte layout of the cache.

  It is the abstract base class of two representations, described in
  sql/binlog/cache_representation.cc: see
  Checksumless_binlog_cache_representation (the standard form) and
  Checksummed_binlog_cache_representation (the binlog file form).

  Besides the storage operations, forwarded to the
  IO_CACHE_binlog_cache_storage, the class provides:

  - two views of the content: the logical view (copy_logical_to(),
    logical_length()), always in the standard form, which is what the
    standard binlog copy, compression and observers such as Group Replication
    read; and the physical view (copy_physical_to(),
    physical_length()), the bytes as stored, which is what BOLT
    promotes;
  - truncation for savepoints and statement rollback at logical offsets,
    mapped to physical offsets (truncate_at_event_boundary());
  - the check that triggers the rewrite: whether a write would make the
    standard-form cache spill (write_would_spill()). The rewrite
    then happens from memory, before anything reaches disk.
*/
class Binlog_cache_representation : public Truncatable_ostream {
 public:
  using Checksum_alg = mysql::binlog::event::enum_binlog_checksum_alg;

  ~Binlog_cache_representation() override = default;

  /**
    Open the cache: an in-memory buffer, and a temporary file created when the
    buffer overflows. Forwarded to IO_CACHE_binlog_cache_storage::open().

    @param dir             Directory of the temporary file.
    @param prefix          Prefix of the temporary file's name.
    @param cache_size      Size of the in-memory buffer.
    @param max_cache_size  Maximum size of the cache.
    @param reserved_bytes  Bytes reserved at the front of the temporary file
                           for the binary log header events.

    @retval false  Success.
    @retval true   Error.
  */
  virtual bool open(const char *dir, const char *prefix, my_off_t cache_size,
                    my_off_t max_cache_size, my_off_t reserved_bytes) = 0;
  /** Close the cache, deleting a named temporary file. */
  virtual void close() = 0;
  /**
    Drop all data. Forwarded to IO_CACHE_binlog_cache_storage::reset().

    @param spilled_file_action  kKeep when the spilled file was promoted to a
                                binary log file: the spilled file is closed
                                but not deleted.

    @retval false  Success.
    @retval true   Error.
  */
  virtual bool reset(Spilled_file_on_reset spilled_file_action) = 0;
  /** @return The number of writes to the temporary file. */
  virtual size_t disk_writes() const = 0;
  /** @return The bytes reserved at the front of the temporary file. */
  virtual my_off_t reserved_bytes() const = 0;
  /** @return The temporary file's name, or nullptr for an anonymous file. */
  virtual const char *tmp_file_name() const = 0;
  /** @return True once the cache has overflowed into its temporary file. */
  virtual bool is_spilled() const = 0;
  /** @return The temporary file's descriptor, or -1 before the spill. */
  virtual File spilled_file() const = 0;
  /** @return True if the temporary file is encrypted. */
  virtual bool is_encrypted() const = 0;
  /**
    Flush the buffered bytes into the temporary file and sync it. The cache
    must be spilled.

    @retval false  Success.
    @retval true   Error.
  */
  virtual bool flush_and_sync_spilled_file() = 0;

  /** @return The checksum algorithm of the stored events: OFF in the standard
              form. */
  virtual Checksum_alg physical_checksum_alg() const = 0;
  /**
    Add an event written in binlog file form to the logical length. Does
    nothing in the standard form, whose logical length is the stored length.

    @param logical_event_length  The event's length without its footer.
  */
  virtual void add_logical_length(my_off_t logical_event_length) = 0;
  /**
    Copy the cache to ostream in the standard form.

    @param ostream        Where the data is copied to.
    @param ostream_error  Set to true when the error is on ostream.

    @retval false  Success.
    @retval true   Error, reading the cache or writing ostream.
  */
  virtual bool copy_logical_to(Basic_ostream &ostream, bool *ostream_error) = 0;
  /** @return The cache's length in the standard form, as SQL and savepoints
              see it. */
  virtual my_off_t logical_length() const = 0;
  /**
    Copy the cache to ostream as stored, with no conversion.

    @param ostream        Where the data is copied to.
    @param ostream_error  Set to true when the error is on ostream.

    @retval false  Success.
    @retval true   Error, reading the cache or writing ostream.
  */
  virtual bool copy_physical_to(Basic_ostream &ostream,
                                bool *ostream_error) = 0;
  /** @return The cache's length as stored, including footers. */
  virtual my_off_t physical_length() const = 0;
  /**
    Truncate the cache at an event boundary, without reading it: the physical
    offset is logical_offset + events_before_offset * the footer length.

    @param logical_offset        The offset in the standard form; an event
                                 boundary.
    @param events_before_offset  The number of events before logical_offset.

    @retval false  Success.
    @retval true   Error: the offset is past the end of the cache, or the
                   truncation failed.
  */
  virtual bool truncate_at_event_boundary(my_off_t logical_offset,
                                          size_t events_before_offset) = 0;
  /**
    @return True if the cache was rewritten into binlog file form (real
            log_pos, and a footer when the algorithm is not OFF); false for
            the standard form.
  */
  virtual bool is_large_trx_rewritten() const = 0;
  /**
    Whether writing length bytes would make the cache spill for the first
    time. Only a transaction that planned a rewrite calls it
    (Binlog_cache_storage::write()).

    @param length  The length of the write.

    @return True if the write would spill a cache that has not spilled yet.
            False otherwise, and always for a cache in binlog file form, which
            has spilled already.
  */
  virtual bool write_would_spill(my_off_t length) const = 0;
};

/**
  @param checksum_alg  A binlog checksum algorithm.

  @return True if events with this algorithm carry a checksum footer: false
          for OFF and UNDEF.
*/
[[nodiscard]] bool is_checksum_alg_in_use(
    mysql::binlog::event::enum_binlog_checksum_alg checksum_alg);

/**
  Create and open a representation in the standard form: events as the
  standard commit path caches them, an anonymous spill file, no reserved
  header region.

  @param dir             Directory of the temporary file.
  @param prefix          Prefix of the temporary file's name.
  @param cache_size      Size of the in-memory buffer.
  @param max_cache_size  Maximum size of the cache.

  @return The representation, or nullptr on error.
*/
[[nodiscard]] std::unique_ptr<Binlog_cache_representation>
create_checksumless_cache_representation(const char *dir, const char *prefix,
                                         my_off_t cache_size,
                                         my_off_t max_cache_size);

/**
  Rewrite a standard form cache into a new representation in binlog file
  form: a named bolt_ temp file with reserved_bytes reserved at the front for
  the header events, real log_pos values, and a CRC footer per event when
  checksum_alg is CRC32. The source cache is not modified. A source cache
  that has not spilled is read from memory without being flushed.

  The source cache may end inside an event, whose remaining bytes are still to
  be written in the standard form: the new representation rewrites the bytes
  written to it until that event ends, then takes events in binlog file form.
  This is how the rewrite runs in the middle of an event, at the first byte
  that does not fit in memory.

  @param source          The cache in the standard form.
  @param checksum_alg    binlog_checksum for the rewritten events.
  @param dir             Directory of the temporary file.
  @param prefix          Prefix of the temporary file's name.
  @param cache_size      Size of the in-memory buffer.
  @param max_cache_size  Maximum size of the cache.
  @param reserved_bytes  Bytes reserved at the front of the temporary file.

  @return The new representation, or nullptr on error.
*/
[[nodiscard]] std::unique_ptr<Binlog_cache_representation>
create_checksummed_cache_representation(
    Binlog_cache_representation &source,
    Binlog_cache_representation::Checksum_alg checksum_alg, const char *dir,
    const char *prefix, my_off_t cache_size, my_off_t max_cache_size,
    my_off_t reserved_bytes);

}  // namespace binlog

#endif  // BINLOG_CACHE_REPRESENTATION_H
