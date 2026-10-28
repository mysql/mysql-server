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
/// Unit tests for mysql::csa::Trx_payload, the RAII memory-accounted holder of
/// one transaction's byte stream. The tests use a real Trx_envelope_queue as
/// the accounting backend and a default-constructed Fetchable_transaction as
/// the wrapped byte source.

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
#include <type_traits>
#include <vector>

#include "mysql/scheduler/statistics_monitor.h"  // Statistics_monitor
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/resource/statistics_map.h"  // Statistics_map
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // ensure_spill_directory
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"  // Format_description_log_event
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {
/// A short bounded wait used to observe that a background admission call is
/// still blocked, without relying on a fixed sleep for correctness.
constexpr std::chrono::milliseconds kShortWait{50};

/// Generous upper bound for a positive "must unblock" observation.
constexpr std::chrono::seconds kLongWait{3};

/// Memory bounds large enough that construction never itself blocks.
constexpr std::size_t kMemoryLimit = 1u << 20;     // 1 MiB
constexpr std::size_t kSpillThreshold = 1u << 16;  // 64 KiB

/// RAII private "relay log directory" for spill-path tests; removed on scope
/// exit (with the spill files and temp-files subdir under it).
struct Scoped_temp_dir {
  std::string path;
  Scoped_temp_dir() {
    static std::atomic<unsigned> counter{0};
    path = (std::filesystem::temp_directory_path() /
            ("imr_payload_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1))))
               .string();
    std::filesystem::create_directories(path);
    // The spill-files subdirectory is provisioned once at server start by
    // ensure_spill_directory() (from init_replica); the queue no longer creates
    // it lazily on the first spill. This harness bypasses server startup, so
    // provision it here through the same production entry point.
    (void)ensure_spill_directory(path);
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

/// Reserve @p trx_length on @p queue's memory path, the same step
/// Trx_payload::create_memory() performs before constructing a payload. The
/// Trx_payload constructor does no accounting but its destructor releases
/// trx_length, so a directly constructed payload must be backed by this
/// reservation. The reservation must fit (the call is the non-blocking fast
/// path here) and the queue must be resumed, since admission rejects on a
/// receiver stop.
void reserve(Trx_envelope_queue &queue, std::size_t trx_length) {
  ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::MEMORY))
      << "memory admission of " << trx_length << " bytes must succeed";
}
}  // namespace

// ---------------------------------------------------------------------------
// Unit tests: Trx_payload accounting.
// ---------------------------------------------------------------------------

// Memory admission reserves exactly trx_length bytes, and the payload
// built on that reservation holds it for its lifetime. The constructor itself
// adds nothing (admission already did).
TEST(ImrTrxPayloadTest, AdmissionReservesTrxLengthHeldByPayload) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // admission rejects on a receiver stop; arm the queue.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  const std::size_t trx_length = 4096;
  const std::size_t before = queue.bytes_used();

  reserve(queue, trx_length);
  // Admission took the reservation.
  ASSERT_EQ(queue.bytes_used(), before + trx_length);
  {
    auto ft = std::make_shared<Fetchable_transaction>();
    Trx_payload payload(ft, trx_length, &queue);
    // Construction adds nothing on top of the admission reservation.
    EXPECT_EQ(queue.bytes_used(), before + trx_length);
  }
}

// Destroying a payload releases exactly the reserved amount, so the
// counter returns to its value before admission.
TEST(ImrTrxPayloadTest, DestructionReleasesTrxLength) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // admission rejects on a receiver stop; arm the queue.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  const std::size_t trx_length = 4096;
  const std::size_t before = queue.bytes_used();

  reserve(queue, trx_length);
  {
    auto ft = std::make_shared<Fetchable_transaction>();
    Trx_payload payload(ft, trx_length, &queue);
    ASSERT_EQ(queue.bytes_used(), before + trx_length);
  }

  // Back to baseline after destruction.
  EXPECT_EQ(queue.bytes_used(), before);
}

