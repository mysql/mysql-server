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
/// Unit tests for the queue-reuse lifecycle primitives of
/// mysql::csa::Trx_envelope_queue: reset(), the per-role stop()/resume() wakes,
/// and the role stop flags used as attach state. These are the
/// additive core extensions that let one queue instance live with Master_info
/// and be reused across receiver/applier sessions; the
/// Master_info-owned lifecycle wiring itself (CREATE at CRST, resume() at
/// start, reset() after a full stop, the semisync gate) is driven from
/// rpl_replica.cc / rpl_mi.cc and is covered by MTR integration tests, not this
/// unit harness.
///
/// The queue rests STOPPED for both roles at construction (the mi-owned model:
/// the queue exists before any thread attaches), so every test arms the role(s)
/// it uses first: resume(RECEIVER) to enqueue, resume(APPLIER) to dispatch.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {
using Scope = Trx_envelope_queue::Scope;

/// A short bounded wait used to confirm that an UNBOUNDED blocking call has not
/// returned -- i.e. it parked. It is used only to observe acquire_admission(),
/// which has no idle timeout and can never return on its own: the future can
/// only become ready via a stop or a release, so this observation is
/// deterministic and never races (robust under TSAN/ASAN and on loaded CI). Its
/// exact value is not timing-critical for correctness; it only needs to be long
/// enough for the producer thread to reach the block. It is deliberately NOT
/// used to observe dispatch_next(), which is a bounded poller that self-returns
/// nullptr after ~100ms -- observing "still parked" there with a timed window
/// would be a genuine wall-clock race (the flake this test file had).
constexpr std::chrono::milliseconds kShortWait{20};

/// A generous wait used only on the success path, where the call is expected to
/// have already returned; it just bounds a hang so a regression fails fast.
constexpr std::chrono::seconds kJoinWait{3};

/// Memory bounds large enough that enqueue never blocks in admission (unless a
/// test deliberately saturates the budget with add_bytes): the limit dwarfs any
/// total the tests reserve, and the spill threshold dwarfs any single
/// transaction length, so every enqueue takes the (non-blocking) MEMORY path.
constexpr std::size_t kMemoryLimit = std::size_t{1} << 30;     // 1 GiB
constexpr std::size_t kSpillThreshold = std::size_t{1} << 20;  // 1 MiB

/// Build the active FDE that every enqueue() must be handed; enqueue() only
/// asserts it non-null (these tests never create a real byte source).
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// Arm both roles, as an attached receiver and applier do.
void resume_both_roles(Trx_envelope_queue &queue) {
  queue.resume(Scope::RECEIVER);
  queue.resume(Scope::APPLIER);
}

/// Whether the receiver role is stopped. acquire_admission() checks the
/// receiver stop flag before it reserves, and a 0-byte memory reservation
/// always fits, so this returns at once and reserves nothing.
bool receiver_stopped(Trx_envelope_queue &queue) {
  return queue.acquire_admission(0, Envelope_path::MEMORY);
}

/// Whether the applier role is stopped (its stop flag is set).
bool applier_stopped(const Trx_envelope_queue &queue) {
  return queue.applier_stop_flag()->load();
}
}  // namespace

// ---------------------------------------------------------------------------
// Role stop-flags as attach state.
// ---------------------------------------------------------------------------

// A freshly constructed queue rests stopped for BOTH roles: it exists before
// any thread attaches, so each role's stop flag is set until it is resumed.
TEST(ImrQueueLifecycleTest, FreshQueueHasBothRolesStopped) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  EXPECT_TRUE(receiver_stopped(queue));
  EXPECT_TRUE(applier_stopped(queue));
  // Empty and unused: nothing to drain before destruction.
}

// resume(scope) arms only the named role and stop(scope) stops only the named
// role; the other role's flag is left as it was. resume() is idempotent for
// each role.
TEST(ImrQueueLifecycleTest, ResumeAndStopToggleOnlyTheNamedRole) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));

  queue.resume(Scope::RECEIVER);
  EXPECT_FALSE(receiver_stopped(queue));
  EXPECT_TRUE(applier_stopped(queue))
      << "resume(RECEIVER) must not arm the applier";
  queue.resume(Scope::RECEIVER);  // idempotent: still armed.
  EXPECT_FALSE(receiver_stopped(queue));

  queue.resume(Scope::APPLIER);
  EXPECT_FALSE(applier_stopped(queue));
  queue.resume(Scope::APPLIER);  // idempotent: still armed.
  EXPECT_FALSE(applier_stopped(queue));
  EXPECT_FALSE(receiver_stopped(queue));

  queue.stop(Scope::APPLIER);
  EXPECT_TRUE(applier_stopped(queue));
  EXPECT_FALSE(receiver_stopped(queue))
      << "stop(APPLIER) must not stop the receiver";

  queue.stop(Scope::RECEIVER);
  EXPECT_TRUE(receiver_stopped(queue));
  EXPECT_TRUE(applier_stopped(queue));
}

