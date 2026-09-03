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
/// Unit tests for the WRITE (sink) side of
/// mysql::csa::Event_set_fetchable_spill. The tests drive the
/// Streaming_event_sink surface (append_event / seal_stream /
/// set_stream_truncated) and assert that the published readable end position
/// advances, that sealing is one-way and rejects further appends, and that
/// truncation propagates to the owning Fetchable_transaction and
/// Transaction_envelope.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "my_byteorder.h"  // int4store, int8store
#include "my_dbug.h"       // DBUG_SET
#include "mysql/binlog/event/codecs/factory.h"
#include "mysql/binlog/event/compression/zstd_comp.h"
#include "mysql/binlog/event/control_events.h"  // Transaction_payload_event
#include "mysql/containers/buffers/managed_buffer_sequence.h"
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // ensure_spill_directory
#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"
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
/// the spill file's FDE. The event's length grows with @p name.
std::vector<unsigned char> serialize_rotate(
    unsigned long long pos, const std::string &name = "binlog.000001") {
  Rotate_log_event ev(name.c_str(), name.size(), pos, /*flags=*/0);
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
  EXPECT_FALSE(ev.write(&os))
      << "serializing a CRC32 Rotate event must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

/// Fill a common event header (no checksum) for an event of @p event_size
/// bytes in total.
void store_event_header(unsigned char *p,
                        mysql::binlog::event::Log_event_type type,
                        std::size_t event_size) {
  std::memset(p, 0, LOG_EVENT_HEADER_LEN);
  p[EVENT_TYPE_OFFSET] = static_cast<unsigned char>(type);
  int4store(p + SERVER_ID_OFFSET, 1);
  int4store(p + EVENT_LEN_OFFSET, static_cast<uint32>(event_size));
}

/// Serialize an Xid event (checksum OFF) carrying @p xid. Built by hand: the
/// server's Xid_log_event needs a THD to be constructed.
std::vector<unsigned char> serialize_xid(std::uint64_t xid) {
  std::vector<unsigned char> out(LOG_EVENT_HEADER_LEN + sizeof(xid));
  store_event_header(out.data(), mysql::binlog::event::XID_EVENT, out.size());
  int8store(out.data() + LOG_EVENT_HEADER_LEN, xid);
  return out;
}

/// Serialize a ZSTD Transaction_payload event (checksum OFF) whose payload is
/// @p inner, the concatenated bytes of the events it carries.
std::vector<unsigned char> serialize_payload(
    const std::vector<unsigned char> &inner) {
  mysql::binlog::event::compression::Zstd_comp compressor;
  compressor.feed(inner.data(), inner.size());
  mysql::containers::buffers::Managed_buffer_sequence<> compressed_buffers;
  EXPECT_EQ(compressor.finish(compressed_buffers),
            mysql::binlog::event::compression::Compress_status::success);
  const std::string compressed = compressed_buffers.read_part().str();

  const mysql::binlog::event::Transaction_payload_event payload_event(
      compressed.data(), compressed.size(),
      mysql::binlog::event::compression::type::ZSTD, inner.size());
  unsigned char fields[mysql::binlog::event::Transaction_payload_event::
                           max_length_of_all_headers];
  const auto encoded = mysql::binlog::event::codecs::Factory::build_codec(
                           mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT)
                           ->encode(payload_event, fields, sizeof(fields));
  EXPECT_FALSE(encoded.second) << "encoding the payload fields must succeed";

  std::vector<unsigned char> out(LOG_EVENT_HEADER_LEN + encoded.first +
                                 compressed.size());
  store_event_header(
      out.data(), mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT, out.size());
  std::memcpy(out.data() + LOG_EVENT_HEADER_LEN, fields, encoded.first);
  std::memcpy(out.data() + LOG_EVENT_HEADER_LEN + encoded.first,
              compressed.data(), compressed.size());
  return out;
}

}  // namespace

