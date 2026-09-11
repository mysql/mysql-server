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
/// Unit and randomized-sequence ("property") tests for the FIFO ordering,
/// cursors, and coordinator sweep of mysql::csa::Trx_envelope_queue.
///
/// These tests exercise the ordering side of the queue (enqueue /
/// dispatch_next / sweep_committed and the three monotonic cursors), not the
/// admission side (covered by imr_queue_admission-t.cc). To keep enqueue from
/// blocking in admission the queue is always built with a very large memory
/// limit and every transaction length stays well under both the limit and the
/// spill threshold, so enqueue always takes the (non-blocking, once it fits)
/// MEMORY path.
///
/// In the commit-only model there is no queue/envelope seal step: an envelope
/// is committed directly via Transaction_envelope::commit(), and the
/// coordinator sweep detects the committed head via is_committed().
///
/// The MySQL tree does not integrate a property-testing library (e.g.
/// rapidcheck), so the property tests are expressed as deterministically seeded
/// randomized-sequence generators run over many trials inside plain gtest
/// TESTs. Each seed is fixed and echoed in every assertion message so any
/// failure reproduces exactly.
///
/// Because @c ~Trx_envelope_queue asserts that the deque is empty and
/// @c bytes_used() == 0, every test fully drains the queue (commit every
/// envelope that owns a payload, then sweep the whole committed prefix) before
/// the queue goes out of scope.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <future>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/common/event_set_fetchable.h"
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_memory.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/log_event.h"

namespace mysql::csa::unittests {

namespace {
/// A short bounded wait used to observe that a background dispatch call is (or
/// is not) still blocked, without relying on a fixed sleep for correctness.
constexpr std::chrono::milliseconds kShortWait{50};

/// Build the active FDE that every enqueue() must be handed. enqueue() takes
/// the precise std::shared_ptr<Format_description_log_event> (the exact type
/// Master_info::get_mi_description_event_shared() returns), so no upcast is
/// needed at the call site. These ordering tests never create a byte source, so
/// the FDE is only asserted non-null.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// Memory bounds large enough that enqueue never blocks in admission: the limit
/// dwarfs any total the tests reserve, and the spill threshold dwarfs any
/// single transaction length, so every enqueue takes the MEMORY path. enqueue()
/// itself creates and attaches the empty MEMORY-path destination, reserving
/// each envelope's @c trx_length bytes at enqueue time, so the tests no longer
/// attach payloads by hand.
constexpr std::size_t kMemoryLimit = std::size_t{1} << 30;    // 1 GiB
constexpr std::size_t kSpillThreshold = std::size_t{1} << 20;  // 1 MiB

/// RAII private "relay log directory" for spill-path enqueue tests; removed on
/// scope exit (with the spill files and temp-files subdir under it).
struct Scoped_temp_dir {
  std::string path;
  Scoped_temp_dir() {
    static std::atomic<unsigned> counter{0};
    path = (std::filesystem::temp_directory_path() /
            ("imr_fifo_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1))))
               .string();
    std::filesystem::create_directories(path);
  }
  ~Scoped_temp_dir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

/// Commit and sweep everything still live in @p envs so the queue is empty and
/// @c bytes_used() == 0 before it is destroyed. @p envs must contain only
/// envelopes that have not yet been swept (their pointers are still valid).
void drain_queue(Trx_envelope_queue &queue,
                 const std::vector<Transaction_envelope *> &envs) {
  // An envelope must be dispatched before it can be committed/swept
  // (commit_seqno <= dispatch_seqno). Dispatch anything still undispatched
  // first. The caller must not have stopped the queue (dispatch_next() would
  // return nullptr without advancing); stopped-queue tests use reset() instead.
  while (queue.dispatch_seqno() < queue.insert_seqno()) {
    queue.dispatch_next();
  }
  for (auto *env : envs) {
    if (!env->is_committed()) env->commit();
  }
  while (queue.commit_seqno() < queue.insert_seqno()) {
    if (queue.sweep_committed()) break;  // structural error: avoid a spin.
  }
}

/// A fake encoded event used by the destination-creating enqueue tests below:
/// it hands back a preset, already-decoded Log_event when the byte source asks
/// it to decode(). This lets the test control exactly which Log_event object
/// fetch_next() yields and assert identity/order, keeping the
/// transaction-payload decompression path out of the harness (mirrors
/// imr_integration-t.cc / imr_queued_transaction_reader-t.cc).
class Fake_reader_event : public IReader_event {
 public:
  explicit Fake_reader_event(std::shared_ptr<Log_event> decoded)
      : m_decoded(std::move(decoded)) {}

  std::shared_ptr<Log_event> decode() override { return m_decoded; }

  // These tests never re-read with reset_events=true, so a no-op suffices.
  void reset(const Format_description_log_event *) override {}

 private:
  std::shared_ptr<Log_event> m_decoded;
};

/// A body event served by the stream: a distinct FDE instance, so object
/// identity is meaningful when asserting fetch order.
std::shared_ptr<Log_event> make_event() {
  return std::make_shared<Format_description_log_event>();
}

/// Wrap a decoded Log_event into a fake encoded IReader_event entry.
IReader_event_ptr make_fake(std::shared_ptr<Log_event> decoded) {
  return std::make_shared<Fake_reader_event>(std::move(decoded));
}
}  // namespace

// ---------------------------------------------------------------------------
// Task 5.2 - Unit tests: FIFO enqueue / dispatch / sweep.
// Requirements 3.1, 3.2, 3.5, 3.6, 3.7, 3.8, 6.2, 6.3, 6.4, 6.5, 6.7
// ---------------------------------------------------------------------------

// Req 3.1: a freshly constructed queue has all three cursors at 0.
TEST(ImrQueueFifoTest, CursorsInitializeToZero) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  EXPECT_EQ(queue.commit_seqno(), 0u);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);
  EXPECT_EQ(queue.insert_seqno(), 0u);
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
  // Empty queue: nothing to drain.
}

// Queue length is the number of entries still owned by the queue. Dispatch and
// worker commit leave ownership unchanged; only sweeping a contiguous committed
// head prefix removes entries.
TEST(ImrQueueFifoTest, QueueLengthTracksOwnedEntriesUntilSweep) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  ASSERT_NE(e1, nullptr);
  EXPECT_EQ(queue.queue_length(), 1u);

  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  ASSERT_NE(e2, nullptr);
  Transaction_envelope *e3 = queue.enqueue(33, true, make_fde());
  ASSERT_NE(e3, nullptr);
  EXPECT_EQ(queue.queue_length(), 3u);

  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_next(), e2);
  ASSERT_EQ(queue.dispatch_next(), e3);
  EXPECT_EQ(queue.queue_length(), 3u);

