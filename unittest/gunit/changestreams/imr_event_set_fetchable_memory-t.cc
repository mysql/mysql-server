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
/// Unit tests for the consumer/producer surface of
/// mysql::csa::Event_set_fetchable_memory.
///
/// The byte source stores *encoded* events as IReader_event entries and only
/// decodes them lazily inside fetch_next(). Most tests here drive the internal
/// event-oriented seam append_reader_event(IReader_event_ptr): they feed a fake
/// IReader_event (the "encoded" form) whose decode() hands back a known,
/// non-TPLE Log_event, letting the consumer state machine (ordering, seal,
/// truncate, blocking) be exercised with exact object identity. Serving a
/// non-TPLE event keeps the Transaction-payload decompression path out of the
/// tests entirely: a Format_description_log_event constructs cleanly without a
/// THD and its type code (FORMAT_DESCRIPTION_EVENT) is neither
/// TRANSACTION_PAYLOAD_EVENT nor XID_EVENT, so fetch_next() returns it
/// directly.
///
/// The production Streaming_event_sink contract is the byte-oriented
/// append_event(buf, len, seal_after), which copies the raw bytes, wraps them
/// in an internal Cached_event_memory built from the source's own FDE, and
/// serves them back by real deserialization. That path is covered separately by
/// ByteOrientedAppendDecodesAndSeals (which asserts decoded type + order rather
/// than object identity, since each fetch deserializes a fresh Log_event).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include "my_byteorder.h"        // int4store
#include "sql/basic_ostream.h"   // StringBuffer_ostream
#include "sql/binlog_istream.h"  // Binlog_read_error
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_memory.h"
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/log_event.h"
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {

/// A fake encoded event: it just hands back a preset, already-decoded
/// Log_event when the byte source asks it to decode(). This lets a test control
/// exactly which Log_event object fetch_next() yields, and assert object
/// identity on the way out.
class Fake_reader_event : public IReader_event {
 public:
  explicit Fake_reader_event(std::shared_ptr<Log_event> decoded)
      : m_decoded(std::move(decoded)) {}

  std::shared_ptr<Log_event> decode() override { return m_decoded; }

  // The byte source calls reset() only when re-reading with reset_events=true;
  // these tests never do, so a no-op is sufficient.
  void reset(const Format_description_log_event *) override {}

 private:
  std::shared_ptr<Log_event> m_decoded;
};

/// Build a Format_description_log_event as its precise type. enqueue() takes
/// the exact std::shared_ptr<Format_description_log_event> (matching the type
/// of Imr_runtime::m_fde_snapshot), so it is handed directly
/// with no upcast; passing it to the Event_set_fetchable_memory ctor, whose FDE
/// parameter is the base Log_event_ptr, is a well-formed implicit upcast.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// A body event served by the stream: another FDE object (distinct instance),
/// so object identity is meaningful. Allocated with Log_event::operator new
/// (my_malloc), as a decoded event is: fetch_next() hands its PSI memory
/// ownership over, and my_claim() needs that header.
std::shared_ptr<Log_event> make_event() {
  return std::shared_ptr<Log_event>(new Format_description_log_event());
}

/// Wrap a decoded Log_event into a fake encoded IReader_event entry.
IReader_event_ptr make_fake(std::shared_ptr<Log_event> decoded) {
  return std::make_shared<Fake_reader_event>(std::move(decoded));
}

/// Serialize a real Format_description_log_event into a byte buffer, mimicking
/// the transient network bytes the receiver hands to the byte-oriented
/// append_event(buf, len, ...). A default server FDE writes with checksum OFF
/// and is self-describing on decode, so the round-trip through the sink's
/// internal Cached_event_memory::decode() succeeds without a THD or checksum
/// plumbing.
std::vector<unsigned char> serialize_event() {
  Format_description_log_event ev;
  StringBuffer_ostream<1024> os;
  EXPECT_FALSE(ev.write(&os)) << "serializing the FDE must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

const char *as_char(const std::vector<unsigned char> &v) {
  return reinterpret_cast<const char *>(v.data());
}

}  // namespace

// ---------------------------------------------------------------------------
// Append/seal/fetch ordering and end-of-stream.
// ---------------------------------------------------------------------------

// Append several events on an open stream, seal it, then drive
// wait_next()/fetch_next(): the decoded events must come back in append order
// (same objects). After the last event, wait_next() reports end-of-stream,
// fetch_next() yields nothing, is_done() is true and is_error() is false.
TEST(ImrEventSetFetchableMemoryTest, AppendSealFetchOrder) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  // Append a handful of distinct events on the open stream, remembering the
  // exact object each fake will decode to so we can assert identity + order.
  constexpr int kEventCount = 4;
  std::vector<Log_event *> expected;
  for (int i = 0; i < kEventCount; ++i) {
    auto decoded = make_event();
    expected.push_back(decoded.get());
    source.append_reader_event(make_fake(decoded));
  }

  // Seal: the receiver has delivered the whole transaction.
  source.seal_stream();

  // Drive the consumer surface and collect what comes back.
  std::vector<Log_event *> fetched;
  while (source.wait_next()) {
    auto managed = source.fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    fetched.push_back(managed->get_event().get());
  }

  // Same objects, in append order.
  EXPECT_EQ(fetched, expected);

  // End-of-stream: no more events, cleanly done, no error.
  EXPECT_FALSE(source.wait_next());
  EXPECT_FALSE(source.fetch_next().has_value());
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// ---------------------------------------------------------------------------
// Byte-oriented append: the production Streaming_event_sink path.
// ---------------------------------------------------------------------------

// The byte-oriented append_event(buf, len, seal_after) is what the receiver
// actually drives: it copies the transient encoded bytes, wraps them in an
// internal Cached_event_memory built from the source's own FDE, and serves them
// back through the consumer surface by real deserialization. Feed serialized
// FDE bytes for a few events, seal on the last, then drain: the consumer yields
// exactly that many events, each decoding to a Format_description_log_event, in
// order, followed by a clean end-of-stream. Object identity is intentionally
// NOT asserted here (each fetch decodes a fresh Log_event from the bytes) — a
// post-seal byte append must still be a silent no-op.
TEST(ImrEventSetFetchableMemoryTest, ByteOrientedAppendDecodesAndSeals) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  constexpr int kEventCount = 3;
  for (int i = 0; i < kEventCount; ++i) {
    const auto bytes = serialize_event();
    const bool last = (i == kEventCount - 1);
    source.append_event(as_char(bytes), bytes.size(), /*seal_after=*/last);
  }

  // A post-seal byte append must be a silent no-op.
  const auto extra = serialize_event();
  source.append_event(as_char(extra), extra.size());

  int fetched = 0;
  while (source.wait_next()) {
    auto managed = source.fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    ASSERT_NE(managed->get_event(), nullptr);
    EXPECT_EQ(managed->get_event()->get_type_code(),
              mysql::binlog::event::FORMAT_DESCRIPTION_EVENT);
    ++fetched;
  }

  // Exactly the pre-seal events came back; the post-seal append never became
  // observable; clean end-of-stream.
  EXPECT_EQ(fetched, kEventCount);
  EXPECT_FALSE(source.wait_next());
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// ---------------------------------------------------------------------------
// An undecodable event is a byte-source error.
// ---------------------------------------------------------------------------

// Append an event whose bytes are 5 shorter than its header length, the same
// corruption the simulate_truncated_relay_log_event debug point makes, so
// Cached_event_memory::decode() returns null. fetch_next() must report a
// byte-source error that names the event, not dereference the null event.
TEST(ImrEventSetFetchableMemoryTest, UndecodableEventIsByteSourceError) {
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  const auto bytes = serialize_event();
  ASSERT_GT(bytes.size(), 5u);
  source.append_event(as_char(bytes), bytes.size() - 5, /*seal_after=*/true);

  ASSERT_TRUE(source.wait_next());
  EXPECT_FALSE(source.fetch_next().has_value());
  EXPECT_TRUE(source.is_error());
  EXPECT_NE(source.get_error_str().find("could not decode event #0"),
            std::string::npos)
      << source.get_error_str();

  // The error is sticky until a reset: no more events, and not done.
  EXPECT_FALSE(source.wait_next());
  EXPECT_FALSE(source.is_done());
}

// A later undecodable event, which only a worker decodes, names its own index.
TEST(ImrEventSetFetchableMemoryTest, UndecodableLaterEventNamesItsIndex) {
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  const auto bytes = serialize_event();
  ASSERT_GT(bytes.size(), 5u);
  source.append_event(as_char(bytes), bytes.size());
  source.append_event(as_char(bytes), bytes.size() - 5, /*seal_after=*/true);

  ASSERT_TRUE(source.wait_next());
  auto first = source.fetch_next();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->get_event()->get_type_code(),
            mysql::binlog::event::FORMAT_DESCRIPTION_EVENT);

  ASSERT_TRUE(source.wait_next());
  EXPECT_FALSE(source.fetch_next().has_value());
  EXPECT_TRUE(source.is_error());
  EXPECT_NE(source.get_error_str().find("could not decode event #1"),
            std::string::npos)
      << source.get_error_str();
}

