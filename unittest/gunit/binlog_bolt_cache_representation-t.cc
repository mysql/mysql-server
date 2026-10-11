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
  Unit tests for the binlog cache representations of the checksumless cache
  (sql/binlog/cache_representation.{h,cc}):

   - conversion of a checksum-free (logical) cache into the checksummed
     promotable form: event lengths, log_pos and CRC32 footers of the
     physical stream;
   - the logical view of a converted cache (copy_logical_to) is byte-identical
     to the stream the checksum-free cache held;
   - logical lengths and truncation, through both the scanning path
     (truncate) and the O(1) event-boundary path
     (truncate_at_event_boundary);
   - event headers and bodies split across IO_CACHE buffers, empty-body
     events, and rejection of offsets that are not event boundaries;
   - the first-spill trigger and the rewrite in the middle of an event.

  The representations are driven directly, rather than through
  Binlog_cache_storage, so the tests do not depend on the process-wide
  #binlog_temp_files directory.
*/

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#include "my_byteorder.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/binlog/event/binlog_event.h"
#include "sql/basic_ostream.h"
#include "sql/binlog/cache_representation.h"
#include "sql/log_event.h"  // LOG_EVENT_HEADER_LEN, EVENT_LEN_OFFSET

namespace binlog::unittests {

using mysql::binlog::event::BINLOG_CHECKSUM_ALG_CRC32;
using mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;

namespace {

constexpr my_off_t kReserved = 64 * 1024;
// Small in-memory buffer, so the caches spill and reads are split.
constexpr my_off_t kCacheSize = 4096;
constexpr my_off_t kMaxCacheSize = 1ULL << 40;

class String_ostream : public Basic_ostream {
 public:
  bool write(const unsigned char *buffer, my_off_t length) override {
    data.append(reinterpret_cast<const char *>(buffer), length);
    return false;
  }
  std::string data;
};

/*
  One standard-form event with the given body size. log_pos is end_pos; the
  standard form uses 0.
*/
std::string make_event(uint32 body_size, uint32 end_pos, unsigned seed) {
  std::string ev(LOG_EVENT_HEADER_LEN + body_size, '\0');
  auto *p = reinterpret_cast<uchar *>(ev.data());
  int4store(p, 1700000000);  // timestamp
  p[4] = 0x1e;               // type code (WRITE_ROWS_EVENT)
  int4store(p + 5, 1);       // server_id
  int4store(p + EVENT_LEN_OFFSET, LOG_EVENT_HEADER_LEN + body_size);
  int4store(p + LOG_POS_OFFSET, end_pos);
  int2store(p + 17, 0);  // flags
  std::mt19937 rng(seed);
  for (uint32 i = 0; i < body_size; ++i)
    p[LOG_EVENT_HEADER_LEN + i] = static_cast<uchar>(rng());
  return ev;
}

struct Logical_stream {
  std::string bytes;
  std::vector<my_off_t> boundaries;  // logical offset after each event
};

Logical_stream make_stream(const std::vector<uint32> &body_sizes) {
  Logical_stream s;
  my_off_t pos = 0;
  unsigned seed = 1;
  for (uint32 body : body_sizes) {
    pos += LOG_EVENT_HEADER_LEN + body;
    s.bytes += make_event(body, 0 /*standard form*/, seed++);
    s.boundaries.push_back(pos);
  }
  return s;
}

class BinlogCacheRepresentationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // A directory of its own, unique among concurrent test processes.
    std::random_device random;
    std::error_code error;
    std::filesystem::path dir;
    do {
      dir = std::filesystem::temp_directory_path(error) /
            ("bolt_cache_rep_" + std::to_string(random()));
      ASSERT_FALSE(error);
    } while (!std::filesystem::create_directory(dir, error) && !error);
    ASSERT_FALSE(error);
    m_dir = dir.string();
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(m_dir, error);
  }

  std::unique_ptr<Binlog_cache_representation> create_checksumless() {
    auto rep = create_checksumless_cache_representation(
        m_dir.c_str(), "bolt_test_", kCacheSize, kMaxCacheSize);
    EXPECT_NE(rep, nullptr);
    return rep;
  }