  // A committed entry behind an uncommitted head remains queue-owned.
  ASSERT_FALSE(e2->commit());
  EXPECT_EQ(queue.queue_length(), 3u);
  ASSERT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 3u);

  // Committing does not remove entries. Sweeping removes the now-contiguous
  // committed prefix (#1 and #2), leaving only #3.
  ASSERT_FALSE(e1->commit());
  EXPECT_EQ(queue.queue_length(), 3u);
  ASSERT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 1u);

  ASSERT_FALSE(e3->commit());
  EXPECT_EQ(queue.queue_length(), 1u);
  ASSERT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
}

// Req 3.2: stream_seqno starts at 1 and strictly increases with each enqueue;
// insert_seqno tracks the last assigned stream_seqno.
TEST(ImrQueueFifoTest, StreamSeqnoStartsAtOneAndStrictlyIncreases) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  Transaction_envelope *e3 = queue.enqueue(33, true, make_fde());

  EXPECT_EQ(e1->stream_seqno(), 1u);
  EXPECT_EQ(e2->stream_seqno(), 2u);
  EXPECT_EQ(e3->stream_seqno(), 3u);
  EXPECT_EQ(queue.insert_seqno(), 3u);
  // dispatch/commit untouched by enqueue.
  EXPECT_EQ(queue.dispatch_seqno(), 0u);
  EXPECT_EQ(queue.commit_seqno(), 0u);

  drain_queue(queue, {e1, e2, e3});
}

// Req 3.5: dispatch_next hands back envelopes in stream_seqno order 1..N and
// advances dispatch_seqno by one each time.
TEST(ImrQueueFifoTest, DispatchAdvancesDispatchSeqnoInOrder) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  constexpr int kN = 4;
  std::vector<Transaction_envelope *> envs;
  for (int i = 0; i < kN; ++i)
    envs.push_back(queue.enqueue(100 + i, true, make_fde()));

  for (int i = 0; i < kN; ++i) {
    // Guard: only dispatch while an undispatched envelope exists so the call
    // never blocks in this single-threaded test.
    ASSERT_LT(queue.dispatch_seqno(), queue.insert_seqno());
    Transaction_envelope *env = queue.dispatch_next();
    ASSERT_NE(env, nullptr);
    EXPECT_EQ(env->stream_seqno(), static_cast<std::uint64_t>(i + 1));
    EXPECT_EQ(queue.dispatch_seqno(), static_cast<std::uint64_t>(i + 1));
  }
  EXPECT_EQ(queue.dispatch_seqno(), queue.insert_seqno());

  drain_queue(queue, envs);
}

// Req 3.5: dispatch_next blocks while the queue is fully dispatched (here,
// empty) and returns nullptr once stop() is requested, without advancing any
// cursor. A blocking API forces this to be a thread-based test.
TEST(ImrQueueFifoTest, DispatchBlocksWhenEmptyReturnsNullptrOnStop) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  std::promise<Transaction_envelope *> result_promise;
  std::future<Transaction_envelope *> result_future =
      result_promise.get_future();

  std::thread waiter(
      [&] { result_promise.set_value(queue.dispatch_next()); });

  // Nothing enqueued, so the call must still be blocked.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);

  queue.stop();
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "dispatch_next must return after stop()";
  EXPECT_EQ(result_future.get(), nullptr);

  waiter.join();
  // Stop did not advance any cursor and nothing was ever enqueued.
  EXPECT_EQ(queue.commit_seqno(), 0u);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);
  EXPECT_EQ(queue.insert_seqno(), 0u);
  // Empty queue: nothing to drain.
}

// Req 3.6, 3.7, 3.8, 6.3, 6.4, 6.5: committing out of source order releases each
// envelope's bytes immediately (independent of sweep) and leaves a
// committed-behind-uncommitted-head envelope in the deque; sweep_committed only
// advances commit_seqno over the contiguous committed head prefix.
TEST(ImrQueueFifoTest, OutOfOrderCommitRetainsCommittedBehindHead) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // Distinct lengths so each byte-accounting assertion is unambiguous.
  const std::size_t len1 = 111, len2 = 222, len3 = 333;
  // enqueue() attaches the MEMORY-path payload, reserving trx_length bytes.
  Transaction_envelope *e1 = queue.enqueue(len1, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(len2, true, make_fde());
  Transaction_envelope *e3 = queue.enqueue(len3, true, make_fde());

  ASSERT_EQ(queue.insert_seqno(), 3u);
  ASSERT_EQ(queue.bytes_used(), len1 + len2 + len3);

  // Dispatch all three before committing (commit_seqno <= dispatch_seqno).
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_next(), e2);
  ASSERT_EQ(queue.dispatch_next(), e3);

  // Commit the MIDDLE envelope first. Its bytes are released immediately.
  EXPECT_FALSE(e2->commit());
  EXPECT_EQ(queue.bytes_used(), len1 + len3);

  // Sweep must NOT advance: the head (#1) is not yet committed. #2 stays in the
  // deque behind the uncommitted head.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 0u);
  EXPECT_EQ(queue.insert_seqno(), 3u);

  // Commit the head (#1); its bytes release immediately.
  EXPECT_FALSE(e1->commit());
  EXPECT_EQ(queue.bytes_used(), len3);

  // Now #1 and #2 form a contiguous committed prefix: sweep pops both.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 2u);
  // #3 is still uncommitted and remains.
  EXPECT_EQ(queue.insert_seqno(), 3u);

  // Commit #3, then sweep the rest.
  EXPECT_FALSE(e3->commit());
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 3u);
  EXPECT_EQ(queue.commit_seqno(), queue.insert_seqno());
  EXPECT_EQ(queue.bytes_used(), 0u);
  // Fully drained: destructor invariants hold.
}