/// Fixture that gives each test a private, empty "relay log directory" and
/// tears the whole tree down afterwards (spill files + temp-files subdir).
class ImrSpillSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    m_relay_log_dir = (fs::temp_directory_path() /
                       ("imr_spillsink_" + std::to_string(::getpid()) + "_" +
                        std::to_string(counter.fetch_add(1))))
                          .string();
    fs::create_directories(m_relay_log_dir);
    // The spill-files subdirectory is provisioned once at server start by
    // ensure_spill_directory() (from init_replica); the spill sink no longer
    // creates it lazily. This harness bypasses server startup, so provision it
    // here through the same production entry point.
    (void)ensure_spill_directory(m_relay_log_dir);
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
  Transaction_envelope env(/*trx_length=*/128, Envelope_path::SPILL);
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

// A compressed transaction spilled as an outer event followed by one ZSTD
// Transaction_payload event is read back decompressed: the consumer gets the
// outer event, then the inner events in order (not the payload wrapper), and
// the inner Xid ends the transaction cleanly.
TEST_F(ImrSpillSinkTest, CompressedPayloadStreamsBackDecompressed) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  // Stand-in for the GTID event, which is never compressed.
  append_rotate(sink, 1);
  constexpr unsigned long long kInnerCount = 64;
  std::vector<unsigned char> inner;
  for (unsigned long long pos = 100; pos < 100 + kInnerCount; ++pos) {
    const auto bytes = serialize_rotate(pos);
    inner.insert(inner.end(), bytes.begin(), bytes.end());
  }
  constexpr std::uint64_t kXid = 4711;
  const auto xid = serialize_xid(kXid);
  inner.insert(inner.end(), xid.begin(), xid.end());
  const auto payload = serialize_payload(inner);
  ASSERT_LT(payload.size(), inner.size()) << "the payload must be compressed";
  ASSERT_FALSE(sink.append_event(as_char(payload), payload.size(),
                                 /*seal_after=*/true));
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  std::vector<unsigned long long> got_pos;
  std::vector<mysql::binlog::event::Log_event_type> got_types;
  std::uint64_t got_xid = 0;
  while (sink.wait_next()) {
    auto managed = sink.fetch_next();
    ASSERT_TRUE(managed.has_value()) << sink.get_error_str();
    Log_event *ev = managed->get_event().get();
    got_types.push_back(ev->get_type_code());
    if (auto *rot = dynamic_cast<Rotate_log_event *>(ev); rot != nullptr)
      got_pos.push_back(rot->pos);
    if (auto *xev = dynamic_cast<Xid_log_event *>(ev); xev != nullptr)
      got_xid = xev->xid;
  }
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
  EXPECT_TRUE(sink.is_done());

  std::vector<unsigned long long> expected_pos{1};
  for (unsigned long long pos = 100; pos < 100 + kInnerCount; ++pos)
    expected_pos.push_back(pos);
  EXPECT_EQ(got_pos, expected_pos);
  ASSERT_EQ(got_types.size(), kInnerCount + 2);
  EXPECT_EQ(got_types.back(), mysql::binlog::event::XID_EVENT);
  EXPECT_EQ(got_xid, kXid);
  for (const auto type : got_types)
    EXPECT_NE(type, mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT)
        << "the payload wrapper must not reach the consumer";
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
  Transaction_envelope env(/*trx_length=*/128, Envelope_path::SPILL);
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
  Transaction_envelope env(/*trx_length=*/128, Envelope_path::SPILL);
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

// ---------------------------------------------------------------------------
// Writer-only error state. The
// receiver reads the sink through Streaming_event_sink (has_io_error(),
// io_error_message()); the consumers read it through Event_set_fetchable
// (is_error(), get_error_str()). The debug points need a debug build.
// ---------------------------------------------------------------------------