  /* Write the stream in small, odd-sized chunks. */
  static void write_chunked(Binlog_cache_representation &rep,
                            const std::string &bytes, size_t chunk) {
    for (size_t off = 0; off < bytes.size(); off += chunk) {
      const size_t n = std::min(chunk, bytes.size() - off);
      ASSERT_FALSE(rep.write(
          reinterpret_cast<const unsigned char *>(bytes.data() + off), n));
    }
  }

  std::unique_ptr<Binlog_cache_representation> convert(
      Binlog_cache_representation &source,
      Binlog_cache_representation::Checksum_alg alg =
          BINLOG_CHECKSUM_ALG_CRC32) {
    auto rewritten = create_checksummed_cache_representation(
        source, alg, m_dir.c_str(), "bolt_test_", kCacheSize, kMaxCacheSize,
        kReserved);
    EXPECT_NE(rewritten, nullptr);
    return rewritten;
  }

  static std::string logical_of(Binlog_cache_representation &rep) {
    String_ostream out;
    bool ostream_error = false;
    EXPECT_FALSE(rep.copy_logical_to(out, &ostream_error));
    return out.data;
  }

  static std::string physical_of(Binlog_cache_representation &rep) {
    String_ostream out;
    bool ostream_error = false;
    EXPECT_FALSE(rep.copy_physical_to(out, &ostream_error));
    return out.data;
  }

  /*
    Check a physical stream: each event is the logical event with
    event_len + 4, log_pos = reserved + physical end, and a valid CRC32 over
    the rewritten header and the body.
  */
  static void expect_physical_matches(const std::string &physical,
                                      const std::string &logical,
                                      bool with_crc = true) {
    const uint32 footer = with_crc ? BINLOG_CHECKSUM_LEN : 0;
    my_off_t lpos = 0, ppos = 0;
    while (lpos < logical.size()) {
      ASSERT_LT(ppos, physical.size());
      const auto *l = reinterpret_cast<const uchar *>(logical.data() + lpos);
      const auto *p = reinterpret_cast<const uchar *>(physical.data() + ppos);
      const uint32 llen = uint4korr(l + EVENT_LEN_OFFSET);
      const uint32 plen = uint4korr(p + EVENT_LEN_OFFSET);
      ASSERT_EQ(plen, llen + footer);
      ASSERT_EQ(uint4korr(p + LOG_POS_OFFSET), kReserved + ppos + plen);
      // Everything but the length and position fields is unchanged.
      ASSERT_EQ(0, memcmp(l, p, EVENT_LEN_OFFSET));
      ASSERT_EQ(0, memcmp(l + LOG_POS_OFFSET + 4, p + LOG_POS_OFFSET + 4,
                          llen - LOG_POS_OFFSET - 4));
      if (with_crc) {
        const uint32 crc = mysql::binlog::event::checksum_crc32(0L, nullptr, 0);
        ASSERT_EQ(uint4korr(p + llen),
                  mysql::binlog::event::checksum_crc32(crc, p, llen));
      }
      lpos += llen;
      ppos += plen;
    }
    ASSERT_EQ(ppos, physical.size());
  }

  std::string m_dir;
};

const std::vector<uint32> kBodySizes = {0,    1, 7, 4096, 13, 9000,  0, 250,
                                        4077, 5, 1, 3000, 0,  70000, 2};

TEST_F(BinlogCacheRepresentationTest, ChecksumlessIsIdentity) {
  const Logical_stream s = make_stream(kBodySizes);
  auto rep = create_checksumless();
  write_chunked(*rep, s.bytes, 333);
  EXPECT_TRUE(rep->is_spilled());
  EXPECT_FALSE(is_checksum_alg_in_use(rep->physical_checksum_alg()));
  EXPECT_EQ(rep->logical_length(), s.bytes.size());
  EXPECT_EQ(rep->physical_length(), s.bytes.size());
  EXPECT_EQ(logical_of(*rep), s.bytes);
  EXPECT_EQ(physical_of(*rep), s.bytes);
}

