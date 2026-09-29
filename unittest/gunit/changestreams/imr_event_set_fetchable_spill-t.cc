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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/// @file
/// Unit tests for the WRITE (sink) side of mysql::csa::Event_set_fetchable_spill
/// (tasks.md task 3). The tests drive the Streaming_event_sink surface
/// (append_event / seal_stream / set_stream_truncated) and assert that the
/// published readable end position advances, that sealing is one-way and
/// rejects further appends, and that truncation propagates to the owning
/// Fetchable_transaction and Transaction_envelope. The consumer read-back is a
/// later task and is not exercised here.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/log_event.h"  // Format_description_log_event

namespace mysql::csa::unittests {

namespace fs = std::filesystem;

namespace {

/// A fresh relay-log FDE, upcast to the Log_event_ptr the sink constructor
/// takes (matching how the payload factory hands the sink its FDE).
std::shared_ptr<Log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// Serialize a real Format_description_log_event into bytes, standing in for
/// one event's raw wire bytes. A default server FDE serializes with checksum
/// OFF and needs no THD.
std::vector<unsigned char> serialize_event() {
  Format_description_log_event ev;
  StringBuffer_ostream<1024> os;
  EXPECT_FALSE(ev.write(&os)) << "serializing a stand-in event must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

const char *as_char(const std::vector<unsigned char> &v) {
  return reinterpret_cast<const char *>(v.data());
}

/// Serialize a Rotate_log_event carrying a distinct @p pos, standing in for one
/// transaction event whose identity survives the serialize/decode round-trip
/// (so the consumer can assert events come back in order). Checksum OFF matches
/// the spill file's FDE.
std::vector<unsigned char> serialize_rotate(unsigned long long pos) {
  Rotate_log_event ev("binlog.000001", std::strlen("binlog.000001"), pos,
                      /*flags=*/0);
  ev.common_footer->checksum_alg =
      mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
  StringBuffer_ostream<1024> os;
  EXPECT_FALSE(ev.write(&os)) << "serializing a Rotate event must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

/// Append one Rotate stand-in event carrying @p pos to the sink.
void append_rotate(Event_set_fetchable_spill &sink, unsigned long long pos,
                   bool seal_after = false) {
  const auto bytes = serialize_rotate(pos);
  sink.append_event(as_char(bytes), bytes.size(), seal_after);
}

/// Drain the consumer surface, returning the pos values of the events fetched
/// in order. Stops when wait_next() reports end-of-stream.
std::vector<unsigned long long> drain_positions(
    Event_set_fetchable_spill &sink) {
  std::vector<unsigned long long> out;
  while (sink.wait_next()) {
    auto managed = sink.fetch_next();
    if (!managed.has_value()) break;
    auto *rot = dynamic_cast<Rotate_log_event *>(managed->get_event().get());
    EXPECT_NE(rot, nullptr) << "streamed-back event must decode to a Rotate";
    if (rot != nullptr) out.push_back(rot->pos);
  }
  return out;
}

/// Short bounded wait used to observe that a consumer is still parked.
constexpr std::chrono::milliseconds kShortWait{50};
/// Generous upper bound for a "must make progress / unblock" observation.
constexpr std::chrono::seconds kLongWait{5};

/// The relay-log identity a CRC32 Rotate stand-in event carries; its exact
/// length is what proves the reader stripped the 4-byte CRC32 trailer (a
/// checksum-OFF FDE would leave those 4 bytes glued onto new_log_ident).
const char *const kRotateIdent = "binlog.000123";

/// A fresh FDE advertising CRC32, matching a source running
/// binlog_checksum=CRC32. The spill writer preserves this algorithm in the
/// file's FDE so the reader strips the per-event checksum trailer.
std::shared_ptr<Log_event> make_fde_crc32() {
  auto fde = std::make_shared<Format_description_log_event>();
  fde->common_footer->checksum_alg =
      mysql::binlog::event::BINLOG_CHECKSUM_ALG_CRC32;
  return fde;
}

/// Serialize a Rotate_log_event WITH a CRC32 checksum trailer (as a CRC32
/// source would send it), carrying kRotateIdent and a distinct @p pos.
std::vector<unsigned char> serialize_rotate_crc32(unsigned long long pos) {
  Rotate_log_event ev(kRotateIdent, std::strlen(kRotateIdent), pos,
                      /*flags=*/0);
  ev.common_footer->checksum_alg =
      mysql::binlog::event::BINLOG_CHECKSUM_ALG_CRC32;
  StringBuffer_ostream<1024> os;
  EXPECT_FALSE(ev.write(&os)) << "serializing a CRC32 Rotate event must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

}  // namespace

/// Fixture that gives each test a private, empty "relay log directory" and
/// tears the whole tree down afterwards (spill files + temp-files subdir).
class ImrSpillSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    m_relay_log_dir =
        (fs::temp_directory_path() /
         ("imr_spillsink_" + std::to_string(::getpid()) + "_" +
          std::to_string(counter.fetch_add(1))))
            .string();
    fs::create_directories(m_relay_log_dir);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(m_relay_log_dir, ec);
  }

  std::string m_relay_log_dir;
};

// ---------------------------------------------------------------------------
// Construction: opens the file and publishes the prefix as the initial
// readable watermark.
// ---------------------------------------------------------------------------

TEST_F(ImrSpillSinkTest, ConstructionOpensFileAndPublishesPrefix) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);

  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
  EXPECT_FALSE(sink.spill_file_name().empty());
  EXPECT_TRUE(fs::exists(sink.spill_file_name()));
  EXPECT_TRUE(sink.is_trx());
  EXPECT_NE(sink.get_fde(), nullptr);