#ifndef NDEBUG
// A reader (consumer) failure is the consumer's own state: the receiver's view
// stays clean, the writer keeps writing and sealing, and a consumer
// reset clears the reader failure so a retry reads the whole transaction.
TEST_F(ImrSpillSinkTest, ReaderFailureLeavesWriterErrorStateUnchanged) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  const Streaming_event_sink &receiver_view = sink;
  append_rotate(sink, 1);

  DBUG_SET("+d,imr_force_spill_reader_open_failure");
  EXPECT_FALSE(sink.wait_next());
  DBUG_SET("-d,imr_force_spill_reader_open_failure");

  // The consumer sees its own failure.
  EXPECT_TRUE(sink.is_error());
  EXPECT_NE(sink.get_error_str().find("could not open spill file"),
            std::string::npos)
      << sink.get_error_str();
  // The receiver does not.
  EXPECT_FALSE(receiver_view.has_io_error());
  EXPECT_TRUE(receiver_view.io_error_message().empty())
      << receiver_view.io_error_message();

  // The writer is not stopped and its state stays clean.
  const auto last = serialize_rotate(2);
  EXPECT_FALSE(sink.append_event(as_char(last), last.size(),
                                 /*seal_after=*/true));
  EXPECT_TRUE(sink.is_sealed());
  EXPECT_FALSE(sink.is_stream_truncated());
  EXPECT_FALSE(receiver_view.has_io_error());
  EXPECT_TRUE(receiver_view.io_error_message().empty())
      << receiver_view.io_error_message();

  // A consumer reset clears the reader failure; the retry reads everything.
  sink.reset(false);
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
  EXPECT_EQ(drain_positions(sink), (std::vector<unsigned long long>{1, 2}));
  EXPECT_TRUE(sink.is_done());
}

// A writer failure reaches the consumer as before: the stream is sealed and
// truncated, both owners are truncated, and the byte source reports the
// writer's text. A consumer reset keeps both the truncation and the
// writer's failure state.
TEST_F(ImrSpillSinkTest, WriterFailureIsVisibleToConsumer) {
  Transaction_envelope env(/*trx_length=*/128, Envelope_path::SPILL);
  Fetchable_transaction ft;
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/&env,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&ft);
  const Streaming_event_sink &receiver_view = sink;
  append_rotate(sink, 1);

  const auto second = serialize_rotate(2);
  DBUG_SET("+d,imr_force_spill_append_failure");
  const bool failed = sink.append_event(as_char(second), second.size());
  DBUG_SET("-d,imr_force_spill_append_failure");
  ASSERT_TRUE(failed);

  // The receiver's view.
  EXPECT_TRUE(receiver_view.has_io_error());
  EXPECT_NE(
      receiver_view.io_error_message().find("forced spill append failure"),
      std::string::npos)
      << receiver_view.io_error_message();

  // The consumer's view.
  EXPECT_TRUE(sink.is_stream_truncated());
  EXPECT_TRUE(sink.is_sealed());
  EXPECT_TRUE(ft.is_truncated());
  EXPECT_TRUE(env.is_truncated());
  EXPECT_TRUE(sink.is_error());
  EXPECT_EQ(sink.get_error_str(), receiver_view.io_error_message());
  EXPECT_FALSE(sink.wait_next());

  // A consumer reset does not clear receiver state.
  sink.reset(false);
  EXPECT_TRUE(sink.is_stream_truncated());
  EXPECT_TRUE(receiver_view.has_io_error());
  EXPECT_NE(
      receiver_view.io_error_message().find("forced spill append failure"),
      std::string::npos)
      << receiver_view.io_error_message();
}

// A worker that reads while a write failure is being recorded sees a
// truncated transaction, which Job_applier::run_phase() rolls back, never a
// fetching error, which it would report as a relay log read failure. The hook
// runs between the truncation and the writer failure in append_event().
TEST_F(ImrSpillSinkTest, WriterFailureIsTruncationNotReadError) {
  Transaction_envelope env(/*trx_length=*/128, Envelope_path::SPILL);
  auto batch = std::make_unique<Event_set_fetchable_spill>(
      /*is_trx=*/true, make_fde(), m_relay_log_dir, &env,
      /*streaming_open=*/true);
  ASSERT_FALSE(batch->is_error()) << batch->get_error_str();
  Event_set_fetchable_spill *sink = batch.get();
  Fetchable_transaction trx;
  trx.append_batch(std::move(batch));
  sink->set_owning_fetchable(&trx);
  const Streaming_event_sink &receiver_view = *sink;

  append_rotate(*sink, 1);
  append_rotate(*sink, 2);
  // The worker reads event 1; event 2 is published but not read yet.
  ASSERT_TRUE(trx.wait_next());
  ASSERT_TRUE(trx.fetch_next().has_value());

  int hook_calls = 0;
  bool wait_result = true;
  bool fetching_error = true;
  bool truncated = false;
  bool writer_error = true;
  sink->set_seal_hook_for_test([&] {
    ++hook_calls;
    // The worker's next steps in Job_applier::run_phase().
    wait_result = trx.wait_next();
    fetching_error = trx.is_fetching_error();
    truncated = trx.is_truncated();
    writer_error = sink->is_error();
  });
  const auto third = serialize_rotate(3);
  DBUG_SET(
      "+d,imr_force_spill_append_failure,imr_spill_append_failure_test_hook");
  const bool failed = sink->append_event(as_char(third), third.size());
  DBUG_SET(
      "-d,imr_force_spill_append_failure,imr_spill_append_failure_test_hook");
  ASSERT_TRUE(failed);
  ASSERT_EQ(hook_calls, 1);

  // In the window: the transaction is truncated, the writer failure is not
  // recorded yet.
  EXPECT_FALSE(wait_result) << "a truncated transaction returns no event";
  EXPECT_TRUE(truncated);
  EXPECT_FALSE(fetching_error)
      << "the worker must see a truncation, not a read error";
  EXPECT_FALSE(writer_error);

  // After the call: still a truncation for the worker, and the receiver sees
  // the writer failure.
  EXPECT_TRUE(trx.is_truncated());
  EXPECT_FALSE(trx.is_fetching_error());
  EXPECT_TRUE(receiver_view.has_io_error());
  EXPECT_NE(
      receiver_view.io_error_message().find("forced spill append failure"),
      std::string::npos)
      << receiver_view.io_error_message();
}