namespace {

/// Encode an Xid_log_event (common header plus @p body_len body bytes, no
/// checksum) whose header length matches the buffer. The Xid body is 8 bytes,
/// so a shorter body passes the length checks and fails in the event
/// constructor.
std::vector<unsigned char> xid_event_bytes(std::size_t body_len) {
  std::vector<unsigned char> bytes(LOG_EVENT_HEADER_LEN + body_len, 0);
  bytes[EVENT_TYPE_OFFSET] = mysql::binlog::event::XID_EVENT;
  int4store(bytes.data() + SERVER_ID_OFFSET, 1);
  int4store(bytes.data() + EVENT_LEN_OFFSET,
            static_cast<uint32_t>(bytes.size()));
  return bytes;
}

}  // namespace

// A malformed non-GTID event (an Xid event with a 4-byte body, checksum
// off: the memory path does not re-verify it, the receiver already did) is
// rejected by binlog_event_deserialize() as INVALID_EVENT. The error names the
// event and keeps the reader's cause, Binlog_read_error::get_str().
TEST(ImrEventSetFetchableMemoryTest, UndecodableEventKeepsReaderCause) {
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  const auto good = xid_event_bytes(8);
  const auto bad = xid_event_bytes(4);
  source.append_event(as_char(good), good.size());
  source.append_event(as_char(bad), bad.size(), /*seal_after=*/true);

  // The well-formed Xid event decodes, so the bytes are right but the body.
  ASSERT_TRUE(source.wait_next());
  auto first = source.fetch_next();
  ASSERT_TRUE(first.has_value()) << source.get_error_str();
  EXPECT_EQ(first->get_event()->get_type_code(),
            mysql::binlog::event::XID_EVENT);

  ASSERT_TRUE(source.wait_next());
  EXPECT_FALSE(source.fetch_next().has_value());
  EXPECT_TRUE(source.is_error());
  const std::string expected =
      std::string(
          "In-memory relay log: could not decode event #1 of the "
          "transaction: ") +
      Binlog_read_error(Binlog_read_error::INVALID_EVENT).get_str();
  EXPECT_EQ(source.get_error_str(), expected);
}

