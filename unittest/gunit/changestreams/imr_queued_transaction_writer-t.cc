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
/// Unit tests for the minimal-state receiver-side
/// mysql::csa::queued_transaction_writer mechanism. The three streaming
/// operations the receiver drives — open_transaction (at the GTID event),
/// append_transaction_event (per body / terminal event), and
/// truncate_transaction (on an incomplete group) — are exercised against a REAL
/// Trx_envelope_queue plus a LOCAL `Open_group group` (the held envelope
/// and its sink) — deliberately NO Master_info — which is exactly how the
/// mechanism is parameterized so it can be tested without receiver coupling.
///
/// Unlike the sibling byte-source tests (which feed fake IReader_events whose
/// decode() returns a preset Log_event), these tests drive the REAL decode
/// path: append_transaction_event forwards the transient bytes to the sink,
/// which copies them into an internal Cached_event_memory whose decode() runs
/// binlog_event_deserialize when the consumer drains the sink. To
/// keep decode() valid and self-contained, each "event" is a genuinely
/// serialized Format_description_log_event: FDE bytes are self-describing for
/// the checksum algorithm (binlog_event_deserialize reads the alg from the FDE
/// body itself, not from the caller's fde), and a default server FDE serializes
/// with checksum OFF, so the round-trip needs no THD and no checksum plumbing.
/// Draining every appended event also decodes it, transferring buffer ownership
/// to the produced Log_event (freed via my_free) — the exact ownership handoff
/// the byte-copy is designed to match, with no leak / double-free.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "my_dbug.h"            // DBUG_SET
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // ensure_spill_directory
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {

// Generous per-channel bounds so enqueue() always takes the MEMORY path and
// never blocks in acquire_admission(). kTrxLength must be at or below the spill
// threshold so classify() picks MEMORY.
constexpr std::size_t kMemoryLimit = 1u << 20;     // 1 MiB
constexpr std::size_t kSpillThreshold = 1u << 16;  // 64 KiB
constexpr std::size_t kTrxLength = 4096;

/// Build the active FDE handed to the ops. The exact type matches
/// Imr_runtime::m_fde_snapshot, which the real receiver hands to them.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// Arm both roles, as an attached receiver and applier do.
void resume_both_roles(Trx_envelope_queue &queue) {
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  queue.resume(Trx_envelope_queue::Scope::APPLIER);
}

/// Serialize a real Format_description_log_event into a byte buffer, mimicking
/// the transient network bytes the receiver would hand to append_event. A
/// default server FDE writes with checksum OFF and is self-describing on
/// decode, so the round-trip through Cached_event_payload::decode() succeeds
/// without a THD or checksum plumbing.
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

/// Whether the receiver's open group is closed (both fields null).
bool is_closed(const Open_group &group) {
  return group.envelope == nullptr && group.sink == nullptr;
}

/// A transaction length strictly above the spill threshold, so classify()
/// routes it to the SPILL path.
constexpr std::size_t kSpillTrxLength = kSpillThreshold + 1;

/// RAII private "relay log directory" for the spill-path tests; removed on
/// scope exit (with the spill files and temp-files subdir under it).
struct Scoped_temp_dir {
  std::string path;
  Scoped_temp_dir() {
    static std::atomic<unsigned> counter{0};
    path = (std::filesystem::temp_directory_path() /
            ("imr_writer_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1))))
               .string();
    std::filesystem::create_directories(path);
    // The spill-files subdirectory is provisioned once at server start by
    // ensure_spill_directory() (from init_replica). This harness bypasses
    // server startup, so provision it here through the same entry point.
    (void)ensure_spill_directory(path);
  }
  ~Scoped_temp_dir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

/// Drain a Fetchable_transaction's consumer surface to completion, returning
/// the number of events served. Every drained event is decoded (its owning
/// buffer handed to the Log_event and freed), so this also proves the
/// byte-copy ownership handoff is clean.
int drain(const std::shared_ptr<Fetchable_transaction> &fetchable) {
  int count = 0;
  while (fetchable->wait_next()) {
    auto managed = fetchable->fetch_next();
    EXPECT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    if (!managed.has_value()) break;
    ++count;
  }
  return count;
}

}  // namespace

// open: open_transaction admits a memory-path transaction and publishes the
// enqueue-created destination's sink through the open group, with the
// envelope held for the receiver.
TEST(QueuedTransactionWriterTest, OpenGroupPublishesSink) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  ASSERT_NE(group.sink, nullptr);
  // enqueue() reserved exactly kTrxLength bytes for the memory-path payload.
  EXPECT_EQ(queue.bytes_used(), kTrxLength);

  // Seal the (empty) stream so the consumer terminates cleanly, then commit and
  // sweep so the queue destructor invariant (empty deque, bytes_used()==0)
  // holds. The raw seal bypasses the helpers, so close the group as
  // close_group() does: clear it, then release the receiver hold last.
  Transaction_envelope *held = group.envelope;
  group.sink->seal_stream();
  group = {};
  held->release_receiver();
  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  EXPECT_EQ(drain(fetchable), 0);
  fetchable->set_success();
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
}

// open-after-stop: once the queue is stopped, open_transaction reports the
// queuing-error indication (true) and leaves the group closed. No envelope is
// created, so the queue destructor invariant holds untouched.
TEST(QueuedTransactionWriterTest,
     OpenGroupAfterStopReportsErrorAndLeavesSinkNull) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  queue.stop(Trx_envelope_queue::Scope::RECEIVER);

  Open_group group;
  Receiver_write_failure f;
  EXPECT_TRUE(open_transaction(queue, group, make_fde(), kTrxLength,
                               /*is_trx=*/true, &f));
  EXPECT_TRUE(is_closed(group));
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_TRUE(f.stopped);
  EXPECT_TRUE(f.reason.empty());
}

// A receiver stop while open_transaction() waits in admission ends the
// wait and records the stop, not a write failure.
TEST(QueuedTransactionWriterTest, StopWhileBlockedInAdmissionFillsStopped) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  queue.add_bytes(kMemoryLimit);  // Full: the admission below waits.

  Open_group group;
  Receiver_write_failure f;
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();
  std::thread receiver([&] {
    result_promise.set_value(open_transaction(queue, group, make_fde(),
                                              kTrxLength, /*is_trx=*/true, &f));
  });

  EXPECT_EQ(result_future.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout);
  queue.stop(Trx_envelope_queue::Scope::RECEIVER);
  const bool woke = result_future.wait_for(std::chrono::seconds(3)) ==
                    std::future_status::ready;
  EXPECT_TRUE(woke) << "the receiver stop did not end the admission wait";
  // Free the budget before join() so a waiter that missed the stop returns.
  queue.release_bytes(kMemoryLimit);
  receiver.join();
  EXPECT_TRUE(result_future.get());

  EXPECT_TRUE(f.stopped);
  EXPECT_TRUE(f.reason.empty());
  EXPECT_TRUE(is_closed(group));
}