// A reader failure followed by a writer failure on the same transaction
// (possible because a reader failure does not stop the writer): each side
// keeps its own message.
TEST_F(ImrSpillSinkTest, ReaderAndWriterFailuresKeepSeparateMessages) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  const Streaming_event_sink &receiver_view = sink;
  append_rotate(sink, 1);

  DBUG_SET("+d,imr_force_spill_reader_open_failure");
  EXPECT_FALSE(sink.wait_next());
  DBUG_SET("-d,imr_force_spill_reader_open_failure");
  ASSERT_TRUE(sink.is_error());
  const std::string reader_message = sink.get_error_str();

  const auto second = serialize_rotate(2);
  DBUG_SET("+d,imr_force_spill_append_failure");
  EXPECT_TRUE(sink.append_event(as_char(second), second.size()));
  DBUG_SET("-d,imr_force_spill_append_failure");

  EXPECT_NE(reader_message.find("could not open spill file"), std::string::npos)
      << reader_message;
  EXPECT_EQ(sink.get_error_str(), reader_message);
  EXPECT_TRUE(receiver_view.has_io_error());
  EXPECT_NE(
      receiver_view.io_error_message().find("forced spill append failure"),
      std::string::npos)
      << receiver_view.io_error_message();
  EXPECT_EQ(receiver_view.io_error_message().find("could not open spill file"),
            std::string::npos)
      << receiver_view.io_error_message();
}

// ---------------------------------------------------------------------------
// The transaction is marked complete before its
// terminal event becomes visible to a consumer. The seal hook is debug only.
// The flag-only tests link the sink to a Fetchable_transaction with no batch,
// so is_fetching_done() reads true exactly when set_fetching_complete() ran
// (see imr_event_set_fetchable_memory-t.cc).
// ---------------------------------------------------------------------------

