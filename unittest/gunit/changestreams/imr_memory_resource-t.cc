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
/// Unit tests for the in-memory relay log's memory limit as a resource on the
/// channel's Resource_instance_monitor:
///
///   - Reservation round-trip: a memory-path admission reserves exactly the
///     transaction's length and the payload releases it on destruction, on any
///     thread; the monitor's available count returns to the limit.
///   - Back-pressure and resume: a full budget parks the receiver, a
///     release lets it proceed, and the largest possible memory-path
///     transaction is always admitted once the queue drains.
///   - Stop breaks the wait: a receiver stop (no THD kill -- these tests
///     run without a THD) aborts a parked admission within a bounded time and
///     without reserving.
///
/// Every test drives the real admission path, Trx_envelope_queue::enqueue()
/// -> Transaction_envelope::create_payload() -> Trx_payload::create_memory() ->
/// acquire_admission(), and reads the monitor directly as well as through
/// bytes_used(). Each test registers the resource with its queue's limit, as
/// Imr_service::reconcile() does, which resets the entry's available count to
/// that limit.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "sql/changestreams/apply/resource/resource_monitor.h"  // Resource_monitor
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/log_event.h"  // Format_description_log_event
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {
using Scope = Trx_envelope_queue::Scope;
using Clock = std::chrono::steady_clock;

/// A short bounded wait used to observe that a background admission is still
/// parked.
constexpr std::chrono::milliseconds kShortWait{50};

/// A generous bound for "must happen" observations, so a regression fails
/// instead of hanging.
constexpr std::chrono::seconds kLongWait{3};

/// Upper bound on the time between a receiver stop and the parked receiver
/// returning. The poll interval is 1 ms; the bound leaves wide slack for loaded
/// or sanitizer builds while still proving the wait is not indefinite.
constexpr std::chrono::milliseconds kStopBound{1000};

/// A fresh relay-log FDE, as the receiver would supply.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// The IMR memory resource entry of the queue under test. Each registration
/// replaces the entry, so this is updated on every call.
Resource_entry *g_memory_resource = nullptr;

/// Register the resource for a new queue, as Imr_service::reconcile() does,
/// and remember its entry for available().
Resource_entry *memory_resource(std::size_t limit) {
  g_memory_resource = test_memory_resource(limit);
  return g_memory_resource;
}

/// The monitor's available count for the IMR memory resource.
long long available() { return g_memory_resource->m_available.load(); }

/// Enqueue a memory-path transaction of @p trx_length on a background thread.
/// The returned future yields the envelope, or nullptr if a receiver stop
/// aborted the enqueue.
std::future<Transaction_envelope *> enqueue_async(Trx_envelope_queue &queue,
                                                  std::size_t trx_length) {
  return std::async(std::launch::async, [&queue, trx_length] {
    return queue.enqueue(trx_length, /*is_trx=*/true, make_fde());
  });
}

/// Make sure a background enqueue has finished before the test proceeds. If it
/// is still parked (a failed expectation above), a receiver stop breaks the
/// wait so the test never hangs or destroys the queue under a live thread.
void finish(Trx_envelope_queue &queue, std::future<Transaction_envelope *> &f) {
  if (f.wait_for(kLongWait) != std::future_status::ready) {
    ADD_FAILURE() << "background enqueue did not finish; stopping the receiver";
    queue.stop(Scope::RECEIVER);
  }
  f.wait();
}
}  // namespace

// ---------------------------------------------------------------------------
// Reservation round-trip.
// ---------------------------------------------------------------------------

