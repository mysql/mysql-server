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
#include <thread>
#include <utility>
#include <vector>

#include "sql/changestreams/apply/resource/resource_monitor.h"  // Resource_instance_monitor
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {
/// A short bounded wait used to observe that a background admission call is (or
/// is not) still blocked, without relying on a fixed sleep for correctness.
constexpr std::chrono::milliseconds kShortWait{50};
}  // namespace

// ---------------------------------------------------------------------------
// Unit tests: classification and blocking admission.
// ---------------------------------------------------------------------------

// trx_length == spill_threshold is NOT spill (it fits the threshold),
// and classifies MEMORY when it fits the limit; trx_length == spill_threshold+1
// is SPILL regardless of usage.
TEST(ImrQueueAdmissionTest, ClassifySpillThresholdBoundary) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 100;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));

  // Exactly at the threshold: not spill. With no usage it fits the limit.
  EXPECT_EQ(queue.classify(spill_threshold), Envelope_path::MEMORY);
  // One byte over the threshold: spill, independent of usage.
  EXPECT_EQ(queue.classify(spill_threshold + 1), Envelope_path::SPILL);
}

// SPILL is returned independently of bytes_used - even when the queue
// is completely full a too-large transaction still classifies SPILL.
TEST(ImrQueueAdmissionTest, ClassifySpillIndependentOfUsage) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 100;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));

  queue.add_bytes(memory_limit);  // Fill the budget entirely.
  EXPECT_EQ(queue.bytes_used(), memory_limit);
  EXPECT_EQ(queue.classify(spill_threshold + 1), Envelope_path::SPILL);

  // This test reserved bytes with add_bytes() without creating the payloads
  // whose destructors would release them, so release them here: the queue
  // destructor asserts bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// classify() only routes: any length within the spill threshold is MEMORY,
// whatever the current usage. Back-pressure is applied by the monitor inside
// acquire_admission(). spill_threshold is set high so these lengths never
// route SPILL.
TEST(ImrQueueAdmissionTest, ClassifyRoutesMemoryRegardlessOfUsage) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));

  queue.add_bytes(900);  // 900 bytes already reserved.
  ASSERT_EQ(queue.bytes_used(), 900u);

  // Fits the remaining budget: MEMORY.
  EXPECT_EQ(queue.classify(100), Envelope_path::MEMORY);
  // Does not fit (900 + 101 > 1000) but is within the spill threshold: still
  // MEMORY. The admission gate, not classify, makes this request wait.
  EXPECT_EQ(queue.classify(101), Envelope_path::MEMORY);

  // Fully saturated: still MEMORY for an in-threshold length.
  queue.add_bytes(100);
  ASSERT_EQ(queue.bytes_used(), memory_limit);
  EXPECT_EQ(queue.classify(1), Envelope_path::MEMORY);

  queue.release_bytes(queue.bytes_used());
}

// classify is pure - it never mutates bytes_used, and repeated
// calls with identical inputs yield identical results.
TEST(ImrQueueAdmissionTest, ClassifyIsPure) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));

  queue.add_bytes(200);
  const std::size_t before = queue.bytes_used();

  const Envelope_path first = queue.classify(300);
  const Envelope_path second = queue.classify(300);

  // Deterministic result for identical inputs.
  EXPECT_EQ(first, second);
  // No observable state mutation.
  EXPECT_EQ(queue.bytes_used(), before);

  // Repeated calls at different sizes still leave the counter untouched.
  (void)queue.classify(700);
  (void)queue.classify(1500);
  EXPECT_EQ(queue.bytes_used(), before);

  // Release what add_bytes() reserved: the queue destructor asserts
  // bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// acquire_admission succeeds immediately (returns false) when