TEST_F(BinlogCacheRepresentationTest, ConversionProducesValidPhysicalStream) {
  const Logical_stream s = make_stream(kBodySizes);
  auto source = create_checksumless();
  write_chunked(*source, s.bytes, 1000);
  auto converted = convert(*source);
  source.reset();

  EXPECT_EQ(converted->physical_checksum_alg(), BINLOG_CHECKSUM_ALG_CRC32);
  EXPECT_EQ(converted->logical_length(), s.bytes.size());
  EXPECT_EQ(converted->physical_length(),
            s.bytes.size() + kBodySizes.size() * BINLOG_CHECKSUM_LEN);
  expect_physical_matches(physical_of(*converted), s.bytes);
  // The logical view of the converted cache is the original stream.
  EXPECT_EQ(logical_of(*converted), s.bytes);
}

// The rewrite reads the source and leaves it as it was.
TEST_F(BinlogCacheRepresentationTest, RewriteLeavesSourceUnchanged) {
  const Logical_stream s = make_stream({10, 20, 4000, 30});
  auto source = create_checksumless();
  write_chunked(*source, s.bytes, 7);
  auto converted = convert(*source);
  EXPECT_FALSE(source->is_large_trx_rewritten());
  EXPECT_EQ(source->logical_length(), s.bytes.size());
  EXPECT_EQ(logical_of(*source), s.bytes);
  EXPECT_TRUE(converted->is_large_trx_rewritten());
}

TEST_F(BinlogCacheRepresentationTest, TruncateMapsLogicalOffsets) {
  const Logical_stream s = make_stream(kBodySizes);
  for (size_t k : {size_t{0}, size_t{1}, size_t{4}, size_t{9},
                   kBodySizes.size() - 1, kBodySizes.size()}) {
    for (bool o1 : {false, true}) {
      auto source = create_checksumless();
      write_chunked(*source, s.bytes, 4096);
      auto converted = convert(*source);
      source.reset();

      const my_off_t logical = (k == 0) ? 0 : s.boundaries[k - 1];
      if (o1)
        ASSERT_FALSE(converted->truncate_at_event_boundary(logical, k));
      else
        ASSERT_FALSE(converted->truncate(logical));
      EXPECT_EQ(converted->logical_length(), logical) << k << o1;
      EXPECT_EQ(converted->physical_length(), logical + k * BINLOG_CHECKSUM_LEN)
          << k << o1;
      EXPECT_EQ(logical_of(*converted), s.bytes.substr(0, logical)) << k << o1;
    }
  }
}

TEST_F(BinlogCacheRepresentationTest, TruncateRejectsNonBoundaryOffsets) {
  const Logical_stream s = make_stream(kBodySizes);
  auto source = create_checksumless();
  write_chunked(*source, s.bytes, 4096);
  auto converted = convert(*source);
  // Inside the 4th event, and beyond the end.
  EXPECT_TRUE(converted->truncate(s.boundaries[3] - 1));
  EXPECT_TRUE(converted->truncate(s.bytes.size() + 1));
  EXPECT_TRUE(converted->truncate_at_event_boundary(s.bytes.size() + 1,
                                                    kBodySizes.size()));
}