// Each memory-path admission takes exactly its trx_length from the resource --
// once, with no second reservation at payload construction -- and each commit
// (which destroys the payload) returns exactly that amount. After the last
// commit the resource's available count is back at the limit and bytes_used()
// is 0.
TEST(ImrMemoryResourceTest, ReservationRoundTripThroughEnqueueAndCommit) {
  const std::size_t memory_limit = 1u << 20;     // 1 MiB
  const std::size_t spill_threshold = 1u << 16;  // 64 KiB
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  // admission rejects on a receiver stop; arm the queue.
  queue.resume(Scope::RECEIVER);

  // The resource was registered at the configured limit.
  ASSERT_EQ(available(), static_cast<long long>(memory_limit));
  ASSERT_EQ(queue.bytes_used(), 0u);

  const std::vector<std::size_t> lengths{100, 4096, spill_threshold, 1};
  std::vector<Transaction_envelope *> envelopes;
  std::size_t reserved = 0;
  for (const std::size_t len : lengths) {
    Transaction_envelope *env = queue.enqueue(len, /*is_trx=*/true, make_fde());
    ASSERT_NE(env, nullptr) << "len=" << len;
    envelopes.push_back(env);
    reserved += len;
    // Exactly one reservation of len per admission.
    EXPECT_EQ(available(), static_cast<long long>(memory_limit - reserved))
        << "len=" << len;
    EXPECT_EQ(queue.bytes_used(), reserved) << "len=" << len;
  }

  // Commit in reverse order: each commit releases exactly its own length,
  // independent of the order.
  for (auto it = envelopes.rbegin(); it != envelopes.rend(); ++it) {
    Transaction_envelope *env = *it;
    const std::size_t len = env->trx_length();
    ASSERT_FALSE(env->commit());
    reserved -= len;
    EXPECT_EQ(available(), static_cast<long long>(memory_limit - reserved))
        << "len=" << len;
    EXPECT_EQ(queue.bytes_used(), reserved) << "len=" << len;
  }

  EXPECT_EQ(available(), static_cast<long long>(memory_limit));
  EXPECT_EQ(queue.bytes_used(), 0u);

  queue.reset();  // drop the committed envelopes still in the deque.
}

// The receiver takes the reservation, but the payload may be destroyed on a
// different thread: a worker's commit, or the coordinator's sweep / reset. The
// release must land on the resource either way.
TEST(ImrMemoryResourceTest, ReservationReleasedOnAnotherThread) {
  const std::size_t memory_limit = 1u << 20;
  const std::size_t spill_threshold = 1u << 16;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  queue.resume(Scope::RECEIVER);

  Transaction_envelope *committed = queue.enqueue(4096, true, make_fde());
  Transaction_envelope *dropped = queue.enqueue(8192, true, make_fde());
  ASSERT_NE(committed, nullptr);
  ASSERT_NE(dropped, nullptr);
  ASSERT_EQ(queue.bytes_used(), 4096u + 8192u);

  // A "worker" thread commits the first envelope, destroying its payload.
  std::thread worker([committed] { EXPECT_FALSE(committed->commit()); });
  worker.join();
  EXPECT_EQ(queue.bytes_used(), 8192u);
  EXPECT_EQ(available(), static_cast<long long>(memory_limit - 8192));

  // Another thread drops the second, uncommitted envelope through reset(),
  // which destroys its payload there.
  std::thread coordinator([&queue] { queue.reset(); });
  coordinator.join();
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(available(), static_cast<long long>(memory_limit));
}

// Emptying the queue while it is detached (RESET REPLICA, or a rebuild) drops
// every envelope in whatever state it is in -- undispatched, dispatched,
// truncated -- and must leave no reservation behind.
TEST(ImrMemoryResourceTest, ResetReleasesEveryOutstandingReservation) {
  const std::size_t memory_limit = 1u << 20;
  const std::size_t spill_threshold = 1u << 16;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  queue.resume(Scope::RECEIVER);
  queue.resume(Scope::APPLIER);

  Transaction_envelope *dispatched = queue.enqueue(1000, true, make_fde());
  Transaction_envelope *truncated = queue.enqueue(2000, true, make_fde());
  Transaction_envelope *pending = queue.enqueue(3000, true, make_fde());
  ASSERT_NE(dispatched, nullptr);
  ASSERT_NE(truncated, nullptr);
  ASSERT_NE(pending, nullptr);

  ASSERT_EQ(queue.dispatch_next(), dispatched);
  truncated->set_truncated();
  ASSERT_EQ(queue.bytes_used(), 6000u);
  ASSERT_EQ(available(), static_cast<long long>(memory_limit - 6000));

  // both roles stopped, as they are before a reset.
  queue.stop(Scope::APPLIER);
  queue.stop(Scope::RECEIVER);
  queue.reset();

  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(available(), static_cast<long long>(memory_limit));
}