// the reservation fits, and the successful call itself takes the reservation
// (create_memory() then builds the payload that owns and later releases it).
TEST(ImrQueueAdmissionTest, AcquireAdmissionSucceedsWhenItFits) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  // Run under a timeout so a regression that blocks here fails fast.
  std::future<bool> failed = std::async(std::launch::async, [&queue] {
    return queue.acquire_admission(100, Envelope_path::MEMORY);
  });
  ASSERT_EQ(failed.wait_for(std::chrono::seconds(3)), std::future_status::ready)
      << "acquire_admission must return immediately when the reservation fits";
  // false == success.
  EXPECT_FALSE(failed.get());

  // The successful admission reserved exactly the requested bytes.
  EXPECT_EQ(queue.bytes_used(), 100u);

  // No payload owns this reservation, so release it by hand.
  queue.release_bytes(100);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// acquire_admission blocks while over the limit, then is
// unblocked and succeeds (returns false) once release_bytes frees enough room.
TEST(ImrQueueAdmissionTest, AcquireAdmissionBlocksThenUnblocksOnRelease) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  queue.add_bytes(memory_limit);  // Fully occupied: a 100-byte request blocks.

  std::atomic<bool> returned{false};
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();

  std::thread waiter([&] {
    const bool failed = queue.acquire_admission(100, Envelope_path::MEMORY);
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
  // The two releases freed 100 bytes and the woken waiter reserved exactly
  // those 100, so the budget is full again (1000 - 50 - 50 + 100).
  EXPECT_EQ(queue.bytes_used(), memory_limit);

  // Release the rest of what add_bytes() reserved: the queue destructor asserts
  // bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// A stop while blocked fails (returns true) and leaves bytes_used
// unchanged (no bytes reserved).
TEST(ImrQueueAdmissionTest, AcquireAdmissionFailsOnStop) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  queue.add_bytes(memory_limit);  // Fully occupied: the request will block.
  const std::size_t used_before = queue.bytes_used();

  std::atomic<bool> returned{false};
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();

  std::thread waiter([&] {
    const bool failed = queue.acquire_admission(100, Envelope_path::MEMORY);
    returned.store(true);
    result_promise.set_value(failed);
  });

  // Confirm it is blocked before requesting stop.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_FALSE(returned.load());

  queue.stop(Trx_envelope_queue::Scope::RECEIVER);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "acquire_admission must return after stop(RECEIVER)";
  // true == failure (stopped, not acquired).
  EXPECT_TRUE(result_future.get());

  waiter.join();
  // A 0-byte admission fails only on a receiver stop.
  EXPECT_TRUE(queue.acquire_admission(0, Envelope_path::MEMORY))
      << "the receiver stays stopped";
  // bytes_used unchanged by the stopped, failed admission attempt.
  EXPECT_EQ(queue.bytes_used(), used_before);

  // Release what add_bytes() reserved: the queue destructor asserts
  // bytes_used() == 0.
  queue.release_bytes(queue.bytes_used());
}

// ---------------------------------------------------------------------------
// Spill path: the relay_log_space_limit soft bound through the spill disk
// entry. Admission reserves the declared size while the entry is under
// its limit, whatever that size is, and waits while it is at or over it.
// ---------------------------------------------------------------------------

// An oversized spill transaction is admitted while under the limit; the next
// one waits, still waits when usage is exactly at the limit, and is admitted
// once usage drops under it.
TEST(ImrQueueAdmissionTest, SpillAdmissionSoftLimit) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  const std::size_t disk_limit = 1000;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit),
                           /*relay_log_dir=*/{}, /*channel_name=*/{}, disk);
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  // Under the limit: a transaction five times the limit is admitted at once.
  EXPECT_FALSE(queue.acquire_admission(5000, Envelope_path::SPILL));
  EXPECT_EQ(disk->m_available.load(), -4000);

  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();
  std::thread waiter([&] {
    result_promise.set_value(
        queue.acquire_admission(600, Envelope_path::SPILL));
  });

  // Over the limit: the next spill transaction waits.
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  // Exactly at the limit (available == 0): it still waits.
  queue.release_spill_reservation(4000);
  EXPECT_EQ(disk->m_available.load(), 0);
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  // Under the limit: admitted, reserving its whole size.
  queue.release_spill_reservation(1);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "a spill admission must proceed once usage is under the limit";
  EXPECT_FALSE(result_future.get());  // false == admitted
  waiter.join();
  EXPECT_EQ(disk->m_available.load(), 1 - 600);

  // The memory budget was never touched.
  EXPECT_EQ(queue.bytes_used(), 0u);

  queue.release_spill_reservation(1000 + 599);
  EXPECT_EQ(disk->m_available.load(), static_cast<long long>(disk_limit));
}