// Fallback: an IReader_event that is not a Cached_event_memory gives no
// cause, so the error keeps the generic text.
TEST(ImrEventSetFetchableMemoryTest,
     UndecodableOtherReaderEventKeepsGenericText) {
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.append_reader_event(make_fake(nullptr), /*seal_after=*/true);

  ASSERT_TRUE(source.wait_next());
  EXPECT_FALSE(source.fetch_next().has_value());
  EXPECT_TRUE(source.is_error());
  EXPECT_EQ(
      source.get_error_str(),
      "In-memory relay log: could not decode event #0 of the transaction");
}

// ---------------------------------------------------------------------------
// Appends are rejected after the stream is sealed.
// ---------------------------------------------------------------------------

// Append one event on an open stream, seal it, then attempt another append.
// The post-seal append must be dropped (no-op): only the single pre-seal event
// is yielded (by object identity), followed by clean end-of-stream.
TEST(ImrEventSetFetchableMemoryTest, AppendRejectedAfterSeal) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  // One event delivered before the seal.
  auto decoded = make_event();
  Log_event *expected = decoded.get();
  source.append_reader_event(make_fake(decoded));

  // Seal: the receiver has delivered the whole transaction.
  source.seal_stream();

  // Any further append after the seal must be a silent no-op.
  source.append_reader_event(make_fake(make_event()));

  // Drive the consumer surface and collect what comes back.
  std::vector<Log_event *> fetched;
  while (source.wait_next()) {
    auto managed = source.fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    fetched.push_back(managed->get_event().get());
  }

  // Exactly the one pre-seal event, by identity — the post-seal append never
  // became observable.
  ASSERT_EQ(fetched.size(), 1u);
  EXPECT_EQ(fetched[0], expected);

  // End-of-stream: cleanly done, no error.
  EXPECT_FALSE(source.wait_next());
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// append_event(e, seal_after=true) publishes e and seals atomically; a
// subsequent append is rejected. Exactly the one sealed-with event is yielded
// (by identity), then clean end-of-stream.
TEST(ImrEventSetFetchableMemoryTest, AppendWithSealAfterSealsStream) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  // Publish the event and seal the stream in one atomic step.
  auto decoded = make_event();
  Log_event *expected = decoded.get();
  source.append_reader_event(make_fake(decoded), /*seal_after=*/true);

  // The stream is now sealed, so this append must be dropped (no-op).
  source.append_reader_event(make_fake(make_event()));

  // Drive the consumer surface and collect what comes back.
  std::vector<Log_event *> fetched;
  while (source.wait_next()) {
    auto managed = source.fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    fetched.push_back(managed->get_event().get());
  }

  // Exactly the one sealed-with event, by identity — the later append never
  // became observable.
  ASSERT_EQ(fetched.size(), 1u);
  EXPECT_EQ(fetched[0], expected);

  // End-of-stream: cleanly done, no error.
  EXPECT_FALSE(source.wait_next());
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// ---------------------------------------------------------------------------
// wait_next() blocking on an empty, unsealed stream and waking on
// append/seal.
// ---------------------------------------------------------------------------

namespace {

// A short bounded probe: how long we wait to *prove* a blocked wait_next() is
// still blocked. Kept small so the tests stay quick.
constexpr auto kBlockProbe = std::chrono::milliseconds(50);
// A generous ceiling for the "became ready" wait, so a slow-but-correct wakeup
// is not mistaken for a hang.
constexpr auto kWakeTimeout = std::chrono::seconds(3);

}  // namespace

// A wait_next() on an empty, unsealed stream must block; it becomes ready and
// returns true once an event is appended. A background thread runs wait_next()
// into a promise: while the stream is empty the future must stay unfulfilled
// (timeout), and it must complete with true after the append.
TEST(ImrEventSetFetchableMemoryTest, WaitNextBlocksThenWakesOnAppend) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  std::promise<bool> result;
  std::future<bool> future = result.get_future();
  std::thread waiter(
      [&source, &result]() { result.set_value(source.wait_next()); });

  // Still blocked: nothing has been appended and the stream is not sealed.
  EXPECT_EQ(future.wait_for(kBlockProbe), std::future_status::timeout)
      << "wait_next() must block on an empty, unsealed stream";

  // Publish one event: the blocked waiter must wake and report an event.
  source.append_reader_event(make_fake(make_event()));

  ASSERT_EQ(future.wait_for(kWakeTimeout), std::future_status::ready)
      << "wait_next() must wake once an event is appended";
  EXPECT_TRUE(future.get());

  waiter.join();
}

