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

  These three subjects are covered by no MTR test, or by an MTR test that cannot
  reach the interesting values:

   - the reserved-region size quantization. Nothing in the MTR suite asserts the
     reservation or its rounding at all.
   - the reserved-region fit and padding arithmetic. The MTR test for the
     "region too small" fallback forces the outcome with a debug symbol rather
     than driving the predicate, because the region is sized from the serialized
     Previous_gtids estimate at spill time and no configuration makes it too
     small while leaving the transaction otherwise promotable.
   - is_bolt_temp_file(). MTR covers roughly seven literal names, each at the
     cost of a full server start, and cannot practically cover the boundaries.
     This predicate has already regressed once: its character class omitted
     A-Z while mkstemp() produces upper case, and a name it wrongly rejects is
     treated as an unsafe entry, which aborts startup.
*/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "my_inttypes.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/binlog/event/large_transaction_header_event.h"
#include "sql/binlog.h"                   // reserved-bytes helpers
#include "sql/binlog/large_trx_commit.h"  // fit and padding arithmetic
#include "sql/binlog_ostream.h"           // is_bolt_temp_file, constants

namespace mysql::binlog::event::unittests {

namespace {

/// The quantum the reservation is rounded up to.
constexpr my_off_t kQuantum = kBinlogTempFileReservedBytes;

/// The headroom always added on top of the Previous_gtids estimate.
constexpr my_off_t kHeadroom = kBinlogTempFilePreviousGtidsHeadroomBytes;

}  // namespace

//
// The reserved-region size: previous_gtids + headroom, rounded up to the
// quantum.
//

class BoltReservedBytesTest : public ::testing::Test {
 protected:
  /// Publish an estimate and read back the reservation it produces.
  static my_off_t reservation_for(my_off_t previous_gtids_size) {
    update_binlog_temp_file_previous_gtids_size_estimate(previous_gtids_size);
    return get_binlog_temp_file_reserved_bytes();
  }
};

TEST_F(BoltReservedBytesTest, RoundsUpToTheQuantum) {
  // An empty Previous_gtids still needs the headroom, which rounds to one
  // quantum.
  EXPECT_EQ(reservation_for(0), kQuantum);
  EXPECT_EQ(reservation_for(1), kQuantum);

  // Exactly one quantum once the headroom is added: the early return, with no
  // rounding applied.
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
  // number of quanta, and it can always hold the estimate plus the headroom.
  for (my_off_t estimate = 0; estimate <= 4 * kQuantum; estimate += 997) {
    const my_off_t reserved = reservation_for(estimate);
    EXPECT_EQ(reserved % kQuantum, my_off_t{0}) << "estimate " << estimate;
    EXPECT_GE(reserved, estimate + kHeadroom) << "estimate " << estimate;
    // Never more than one quantum of slack beyond what was required.
    EXPECT_LT(reserved, estimate + kHeadroom + kQuantum)
        << "estimate " << estimate;
  }
}

TEST_F(BoltReservedBytesTest, IsMonotonic) {
  my_off_t previous = 0;
  for (my_off_t estimate = 0; estimate <= 4 * kQuantum; estimate += 1024) {
    const my_off_t reserved = reservation_for(estimate);
    EXPECT_GE(reserved, previous) << "estimate " << estimate;
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

  /**
    The region is filled exactly when the prefix, the padded
    Large_transaction_header event and the Gtid event add up to its size. This
    is the invariant write_promoted_binlog_header() asserts after serializing
    them, expressed as arithmetic.
  */
  static my_off_t region_used(my_off_t reserved, my_off_t gtid_event_length,
                              my_off_t prefix_length, my_off_t checksum_len) {
    const my_off_t padding = large_trx_header_event_padding(
        reserved, gtid_event_length, prefix_length, checksum_len);
    const my_off_t lth_total =
        large_trx_header_event_min_length(checksum_len) + padding;
    return prefix_length + lth_total + gtid_event_length + checksum_len;
  }
};

TEST_F(BoltHeaderArithmeticTest, MinLengthAccountsForTheChecksum) {
  const my_off_t without = large_trx_header_event_min_length(0);
  const my_off_t with = large_trx_header_event_min_length(BINLOG_CHECKSUM_LEN);
  EXPECT_EQ(with, without + BINLOG_CHECKSUM_LEN);

  // A minimal event is its header plus the fixed body, and nothing else.
  EXPECT_EQ(without, static_cast<my_off_t>(
                         LOG_EVENT_HEADER_LEN +
                         Large_transaction_header_event::kFixedBodyLength));
}

TEST_F(BoltHeaderArithmeticTest, FitIsExactAtTheBoundary) {
  for (const my_off_t checksum_len :
       {my_off_t{0}, my_off_t{BINLOG_CHECKSUM_LEN}}) {
    const my_off_t prefix = 1024;
    // The smallest region that can hold everything the check budgets.
    const my_off_t needed = prefix +
                            large_trx_header_event_min_length(checksum_len) +
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
  const my_off_t needed = prefix +
                          large_trx_header_event_min_length(checksum_len) +
                          max_gtid_length() + checksum_len;

  ASSERT_TRUE(large_trx_header_events_fit(prefix, checksum_len, needed));
  EXPECT_EQ(large_trx_header_event_padding(needed, max_gtid_length(), prefix,
                                           checksum_len),
            my_off_t{0});

  // A shorter Gtid event in the same region leaves exactly the difference as
  // padding.
  const my_off_t shorter = max_gtid_length() - 40;
  EXPECT_EQ(
      large_trx_header_event_padding(needed, shorter, prefix, checksum_len),
      my_off_t{40});
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
  // mkstemp() draws from [A-Za-z0-9], so upper case must be accepted. This is
  // the case the original character class got wrong.
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("aB3xY9").c_str()));
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("ZZZZZZ").c_str()));
  EXPECT_TRUE(is_bolt_temp_file(with_prefix("0123456789abcdef").c_str()));

  // The promoted-name form is <hex start time>_<hex serial>, so an embedded
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

}  // namespace mysql::binlog::event::unittests
