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
/// Unit tests (tasks.md task 4.2) for the minimal-state receiver-side
/// mysql::csa::queued_transaction_writer mechanism. The three streaming
/// operations the receiver drives — open_transaction (at the GTID event),
/// append_transaction_event (per body / terminal event), and
/// truncate_transaction (on an incomplete group) — are exercised against a REAL
/// Trx_envelope_queue plus a LOCAL `Streaming_event_sink *current_sink` —
/// deliberately NO Master_info — which is exactly how the mechanism is
/// parameterized so it can be tested without any receiver coupling.
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"
#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"

namespace mysql::csa::unittests {

namespace {

// Generous per-channel bounds so enqueue() always takes the MEMORY path and
// never blocks in acquire_admission(). kTrxLength must be at or below the spill
// threshold so classify() picks MEMORY.
constexpr std::size_t kMemoryLimit = 1u << 20;     // 1 MiB
constexpr std::size_t kSpillThreshold = 1u << 16;  // 64 KiB
constexpr std::size_t kTrxLength = 4096;

/// Build the active FDE handed to the ops. The exact type matches what
/// Master_info::get_mi_description_event_shared() returns in the real receiver.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
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
// enqueue-created destination's sink through current_sink.
TEST(QueuedTransactionWriterTest, OpenGroupPublishesSink) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  Streaming_event_sink *current_sink = nullptr;
  ASSERT_FALSE(open_transaction(queue, current_sink, make_fde(), kTrxLength));
  ASSERT_NE(current_sink, nullptr);
  // enqueue() reserved exactly kTrxLength bytes for the memory-path payload.
  EXPECT_EQ(queue.bytes_used(), kTrxLength);

  // Seal the (empty) stream so the consumer terminates cleanly, then commit and
  // sweep so the queue destructor invariant (empty deque, bytes_used()==0)
  // holds.
  current_sink->seal_stream();
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
// queuing-error indication (true) and leaves current_sink null. No envelope is
// created, so the queue destructor invariant holds untouched.
TEST(QueuedTransactionWriterTest, OpenGroupAfterStopReportsErrorAndLeavesSinkNull) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  queue.stop();

  // Start from a deliberately non-null sentinel to prove the op clears it.
  auto *sentinel = reinterpret_cast<Streaming_event_sink *>(0x1);
  Streaming_event_sink *current_sink = sentinel;