// open-while-open: a new GTID event while a group is still open (the previous
// transaction is incomplete, e.g. after a reconnect) truncates that group
// before opening the new one, as the disk-path reader truncates its active
// transaction on a new GTID. The old consumer observes truncation (so its
// worker rolls back instead of waiting forever); the new group is independent
// and completes normally.
TEST(QueuedTransactionWriterTest, OpenGroupWhileOpenTruncatesPreviousGroup) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Streaming_event_sink *first_sink = group.sink;
  ASSERT_NE(first_sink, nullptr);
  const auto e1 = serialize_event();
  EXPECT_FALSE(append_transaction_event(group, as_char(e1), e1.size(),
                                        /*is_terminal=*/false));

  // The next GTID arrives with the first group still open.
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  ASSERT_NE(group.sink, nullptr);
  EXPECT_NE(group.sink, first_sink);

  // The first envelope was truncated: its consumer stops at once.
  Transaction_envelope *first = queue.dispatch_next();
  ASSERT_NE(first, nullptr);
  EXPECT_TRUE(first->is_truncated());
  auto first_fetchable = first->payload()->fetchable();
  ASSERT_NE(first_fetchable, nullptr);
  EXPECT_EQ(drain(first_fetchable), 0);
  EXPECT_TRUE(first_fetchable->is_truncated());

  // The second group is untouched and completes normally.
  const auto e2 = serialize_event();
  EXPECT_FALSE(append_transaction_event(group, as_char(e2), e2.size(),
                                        /*is_terminal=*/true));
  EXPECT_TRUE(is_closed(group));
  Transaction_envelope *second = queue.dispatch_next();
  ASSERT_NE(second, nullptr);
  EXPECT_FALSE(second->is_truncated());
  auto second_fetchable = second->payload()->fetchable();
  ASSERT_NE(second_fetchable, nullptr);
  EXPECT_EQ(drain(second_fetchable), 1);
  EXPECT_TRUE(second_fetchable->is_fetching_done());

  // Commit the second; the sweep reclaims the truncated head and then it.
  second_fetchable->set_success();
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(queue.queue_length(), 0u);
}

