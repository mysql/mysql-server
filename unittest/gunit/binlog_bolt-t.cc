/* Copyright (c) 2026, Oracle and/or its affiliates.

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

/*
  Unit tests for the pure logic of the binlog large transaction optimization
  (BOLT). Everything here is arithmetic or a predicate: no server, no THD, no
  locks and no files.

  These four subjects are covered by no MTR test, or by an MTR test that cannot
  reach the interesting values:

   - the reserved-region size quantization. Nothing in the MTR suite asserts the
     reservation or its rounding at all.
   - the reserved-region fit and padding arithmetic. The MTR test for the
     "region too small" fallback forces the outcome with a debug symbol rather
     than driving the predicate, because the region is sized from the active
     binary log header at the first event and no configuration makes it too
     small while leaving the transaction otherwise promotable.
   - is_bolt_temp_file(). Each name that MTR covers costs a full server
     start, so MTR cannot cover the boundaries. A name that
     is_bolt_temp_file() wrongly rejects is left in the directory at startup.
   - how find_large_trx_terminating_event() classifies a header: a header
     that matches the event at its offset, no XID to verify, a header that
     does not match the file, and a failing reader. MTR cannot make a seek or
     a read fail with an I/O error. A fake reader stands in for the binary log
     file.
*/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "my_byteorder.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/binlog/event/large_transaction_header_event.h"
#include "sql/binlog.h"                   // reserved-bytes helpers
#include "sql/binlog/large_trx_commit.h"  // fit and padding arithmetic
#include "sql/binlog_ostream.h"           // is_bolt_temp_file, constants
#include "sql/binlog_reader.h"            // IBasic_binlog_file_reader

namespace mysql::binlog::event::unittests {

namespace {

/// The quantum the reservation is rounded up to.
constexpr my_off_t kQuantum = kBinlogTempFileReservedBytes;

/// The headroom always added on top of the binary log header length.
constexpr my_off_t kHeadroom = kBinlogTempFileHeaderHeadroomBytes;

}  // namespace

//
// The reserved-region size: header length + headroom, rounded up to the
// quantum.
//

class BoltReservedBytesTest : public ::testing::Test {
 protected:
  /// Set a header length and read back the reservation it produces.
  static my_off_t reservation_for(my_off_t header_length) {
    set_binlog_cache_reserved_bytes(header_length);
    return get_binlog_cache_reserved_bytes();
  }

  /// Leave the reservation of an empty header for the other tests.
  void TearDown() override { set_binlog_cache_reserved_bytes(0); }
};

TEST_F(BoltReservedBytesTest, RoundsUpToTheQuantum) {
  // An empty header still needs the headroom, which rounds to one
  // quantum.
  EXPECT_EQ(reservation_for(0), kQuantum);
  EXPECT_EQ(reservation_for(1), kQuantum);

  // Exactly one quantum once the headroom is added.
  ASSERT_LT(kHeadroom, kQuantum) << "the headroom must be inside one quantum";
  EXPECT_EQ(reservation_for(kQuantum - kHeadroom), kQuantum);

  // One byte past it must cost a whole further quantum.
  EXPECT_EQ(reservation_for(kQuantum - kHeadroom + 1), 2 * kQuantum);

  // And the same shape one quantum further out.
  EXPECT_EQ(reservation_for(2 * kQuantum - kHeadroom), 2 * kQuantum);
  EXPECT_EQ(reservation_for(2 * kQuantum - kHeadroom + 1), 3 * kQuantum);
}

TEST_F(BoltReservedBytesTest, IsAlwaysAQuantumMultipleAndLeavesHeadroom) {
  // The two properties the callers depend on, over a range that crosses
  // several quantum boundaries: the spill file's reserved region is a whole
  // number of quanta, and it can always hold the header plus the headroom.
  for (my_off_t length = 0; length <= 4 * kQuantum; length += 997) {
    const my_off_t reserved = reservation_for(length);
    EXPECT_EQ(reserved % kQuantum, my_off_t{0}) << "header length " << length;
    EXPECT_GE(reserved, length + kHeadroom) << "header length " << length;
    // Never more than one quantum of slack beyond what was required.
    EXPECT_LT(reserved, length + kHeadroom + kQuantum)
        << "header length " << length;
  }
}

TEST_F(BoltReservedBytesTest, IsMonotonic) {
  my_off_t previous = 0;
  for (my_off_t length = 0; length <= 4 * kQuantum; length += 1024) {
    const my_off_t reserved = reservation_for(length);
    EXPECT_GE(reserved, previous) << "header length " << length;
    previous = reserved;
  }
}

//
// The reserved-region fit and padding arithmetic.
//

class BoltHeaderArithmeticTest : public ::testing::Test {
 protected:
  /// The largest a Gtid event can be, which is what the fit check budgets.
  static my_off_t max_gtid_length() {
    return static_cast<my_off_t>(Gtid_event::get_max_event_length());
  }