  // Streaming-open: not sealed, not truncated, and the published watermark is
  // the (non-empty) relay-log prefix.
  EXPECT_FALSE(sink.is_sealed());
  EXPECT_FALSE(sink.is_stream_truncated());
  EXPECT_GT(sink.published_end_position(), static_cast<std::size_t>(4));
}

// ---------------------------------------------------------------------------
// append_event advances the published position by the appended length, and
// sealing on the terminal event is one-way (further appends are rejected).
// ---------------------------------------------------------------------------

TEST_F(ImrSpillSinkTest, AppendAdvancesPublishedPositionAndSealsOnce) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  constexpr int kEventCount = 4;
  std::size_t prev = sink.published_end_position();
  for (int i = 0; i < kEventCount; ++i) {
    const auto ev = serialize_event();
    const bool last = (i == kEventCount - 1);
    sink.append_event(as_char(ev), ev.size(), /*seal_after=*/last);

    ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
    EXPECT_EQ(sink.published_end_position(), prev + ev.size());
    EXPECT_GT(sink.published_end_position(), prev);
    prev = sink.published_end_position();
  }

  // The last append sealed the stream.
  EXPECT_TRUE(sink.is_sealed());
  EXPECT_FALSE(sink.is_stream_truncated());

  // A post-seal append is a silent no-op: the position does not move.
  const auto extra = serialize_event();
  sink.append_event(as_char(extra), extra.size());
  EXPECT_EQ(sink.published_end_position(), prev);
}

// ---------------------------------------------------------------------------
// seal_stream() seals independently of an append, and appends after it are
// rejected.
// ---------------------------------------------------------------------------

TEST_F(ImrSpillSinkTest, SealStreamRejectsFurtherAppends) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  const auto ev = serialize_event();
  sink.append_event(as_char(ev), ev.size());
  const std::size_t after_one = sink.published_end_position();

  sink.seal_stream();
  EXPECT_TRUE(sink.is_sealed());

  const auto extra = serialize_event();
  sink.append_event(as_char(extra), extra.size());
  EXPECT_EQ(sink.published_end_position(), after_one)
      << "append after seal must not advance the position";
}

// ---------------------------------------------------------------------------
// set_stream_truncated() marks the batch truncated AND propagates to the owning
// Fetchable_transaction and Transaction_envelope.
// ---------------------------------------------------------------------------