// ---------------------------------------------------------------------------
// Back-pressure and resume.
// ---------------------------------------------------------------------------

// A full budget parks the next memory-path admission: it takes no reservation
// and does not enqueue while parked. A commit that frees enough room lets it
// proceed; it then holds its own reservation.
TEST(ImrMemoryResourceTest, ParksAtLimitAndResumesWhenPayloadReleased) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  queue.resume(Scope::RECEIVER);

  // Fill the budget exactly. Both admissions fit.
  Transaction_envelope *e1 = queue.enqueue(500, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(500, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);
  ASSERT_EQ(queue.bytes_used(), memory_limit);

  auto parked = enqueue_async(queue, 100);

  // The admission parks: no reservation, no enqueue.
  EXPECT_EQ(parked.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_EQ(queue.bytes_used(), memory_limit);
  EXPECT_EQ(queue.insert_seqno(), 2u);

  // Committing e1 frees 500 bytes; the parked admission sees it on its next
  // poll and proceeds. Checks before finish() are non-fatal: a fatal
  // one would return with the background enqueue still parked.
  EXPECT_FALSE(e1->commit());
  EXPECT_EQ(parked.wait_for(kLongWait), std::future_status::ready)
      << "a release that frees enough room must unblock the parked admission";
  finish(queue, parked);
  EXPECT_NE(parked.get(), nullptr);

  // It now holds its own 100 bytes on top of e2's 500.
  EXPECT_EQ(queue.bytes_used(), 600u);
  EXPECT_EQ(queue.insert_seqno(), 3u);

  queue.reset();
}

// Forward progress: the spill threshold is strictly below the limit,
// so the largest possible memory-path transaction -- one exactly at the
// threshold -- always fits an empty budget. Behind a full queue it parks,
// stays parked through a partial release that is still not enough, and is
// admitted once the queue drains.
TEST(ImrMemoryResourceTest, AtThresholdTransactionAdmittedOnceQueueDrains) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = memory_limit - 1;  // 999
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  queue.resume(Scope::RECEIVER);

  // Fill to 999 bytes with two memory-path transactions.
  Transaction_envelope *e1 = queue.enqueue(600, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(399, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);
  ASSERT_EQ(queue.bytes_used(), 999u);

  // An at-threshold transaction is memory-path (not spill) and does not fit.
  // (Checked before the background enqueue starts, so a fatal check is safe.)
  ASSERT_EQ(queue.classify(spill_threshold), Envelope_path::MEMORY);
  auto parked = enqueue_async(queue, spill_threshold);
  EXPECT_EQ(parked.wait_for(kShortWait), std::future_status::timeout);

  // Freeing 600 leaves 399 used: 399 + 999 > 1000, so it stays parked.
  // (Checks before finish() are non-fatal, as above.)
  EXPECT_FALSE(e1->commit());
  EXPECT_EQ(parked.wait_for(kShortWait), std::future_status::timeout);
  EXPECT_EQ(queue.bytes_used(), 399u);

  // Draining the rest lets it in.
  EXPECT_FALSE(e2->commit());
  EXPECT_EQ(parked.wait_for(kLongWait), std::future_status::ready)
      << "an at-threshold transaction must be admitted once the queue drains";
  finish(queue, parked);
  EXPECT_NE(parked.get(), nullptr);
  EXPECT_EQ(queue.bytes_used(), spill_threshold);

  queue.reset();
}

// ---------------------------------------------------------------------------
// A receiver stop breaks the wait.
// ---------------------------------------------------------------------------

// A receiver parked on the limit returns within a bounded time of a receiver
// stop -- here with no THD at all, so nothing but the queue's own stop flag
// can end the wait. The enqueue is aborted without a reservation
// and the stop leaves the applier role untouched.
TEST(ImrMemoryResourceTest, ReceiverStopBreaksParkedAdmissionWithinBound) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  queue.resume(Scope::RECEIVER);
  queue.resume(Scope::APPLIER);

  Transaction_envelope *e1 = queue.enqueue(500, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(500, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);

  auto parked = enqueue_async(queue, 100);
  EXPECT_EQ(parked.wait_for(kShortWait), std::future_status::timeout);

  const auto stop_requested = Clock::now();
  queue.stop(Scope::RECEIVER);
  const bool returned = parked.wait_for(kLongWait) == std::future_status::ready;
  const auto observed_after = Clock::now() - stop_requested;
  finish(queue, parked);

  ASSERT_TRUE(returned) << "a receiver stop must end the parked admission";
  // Bounded stop latency.
  EXPECT_LT(observed_after, kStopBound);
  // Aborted: no envelope, no reservation.
  EXPECT_EQ(parked.get(), nullptr);
  EXPECT_EQ(queue.bytes_used(), memory_limit);
  EXPECT_EQ(queue.insert_seqno(), 2u);

  // The receiver stop does not affect the applier role.
  EXPECT_FALSE(queue.applier_stop_flag()->load());
  EXPECT_EQ(queue.dispatch_next(), e1);

  queue.reset();
}

// A stop that is already requested when admission starts aborts before any
// reservation is attempted, even though the request would fit, and the
// receiver never parks.
TEST(ImrMemoryResourceTest, StopBeforeAdmissionReservesNothingEvenIfItFits) {
  const std::size_t memory_limit = 1000;
  const std::size_t spill_threshold = 500;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  queue.resume(Scope::RECEIVER);

  queue.stop(Scope::RECEIVER);
  EXPECT_EQ(queue.enqueue(100, true, make_fde()), nullptr);

  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(available(), static_cast<long long>(memory_limit));
  EXPECT_EQ(queue.insert_seqno(), 0u);
}

// Admission and release keep working while another thread registers new
// names on the same channel's monitor, as the SQL thread does at every start
// (CSA registers declared_channel_memory) while the IO thread keeps queuing.
// The queue reaches its resource only through the entry it was given, never
// through the monitor's name map, so the map growing (and rehashing) under it
// is harmless. Mainly a guard for sanitizer builds (ASan/TSan): before the
// entry was passed in, every admission looked the resource up by name.
TEST(ImrMemoryResourceTest, AdmitAndReleaseWhileOtherNamesAreRegistered) {
  const std::size_t memory_limit = 64 * 1024;
  const std::size_t spill_threshold = 16 * 1024;
  constexpr int kIterations = 20000;
  constexpr std::size_t kLen = 512;
  Trx_envelope_queue queue(memory_limit, spill_threshold,
                           memory_resource(memory_limit));
  queue.resume(Scope::RECEIVER);

  std::atomic<bool> done{false};
  std::thread registrar([&done] {
    auto &monitor = Resource_monitor::get(kTestInstance);
    for (int i = 0; !done.load(); ++i)
      monitor.register_resource("stress_other_" + std::to_string(i % 20000), 1);
  });

  for (int i = 0; i < kIterations; ++i) {
    Transaction_envelope *env =
        queue.enqueue(kLen, /*is_trx=*/true, make_fde());
    ASSERT_NE(env, nullptr) << "iteration " << i;
    ASSERT_FALSE(env->commit()) << "iteration " << i;
    if (i % 100 == 99) queue.reset();  // drop the committed envelopes
  }
  done.store(true);
  registrar.join();

  queue.reset();
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(available(), static_cast<long long>(memory_limit));
}

}  // namespace mysql::csa::unittests