// append order + terminal seal: after opening, appending a couple of
// non-terminal events then a terminal one seals the stream exactly once, closes
// the group, and the consumer observes the events in order followed
// by a clean end-of-stream.
TEST(QueuedTransactionWriterTest, AppendOrderAndTerminalSeal) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  ASSERT_NE(group.sink, nullptr);

  const auto e1 = serialize_event();
  const auto e2 = serialize_event();
  const auto e3 = serialize_event();

  EXPECT_FALSE(append_transaction_event(group, as_char(e1), e1.size(),
                                        /*is_terminal=*/false));
  EXPECT_NE(group.sink, nullptr);
  EXPECT_FALSE(append_transaction_event(group, as_char(e2), e2.size(),
                                        /*is_terminal=*/false));
  EXPECT_NE(group.sink, nullptr);
  // Terminal append seals the stream and clears the sink.
  EXPECT_FALSE(append_transaction_event(group, as_char(e3), e3.size(),
                                        /*is_terminal=*/true));
  EXPECT_TRUE(is_closed(group));

  // Drive the consumer: all three events come back, then a clean end.
  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  EXPECT_EQ(drain(fetchable), 3);
  EXPECT_TRUE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());

  // Commit + sweep so the queue destructor invariant holds.
  fetchable->set_success();
  EXPECT_TRUE(env->is_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
}

// atomic-DDL single event: a group opened by open_transaction and terminated by
// a SINGLE terminal append (the sole body-and-terminal event, mirroring an
// atomic-DDL Query_log_event) seals the stream exactly once and closes
// the group. The consumer drains exactly one event, then reaches a clean
// end-of-stream — one event both opened and terminated the group.
TEST(QueuedTransactionWriterTest, AtomicDdlSingleEventOpensAndSealsOnce) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  ASSERT_NE(group.sink, nullptr);

  // The sole event is both the body and the terminal event: appending it seals
  // the stream and clears the sink in one step.
  const auto e = serialize_event();
  EXPECT_FALSE(append_transaction_event(group, as_char(e), e.size(),
                                        /*is_terminal=*/true));
  EXPECT_TRUE(is_closed(group));

  // Drive the consumer: exactly one event comes back, then a clean end.
  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  EXPECT_EQ(drain(fetchable), 1);
  EXPECT_TRUE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());

  // Commit + sweep so the queue destructor invariant holds.
  fetchable->set_success();
  EXPECT_TRUE(env->is_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
}

// append defensive: append_event with a closed group is a no-op that
// returns true (error).
TEST(QueuedTransactionWriterTest, AppendWithNullSinkReturnsError) {
  Open_group group;
  Receiver_write_failure f;
  const auto e = serialize_event();
  EXPECT_TRUE(append_transaction_event(group, as_char(e), e.size(),
                                       /*is_terminal=*/false, &f));
  EXPECT_TRUE(is_closed(group));
  EXPECT_FALSE(f.stopped);
  EXPECT_EQ(f.reason,
            "[In-memory relay log] no open transaction group for the event");
}

// truncate: after opening and appending one event, truncate_transaction marks
// the transaction truncated at BOTH levels -- the batch stream AND the owning
// Fetchable_transaction -- and closes the group. The consumer therefore
// stops immediately: it delivers no buffered partial events and surfaces
// truncation (is_truncated), NOT a clean done and NOT an error. That is what
// drives the worker's is_truncated() rollback-and-replay branch and prevents a
// buffered event from being mis-treated as the transaction's terminal event. A
// second truncate_transaction on a closed group is a no-op.
TEST(QueuedTransactionWriterTest,
     TruncateClearsSinkAndConsumerObservesTruncation) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  ASSERT_NE(group.sink, nullptr);

  const auto e1 = serialize_event();
  EXPECT_FALSE(append_transaction_event(group, as_char(e1), e1.size(),
                                        /*is_terminal=*/false));

  // Truncate the still-open group: batch + transaction marked truncated, sink
  // cleared.
  truncate_transaction(group);
  EXPECT_TRUE(is_closed(group));

  // truncate_transaction on a null sink is a no-op (no crash, stays null).
  truncate_transaction(group);
  EXPECT_TRUE(is_closed(group));

  // The consumer stops immediately on truncation: no buffered event is
  // delivered, and it surfaces truncation rather than a clean done.
  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  EXPECT_EQ(drain(fetchable), 0);
  EXPECT_TRUE(fetchable->is_truncated());
  EXPECT_FALSE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());

  // A worker applying a truncated stream rolls back and never calls
  // set_success(): a truncated transaction is a terminal, rolled-back state and
  // is never committed (commit() and truncation are mutually exclusive).
  // Truncation alone does not release the payload, so the reserved bytes stay
  // charged until the committed-head sweep drops the dispatched, truncated
  // envelope (reclaimed exactly like a committed one), releasing its bytes.
  // After the sweep env dangles - do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// ---------------------------------------------------------------------------
// Property test: seal-exactly-once.
// ---------------------------------------------------------------------------