// A payload's destruction frees budget that a receiver parked in
// acquire_admission then takes. The release is wait-free (no condition
// variable); the parked receiver sees the freed budget on its next 1 ms poll.
// Verified behaviorally.
TEST(ImrTrxPayloadTest, DestructionUnblocksParkedAdmission) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // admission rejects on a receiver stop; arm the queue.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  // Fill the budget to exactly the limit with one payload, leaving no room.
  // A request for trx_length bytes then parks until this payload is destroyed.
  const std::size_t trx_length = kMemoryLimit / 2;
  reserve(queue, kMemoryLimit);
  auto ft = std::make_shared<Fetchable_transaction>();
  auto payload = std::make_unique<Trx_payload>(ft, kMemoryLimit, &queue);
  ASSERT_EQ(queue.bytes_used(), kMemoryLimit);

  std::atomic<bool> returned{false};
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();

  std::thread waiter([&] {
    const bool failed =
        queue.acquire_admission(trx_length, Envelope_path::MEMORY);
    returned.store(true);
    result_promise.set_value(failed);
  });

  // Still blocked: the budget is full and nothing has been released.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_FALSE(returned.load());

  // Destroying the payload releases kMemoryLimit bytes; the parked waiter's
  // next poll then fits and acquires admission.
  payload.reset();

  ASSERT_EQ(result_future.wait_for(kLongWait), std::future_status::ready)
      << "payload destruction must unblock the parked admission";
  // false == success (admission acquired).
  EXPECT_FALSE(result_future.get());

  waiter.join();
  // The waiter's successful admission now holds its own reservation.
  EXPECT_EQ(queue.bytes_used(), trx_length);
  queue.release_bytes(trx_length);  // no payload owns it; undo by hand.
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// byte_size() returns the trx_length the payload was constructed with.
TEST(ImrTrxPayloadTest, ByteSizeReturnsConstructedLength) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // admission rejects on a receiver stop; arm the queue.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  const std::size_t trx_length = 12345;
  reserve(queue, trx_length);  // the destructor releases trx_length.
  auto ft = std::make_shared<Fetchable_transaction>();
  Trx_payload payload(ft, trx_length, &queue);

  EXPECT_EQ(payload.byte_size(), trx_length);
}

// fetchable() returns a non-null shared_ptr that shares ownership of
// the same Fetchable_transaction the payload was constructed with.
TEST(ImrTrxPayloadTest, FetchableSharesOwnership) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  auto ft = std::make_shared<Fetchable_transaction>();
  const long use_count_before = ft.use_count();

  {
    reserve(queue, 1024);  // the destructor releases 1024.
    Trx_payload payload(ft, 1024, &queue);
    // Holding the payload adds a shared owner.
    EXPECT_GT(ft.use_count(), use_count_before);

    std::shared_ptr<Fetchable_transaction> got = payload.fetchable();
    ASSERT_NE(got, nullptr);
    // Same underlying object.
    EXPECT_EQ(got.get(), ft.get());
  }

  // The payload dropped its reference; only the test's `ft` remains.
  EXPECT_EQ(ft.use_count(), use_count_before);
}

// Compile-time: copy and move are deleted. The payload is pinned
// inside its owning envelope, so none of these operations may be available.
static_assert(!std::is_copy_constructible_v<Trx_payload>,
              "Trx_payload must not be copy constructible");
static_assert(!std::is_copy_assignable_v<Trx_payload>,
              "Trx_payload must not be copy assignable");
static_assert(!std::is_move_constructible_v<Trx_payload>,
              "Trx_payload must not be move constructible");
static_assert(!std::is_move_assignable_v<Trx_payload>,
              "Trx_payload must not be move assignable");

// A runtime test carrying the same static assertions so the check is visible in
// the test report as well.
TEST(ImrTrxPayloadTest, CopyAndMoveAreDeleted) {
  EXPECT_FALSE(std::is_copy_constructible_v<Trx_payload>);
  EXPECT_FALSE(std::is_copy_assignable_v<Trx_payload>);
  EXPECT_FALSE(std::is_move_constructible_v<Trx_payload>);
  EXPECT_FALSE(std::is_move_assignable_v<Trx_payload>);
}

