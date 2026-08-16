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
/// Unit tests for mysql::csa::Trx_envelope_queue::sweep_and_dispatch() -- the
/// folded coordinator step the applier's Queued_transaction_reader drives once
/// per iteration. In a single m_queue_mutex hold it (1) sweeps the contiguous
/// committed head prefix (reclaiming slots workers finished), then (2) blocks
/// for the next undispatched envelope (or a stop), then (3) dispatches it. These
/// tests exercise that fold, its shared dispatch_next_locked() helper, and the
/// behavior-preserving sweep_committed(need_lock) refactor.
///
/// The queue rests STOPPED for both roles at construction (the mi-owned model),
/// so every test arms it with resume() right after construction. No seal-gated
/// dispatch: an envelope is handed out while still uncommitted/open; the sweep
/// step only reclaims already-committed slots and never blocks the dispatch.

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <thread>

#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"

namespace mysql::csa::unittests {

namespace {
using Scope = Trx_envelope_queue::Scope;

/// A short bounded wait to observe that a background call is still blocked.
constexpr std::chrono::milliseconds kShortWait{50};
/// A generous wait bounding the success path so a regression fails fast.
constexpr std::chrono::seconds kJoinWait{3};

/// Bounds large enough that enqueue never blocks in admission: every enqueue
/// takes the (non-blocking) MEMORY path.
constexpr std::size_t kMemoryLimit = std::size_t{1} << 30;     // 1 GiB
constexpr std::size_t kSpillThreshold = std::size_t{1} << 20;  // 1 MiB

/// Build the active FDE every enqueue() must be handed; enqueue() only asserts
/// it non-null (these tests never create a real byte source).
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}
}  // namespace

// A single sweep_and_dispatch() call BOTH reclaims the committed head prefix
// (dequeues it, advances commit_seqno, and bytes_used() reflects the payload the
// committing worker already released) AND dispatches the next undispatched
// envelope (advances dispatch_seqno) -- the committed head is reclaimed before
// the same call hands back the next envelope.
// Requirements 6.3, 6.4, 6.6
TEST(ImrSweepAndDispatchTest, SweepsCommittedHeadAndDispatchesNextInOneCall) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  ASSERT_NE(queue.enqueue(33, true, make_fde()), nullptr);  // e3
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);
  ASSERT_EQ(queue.insert_seqno(), 3u);
  ASSERT_EQ(queue.bytes_used(), 66u);

  // First step: nothing committed yet, so the sweep is a no-op and e1 is
  // dispatched.
  ASSERT_EQ(queue.sweep_and_dispatch(), e1);
  ASSERT_EQ(queue.commit_seqno(), 0u);
  ASSERT_EQ(queue.dispatch_seqno(), 1u);

  // The worker commits e1: commit() releases e1's payload bytes (66 - 11 = 55).
  ASSERT_FALSE(e1->commit());
  ASSERT_EQ(queue.bytes_used(), 55u);

  // Second step: one call both sweeps the committed head (e1) AND dispatches the
  // next envelope (e2).
  Transaction_envelope *dispatched = queue.sweep_and_dispatch();
  EXPECT_EQ(dispatched, e2) << "the same call must hand back the next envelope";
  EXPECT_EQ(queue.commit_seqno(), 1u) << "committed head e1 was swept";
  EXPECT_EQ(queue.dispatch_seqno(), 2u) << "e2 was dispatched";
  EXPECT_EQ(queue.insert_seqno(), 3u);
  // Sweeping does not touch the byte counter; e1's bytes were released at commit.
  EXPECT_EQ(queue.bytes_used(), 55u);

  queue.reset();  // drop e2, e3; back to the pristine empty state.
}

// With no undispatched envelope, sweep_and_dispatch() blocks (matching
// dispatch_next()); a subsequent enqueue wakes it and it returns that envelope.
// Requirements 6.3, 6.5
TEST(ImrSweepAndDispatchTest, BlocksWhenEmptyThenReturnsOnEnqueue) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  std::promise<Transaction_envelope *> got;
  std::future<Transaction_envelope *> fut = got.get_future();
  std::thread consumer([&] { got.set_value(queue.sweep_and_dispatch()); });

  // Empty queue, applier armed: it must be parked.
  ASSERT_EQ(fut.wait_for(kShortWait), std::future_status::timeout);

  Transaction_envelope *env = queue.enqueue(11, true, make_fde());
  ASSERT_NE(env, nullptr);

  ASSERT_EQ(fut.wait_for(kJoinWait), std::future_status::ready)
      << "enqueue must wake the parked sweep_and_dispatch()";
  EXPECT_EQ(fut.get(), env);
  consumer.join();

  EXPECT_EQ(queue.dispatch_seqno(), 1u);
  queue.reset();
}