  /// The longest terminating event body, which is what the fit check budgets.
  static constexpr my_off_t kMaxBody =
      Large_transaction_header_event::kMaxTerminatingEventBodyLength;

  /**
    The region is filled exactly when the prefix, the padded
    Large_transaction_header event and the Gtid event add up to its size. This
    is the invariant promote_spilled_file() asserts after serializing
    them, expressed as arithmetic.
  */
  static my_off_t region_used(my_off_t reserved, my_off_t gtid_event_length,
                              my_off_t prefix_length, my_off_t checksum_len) {
    const my_off_t padding = large_trx_header_event_padding(
        reserved, gtid_event_length, prefix_length, checksum_len, kMaxBody);
    const my_off_t lth_total =
        large_trx_header_event_min_length(checksum_len, kMaxBody) + padding;
    return prefix_length + lth_total + gtid_event_length + checksum_len;
  }
};

TEST_F(BoltHeaderArithmeticTest, MinLengthAccountsForTheChecksum) {
  const my_off_t without = large_trx_header_event_min_length(0, 8);
  const my_off_t with =
      large_trx_header_event_min_length(BINLOG_CHECKSUM_LEN, 8);
  EXPECT_EQ(with, without + BINLOG_CHECKSUM_LEN);

  // A minimal event is its header, the fixed body and the terminating event's
  // body, and nothing else.
  EXPECT_EQ(without, static_cast<my_off_t>(
                         LOG_EVENT_HEADER_LEN +
                         Large_transaction_header_event::kFixedBodyLength + 8));
}

TEST_F(BoltHeaderArithmeticTest, FitIsExactAtTheBoundary) {
  for (const my_off_t checksum_len :
       {my_off_t{0}, my_off_t{BINLOG_CHECKSUM_LEN}}) {
    const my_off_t prefix = 1024;
    // The smallest region that can hold everything the check budgets.
    const my_off_t needed =
        prefix + large_trx_header_event_min_length(checksum_len, kMaxBody) +
        max_gtid_length() + checksum_len;

    EXPECT_TRUE(large_trx_header_events_fit(prefix, checksum_len, needed))
        << "checksum_len " << checksum_len;
    // One byte short must not fit. This is the boundary MTR cannot reach.
    EXPECT_FALSE(large_trx_header_events_fit(prefix, checksum_len, needed - 1))
        << "checksum_len " << checksum_len;
    // One byte spare must still fit.
    EXPECT_TRUE(large_trx_header_events_fit(prefix, checksum_len, needed + 1))
        << "checksum_len " << checksum_len;
  }
}

TEST_F(BoltHeaderArithmeticTest, PaddingFillsTheRegionExactly) {
  // Whatever the actual Gtid event turns out to be, the padding must make the
  // header events end precisely at the reserved offset, because that offset is
  // where the transaction's first event was written at spill time.
  const my_off_t checksum_len = BINLOG_CHECKSUM_LEN;
  const my_off_t prefix = 1024;
  const my_off_t reserved = 4 * kQuantum;

  ASSERT_TRUE(large_trx_header_events_fit(prefix, checksum_len, reserved));

  for (my_off_t gtid_length = 1; gtid_length <= max_gtid_length();
       gtid_length += 17) {
    EXPECT_EQ(region_used(reserved, gtid_length, prefix, checksum_len),
              reserved)
        << "gtid_length " << gtid_length;
  }
  // And at the largest Gtid event the fit check budgets for.
  EXPECT_EQ(region_used(reserved, max_gtid_length(), prefix, checksum_len),
            reserved);
}

TEST_F(BoltHeaderArithmeticTest, PaddingDoesNotUnderflowAtTheTightestFit) {
  // The fit check budgets Gtid_event::get_max_event_length() while the padding
  // subtracts the event's actual length, so the two disagree by construction.
  // At the tightest region that still fits, and with the largest possible Gtid
  // event, the padding must come out at exactly zero rather than wrapping
  // around: my_off_t is unsigned, so an underflow here would become an enormous
  // padding size rather than a negative one.
  const my_off_t checksum_len = BINLOG_CHECKSUM_LEN;
  const my_off_t prefix = 1024;
  const my_off_t needed =
      prefix + large_trx_header_event_min_length(checksum_len, kMaxBody) +
      max_gtid_length() + checksum_len;

  ASSERT_TRUE(large_trx_header_events_fit(prefix, checksum_len, needed));
  EXPECT_EQ(large_trx_header_event_padding(needed, max_gtid_length(), prefix,
                                           checksum_len, kMaxBody),
            my_off_t{0});

  // A shorter Gtid event in the same region leaves exactly the difference as
  // padding.
  const my_off_t shorter = max_gtid_length() - 40;
  EXPECT_EQ(large_trx_header_event_padding(needed, shorter, prefix,
                                           checksum_len, kMaxBody),
            my_off_t{40});

  // A shorter terminating event body (an Xid event's 8 bytes) leaves the
  // difference as padding too.
  EXPECT_EQ(large_trx_header_event_padding(needed, max_gtid_length(), prefix,
                                           checksum_len, 8),
            kMaxBody - 8);
}

//
// The spill-file name predicate.
//

class BoltTempFileNameTest : public ::testing::Test {
 protected:
  static std::string with_prefix(const std::string &suffix) {
    return std::string{kBinlogTempFilePrefix} + suffix;
  }
};

TEST_F(BoltTempFileNameTest, AcceptsTheNamesTheCacheCreates) {
  // The alphabet is [A-Za-z0-9_].
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("aB3xY9").c_str()));
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("ZZZZZZ").c_str()));
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("0123456789abcdef").c_str()));

  // The cache creates <hex start time>_<hex serial>, so an embedded
  // underscore is part of the alphabet.
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("18f2c4a1b_0").c_str()));
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("_").c_str()));

  // A single character is enough of a suffix.
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("a").c_str()));
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("0").c_str()));
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("Z").c_str()));
}