// Seal-exactly-once property: over many deterministically-seeded trials,
// each trial opens a group, appends a RANDOM number n of non-terminal events,
// then a SINGLE terminal append — the one and only seal. That terminal append
// closes the group (the observable "sealed" signal). To prove no
// append succeeds after the seal, the pre-seal sink pointer is captured and a
// couple of further appends are driven into it via a local handle (bypassing
// the mechanism's null guard): because the sink is sealed, its byte source
// drops each event, so none is delivered. Draining the consumer must then yield
// exactly the n body events + 1 terminal (the post-seal appends delivered
// nothing) and reach a clean end-of-stream — demonstrating the seal happened
// exactly once and nothing was appended after it.
TEST(QueuedTransactionWriterTest, PropertySealExactlyOnce) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0x5EA10CE5u;
  constexpr int kTrials = 200;
  constexpr int kMaxBodyEvents = 6;  // K: random body-event count is 0..K.

  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> n_dist(0, kMaxBodyEvents);

  for (int trial = 0; trial < kTrials; ++trial) {
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                             test_memory_resource(kMemoryLimit));
    // mi-owned queue defaults to stopped; arm it for the test.
    resume_both_roles(queue);

    // Open the group: success (false) and a published, non-null sink.
    Open_group group;
    ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength))
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_NE(group.sink, nullptr) << "seed=" << kSeed << " trial=" << trial;

    // Append a random number of non-terminal (body) events.
    const int n = n_dist(rng);
    for (int step = 0; step < n; ++step) {
      const auto body = serialize_event();
      ASSERT_FALSE(append_transaction_event(group, as_char(body), body.size(),
                                            /*is_terminal=*/false))
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;
      ASSERT_NE(group.sink, nullptr)
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;
    }

    // Capture the sink pointer BEFORE the seal so post-seal appends can target
    // it directly (the mechanism closes the group on the terminal event).
    Streaming_event_sink *sealed_sink = group.sink;

    // The single terminal append is the one and only seal.
    const auto terminal = serialize_event();
    ASSERT_FALSE(append_transaction_event(group, as_char(terminal),
                                          terminal.size(),
                                          /*is_terminal=*/true))
        << "seed=" << kSeed << " trial=" << trial;
    // Sealing cleared the sink handle (observable "sealed" signal).
    ASSERT_TRUE(is_closed(group)) << "seed=" << kSeed << " trial=" << trial;

    // Prove no append succeeds after the seal: drive a couple more appends into
    // the sealed sink via a local handle that bypasses the null guard. The
    // mechanism reports success (its handle is non-null) but the sealed byte
    // source no-ops each append, so nothing is delivered; is_terminal=false
    // keeps the local handle set.
    for (int post = 0; post < 2; ++post) {
      Open_group p{/*envelope=*/nullptr, sealed_sink};
      const auto extra = serialize_event();
      ASSERT_FALSE(append_transaction_event(p, as_char(extra), extra.size(),
                                            /*is_terminal=*/false))
          << "seed=" << kSeed << " trial=" << trial << " post-seal=" << post;
    }

    // Drive the consumer: exactly the n body events + 1 terminal come back, the
    // post-seal appends delivered nothing, and the stream ends cleanly.
    Transaction_envelope *env = queue.dispatch_next();
    ASSERT_NE(env, nullptr) << "seed=" << kSeed << " trial=" << trial;
    ASSERT_NE(env->payload(), nullptr)
        << "seed=" << kSeed << " trial=" << trial;
    auto fetchable = env->payload()->fetchable();
    ASSERT_NE(fetchable, nullptr) << "seed=" << kSeed << " trial=" << trial;

    EXPECT_EQ(drain(fetchable), n + 1)
        << "seed=" << kSeed << " trial=" << trial;
    EXPECT_TRUE(fetchable->is_fetching_done())
        << "seed=" << kSeed << " trial=" << trial;
    EXPECT_FALSE(fetchable->is_fetching_error())
        << "seed=" << kSeed << " trial=" << trial;

    // Commit + sweep so the queue destructor invariant (empty deque,
    // bytes_used()==0) holds this trial.
    fetchable->set_success();
    EXPECT_EQ(queue.bytes_used(), 0u) << "seed=" << kSeed << " trial=" << trial;
    ASSERT_FALSE(queue.sweep_committed())
        << "seed=" << kSeed << " trial=" << trial;
  }
}

// ---------------------------------------------------------------------------
// Property test: truncate-on-incomplete and current-sink clearing.
// ---------------------------------------------------------------------------