// A payload releases its bytes exactly once, only on destruction.
// Since the payload is neither copyable nor movable and owns its reservation
// for its whole lifetime, a single admit/construct/destruct cycle returns the
// counter to its baseline with no residual and no double release. (Repeated
// across a batch of payloads to guard against any drift.)
TEST(ImrTrxPayloadTest, ReleaseHappensExactlyOnceOnDestruction) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // admission rejects on a receiver stop; arm the queue.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  const std::size_t baseline = queue.bytes_used();

  for (int i = 0; i < 8; ++i) {
    const std::size_t trx_length = 1024 * (i + 1);
    {
      reserve(queue, trx_length);
      auto ft = std::make_shared<Fetchable_transaction>();
      Trx_payload payload(ft, trx_length, &queue);
      ASSERT_EQ(queue.bytes_used(), baseline + trx_length);
    }
    // Destruction released exactly trx_length: back to baseline every cycle.
    EXPECT_EQ(queue.bytes_used(), baseline);
  }
}

// ---------------------------------------------------------------------------
// Property test: counter conservation.
// ---------------------------------------------------------------------------

// Counter conservation property: over a randomized multiset of Trx_payload
// lifetimes created and destroyed in RANDOM order, each admission reserves
// exactly its trx_length (the payload is then built on that reservation, as
// create_memory() does) and each destruction releases exactly the amount its
// payload held - independent of the order in which payloads are created versus
// destroyed. The queue's bytes_used() therefore always
// equals the sum of the byte_size() of the currently-live payloads, and once
// every payload has been destroyed bytes_used() returns to 0.
//
// The MySQL tree does not integrate a property-testing library, so the property
// is expressed as a deterministically seeded randomized-sequence generator run
// over many trials. The seed is fixed and echoed in every assertion message so
// any failure reproduces exactly. Because Trx_payload is neither copyable nor
// movable, live payloads are pinned on the heap and held via a movable
// unique_ptr handle so they can be stored in a vector and erased in random
// order.
TEST(ImrTrxPayloadTest, PropertyCounterConservation) {
  // Fixed, deterministic seed so any failure reproduces exactly.
  constexpr std::uint32_t kSeed = 0x5EED1A9Cu;
  constexpr int kTrials = 5000;

  // A large limit this test never approaches, so every admission takes the
  // non-blocking fast path; the spill_threshold is irrelevant here (admission
  // is driven directly on the MEMORY path, not routed through classify).
  const std::size_t memory_limit = 1u << 30;
  const std::size_t spill_threshold = 1u << 16;

  std::mt19937 rng(kSeed);
  // trx_length range includes 0 and spans a wide interval well within the
  // limit, so many payloads can be live simultaneously.
  std::uniform_int_distribution<std::size_t> len_dist(0, 1u << 20);
  // Bias toward construction so the live set grows and shrinks repeatedly.
  std::uniform_int_distribution<int> action_dist(0, 2);

  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  // Live payloads, pinned on the heap. The independent model `expected` mirrors
  // the sum of the live payloads' byte_size() and must equal bytes_used() after
  // every single operation.
  std::vector<std::unique_ptr<Trx_payload>> live;
  std::size_t expected = 0;

  ASSERT_EQ(queue.bytes_used(), expected) << "seed=" << kSeed;

  for (int trial = 0; trial < kTrials; ++trial) {
    const bool do_construct = live.empty() || action_dist(rng) != 0;

    if (do_construct) {
      // ADMIT + CONSTRUCT: reserving exactly `len` must raise bytes_used() by
      // exactly `len`.
      const std::size_t len = len_dist(rng);
      const std::size_t before = queue.bytes_used();

      reserve(queue, len);
      auto ft = std::make_shared<Fetchable_transaction>();
      live.push_back(std::make_unique<Trx_payload>(ft, len, &queue));
      expected += len;

      ASSERT_EQ(queue.bytes_used(), before + len)
          << "seed=" << kSeed << " trial=" << trial << " len=" << len;
      ASSERT_EQ(queue.bytes_used(), expected)
          << "seed=" << kSeed << " trial=" << trial << " len=" << len;
    } else {
      // DESTROY a random live payload: destruction must lower bytes_used() by
      // exactly the amount that payload reserved at construction,
      // regardless of construction order.
      std::uniform_int_distribution<std::size_t> idx_dist(0, live.size() - 1);
      const std::size_t idx = idx_dist(rng);
      const std::size_t amount = live[idx]->byte_size();
      const std::size_t before = queue.bytes_used();

      // Swap-and-pop: moving the unique_ptr handle does not move the pinned
      // payload; popping it destroys the payload and releases its bytes.
      live[idx] = std::move(live.back());
      live.pop_back();
      expected -= amount;

      ASSERT_EQ(queue.bytes_used(), before - amount)
          << "seed=" << kSeed << " trial=" << trial << " amount=" << amount;
      ASSERT_EQ(queue.bytes_used(), expected)
          << "seed=" << kSeed << " trial=" << trial << " amount=" << amount;
    }
  }

  // Destroy everything that remains: with no live payloads the counter must be
  // fully reclaimed to zero.
  live.clear();
  expected = 0;
  ASSERT_EQ(queue.bytes_used(), expected) << "seed=" << kSeed;
}