// Req 3.6, 6.5: sweep_committed advances commit_seqno only over the contiguous
// committed head and never changes bytes_used() (the worker already released
// the bytes at commit); it returns false on success.
TEST(ImrQueueFifoTest, SweepAdvancesContiguousPrefixWithoutChangingBytes) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  const std::size_t len1 = 500, len2 = 700;
  // enqueue() attaches the MEMORY-path payload, reserving trx_length bytes.
  Transaction_envelope *e1 = queue.enqueue(len1, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(len2, true, make_fde());

  ASSERT_EQ(queue.bytes_used(), len1 + len2);

  // Dispatch both before committing (commit_seqno <= dispatch_seqno).
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_next(), e2);

  // Commit only the head; its bytes are released at commit time.
  ASSERT_FALSE(e1->commit());
  const std::size_t bytes_after_commit = queue.bytes_used();
  EXPECT_EQ(bytes_after_commit, len2);

  // Sweep pops the single committed head and does not touch the byte counter.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.bytes_used(), bytes_after_commit);

  // A second sweep with a non-committed head is a no-op success.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.bytes_used(), bytes_after_commit);

  // Finish: commit #2 and sweep it away.
  ASSERT_FALSE(e2->commit());
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 2u);
  EXPECT_EQ(queue.commit_seqno(), queue.insert_seqno());
}

// Req 6.2, 6.7: once every envelope has committed, bytes_used() is 0 whether or
// not the coordinator has swept them (bytes are reclaimed at commit, not at
// dequeue).
TEST(ImrQueueFifoTest, BytesUsedZeroWhenAllCommittedBeforeSweep) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  const std::size_t len1 = 128, len2 = 256, len3 = 512;
  // enqueue() attaches the MEMORY-path payload, reserving trx_length bytes.
  Transaction_envelope *e1 = queue.enqueue(len1, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(len2, true, make_fde());
  Transaction_envelope *e3 = queue.enqueue(len3, true, make_fde());

  ASSERT_EQ(queue.bytes_used(), len1 + len2 + len3);

  // Dispatch all three before committing (commit_seqno <= dispatch_seqno).
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_next(), e2);
  ASSERT_EQ(queue.dispatch_next(), e3);

  // Commit all three (in an out-of-order sequence), WITHOUT sweeping.
  ASSERT_FALSE(e2->commit());
  ASSERT_FALSE(e3->commit());
  ASSERT_FALSE(e1->commit());

  // All committed: bytes are back to 0 even though nothing has been swept and
  // the deque is still full (commit_seqno has not moved yet).
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(queue.commit_seqno(), 0u);
  EXPECT_EQ(queue.insert_seqno(), 3u);

  // Now sweep the whole committed prefix in one call.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 3u);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// ---------------------------------------------------------------------------
// Task 3.4 - Unit tests: destination-creating enqueue (post-3.3 behavior).
// Requirements 3.2, 3.3, 3.4, 3.5, 3.6, 3.8
// ---------------------------------------------------------------------------

// Req 3.2, 3.3, 3.4: enqueue() on the MEMORY path creates and attaches the
// empty single-batch destination itself. The returned envelope is on the MEMORY
// path, owns a payload, has reserved exactly trx_length bytes AT ENQUEUE TIME
// (the key change from the pre-3.3 behavior where bytes were 0 right after
// enqueue), and exposes the single memory batch as its current sink — the sink
// IS an Event_set_fetchable_memory, and the payload's Fetchable_transaction is
// trx-typed.
TEST(ImrQueueFifoTest, EnqueueCreatesMemoryDestination) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  constexpr std::size_t kLen = 4096;

  Transaction_envelope *env = queue.enqueue(kLen, /*is_trx=*/true, make_fde());
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(env->path(), Envelope_path::MEMORY);

  // enqueue() attached the payload, so it is non-null on return.
  ASSERT_NE(env->payload(), nullptr);

  // Exactly trx_length bytes are reserved at enqueue time (post-3.3 behavior).
  EXPECT_EQ(queue.bytes_used(), kLen);

  // The current sink exists and IS the single Event_set_fetchable_memory batch.
  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);
  EXPECT_NE(dynamic_cast<Event_set_fetchable_memory *>(sink), nullptr)
      << "the single memory batch must be the enqueue-created sink";

  // The wrapped Fetchable_transaction is present and carries is_trx=true.
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  EXPECT_TRUE(fetchable->is_trx());

  // Drain so the queue destructor's empty / bytes_used()==0 invariant holds.
  drain_queue(queue, {env});
}

