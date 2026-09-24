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

#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"  // Format_description_log_event

namespace mysql::csa::unittests {

namespace {
/// A short bounded wait used to observe that a background admission call is
/// still blocked, without relying on a fixed sleep for correctness.
constexpr std::chrono::milliseconds kShortWait{50};

/// Generous upper bound for a positive "must unblock" observation.
constexpr std::chrono::seconds kLongWait{3};

/// Memory bounds large enough that construction never itself blocks.
constexpr std::size_t kMemoryLimit = 1u << 20;   // 1 MiB
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

// ---------------------------------------------------------------------------
// Task 3.2 - Unit tests: Trx_payload accounting.
// Requirements 1.1, 1.2, 1.3, 1.4, 1.5, 1.6
// ---------------------------------------------------------------------------

// Req 1.1: constructing a payload reserves exactly trx_length bytes against the
// owning queue's memory-usage counter.
TEST(ImrTrxPayloadTest, ConstructionAddsTrxLength) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  const std::size_t trx_length = 4096;
  const std::size_t before = queue.bytes_used();

  {
    auto ft = std::make_shared<Fetchable_transaction>();
    Trx_payload payload(ft, trx_length, &queue);
    // Counter increased by exactly trx_length while the payload is alive.
    EXPECT_EQ(queue.bytes_used(), before + trx_length);
  }
}

// Req 1.2: destroying a payload subtracts exactly the reserved amount, so the
// counter returns to its prior value.
TEST(ImrTrxPayloadTest, DestructionSubtractsTrxLength) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  const std::size_t trx_length = 4096;
  const std::size_t before = queue.bytes_used();

  {
    auto ft = std::make_shared<Fetchable_transaction>();
    Trx_payload payload(ft, trx_length, &queue);
    ASSERT_EQ(queue.bytes_used(), before + trx_length);
  }

  // Back to baseline after destruction.
  EXPECT_EQ(queue.bytes_used(), before);
}

// Req 1.3: destruction notifies the memory-availability condition variable, so
// a thread blocked in acquire_admission wakes and succeeds once the payload's
// destructor frees enough room. Verified behaviorally.
TEST(ImrTrxPayloadTest, DestructionNotifiesBlockedWaiter) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // Fill the budget to exactly the limit using a payload, leaving no room.
  // A request for trx_length bytes then blocks until this payload is destroyed.
  const std::size_t trx_length = kMemoryLimit / 2;
  auto ft = std::make_shared<Fetchable_transaction>();
  auto payload = std::make_unique<Trx_payload>(ft, kMemoryLimit, &queue);
  ASSERT_EQ(queue.bytes_used(), kMemoryLimit);

  std::atomic<bool> returned{false};
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();

  std::thread waiter([&] {
    const bool failed = queue.acquire_admission(trx_length);
    returned.store(true);
    result_promise.set_value(failed);
  });

  // Still blocked: the budget is full and nothing has been released.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_FALSE(returned.load());

  // Destroying the payload releases kMemoryLimit bytes and must wake the
  // waiter, which then re-evaluates the predicate and acquires admission.
  payload.reset();

  ASSERT_EQ(result_future.wait_for(kLongWait), std::future_status::ready)
      << "payload destruction must notify the blocked admission waiter";
  // false == success (admission acquired).
  EXPECT_FALSE(result_future.get());

  waiter.join();
  // The waiter reserved nothing itself; the queue is empty again.
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Req 1.5: byte_size() returns the trx_length the payload was constructed with.
TEST(ImrTrxPayloadTest, ByteSizeReturnsConstructedLength) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  const std::size_t trx_length = 12345;
  auto ft = std::make_shared<Fetchable_transaction>();
  Trx_payload payload(ft, trx_length, &queue);

  EXPECT_EQ(payload.byte_size(), trx_length);
}

// Req 1.4: fetchable() returns a non-null shared_ptr that shares ownership of
// the same Fetchable_transaction the payload was constructed with.
TEST(ImrTrxPayloadTest, FetchableSharesOwnership) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  auto ft = std::make_shared<Fetchable_transaction>();
  const long use_count_before = ft.use_count();

  {
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

// Req 1.4/1.5 (compile-time): copy and move are deleted. The payload is pinned
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

// Req 1.6: a payload releases its bytes exactly once, only on destruction.
// Since the payload is neither copyable nor movable and owns its reservation
// for its whole lifetime, a single construct/destruct cycle returns the counter
// to its baseline with no residual and no double release. (Repeated across a
// batch of payloads to guard against any drift.)
TEST(ImrTrxPayloadTest, ReleaseHappensExactlyOnceOnDestruction) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.
  const std::size_t baseline = queue.bytes_used();

  for (int i = 0; i < 8; ++i) {
    const std::size_t trx_length = 1024 * (i + 1);
    {
      auto ft = std::make_shared<Fetchable_transaction>();
      Trx_payload payload(ft, trx_length, &queue);
      ASSERT_EQ(queue.bytes_used(), baseline + trx_length);
    }
    // Destruction released exactly trx_length: back to baseline every cycle.
    EXPECT_EQ(queue.bytes_used(), baseline);
  }
}