// ---------------------------------------------------------------------------
// resume() clears a prior scoped stop().
// ---------------------------------------------------------------------------

// A receiver-scoped stop makes the MEMORY-path enqueue fail (its admission
// bails); resume(RECEIVER) clears that so a later enqueue is admitted again.
TEST(ImrQueueLifecycleTest, ResumeReceiverReenablesEnqueue) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  queue.resume(Scope::RECEIVER);

  queue.stop(Scope::RECEIVER);
  // Admission bails on the receiver stop, so enqueue reports failure (nullptr)
  // and reserves nothing.
  EXPECT_EQ(queue.enqueue(11, true, make_fde()), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);

  queue.resume(Scope::RECEIVER);
  Transaction_envelope *env = queue.enqueue(11, true, make_fde());
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(queue.insert_seqno(), 1u);

  queue.reset();  // drop the live envelope; back to the pristine empty state.
}

// An applier-scoped stop makes dispatch_next() return nullptr even with an
// undispatched envelope present; resume(APPLIER) clears that so the same
// envelope dispatches.
TEST(ImrQueueLifecycleTest, ResumeApplierReenablesDispatch) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Transaction_envelope *env = queue.enqueue(11, true, make_fde());
  ASSERT_NE(env, nullptr);

  queue.stop(Scope::APPLIER);
  EXPECT_EQ(queue.dispatch_next(), nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 0u)
      << "a stopped dispatch advances nothing";

  queue.resume(Scope::APPLIER);
  Transaction_envelope *dispatched = queue.dispatch_next();
  ASSERT_EQ(dispatched, env);
  EXPECT_EQ(queue.dispatch_seqno(), 1u);

  queue.reset();
}

// ---------------------------------------------------------------------------
// reset() drains and zeroes, and the queue is reusable afterwards.
// ---------------------------------------------------------------------------

// reset() drops every leftover envelope (even dispatched / committed ones),
// zeroes the three cursors and returns bytes_used() to 0. It does not touch the
// role stop-flags.
TEST(ImrQueueLifecycleTest, ResetDrainsAndZeroesCursors) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(queue.enqueue(22, true, make_fde()), nullptr);
  ASSERT_NE(queue.enqueue(33, true, make_fde()), nullptr);
  ASSERT_EQ(queue.insert_seqno(), 3u);
  ASSERT_EQ(queue.queue_length(), 3u);
  ASSERT_EQ(queue.bytes_used(), 66u);

  // Advance the dispatch and commit cursors so reset() has non-zero cursors to
  // clear: dispatch and commit+sweep the head.
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_FALSE(e1->commit());
  ASSERT_FALSE(queue.sweep_committed());
  ASSERT_EQ(queue.commit_seqno(), 1u);
  ASSERT_EQ(queue.dispatch_seqno(), 1u);

  queue.reset();

  EXPECT_EQ(queue.commit_seqno(), 0u);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);
  EXPECT_EQ(queue.insert_seqno(), 0u);
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
  // reset() leaves the attach state alone: both roles were armed, still armed.
  EXPECT_FALSE(receiver_stopped(queue));
  EXPECT_FALSE(applier_stopped(queue));
}

// After reset() the queue is reusable: enqueue restarts the cursors at 1 and
// they track a fresh session.
TEST(ImrQueueLifecycleTest, ResetLeavesQueueReusable) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  queue.resume(Scope::RECEIVER);

  ASSERT_NE(queue.enqueue(11, true, make_fde()), nullptr);
  ASSERT_NE(queue.enqueue(22, true, make_fde()), nullptr);
  ASSERT_EQ(queue.insert_seqno(), 2u);

  queue.reset();
  ASSERT_EQ(queue.insert_seqno(), 0u);
  ASSERT_EQ(queue.bytes_used(), 0u);

  // Reuse: a new enqueue starts a fresh numbering from 1.
  Transaction_envelope *again = queue.enqueue(44, true, make_fde());
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(queue.insert_seqno(), 1u);

  queue.reset();
}