// Task 6: a transaction larger than the spill threshold is routed to the SPILL
// path — enqueue() returns a non-null envelope with a live spill destination
// (an Event_set_fetchable_spill over a real file), advances stream_seqno, and
// reserves ZERO bytes against the memory counter (Req 3.7, 5.5).
TEST(ImrQueueFifoTest, EnqueueSpillCreatesSpillDestinationNoReservation) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold, relay_dir.path);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // Strictly greater than the spill threshold -> SPILL path.
  const std::size_t kBig = kSpillThreshold + 1;
  Transaction_envelope *env = queue.enqueue(kBig, /*is_trx=*/true, make_fde());
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(env->path(), Envelope_path::SPILL);
  EXPECT_EQ(env->stream_seqno(), 1u);

  // A payload and a reachable spill sink now exist.
  ASSERT_NE(env->payload(), nullptr);
  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);
  auto *spill = dynamic_cast<Event_set_fetchable_spill *>(sink);
  ASSERT_NE(spill, nullptr) << "spill enqueue must expose a spill sink";
  EXPECT_FALSE(spill->is_error()) << spill->get_error_str();
  EXPECT_FALSE(spill->spill_file_name().empty());

  // Req 3.7 / 5.5: the spill path never charges the memory budget.
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(env->payload()->byte_size(), static_cast<std::size_t>(0));

  drain_queue(queue, {env});
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Task 6 demo: a mix of a small (memory) and a >threshold (spill) transaction
// both produce dispatchable envelopes with live sinks, source order is
// preserved, and only the memory transaction charges the byte counter.
TEST(ImrQueueFifoTest, EnqueueMixMemoryAndSpill) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold, relay_dir.path);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  const std::size_t kSmall = 4096;               // memory path
  const std::size_t kBig = kSpillThreshold + 1;  // spill path

  Transaction_envelope *mem = queue.enqueue(kSmall, /*is_trx=*/true, make_fde());
  Transaction_envelope *spl = queue.enqueue(kBig, /*is_trx=*/true, make_fde());
  ASSERT_NE(mem, nullptr);
  ASSERT_NE(spl, nullptr);

  // Path classification and source ordering.
  EXPECT_EQ(mem->path(), Envelope_path::MEMORY);
  EXPECT_EQ(spl->path(), Envelope_path::SPILL);
  EXPECT_EQ(mem->stream_seqno(), 1u);
  EXPECT_EQ(spl->stream_seqno(), 2u);

  // Both have live, path-appropriate sinks.
  ASSERT_NE(mem->current_sink(), nullptr);
  ASSERT_NE(spl->current_sink(), nullptr);
  EXPECT_NE(dynamic_cast<Event_set_fetchable_memory *>(mem->current_sink()),
            nullptr);
  auto *spill = dynamic_cast<Event_set_fetchable_spill *>(spl->current_sink());
  ASSERT_NE(spill, nullptr);
  EXPECT_FALSE(spill->is_error()) << spill->get_error_str();

  // Only the memory transaction charges the byte counter (Req 3.7 / 5.5).
  EXPECT_EQ(queue.bytes_used(), kSmall);

  drain_queue(queue, {mem, spl});
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Req 3.2: the is_trx flag handed to enqueue() propagates through to the
// created destination's Fetchable_transaction (false variant of the test
// above).
TEST(ImrQueueFifoTest, EnqueueIsTrxFalsePropagates) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  constexpr std::size_t kLen = 4096;

  Transaction_envelope *env = queue.enqueue(kLen, /*is_trx=*/false, make_fde());
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(env->path(), Envelope_path::MEMORY);
  ASSERT_NE(env->payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), kLen);

  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);
  EXPECT_NE(dynamic_cast<Event_set_fetchable_memory *>(sink), nullptr);

  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  EXPECT_FALSE(fetchable->is_trx());

  drain_queue(queue, {env});
}

// Req 3.4 (single-batch invariant): the enqueue-created destination is exactly
// ONE completed batch. Stream a few events into the sink, seal the byte stream
// on the terminal event, then drive the fetchable consumer to completion: the
// events come back in append order and the stream reaches a CLEAN end
// (is_fetching_done() true, is_fetching_error() false) — proving there is
// exactly one batch that terminates. create_memory() already sealed the
// (single-batch) metadata stream via set_fetching_complete(), so it is NOT
// called again here.
TEST(ImrQueueFifoTest, EnqueueSingleBatchDrainsCleanly) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  constexpr std::size_t kLen = 4096;

  Transaction_envelope *env = queue.enqueue(kLen, /*is_trx=*/true, make_fde());
  ASSERT_NE(env, nullptr);
  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);
  // The sink is the memory batch; reach the event-oriented injection seam so
  // the test can assert exact object identity on the way out (the byte-oriented
  // append_event decodes fresh events and would lose identity).
  auto *mem_sink = dynamic_cast<Event_set_fetchable_memory *>(sink);
  ASSERT_NE(mem_sink, nullptr);

  // Stream a few events into the still-open byte stream; seal on the last one.
  constexpr int kEventCount = 3;
  std::vector<Log_event *> expected;
  expected.reserve(kEventCount);
  for (int i = 0; i < kEventCount; ++i) {
    auto decoded = make_event();
    expected.push_back(decoded.get());
    const bool last = (i == kEventCount - 1);
    mem_sink->append_reader_event(make_fake(decoded), /*seal_after=*/last);
  }

  // Drive the consumer surface via the enqueue-created Fetchable_transaction.
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  std::vector<Log_event *> fetched;
  fetched.reserve(kEventCount);
  while (fetchable->wait_next()) {
    auto managed = fetchable->fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    fetched.push_back(managed->get_event().get());
  }

  // Events flow back in append order and the single batch ends cleanly.
  EXPECT_EQ(fetched, expected);
  EXPECT_TRUE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());

  drain_queue(queue, {env});
}