// A stop while a spill admission waits on the disk limit fails it (returns
// true) without reserving anything.
TEST(ImrQueueAdmissionTest, SpillAdmissionFailsOnStop) {
  const std::size_t disk_limit = 1000;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(/*memory_limit=*/1000, /*spill_threshold=*/500,
                           test_memory_resource(1000), /*relay_log_dir=*/{},
                           /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  EXPECT_FALSE(queue.acquire_admission(1000, Envelope_path::SPILL));  // -> 0

  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();
  std::thread waiter([&] {
    result_promise.set_value(
        queue.acquire_admission(600, Envelope_path::SPILL));
  });
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);

  queue.stop(Trx_envelope_queue::Scope::RECEIVER);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "a waiting spill admission must return after stop(RECEIVER)";
  EXPECT_TRUE(result_future.get());  // true == stopped, nothing reserved
  waiter.join();
  EXPECT_EQ(disk->m_available.load(), 0);

  queue.release_spill_reservation(1000);
}

// Without a spill disk entry (relay_log_space_limit = 0) the spill path is
// unlimited, but a stop still rejects it.
TEST(ImrQueueAdmissionTest, SpillAdmissionUnlimitedWithoutEntry) {
  Trx_envelope_queue queue(/*memory_limit=*/1000, /*spill_threshold=*/500,
                           test_memory_resource(1000));
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  EXPECT_FALSE(queue.acquire_admission(1u << 30, Envelope_path::SPILL));
  EXPECT_FALSE(queue.acquire_admission(1u << 30, Envelope_path::SPILL));
  queue.release_spill_reservation(1u << 30);  // no entry: a no-op
  queue.stop(Trx_envelope_queue::Scope::RECEIVER);
  EXPECT_TRUE(queue.acquire_admission(1, Envelope_path::SPILL));
}

// ---------------------------------------------------------------------------
// The read-only admission predicates and the lock-free pre-wait
// (wait_until_admissible) the receiver runs before it takes LOCK_log.
// ---------------------------------------------------------------------------

// The read-only predicates give the same answer as the try-lock functions on
// the boundary values, and never change the count.
TEST(ImrQueueAdmissionTest, PredicatesMatchTryLockOnBoundaries) {
  const long long limit = 1000;
  // {available, amount}: below, at and above the request; at the limit with a
  // request above it; zero, negative.
  const std::vector<std::pair<long long, std::size_t>> cases = {
      {1000, 1000}, {1000, 1001}, {1000, 5000}, {999, 1000}, {999, 999},
      {100, 99},    {100, 100},   {100, 101},   {1, 1},      {1, 2},
      {0, 0},       {0, 1},       {-1, 1},      {-500, 0},   {500, 0}};
  Resource_entry entry;
  entry.m_limit.store(limit);
  for (const auto &[available, amount] : cases) {
    entry.m_available.store(available);
    const bool would = Resource_instance_monitor::would_lock(entry, amount);
    const bool would_soft =
        Resource_instance_monitor::would_lock_soft(entry, amount);
    EXPECT_EQ(entry.m_available.load(), available)
        << "the predicates reserve nothing";

    const bool locked =
        Resource_instance_monitor::try_lock_resource(entry, amount);
    EXPECT_EQ(would, locked)
        << "would_lock vs try_lock_resource, available=" << available
        << " amount=" << amount;
    entry.m_available.store(available);

    const bool locked_soft =
        Resource_instance_monitor::try_lock_resource_soft(entry, amount);
    EXPECT_EQ(would_soft, locked_soft)
        << "would_lock_soft vs try_lock_resource_soft, available=" << available
        << " amount=" << amount;
  }
}