// ---------------------------------------------------------------------------
// Scoped wake: one role stops without poisoning the other.
// ---------------------------------------------------------------------------

// stop(APPLIER) wakes a coordinator parked in dispatch_next() (it returns
// nullptr) while the receiver keeps admitting: the receiver stays armed and a
// concurrent-role enqueue still succeeds.
TEST(ImrQueueLifecycleTest, StopApplierWakesDispatchReceiverKeepsAdmitting) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  std::promise<Transaction_envelope *> got;
  std::future<Transaction_envelope *> fut = got.get_future();
  // Run a concurrent consumer in dispatch_next() on the empty queue; this also
  // exercises the stop()-vs-dispatch_next() concurrency for TSAN's race
  // detector. dispatch_next() is a bounded poller (it self-returns nullptr
  // after ~100ms even without a stop), so "still parked" is NOT a property we
  // can assert without a wall-clock race -- we assert only the deterministic
  // outcome below. The "stop wakes a concurrently parked waiter" mechanism is
  // covered deterministically by the acquire_admission() waiter
  // (StopReceiver... / StopBothRoles...), which has no idle timeout.
  std::thread consumer([&] { got.set_value(queue.dispatch_next()); });

  queue.stop(Scope::APPLIER);
  // kJoinWait only bounds a hang regression; dispatch returns within ~100ms.
  const bool returned = fut.wait_for(kJoinWait) == std::future_status::ready;

  consumer.join();  // always joined: stop(APPLIER) (or the idle poll) ended it.

  EXPECT_TRUE(returned)
      << "a concurrent dispatch_next() must finish after stop(APPLIER)";
  EXPECT_EQ(fut.get(), nullptr) << "a stopped dispatch returns nullptr";

  // The applier is stopped but the receiver is not: the receiver can still
  // admit and enqueue.
  EXPECT_TRUE(applier_stopped(queue));
  EXPECT_FALSE(receiver_stopped(queue));
  Transaction_envelope *env = queue.enqueue(11, true, make_fde());
  EXPECT_NE(env, nullptr) << "stop(APPLIER) must not poison the receiver";

  queue.reset();
}