  EXPECT_TRUE(open_transaction(queue, current_sink, make_fde(), kTrxLength));
  EXPECT_EQ(current_sink, nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// append order + terminal seal: after opening, appending a couple of
// non-terminal events then a terminal one seals the stream exactly once, clears
// current_sink to null, and the consumer observes the events in order followed
// by a clean end-of-stream.
TEST(QueuedTransactionWriterTest, AppendOrderAndTerminalSeal) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  Streaming_event_sink *current_sink = nullptr;
  ASSERT_FALSE(open_transaction(queue, current_sink, make_fde(), kTrxLength));
  ASSERT_NE(current_sink, nullptr);

  const auto e1 = serialize_event();
  const auto e2 = serialize_event();
  const auto e3 = serialize_event();

  EXPECT_FALSE(append_transaction_event(current_sink, as_char(e1), e1.size(),
                                        /*is_terminal=*/false));
  EXPECT_NE(current_sink, nullptr);
  EXPECT_FALSE(append_transaction_event(current_sink, as_char(e2), e2.size(),
                                        /*is_terminal=*/false));
  EXPECT_NE(current_sink, nullptr);
  // Terminal append seals the stream and clears the sink.
  EXPECT_FALSE(append_transaction_event(current_sink, as_char(e3), e3.size(),
                                        /*is_terminal=*/true));
  EXPECT_EQ(current_sink, nullptr);

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
// atomic-DDL Query_log_event) seals the stream exactly once and clears
// current_sink. The consumer drains exactly one event, then reaches a clean
// end-of-stream — one event both opened and terminated the group.
TEST(QueuedTransactionWriterTest, AtomicDdlSingleEventOpensAndSealsOnce) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  Streaming_event_sink *current_sink = nullptr;
  ASSERT_FALSE(open_transaction(queue, current_sink, make_fde(), kTrxLength));
  ASSERT_NE(current_sink, nullptr);

  // The sole event is both the body and the terminal event: appending it seals
  // the stream and clears the sink in one step.
  const auto e = serialize_event();
  EXPECT_FALSE(append_transaction_event(current_sink, as_char(e), e.size(),
                                        /*is_terminal=*/true));
  EXPECT_EQ(current_sink, nullptr);

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

// append defensive: append_event with a null current_sink is a no-op that
// returns true (error).
TEST(QueuedTransactionWriterTest, AppendWithNullSinkReturnsError) {
  Streaming_event_sink *current_sink = nullptr;
  const auto e = serialize_event();
  EXPECT_TRUE(append_transaction_event(current_sink, as_char(e), e.size(),
                                       /*is_terminal=*/false));
  EXPECT_EQ(current_sink, nullptr);
}

// truncate: after opening and appending one event, truncate_transaction marks
// the transaction truncated at BOTH levels -- the batch stream AND the owning
// Fetchable_transaction -- and clears current_sink. The consumer therefore
// stops immediately: it delivers no buffered partial events and surfaces
// truncation (is_truncated), NOT a clean done and NOT an error. That is what
// drives the worker's is_truncated() rollback-and-replay branch and prevents a
// buffered event from being mis-treated as the transaction's terminal event. A
// second truncate_transaction on a null current_sink is a no-op.
TEST(QueuedTransactionWriterTest, TruncateClearsSinkAndConsumerObservesTruncation) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  Streaming_event_sink *current_sink = nullptr;
  ASSERT_FALSE(open_transaction(queue, current_sink, make_fde(), kTrxLength));
  ASSERT_NE(current_sink, nullptr);

  const auto e1 = serialize_event();
  EXPECT_FALSE(append_transaction_event(current_sink, as_char(e1), e1.size(),
                                        /*is_terminal=*/false));

  // Truncate the still-open group: batch + transaction marked truncated, sink
  // cleared.
  truncate_transaction(current_sink);
  EXPECT_EQ(current_sink, nullptr);

  // truncate_transaction on a null sink is a no-op (no crash, stays null).
  truncate_transaction(current_sink);
  EXPECT_EQ(current_sink, nullptr);

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

  // Commit + sweep so the queue destructor invariant holds (the worker's
  // success hook commits the rolled-back transaction as an empty unit).
  fetchable->set_success();
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(queue.sweep_committed());
}

// ---------------------------------------------------------------------------
// Task 4.3 - Property test: seal-exactly-once.
// Property 2: R2 — seal-exactly-once.
// Validates: Requirements 2.4
// ---------------------------------------------------------------------------

// Property 2 (seal-exactly-once): over many deterministically-seeded trials,
// each trial opens a group, appends a RANDOM number n of non-terminal events,
// then a SINGLE terminal append — the one and only seal. That terminal append
// clears current_sink to null (the observable "sealed" signal). To prove no
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
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
    queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

    // Open the group: success (false) and a published, non-null sink.
    Streaming_event_sink *current_sink = nullptr;
    ASSERT_FALSE(open_transaction(queue, current_sink, make_fde(), kTrxLength))
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_NE(current_sink, nullptr) << "seed=" << kSeed << " trial=" << trial;

    // Append a random number of non-terminal (body) events.
    const int n = n_dist(rng);
    for (int step = 0; step < n; ++step) {
      const auto body = serialize_event();
      ASSERT_FALSE(append_transaction_event(current_sink, as_char(body),
                                            body.size(),
                                            /*is_terminal=*/false))
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;
      ASSERT_NE(current_sink, nullptr)
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;
    }

    // Capture the sink pointer BEFORE the seal so post-seal appends can target
    // it directly (the mechanism nulls current_sink on the terminal event).
    Streaming_event_sink *sealed_sink = current_sink;

    // The single terminal append is the one and only seal.
    const auto terminal = serialize_event();
    ASSERT_FALSE(append_transaction_event(current_sink, as_char(terminal),
                                          terminal.size(),
                                          /*is_terminal=*/true))
        << "seed=" << kSeed << " trial=" << trial;
    // Sealing cleared the sink handle (observable "sealed" signal).
    ASSERT_EQ(current_sink, nullptr) << "seed=" << kSeed << " trial=" << trial;