TEST_F(BinlogCacheRepresentationTest, AppendAfterConversionAndTruncate) {
  // Events written after conversion carry a checksum and are recorded with
  // their logical length, as binlog_cache_data::write_event() does.
  const Logical_stream s = make_stream(kBodySizes);
  auto source = create_checksumless();
  write_chunked(*source, s.bytes, 4096);
  auto converted = convert(*source);
  source.reset();

  ASSERT_FALSE(converted->truncate_at_event_boundary(s.boundaries[5], 6));

  // Re-append the tail of the stream in physical form.
  const std::string tail = s.bytes.substr(s.boundaries[5]);
  String_ostream physical_tail;
  {
    // Convert the tail alone through a fresh checksumless cache; its log_pos
    // values restart at kReserved, so fix them up afterwards.
    auto tmp = create_checksumless();
    write_chunked(*tmp, tail, 4096);
    auto tmp_conv = convert(*tmp);
    physical_tail.data = physical_of(*tmp_conv);
  }
  my_off_t pend = converted->physical_length();
  for (my_off_t off = 0; off < physical_tail.data.size();) {
    auto *p = reinterpret_cast<uchar *>(physical_tail.data.data() + off);
    const uint32 plen = uint4korr(p + EVENT_LEN_OFFSET);
    pend += plen;
    int4store(p + LOG_POS_OFFSET, static_cast<uint32>(kReserved + pend));
    const uint32 crc = mysql::binlog::event::checksum_crc32(0L, nullptr, 0);
    int4store(p + plen - BINLOG_CHECKSUM_LEN,
              mysql::binlog::event::checksum_crc32(crc, p,
                                                   plen - BINLOG_CHECKSUM_LEN));
    ASSERT_FALSE(converted->write(p, plen));
    converted->add_logical_length(plen - BINLOG_CHECKSUM_LEN);
    off += plen;
  }

  EXPECT_EQ(converted->logical_length(), s.bytes.size());
  EXPECT_EQ(logical_of(*converted), s.bytes);
  expect_physical_matches(physical_of(*converted), s.bytes);
}

TEST_F(BinlogCacheRepresentationTest, ResetEmptiesConvertedCache) {
  const Logical_stream s = make_stream(kBodySizes);
  auto source = create_checksumless();
  write_chunked(*source, s.bytes, 4096);
  auto converted = convert(*source);
  ASSERT_FALSE(converted->reset(false /*preserve_spilled_file*/));
  EXPECT_EQ(converted->logical_length(), 0U);
  EXPECT_EQ(converted->physical_length(), 0U);
  EXPECT_FALSE(converted->is_spilled());
}

// The rewrite is triggered by the first write that does not fit in the
// in-memory buffer; filling the buffer exactly does not spill. IO_CACHE may
// round the buffer up, so its size is found by filling it.
TEST_F(BinlogCacheRepresentationTest, WriteWouldSpill) {
  auto rep = create_checksumless();
  const unsigned char byte = 'x';
  my_off_t buffer_size = 0;
  while (!rep->write_would_spill(1)) {
    ASSERT_FALSE(rep->write(&byte, 1));
    ++buffer_size;
    ASSERT_LE(buffer_size, kCacheSize * 4);
  }
  EXPECT_GE(buffer_size, kCacheSize);
  EXPECT_FALSE(rep->is_spilled());  // the buffer is exactly full
  EXPECT_FALSE(rep->write_would_spill(0));
  // The next byte spills; once spilled, there is no first spill to trigger.
  ASSERT_FALSE(rep->write(&byte, 1));
  EXPECT_TRUE(rep->is_spilled());
  EXPECT_FALSE(rep->write_would_spill(kCacheSize * 4));
  EXPECT_EQ(rep->logical_length(), buffer_size + 1);
}

// The rewrite reads an unspilled source from memory, and the
// source never creates a file.
TEST_F(BinlogCacheRepresentationTest, RewriteFromUnspilledSource) {
  const Logical_stream s = make_stream({0, 10, 20, 1000, 30, 7});
  auto source = create_checksumless();
  write_chunked(*source, s.bytes, 100);
  ASSERT_FALSE(source->is_spilled());
  auto rewritten = convert(*source);
  EXPECT_FALSE(source->is_spilled());
  EXPECT_EQ(source->disk_writes(), 0U);
  // The source stays writable, as if the rewrite had failed.
  const Logical_stream more = make_stream({5});
  write_chunked(*source, more.bytes, 100);
  EXPECT_FALSE(source->is_spilled());
  EXPECT_EQ(logical_of(*source), s.bytes + more.bytes);
  source.reset();

  EXPECT_TRUE(rewritten->is_large_trx_rewritten());
  EXPECT_FALSE(rewritten->write_would_spill(kCacheSize * 4));
  EXPECT_EQ(rewritten->logical_length(), s.bytes.size());
  expect_physical_matches(physical_of(*rewritten), s.bytes);
  EXPECT_EQ(logical_of(*rewritten), s.bytes);
}