// Req 3.6, 3.8: a stop() while a second enqueue is blocked in admission makes
// that enqueue return nullptr WITHOUT side effects — it emplaced no envelope
// (insert_seqno unchanged), created no destination, and reserved no bytes
// (bytes_used() unchanged). A small memory_limit forces the second enqueue to
// block; the spill threshold equals the limit so neither length routes SPILL.
TEST(ImrQueueFifoTest, EnqueueStopWhileBlockedReturnsNullptr) {
  const std::size_t kSmallLimit = 4096;
  // spill_threshold == limit, so a length up to the limit still classifies
  // MEMORY (it is not > threshold) and admission — not spill — is what blocks.
  Trx_envelope_queue queue(kSmallLimit, kSmallLimit);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // First envelope fills the whole budget.
  const std::size_t len1 = kSmallLimit;
  Transaction_envelope *e1 = queue.enqueue(len1, /*is_trx=*/true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_EQ(queue.insert_seqno(), 1u);
  ASSERT_EQ(queue.bytes_used(), len1);

  // Second envelope has no room: MEMORY-classified (<= threshold) but its
  // reservation would exceed the limit, so it must block in acquire_admission.
  const std::size_t len2 = 100;
  std::promise<Transaction_envelope *> result_promise;
  std::future<Transaction_envelope *> result_future =
      result_promise.get_future();
  std::thread waiter([&] {
    result_promise.set_value(queue.enqueue(len2, /*is_trx=*/true, make_fde()));
  });

  // Nothing has been released, so the second enqueue must still be blocked.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);

  // Stop unblocks the parked enqueue; it must abort and return nullptr.
  queue.stop();
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "enqueue must return after stop()";
  EXPECT_EQ(result_future.get(), nullptr);
  waiter.join();

  // The blocked enqueue emplaced nothing, created no destination, reserved no
  // bytes: both cursors and the byte counter are exactly as after the first.
  EXPECT_EQ(queue.insert_seqno(), 1u);
  EXPECT_EQ(queue.bytes_used(), len1);

  // Stopped queue: reset() drops e1 and restores the empty invariant.
  queue.reset();
}

// ---------------------------------------------------------------------------
// Task 5.3 - Property test: cursor monotonicity and ordering.
// Validates: Requirements 3.3, 3.4
// ---------------------------------------------------------------------------

namespace {
/// Independent shadow model of one still-live (enqueued, not-yet-swept)
/// envelope, kept in FIFO order in a deque whose front mirrors the queue head.
struct EnvModel {
  Transaction_envelope *ptr;  ///< Pointer returned by enqueue (valid until swept).
  std::uint64_t stream_seqno;  ///< Expected stream_seqno (1-based).
  std::size_t len;             ///< Reserved bytes (for the final drain check).
  bool committed;              ///< Shadow commit flag.
  bool dispatched;             ///< Whether we have dispatched this envelope.
};
}  // namespace

// Property 3 (Cursor monotonicity and ordering): over a randomized sequence of
// enqueue / dispatch / commit / sweep operations, after EVERY operation the
// invariant commit_seqno <= dispatch_seqno <= insert_seqno holds and each
// cursor is non-decreasing versus its previously observed value. As a stronger
// cross-check, commit_seqno always equals insert_seqno minus the number of
// envelopes still present in the deque (mirrored by the shadow model).
//
// dispatch_next() blocks when the queue is fully dispatched, so DISPATCH is
// guarded to fire only while dispatch_seqno < insert_seqno; the whole sequence
// therefore runs single-threaded without ever blocking.
TEST(ImrQueueFifoTest, PropertyCursorMonotonicity) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0xF1F0C0DEu;
  constexpr int kTrials = 300;
  constexpr int kOpsPerTrial = 48;

  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> op_dist(0, 3);  // 4 operation kinds.
  std::uniform_int_distribution<std::size_t> len_dist(1, 4096);

  enum Op { kEnqueue = 0, kDispatch = 1, kCommit = 2, kSweep = 3 };

  for (int trial = 0; trial < kTrials; ++trial) {
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
    queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
    std::deque<EnvModel> model;  // front mirrors the queue head.
    std::uint64_t next_stream = 0;

    std::uint64_t prev_commit = 0, prev_dispatch = 0, prev_insert = 0;

    for (int step = 0; step < kOpsPerTrial; ++step) {
      switch (op_dist(rng)) {
        case kEnqueue: {
          const std::size_t len = len_dist(rng);
          Transaction_envelope *env = queue.enqueue(len, true, make_fde());
          ASSERT_NE(env, nullptr)
              << "seed=" << kSeed << " trial=" << trial << " step=" << step;
          // enqueue() attaches the MEMORY-path payload, reserving len bytes.
          ++next_stream;
          ASSERT_EQ(env->stream_seqno(), next_stream)
              << "seed=" << kSeed << " trial=" << trial << " step=" << step;
          model.push_back({env, next_stream, len, false, false});
          break;
        }
        case kDispatch: {
          // Guard: dispatch only when an undispatched envelope exists, so the
          // call cannot block.
          if (queue.dispatch_seqno() < queue.insert_seqno()) {
            Transaction_envelope *env = queue.dispatch_next();
            ASSERT_NE(env, nullptr)
                << "seed=" << kSeed << " trial=" << trial << " step=" << step;
            // The next envelope to dispatch is the first not-yet-dispatched one
            // in FIFO order.
            auto it = std::find_if(model.begin(), model.end(),
                                   [](const EnvModel &m) { return !m.dispatched; });
            ASSERT_NE(it, model.end())
                << "seed=" << kSeed << " trial=" << trial << " step=" << step;
            EXPECT_EQ(env, it->ptr)
                << "seed=" << kSeed << " trial=" << trial << " step=" << step;
            EXPECT_EQ(env->stream_seqno(), it->stream_seqno)
                << "seed=" << kSeed << " trial=" << trial << " step=" << step;
            it->dispatched = true;
          }
          break;
        }
        case kCommit: {
          // Commit the first dispatched-but-uncommitted envelope directly (no
          // seal step in the commit-only model).
          auto it = std::find_if(model.begin(), model.end(), [](const EnvModel &m) {
            return m.dispatched && !m.committed;
          });
          if (it != model.end()) {
            ASSERT_FALSE(it->ptr->commit())
                << "seed=" << kSeed << " trial=" << trial << " step=" << step;
            it->committed = true;
          }
          break;
        }
        case kSweep: {
          ASSERT_FALSE(queue.sweep_committed())
              << "seed=" << kSeed << " trial=" << trial << " step=" << step;
          // Mirror the sweep on the model: pop the contiguous committed prefix.
          while (!model.empty() && model.front().committed) {
            model.pop_front();
          }
          break;
        }
        default:
          FAIL() << "unreachable op";
      }

      // Invariant + monotonicity checks after EVERY operation.
      const std::uint64_t c = queue.commit_seqno();
      const std::uint64_t d = queue.dispatch_seqno();
      const std::uint64_t i = queue.insert_seqno();

      ASSERT_LE(c, d) << "seed=" << kSeed << " trial=" << trial
                      << " step=" << step << " (commit>dispatch)";
      ASSERT_LE(d, i) << "seed=" << kSeed << " trial=" << trial
                      << " step=" << step << " (dispatch>insert)";
      ASSERT_GE(c, prev_commit) << "seed=" << kSeed << " trial=" << trial
                                << " step=" << step << " (commit regressed)";
      ASSERT_GE(d, prev_dispatch) << "seed=" << kSeed << " trial=" << trial
                                  << " step=" << step << " (dispatch regressed)";
      ASSERT_GE(i, prev_insert) << "seed=" << kSeed << " trial=" << trial
                                << " step=" << step << " (insert regressed)";
      // Strong cross-check: swept count == enqueued - still-present, and the
      // encapsulated queue length matches the independently maintained model.
      ASSERT_EQ(c, i - static_cast<std::uint64_t>(model.size()))
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;
      ASSERT_EQ(queue.queue_length(), model.size())
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;

      prev_commit = c;
      prev_dispatch = d;
      prev_insert = i;
    }

    // Dispatch anything still undispatched before committing (commit <= dispatch).
    while (queue.dispatch_seqno() < queue.insert_seqno()) {
      ASSERT_NE(queue.dispatch_next(), nullptr)
          << "seed=" << kSeed << " trial=" << trial << " (drain dispatch)";
    }

    // Drain fully: commit every remaining envelope, then sweep it all.
    for (EnvModel &m : model) {
      if (!m.committed) {
        ASSERT_FALSE(m.ptr->commit())
            << "seed=" << kSeed << " trial=" << trial << " (drain commit)";
        m.committed = true;
      }
    }
    while (queue.commit_seqno() < queue.insert_seqno()) {
      ASSERT_FALSE(queue.sweep_committed())
          << "seed=" << kSeed << " trial=" << trial << " (drain sweep)";
    }
    ASSERT_EQ(queue.bytes_used(), 0u)
        << "seed=" << kSeed << " trial=" << trial << " (drained)";
  }
}