TEST_F(BoltTempFileNameTest, RejectsThePrefixWithNoSuffix) {
  // The bare prefix is not a name the cache ever creates, and deleting it
  // would mean deleting something the binary log does not own.
  EXPECT_FALSE(is_bolt_temp_file(kBinlogTempFilePrefix));
}

TEST_F(BoltTempFileNameTest, RejectsNamesOutsideTheAlphabet) {
  EXPECT_FALSE(is_bolt_temp_file(with_prefix("a-b").c_str()));
  EXPECT_FALSE(is_bolt_temp_file(with_prefix("a.b").c_str()));
  EXPECT_FALSE(is_bolt_temp_file(with_prefix("a b").c_str()));
  EXPECT_FALSE(is_bolt_temp_file(with_prefix("a/b").c_str()));
  EXPECT_FALSE(is_bolt_temp_file(with_prefix("a\tb").c_str()));
  // A rejected character anywhere in the suffix is enough, including last.
  EXPECT_FALSE(is_bolt_temp_file(with_prefix("abc!").c_str()));
}

TEST_F(BoltTempFileNameTest, RejectsNamesThatDoNotCarryThePrefix) {
  EXPECT_FALSE(is_bolt_temp_file(""));
  EXPECT_FALSE(is_bolt_temp_file("bolt"));
  EXPECT_FALSE(is_bolt_temp_file("boltx"));
  EXPECT_FALSE(is_bolt_temp_file("xbolt_a"));
  EXPECT_FALSE(is_bolt_temp_file("not_a_managed_bolt_file"));
  EXPECT_FALSE(is_bolt_temp_file("binlog.000001"));
  EXPECT_FALSE(is_bolt_temp_file("binlog.index"));

  // The prefix is matched case sensitively.
  EXPECT_FALSE(is_bolt_temp_file("BOLT_abc"));
  EXPECT_FALSE(is_bolt_temp_file("Bolt_abc"));
}

//
// find_large_trx_terminating_event(): the results other than kVerified.
//

/**
  A binary log file reader with no file. seek() and read_event_object() fail
  as the test asks, and every other function must not be called.
*/
class Fake_binlog_file_reader : public IBasic_binlog_file_reader {
 public:
  my_off_t m_position{1000};
  bool m_fail_seek_to_offset{false};
  bool m_fail_seek_back{false};
  Binlog_read_error::Error_type m_read_error{Binlog_read_error::TRUNC_EVENT};
  int m_seeks{0};
  int m_reads{0};