// stop(RECEIVER) wakes an IO thread parked in acquire_admission() (it returns
// failure) while the applier keeps dispatching: the applier stays armed and a
// concurrent-role dispatch of an already-enqueued envelope still succeeds.
TEST(ImrQueueLifecycleTest, StopReceiverWakesAdmissionApplierKeepsDispatching) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  // One dispatchable envelope enqueued before the budget is saturated.
  Transaction_envelope *env = queue.enqueue(11, true, make_fde());
  ASSERT_NE(env, nullptr);

  // Saturate the rest of the budget so the next admission blocks. env already
  // holds its own 11-byte reservation, and add_bytes() must fit, so reserve
  // only the remainder. This reservation is not tied to any payload, so it must
  // be released by hand before reset().
  const std::size_t saturation = kMemoryLimit - queue.bytes_used();
  queue.add_bytes(saturation);
  ASSERT_EQ(queue.bytes_used(), kMemoryLimit);

  std::promise<bool> failed;
  std::future<bool> fut = failed.get_future();
  // Park a producer in acquire_admission(): the budget is full, so it blocks.
  std::thread producer([&] {
    failed.set_value(queue.acquire_admission(100, Envelope_path::MEMORY));
  });

  // acquire_admission() has NO idle timeout (unlike dispatch_next()): it stays
  // blocked until a stop or a release, so this "still parked" observation is
  // deterministic -- the future can never become ready on its own, so it never
  // races (robust under TSAN/ASAN and loaded CI). Recorded, not asserted, so
  // the thread is always joined below.
  const bool parked = fut.wait_for(kShortWait) == std::future_status::timeout;

  queue.stop(Scope::RECEIVER);
  const bool woken = fut.wait_for(kJoinWait) == std::future_status::ready;

  producer.join();  // always: stop(RECEIVER) unblocked acquire_admission().

  EXPECT_TRUE(parked)
      << "acquire_admission() must park when the budget is full";
  EXPECT_TRUE(woken)
      << "stop(RECEIVER) must wake the parked acquire_admission()";
  EXPECT_TRUE(fut.get()) << "a stopped admission reports failure";

  // The receiver is stopped but the applier is not: the applier can still
  // dispatch the envelope enqueued earlier.
  EXPECT_TRUE(receiver_stopped(queue));
  EXPECT_FALSE(applier_stopped(queue));
  EXPECT_EQ(queue.dispatch_next(), env)
      << "stop(RECEIVER) must not poison the applier";

  queue.release_bytes(saturation);  // undo the manual saturation.
  queue.reset();                    // drop env and its reserved bytes.
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// stop(APPLIER) then stop(RECEIVER) wake BOTH a parked dispatch_next() and a
// parked acquire_admission().
TEST(ImrQueueLifecycleTest, StopBothRolesWakesBothWaiters) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  // Saturate so the producer blocks in admission; queue empty so the consumer
  // blocks in dispatch.
  queue.add_bytes(kMemoryLimit);

  std::promise<bool> admission_failed;
  std::future<bool> admission_fut = admission_failed.get_future();
  std::thread producer([&] {
    admission_failed.set_value(
        queue.acquire_admission(100, Envelope_path::MEMORY));
  });

  std::promise<Transaction_envelope *> dispatched;
  std::future<Transaction_envelope *> dispatch_fut = dispatched.get_future();
  std::thread consumer([&] { dispatched.set_value(queue.dispatch_next()); });

  // acquire_admission() has NO idle timeout, so observing it "still parked" is
  // deterministic and never races. dispatch_next() IS a bounded poller (it
  // self-returns nullptr after ~100ms), so we do NOT assert it "still parked"
  // -- that would be a wall-clock race (fragile under TSAN/ASAN/load); we
  // assert only its deterministic outcome after the stop. The concurrent
  // consumer still exercises stop()-vs-dispatch_next() for TSAN's race
  // detector.
  const bool admission_parked =
      admission_fut.wait_for(kShortWait) == std::future_status::timeout;

  // The order of a full STOP REPLICA: applier, then receiver.
  queue.stop(Scope::APPLIER);
  queue.stop(Scope::RECEIVER);

  // kJoinWait only bounds a hang regression; both waiters return within ~100ms.
  const bool admission_woken =
      admission_fut.wait_for(kJoinWait) == std::future_status::ready;
  const bool dispatch_returned =
      dispatch_fut.wait_for(kJoinWait) == std::future_status::ready;

  producer.join();
  consumer.join();

  EXPECT_TRUE(admission_parked)
      << "acquire_admission() must park when the budget is full";
  EXPECT_TRUE(admission_woken)
      << "stop(RECEIVER) must wake the parked acquire_admission()";
  EXPECT_TRUE(dispatch_returned)
      << "a concurrent dispatch_next() must finish after stop(APPLIER)";
  EXPECT_TRUE(admission_fut.get());
  EXPECT_EQ(dispatch_fut.get(), nullptr);

  EXPECT_TRUE(receiver_stopped(queue));
  EXPECT_TRUE(applier_stopped(queue));

  queue.release_bytes(kMemoryLimit);  // undo the manual saturation.
  queue.reset();                      // queue is already empty; bytes -> 0.
}

// sweep_committed()'s guard must never advance commit_seqno past
// dispatch_seqno, even when contiguous committed envelopes sit AHEAD of the
// dispatch cursor -- the situation that arises when an out-of-order commit
// lands right after a dispatched head. The sweep stops at the dispatch cursor
// and reclaims the rest only as dispatch advances.
TEST(ImrQueueLifecycleTest, SweepDoesNotAdvanceCommitPastDispatch) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);

  // Dispatch only e1, but commit BOTH e1 and e2 -- e2 is committed "ahead" of
  // the dispatch cursor, mimicking an out-of-order commit sitting past the
  // rewound dispatch_seqno.
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_seqno(), 1u);
  ASSERT_FALSE(e1->commit());
  ASSERT_FALSE(e2->commit());

  // Sweep reclaims e1 (commit 0 -> 1) but must STOP there: sweeping e2 would
  // push commit_seqno (2) past dispatch_seqno (1).
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.dispatch_seqno(), 1u);
  EXPECT_LE(queue.commit_seqno(), queue.dispatch_seqno());
  EXPECT_EQ(queue.queue_length(), 1u);  // e2 still queued

  // Once e2 is dispatched (dispatch == 2), the next sweep may reclaim it.
  ASSERT_EQ(queue.dispatch_next(), e2);
  ASSERT_EQ(queue.dispatch_seqno(), 2u);
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 2u);
  EXPECT_LE(queue.commit_seqno(), queue.dispatch_seqno());
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// ---------------------------------------------------------------------------
// Truncated envelopes are reclaimed by the sweep and skip-dispatched.
// ---------------------------------------------------------------------------