// A terminal append marks the transaction complete before the published end
// and the seal become visible; non-terminal appends never mark it complete.
TEST_F(ImrSpillSinkTest, TerminalAppendMarksCompleteBeforeVisible) {
  Fetchable_transaction owner;
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&owner);
  int hook_calls = 0;
  bool complete_in_hook = false;
  bool sealed_in_hook = false;
  std::size_t end_in_hook = 0;
  sink.set_seal_hook_for_test([&] {
    ++hook_calls;
    complete_in_hook = owner.is_fetching_done();
    sealed_in_hook = sink.is_sealed();
    end_in_hook = sink.published_end_position();
  });

  append_rotate(sink, 1);
  append_rotate(sink, 2);
  EXPECT_EQ(hook_calls, 0) << "a non-terminal append does not seal";
  EXPECT_FALSE(owner.is_fetching_done())
      << "a non-terminal append must not mark the transaction complete";
  const std::size_t before_terminal = sink.published_end_position();

  append_rotate(sink, 3, /*seal_after=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  EXPECT_EQ(hook_calls, 1);
  EXPECT_TRUE(sealed_in_hook) << "the hook runs once the seal is visible";
  EXPECT_GT(end_in_hook, before_terminal)
      << "the hook runs once the terminal event is published";
  EXPECT_TRUE(complete_in_hook)
      << "the transaction must be complete when its terminal event becomes "
         "visible";
  EXPECT_TRUE(owner.is_fetching_done());
  EXPECT_FALSE(owner.is_truncated());
}

// seal_stream() uses the same order: complete, then the seal.
TEST_F(ImrSpillSinkTest, SealStreamMarksCompleteBeforeVisible) {
  Fetchable_transaction owner;
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&owner);
  int hook_calls = 0;
  bool complete_in_hook = false;
  sink.set_seal_hook_for_test([&] {
    ++hook_calls;
    complete_in_hook = owner.is_fetching_done();
  });

  append_rotate(sink, 1);
  EXPECT_FALSE(owner.is_fetching_done());
  sink.seal_stream();
  EXPECT_EQ(hook_calls, 1);
  EXPECT_TRUE(complete_in_hook);
}

// A truncated stream is never marked complete: not by a later terminal append,
// which the guard rejects, and not by seal_stream(). After the truncation the
// sink is linked to a second owner that is not truncated, so any later
// set_fetching_complete() would show on it (the first owner's truncation hides
// its complete flag from every accessor).
TEST_F(ImrSpillSinkTest, TruncatedStreamIsNeverMarkedComplete) {
  Fetchable_transaction owner;
  Fetchable_transaction later_owner;
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&owner);
  int hook_calls = 0;
  sink.set_seal_hook_for_test([&] { ++hook_calls; });

  append_rotate(sink, 1);
  sink.set_stream_truncated();
  EXPECT_TRUE(owner.is_truncated());
  EXPECT_FALSE(owner.is_fetching_done());

  sink.set_owning_fetchable(&later_owner);
  append_rotate(sink, 2, /*seal_after=*/true);
  sink.seal_stream();
  EXPECT_EQ(hook_calls, 0) << "a truncated stream has no normal seal";
  EXPECT_FALSE(later_owner.is_fetching_done())
      << "a truncated stream must never mark the transaction complete";
  EXPECT_FALSE(later_owner.is_truncated());
}

// A write failure on the terminal append truncates the stream and never marks
// the transaction complete, nor does a later seal_stream().
TEST_F(ImrSpillSinkTest, TerminalWriteFailureIsNeverMarkedComplete) {
  Fetchable_transaction owner;
  Fetchable_transaction later_owner;
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&owner);
  int hook_calls = 0;
  sink.set_seal_hook_for_test([&] { ++hook_calls; });

  append_rotate(sink, 1);
  const auto terminal = serialize_rotate(2);
  DBUG_SET("+d,imr_force_spill_append_failure");
  const bool failed = sink.append_event(as_char(terminal), terminal.size(),
                                        /*seal_after=*/true);
  DBUG_SET("-d,imr_force_spill_append_failure");
  ASSERT_TRUE(failed);

  EXPECT_EQ(hook_calls, 0);
  EXPECT_TRUE(sink.is_stream_truncated());
  EXPECT_TRUE(owner.is_truncated());
  EXPECT_FALSE(owner.is_fetching_done());

  sink.set_owning_fetchable(&later_owner);
  sink.seal_stream();
  EXPECT_EQ(hook_calls, 0);
  EXPECT_FALSE(later_owner.is_fetching_done())
      << "a failed terminal write must never mark the transaction complete";
}