  bool open(const char *, my_off_t, Format_description_log_event **) override {
    ADD_FAILURE();
    return true;
  }
  void close() override { ADD_FAILURE(); }
  bool has_fatal_error() const override {
    return m_read_error > Binlog_read_error::READ_EOF;
  }
  Binlog_read_error::Error_type get_error_type() const override {
    return m_read_error;
  }
  const char *get_error_str() const override { return "fake"; }
  bool is_checksum_verification_enabled() const override { return false; }
  my_off_t position() const override { return m_position; }
  bool seek(my_off_t pos) override {
    ++m_seeks;
    if ((m_fail_seek_to_offset && m_seeks == 1) ||
        (m_fail_seek_back && m_seeks == 2))
      return true;
    m_position = pos;
    return false;
  }
  Log_event *read_event_object() override {
    ++m_reads;
    if (m_event.empty()) return nullptr;
    /* The decoded event owns its buffer, so decode a copy. */
    auto *data = static_cast<unsigned char *>(
        my_malloc(PSI_NOT_INSTRUMENTED, m_event.size(), MYF(0)));
    memcpy(data, m_event.data(), m_event.size());
    Log_event *event = nullptr;
    if (binlog_event_deserialize(data, m_event.size(), &m_fde, false, &event) !=
        Binlog_read_error::SUCCESS) {
      my_free(data);
      return nullptr;
    }
    event->register_temp_buf(reinterpret_cast<char *>(data));
    m_position += m_event.size();
    return event;
  }

  /**
    Make read_event_object() return an Xid event with the given XID and no
    checksum. When extra_body_bytes is not 0, the event's body is longer than
    an Xid event's body by that many bytes.
  */
  void set_xid_event(uint64_t xid, size_t extra_body_bytes = 0) {
    m_event.assign(LOG_EVENT_HEADER_LEN + sizeof(xid) + extra_body_bytes, 0);
    unsigned char *header = m_event.data();
    header[EVENT_TYPE_OFFSET] = XID_EVENT;
    int4store(header + SERVER_ID_OFFSET, 1);
    int4store(header + EVENT_LEN_OFFSET, static_cast<uint32>(m_event.size()));
    int8store(header + LOG_EVENT_HEADER_LEN, xid);
  }
  const Format_description_event &format_description_event() const override {
    return m_fde;
  }
  std::optional<Event_metadata> read_event_metadata() override {
    ADD_FAILURE();
    return {};
  }
  Log_event *read_event_payload(const Event_metadata &) override {
    ADD_FAILURE();
    return nullptr;
  }
  std::optional<Event_payload> read_payload(const Event_metadata &) override {
    ADD_FAILURE();
    return {};
  }
  bool skip_event_payload(const Event_metadata &) override {
    ADD_FAILURE();
    return true;
  }

  Fake_binlog_file_reader() {
    m_fde.footer()->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;
  }

 private:
  Format_description_event m_fde{BINLOG_VERSION, "9.7.0"};
  /** The serialized event that read_event_object() returns, if any. */
  std::vector<unsigned char> m_event;
};

class BoltHeaderCheckTest : public ::testing::Test {
 protected:
  static constexpr my_off_t kFileSize = 100000;

  /// Check a header that records the given terminating event.
  static Large_trx_header_check check(Fake_binlog_file_reader &reader,
                                      uint64_t offset, uint8_t type,
                                      const std::string &body) {
    const Large_transaction_header_event lth(offset, type, body, 0);
    my_off_t end = 0;
    const Large_trx_header_check result =
        find_large_trx_terminating_event(reader, kFileSize, lth, &end);
    EXPECT_EQ(end, 0U);
    return result;
  }

  /// The body that a header records for an Xid event with the given XID.
  static std::string xid_body(uint64_t xid) {
    char body[sizeof(xid)];
    int8store(body, xid);
    return std::string(body, sizeof(body));
  }
};

TEST_F(BoltHeaderCheckTest, AHeaderThatMatchesTheEventIsVerified) {
  Fake_binlog_file_reader reader;
  reader.set_xid_event(42);
  const Large_transaction_header_event lth(5000, XID_EVENT, xid_body(42), 0);
  my_off_t end = 0;
  EXPECT_EQ(find_large_trx_terminating_event(reader, kFileSize, lth, &end),
            Large_trx_header_check::kVerified);
  EXPECT_EQ(end, 5000U + LOG_EVENT_HEADER_LEN + sizeof(uint64_t));
  /* The reader is back at its position. */
  EXPECT_EQ(reader.m_position, 1000U);
}

