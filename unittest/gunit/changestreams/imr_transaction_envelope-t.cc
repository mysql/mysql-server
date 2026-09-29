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
/// Unit tests for mysql::csa::Transaction_envelope, the lightweight FIFO-queue
/// entry that tracks a single transaction's COMMIT flag (uncommitted ->
/// committed) and owns its heavy Trx_payload. The envelope tracks only the
/// commit axis; reception ("fully received") is owned by the byte source, not
/// the envelope. The tests use a real Trx_envelope_queue as the byte-accounting
/// backend and a default-constructed Fetchable_transaction as the wrapped byte
/// source so payload release is observed through the queue's memory-usage
/// counter.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"  // Format_description_log_event

namespace mysql::csa::unittests {

namespace {
/// Memory bounds large enough that attaching a payload never blocks; payloads
/// are attached directly, so the spill threshold is not exercised here.
constexpr std::size_t kMemoryLimit = 1u << 20;     // 1 MiB
constexpr std::size_t kSpillThreshold = 1u << 16;  // 64 KiB

/// A representative payload size for the byte-accounting tests.
constexpr std::size_t kTrxLength = 4096;

/// Build a payload wrapping a fresh Fetchable_transaction against @p queue.
std::unique_ptr<Trx_payload> make_payload(Trx_envelope_queue *queue,
                                          std::size_t len) {
  return std::make_unique<Trx_payload>(
      std::make_shared<Fetchable_transaction>(), len, queue);
}

/// RAII private "relay log directory" for spill-path tests; removed on scope
/// exit (along with the spill files and temp-files subdir under it).
struct Scoped_temp_dir {
  std::string path;
  Scoped_temp_dir() {
    static std::atomic<unsigned> counter{0};
    path = (std::filesystem::temp_directory_path() /
            ("imr_env_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1))))
               .string();
    std::filesystem::create_directories(path);
  }
  ~Scoped_temp_dir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

/// A fresh relay-log FDE, as the queue/receiver would supply.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}
}  // namespace

// The envelope owns a std::mutex member and lives by value in the queue's
// deque, so it must be neither copyable nor movable.
static_assert(!std::is_copy_constructible_v<Transaction_envelope>,
              "Transaction_envelope must not be copy constructible");
static_assert(!std::is_move_constructible_v<Transaction_envelope>,
              "Transaction_envelope must not be move constructible");

// ---------------------------------------------------------------------------
// Task 4.2 - Unit tests: envelope commit lifecycle.
// Requirements 2.1, 2.2, 2.3, 2.5, 2.6
// ---------------------------------------------------------------------------

// Req 2.1: a freshly constructed envelope is uncommitted and its immutable
// accessors echo the values it was constructed with.
TEST(ImrTransactionEnvelopeTest, InitialStateUncommittedAndAccessors) {
  const std::uint64_t stream_seqno = 42;
  const std::size_t trx_length = 8192;

  Transaction_envelope env(stream_seqno, trx_length, Envelope_path::MEMORY);

  EXPECT_FALSE(env.is_committed());
  EXPECT_FALSE(env.is_truncated());
  EXPECT_EQ(env.stream_seqno(), stream_seqno);
  EXPECT_EQ(env.trx_length(), trx_length);
  EXPECT_EQ(env.path(), Envelope_path::MEMORY);

  // The SPILL path is recorded verbatim as well.
  Transaction_envelope spill_env(7, 128, Envelope_path::SPILL);
  EXPECT_EQ(spill_env.path(), Envelope_path::SPILL);
}

// Req 2.6, 9.2: commit() releases the payload's reserved bytes and nulls the
// payload reference. Committing a freshly-attached (uncommitted) envelope
// SUCCEEDS (returns false) — there is no seal precondition. Byte release is
// observed through the queue counter.
TEST(ImrTransactionEnvelopeTest, CommitReleasesPayloadBytesAndNullsPayload) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  Transaction_envelope env(1, kTrxLength, Envelope_path::MEMORY);

  env.attach_payload(make_payload(&queue, kTrxLength));
  ASSERT_EQ(queue.bytes_used(), kTrxLength);
  ASSERT_NE(env.payload(), nullptr);

  // Commit from uncommitted succeeds: false == success.
  EXPECT_FALSE(env.commit());

  EXPECT_TRUE(env.is_committed());
  EXPECT_EQ(env.payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Req 2.5: a second commit() on an already-committed envelope is a contract
// violation (the envelope is committed by its single worker exactly once). It
// is rejected (returns true) and leaves the commit flag, payload reference, and
// byte counter unchanged - no double release of the payload's bytes.
TEST(ImrTransactionEnvelopeTest, ReCommitRejectedNoDoubleRelease) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  Transaction_envelope env(1, kTrxLength, Envelope_path::MEMORY);

  env.attach_payload(make_payload(&queue, kTrxLength));

  // First commit: succeeds (false), releasing the reserved bytes.
  ASSERT_FALSE(env.commit());
  ASSERT_TRUE(env.is_committed());
  ASSERT_EQ(env.payload(), nullptr);
  ASSERT_EQ(queue.bytes_used(), 0u);

  // Re-commit: rejected (true == failure), no state change, no payload change,
  // no further byte release.
  EXPECT_TRUE(env.commit());
  EXPECT_TRUE(env.is_committed());
  EXPECT_EQ(env.payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Req 2.6: reset_payload() drops the payload, releasing its bytes and nulling
// the reference, without committing the envelope.
TEST(ImrTransactionEnvelopeTest, ResetPayloadReleasesBytesAndNullsPayload) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  Transaction_envelope env(1, kTrxLength, Envelope_path::MEMORY);

  env.attach_payload(make_payload(&queue, kTrxLength));
  ASSERT_EQ(queue.bytes_used(), kTrxLength);
  ASSERT_NE(env.payload(), nullptr);

  env.reset_payload();

  EXPECT_EQ(env.payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);
  // reset_payload() does not itself commit the envelope.
  EXPECT_FALSE(env.is_committed());
}

// ---------------------------------------------------------------------------
// Truncated end-state (Task 3).
// ---------------------------------------------------------------------------

// set_truncated() marks the envelope truncated (a second terminal state) and
// leaves it uncommitted. Unlike commit(), it does NOT release the payload: the
// reserved bytes stay charged until the envelope is swept/destroyed (Task 4's
// sweep-time release), which the queue counter confirms is unchanged here.
TEST(ImrTransactionEnvelopeTest, SetTruncatedMarksTruncatedAndKeepsPayload) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  Transaction_envelope env(1, kTrxLength, Envelope_path::MEMORY);

  env.attach_payload(make_payload(&queue, kTrxLength));
  ASSERT_EQ(queue.bytes_used(), kTrxLength);
  ASSERT_FALSE(env.is_truncated());
  ASSERT_FALSE(env.is_committed());

  env.set_truncated();

  EXPECT_TRUE(env.is_truncated());
  // Truncated is mutually exclusive with committed.
  EXPECT_FALSE(env.is_committed());
  // Payload (and its reserved bytes) is retained; not released by truncation.
  EXPECT_NE(env.payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), kTrxLength);

  // set_truncated() is idempotent: a second call is a no-op.
  env.set_truncated();
  EXPECT_TRUE(env.is_truncated());
  EXPECT_EQ(queue.bytes_used(), kTrxLength);
  // (env destroyed at scope end -> payload dtor releases the bytes; see below.)
}

// A truncated envelope releases its reserved bytes when it is destroyed — this
// is how the coordinator's sweep (Task 4) reclaims a truncated head: dropping
// the envelope runs the Trx_payload destructor, which returns the bytes to the
// queue counter. (For a committed envelope the release happens earlier, at
// commit(); for a truncated one it happens here, at sweep/destruction.)
TEST(ImrTransactionEnvelopeTest, TruncatedEnvelopeReleasesBytesOnDestruction) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  {
    Transaction_envelope env(1, kTrxLength, Envelope_path::MEMORY);
    env.attach_payload(make_payload(&queue, kTrxLength));
    env.set_truncated();
    // Not released by truncation itself.
    ASSERT_EQ(queue.bytes_used(), kTrxLength);
  }
  // Envelope destroyed exactly as the sweep would drop it: bytes released once.
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// ---------------------------------------------------------------------------
// Task 4.3 - Property test: commit-once monotonicity.
// Validates: Requirements 2.2, 2.3, 2.5 (Property 5: Commit-once)
// ---------------------------------------------------------------------------

// Property 5 (Commit-once): over a randomized sequence of commit() calls,
// is_committed() starts false, flips false->true on the FIRST call (which
// returns false == success), stays true forever after, every later commit()
// returns true (rejected), and is_committed() never regresses.
//
// The MySQL tree does not integrate a property-testing library, so the property
// is expressed as a deterministically seeded randomized-sequence generator run
// over many trials. The seed is fixed and echoed in every assertion message so
// any failure reproduces exactly. A payload is attached right after
// construction so commit()'s payload-reset path is genuinely exercised; a real
// Trx_envelope_queue declared before the envelope outlives it.
TEST(ImrTransactionEnvelopeTest, PropertyCommitOnceMonotonicity) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0x5A1E5EEDu;
  constexpr int kTrials = 3000;
  constexpr int kStepsPerTrial = 8;

