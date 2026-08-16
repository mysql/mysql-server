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
/// Unit and randomized-sequence ("property") tests for the memory-accounting
/// and admission core of mysql::csa::Trx_envelope_queue.
///
/// The MySQL tree does not integrate a property-testing library (e.g.
/// rapidcheck), so the property test below is expressed as a deterministically
/// seeded randomized-sequence generator that runs many trials inside a plain
/// gtest TEST. The seed is fixed and echoed in every assertion message so any
/// failure reproduces exactly.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <numeric>
#include <random>
#include <vector>

#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"

namespace mysql::csa::unittests {

namespace {
/// A short bounded wait used to observe that a background admission call is (or
/// is not) still blocked, without relying on a fixed sleep for correctness.
constexpr std::chrono::milliseconds kShortWait{50};
}  // namespace

// ---------------------------------------------------------------------------
// Task 2.2 - Unit tests: classification and blocking admission.
// Requirements 4.1, 4.2, 4.3, 4.5, 4.6, 5.1, 5.2, 5.3, 5.4, 5.5
// ---------------------------------------------------------------------------

// Req 4.1: trx_length == spill_threshold is NOT spill (it fits the threshold),
// and classifies MEMORY when it fits the limit; trx_length == spill_threshold+1
// is SPILL regardless of usage.
TEST(ImrQueueAdmissionTest, ClassifySpillThresholdBoundary) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 100;
  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // Exactly at the threshold: not spill. With no usage it fits the limit.
  EXPECT_EQ(queue.classify(spill_threshold), Admission::MEMORY);
  // One byte over the threshold: spill, independent of usage.
  EXPECT_EQ(queue.classify(spill_threshold + 1), Admission::SPILL);
}

// Req 4.1: SPILL is returned independently of bytes_used - even when the queue
// is completely full a too-large transaction still classifies SPILL.
TEST(ImrQueueAdmissionTest, ClassifySpillIndependentOfUsage) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 100;
  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  queue.add_bytes(memory_limit);  // Fill the budget entirely.
  EXPECT_EQ(queue.bytes_used(), memory_limit);
  EXPECT_EQ(queue.classify(spill_threshold + 1), Admission::SPILL);

  // This test reserved bytes with add_bytes() without creating the payloads
  // whose destructors would release them, so release them here: the queue
  // destructor asserts bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// Req 4.2 / 4.3: at the memory_limit boundary the decision flips from MEMORY to
// WOULD_BLOCK. spill_threshold is set high so these lengths never route SPILL.
TEST(ImrQueueAdmissionTest, ClassifyMemoryLimitBoundary) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  queue.add_bytes(900);  // 900 bytes already reserved.
  ASSERT_EQ(queue.bytes_used(), 900u);

  // bytes_used + trx_length == memory_limit -> MEMORY.
  EXPECT_EQ(queue.classify(100), Admission::MEMORY);
  // bytes_used + trx_length == memory_limit + 1 -> WOULD_BLOCK.
  EXPECT_EQ(queue.classify(101), Admission::WOULD_BLOCK);

  // Release what add_bytes() reserved: the queue destructor asserts
  // bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// Req 4.5 / 4.6: classify is pure - it never mutates bytes_used, and repeated
// calls with identical inputs yield identical results.
TEST(ImrQueueAdmissionTest, ClassifyIsPure) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  queue.add_bytes(200);
  const std::size_t before = queue.bytes_used();

  const Admission first = queue.classify(300);
  const Admission second = queue.classify(300);

  // Deterministic result for identical inputs (Req 4.5).
  EXPECT_EQ(first, second);
  // No observable state mutation (Req 4.6).
  EXPECT_EQ(queue.bytes_used(), before);

  // Repeated calls at different sizes still leave the counter untouched.
  (void)queue.classify(700);
  (void)queue.classify(1500);
  EXPECT_EQ(queue.bytes_used(), before);

  // Release what add_bytes() reserved: the queue destructor asserts
  // bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// Req 5.2 / 5.3: acquire_admission succeeds immediately (returns false) when the
// reservation fits, and the call itself reserves nothing (the Trx_payload ctor
// reserves later).
TEST(ImrQueueAdmissionTest, AcquireAdmissionSucceedsWhenItFits) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // Run under a timeout so a regression that blocks here fails fast.
  std::future<bool> failed = std::async(
      std::launch::async, [&queue] { return queue.acquire_admission(100); });
  ASSERT_EQ(failed.wait_for(std::chrono::seconds(3)), std::future_status::ready)
      << "acquire_admission must return immediately when the reservation fits";
  // false == success.
  EXPECT_FALSE(failed.get());

  // acquire_admission does not itself reserve any bytes.
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Req 5.1 / 5.4: acquire_admission blocks while over the limit, then is
// unblocked and succeeds (returns false) once release_bytes frees enough room.
TEST(ImrQueueAdmissionTest, AcquireAdmissionBlocksThenUnblocksOnRelease) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  queue.add_bytes(memory_limit);  // Fully occupied: a 100-byte request blocks.

  std::atomic<bool> returned{false};
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();

  std::thread waiter([&] {
    const bool failed = queue.acquire_admission(100);
    returned.store(true);
    result_promise.set_value(failed);
  });

  // The call must still be blocked: nothing has been released yet.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_FALSE(returned.load());

  // Partial release: freeing only 50 bytes leaves bytes_used at 950, so the
  // 100-byte request still does not fit (950 + 100 > 1000). The waiter must
  // wake, re-evaluate the predicate, and keep blocking.
  queue.release_bytes(50);
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_FALSE(returned.load());

  // Second release brings bytes_used to 900, so 900 + 100 == memory_limit fits
  // and the waiter must now acquire admission.
  queue.release_bytes(50);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "acquire_admission must return once release_bytes frees enough room";
  // false == success.
  EXPECT_FALSE(result_future.get());

  waiter.join();
  // The waiter reserved nothing itself; only the two releases changed the
  // counter (1000 - 50 - 50).
  EXPECT_EQ(queue.bytes_used(), memory_limit - 100);

  // Release the rest of what add_bytes() reserved: the queue destructor asserts
  // bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// Req 5.5: a stop while blocked fails (returns true) and leaves bytes_used