TEST_F(BoltHeaderCheckTest, AnEventThatDoesNotMatchTheHeaderIsInvalid) {
  {
    /* Another transaction's Xid event at the recorded offset. */
    Fake_binlog_file_reader reader;
    reader.set_xid_event(43);
    EXPECT_EQ(check(reader, 5000, XID_EVENT, xid_body(42)),
              Large_trx_header_check::kInvalid);
    EXPECT_EQ(reader.m_position, 1000U);
  }
  {
    /* The header records another event type. */
    Fake_binlog_file_reader reader;
    reader.set_xid_event(42);
    EXPECT_EQ(check(reader, 5000, XA_PREPARE_LOG_EVENT, xid_body(42)),
              Large_trx_header_check::kInvalid);
  }
  {
    /* The event is longer than the header records. */
    Fake_binlog_file_reader reader;
    reader.set_xid_event(42, 4);
    EXPECT_EQ(check(reader, 5000, XID_EVENT, xid_body(42)),
              Large_trx_header_check::kInvalid);
  }
}

TEST_F(BoltHeaderCheckTest, ACommitQueryTransactionIsNotApplicable) {
  /* A transaction that is not two-phase committed ends with a COMMIT query,
     and its header records no XID. The file is not read. */
  Fake_binlog_file_reader reader;
  EXPECT_EQ(check(reader, 5000, QUERY_EVENT, ""),
            Large_trx_header_check::kNotApplicable);
  EXPECT_EQ(reader.m_seeks, 0);
  EXPECT_EQ(reader.m_reads, 0);
  EXPECT_EQ(reader.m_position, 1000U);
}

TEST_F(BoltHeaderCheckTest, AHeaderThatDoesNotDescribeTheFileIsInvalid) {
  Fake_binlog_file_reader reader;
  /* An Xid event with no body to verify against. */
  EXPECT_EQ(check(reader, 5000, XID_EVENT, ""),
            Large_trx_header_check::kInvalid);
  /* An offset that is not ahead of the reader, or not inside the file. */
  EXPECT_EQ(check(reader, 1000, XID_EVENT, "12345678"),
            Large_trx_header_check::kInvalid);
  EXPECT_EQ(check(reader, kFileSize, XID_EVENT, "12345678"),
            Large_trx_header_check::kInvalid);
  EXPECT_EQ(check(reader, kFileSize - 1, XID_EVENT, "12345678"),
            Large_trx_header_check::kInvalid);
  EXPECT_EQ(reader.m_seeks, 0);
}

TEST_F(BoltHeaderCheckTest, AnEventThatCannotBeReadIsInvalid) {
  for (const auto error :
       {Binlog_read_error::TRUNC_EVENT, Binlog_read_error::READ_EOF,
        Binlog_read_error::BOGUS, Binlog_read_error::EVENT_TOO_LARGE,
        Binlog_read_error::CHECKSUM_FAILURE, Binlog_read_error::SYSTEM_IO,
        Binlog_read_error::MEM_ALLOCATE}) {
    Fake_binlog_file_reader reader;
    reader.m_read_error = error;
    EXPECT_EQ(check(reader, 5000, XID_EVENT, "12345678"),
              Large_trx_header_check::kInvalid)
        << "read error " << error;
    /* The reader is back at its position. */
    EXPECT_EQ(reader.m_position, 1000U);
  }
}

TEST_F(BoltHeaderCheckTest, AFailedSeekToTheEventIsInvalid) {
  Fake_binlog_file_reader reader;
  reader.m_fail_seek_to_offset = true;
  EXPECT_EQ(check(reader, 5000, XID_EVENT, "12345678"),
            Large_trx_header_check::kInvalid);
  EXPECT_EQ(reader.m_reads, 0);
  /* The reader is back at its position. */
  EXPECT_EQ(reader.m_position, 1000U);
}

TEST_F(BoltHeaderCheckTest, AFailedSeekBackIsAReadError) {
  Fake_binlog_file_reader reader;
  reader.m_fail_seek_back = true;
  EXPECT_EQ(check(reader, 5000, XA_PREPARE_LOG_EVENT, "12345678"),
            Large_trx_header_check::kReadError);
}

}  // namespace mysql::binlog::event::unittests