// ---------------------------------------------------------------------------
// Task 3.3 - Property test: counter conservation.
// Validates: Requirements 1.1, 1.2, 6.5, 9.5
// ---------------------------------------------------------------------------

// Property 1 (Counter conservation): over a randomized multiset of Trx_payload
// lifetimes constructed and destroyed in RANDOM order, each construction
// reserves exactly its trx_length and each destruction releases exactly the
// amount its payload had reserved - independent of the order in which payloads
// are created versus destroyed. The queue's bytes_used() therefore always
// equals the sum of the byte_size() of the currently-live payloads, and once
// every payload has been destroyed bytes_used() returns to 0 (Req 6.5, 9.5).
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

  // A large limit that this direct-construction test never approaches; the
  // spill_threshold is irrelevant here (payloads are constructed directly, not
  // routed through classify/acquire_admission).
  const std::size_t memory_limit = 1u << 30;
  const std::size_t spill_threshold = 1u << 16;

  std::mt19937 rng(kSeed);
  // trx_length range includes 0 and spans a wide interval well within the
  // limit, so many payloads can be live simultaneously.
  std::uniform_int_distribution<std::size_t> len_dist(0, 1u << 20);
  // Bias toward construction so the live set grows and shrinks repeatedly.
  std::uniform_int_distribution<int> action_dist(0, 2);

  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // Live payloads, pinned on the heap. The independent model `expected` mirrors
  // the sum of the live payloads' byte_size() and must equal bytes_used() after
  // every single operation.
  std::vector<std::unique_ptr<Trx_payload>> live;
  std::size_t expected = 0;

  ASSERT_EQ(queue.bytes_used(), expected) << "seed=" << kSeed;

  for (int trial = 0; trial < kTrials; ++trial) {
    const bool do_construct = live.empty() || action_dist(rng) != 0;

    if (do_construct) {
      // CONSTRUCT: reserving exactly `len` must raise bytes_used() by exactly
      // `len` (Req 1.1).
      const std::size_t len = len_dist(rng);
      const std::size_t before = queue.bytes_used();

      auto ft = std::make_shared<Fetchable_transaction>();
      live.push_back(std::make_unique<Trx_payload>(ft, len, &queue));
      expected += len;

      ASSERT_EQ(queue.bytes_used(), before + len)
          << "seed=" << kSeed << " trial=" << trial << " len=" << len;
      ASSERT_EQ(queue.bytes_used(), expected)
          << "seed=" << kSeed << " trial=" << trial << " len=" << len;
    } else {
      // DESTROY a random live payload: destruction must lower bytes_used() by
      // exactly the amount that payload reserved at construction (Req 1.2),
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
  // fully reclaimed to zero (Req 6.5, 9.5).
  live.clear();
  expected = 0;
  ASSERT_EQ(queue.bytes_used(), expected) << "seed=" << kSeed;
}

// ---------------------------------------------------------------------------
// Task 5 - Zero-reservation spill payload.
// ---------------------------------------------------------------------------

// create_spill() builds a live spill payload whose sink is reachable and whose
// backing file exists, reserves ZERO bytes against the queue counter (Req 3.7),
// reports byte_size() == 0, and — on destruction — releases zero (the counter
// never moves off its prior value).
TEST(ImrTrxPayloadTest, CreateSpillReservesZeroAndExposesSink) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold, relay_dir.path);
  const std::size_t before = queue.bytes_used();

  // A standalone owning envelope for the spill source's commit/truncate target.
  Transaction_envelope env(1, /*trx_length=*/0, Envelope_path::SPILL);

  {
    auto payload = Trx_payload::create_spill(/*is_trx=*/true, make_fde(), &queue,
                                             &env);
    ASSERT_NE(payload, nullptr);

    // Zero reservation (Req 3.7).
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

}  // namespace mysql::csa::unittests