  std::mt19937 rng(kSeed);
  // 0 -> skip this step; 1 -> issue a commit() call. Randomizing WHEN the
  // commits happen (and how many) exercises the "at most one success" contract.
  std::uniform_int_distribution<int> op_dist(0, 1);

  for (int trial = 0; trial < kTrials; ++trial) {
    // Queue declared before the envelope so it outlives it; the payload's
    // bytes are released through it when commit() resets the payload.
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
    Transaction_envelope env(1, kTrxLength, Envelope_path::MEMORY);
    env.attach_payload(make_payload(&queue, kTrxLength));

    // Independent shadow model of the expected commit flag.
    bool shadow_committed = false;

    // Initial: uncommitted.
    ASSERT_FALSE(env.is_committed())
        << "seed=" << kSeed << " trial=" << trial << " (initial state)";

    for (int step = 0; step < kStepsPerTrial; ++step) {
      const bool prev_committed = shadow_committed;

      if (op_dist(rng) == 1) {
        // commit() succeeds (false) iff currently uncommitted; every later call
        // is rejected (true).
        const bool expected_failure = shadow_committed;
        const bool actual_failure = env.commit();
        ASSERT_EQ(actual_failure, expected_failure)
            << "seed=" << kSeed << " trial=" << trial << " step=" << step
            << " (commit return mismatch)";
        shadow_committed = true;  // Committed after the first successful call.
        // The very first successful commit released the bytes; refine the
        // shadow: only flip to committed when the call actually succeeded.
        if (!prev_committed && !actual_failure) {
          // First successful commit: bytes released.
          ASSERT_EQ(queue.bytes_used(), 0u)
              << "seed=" << kSeed << " trial=" << trial << " step=" << step;
        }
      }

      // Observed flag matches the model.
      const bool observed = env.is_committed();
      ASSERT_EQ(observed, shadow_committed)
          << "seed=" << kSeed << " trial=" << trial << " step=" << step;

      // Core Property 5: the commit flag never regresses true->false.
      ASSERT_TRUE(observed || !prev_committed)
          << "seed=" << kSeed << " trial=" << trial << " step=" << step
          << " (commit flag regressed)";
    }

    // Ensure the envelope is committed by the end so the queue drains cleanly
    // (bytes_used() == 0), whether or not the random sequence issued a commit.
    if (!env.is_committed()) {
      ASSERT_FALSE(env.commit())
          << "seed=" << kSeed << " trial=" << trial << " (final commit)";
    }
    ASSERT_EQ(queue.bytes_used(), 0u)
        << "seed=" << kSeed << " trial=" << trial << " (drained)";
  }
}

// ---------------------------------------------------------------------------
// Task 4.4 - Property test: payload lifetime.
// Validates: Requirements 9.1, 9.2, 9.5
// ---------------------------------------------------------------------------

// Property 6 (Payload lifetime): a memory-path envelope holds a non-null
// payload from admission (attach) until commit, and after a successful commit
// the payload is null and its reserved bytes are released back to the queue.
// The property is checked across a batch of concurrently-live envelopes sharing
// one queue: bytes_used() always equals the sum of the not-yet-committed
// envelopes' trx_length, and returns to 0 once every envelope has committed.
//
// The MySQL tree does not integrate a property-testing library, so the property
// is expressed as a deterministically seeded randomized-sequence generator run
// over many trials. The seed is fixed and echoed in every assertion message so
// any failure reproduces exactly. Because Transaction_envelope is non-movable,
// the live envelopes are pinned on the heap and held via movable unique_ptr
// handles in a vector; the single queue is declared first so it outlives them
// all. Envelopes are committed directly (no seal step).
TEST(ImrTransactionEnvelopeTest, PropertyPayloadLifetime) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0x9A710AD5u;
  constexpr int kTrials = 400;
  constexpr int kEnvelopesPerTrial = 12;