// ---------------------------------------------------------------------------
// Zero-reservation spill payload.
// ---------------------------------------------------------------------------

// create_spill() builds a live spill payload whose sink is reachable and whose
// backing file exists, reserves ZERO bytes against the queue counter,
// reports byte_size() == 0, and — on destruction — releases zero (the counter
// never moves off its prior value).
TEST(ImrTrxPayloadTest, CreateSpillReservesZeroAndExposesSink) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  // create_spill() runs spill admission, which rejects on a receiver stop; a
  // new queue starts receiver-stopped, so arm it first.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  const std::size_t before = queue.bytes_used();

  // A standalone owning envelope for the spill source's commit/truncate target.
  Transaction_envelope env(/*trx_length=*/0, Envelope_path::SPILL);

  {
    auto payload = Trx_payload::create_spill(
        /*is_trx=*/true, make_fde(), /*trx_length=*/kSpillThreshold + 1, &queue,
        &env);
    ASSERT_NE(payload, nullptr);

    // Zero reservation.
    EXPECT_EQ(payload->byte_size(), static_cast<std::size_t>(0));
    EXPECT_EQ(queue.bytes_used(), before);

    // Reachable, healthy spill sink over a real file.
    Streaming_event_sink *sink = payload->sink();
    ASSERT_NE(sink, nullptr);
    auto *spill = dynamic_cast<Event_set_fetchable_spill *>(sink);
    ASSERT_NE(spill, nullptr);
    EXPECT_FALSE(spill->is_error()) << spill->get_error_str();
    EXPECT_FALSE(spill->spill_file_name().empty());

    // A live wrapped Fetchable_transaction is shared out.
    EXPECT_NE(payload->fetchable(), nullptr);
  }

  // Payload destroyed: released zero, so the counter is unchanged.
  EXPECT_EQ(queue.bytes_used(), before);
}

// With a spill disk limit, create_spill() reserves the declared size on the
// spill disk entry, the reservation is held while the spill file exists, and
// it is returned when the payload (and so the file) is destroyed. The memory
// budget is never touched.
TEST(ImrTrxPayloadTest, CreateSpillHoldsDiskReservationWhileFileExists) {
  Scoped_temp_dir relay_dir;
  const std::size_t disk_limit = 4 * kSpillThreshold;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path,
                           /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  Transaction_envelope env(/*trx_length=*/0, Envelope_path::SPILL);
  const std::size_t trx_length = kSpillThreshold + 1;

  std::string file_name;
  {
    auto payload = Trx_payload::create_spill(/*is_trx=*/true, make_fde(),
                                             trx_length, &queue, &env);
    ASSERT_NE(payload, nullptr);
    auto *spill = dynamic_cast<Event_set_fetchable_spill *>(payload->sink());
    ASSERT_NE(spill, nullptr);
    file_name = spill->spill_file_name();
    ASSERT_TRUE(std::filesystem::exists(file_name));

    EXPECT_EQ(disk->m_available.load(),
              static_cast<long long>(disk_limit - trx_length));
    EXPECT_EQ(queue.bytes_used(), 0u);
  }

  // The file is gone and the whole reservation is back.
  EXPECT_FALSE(std::filesystem::exists(file_name));
  EXPECT_EQ(disk->m_available.load(), static_cast<long long>(disk_limit));
}