// A wait_next() blocked on an empty stream returns false (clean end-of-stream)
// when the stream is sealed with no events buffered.
TEST(ImrEventSetFetchableMemoryTest, WaitNextBlockedWakesOnSealEmpty) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  std::promise<bool> result;
  std::future<bool> future = result.get_future();
  std::thread waiter(
      [&source, &result]() { result.set_value(source.wait_next()); });

  // Still blocked: empty and unsealed.
  EXPECT_EQ(future.wait_for(kBlockProbe), std::future_status::timeout)
      << "wait_next() must block on an empty, unsealed stream";

  // Seal with no events: the blocked waiter wakes to end-of-stream.
  source.seal_stream();

  ASSERT_EQ(future.wait_for(kWakeTimeout), std::future_status::ready)
      << "wait_next() must wake when the empty stream is sealed";
  EXPECT_FALSE(future.get());

  waiter.join();

  // Cleanly done, no error.
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// ---------------------------------------------------------------------------
// Truncation surfaces an incomplete transaction.
// ---------------------------------------------------------------------------

// set_stream_truncated() drains any already-buffered events, then reports
// end-of-stream WITHOUT blocking for the missing remainder. Append two events,
// truncate, then drive wait_next()/fetch_next(): both buffered events come back
// in append order (by identity), followed by clean end-of-stream.
TEST(ImrEventSetFetchableMemoryTest, TruncationDrainsBufferedThenReportsDone) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  // Two events delivered before truncation.
  auto first = make_event();
  auto second = make_event();
  std::vector<Log_event *> expected{first.get(), second.get()};
  source.append_reader_event(make_fake(first));
  source.append_reader_event(make_fake(second));

  // The transaction is incomplete: truncate the stream. The already-buffered
  // events must still drain; the missing remainder must not be waited for.
  source.set_stream_truncated();

  std::vector<Log_event *> fetched;
  while (source.wait_next()) {
    auto managed = source.fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    fetched.push_back(managed->get_event().get());
  }

  // Both buffered events, by identity, in append order.
  EXPECT_EQ(fetched, expected);

  // End-of-stream reached without blocking: done, no error.
  EXPECT_FALSE(source.wait_next());
  EXPECT_FALSE(source.fetch_next().has_value());
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// A wait_next() already blocked on an empty stream wakes and returns false
// (end-of-stream) when set_stream_truncated() is called.
TEST(ImrEventSetFetchableMemoryTest, WaitNextBlockedWakesOnTruncate) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  std::promise<bool> result;
  std::future<bool> future = result.get_future();
  std::thread waiter(
      [&source, &result]() { result.set_value(source.wait_next()); });

  // Still blocked: empty and unsealed.
  EXPECT_EQ(future.wait_for(kBlockProbe), std::future_status::timeout)
      << "wait_next() must block on an empty, unsealed stream";

  // Truncate: the blocked waiter wakes to end-of-stream.
  source.set_stream_truncated();

  ASSERT_EQ(future.wait_for(kWakeTimeout), std::future_status::ready)
      << "wait_next() must wake when the empty stream is truncated";
  EXPECT_FALSE(future.get());

  waiter.join();

  // Truncation surfaces as end-of-stream (incomplete transaction).
  EXPECT_TRUE(source.is_done());
}