    // Prove no append succeeds after the seal: drive a couple more appends into
    // the sealed sink via a local handle that bypasses the null guard. The
    // mechanism reports success (its handle is non-null) but the sealed byte
    // source no-ops each append, so nothing is delivered; is_terminal=false
    // keeps the local handle set.
    for (int post = 0; post < 2; ++post) {
      Streaming_event_sink *p = sealed_sink;
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
    EXPECT_EQ(queue.bytes_used(), 0u)
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_FALSE(queue.sweep_committed())
        << "seed=" << kSeed << " trial=" << trial;
  }
}

// ---------------------------------------------------------------------------
// Task 4.4 - Property test: truncate-on-incomplete and current-sink clearing.
// Property 3: R3 — truncate-on-incomplete.
// Property 4: R4 — current-sink cleared at group end.
// Validates: Requirements 4.1, 2.4, 7.4
// ---------------------------------------------------------------------------

// Property 3 + 4: over many deterministically-seeded trials, each trial opens a
// group, appends a RANDOM number n of non-terminal events, then randomly ends
// the group one of two ways:
//   mode A: a terminal append (normal completion), or
//   mode B: truncate_transaction() (models rotate/incomplete/error/stop mid-
//           transaction).
// In BOTH modes current_sink must be null afterward (R4: cleared at group end),
// and it is asserted null at the start of the trial (outside any open group) —
// i.e. it is non-null ONLY between open and terminate/truncate. For mode B, a
// repeat truncate on the now-null sink is a no-op (stays null) and a post-
// truncate append into the truncated sink (via a captured local handle)
// delivers nothing — proving exactly one truncation took effect (R3). Draining
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
  std::uniform_int_distribution<int> mode_dist(0, 1);  // 0=terminal, 1=truncate.

  for (int trial = 0; trial < kTrials; ++trial) {
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
    queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

    // R4: current_sink is null outside any open group (start of trial).
    Streaming_event_sink *current_sink = nullptr;
    ASSERT_EQ(current_sink, nullptr) << "seed=" << kSeed << " trial=" << trial;

    ASSERT_FALSE(open_transaction(queue, current_sink, make_fde(), kTrxLength))
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_NE(current_sink, nullptr) << "seed=" << kSeed << " trial=" << trial;

    // Append a random number of non-terminal (body) events.
    const int n = n_dist(rng);
    for (int step = 0; step < n; ++step) {
      const auto body = serialize_event();
      ASSERT_FALSE(append_transaction_event(current_sink, as_char(body),
                                            body.size(),
                                            /*is_terminal=*/false))
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;
    }

    const bool mode_truncate = (mode_dist(rng) == 1);
    int expected_count = 0;
    if (!mode_truncate) {
      // Mode A: normal termination via the terminal event.
      const auto terminal = serialize_event();
      ASSERT_FALSE(append_transaction_event(current_sink, as_char(terminal),
                                            terminal.size(),
                                            /*is_terminal=*/true))
          << "seed=" << kSeed << " trial=" << trial << " (mode A)";
      // R4: cleared at group end.
      ASSERT_EQ(current_sink, nullptr)
          << "seed=" << kSeed << " trial=" << trial << " (mode A)";
      expected_count = n + 1;  // n body events + 1 terminal.
    } else {
      // Mode B: rotate/incomplete/error/stop mid-transaction → truncate.
      // Capture the sink pointer BEFORE truncation for the post-truncate probe.
      Streaming_event_sink *truncated_sink = current_sink;
      truncate_transaction(current_sink);
      // R4: cleared at group end.
      ASSERT_EQ(current_sink, nullptr)
          << "seed=" << kSeed << " trial=" << trial << " (mode B)";
      // A repeat truncate on the now-null sink is a no-op (stays null).
      truncate_transaction(current_sink);
      ASSERT_EQ(current_sink, nullptr)
          << "seed=" << kSeed << " trial=" << trial << " (mode B repeat)";
      // A post-truncate append into the truncated sink delivers nothing —
      // exactly one truncation took effect (R3).
      Streaming_event_sink *p = truncated_sink;
      const auto extra = serialize_event();
      ASSERT_FALSE(append_transaction_event(p, as_char(extra), extra.size(),
                                            /*is_terminal=*/false))
          << "seed=" << kSeed << " trial=" << trial << " (post-truncate)";
      // Truncation marks the transaction truncated, so the consumer stops
      // immediately: no buffered partial event is delivered.
      expected_count = 0;
    }

    // R4: current_sink is null after the group ends, in BOTH modes.
    ASSERT_EQ(current_sink, nullptr) << "seed=" << kSeed << " trial=" << trial;

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

    // Commit + sweep so the queue destructor invariant (empty deque,
    // bytes_used()==0) holds this trial.
    fetchable->set_success();
    EXPECT_EQ(queue.bytes_used(), 0u)
        << "seed=" << kSeed << " trial=" << trial;
    ASSERT_FALSE(queue.sweep_committed())
        << "seed=" << kSeed << " trial=" << trial;
  }
}

}  // namespace mysql::csa::unittests