// Both properties: over many deterministically-seeded trials, each trial opens
// a group, appends a RANDOM number n of non-terminal events, then randomly ends
// the group one of two ways:
//   mode A: a terminal append (normal completion), or
//   mode B: truncate_transaction() (models rotate/incomplete/error/stop mid-
//           transaction).
// In BOTH modes the group must be closed afterward (cleared at group end),
// and it is asserted null at the start of the trial (outside any open group) —
// i.e. it is non-null ONLY between open and terminate/truncate. For mode B, a
// repeat truncate on the now-null sink is a no-op (stays null) and a post-
// truncate append into the truncated sink (via a captured local handle)
// delivers nothing — proving exactly one truncation took effect. Draining
// the consumer then differs by mode: mode A (terminal) delivers all n+1 events
// and reaches a clean done; mode B (truncate) marks the transaction truncated,
// so the consumer stops immediately (delivers nothing) and surfaces truncation
// (is_truncated, not done and not error) — the signal that drives the worker's
// rollback-and-replay branch.
TEST(QueuedTransactionWriterTest, PropertyTruncateOnIncompleteAndSinkCleared) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0x7C0FFEE1u;
  constexpr int kTrials = 200;
  constexpr int kMaxBodyEvents = 6;  // K: random body-event count is 0..K.

  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> n_dist(0, kMaxBodyEvents);
  // 0=terminal, 1=truncate.
  std::uniform_int_distribution<int> mode_dist(0, 1);

  for (int trial = 0; trial < kTrials; ++trial) {
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                             test_memory_resource(kMemoryLimit));
    // mi-owned queue defaults to stopped; arm it for the test.
    resume_both_roles(queue);

    // The group is closed outside any open group (start of trial).
    Open_group group;
    ASSERT_TRUE(is_closed(group)) << "seed=" << kSeed << " trial=" << trial;

    ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength))
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_NE(group.sink, nullptr) << "seed=" << kSeed << " trial=" << trial;

    // Append a random number of non-terminal (body) events.
    const int n = n_dist(rng);
    for (int step = 0; step < n; ++step) {
      const auto body = serialize_event();
      ASSERT_FALSE(append_transaction_event(group, as_char(body), body.size(),
                                            /*is_terminal=*/false))
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;
    }

    const bool mode_truncate = (mode_dist(rng) == 1);
    int expected_count = 0;
    if (!mode_truncate) {
      // Mode A: normal termination via the terminal event.
      const auto terminal = serialize_event();
      ASSERT_FALSE(append_transaction_event(group, as_char(terminal),
                                            terminal.size(),
                                            /*is_terminal=*/true))
          << "seed=" << kSeed << " trial=" << trial << " (mode A)";
      // Cleared at group end.
      ASSERT_TRUE(is_closed(group))
          << "seed=" << kSeed << " trial=" << trial << " (mode A)";
      expected_count = n + 1;  // n body events + 1 terminal.
    } else {
      // Mode B: rotate/incomplete/error/stop mid-transaction → truncate.
      // Capture the sink pointer BEFORE truncation for the post-truncate probe.
      Streaming_event_sink *truncated_sink = group.sink;
      truncate_transaction(group);
      // Cleared at group end.
      ASSERT_TRUE(is_closed(group))
          << "seed=" << kSeed << " trial=" << trial << " (mode B)";
      // A repeat truncate on the now-null sink is a no-op (stays null).
      truncate_transaction(group);
      ASSERT_TRUE(is_closed(group))
          << "seed=" << kSeed << " trial=" << trial << " (mode B repeat)";
      // A post-truncate append into the truncated sink delivers nothing —
      // exactly one truncation took effect.
      Open_group p{/*envelope=*/nullptr, truncated_sink};
      const auto extra = serialize_event();
      ASSERT_FALSE(append_transaction_event(p, as_char(extra), extra.size(),
                                            /*is_terminal=*/false))
          << "seed=" << kSeed << " trial=" << trial << " (post-truncate)";
      // Truncation marks the transaction truncated, so the consumer stops
      // immediately: no buffered partial event is delivered.
      expected_count = 0;
    }

    // The group is closed after it ends, in BOTH modes.
    ASSERT_TRUE(is_closed(group)) << "seed=" << kSeed << " trial=" << trial;

    // Drive the consumer. Mode A (terminal) delivers all n+1 events and reaches
    // a clean done. Mode B (truncate) delivers nothing and surfaces truncation
    // (not done, not error) so the worker takes its rollback-and-replay branch.
    Transaction_envelope *env = queue.dispatch_next();
    ASSERT_NE(env, nullptr) << "seed=" << kSeed << " trial=" << trial;
    ASSERT_NE(env->payload(), nullptr)
        << "seed=" << kSeed << " trial=" << trial;
    auto fetchable = env->payload()->fetchable();
    ASSERT_NE(fetchable, nullptr) << "seed=" << kSeed << " trial=" << trial;

    EXPECT_EQ(drain(fetchable), expected_count)
        << "seed=" << kSeed << " trial=" << trial
        << " mode=" << (mode_truncate ? "truncate" : "terminal");
    if (mode_truncate) {
      EXPECT_TRUE(fetchable->is_truncated())
          << "seed=" << kSeed << " trial=" << trial << " (mode B)";
      EXPECT_FALSE(fetchable->is_fetching_done())
          << "seed=" << kSeed << " trial=" << trial << " (mode B)";
    } else {
      EXPECT_TRUE(fetchable->is_fetching_done())
          << "seed=" << kSeed << " trial=" << trial << " (mode A)";
      EXPECT_FALSE(fetchable->is_truncated())
          << "seed=" << kSeed << " trial=" << trial << " (mode A)";
    }
    EXPECT_FALSE(fetchable->is_fetching_error())
        << "seed=" << kSeed << " trial=" << trial;

    // Drain so the queue destructor invariant (empty deque, bytes_used()==0)
    // holds this trial. Mode A (terminal) is committed by the worker's success
    // hook; mode B (truncated) is rolled back and never committed (commit() and
    // truncation are mutually exclusive). Either terminal state is reclaimed by
    // the committed-head sweep, so bytes_used() returns to 0 once the envelope
    // is swept -- released at commit for mode A, at sweep for mode B.
    if (!mode_truncate) {
      fetchable->set_success();
    }
    ASSERT_FALSE(queue.sweep_committed())
        << "seed=" << kSeed << " trial=" << trial;
    EXPECT_EQ(queue.bytes_used(), 0u) << "seed=" << kSeed << " trial=" << trial;
  }
}