// The completion race on the spill sink, made deterministic: a worker fetches
// the terminal event inside the hook window, between the moment it becomes
// visible and the notify, with the sink wired as Trx_payload::create_spill()
// wires it. The worker must read the event as the last of the transaction.
TEST_F(ImrSpillSinkTest, ConsumerInSealWindowSeesLastInTransaction) {
  auto batch = std::make_unique<Event_set_fetchable_spill>(
      /*is_trx=*/true, make_fde(), m_relay_log_dir,
      /*owner_envelope=*/nullptr, /*streaming_open=*/true);
  ASSERT_FALSE(batch->is_error()) << batch->get_error_str();
  Event_set_fetchable_spill *sink = batch.get();
  Fetchable_transaction trx;
  trx.append_batch(std::move(batch));
  sink->set_owning_fetchable(&trx);

  // The non-terminal events: fetched, none is the last of the transaction.
  append_rotate(*sink, 1);
  append_rotate(*sink, 2);
  for (unsigned long long pos = 1; pos <= 2; ++pos) {
    ASSERT_TRUE(trx.wait_next());
    auto managed = trx.fetch_next();
    ASSERT_TRUE(managed.has_value());
    auto *rot = dynamic_cast<Rotate_log_event *>(managed->get_event().get());
    ASSERT_NE(rot, nullptr);
    EXPECT_EQ(rot->pos, pos);
    EXPECT_FALSE(managed->is_last_in_transaction());
  }

  // In the hook, a worker thread fetches the terminal event. Its bytes are
  // already published, so its wait_next() returns at once and never waits for
  // the notify that follows the hook.
  std::thread worker;
  std::promise<std::pair<unsigned long long, bool>> fetched;
  auto fetched_future = fetched.get_future();
  bool worker_finished_in_hook = false;
  sink->set_seal_hook_for_test([&] {
    worker = std::thread([&trx, &fetched] {
      std::pair<unsigned long long, bool> result{0, false};
      if (trx.wait_next()) {
        auto managed = trx.fetch_next();
        auto *rot =
            managed.has_value()
                ? dynamic_cast<Rotate_log_event *>(managed->get_event().get())
                : nullptr;
        if (rot != nullptr)
          result = {rot->pos, managed->is_last_in_transaction()};
      }
      fetched.set_value(result);
    });
    worker_finished_in_hook =
        fetched_future.wait_for(kLongWait) == std::future_status::ready;
  });
  append_rotate(*sink, 3, /*seal_after=*/true);
  ASSERT_TRUE(worker.joinable()) << "the seal hook did not run";
  worker.join();

  EXPECT_TRUE(worker_finished_in_hook)
      << "the worker must fetch the terminal event inside the hook window";
  const auto result = fetched_future.get();
  EXPECT_EQ(result.first, 3u);
  EXPECT_TRUE(result.second)
      << "a worker that fetches the terminal event as soon as it is visible "
         "must read it as the last event of the transaction";
  EXPECT_TRUE(trx.is_fetching_done());
  EXPECT_FALSE(trx.wait_next());
}
#endif  // NDEBUG

// ---------------------------------------------------------------------------
// The applier stop flag ends a parked spill batch wait.
// ---------------------------------------------------------------------------

namespace {
/// Bound on how long a parked wait may take to notice the applier stop flag
/// (the batch polls it every Trx_envelope_queue::kApplierStopPollInterval).
constexpr std::chrono::seconds kStopWait{1};
}  // namespace

// A consumer parked in wait_next() with nothing published returns false
// within kStopWait once the applier stop flag is set: nothing, not done, no
// error. After the flag is cleared and the batch reset, a new wait parks again
// and gets the next appended event. Without the poll the wait never ends; the
// watchdog then truncates the stream to release the thread and the test
// fails.
TEST_F(ImrSpillSinkTest, ApplierStopFlagEndsParkedWait) {
  std::atomic<bool> applier_stop{false};
  Event_set_fetchable_spill sink(
      /*is_trx=*/true, make_fde(), m_relay_log_dir, /*owner_envelope=*/nullptr,
      /*streaming_open=*/true, /*channel_name=*/{}, /*owner_queue=*/nullptr,
      /*reserved_disk_bytes=*/0, /*stats_instance_id=*/0, &applier_stop);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  std::promise<bool> result;
  auto future = result.get_future();
  std::thread waiter([&] { result.set_value(sink.wait_next()); });
  EXPECT_EQ(future.wait_for(kShortWait), std::future_status::timeout)
      << "the consumer is parked";

  applier_stop.store(true);
  const bool ended = future.wait_for(kStopWait) == std::future_status::ready;
  if (!ended) sink.set_stream_truncated();  // watchdog: release the waiter
  waiter.join();
  ASSERT_TRUE(ended) << "wait_next() must end within 1 s of the applier stop";
  EXPECT_FALSE(future.get());
  EXPECT_FALSE(sink.is_done()) << "an applier stop is not end-of-stream";
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();

  // Restart: clear the flag and reset, as the next START does.
  applier_stop.store(false);
  sink.reset(false);
  std::promise<bool> result2;
  auto future2 = result2.get_future();
  std::thread waiter2([&] { result2.set_value(sink.wait_next()); });
  EXPECT_EQ(future2.wait_for(kShortWait), std::future_status::timeout)
      << "the consumer is parked";
  append_rotate(sink, 7);
  const bool woke = future2.wait_for(kLongWait) == std::future_status::ready;
  if (!woke) sink.set_stream_truncated();
  waiter2.join();
  ASSERT_TRUE(woke) << "wait_next() must wake once an event is published";
  EXPECT_TRUE(future2.get());
  auto managed = sink.fetch_next();
  ASSERT_TRUE(managed.has_value()) << sink.get_error_str();
  auto *rot = dynamic_cast<Rotate_log_event *>(managed->get_event().get());
  ASSERT_NE(rot, nullptr);
  EXPECT_EQ(rot->pos, 7u);
}