/*
  The rewrite runs in the middle of an event. The source holds
  the events before the event in progress and the first `cut` bytes of it;
  the rest of that event, still in the standard form, is written to the
  rewritten representation, and one more event follows in binlog file form,
  as binlog_cache_data::write_event() writes it. cut == 0 is an event in
  progress whose first write (its header) did not fit.
*/
TEST_F(BinlogCacheRepresentationTest, RewriteInTheMiddleOfAnEvent) {
  const Logical_stream prefix = make_stream({0, 10, 300, 5});
  const std::string in_progress = make_event(2500, 0, 77);
  const std::string next_event = make_event(40, 0, 78);
  const std::string logical = prefix.bytes + in_progress + next_event;

  for (const Binlog_cache_representation::Checksum_alg alg :
       {BINLOG_CHECKSUM_ALG_CRC32, BINLOG_CHECKSUM_ALG_OFF}) {
    const bool with_crc = alg == BINLOG_CHECKSUM_ALG_CRC32;
    const uint32 footer = with_crc ? BINLOG_CHECKSUM_LEN : 0;
    for (size_t cut :
         {size_t{0}, size_t{1}, size_t{LOG_EVENT_HEADER_LEN - 1},
          size_t{LOG_EVENT_HEADER_LEN}, size_t{LOG_EVENT_HEADER_LEN + 1},
          size_t{1000}, in_progress.size() - 1}) {
      auto source = create_checksumless();
      write_chunked(*source, prefix.bytes + in_progress.substr(0, cut), 64);
      ASSERT_FALSE(source->is_spilled()) << cut;
      auto rewritten = create_checksummed_cache_representation(
          *source, alg, m_dir.c_str(), "bolt_test_", kCacheSize, kMaxCacheSize,
          kReserved);
      ASSERT_NE(rewritten, nullptr) << cut;
      source.reset();

      // The rest of the event in progress, in the standard form.
      write_chunked(*rewritten, in_progress.substr(cut), 333);
      EXPECT_EQ(rewritten->logical_length(),
                prefix.bytes.size() + in_progress.size())
          << cut;

      // The next event, in binlog file form.
      std::string physical_next = next_event;
      auto *p = reinterpret_cast<uchar *>(physical_next.data());
      const uint32 plen = static_cast<uint32>(next_event.size()) + footer;
      int4store(p + EVENT_LEN_OFFSET, plen);
      int4store(
          p + LOG_POS_OFFSET,
          static_cast<uint32>(kReserved + rewritten->physical_length() + plen));
      if (with_crc) {
        const uint32 crc = mysql::binlog::event::checksum_crc32(0L, nullptr, 0);
        uchar footer_bytes[BINLOG_CHECKSUM_LEN];
        int4store(footer_bytes, mysql::binlog::event::checksum_crc32(
                                    crc, p, physical_next.size()));
        physical_next.append(reinterpret_cast<char *>(footer_bytes),
                             BINLOG_CHECKSUM_LEN);
      }
      ASSERT_FALSE(rewritten->write(
          reinterpret_cast<const unsigned char *>(physical_next.data()),
          physical_next.size()))
          << cut;
      rewritten->add_logical_length(next_event.size());

      EXPECT_EQ(rewritten->logical_length(), logical.size()) << cut;
      expect_physical_matches(physical_of(*rewritten), logical, with_crc);
      EXPECT_EQ(logical_of(*rewritten), logical) << cut;
    }
  }
}