// ---------------------------------------------------------------------------
// Task 5.4 - Property test: FIFO dispatch and sweep.
// Validates: Requirements 3.5, 3.6, 6.3, 6.4
// ---------------------------------------------------------------------------

// Property 4 (FIFO dispatch and sweep): a batch is enqueued, then dispatched
// (which must return the envelopes in non-decreasing stream_seqno order
// 1..N == dispatch order), then committed in a RANDOM order
// (replica_preserve_commit_order=OFF semantics). After each commit + sweep,
// commit_seqno only advances over the contiguous committed prefix: a committed
// envelope behind an uncommitted head is retained until the head commits. An
// independent model of which stream_seqnos are committed predicts exactly how
// far commit_seqno advances and what bytes_used() should be. After all commits
// and the final sweep, commit_seqno == insert_seqno and bytes_used() == 0.
TEST(ImrQueueFifoTest, PropertyFifoDispatchAndSweep) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0xD15A7C4Fu;
  constexpr int kTrials = 400;

  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> batch_dist(1, 8);
  std::uniform_int_distribution<std::size_t> len_dist(1, 4096);

  for (int trial = 0; trial < kTrials; ++trial) {
    const int n = batch_dist(rng);
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
    queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

    // Per-stream_seqno bookkeeping (indexed 1..n). Index 0 is unused.
    std::vector<Transaction_envelope *> envs(n + 1, nullptr);
    std::vector<std::size_t> lens(n + 1, 0);
    std::vector<bool> committed(n + 1, false);

    // Enqueue the batch and attach a payload to each envelope.
    std::size_t total_bytes = 0;
    for (int s = 1; s <= n; ++s) {
      const std::size_t len = len_dist(rng);
      Transaction_envelope *env = queue.enqueue(len, true, make_fde());
      ASSERT_NE(env, nullptr)
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;
      // enqueue() attaches the MEMORY-path payload, reserving len bytes.
      envs[s] = env;
      lens[s] = len;
      total_bytes += len;
    }
    ASSERT_EQ(queue.insert_seqno(), static_cast<std::uint64_t>(n))
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_EQ(queue.bytes_used(), total_bytes)
        << "seed=" << kSeed << " trial=" << trial;

    // Dispatch the whole batch: it must come back in stream_seqno order 1..n.
    for (int s = 1; s <= n; ++s) {
      ASSERT_LT(queue.dispatch_seqno(), queue.insert_seqno())
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;
      Transaction_envelope *env = queue.dispatch_next();
      ASSERT_NE(env, nullptr)
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;
      EXPECT_EQ(env->stream_seqno(), static_cast<std::uint64_t>(s))
          << "seed=" << kSeed << " trial=" << trial << " s=" << s
          << " (dispatch out of FIFO order)";
      EXPECT_EQ(env, envs[s])
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;
      EXPECT_EQ(queue.dispatch_seqno(), static_cast<std::uint64_t>(s))
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;
    }
    ASSERT_EQ(queue.dispatch_seqno(), queue.insert_seqno())
        << "seed=" << kSeed << " trial=" << trial;

    // Commit the batch in a random order.
    std::vector<int> order(n);
    for (int k = 0; k < n; ++k) order[k] = k + 1;
    std::shuffle(order.begin(), order.end(), rng);

    std::size_t expected_bytes = total_bytes;
    for (int k = 0; k < n; ++k) {
      const int s = order[k];
      // Commit releases this envelope's bytes immediately (before any sweep).
      ASSERT_FALSE(envs[s]->commit())
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;
      committed[s] = true;
      expected_bytes -= lens[s];
      ASSERT_EQ(queue.bytes_used(), expected_bytes)
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;

      // Sweep advances commit_seqno only over the contiguous committed prefix.
      ASSERT_FALSE(queue.sweep_committed())
          << "seed=" << kSeed << " trial=" << trial << " s=" << s;
      std::uint64_t expected_commit = 0;
      while (expected_commit < static_cast<std::uint64_t>(n) &&
             committed[expected_commit + 1]) {
        ++expected_commit;
      }
      ASSERT_EQ(queue.commit_seqno(), expected_commit)
          << "seed=" << kSeed << " trial=" << trial << " s=" << s
          << " (commit_seqno past the contiguous committed prefix)";
    }

    // Everything committed and swept: cursors meet and no bytes remain.
    ASSERT_EQ(queue.commit_seqno(), queue.insert_seqno())
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_EQ(queue.commit_seqno(), static_cast<std::uint64_t>(n))
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_EQ(queue.bytes_used(), 0u)
        << "seed=" << kSeed << " trial=" << trial;
  }
}

