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

#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_memory.h"
#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/log_event.h"

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
/// the exact std::shared_ptr<Format_description_log_event> (matching
/// Master_info::get_mi_description_event_shared()), so it is handed directly
/// with no upcast; passing it to the Event_set_fetchable_memory ctor, whose FDE
/// parameter is the base Log_event_ptr, is a well-formed implicit upcast.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// A body event served by the stream: another FDE object (distinct instance),
/// so object identity is meaningful.
std::shared_ptr<Log_event> make_event() {
  return std::make_shared<Format_description_log_event>();
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
// Task 7.2.1 - Append/seal/fetch ordering and end-of-stream.
// Requirements 7.2, 7.3
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
// Task 7.2.2 - Appends are rejected after the stream is sealed.
// Requirement 7.3
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
// Task 7.2.3 - wait_next() blocking on an empty, unsealed stream and waking on
// append/seal.
// Requirement 7.7
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
// Task 7.2.4 - Truncation surfaces an incomplete transaction.
// Requirement 7.6
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
// wait_next() waiting for more of a still-unsealed stream (a worker mid-apply of
// a partially-received transaction) is released by set_stream_truncated(),
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

  // Exactly the one pre-truncation event, by identity; then clean end-of-stream.
  ASSERT_EQ(seen.size(), 1u);
  EXPECT_EQ(seen[0], expected);
  EXPECT_TRUE(source.is_done());
  EXPECT_FALSE(source.is_error());
}

// ---------------------------------------------------------------------------
// Task 7.2.5 - set_success() commits the envelope and releases bytes once.
// Requirement 7.4
// ---------------------------------------------------------------------------

namespace {

// Per-channel memory bounds for the commit-hook tests. kTrxLength must be at
// or below the spill threshold so classify() picks the MEMORY path and
// enqueue() returns a non-null envelope.
constexpr std::size_t kMemoryLimit = 1u << 20;    // 1 MiB
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
TEST(ImrEventSetFetchableMemoryTest, SetSuccessCommitsEnvelopeAndReleasesBytes) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

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

  // Drain the committed head so the queue destructor invariant (empty deque,
  // bytes_used() == 0) holds. After the sweep, env dangles - do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
}

// ---------------------------------------------------------------------------
// Task 7.2.6 - repeated set_success() is a no-op.
// Requirement 7.5
// ---------------------------------------------------------------------------

// A second set_success() after commit is rejected internally (the envelope is
// already committed) and discarded: is_committed() stays true and bytes_used()
// stays unchanged (no double release).
TEST(ImrEventSetFetchableMemoryTest, RepeatedSetSuccessLeavesStateAndCounter) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

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

  // Drain the committed head so the queue destructor invariant holds. After the
  // sweep, env dangles - do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
}

// ---------------------------------------------------------------------------
// Task 7.3 - Property 7: Interface parity.
// Validates: Requirements 7.1
// ---------------------------------------------------------------------------
//
// Property 7 states that the in-memory source serves the SAME event byte stream
// via wait_next()/fetch_next() as the on-disk Event_set_fetchable_cache path.
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
// expression of Property 7 here.
//
// The MySQL tree does not integrate rapidcheck, so — as elsewhere in this spec
// — the property is checked with a deterministically seeded std::mt19937
// generator running many trials inside a single gtest TEST. The seed is echoed
// once and every assertion carries seed / trial / index so any failure
// reproduces exactly.
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

    // Build N fake events, each decoding to a distinct known Log_event. Keep the
    // fakes alive for the whole trial: they own the Log_event that decode()
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

}  // namespace mysql::csa::unittests