// ---------------------------------------------------------------------------
// Receiver hold: the receiver's open group holds its envelope, so neither
// a worker's commit nor the sweep frees what the receiver still writes.
// ---------------------------------------------------------------------------

// open_transaction() returns a held envelope whose sink is the group's sink;
// the terminal append closes the group and releases the hold.
TEST(QueuedTransactionWriterTest, OpenTransactionReturnsHeldEnvelope) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  EXPECT_TRUE(env->is_receiver_held());
  EXPECT_EQ(group.sink, env->current_sink());

  const auto e = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e), e.size(),
                                        /*is_terminal=*/true));
  EXPECT_TRUE(is_closed(group));
  EXPECT_FALSE(env->is_receiver_held());

  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  EXPECT_EQ(drain(fetchable), 1);
  fetchable->set_success();
  fetchable.reset();
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// The receiver-hold scenario on the memory path: a worker skips an already
// executed transaction at its GTID event and commits it while the receiver
// still streams the body. The commit keeps the payload (the hold), the sweep
// does not pop the envelope, and the receiver's later appends write into live
// memory. The terminal event releases the hold and frees the payload.
TEST(QueuedTransactionWriterTest, SkipMidStreamKeepsPayloadUntilTerminal) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  const auto e1 = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e1), e1.size(),
                                        /*is_terminal=*/false));

  // The worker takes the transaction, skips it and commits (the batch's commit
  // hook -> Transaction_envelope::commit()). Dropping the local shared_ptr
  // models Job_binlog::set_success releasing the job's reference, so only the
  // payload (kept by the hold) still owns the transaction and its sink.
  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  fetchable->set_success();
  fetchable.reset();

  EXPECT_TRUE(env->is_committed());
  EXPECT_NE(env->payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), kTrxLength);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 1u) << "the held envelope is not swept";

  // The receiver keeps appending the rest of the skipped transaction.
  const auto e2 = serialize_event();
  const auto e3 = serialize_event();
  const auto e4 = serialize_event();
  EXPECT_FALSE(append_transaction_event(group, as_char(e2), e2.size(),
                                        /*is_terminal=*/false));
  EXPECT_FALSE(append_transaction_event(group, as_char(e3), e3.size(),
                                        /*is_terminal=*/false));
  EXPECT_EQ(group.envelope, env);
  EXPECT_FALSE(append_transaction_event(group, as_char(e4), e4.size(),
                                        /*is_terminal=*/true));

  // The terminal event closed the group; the release freed the payload.
  EXPECT_TRUE(is_closed(group));
  EXPECT_EQ(env->payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
}

// Facet (b), the post-unlock window: a worker commits after the terminal event
// sealed the stream but before the receiver closed its group (the sink still
// runs set_fetching_complete() and the notify after it unlocks the stream).
// The raw sink append below stops inside that window. The commit keeps the
// payload and the sink alive; the receiver's close then frees them.
TEST(QueuedTransactionWriterTest, CommitAfterSealBeforeCloseKeepsPayload) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);

  // The terminal append through the sink itself: sealed, group still open.
  const auto e = serialize_event();
  ASSERT_FALSE(group.sink->append_event(as_char(e), e.size(),
                                        /*seal_after=*/true));

  // The worker drains and commits inside the window.
  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  EXPECT_EQ(drain(fetchable), 1);
  EXPECT_TRUE(fetchable->is_fetching_done());
  fetchable->set_success();
  fetchable.reset();

  EXPECT_TRUE(env->is_committed());
  EXPECT_NE(env->payload(), nullptr);
  EXPECT_EQ(env->current_sink(), group.sink) << "the sink is still alive";
  EXPECT_EQ(queue.bytes_used(), kTrxLength);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 1u);

  // The receiver closes the group as close_group() does: clear, then release.
  Transaction_envelope *held = group.envelope;
  group = {};
  held->release_receiver();
  EXPECT_EQ(env->payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
}