// ---------------------------------------------------------------------------
// Task 3.5 - Property test: set-before-observable and single-batch.
// Property 1: R1 — current-sink set-before-observable
// Property 7: R7 — single-batch preserved by the receiver
// Validates: Requirements 3.1, 3.2, 3.4
// ---------------------------------------------------------------------------

// Over a randomized sequence of enqueue / commit / sweep operations, every
// envelope the test can observe from the pointer enqueue() returns already
// carries its destination and sink (R1: set-before-observable) — this holds
// structurally single-threaded because enqueue() creates and attaches the
// MEMORY-path destination (payload + Event_set_fetchable_memory sink) under
// m_queue_mutex before it returns/notifies, so there is no window in which a
// returned envelope lacks a payload/sink. Each observed envelope wraps exactly
// ONE memory batch (R7): the current sink IS the Event_set_fetchable_memory,
// the wrapped Fetchable_transaction round-trips the is_trx flag, and a subset
// of envelopes is driven through a single-batch drain that reaches a CLEAN end
// (is_fetching_done() && !is_fetching_error()) — a second batch would keep the
// stream from terminating. bytes_used() never exceeds memory_limit at any
// admission point (checked after every enqueue, commit, and sweep).
//
// memory_limit comfortably holds every envelope that can be live at once
// (kOpsPerTrial * kMaxLen, doubled for headroom), so a single-threaded enqueue
// never blocks in admission (no deadlock); commit/sweep are still exercised to
// drive byte release. Every trx_length stays <= the spill threshold so
// classify() always picks MEMORY. NO fixed sleeps.
TEST(ImrQueueFifoTest, PropertySetBeforeObservableAndSingleBatch) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0x5E7B4C0Eu;
  constexpr int kTrials = 200;
  constexpr int kOpsPerTrial = 40;

  constexpr std::size_t kMinLen = 1;
  constexpr std::size_t kMaxLen = 4096;
  // Comfortably holds every envelope that can be live simultaneously, so a
  // single-threaded enqueue never parks in admission (would deadlock).
  constexpr std::size_t kMemLimit =
      static_cast<std::size_t>(kOpsPerTrial) * kMaxLen * 2;
  // Every trx_length (<= kMaxLen) is <= the spill threshold, so classify()
  // always routes MEMORY and never SPILL.
  constexpr std::size_t kSpill = kMaxLen;

  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> op_dist(0, 9);  // enqueue-weighted below.
  std::uniform_int_distribution<std::size_t> len_dist(kMinLen, kMaxLen);
  std::uniform_int_distribution<int> bool_dist(0, 1);
  std::uniform_int_distribution<int> drive_dist(0, 3);  // ~1 in 4 gets drained.

  for (int trial = 0; trial < kTrials; ++trial) {
    Trx_envelope_queue queue(kMemLimit, kSpill);
    queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
    std::deque<EnvModel> model;  // front mirrors the queue head; live only.
    std::uint64_t next_stream = 0;

    for (int step = 0; step < kOpsPerTrial; ++step) {
      // Enqueue on 0..5 (60%), commit on 6..7, sweep on 8..9: mixing occasional
      // commit/sweep in keeps memory moving without ever blocking admission.
      const int op = op_dist(rng);
      if (op <= 5) {
        const std::size_t len = len_dist(rng);
        const bool is_trx = (bool_dist(rng) == 1);
        Transaction_envelope *env = queue.enqueue(len, is_trx, make_fde());
        ASSERT_NE(env, nullptr)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step;
        ++next_stream;
        ASSERT_EQ(env->stream_seqno(), next_stream)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step;

        // R1 (set-before-observable): the destination + sink are already
        // present the moment the envelope is observable from the returned
        // pointer — no observable window without a payload/sink.
        ASSERT_NE(env->payload(), nullptr)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step
            << " (payload missing on observable envelope)";
        Streaming_event_sink *sink = env->current_sink();
        ASSERT_NE(sink, nullptr)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step
            << " (sink missing on observable envelope)";

        // R7 (single memory batch): the current sink IS the memory batch.
        ASSERT_NE(dynamic_cast<Event_set_fetchable_memory *>(sink), nullptr)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step
            << " (current sink is not the memory batch)";

        // The wrapped Fetchable_transaction exists and round-trips is_trx.
        auto fetchable = env->payload()->fetchable();
        ASSERT_NE(fetchable, nullptr)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step;
        ASSERT_EQ(fetchable->is_trx(), is_trx)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step
            << " (is_trx did not round-trip)";

        // Drive a single-batch drain on a subset: stream one sealed event via
        // the current sink, then consume to completion. A CLEAN end proves
        // there is exactly ONE batch (a second batch would keep the stream from
        // terminating). Kept on ~1/4 of envelopes to stay fast.
        if (drive_dist(rng) == 0) {
          dynamic_cast<Event_set_fetchable_memory *>(sink)->append_reader_event(
              make_fake(make_event()), /*seal_after=*/true);
          while (fetchable->wait_next()) {
            auto managed = fetchable->fetch_next();
            ASSERT_TRUE(managed.has_value())
                << "seed=" << kSeed << " trial=" << trial << " step=" << step
                << " (wait_next true but fetch_next empty)";
          }
          EXPECT_TRUE(fetchable->is_fetching_done())
              << "seed=" << kSeed << " trial=" << trial << " step=" << step
              << " (single batch did not reach a clean end)";
          EXPECT_FALSE(fetchable->is_fetching_error())
              << "seed=" << kSeed << " trial=" << trial << " step=" << step
              << " (single batch ended in error)";
        }

        // This suite exercises set-before-observable and single-batch, not
        // dispatch ordering, but an envelope must be dispatched before commit
        // (commit_seqno <= dispatch_seqno). Dispatch it immediately.
        ASSERT_EQ(queue.dispatch_next(), env)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step;
        model.push_back({env, next_stream, len, false, false});
      } else if (op <= 7) {
        // Commit a random still-live, uncommitted envelope (out-of-order commit
        // is allowed); its bytes release immediately.
        std::vector<std::size_t> uncommitted;
        for (std::size_t k = 0; k < model.size(); ++k) {
          if (!model[k].committed) uncommitted.push_back(k);
        }
        if (!uncommitted.empty()) {
          std::uniform_int_distribution<std::size_t> pick(
              0, uncommitted.size() - 1);
          EnvModel &m = model[uncommitted[pick(rng)]];
          ASSERT_FALSE(m.ptr->commit())
              << "seed=" << kSeed << " trial=" << trial << " step=" << step;
          m.committed = true;
        }
      } else {
        // Sweep: pop the contiguous committed head prefix from both the queue
        // and the shadow model.
        ASSERT_FALSE(queue.sweep_committed())
            << "seed=" << kSeed << " trial=" << trial << " step=" << step;
        while (!model.empty() && model.front().committed) {
          model.pop_front();
        }
      }

      // Memory bound: bytes_used() never exceeds the limit at any point.
      ASSERT_LE(queue.bytes_used(), kMemLimit)
          << "seed=" << kSeed << " trial=" << trial << " step=" << step
          << " (bytes_used exceeded memory_limit)";
    }

    // Fully drain: commit every still-live envelope, then sweep the whole
    // committed prefix. Only the not-yet-swept envelopes remain in the model,
    // so their pointers are valid; commit-before-sweep is honored by
    // drain_queue.
    std::vector<Transaction_envelope *> live;
    live.reserve(model.size());
    for (const EnvModel &m : model) live.push_back(m.ptr);
    drain_queue(queue, live);
    ASSERT_EQ(queue.bytes_used(), 0u)
        << "seed=" << kSeed << " trial=" << trial << " (drained)";
    ASSERT_EQ(queue.commit_seqno(), queue.insert_seqno())
        << "seed=" << kSeed << " trial=" << trial << " (drained)";
  }
}