// When the transaction fits, wait_until_admissible() returns at once and
// reserves nothing, on the memory path and on the spill path, and also when a
// path's limit is off.
TEST(ImrQueueAdmissionTest, WaitUntilAdmissibleFitsReservesNothing) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Resource_entry *disk = test_spill_disk_resource(1000);
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit),
                           /*relay_log_dir=*/{}, /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  queue.add_bytes(400);
  std::future<bool> memory = std::async(std::launch::async, [&queue] {
    return queue.wait_until_admissible(600);
  });
  ASSERT_EQ(memory.wait_for(std::chrono::seconds(3)), std::future_status::ready)
      << "wait_until_admissible must return at once when the trx fits";
  EXPECT_FALSE(memory.get());  // false == would be admitted
  EXPECT_EQ(queue.bytes_used(), 400u) << "the pre-wait reserves nothing";

  // Spill path (above the threshold), under the disk limit.
  EXPECT_TRUE(queue.is_admissible(5000));
  EXPECT_FALSE(queue.wait_until_admissible(5000));
  EXPECT_EQ(disk->m_available.load(), 1000) << "the pre-wait reserves nothing";
  EXPECT_EQ(queue.bytes_used(), 400u);
  queue.release_bytes(400);

  // Limits off: no memory entry (resource tracking off) and no spill entry.
  Trx_envelope_queue unlimited(memory_limit, spill_threshold,
                               /*memory_resource=*/nullptr);
  unlimited.resume(Trx_envelope_queue::Scope::RECEIVER);
  EXPECT_TRUE(unlimited.is_admissible(1u << 30));
  EXPECT_FALSE(unlimited.wait_until_admissible(1u << 30));
  EXPECT_FALSE(unlimited.wait_until_admissible(100));
}

// It blocks while the memory limit is reached and returns once another thread
// releases enough bytes; it still reserves nothing. The following
// acquire_admission() then takes its first attempt (no poll).
TEST(ImrQueueAdmissionTest, WaitUntilAdmissibleBlocksThenReturnsOnRelease) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           test_memory_resource(memory_limit));
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  queue.add_bytes(memory_limit);  // Saturated: a 100-byte trx must wait.
  EXPECT_FALSE(queue.is_admissible(100));

  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();
  std::thread waiter(
      [&] { result_promise.set_value(queue.wait_until_admissible(100)); });

  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  // Not enough yet: 950 used, 100 more does not fit.
  queue.release_bytes(50);
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  // Now 900 used: 100 more fits.
  queue.release_bytes(50);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "wait_until_admissible must return once release_bytes frees room";
  EXPECT_FALSE(result_future.get());
  waiter.join();
  EXPECT_EQ(queue.bytes_used(), 900u) << "the pre-wait reserves nothing";

#ifndef NDEBUG
  const std::size_t polls_before = queue.admission_polls();
#endif
  // Single reserver: nothing reserved since the pre-wait returned, so the
  // admission reserves at its first attempt.
  EXPECT_FALSE(queue.acquire_admission(100, Envelope_path::MEMORY));
  EXPECT_EQ(queue.bytes_used(), memory_limit);
#ifndef NDEBUG
  EXPECT_EQ(queue.admission_polls(), polls_before)
      << "acquire_admission after a successful pre-wait must not poll";
#endif

  queue.release_bytes(queue.bytes_used());
}