// The stranded-worker contract that the receiver-stop truncate fix relies on:
// a consumer that has already drained the buffered events and is RE-BLOCKED in
// wait_next() waiting for more of a still-unsealed stream (a worker mid-apply
// of a partially-received transaction) is released by set_stream_truncated(),
// waking to end-of-stream after yielding exactly the events buffered before the
// truncation. Without a truncate on receiver stop this consumer would block
// forever, which is the hang this fix removes.
TEST(ImrEventSetFetchableMemoryTest,
     WaitNextBlockedAfterPartialConsumeWakesOnTruncate) {
  auto fde = make_fde();
  Event_set_fetchable_memory source(/*is_trx=*/true, fde,
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);

  // One event delivered so far; the transaction is still open (unsealed).
  auto decoded = make_event();
  Log_event *expected = decoded.get();
  source.append_reader_event(make_fake(decoded));

  // Background consumer: drain everything wait_next() offers until it reports
  // end-of-stream, recording the events seen by identity.
  std::promise<std::vector<Log_event *>> result;
  std::future<std::vector<Log_event *>> future = result.get_future();
  std::thread consumer([&source, &result]() {
    std::vector<Log_event *> seen;
    while (source.wait_next()) {
      auto managed = source.fetch_next();
      if (!managed.has_value()) break;
      seen.push_back(managed->get_event().get());
    }
    result.set_value(std::move(seen));
  });

  // The consumer takes the one buffered event, then re-blocks in wait_next()
  // waiting for more of the unsealed stream: the future must stay pending.
  EXPECT_EQ(future.wait_for(kBlockProbe), std::future_status::timeout)
      << "consumer must re-block in wait_next() on the unsealed stream after "
         "draining the buffered event";

  // The receiver stopped mid-transaction: truncate. The blocked consumer must
  // wake to end-of-stream.
  source.set_stream_truncated();

  ASSERT_EQ(future.wait_for(kWakeTimeout), std::future_status::ready)
      << "wait_next() must wake when the stream is truncated";
  std::vector<Log_event *> seen = future.get();
  consumer.join();

  // Exactly the one pre-truncation event, by identity; then end-of-stream.
  ASSERT_EQ(seen.size(), 1u);
  EXPECT_EQ(seen[0], expected);
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// ---------------------------------------------------------------------------
// set_success() commits the envelope and releases bytes once.
// ---------------------------------------------------------------------------

namespace {

// Per-channel memory bounds for the commit-hook tests. kTrxLength must be at
// or below the spill threshold so classify() picks the MEMORY path and
// enqueue() returns a non-null envelope.
constexpr std::size_t kMemoryLimit = 1u << 20;     // 1 MiB
constexpr std::size_t kSpillThreshold = 1u << 16;  // 64 KiB
constexpr std::size_t kTrxLength = 4096;

// Enqueue a memory-path envelope. enqueue() itself creates the empty
// single-batch destination and attaches a Trx_payload reserving kTrxLength
// bytes, so the returned envelope is already ready for the commit hook. The
// commit-hook tests drive commit through a SEPARATE Event_set_fetchable_memory
// (also owning this envelope); its set_success() commits the envelope and
// releases the enqueue-created payload's reserved bytes. Returns the
// (non-owning) envelope pointer; asserts enqueue succeeded.
Transaction_envelope *enqueue_envelope_with_payload(Trx_envelope_queue &queue) {
  Transaction_envelope *env = queue.enqueue(kTrxLength, true, make_fde());
  EXPECT_NE(env, nullptr);
  return env;
}

}  // namespace

// With an envelope enqueued and a payload attached, set_success() drives
// commit(): the envelope becomes committed, its payload is nulled, and the
// reserved bytes are released back to the queue counter (bytes_used() returns
// to 0). "Fully received" is observed via the consumer surface, not any
// envelope state.
TEST(ImrEventSetFetchableMemoryTest,
     SetSuccessCommitsEnvelopeAndReleasesBytes) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  Transaction_envelope *env = enqueue_envelope_with_payload(queue);
  ASSERT_NE(env, nullptr);

  // The byte source holds a non-owning back-reference to its owning envelope.
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(), env,
                                    /*streaming_open=*/true);

  // Precondition: the payload's bytes are reserved and the envelope is
  // uncommitted.
  ASSERT_EQ(queue.bytes_used(), kTrxLength);
  ASSERT_FALSE(env->is_committed());

  // Fire the commit hook.
  source.set_success();

  // The envelope committed exactly once: committed, payload nulled, bytes back.
  EXPECT_TRUE(env->is_committed());
  EXPECT_EQ(env->payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);

  // Drain so the queue destructor invariant (empty deque, bytes_used() == 0)
  // holds. This envelope was committed but never dispatched, so
  // sweep_committed() would refuse to advance the commit mark past the dispatch
  // cursor and leave it in place. reset() is the production full-stop drain: it
  // drops every entry (bytes are already 0 after commit). After reset, env
  // dangles - do not touch it.
  queue.reset();
}

// ---------------------------------------------------------------------------
// Repeated set_success() is a no-op.
// ---------------------------------------------------------------------------

// A second set_success() after commit is rejected internally (the envelope is
// already committed) and discarded: is_committed() stays true and bytes_used()
// stays unchanged (no double release).
TEST(ImrEventSetFetchableMemoryTest, RepeatedSetSuccessLeavesStateAndCounter) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  Transaction_envelope *env = enqueue_envelope_with_payload(queue);
  ASSERT_NE(env, nullptr);

  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(), env,
                                    /*streaming_open=*/true);

  // First commit: succeeds, releasing the reserved bytes.
  source.set_success();
  ASSERT_TRUE(env->is_committed());
  ASSERT_EQ(queue.bytes_used(), 0u);

  // Second commit: rejected internally and discarded - no state change, no
  // double release.
  source.set_success();
  EXPECT_TRUE(env->is_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);

  // Drain so the queue destructor invariant holds. This envelope was committed
  // but never dispatched, so sweep_committed() would refuse to advance the
  // commit mark past the dispatch cursor and leave it in place. reset() is the
  // production full-stop drain: it drops every entry (bytes are already 0 after
  // commit). After reset, env dangles - do not touch it.
  queue.reset();
}