// On an open stream, fetch_next() past the published end returns "nothing
// yet" (not done, no error); only the seal makes the batch done.
TEST_F(ImrSpillSinkTest, FetchPastPublishedEndOnOpenStreamIsNotDone) {
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();

  append_rotate(sink, 5);
  ASSERT_TRUE(sink.wait_next());
  auto managed = sink.fetch_next();
  ASSERT_TRUE(managed.has_value()) << sink.get_error_str();
  auto *rot = dynamic_cast<Rotate_log_event *>(managed->get_event().get());
  ASSERT_NE(rot, nullptr);
  EXPECT_EQ(rot->pos, 5u);

  EXPECT_FALSE(sink.fetch_next().has_value());
  EXPECT_FALSE(sink.is_done()) << "an unsealed stream is not done";
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();

  sink.seal_stream();
  EXPECT_FALSE(sink.wait_next());
  EXPECT_TRUE(sink.is_done());
  EXPECT_FALSE(sink.is_error()) << sink.get_error_str();
}

// ---------------------------------------------------------------------------
// APPLIER_EVENT_MEMORY_LIMIT: each accepted append raises the owning
// transaction's maximum event length, which a worker reserves per event.
// ---------------------------------------------------------------------------

// The value is the maximum over the accepted appends. An append rejected
// because the stream is sealed does not raise it.
TEST_F(ImrSpillSinkTest, AppendRaisesOwnerMaxEventLength) {
  Fetchable_transaction owner;
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&owner);
  EXPECT_EQ(owner.get_max_event_length(), 0u);

  const auto small = serialize_rotate(1);
  const auto large = serialize_rotate(2, std::string(200, 'b'));
  ASSERT_LT(small.size(), large.size());

  sink.append_event(as_char(small), small.size());
  EXPECT_EQ(owner.get_max_event_length(), small.size());
  sink.append_event(as_char(large), large.size());
  EXPECT_EQ(owner.get_max_event_length(), large.size());
  const auto third = serialize_rotate(3);
  sink.append_event(as_char(third), third.size());
  EXPECT_EQ(owner.get_max_event_length(), large.size());

  const auto terminal = serialize_rotate(4);
  sink.append_event(as_char(terminal), terminal.size(), /*seal_after=*/true);
  EXPECT_EQ(owner.get_max_event_length(), large.size());
  const auto huge = serialize_rotate(5, std::string(400, 'c'));
  sink.append_event(as_char(huge), huge.size());
  EXPECT_EQ(owner.get_max_event_length(), large.size())
      << "an append rejected after the seal must not raise the value";
}

#ifndef NDEBUG
// A failed write truncates the stream and does not raise the value.
TEST_F(ImrSpillSinkTest, FailedWriteDoesNotRaiseMaxEventLength) {
  Fetchable_transaction owner;
  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  sink.set_owning_fetchable(&owner);

  const auto small = serialize_rotate(1);
  sink.append_event(as_char(small), small.size());
  ASSERT_EQ(owner.get_max_event_length(), small.size());

  const auto large = serialize_rotate(2, std::string(200, 'b'));
  DBUG_SET("+d,imr_force_spill_append_failure");
  const bool failed = sink.append_event(as_char(large), large.size());
  DBUG_SET("-d,imr_force_spill_append_failure");
  ASSERT_TRUE(failed);
  EXPECT_TRUE(sink.is_stream_truncated());
  EXPECT_EQ(owner.get_max_event_length(), small.size())
      << "a failed write must not raise the value";
}
#endif  // NDEBUG