// unchanged (no bytes reserved).
TEST(ImrQueueAdmissionTest, AcquireAdmissionFailsOnStop) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  queue.add_bytes(memory_limit);  // Fully occupied: the request will block.
  const std::size_t used_before = queue.bytes_used();

  std::atomic<bool> returned{false};
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();

  std::thread waiter([&] {
    const bool failed = queue.acquire_admission(100);
    returned.store(true);
    result_promise.set_value(failed);
  });

  // Confirm it is blocked before requesting stop.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_FALSE(returned.load());

  queue.stop();
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "acquire_admission must return after stop()";
  // true == failure (stopped, not acquired).
  EXPECT_TRUE(result_future.get());

  waiter.join();
  EXPECT_TRUE(queue.is_stopped());
  // bytes_used unchanged by the stopped, failed admission attempt.
  EXPECT_EQ(queue.bytes_used(), used_before);

  // Release what add_bytes() reserved: the queue destructor asserts
  // bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// ---------------------------------------------------------------------------
// Task 2.3 - Property test: the memory bound.
// Validates: Requirements 4.1, 4.2, 4.3, 5.2, 5.6
// ---------------------------------------------------------------------------

// Property 2 (Memory bound): over a randomized single-producer admit/release
// sequence, an admitted memory-path reservation never lets bytes_used exceed
// memory_limit; every trx_length > spill_threshold classifies SPILL regardless
// of the current usage; and bytes_used always equals the sum of live
// reservations, returning to 0 once everything is released.
TEST(ImrQueueAdmissionTest, PropertyMemoryBound) {
  // Fixed, deterministic seed so any failure reproduces exactly. It is echoed
  // in every assertion message below.
  constexpr std::uint32_t kSeed = 0xC0FFEEu;
  constexpr int kTrials = 4000;

  const std::size_t memory_limit = 10000;
  const std::size_t spill_threshold = 500;

  std::mt19937 rng(kSeed);
  // Range spans well above spill_threshold so SPILL is exercised often, and
  // includes 0 and both boundary values.
  std::uniform_int_distribution<std::size_t> len_dist(0, 1000);
  std::uniform_int_distribution<int> action_dist(0, 3);

  Trx_envelope_queue queue(memory_limit, spill_threshold);
  queue.resume();  // mi-owned queue defaults to stopped; arm it for the test.

  // Multiset of live reservations currently counted in bytes_used.
  std::vector<std::size_t> live;

  auto sum_live = [&live]() {
    return std::accumulate(live.begin(), live.end(), std::size_t{0});
  };

  for (int trial = 0; trial < kTrials; ++trial) {
    const std::size_t trx_length = len_dist(rng);
    const Admission decision = queue.classify(trx_length);

    // Every over-threshold length must classify SPILL, independent of usage
    // (Req 4.1).
    if (trx_length > spill_threshold) {
      ASSERT_EQ(decision, Admission::SPILL)
          << "seed=" << kSeed << " trial=" << trial
          << " trx_length=" << trx_length;
    }

    if (decision == Admission::SPILL) {
      // Spill path does not touch the memory counter: leave `live` and the
      // counter untouched (verified by the invariant check at the loop end).
    } else {
      // Non-blocking admission: admit only when it actually fits (single
      // threaded, so we drive admission without blocking). classify already
      // encodes this, cross-check with the raw bound (Req 4.2/4.3).
      const bool fits = queue.bytes_used() + trx_length <= memory_limit;
      ASSERT_EQ(decision == Admission::MEMORY, fits)
          << "seed=" << kSeed << " trial=" << trial
          << " trx_length=" << trx_length
          << " bytes_used=" << queue.bytes_used();
      if (decision == Admission::MEMORY) {
        queue.add_bytes(trx_length);
        live.push_back(trx_length);
        // Memory bound holds at every admission point (Req 5.2/5.6).
        ASSERT_LE(queue.bytes_used(), memory_limit)
            << "seed=" << kSeed << " trial=" << trial
            << " trx_length=" << trx_length;
      }
    }

    // Randomly release a previously-admitted reservation.
    if (!live.empty() && action_dist(rng) == 0) {
      std::uniform_int_distribution<std::size_t> idx_dist(0, live.size() - 1);
      const std::size_t idx = idx_dist(rng);
      const std::size_t amount = live[idx];
      live[idx] = live.back();
      live.pop_back();
      queue.release_bytes(amount);
    }

    // Counter always equals the sum of live reservations (Req 5.6).
    ASSERT_EQ(queue.bytes_used(), sum_live())
        << "seed=" << kSeed << " trial=" << trial;
  }

  // Release everything that remains; the counter must return to zero.
  for (const std::size_t amount : live) {
    queue.release_bytes(amount);
  }
  live.clear();
  ASSERT_EQ(queue.bytes_used(), 0u) << "seed=" << kSeed;
}

}  // namespace mysql::csa::unittests