// Facet (e), skip commit then truncation: the receiver truncates a group that
// a worker already committed (IO stop, reconnect Rotate, new GTID). No assert;
// the envelope stays committed and is reclaimed after the release.
TEST(QueuedTransactionWriterTest, CommitThenTruncateIsNoOp) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  const auto e = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e), e.size(),
                                        /*is_terminal=*/false));

  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  fetchable->set_success();
  fetchable.reset();
  ASSERT_TRUE(env->is_committed());

  truncate_transaction(group);
  EXPECT_TRUE(is_closed(group));
  EXPECT_TRUE(env->is_committed());
  EXPECT_FALSE(env->is_truncated());
  EXPECT_EQ(env->payload(), nullptr) << "freed at the release";
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
}

// Facet (f), truncation then skip commit: the worker passed its is_truncated()
// check before the receiver truncated the group, then runs the commit hook.
// commit() is a no-op (the envelope stays truncated, not committed) and the
// envelope is reclaimed by the sweep.
TEST(QueuedTransactionWriterTest, TruncateThenCommitIsNoOp) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  const auto e = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e), e.size(),
                                        /*is_terminal=*/false));

  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  truncate_transaction(group);
  EXPECT_TRUE(is_closed(group));
  ASSERT_TRUE(env->is_truncated());

  // The commit hook sees the transaction truncated and returns without
  // touching the envelope (not counted, no assert).
  fetchable->set_success();
  fetchable.reset();
  EXPECT_TRUE(env->is_truncated());
  EXPECT_FALSE(env->is_committed());
  EXPECT_TRUE(env->is_reclaimable());
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Facet (f) with the envelope reclaimed: the receiver's truncation releases
// the hold, so the coordinator can sweep the dispatched envelope before the
// skipping worker's commit hook runs. The hook must then not touch the freed
// envelope; it only sees the Fetchable_transaction, which the job keeps alive.
// Run under MALLOC_PERTURB_ (or a memory checker) to make an access fail.
TEST(QueuedTransactionWriterTest, TruncateSweepThenCommitSkipsEnvelope) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  const auto e = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e), e.size(),
                                        /*is_terminal=*/false));

  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  truncate_transaction(group);
  EXPECT_TRUE(is_closed(group));
  EXPECT_TRUE(fetchable->is_truncated());
  EXPECT_FALSE(queue.sweep_committed());
  ASSERT_EQ(queue.queue_length(), 0u) << "the envelope is freed";
  env = nullptr;

  fetchable->set_success();
  fetchable.reset();
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// The spill variant of the case above.
TEST(QueuedTransactionWriterTest, SpillTruncateSweepThenCommitSkipsEnvelope) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kSpillTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  ASSERT_EQ(env->path(), Envelope_path::SPILL);
  const auto e = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e), e.size(),
                                        /*is_terminal=*/false));

  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  truncate_transaction(group);
  EXPECT_TRUE(is_closed(group));
  EXPECT_TRUE(fetchable->is_truncated());
  EXPECT_FALSE(queue.sweep_committed());
  ASSERT_EQ(queue.queue_length(), 0u) << "the envelope is freed";
  env = nullptr;

  fetchable->set_success();
  fetchable.reset();
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
}

// Sticky truncation: truncation is receiver state, so a consumer reset
// keeps it. The Job_binlog constructor calls reset_fetching(false) and a retry
// calls reset_fetching(true); if either cleared the flag, a job built after
// the truncation would wait forever on a stream that never completes, and the
// commit hook's is_truncated() guard would let a skip commit touch a swept
// envelope. is_truncated() is asserted before wait_next(), so a regression
// fails here instead of blocking.
void expect_truncation_survives_reset(Trx_envelope_queue &queue,
                                      std::size_t trx_length) {
  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), trx_length));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  const auto e = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e), e.size(),
                                        /*is_terminal=*/false));

  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  truncate_transaction(group);
  EXPECT_TRUE(is_closed(group));
  ASSERT_TRUE(fetchable->is_truncated());

  for (const bool all : {false, true}) {
    fetchable->reset_fetching(all);
    ASSERT_TRUE(fetchable->is_truncated()) << "reset_fetching(" << all << ")";
    EXPECT_FALSE(fetchable->wait_next()) << "reset_fetching(" << all << ")";
    EXPECT_FALSE(fetchable->fetch_next().has_value());
    EXPECT_FALSE(fetchable->is_fetching_done());
    EXPECT_FALSE(fetchable->is_fetching_error());
  }

  // The commit hook skips the truncated envelope; the sweep reclaims it.
  fetchable->set_success();
  EXPECT_FALSE(env->is_committed());
  EXPECT_TRUE(env->is_reclaimable());
  fetchable.reset();
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
}

TEST(QueuedTransactionWriterTest, TruncationSurvivesConsumerReset) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);
  expect_truncation_survives_reset(queue, kTrxLength);
  EXPECT_EQ(queue.bytes_used(), 0u);
  // Drain what a failed assertion left, so the queue destructor does not
  // assert. A no-op when the helper passed.
  queue.reset();
}

