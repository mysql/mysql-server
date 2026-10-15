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

/**
  @file

  @brief Implementation of the binlog cache representations
  (Checksumless_binlog_cache_representation and
  Checksummed_binlog_cache_representation) of the interface in
  sql/binlog/cache_representation.h.
*/

#include "sql/binlog/cache_representation.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <optional>

#include "my_byteorder.h"
#include "my_dbug.h"
#include "sql/binlog_ostream.h"
#include "sql/log_event.h"

namespace binlog {

namespace {

/**
  The standard form, used by every transaction until its first spill. Events
  are cached exactly as the standard commit path caches them: no checksum
  footer, log_pos 0. When the cache spills, IO_CACHE writes them to an
  anonymous temporary file.
*/
class Checksumless_binlog_cache_representation final
    : public Binlog_cache_representation {
 public:
  bool open(const char *dir, const char *prefix, my_off_t cache_size,
            my_off_t max_cache_size, my_off_t reserved_bytes) override {
    return m_file.open(dir, prefix, cache_size, max_cache_size, reserved_bytes);
  }

  void close() override { m_file.close(); }

  bool write(const unsigned char *buffer, my_off_t length) override {
    return m_file.write(buffer, length);
  }

  bool truncate(my_off_t logical_offset) override {
    return m_file.truncate(logical_offset);
  }

  /* The binlog cache does not seek: return an error. */
  bool seek(my_off_t offset [[maybe_unused]]) override { return true; }

  /* Nothing to do: flush_and_sync_spilled_file() flushes and syncs the file. */
  bool flush() override { return false; }

  /* Nothing to do: flush_and_sync_spilled_file() flushes and syncs the file. */
  bool sync() override { return false; }

  bool reset(Spilled_file_on_reset spilled_file_action) override {
    return m_file.reset(spilled_file_action);
  }

  size_t disk_writes() const override { return m_file.disk_writes(); }

  my_off_t reserved_bytes() const override { return m_file.reserved_bytes(); }

  const char *tmp_file_name() const override { return m_file.tmp_file_name(); }

  bool is_spilled() const override { return m_file.is_spilled(); }

  File spilled_file() const override { return m_file.spilled_file(); }

  bool is_encrypted() const override { return m_file.is_encrypted(); }

  bool flush_and_sync_spilled_file() override {
    return m_file.flush_and_sync_spilled_file();
  }

  Checksum_alg physical_checksum_alg() const override {
    return mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
  }

  void add_logical_length(my_off_t logical_event_length
                          [[maybe_unused]]) override {}

  bool copy_logical_to(Basic_ostream &ostream, bool *ostream_error) override {
    return stream_copy(&m_file, &ostream, ostream_error);
  }

  bool copy_physical_to(Basic_ostream &ostream, bool *ostream_error) override {
    return stream_copy(&m_file, &ostream, ostream_error);
  }

  my_off_t logical_length() const override { return m_file.length(); }

  my_off_t physical_length() const override { return m_file.length(); }

  bool truncate_at_event_boundary(my_off_t logical_offset,
                                  size_t events_before_offset
                                  [[maybe_unused]]) override {
    return truncate(logical_offset);
  }

  bool is_large_trx_rewritten() const override { return false; }
  bool write_would_spill(my_off_t length) const override {
    return m_file.write_would_spill(length);
  }

 private:
  /** The in-memory buffer and the anonymous temporary file. */
  IO_CACHE_binlog_cache_storage m_file;
};

/**
  The binlog file form, used by the binary log large transaction optimization
  (BOLT), from the first spill of an eligible transaction (see
  create_checksummed_cache_representation()). Every later event is written in
  the same form, both while it is in the in-memory buffer and after it reaches
  the file, so at commit the file can be promoted into the binary log sequence
  without being rewritten.
*/
class Checksummed_binlog_cache_representation final
    : public Binlog_cache_representation {
 public:
  /**
    @param checksum_alg  binlog_checksum for the events: CRC32 adds a footer
                         per event; OFF and UNDEF add none.
  */
  explicit Checksummed_binlog_cache_representation(Checksum_alg checksum_alg)
      : m_checksum_alg(is_checksum_alg_in_use(checksum_alg)
                           ? checksum_alg
                           : mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF),
        m_footer_len(is_checksum_alg_in_use(checksum_alg) ? BINLOG_CHECKSUM_LEN
                                                          : 0) {}

  /** @see create_checksummed_cache_representation() */
  static std::unique_ptr<Binlog_cache_representation>
  create_from_standard_format(Binlog_cache_representation &source,
                              Checksum_alg checksum_alg, const char *dir,
                              const char *prefix, my_off_t cache_size,
                              my_off_t max_cache_size,
                              my_off_t reserved_bytes) {
    assert(!source.is_large_trx_rewritten());

    /*
      Create and open the new cache: a named bolt_ temp file with the
      reserved header region at the front.
    */
    auto rewritten =
        std::make_unique<Checksummed_binlog_cache_representation>(checksum_alg);
    if (rewritten->open(dir, prefix, cache_size, max_cache_size,
                        reserved_bytes))
      return nullptr;
    rewritten->m_file.set_named_file();

    /*
      Create the converter. The copy below converts the source's bytes and
      writes them into m_file directly. After the copy, write() uses the same
      converter for the rest of the event in progress.
    */
    auto to_binlog_file_format = std::make_unique<Event_format_converter>(
        rewritten->m_file,  // downstream: where the converted bytes go
        Event_format_converter::Direction::kToBinlogFileFormat,  // direction
        rewritten->m_footer_len,               // 4 with CRC32, 0 with NONE
        static_cast<uint32>(reserved_bytes));  // log_pos counts from here

    /*
      Copy the source's bytes through the converter; the source is all in
      memory, so nothing is flushed. The copy leaves the source's IO_CACHE in
      read mode. Truncate the source to its own length, to put it back in
      write mode, also without flushing, so the source stays usable if the
      rewrite fails.
    */
    bool ostream_error = false;
    const bool copy_failed =
        source.copy_logical_to(*to_binlog_file_format, &ostream_error);
    if (source.truncate(source.logical_length()) || copy_failed) return nullptr;

    DBUG_EXECUTE_IF("fail_bolt_cache_conversion", { return nullptr; });

    rewritten->m_logical_length = source.logical_length();

    /*
      Keep the converter, with its parse state, for the rest of the event in
      progress (see write()). Even a source that ends at an event boundary is
      followed by an event in the standard form: the rewrite runs when the
      first write of an event would spill.
    */
    rewritten->m_in_progress_event_converter = std::move(to_binlog_file_format);

    return rewritten;
  }

  bool open(const char *dir, const char *prefix, my_off_t cache_size,
            my_off_t max_cache_size, my_off_t reserved_bytes) override {
    return m_file.open(dir, prefix, cache_size, max_cache_size, reserved_bytes);
  }

  void close() override { m_file.close(); }

  /*
    The caller (binlog_cache_data::write_event()) picks each event's format
    when the event starts, so every event that reaches this function is in
    binlog file format, except the one being written when the rewrite ran: it
    started in standard format, and its remaining bytes still arrive in
    standard format.
  */
  bool write(const unsigned char *buffer, my_off_t length) override {
    // No in-progress event being written: the incoming bytes are in binlog
    // file format already. Store as is.
    if (m_in_progress_event_converter == nullptr)
      return m_file.write(buffer, length);

    // The rest of the in-progress event, in standard format.
    if (m_in_progress_event_converter->write(buffer, length)) return true;
    // Standard-format bytes are logical bytes, so add them as they are.
    m_logical_length += length;

    // Reset m_in_progress_event_converter after the in-progress event is
    // done: later bytes are in binlog file format.
    if (m_in_progress_event_converter->at_event_boundary())
      m_in_progress_event_converter.reset();
    return false;
  }

  bool truncate(my_off_t logical_offset) override {
    m_in_progress_event_converter.reset();
    const std::optional<my_off_t> physical_offset =
        find_physical_offset(logical_offset);
    if (!physical_offset.has_value()) {
      // The scan left the cache in read mode: return it to write mode at its
      // end, unchanged.
      (void)m_file.truncate(m_file.length());
      return true;
    }
    if (m_file.truncate(*physical_offset)) return true;
    m_logical_length = logical_offset;
    return false;
  }

  /* The binlog cache does not seek: return an error. */
  bool seek(my_off_t offset [[maybe_unused]]) override { return true; }

  /* Nothing to do: flush_and_sync_spilled_file() flushes and syncs the file. */
  bool flush() override { return false; }

  /* Nothing to do: flush_and_sync_spilled_file() flushes and syncs the file. */
  bool sync() override { return false; }

  bool reset(Spilled_file_on_reset spilled_file_action) override {
    m_in_progress_event_converter.reset();
    m_logical_length = 0;
    return m_file.reset(spilled_file_action);
  }

  size_t disk_writes() const override { return m_file.disk_writes(); }

  my_off_t reserved_bytes() const override { return m_file.reserved_bytes(); }

  const char *tmp_file_name() const override { return m_file.tmp_file_name(); }

  bool is_spilled() const override { return m_file.is_spilled(); }

  File spilled_file() const override { return m_file.spilled_file(); }

  bool is_encrypted() const override { return m_file.is_encrypted(); }

  bool flush_and_sync_spilled_file() override {
    return m_file.flush_and_sync_spilled_file();
  }

  Checksum_alg physical_checksum_alg() const override { return m_checksum_alg; }

  void add_logical_length(my_off_t logical_event_length) override {
    m_logical_length += logical_event_length;
  }

  bool copy_logical_to(Basic_ostream &ostream, bool *ostream_error) override {
    Event_format_converter to_standard_format(
        ostream, Event_format_converter::Direction::kToStandardFormat,
        m_footer_len, 0);
    return stream_copy(&m_file, &to_standard_format, ostream_error);
  }

  bool copy_physical_to(Basic_ostream &ostream, bool *ostream_error) override {
    return stream_copy(&m_file, &ostream, ostream_error);
  }

  my_off_t logical_length() const override { return m_logical_length; }

  my_off_t physical_length() const override { return m_file.length(); }

  bool truncate_at_event_boundary(my_off_t logical_offset,
                                  size_t events_before_offset) override {
    m_in_progress_event_converter.reset();
    if (logical_offset > m_logical_length) return true;
    const my_off_t physical_offset =
        logical_offset +
        static_cast<my_off_t>(events_before_offset) * m_footer_len;
    if (physical_offset > m_file.length()) return true;
    if (m_file.truncate(physical_offset)) return true;
    m_logical_length = logical_offset;
    return false;
  }

  bool is_large_trx_rewritten() const override { return true; }
  bool write_would_spill(my_off_t length [[maybe_unused]]) const override {
    return false;
  }

 private:
  /**
    Find the physical offset of a logical offset by scanning the event
    headers from the start of the cache, counting events: each event has one
    footer, so the physical offset is logical_offset + events * footer. Used
    by truncate(), only as a fallback: savepoint and statement rollback use
    truncate_at_event_boundary(), which does not scan. Leaves the cache in
    read mode.

    @param logical_offset  The offset in the standard form.

    @return The physical offset, or std::nullopt if logical_offset is not an
            event boundary or the cache cannot be read.
  */
  std::optional<my_off_t> find_physical_offset(my_off_t logical_offset) {
    if (logical_offset > m_logical_length) return std::nullopt;
    if (logical_offset == m_logical_length) return m_file.length();

    my_off_t logical_position = 0;  // where the next event starts, logically
    my_off_t events = 0;            // events before logical_position
    std::array<uchar, LOG_EVENT_HEADER_LEN> header{};
    my_off_t header_len = 0;     // header bytes collected so far
    my_off_t bytes_to_skip = 0;  // the rest of the current event

    unsigned char *buffer = nullptr;
    my_off_t length = 0;
    bool error = m_file.begin(&buffer, &length);
    while (!error && length > 0 && logical_position < logical_offset) {
      my_off_t n = 0;
      if (bytes_to_skip > 0) {
        n = std::min(bytes_to_skip, length);
        bytes_to_skip -= n;
      } else {
        // Collect the header; it can be split across buffers.
        n = std::min(LOG_EVENT_HEADER_LEN - header_len, length);
        memcpy(header.data() + header_len, buffer, n);
        header_len += n;
        if (header_len == LOG_EVENT_HEADER_LEN) {
          const uint32 event_len = uint4korr(header.data() + EVENT_LEN_OFFSET);
          if (event_len < LOG_EVENT_HEADER_LEN + m_footer_len)
            return std::nullopt;
          logical_position += event_len - m_footer_len;
          ++events;
          bytes_to_skip = event_len - LOG_EVENT_HEADER_LEN;
          header_len = 0;
        }
      }
      buffer += n;
      length -= n;
      if (length == 0) error = m_file.next(&buffer, &length);
    }
    if (error || logical_position != logical_offset) return std::nullopt;
    return logical_offset + events * m_footer_len;
  }

  /**
    Converts events between the standard format and the binlog file format
    (not related to binlog_format sys var), rewriting each event's header,
    and adding or removing its footer, as the events pass through.

    - kToBinlogFileFormat (the rewrite): event_len + footer, log_pos =
      running end offset in the binlog file (after the reserved region),
      CRC32 footer appended when footer_len is not 0.
    - kToStandardFormat (the logical view): event_len - footer, log_pos = 0,
      footer skipped.
  */
  class Event_format_converter : public Basic_ostream {
   public:
    /** Which way the events are converted. */
    enum class Direction { kToBinlogFileFormat, kToStandardFormat };

    /**
      @param downstream     Where the converted bytes are written.
      @param direction      Which way the events are converted.
      @param footer_len     The footer length of the binlog file form:
                            BINLOG_CHECKSUM_LEN with CRC32, 0 with NONE.
      @param start_log_pos  To binlog file form: the file offset of the first
                            event. Unused to standard form.
    */
    Event_format_converter(Basic_ostream &downstream, Direction direction,
                           uint32 footer_len, uint32 start_log_pos)
        : m_downstream(downstream),
          m_direction(direction),
          m_footer_len(footer_len),
          m_adds_footer(direction == Direction::kToBinlogFileFormat &&
                        footer_len != 0),
          m_dropped_footer_len(
              direction == Direction::kToStandardFormat ? footer_len : 0),
          m_end_log_pos(start_log_pos) {}

    bool write(const unsigned char *buffer, my_off_t length) override {
      while (length > 0) {
        my_off_t n = 0;
        if (m_header_len < LOG_EVENT_HEADER_LEN) {
          // Collect the header (it can be split across writes), then rewrite
          // and forward it.
          n = std::min(LOG_EVENT_HEADER_LEN - m_header_len, length);
          memcpy(m_header.data() + m_header_len, buffer, n);
          m_header_len += n;
          if (m_header_len == LOG_EVENT_HEADER_LEN &&
              (rewrite_header() ||
               forward(m_header.data(), LOG_EVENT_HEADER_LEN)))
            return true;
        } else {
          // The rest of the event: forward it, except a footer being dropped.
          n = std::min(m_rest_of_event, length);
          const my_off_t body_left =
              m_rest_of_event > m_dropped_footer_len
                  ? m_rest_of_event - m_dropped_footer_len
                  : 0;
          const my_off_t body = std::min(n, body_left);
          if (body > 0 && forward(buffer, body)) return true;
          m_rest_of_event -= n;
        }
        buffer += n;
        length -= n;
        if (m_header_len == LOG_EVENT_HEADER_LEN && m_rest_of_event == 0 &&
            end_event())
          return true;
      }
      return false;
    }

    /** @return True between events. */
    bool at_event_boundary() const { return m_header_len == 0; }

   private:
    /**
      Rewrite event_len and log_pos in m_header, and set m_rest_of_event to
      the source bytes that follow the header (body, and a footer being
      dropped).

      @retval false  Success.
      @retval true   The event length is too short for its header and footer.
    */
    bool rewrite_header() {
      uchar *const header = m_header.data();
      const uint32 event_len = uint4korr(header + EVENT_LEN_OFFSET);
      if (event_len < LOG_EVENT_HEADER_LEN + m_dropped_footer_len) return true;
      m_rest_of_event = event_len - LOG_EVENT_HEADER_LEN;
      if (m_direction == Direction::kToStandardFormat) {
        int4store(header + EVENT_LEN_OFFSET, event_len - m_footer_len);
        int4store(header + LOG_POS_OFFSET, 0);
      } else {
        m_end_log_pos += event_len + m_footer_len;
        int4store(header + EVENT_LEN_OFFSET, event_len + m_footer_len);
        int4store(header + LOG_POS_OFFSET, m_end_log_pos);
      }
      return false;
    }

    /**
      Write bytes downstream, adding them to the CRC if a footer is added.

      @param bytes  The bytes.
      @param n      Their number.

      @retval false  Success.
      @retval true   Error writing downstream.
    */
    bool forward(const unsigned char *bytes, my_off_t n) {
      if (m_adds_footer)
        m_checksum = mysql::binlog::event::checksum_crc32(m_checksum, bytes, n);
      return m_downstream.write(bytes, n);
    }

    /**
      The event is complete: append its CRC32 footer if one is added.

      @retval false  Success.
      @retval true   Error writing downstream.
    */
    bool end_event() {
      m_header_len = 0;
      if (!m_adds_footer) return false;
      uchar footer[BINLOG_CHECKSUM_LEN];
      int4store(footer, m_checksum);
      m_checksum = mysql::binlog::event::checksum_crc32(0L, nullptr, 0);
      return m_downstream.write(footer, BINLOG_CHECKSUM_LEN);
    }

    Basic_ostream &m_downstream;
    const Direction m_direction;
    const uint32 m_footer_len;
    /** A CRC32 footer is computed and appended (to binlog file format). */
    const bool m_adds_footer;
    /** Footer bytes dropped from each source event (to standard format). */
    const my_off_t m_dropped_footer_len;
    /** To binlog file form: the end offset of the last converted event. */
    uint32 m_end_log_pos;
    /** The CRC32 of the current event so far, when a footer is added. */
    uint32 m_checksum = mysql::binlog::event::checksum_crc32(0L, nullptr, 0);
    /** The current event's header, collected across writes. */
    std::array<uchar, LOG_EVENT_HEADER_LEN> m_header{};
    /** Header bytes collected so far; 0 between events. */
    my_off_t m_header_len = 0;
    /** Source bytes of the current event still to come after its header. */
    my_off_t m_rest_of_event = 0;
  };

  /** The in-memory buffer and the named bolt_ temporary file. */
  IO_CACHE_binlog_cache_storage m_file;
  Checksum_alg m_checksum_alg;
  uint32 m_footer_len;
  /** The cache's length in the standard form. */
  my_off_t m_logical_length = 0;
  /** Converts the rest of the event in progress when the cache was rewritten;
      nullptr once it ends. */
  std::unique_ptr<Event_format_converter> m_in_progress_event_converter;
};

}  // namespace

bool is_checksum_alg_in_use(
    mysql::binlog::event::enum_binlog_checksum_alg checksum_alg) {
  return checksum_alg != mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF &&
         checksum_alg != mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
}

std::unique_ptr<Binlog_cache_representation>
create_checksumless_cache_representation(const char *dir, const char *prefix,
                                         my_off_t cache_size,
                                         my_off_t max_cache_size) {
  auto representation =
      std::make_unique<Checksumless_binlog_cache_representation>();
  if (representation->open(dir, prefix, cache_size, max_cache_size,
                           0 /*reserved_bytes*/))
    return nullptr;
  return representation;
}

std::unique_ptr<Binlog_cache_representation>
create_checksummed_cache_representation(
    Binlog_cache_representation &source,
    Binlog_cache_representation::Checksum_alg checksum_alg, const char *dir,
    const char *prefix, my_off_t cache_size, my_off_t max_cache_size,
    my_off_t reserved_bytes) {
  return Checksummed_binlog_cache_representation::create_from_standard_format(
      source, checksum_alg, dir, prefix, cache_size, max_cache_size,
      reserved_bytes);
}

}  // namespace binlog