  // A large limit so attaching a payload never blocks; the spill threshold is
  // not exercised (payloads are attached directly).
  const std::size_t memory_limit = 1u << 24;      // 16 MiB
  const std::size_t spill_threshold = 1u << 16;   // 64 KiB

  std::mt19937 rng(kSeed);
  // Payload sizes stay well under spill_threshold and include 0.
  std::uniform_int_distribution<std::size_t> len_dist(0, (1u << 16) - 1);

  for (int trial = 0; trial < kTrials; ++trial) {
    // One real queue, declared first so it outlives every envelope below.
    Trx_envelope_queue queue(memory_limit, spill_threshold);

    // Pinned, still-live envelopes and their reserved byte sizes, tracked as an
    // independent model of bytes_used() (sum of not-yet-committed lengths).
    std::vector<std::unique_ptr<Transaction_envelope>> live;
    std::size_t expected = 0;

    ASSERT_EQ(queue.bytes_used(), expected)
        << "seed=" << kSeed << " trial=" << trial << " (initial)";

    for (int i = 0; i < kEnvelopesPerTrial; ++i) {
      const std::size_t len = len_dist(rng);
      const std::size_t before = queue.bytes_used();

      auto env = std::make_unique<Transaction_envelope>(
          static_cast<std::uint64_t>(i + 1), len, Envelope_path::MEMORY);
      env->attach_payload(make_payload(&queue, len));

      // Req 9.1: non-null payload from admission, and bytes_used rose by exactly
      // this envelope's declared length.
      ASSERT_NE(env->payload(), nullptr)
          << "seed=" << kSeed << " trial=" << trial << " i=" << i;
      ASSERT_EQ(queue.bytes_used(), before + len)
          << "seed=" << kSeed << " trial=" << trial << " i=" << i
          << " len=" << len;
      // Uncommitted while its bytes are still reserved.
      ASSERT_FALSE(env->is_committed())
          << "seed=" << kSeed << " trial=" << trial << " i=" << i;

      live.push_back(std::move(env));
      expected += len;

      // bytes_used() equals the model (sum of live, not-yet-committed lengths).
      ASSERT_EQ(queue.bytes_used(), expected)
          << "seed=" << kSeed << " trial=" << trial << " i=" << i;
    }

    // Commit every live envelope in a randomized order. Each commit must null
    // the payload and release exactly that envelope's reserved bytes.
    std::shuffle(live.begin(), live.end(), rng);
    for (std::size_t k = 0; k < live.size(); ++k) {
      Transaction_envelope *env = live[k].get();
      const std::size_t len = env->trx_length();
      const std::size_t before = queue.bytes_used();

      // Non-null right up to the commit call.
      ASSERT_NE(env->payload(), nullptr)
          << "seed=" << kSeed << " trial=" << trial << " k=" << k;
      ASSERT_FALSE(env->is_committed())
          << "seed=" << kSeed << " trial=" << trial << " k=" << k;

      // commit() succeeds (false) from uncommitted (no seal precondition).
      ASSERT_FALSE(env->commit())
          << "seed=" << kSeed << " trial=" << trial << " k=" << k;

      // Req 9.2 / 9.5: after commit the payload is null and the reserved bytes
      // were released (bytes_used dropped by exactly this envelope's length).
      ASSERT_TRUE(env->is_committed())
          << "seed=" << kSeed << " trial=" << trial << " k=" << k;
      ASSERT_EQ(env->payload(), nullptr)
          << "seed=" << kSeed << " trial=" << trial << " k=" << k;
      ASSERT_EQ(queue.bytes_used(), before - len)
          << "seed=" << kSeed << " trial=" << trial << " k=" << k
          << " len=" << len;

      expected -= len;
      ASSERT_EQ(queue.bytes_used(), expected)
          << "seed=" << kSeed << " trial=" << trial << " k=" << k;
    }

    // Every envelope committed: all reserved bytes released.
    ASSERT_EQ(queue.bytes_used(), 0u)
        << "seed=" << kSeed << " trial=" << trial << " (all committed)";
  }
}

// ---------------------------------------------------------------------------
// Task 5 - Spill-path destination: zero-reservation, live sink.
// ---------------------------------------------------------------------------

// create_spill_destination() attaches a live spill payload whose sink is
// reachable, reserves NO bytes against the queue counter (Req 3.7), and — when
// the payload is dropped — leaves the counter at zero.
TEST(ImrTransactionEnvelopeTest, CreateSpillDestinationLiveSinkNoReservation) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold, relay_dir.path);
  const std::size_t before = queue.bytes_used();

  Transaction_envelope env(1, kTrxLength, Envelope_path::SPILL);
  env.create_spill_destination(/*is_trx=*/true, make_fde(), &queue);

  // A payload and a reachable sink now exist.
  ASSERT_NE(env.payload(), nullptr);
  Streaming_event_sink *sink = env.current_sink();
  ASSERT_NE(sink, nullptr);
  auto *spill = dynamic_cast<Event_set_fetchable_spill *>(sink);
  ASSERT_NE(spill, nullptr) << "spill destination must expose a spill sink";
  EXPECT_FALSE(spill->is_error()) << spill->get_error_str();
  EXPECT_FALSE(spill->spill_file_name().empty());

  // Req 3.7: the spill path reserves zero bytes — the counter is unchanged and
  // the payload's accounted size is zero.
  EXPECT_EQ(queue.bytes_used(), before);
  EXPECT_EQ(env.payload()->byte_size(), static_cast<std::size_t>(0));

  // Dropping the payload releases zero, leaving the counter at zero.
  env.reset_payload();
  EXPECT_EQ(env.payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), before);
}

}  // namespace mysql::csa::unittests