TEST(QueuedTransactionWriterTest, SpillTruncationSurvivesConsumerReset) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  resume_both_roles(queue);
  expect_truncation_survives_reset(queue, kSpillTrxLength);
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  queue.reset();  // See TruncationSurvivesConsumerReset.
}

// The receiver-hold scenario on the spill path: the spill file outlives a skip
// commit until the receiver's terminal event releases the hold, which deletes
// it on the receiver's thread.
TEST(QueuedTransactionWriterTest, SpillSkipMidStreamKeepsFileUntilTerminal) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kSpillTrxLength));
  Transaction_envelope *env = group.envelope;
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(env->path(), Envelope_path::SPILL);
  auto *spill = dynamic_cast<Event_set_fetchable_spill *>(group.sink);
  ASSERT_NE(spill, nullptr);
  // Copy the name: the string lives in the batch, which the release frees.
  const std::string file_name = spill->spill_file_name();
  ASSERT_FALSE(file_name.empty());
  EXPECT_TRUE(std::filesystem::exists(file_name));
  EXPECT_EQ(queue.spill_file_count(), 1u);

  const auto e1 = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(e1), e1.size(),
                                        /*is_terminal=*/false));

  // Skip commit while the receiver still streams.
  ASSERT_EQ(queue.dispatch_next(), env);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  fetchable->set_success();
  fetchable.reset();
  EXPECT_TRUE(env->is_committed());
  EXPECT_NE(env->payload(), nullptr);
  EXPECT_TRUE(std::filesystem::exists(file_name)) << "held: file kept";
  EXPECT_EQ(queue.spill_file_count(), 1u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 1u);

  const auto e2 = serialize_event();
  const auto e3 = serialize_event();
  EXPECT_FALSE(append_transaction_event(group, as_char(e2), e2.size(),
                                        /*is_terminal=*/false));
  EXPECT_FALSE(append_transaction_event(group, as_char(e3), e3.size(),
                                        /*is_terminal=*/true));

  // The release at the terminal event deleted the file, before any sweep.
  EXPECT_TRUE(is_closed(group));
  EXPECT_EQ(env->payload(), nullptr);
  EXPECT_FALSE(std::filesystem::exists(file_name));
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
}

// Facet (d), spill provisioning failure: open_transaction() reports the
// failure with the group closed, and the envelope it enqueued (truncated by
// validate_payload()) is released and reclaimable.
TEST(QueuedTransactionWriterTest, ProvisioningFailureReleasesEnvelope) {
  // No relay-log directory, so the spill writer cannot create its file.
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Open_group group;
  Receiver_write_failure f;
  EXPECT_TRUE(open_transaction(queue, group, make_fde(), kSpillTrxLength,
                               /*is_trx=*/true, &f));
  EXPECT_TRUE(is_closed(group));
  ASSERT_EQ(queue.queue_length(), 1u);
  const std::string prefix(
      "[In-memory relay log] failed to admit incoming transaction: ");
  EXPECT_FALSE(f.stopped);
  EXPECT_EQ(f.reason.rfind(prefix, 0), 0u) << f.reason;
  EXPECT_GT(f.reason.size(), prefix.size()) << f.reason;

  // dispatch_next() hands out even a truncated envelope (sweep_and_dispatch()
  // would skip it); use it to reach the pointer and move the dispatch cursor.
  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  EXPECT_TRUE(env->is_truncated());
  EXPECT_FALSE(env->is_receiver_held());
  EXPECT_TRUE(env->is_reclaimable());
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
}

#ifndef NDEBUG
// A spill append failure closes the group and records the specific
// reason; the truncated envelope and its file are reclaimed.
TEST(QueuedTransactionWriterTest, AppendFailureFillsReason) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  resume_both_roles(queue);

  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kSpillTrxLength));
  ASSERT_EQ(group.envelope->path(), Envelope_path::SPILL);

  Receiver_write_failure f;
  const auto e = serialize_event();
  DBUG_SET("+d,imr_force_spill_append_failure");
  const bool failed = append_transaction_event(group, as_char(e), e.size(),
                                               /*is_terminal=*/false, &f);
  DBUG_SET("-d,imr_force_spill_append_failure");
  ASSERT_TRUE(failed);
  EXPECT_TRUE(is_closed(group));
  EXPECT_FALSE(f.stopped);
  EXPECT_EQ(f.reason.rfind("[In-memory relay log] failed to append event to "
                           "incoming transaction: ",
                           0),
            0u)
      << f.reason;
  EXPECT_NE(f.reason.find("forced spill append failure"), std::string::npos)
      << f.reason;

  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  EXPECT_TRUE(env->is_truncated());
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.spill_file_count(), 0u);
}
#endif  // NDEBUG

}  // namespace mysql::csa::unittests