// stop(APPLIER) wakes a parked sweep_and_dispatch(): it returns nullptr and
// advances no cursor, and every later call also returns nullptr.
// Requirements 6.3, 7.5
TEST(ImrSweepAndDispatchTest, StopApplierWakesReturnsNullptrNoCursorAdvance) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  std::promise<Transaction_envelope *> got;
  std::future<Transaction_envelope *> fut = got.get_future();
  std::thread consumer([&] { got.set_value(queue.sweep_and_dispatch()); });

  ASSERT_EQ(fut.wait_for(kShortWait), std::future_status::timeout);

  queue.stop(Scope::APPLIER);
  ASSERT_EQ(fut.wait_for(kJoinWait), std::future_status::ready)
      << "stop(APPLIER) must wake the parked sweep_and_dispatch()";
  EXPECT_EQ(fut.get(), nullptr);
  consumer.join();

  EXPECT_EQ(queue.dispatch_seqno(), 0u) << "a stopped step advances nothing";
  EXPECT_EQ(queue.commit_seqno(), 0u);
  // Once stopped, every later call returns nullptr too.
  EXPECT_EQ(queue.sweep_and_dispatch(), nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);

  queue.reset();
}

// stop(ALL) also wakes a parked sweep_and_dispatch().
// Requirements 6.3, 7.5
TEST(ImrSweepAndDispatchTest, StopAllWakesReturnsNullptr) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  std::promise<Transaction_envelope *> got;
  std::future<Transaction_envelope *> fut = got.get_future();
  std::thread consumer([&] { got.set_value(queue.sweep_and_dispatch()); });

  ASSERT_EQ(fut.wait_for(kShortWait), std::future_status::timeout);

  queue.stop();  // Scope::ALL
  ASSERT_EQ(fut.wait_for(kJoinWait), std::future_status::ready)
      << "stop(ALL) must wake the parked sweep_and_dispatch()";
  EXPECT_EQ(fut.get(), nullptr);
  consumer.join();

  EXPECT_TRUE(queue.is_stopped());
  queue.reset();
}

// sweep_and_dispatch() hands out an envelope while it is still uncommitted and
// open -- there is no seal gate. The sweep step reclaims only already-committed
// slots and never blocks the dispatch of the live envelope.
// Requirements 6.3, 6.4, 6.5, 6.6, 6.7
TEST(ImrSweepAndDispatchTest, DispatchesUncommittedEnvelopeNoSealGate) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  Transaction_envelope *env = queue.enqueue(11, true, make_fde());
  ASSERT_NE(env, nullptr);
  ASSERT_FALSE(env->is_committed());

  Transaction_envelope *dispatched = queue.sweep_and_dispatch();
  EXPECT_EQ(dispatched, env);
  EXPECT_FALSE(dispatched->is_committed())
      << "dispatch must not wait for the commit/seal";
  EXPECT_EQ(queue.dispatch_seqno(), 1u);
  EXPECT_EQ(queue.commit_seqno(), 0u);

  queue.reset();
}

// The extracted building block sweep_committed(need_lock=true) still sweeps
// correctly as a standalone external call (behavior-preserving refactor): it
// dequeues the contiguous committed head prefix, advances commit_seqno, returns
// false on success, and leaves the byte counter reflecting the released
// payloads.
// Requirements 6.4
TEST(ImrSweepAndDispatchTest, SweepCommittedStandaloneStillSweeps) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold);
  queue.resume();

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);

  // Dispatch both (commit_seqno <= dispatch_seqno must hold), then commit both;
  // commit() releases each payload's bytes, so the counter reaches 0.
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_next(), e2);
  ASSERT_FALSE(e1->commit());
  ASSERT_FALSE(e2->commit());
  ASSERT_EQ(queue.bytes_used(), 0u);

  EXPECT_FALSE(queue.sweep_committed()) << "standalone sweep succeeds";
  EXPECT_EQ(queue.commit_seqno(), 2u) << "both committed heads were swept";
  EXPECT_EQ(queue.dispatch_seqno(), 2u);
  EXPECT_EQ(queue.insert_seqno(), 2u);
  EXPECT_EQ(queue.bytes_used(), 0u);
  // The queue is fully drained and empty; the destructor's empty-queue
  // invariant holds without a reset().
}

// NOTE: the structural-inconsistency guard in sweep_and_dispatch() (head
// stream_seqno != commit_seqno + 1 -> mark the applier stopped, return nullptr)
// is an assert-in-debug / abort-in-release path that the public commit-order
// invariant makes unreachable through the normal API. It is not covered here
// because there is no test seam to construct the corrupt-cursor state without
// friend access to the queue internals.

}  // namespace mysql::csa::unittests