TEST_F(ImrSpillSinkTest, TruncatePropagatesToOwners) {
  Transaction_envelope env(/*stream_seqno=*/1, /*trx_length=*/128,
                           Envelope_path::SPILL);
  Fetchable_transaction ft;

  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/&env,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&ft);

  // Append one event, then truncate mid-stream.
  const auto ev = serialize_event();
  sink.append_event(as_char(ev), ev.size());
  ASSERT_FALSE(ft.is_truncated());
  ASSERT_FALSE(env.is_truncated());

  sink.set_stream_truncated();

  EXPECT_TRUE(sink.is_stream_truncated());
  EXPECT_TRUE(sink.is_sealed());  // truncate also seals
  EXPECT_TRUE(ft.is_truncated()) << "truncation must reach the transaction";
  EXPECT_TRUE(env.is_truncated()) << "truncation must reach the envelope";

  // Appends after truncation are rejected.
  const std::size_t pos = sink.published_end_position();
  const auto extra = serialize_event();
  sink.append_event(as_char(extra), extra.size());
  EXPECT_EQ(sink.published_end_position(), pos);
}

// A truncate with no wired owners is safe (nullptr owners are simply skipped).
TEST_F(ImrSpillSinkTest, TruncateWithoutOwnersIsSafe) {
  Event_set_fetchable_spill sink(/*is_trx=*/false, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  sink.set_stream_truncated();
  EXPECT_TRUE(sink.is_stream_truncated());
  EXPECT_TRUE(sink.is_sealed());
  EXPECT_FALSE(sink.is_trx());
}

// ---------------------------------------------------------------------------
// READ (consumer) side: stream events back through the Event_set_fetchable
// surface (wait_next / fetch_next) over the spill file.
// ---------------------------------------------------------------------------

// A sequence written by the sink and sealed on the last event is streamed back
// through the consumer interface, decoding to the same events in order, then
// the consumer terminates cleanly (is_done, not error).
TEST_F(ImrSpillSinkTest, StreamsBackEventsInOrder) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  constexpr int kEventCount = 6;
  std::vector<unsigned long long> expected;
  for (int i = 0; i < kEventCount; ++i) {
    const unsigned long long pos = 100 + i;
    expected.push_back(pos);
    append_rotate(sink, pos, /*seal_after=*/(i == kEventCount - 1));
    ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  }

  const auto got = drain_positions(sink);
  EXPECT_EQ(got, expected);
  EXPECT_TRUE(sink.is_done());
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();

  // Fully drained: further consumer calls are terminal no-ops.
  EXPECT_FALSE(sink.wait_next());
  EXPECT_FALSE(sink.fetch_next().has_value());
}

// Regression test for the checksum-algorithm mismatch: a CRC32 source sends
// events with a trailing 4-byte CRC32, and the spill file's FDE (preserved from
// the receiver's FDE) advertises CRC32, so the consumer must strip that trailer
// before decoding. Proven by the decoded Rotate's new_log_ident being EXACTLY
// kRotateIdent: under the previous "force checksum OFF" writer the reader would
// not strip the 4 CRC bytes, leaving them glued onto the identity (4 bytes
// longer) and corrupting every event.
TEST_F(ImrSpillSinkTest, Crc32EventsRoundTripWithMatchingChecksumFde) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde_crc32(),
                                 m_relay_log_dir, /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  constexpr int kEventCount = 4;
  std::vector<unsigned long long> expected_pos;
  for (int i = 0; i < kEventCount; ++i) {
    const unsigned long long pos = 500 + i;
    expected_pos.push_back(pos);
    const auto bytes = serialize_rotate_crc32(pos);
    sink.append_event(as_char(bytes), bytes.size(),
                      /*seal_after=*/(i == kEventCount - 1));
    ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  }

  const std::size_t expected_ident_len = std::strlen(kRotateIdent);
  std::vector<unsigned long long> got_pos;
  while (sink.wait_next()) {
    auto managed = sink.fetch_next();
    ASSERT_TRUE(managed.has_value());
    auto *rot = dynamic_cast<Rotate_log_event *>(managed->get_event().get());
    ASSERT_NE(rot, nullptr);
    got_pos.push_back(rot->pos);
    // The decisive assertion: the CRC32 trailer was stripped, so the decoded
    // identity is exactly kRotateIdent (not 4 bytes longer).
    EXPECT_EQ(rot->ident_len, expected_ident_len);
    EXPECT_EQ(std::string(rot->new_log_ident, rot->ident_len),
              std::string(kRotateIdent));
  }
  EXPECT_EQ(got_pos, expected_pos);
  EXPECT_TRUE(sink.is_done());
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
}