// Control for the poll counter: without the pre-wait, an admission that has
// to wait does poll.
#ifndef NDEBUG
TEST(ImrQueueAdmissionTest, AdmissionPollsWithoutPreWait) {
  const std::size_t memory_limit = 1000;
  Trx_envelope_queue queue(memory_limit, /*spill_threshold=*/500,
                           test_memory_resource(memory_limit));
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  queue.add_bytes(memory_limit);
  const std::size_t polls_before = queue.admission_polls();
  std::future<bool> admitted = std::async(std::launch::async, [&queue] {
    return queue.acquire_admission(100, Envelope_path::MEMORY);
  });
  EXPECT_EQ(admitted.wait_for(kShortWait), std::future_status::timeout);
  queue.release_bytes(100);
  ASSERT_EQ(admitted.wait_for(std::chrono::seconds(3)),
            std::future_status::ready);
  EXPECT_FALSE(admitted.get());
  EXPECT_GT(queue.admission_polls(), polls_before);
  queue.release_bytes(queue.bytes_used());
}
#endif

// A stop while it waits makes it return true, reserving nothing.
TEST(ImrQueueAdmissionTest, WaitUntilAdmissibleReturnsOnStop) {
  const std::size_t memory_limit = 1000;
  Trx_envelope_queue queue(memory_limit, /*spill_threshold=*/500,
                           test_memory_resource(memory_limit));
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  queue.add_bytes(memory_limit);

  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();
  std::thread waiter(
      [&] { result_promise.set_value(queue.wait_until_admissible(100)); });
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);

  // Stopping only the receiver role ends the receiver's wait.
  queue.stop(Trx_envelope_queue::Scope::RECEIVER);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready)
      << "wait_until_admissible must return after stop(RECEIVER)";
  EXPECT_TRUE(result_future.get());  // true == stopped
  waiter.join();
  EXPECT_EQ(queue.bytes_used(), memory_limit);
  // The admission that follows a stopped pre-wait reports the stop first.
  EXPECT_TRUE(queue.acquire_admission(100, Envelope_path::MEMORY));
  EXPECT_EQ(queue.bytes_used(), memory_limit);

  queue.release_bytes(queue.bytes_used());
}

// A memory-path trx larger than the limit (possible when the spill threshold
// is above the limit) is admissible when usage is zero, and waits otherwise.
TEST(ImrQueueAdmissionTest, WaitUntilAdmissibleOversizedAtZeroUsage) {
  const std::size_t memory_limit = 1000;
  Trx_envelope_queue queue(memory_limit, /*spill_threshold=*/5000,
                           test_memory_resource(memory_limit));
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  ASSERT_EQ(queue.classify(1500), Envelope_path::MEMORY);
  EXPECT_TRUE(queue.is_admissible(1500));
  EXPECT_FALSE(queue.wait_until_admissible(1500));
  EXPECT_EQ(queue.bytes_used(), 0u);

  queue.add_bytes(1);
  EXPECT_FALSE(queue.is_admissible(1500));
  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();
  std::thread waiter(
      [&] { result_promise.set_value(queue.wait_until_admissible(1500)); });
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  queue.release_bytes(1);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready);
  EXPECT_FALSE(result_future.get());
  waiter.join();

  EXPECT_FALSE(queue.acquire_admission(1500, Envelope_path::MEMORY));
  queue.release_bytes(1500);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Spill path, soft rule: it waits while the disk entry is at or over its limit