// A truncation while the event in progress is being rewritten
// (the statement failed) ends that event; later events are not rewritten.
TEST_F(BinlogCacheRepresentationTest, TruncateDuringEventInProgress) {
  const Logical_stream prefix = make_stream({0, 10, 300, 5});
  const std::string in_progress = make_event(2500, 0, 77);
  auto source = create_checksumless();
  write_chunked(*source, prefix.bytes + in_progress.substr(0, 700), 64);
  auto rewritten = create_checksummed_cache_representation(
      *source, BINLOG_CHECKSUM_ALG_CRC32, m_dir.c_str(), "bolt_test_",
      kCacheSize, kMaxCacheSize, kReserved);
  ASSERT_NE(rewritten, nullptr);
  source.reset();

  ASSERT_FALSE(rewritten->truncate_at_event_boundary(prefix.bytes.size(),
                                                     prefix.boundaries.size()));
  EXPECT_EQ(rewritten->logical_length(), prefix.bytes.size());
  EXPECT_EQ(
      rewritten->physical_length(),
      prefix.bytes.size() + prefix.boundaries.size() * BINLOG_CHECKSUM_LEN);

  // A file-form write after the truncation is stored as is.
  const std::string raw(50, 'z');
  ASSERT_FALSE(rewritten->write(
      reinterpret_cast<const unsigned char *>(raw.data()), raw.size()));
  EXPECT_EQ(rewritten->logical_length(), prefix.bytes.size());
  const std::string physical = physical_of(*rewritten);
  ASSERT_EQ(physical.size(),
            prefix.bytes.size() +
                prefix.boundaries.size() * BINLOG_CHECKSUM_LEN + raw.size());
  EXPECT_EQ(physical.substr(physical.size() - raw.size()), raw);
}

// binlog_checksum=NONE is rewritten too: real log_pos, no footer.
TEST_F(BinlogCacheRepresentationTest, RewriteWithoutChecksum) {
  const Logical_stream s = make_stream(kBodySizes);
  auto source = create_checksumless();
  write_chunked(*source, s.bytes, 777);
  auto rewritten = convert(*source, BINLOG_CHECKSUM_ALG_OFF);
  source.reset();

  EXPECT_TRUE(rewritten->is_large_trx_rewritten());
  EXPECT_EQ(rewritten->physical_checksum_alg(), BINLOG_CHECKSUM_ALG_OFF);
  EXPECT_EQ(rewritten->physical_length(), s.bytes.size());
  expect_physical_matches(physical_of(*rewritten), s.bytes, false /*with_crc*/);
  // The logical view puts log_pos back to 0: the original stream.
  EXPECT_EQ(logical_of(*rewritten), s.bytes);
  // O(1) truncation with no footer bytes per event.
  ASSERT_FALSE(rewritten->truncate_at_event_boundary(s.boundaries[4], 5));
  EXPECT_EQ(rewritten->physical_length(), s.boundaries[4]);
  EXPECT_EQ(logical_of(*rewritten), s.bytes.substr(0, s.boundaries[4]));
}

// The scanning truncate() handles header-only events, with and without a
// footer.
TEST_F(BinlogCacheRepresentationTest, ScanHandlesHeaderOnlyEvents) {
  const Logical_stream s = make_stream({0, 0, 15, 0, 7});
  for (const Binlog_cache_representation::Checksum_alg alg :
       {BINLOG_CHECKSUM_ALG_CRC32, BINLOG_CHECKSUM_ALG_OFF}) {
    const my_off_t footer =
        alg == BINLOG_CHECKSUM_ALG_CRC32 ? BINLOG_CHECKSUM_LEN : 0;
    for (size_t k = 1; k <= s.boundaries.size(); ++k) {
      auto source = create_checksumless();
      write_chunked(*source, s.bytes, 5);
      auto rewritten = convert(*source, alg);
      ASSERT_FALSE(rewritten->truncate(s.boundaries[k - 1])) << k;
      EXPECT_EQ(rewritten->logical_length(), s.boundaries[k - 1]) << k;
      EXPECT_EQ(rewritten->physical_length(), s.boundaries[k - 1] + k * footer)
          << k;
      EXPECT_EQ(logical_of(*rewritten), s.bytes.substr(0, s.boundaries[k - 1]))
          << k;
    }
  }
}

}  // namespace
}  // namespace binlog::unittests