// A mid-stream wait_next() blocks until the writer publishes more bytes, then
// unblocks and delivers them (streaming read-back, not seal-gated).
TEST_F(ImrSpillSinkTest, WaitBlocksUntilPublishedThenUnblocks) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  // Publish only the first event (do not seal): the consumer can read it, then
  // must block waiting for more.
  append_rotate(sink, 1);

  std::atomic<int> fetched{0};
  std::promise<std::vector<unsigned long long>> done;
  auto future = done.get_future();
  std::thread consumer([&] {
    std::vector<unsigned long long> out;
    while (sink.wait_next()) {
      auto managed = sink.fetch_next();
      if (!managed.has_value()) break;
      auto *rot = dynamic_cast<Rotate_log_event *>(managed->get_event().get());
      if (rot != nullptr) out.push_back(rot->pos);
      fetched.fetch_add(1);
    }
    done.set_value(std::move(out));
  });

  // The consumer reads event 1 then parks in wait_next (stream not sealed).
  const auto deadline = std::chrono::steady_clock::now() + kLongWait;
  while (fetched.load() < 1 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(fetched.load(), 1);
  // Still blocked: it has not finished draining (no seal yet).
  EXPECT_EQ(future.wait_for(kShortWait), std::future_status::timeout)
      << "consumer must block until more bytes are published";

  // Publish the terminal event: the parked consumer must wake and finish.
  append_rotate(sink, 2, /*seal_after=*/true);
  ASSERT_EQ(future.wait_for(kLongWait), std::future_status::ready)
      << "consumer must unblock once more bytes are published";
  const auto out = future.get();
  consumer.join();

  EXPECT_EQ(out, (std::vector<unsigned long long>{1, 2}));
  EXPECT_TRUE(sink.is_done());
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
}

// Truncation mid-stream: the consumer drains the events published so far, then
// stops (done, not error), and the truncation reaches the owning transaction
// and envelope so the worker can roll back.
TEST_F(ImrSpillSinkTest, TruncateMidStreamStopsAfterAvailable) {
  Transaction_envelope env(/*stream_seqno=*/1, /*trx_length=*/128,
                           Envelope_path::SPILL);
  Fetchable_transaction ft;

  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/&env,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&ft);

  // Two events published but the stream is never sealed; instead it truncates.
  append_rotate(sink, 11);
  append_rotate(sink, 22);
  sink.set_stream_truncated();

  const auto got = drain_positions(sink);
  EXPECT_EQ(got, (std::vector<unsigned long long>{11, 22}))
      << "consumer must surface the events received before truncation";
  EXPECT_TRUE(sink.is_done());
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
  EXPECT_TRUE(ft.is_truncated());
  EXPECT_TRUE(env.is_truncated());
}

// A consumer parked in wait_next() with nothing published is unblocked by a
// truncation and reports end-of-stream having read nothing.
TEST_F(ImrSpillSinkTest, TruncateUnblocksParkedConsumer) {
  Transaction_envelope env(/*stream_seqno=*/1, /*trx_length=*/128,
                           Envelope_path::SPILL);
  Fetchable_transaction ft;

  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/&env,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&ft);

  std::promise<std::size_t> done;
  auto future = done.get_future();
  std::thread consumer([&] {
    std::size_t n = 0;
    while (sink.wait_next()) {
      auto managed = sink.fetch_next();
      if (!managed.has_value()) break;
      ++n;
    }
    done.set_value(n);
  });

  // Nothing published beyond the prefix: the consumer parks in wait_next.
  EXPECT_EQ(future.wait_for(kShortWait), std::future_status::timeout)
      << "consumer must park until sealed/truncated";

  sink.set_stream_truncated();
  ASSERT_EQ(future.wait_for(kLongWait), std::future_status::ready)
      << "truncation must wake a parked consumer";
  const std::size_t n = future.get();
  consumer.join();

  EXPECT_EQ(n, static_cast<std::size_t>(0));
  EXPECT_TRUE(sink.is_stream_truncated());
  EXPECT_TRUE(ft.is_truncated());
  EXPECT_TRUE(env.is_truncated());
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
}

}  // namespace mysql::csa::unittests