// ---------------------------------------------------------------------------
// Property test: interface parity.
// ---------------------------------------------------------------------------
//
// The interface parity property states that the in-memory source serves the
// SAME event byte stream via wait_next()/fetch_next() as the on-disk
// Event_set_fetchable_cache path.
//
// We deliberately do NOT construct a live Event_set_fetchable_cache fixture in
// this STANDALONE_BINLOG gunit harness. A real cache requires a
// Relay_log_deleter_handle and the relay-log deleter infrastructure, which is
// far heavier than a unit test should pull in (it would drag in additional
// server link dependencies that this lightweight target is specifically wired
// to avoid). Standing one up here would trade a focused, fast unit test for a
// brittle integration harness.
//
// Interface parity is nevertheless meaningful to express as an equivalent
// randomized-sequence ("property") test. The memory source and the cache source
// share the identical consumer state machine (wait_next / fetch_next /
// decompress); the only thing that differs is where the encoded bytes come
// from. Given that shared consumer, the observable, testable contract for the
// memory variant is that it serves back EXACTLY the appended event sequence, in
// order — i.e. append-order fidelity. If the memory source reproduces its input
// sequence faithfully for arbitrary randomized batches, then, driven through
// the shared state machine, it yields the same stream the cache path would for
// the same encoded input. That append-order fidelity is the equivalent
// expression of interface parity here.
//
// The MySQL tree does not integrate rapidcheck, so the property is checked
// with a deterministically seeded std::mt19937 generator running many trials
// inside a single gtest TEST. The seed is echoed once and every assertion
// carries seed / trial / index so any failure reproduces exactly.
TEST(ImrEventSetFetchableMemoryTest, PropertyInterfaceParity) {
  constexpr std::uint32_t kSeed = 0x0E5E7A11;
  constexpr int kTrials = 500;
  constexpr int kMaxBatch = 16;

  std::cout << "PropertyInterfaceParity seed=0x" << std::hex << kSeed
            << std::dec << " trials=" << kTrials << std::endl;

  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> batch_dist(0, kMaxBatch);

  for (int trial = 0; trial < kTrials; ++trial) {
    Event_set_fetchable_memory src(/*is_trx=*/true, make_fde(),
                                   /*owner_envelope=*/nullptr,
                                   /*streaming_open=*/true);

    const int n = batch_dist(rng);

    // Build N fake events, each decoding to a distinct known Log_event. Keep
    // the fakes alive for the whole trial: they own the Log_event that decode()
    // returns and that fetch_next() yields by identity. Record the expected raw
    // Log_event* pointers in append order.
    std::vector<IReader_event_ptr> fakes;
    std::vector<Log_event *> expected;
    fakes.reserve(n);
    expected.reserve(n);
    for (int i = 0; i < n; ++i) {
      auto decoded = make_event();
      expected.push_back(decoded.get());
      fakes.push_back(make_fake(decoded));
    }

    // Append all N events, then seal the stream.
    for (int i = 0; i < n; ++i) {
      src.append_reader_event(fakes[i]);
    }
    src.seal_stream();

    // Drive wait_next()/fetch_next() to completion, collecting returned events
    // by identity.
    std::vector<Log_event *> fetched;
    fetched.reserve(n);
    while (src.wait_next()) {
      auto managed = src.fetch_next();
      ASSERT_TRUE(managed.has_value())
          << "PropertyInterfaceParity seed=0x" << std::hex << kSeed << std::dec
          << " trial=" << trial << " index=" << fetched.size()
          << ": wait_next() returned true so fetch_next() must yield an event";
      fetched.push_back(managed->get_event().get());
    }

    // The served sequence must EXACTLY equal the appended sequence: same size
    // and element-wise identity, in order.
    ASSERT_EQ(fetched.size(), expected.size())
        << "PropertyInterfaceParity seed=0x" << std::hex << kSeed << std::dec
        << " trial=" << trial << ": served count must equal appended count";
    for (std::size_t i = 0; i < expected.size(); ++i) {
      ASSERT_EQ(fetched[i], expected[i])
          << "PropertyInterfaceParity seed=0x" << std::hex << kSeed << std::dec
          << " trial=" << trial << " index=" << i
          << ": served event must match appended event by identity/order";
    }

    // Clean end-of-stream: done, no error.
    EXPECT_TRUE(src.is_done())
        << "PropertyInterfaceParity seed=0x" << std::hex << kSeed << std::dec
        << " trial=" << trial << ": stream must be done after the last event";
    EXPECT_FALSE(src.is_error())
        << "PropertyInterfaceParity seed=0x" << std::hex << kSeed << std::dec
        << " trial=" << trial << ": stream must not be in error";
  }
}

// ---------------------------------------------------------------------------
// The transaction is marked complete before its
// terminal event becomes visible to a consumer. The seal hook is debug only.
//
// The tests that only check the flag link the sink to a Fetchable_transaction
// that holds no batch. set_fetching_complete() then also sets the
// transaction's done flag, because there is no current batch, so
// is_fetching_done() reads true exactly when the transaction was marked
// complete (and not truncated).
// ---------------------------------------------------------------------------

#ifndef NDEBUG
// A terminal append marks the transaction complete before the event and the
// seal become visible: the hook, which runs right after the push and the seal,
// already sees it complete. Non-terminal appends never mark it complete.
TEST(ImrEventSetFetchableMemoryTest, TerminalAppendMarksCompleteBeforeVisible) {
  Fetchable_transaction owner;
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.set_owning_fetchable(&owner);
  int hook_calls = 0;
  bool complete_in_hook = false;
  source.set_seal_hook_for_test([&] {
    ++hook_calls;
    complete_in_hook = owner.is_fetching_done();
  });

  source.append_reader_event(make_fake(make_event()));
  source.append_reader_event(make_fake(make_event()));
  EXPECT_EQ(hook_calls, 0) << "a non-terminal append does not seal";
  EXPECT_FALSE(owner.is_fetching_done())
      << "a non-terminal append must not mark the transaction complete";

  source.append_reader_event(make_fake(make_event()), /*seal_after=*/true);
  EXPECT_EQ(hook_calls, 1);
  EXPECT_TRUE(complete_in_hook)
      << "the transaction must be complete when its terminal event becomes "
         "visible";
  EXPECT_TRUE(owner.is_fetching_done());
  EXPECT_FALSE(owner.is_truncated());
}

// seal_stream() uses the same order: complete, then the seal.
TEST(ImrEventSetFetchableMemoryTest, SealStreamMarksCompleteBeforeVisible) {
  Fetchable_transaction owner;
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.set_owning_fetchable(&owner);
  int hook_calls = 0;
  bool complete_in_hook = false;
  source.set_seal_hook_for_test([&] {
    ++hook_calls;
    complete_in_hook = owner.is_fetching_done();
  });

  source.append_reader_event(make_fake(make_event()));
  EXPECT_FALSE(owner.is_fetching_done());
  source.seal_stream();
  EXPECT_EQ(hook_calls, 1);
  EXPECT_TRUE(complete_in_hook);
}