// ---------------------------------------------------------------------------
// Descriptors: a sealed or failed transaction waiting in the queue holds no
// spill file descriptor; readers open the file by name.
// ---------------------------------------------------------------------------

namespace {

/// Number of descriptors of this process open on spill files under @p dir.
std::size_t count_spill_fds(const std::string &dir) {
  const std::string dir_prefix = fs::canonical(dir).string();
  std::size_t count = 0;
  for (const auto &entry : fs::directory_iterator("/proc/self/fd")) {
    std::error_code ec;
    const fs::path target = fs::read_symlink(entry.path(), ec);
    if (ec) continue;
    if (target.string().rfind(dir_prefix, 0) != 0) continue;
    if (target.filename().string().rfind(Spill_file_writer::kFileNamePrefix,
                                         0) != 0)
      continue;
    ++count;
  }
  return count;
}

}  // namespace

// The terminal append closes the write stream: many sealed transactions hold
// no descriptor, their files stay on disk and readable, and the files are
// deleted only when the byte sources are destroyed.
TEST_F(ImrSpillSinkTest, SealedSinkClosesItsDescriptor) {
  if (!fs::exists("/proc/self/fd")) GTEST_SKIP() << "needs /proc/self/fd";
  const std::size_t before = count_spill_fds(m_relay_log_dir);

  constexpr int kSinkCount = 200;
  std::vector<std::unique_ptr<Event_set_fetchable_spill>> sinks;
  for (int i = 0; i < kSinkCount; ++i) {
    sinks.push_back(std::make_unique<Event_set_fetchable_spill>(
        /*is_trx=*/true, make_fde(), m_relay_log_dir,
        /*owner_envelope=*/nullptr, /*streaming_open=*/true));
    ASSERT_FALSE(sinks.back()->is_error()) << sinks.back()->get_error_str();
    append_rotate(*sinks.back(), 1, /*seal_after=*/true);
    ASSERT_TRUE(sinks.back()->is_sealed());
  }
  EXPECT_EQ(count_spill_fds(m_relay_log_dir), before)
      << "a sealed transaction must not hold its spill file descriptor";

  std::vector<std::string> names;
  for (const auto &sink : sinks) {
    names.push_back(sink->spill_file_name());
    EXPECT_TRUE(fs::exists(names.back()));
  }
  for (const auto &sink : sinks) {
    EXPECT_EQ(drain_positions(*sink), (std::vector<unsigned long long>{1}));
  }
  EXPECT_EQ(count_spill_fds(m_relay_log_dir), before)
      << "a drained reader must close its descriptor";

  sinks.clear();
  for (const auto &name : names) EXPECT_FALSE(fs::exists(name)) << name;
}

#ifndef NDEBUG
// A failed append closes the write stream: the truncated transaction holds no
// descriptor, and its file stays until the byte source is destroyed.
TEST_F(ImrSpillSinkTest, FailedAppendClosesItsDescriptor) {
  if (!fs::exists("/proc/self/fd")) GTEST_SKIP() << "needs /proc/self/fd";
  const std::size_t before = count_spill_fds(m_relay_log_dir);

  Event_set_fetchable_spill sink(/*is_trx=*/true, make_fde(), m_relay_log_dir,
                                 /*owner_envelope=*/nullptr,
                                 /*streaming_open=*/true);
  ASSERT_FALSE(sink.is_error()) << sink.get_error_str();
  append_rotate(sink, 1);
  EXPECT_EQ(count_spill_fds(m_relay_log_dir), before + 1)
      << "the stream stays open while the transaction is written";

  const auto second = serialize_rotate(2);
  DBUG_SET("+d,imr_force_spill_append_failure");
  const bool failed = sink.append_event(as_char(second), second.size());
  DBUG_SET("-d,imr_force_spill_append_failure");
  ASSERT_TRUE(failed);
  EXPECT_EQ(count_spill_fds(m_relay_log_dir), before)
      << "a failed transaction must not hold its spill file descriptor";
  EXPECT_TRUE(fs::exists(sink.spill_file_name()));
}
#endif  // NDEBUG

}  // namespace mysql::csa::unittests