// (available <= 0) and returns once it is under it, even for a request larger
// than what is available. Nothing is reserved; the following admission then
// takes its first attempt.
TEST(ImrQueueAdmissionTest, WaitUntilAdmissibleSpillSoftRule) {
  const std::size_t disk_limit = 1000;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(/*memory_limit=*/1000, /*spill_threshold=*/500,
                           test_memory_resource(1000), /*relay_log_dir=*/{},
                           /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  EXPECT_FALSE(queue.acquire_admission(1000, Envelope_path::SPILL));  // -> 0
  EXPECT_FALSE(queue.is_admissible(600));

  std::promise<bool> result_promise;
  std::future<bool> result_future = result_promise.get_future();
  std::thread waiter(
      [&] { result_promise.set_value(queue.wait_until_admissible(5000)); });
  EXPECT_EQ(result_future.wait_for(kShortWait), std::future_status::timeout);
  // Under the limit by one byte: an oversized request is admissible.
  queue.release_spill_reservation(1);
  ASSERT_EQ(result_future.wait_for(std::chrono::seconds(3)),
            std::future_status::ready);
  EXPECT_FALSE(result_future.get());
  waiter.join();
  EXPECT_EQ(disk->m_available.load(), 1) << "the pre-wait reserves nothing";

#ifndef NDEBUG
  const std::size_t polls_before = queue.admission_polls();
#endif
  EXPECT_FALSE(queue.acquire_admission(5000, Envelope_path::SPILL));
  EXPECT_EQ(disk->m_available.load(), 1 - 5000);
#ifndef NDEBUG
  EXPECT_EQ(queue.admission_polls(), polls_before);
#endif
  EXPECT_EQ(queue.bytes_used(), 0u);  // the memory budget is untouched

  queue.release_spill_reservation(999 + 5000);
  EXPECT_EQ(disk->m_available.load(), static_cast<long long>(disk_limit));
}

// ---------------------------------------------------------------------------
// Property test: the memory bound.
// ---------------------------------------------------------------------------

// Memory bound property: over a randomized single-producer admit/release
// sequence, an admitted memory-path reservation never lets bytes_used exceed
// memory_limit and an over-limit reservation is refused by the gate;
// classify() routes every trx_length > spill_threshold to SPILL and everything
// else to MEMORY, regardless of the current usage; and bytes_used always equals
// the sum of live reservations, returning to 0 once everything is released.
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

  Resource_entry *const memory_resource = test_memory_resource(memory_limit);
  Trx_envelope_queue queue(memory_limit, spill_threshold, memory_resource);
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  // Multiset of live reservations currently counted in bytes_used.
  std::vector<std::size_t> live;

  auto sum_live = [&live]() {
    return std::accumulate(live.begin(), live.end(), std::size_t{0});
  };

  for (int trial = 0; trial < kTrials; ++trial) {
    const std::size_t trx_length = len_dist(rng);
    const Envelope_path decision = queue.classify(trx_length);

    // Every over-threshold length must classify SPILL, independent of usage.
    if (trx_length > spill_threshold) {
      ASSERT_EQ(decision, Envelope_path::SPILL)
          << "seed=" << kSeed << " trial=" << trial
          << " trx_length=" << trx_length;
    }

    if (decision == Envelope_path::SPILL) {
      // Spill path does not touch the memory counter: leave `live` and the
      // counter untouched (verified by the invariant check at the loop end).
    } else {
      // classify() only routes, so every in-threshold length is MEMORY
      // whatever the usage; the monitor is the gate.
      ASSERT_EQ(decision, Envelope_path::MEMORY)
          << "seed=" << kSeed << " trial=" << trial
          << " trx_length=" << trx_length;
      const bool fits = queue.bytes_used() + trx_length <= memory_limit;
      if (fits) {
        // Fits: admission takes the non-blocking fast path and reserves.
        ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::MEMORY))
            << "seed=" << kSeed << " trial=" << trial
            << " trx_length=" << trx_length;
        live.push_back(trx_length);
        // Memory bound holds at every admission point.
        ASSERT_LE(queue.bytes_used(), memory_limit)
            << "seed=" << kSeed << " trial=" << trial
            << " trx_length=" << trx_length;
      } else {
        // Does not fit: the gate refuses. Probe it with the monitor's
        // non-blocking reserve (acquire_admission would park here, since this
        // test is single threaded); a refused probe must change nothing.
        const std::size_t used_before = queue.bytes_used();
        ASSERT_FALSE(Resource_instance_monitor::try_lock_resource(
            *memory_resource, trx_length))
            << "seed=" << kSeed << " trial=" << trial
            << " trx_length=" << trx_length << " bytes_used=" << used_before;
        ASSERT_EQ(queue.bytes_used(), used_before)
            << "seed=" << kSeed << " trial=" << trial;
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

    // Counter always equals the sum of live reservations.
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