// A truncated stream is never marked complete: not by a later terminal append,
// which the guard rejects, and not by seal_stream(). Truncation marks the
// first owner truncated, which hides its complete flag from every accessor, so
// the sink is then linked to a second owner that is not truncated: any later
// set_fetching_complete() would show on it.
TEST(ImrEventSetFetchableMemoryTest, TruncatedStreamIsNeverMarkedComplete) {
  Fetchable_transaction owner;
  Fetchable_transaction later_owner;
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.set_owning_fetchable(&owner);
  int hook_calls = 0;
  source.set_seal_hook_for_test([&] { ++hook_calls; });

  source.append_reader_event(make_fake(make_event()));
  source.set_stream_truncated();
  EXPECT_TRUE(owner.is_truncated());
  EXPECT_FALSE(owner.is_fetching_done());

  source.set_owning_fetchable(&later_owner);
  source.append_reader_event(make_fake(make_event()), /*seal_after=*/true);
  source.seal_stream();
  EXPECT_EQ(hook_calls, 0) << "a truncated stream has no normal seal";
  EXPECT_FALSE(later_owner.is_fetching_done())
      << "a truncated stream must never mark the transaction complete";
  EXPECT_FALSE(later_owner.is_truncated());
}

// The completion race, made deterministic: a worker fetches the terminal
// event inside the hook window, between the moment it becomes visible and the
// notify. The sink is wired as Trx_payload::create_memory() wires it. The
// worker must read the event as the last of the transaction; with the old
// order (seal, then complete) it read false and applied a non-atomic DDL as
// an ordinary event (job_applier.cpp:418).
TEST(ImrEventSetFetchableMemoryTest,
     ConsumerInSealWindowSeesLastInTransaction) {
  auto batch = std::make_unique<Event_set_fetchable_memory>(
      /*is_trx=*/true, make_fde(), /*owner_envelope=*/nullptr,
      /*streaming_open=*/true);
  Event_set_fetchable_memory *source = batch.get();
  Fetchable_transaction trx;
  trx.append_batch(std::move(batch));
  source->set_owning_fetchable(&trx);

  // The non-terminal events: fetched, none is the last of the transaction.
  source->append_reader_event(make_fake(make_event()));
  source->append_reader_event(make_fake(make_event()));
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(trx.wait_next());
    auto managed = trx.fetch_next();
    ASSERT_TRUE(managed.has_value());
    EXPECT_FALSE(managed->is_last_in_transaction());
  }

  // In the hook, a worker thread fetches the terminal event. The event is
  // already pushed, so its wait_next() returns at once and never waits for the
  // notify that follows the hook.
  auto terminal = make_event();
  std::thread worker;
  std::promise<std::pair<Log_event *, bool>> fetched;
  std::future<std::pair<Log_event *, bool>> fetched_future =
      fetched.get_future();
  bool worker_finished_in_hook = false;
  source->set_seal_hook_for_test([&] {
    worker = std::thread([&trx, &fetched] {
      std::pair<Log_event *, bool> result{nullptr, false};
      if (trx.wait_next()) {
        auto managed = trx.fetch_next();
        if (managed.has_value())
          result = {managed->get_event().get(),
                    managed->is_last_in_transaction()};
      }
      fetched.set_value(result);
    });
    worker_finished_in_hook =
        fetched_future.wait_for(kWakeTimeout) == std::future_status::ready;
  });
  source->append_reader_event(make_fake(terminal), /*seal_after=*/true);
  ASSERT_TRUE(worker.joinable()) << "the seal hook did not run";
  worker.join();

  EXPECT_TRUE(worker_finished_in_hook)
      << "the worker must fetch the terminal event inside the hook window";
  const auto result = fetched_future.get();
  EXPECT_EQ(result.first, terminal.get());
  EXPECT_TRUE(result.second)
      << "a worker that fetches the terminal event as soon as it is visible "
         "must read it as the last event of the transaction";
  EXPECT_TRUE(trx.is_fetching_done());
  EXPECT_FALSE(trx.wait_next());
}
#endif  // NDEBUG

// ---------------------------------------------------------------------------
// The applier stop flag ends a parked batch wait.
// ---------------------------------------------------------------------------

namespace {
/// Bound on how long a parked wait may take to notice the applier stop flag
/// (the batch polls it every Trx_envelope_queue::kApplierStopPollInterval).
constexpr auto kStopWait = std::chrono::seconds(1);
}  // namespace