// A dispatched envelope marked truncated is a terminal state the sweep
// reclaims exactly like a committed one: sweep_committed() drops it, advances
// the commit low-water mark, and releases its reserved bytes (at sweep, since
// truncation -- unlike commit -- does not reset the payload).
TEST(ImrQueueLifecycleTest, SweepReclaimsDispatchedTruncatedHead) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_EQ(queue.bytes_used(), 11u);

  // Dispatch it, then truncate it (models a worker that was streaming-applying
  // when the receiver truncated the transaction).
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_seqno(), 1u);
  e1->set_truncated();
  ASSERT_TRUE(e1->is_truncated());
  // Truncation alone does not release the payload; the bytes are still charged.
  ASSERT_EQ(queue.bytes_used(), 11u);

  // The sweep reclaims the truncated head just like a committed one.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);  // released at sweep (envelope dropped)
}

// A truncated envelope that was NEVER dispatched is reclaimed via
// skip-dispatch: sweep_and_dispatch() advances the dispatch cursor past it
// (creating no worker for it), the follow-up sweep drops it, and the next live
// transaction is returned. This is the path that keeps the applier progressing
// after an IO-only stop truncates the open transaction.
TEST(ImrQueueLifecycleTest,
     SweepAndDispatchSkipsUndispatchedTruncatedAndReturnsNext) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);
  ASSERT_EQ(queue.bytes_used(), 33u);

  // Truncate e1 before it is ever dispatched.
  e1->set_truncated();

  // One coordinator step: e1 is skip-dispatched + reclaimed, e2 is returned.
  Transaction_envelope *dispatched = queue.sweep_and_dispatch();
  EXPECT_EQ(dispatched, e2)
      << "the truncated head must be skipped, next returned";

  // e1 reclaimed (commit advanced past it), e2 dispatched, cursors consistent.
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.dispatch_seqno(), 2u);
  EXPECT_LE(queue.commit_seqno(), queue.dispatch_seqno());
  EXPECT_EQ(queue.queue_length(), 1u);  // only e2 remains
  EXPECT_EQ(queue.bytes_used(), 22u);   // e1's 11 released, e2's 22 still held

  queue.reset();  // drop e2 and its reserved bytes.
}

// The sweep reclaims a contiguous head prefix that mixes both terminal states:
// committed and truncated envelopes are dropped together, in order, and every
// reserved byte is released.
TEST(ImrQueueLifecycleTest, SweepReclaimsContiguousCommittedAndTruncated) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  Transaction_envelope *e2 = queue.enqueue(22, true, make_fde());
  Transaction_envelope *e3 = queue.enqueue(33, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);
  ASSERT_NE(e3, nullptr);

  // Dispatch all three, then finalize with a mix: commit, truncate, commit.
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_EQ(queue.dispatch_next(), e2);
  ASSERT_EQ(queue.dispatch_next(), e3);
  ASSERT_FALSE(e1->commit());
  e2->set_truncated();
  ASSERT_FALSE(e3->commit());

  // One sweep drops the whole finalized prefix e1..e3.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 3u);
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// Lifetime: the sweep may destroy a truncated envelope while an in-flight
// worker still references the transaction via its
// shared_ptr<Fetchable_transaction>. Dropping the envelope releases the payload
// bytes but must not invalidate the still-held byte source. The truncated path
// never calls the sink's set_success() (the only m_envelope deref), so the
// dangling back-pointer is never followed -- exercised here by keeping the
// fetchable alive across the sweep and using it afterwards.
TEST(ImrQueueLifecycleTest,
     SweepTruncatedWhileWorkerHoldsFetchableNoUseAfterFree) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);

  Transaction_envelope *e1 = queue.enqueue(11, true, make_fde());
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e1->payload(), nullptr);

  // A worker would hold a shared-ownership copy of the byte source; take one
  // here so it outlives the envelope the sweep is about to destroy.
  std::shared_ptr<Fetchable_transaction> worker_ref =
      e1->payload()->fetchable();
  ASSERT_NE(worker_ref, nullptr);

  ASSERT_EQ(queue.dispatch_next(), e1);
  e1->set_truncated();

  // Sweep destroys the envelope (and its payload) -- e1 dangles now.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);  // payload bytes released exactly once

  // The byte source is still alive through worker_ref and safely usable; this
  // touches the sink WITHOUT going through set_success()/m_envelope.
  EXPECT_TRUE(worker_ref->is_trx());

  worker_ref.reset();  // last reference: byte source destroyed cleanly.
}

}  // namespace mysql::csa::unittests