// A spill source whose file cannot be created still returns the disk
// reservation it was handed, when it is destroyed.
TEST(ImrTrxPayloadTest, FailedSpillProvisioningReturnsDiskReservation) {
  const std::size_t disk_limit = 4 * kSpillThreshold;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  // A relay log directory that does not exist: opening the spill file fails.
  const std::string missing_dir =
      (std::filesystem::temp_directory_path() /
       ("imr_payload_missing_" + std::to_string(::getpid())))
          .string();
  ASSERT_FALSE(std::filesystem::exists(missing_dir));
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), missing_dir,
                           /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  const std::size_t trx_length = kSpillThreshold + 1;
  ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::SPILL));
  EXPECT_EQ(disk->m_available.load(),
            static_cast<long long>(disk_limit - trx_length));
  {
    Event_set_fetchable_spill src(/*is_trx=*/true, make_fde(), missing_dir,
                                  /*owner_envelope=*/nullptr,
                                  /*streaming_open=*/true, /*channel_name=*/{},
                                  &queue, trx_length);
    EXPECT_TRUE(src.is_error());
  }
  EXPECT_EQ(disk->m_available.load(), static_cast<long long>(disk_limit));
}

namespace {
/// Coalesced value of a CSA statistic of @p instance_id, 0 when not registered.
long long imr_stat(std::size_t instance_id, const char *name) {
  auto stat = scheduler::Statistics_monitor::get(instance_id).find_stat(name);
  return stat.has_value() ? stat->get().get() : 0;
}

/// Complete one transaction through @p queue's real commit hook: admit an
/// envelope of @p trx_length on its path, then run the success callback
/// @p calls times, as the applier does at commit. Like the applier's job, the
/// test holds a reference to the Fetchable_transaction, so the byte source
/// outlives the payload reset that commit() performs.
void complete_through_hook(Trx_envelope_queue &queue, std::size_t trx_length,
                           Envelope_path path, int calls = 1) {
  Transaction_envelope env(trx_length, path);
  ASSERT_FALSE(env.create_payload(/*is_trx=*/true, make_fde(), &queue));
  ASSERT_NE(env.payload(), nullptr);
  std::shared_ptr<Fetchable_transaction> fetchable = env.payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  for (int i = 0; i < calls; ++i) fetchable->set_success();
  EXPECT_TRUE(env.is_committed());
}
}  // namespace

// The commit hooks count each completed transaction once, on its path, in the
// channel's CSA statistics: count and trx_length for the memory path and for
// the spill path. A repeated success callback does not count again.
TEST(ImrTrxPayloadTest, CommitHookCountsCompletedTransactionsByPath) {
  constexpr std::size_t kInstance = 9;
  ASSERT_FALSE(Statistics_map::init_statistics(
      kInstance, /*num_threads=*/1, /*enable_extended_statistics=*/false,
      /*enable_imr_statistics=*/true));
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path,
                           /*channel_name=*/{}, /*spill_disk_resource=*/nullptr,
                           kInstance);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  complete_through_hook(queue, 100, Envelope_path::MEMORY);
  complete_through_hook(queue, 200, Envelope_path::MEMORY, /*calls=*/2);
  complete_through_hook(queue, kSpillThreshold + 1, Envelope_path::SPILL);

  EXPECT_EQ(imr_stat(kInstance, Statistics_map::imr_mem_trx_cnt), 2);
  EXPECT_EQ(imr_stat(kInstance, Statistics_map::imr_mem_trx_len), 300);
  EXPECT_EQ(imr_stat(kInstance, Statistics_map::imr_spl_trx_cnt), 1);
  EXPECT_EQ(imr_stat(kInstance, Statistics_map::imr_spl_trx_len),
            static_cast<long long>(kSpillThreshold + 1));

  scheduler::Statistics_monitor::clear(kInstance);
}

// With applier metric collection off at registration, the counters stay 0.
TEST(ImrTrxPayloadTest, CommitHookCountsNothingWhenCollectionIsOff) {
  constexpr std::size_t kInstance = 10;
  ASSERT_FALSE(Statistics_map::init_statistics(
      kInstance, /*num_threads=*/1, /*enable_extended_statistics=*/false,
      /*enable_imr_statistics=*/false));
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path,
                           /*channel_name=*/{}, /*spill_disk_resource=*/nullptr,
                           kInstance);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  complete_through_hook(queue, 100, Envelope_path::MEMORY);
  complete_through_hook(queue, kSpillThreshold + 1, Envelope_path::SPILL);

  EXPECT_EQ(imr_stat(kInstance, Statistics_map::imr_mem_trx_cnt), 0);
  EXPECT_EQ(imr_stat(kInstance, Statistics_map::imr_spl_trx_cnt), 0);

  scheduler::Statistics_monitor::clear(kInstance);
}

}  // namespace mysql::csa::unittests