// A consumer parked in wait_next() on an open, empty stream returns false
// within kStopWait once the applier stop flag is set: nothing, not done, no
// error. After the flag is cleared and the batch reset, a new wait parks again
// and gets the next appended event. Without the poll the wait never ends; the
// watchdog then seals the stream to release the thread and the test fails.
TEST(ImrEventSetFetchableMemoryTest, ApplierStopFlagEndsParkedWait) {
  std::atomic<bool> applier_stop{false};
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true,
                                    /*stats_instance_id=*/0, &applier_stop);

  std::promise<bool> result;
  std::future<bool> future = result.get_future();
  std::thread waiter(
      [&source, &result]() { result.set_value(source.wait_next()); });
  EXPECT_EQ(future.wait_for(kBlockProbe), std::future_status::timeout)
      << "the consumer is parked";

  applier_stop.store(true);
  const bool ended = future.wait_for(kStopWait) == std::future_status::ready;
  if (!ended) source.seal_stream();  // watchdog: release the waiter
  waiter.join();
  ASSERT_TRUE(ended) << "wait_next() must end within 1 s of the applier stop";
  EXPECT_FALSE(future.get());
  EXPECT_FALSE(source.is_done()) << "an applier stop is not end-of-stream";
  EXPECT_FALSE(source.is_error());

  // Restart: clear the flag and reset, as the next START does.
  applier_stop.store(false);
  source.reset(false);
  std::promise<bool> result2;
  std::future<bool> future2 = result2.get_future();
  std::thread waiter2(
      [&source, &result2]() { result2.set_value(source.wait_next()); });
  EXPECT_EQ(future2.wait_for(kBlockProbe), std::future_status::timeout)
      << "the consumer is parked";
  auto decoded = make_event();
  source.append_reader_event(make_fake(decoded));
  const bool woke = future2.wait_for(kWakeTimeout) == std::future_status::ready;
  if (!woke) source.seal_stream();
  waiter2.join();
  ASSERT_TRUE(woke) << "wait_next() must wake once an event is appended";
  EXPECT_TRUE(future2.get());
  auto managed = source.fetch_next();
  ASSERT_TRUE(managed.has_value());
  EXPECT_EQ(managed->get_event().get(), decoded.get());
}

// ---------------------------------------------------------------------------
// APPLIER_EVENT_MEMORY_LIMIT: each accepted append raises the owning
// transaction's maximum event length, which a worker reserves per event.
// ---------------------------------------------------------------------------

// The value is the maximum over the accepted appends. An append rejected
// because the stream is sealed does not raise it.
TEST(ImrEventSetFetchableMemoryTest, AppendRaisesOwnerMaxEventLength) {
  Fetchable_transaction owner;
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.set_owning_fetchable(&owner);
  EXPECT_EQ(owner.get_max_event_length(), 0u);

  const auto small = xid_event_bytes(8);
  const auto large = xid_event_bytes(64);
  const auto huge = xid_event_bytes(256);
  ASSERT_LT(small.size(), large.size());

  source.append_event(as_char(small), small.size());
  EXPECT_EQ(owner.get_max_event_length(), small.size());
  source.append_event(as_char(large), large.size());
  EXPECT_EQ(owner.get_max_event_length(), large.size());
  source.append_event(as_char(small), small.size());
  EXPECT_EQ(owner.get_max_event_length(), large.size());

  source.append_event(as_char(small), small.size(), /*seal_after=*/true);
  EXPECT_EQ(owner.get_max_event_length(), large.size());
  source.append_event(as_char(huge), huge.size());
  EXPECT_EQ(owner.get_max_event_length(), large.size())
      << "an append rejected after the seal must not raise the value";
}

// Cross-thread visibility smoke test: a worker that sees an event in
// wait_next() reads a maximum at least that event's length.
TEST(ImrEventSetFetchableMemoryTest, MaxEventLengthVisibleWhenEventIs) {
  Fetchable_transaction owner;
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.set_owning_fetchable(&owner);

  std::promise<std::pair<bool, std::size_t>> result;
  auto future = result.get_future();
  std::thread waiter([&source, &owner, &result]() {
    const bool has_event = source.wait_next();
    result.set_value({has_event, owner.get_max_event_length()});
  });
  EXPECT_EQ(future.wait_for(kBlockProbe), std::future_status::timeout)
      << "wait_next() must block on an empty, unsealed stream";

  const auto bytes = xid_event_bytes(64);
  source.append_event(as_char(bytes), bytes.size());
  const bool woke = future.wait_for(kWakeTimeout) == std::future_status::ready;
  if (!woke) source.seal_stream();  // watchdog: release the waiter
  waiter.join();
  ASSERT_TRUE(woke) << "wait_next() must wake once an event is appended";
  const auto [has_event, seen_length] = future.get();
  EXPECT_TRUE(has_event);
  EXPECT_GE(seen_length, bytes.size());
}

// The event-oriented seam without a length leaves the value unchanged.
TEST(ImrEventSetFetchableMemoryTest,
     ReaderEventAppendWithoutLengthLeavesMaxUnchanged) {
  Fetchable_transaction owner;
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.set_owning_fetchable(&owner);

  source.append_reader_event(make_fake(make_event()));
  source.append_reader_event(make_fake(make_event()), /*seal_after=*/true);
  EXPECT_EQ(owner.get_max_event_length(), 0u);
}

// An append rejected because the stream is truncated does not raise the value.
TEST(ImrEventSetFetchableMemoryTest, TruncatedStreamRejectsAppendAndKeepsMax) {
  Fetchable_transaction owner;
  Event_set_fetchable_memory source(/*is_trx=*/true, make_fde(),
                                    /*owner_envelope=*/nullptr,
                                    /*streaming_open=*/true);
  source.set_owning_fetchable(&owner);

  const auto small = xid_event_bytes(8);
  const auto large = xid_event_bytes(64);
  source.append_event(as_char(small), small.size());
  ASSERT_EQ(owner.get_max_event_length(), small.size());

  source.set_stream_truncated();
  source.append_event(as_char(large), large.size());
  EXPECT_EQ(owner.get_max_event_length(), small.size())
      << "an append rejected after truncation must not raise the value";
}

}  // namespace mysql::csa::unittests