// ---------------------------------------------------------------------------
// Restart re-dispatch: sweep_and_dispatch() re-serves uncommitted envelopes in
// source order and SKIPS any that committed out of source order in the prior
// session (reset payload).
// ---------------------------------------------------------------------------

// sweep_and_dispatch() (the coordinator entry point) skips an envelope that
// committed out of source order (reset payload) and returns the next
// uncommitted one, advancing the dispatch cursor past the skipped one. The skip
// is localized in the queue here (not the reader), so a committed envelope is
// never handed to the coordinator payload-less.
TEST(ImrQueueFifoTest, SweepAndDispatchSkipsCommittedOutOfOrder) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  Transaction_envelope *e3 = queue.enqueue(33, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);
  ASSERT_NE(e3, nullptr);

  // e1 dispatched (head, uncommitted); e2 committed out of order behind it, so
  // its payload is reset to null. The dispatch cursor now points at e2.
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_FALSE(e2->commit());
  ASSERT_EQ(e2->payload(), nullptr);
  ASSERT_EQ(queue.dispatch_seqno(), 1u);

  // sweep_and_dispatch() skips the committed e2 and returns e3, advancing the
  // dispatch cursor past BOTH.
  Transaction_envelope *got = queue.sweep_and_dispatch();
  ASSERT_EQ(got, e3);
  EXPECT_NE(got->payload(), nullptr) << "a dispatched envelope must have a payload";
  EXPECT_EQ(queue.dispatch_seqno(), 3u);

  // Drain: commit the still-uncommitted e1 and e3 (e2 already committed), then
  // sweep the whole prefix so the queue dtor's empty/bytes==0 invariant holds.
  ASSERT_FALSE(e1->commit());
  ASSERT_FALSE(e3->commit());
  ASSERT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// NOTE: dispatch_next() itself does NOT skip already-committed envelopes -- it
// returns whatever sits at dispatch_seqno and advances the cursor. The skip of
// out-of-order-committed envelopes on restart is done by sweep_and_dispatch()
// (the coordinator entry point, tested just above), so the low-level
// dispatch_next() primitive stays lock-simple. The end-to-end skip through the
// reader is covered in imr_queued_transaction_reader-t.cc.

// NOTE: sweep_committed()'s guard never advancing commit_seqno past
// dispatch_seqno is covered in imr_queue_lifecycle-t.cc
// (SweepDoesNotAdvanceCommitPastDispatch).

}  // namespace mysql::csa::unittests
