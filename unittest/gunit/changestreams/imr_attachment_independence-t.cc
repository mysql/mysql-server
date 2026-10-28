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
/// Unit tests for the replication role lifecycle of
/// mysql::csa::Imr_runtime.
///
/// **Property: The queue survives any single-role detach.**
///
/// The channel-lifetime Imr_runtime owns the queue. Each replication
/// thread attaches its role (RECEIVER for the IO thread, APPLIER for the SQL
/// thread) with Imr_runtime::attach_role() when it starts, and
/// detaches it with detach_role() in its own exit path, once it no longer uses
/// the queue. attach_role() records the calling thread as the role's owner,
/// clears the role's stop flag on the queue.
/// detach_role() acts only for the owner: it truncates the receiver's open
/// group (RECEIVER) or sweeps the applier's committed head (APPLIER), sets the
/// stop flag, and clears the owner last. So Imr_runtime::fully_detached(),
/// which reports that NEITHER role has an owner, tracks the threads'
/// lifetime. "Stop requested" is request_stop(): a
/// STOP calls it before the threads exit, to wake them. It sets only the stop
/// flag, so "stop requested" is not "detached".
///
/// The cases check that:
/// - the queue object persists (its address is stable) over arbitrary
///   attach/detach interleavings, and at each step the expected state matches
///   fully_detached(), is_role_owner() and Imr_runtime::is_attached();
/// - a stop request without a detach leaves the runtime attached, the state
///   that suppresses the reset on a join timeout;
/// - a role has at most one owner: a second attach_role(), from another
///   thread or from the owner, is refused and changes nothing, and
///   detach_role() or truncate_receiver_group() from a thread that does not
///   own the role is a no-op;
/// - detach_role(RECEIVER) truncates a still-open group and drops its sink;
/// - set_receiver_fde() takes a read-only copy of the receiver's FDE, equal to
///   it field by field. Every open_transaction() hands that copy out until the
///   next set_receiver_fde(), and detach_role(RECEIVER) releases it. Its events
///   decode correctly while the receiver's object has its checksum algorithm
///   switched in place. In debug builds, open_transaction() without a
///   snapshot, or after an in-place change of a decoding field, asserts; in
///   release builds it copies the receiver's FDE.
///
/// The cases pass a null Master_info to detach_role(), which skips the
/// last-out reset. The reset with a real Master_info, and the thread exit
/// paths that call these methods, are covered by MTR only.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "my_byteorder.h"       // int4store
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/imr_runtime.h"
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/log_event.h"  // Format_description_log_event
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"
#include "unittest/gunit/test_utils.h"  // my_testing::Server_initializer

namespace mysql::csa::unittests {

namespace {
using Scope = Imr_runtime::Scope;
constexpr Scope kReceiver = Scope::RECEIVER;
constexpr Scope kApplier = Scope::APPLIER;

/// A runtime whose queue never blocks or spills. There is no memory resource,
/// so the memory limit is not enforced, and the receiver cases open groups of
/// kTrxLength, below the spill threshold, so they take the memory path. The
/// spill directory is empty.
Imr_config make_config() {
  Imr_config cfg;
  cfg.memory_limit = std::size_t{1} << 30;
  cfg.spill_threshold = std::size_t{1} << 20;
  return cfg;
}

constexpr int kIterations = 500;
constexpr std::size_t kTrxLength = 4096;

/// On scope exit, detaches every role the test thread still owns and empties
/// the queue. ~Imr_runtime asserts fully_detached() and
/// ~Trx_envelope_queue asserts an empty deque. Without this guard, a failing
/// ASSERT_* that returns from a case with a role attached or a group queued
/// would abort the whole binary instead of failing that one case. Declare it
/// right after the runtime, so it runs before the runtime's destructor. On a
/// case's success path every role is already detached and the queue is
/// already empty, so it changes nothing.
class Detach_guard {
 public:
  explicit Detach_guard(Imr_runtime &imr) : m_imr(imr) {}
  Detach_guard(const Detach_guard &) = delete;
  Detach_guard &operator=(const Detach_guard &) = delete;
  ~Detach_guard() {
    for (const Scope scope : {kReceiver, kApplier}) {
      if (m_imr.is_role_owner(scope)) (void)m_imr.detach_role(scope, nullptr);
    }
    m_imr.queue()->reset();
  }

 private:
  Imr_runtime &m_imr;
};
}  // namespace

// A fresh runtime is fully detached (neither role attached, no owner).
TEST(ImrAttachmentIndependenceTest, FreshRuntimeIsFullyDetached) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  EXPECT_TRUE(imr.fully_detached());
  EXPECT_FALSE(imr.is_attached(kReceiver));
  EXPECT_FALSE(imr.is_attached(kApplier));
  EXPECT_FALSE(imr.is_role_owner(kReceiver));
  EXPECT_FALSE(imr.is_role_owner(kApplier));
}

// Detaching the applier while the receiver stays attached keeps the queue live
// (same object) and NOT fully detached; detaching the last role flips it back.
TEST(ImrAttachmentIndependenceTest, SingleRoleDetachKeepsQueueAndNotDetached) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  const Trx_envelope_queue *q = imr.queue();

  ASSERT_FALSE(imr.attach_role(kReceiver));
  ASSERT_FALSE(imr.attach_role(kApplier));
  EXPECT_FALSE(imr.fully_detached());
  EXPECT_EQ(imr.queue(), q);

  // stop the SQL thread only
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  EXPECT_FALSE(imr.is_attached(kApplier));
  EXPECT_TRUE(imr.is_attached(kReceiver));
  EXPECT_FALSE(imr.fully_detached()) << "receiver still attached";
  EXPECT_EQ(imr.queue(), q) << "queue must survive an applier-only detach";

  // now the last role detaches
  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_FALSE(imr.is_attached(kReceiver));
  EXPECT_TRUE(imr.fully_detached());
  EXPECT_EQ(imr.queue(), q);
}

// "Stop requested" is distinct from "detached".
//
// request_stop() sets a role's stop flag (the wake a STOP sends before the
// threads exit) but does NOT detach it. This reproduces the join-timeout
// window: the stop-command thread has asked both roles to stop -- so both role
// stop flags are set -- yet the replication threads have not exited, so
// their roles are still attached. In that state fully_detached() MUST stay
// false, which is what suppresses the volatile-state reset while a thread
// could still be touching the queue. Only detach_role(), which each thread
// calls in its own exit path, flips fully_detached() to true.
TEST(ImrAttachmentIndependenceTest, StopRequestedIsNotDetached) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  ASSERT_FALSE(imr.attach_role(kReceiver));
  ASSERT_FALSE(imr.attach_role(kApplier));
  ASSERT_FALSE(imr.fully_detached());

  // Ask both roles to stop (the STOP wake) without detaching them.
  imr.request_stop(kReceiver);
  imr.request_stop(kApplier);

  // Both stop flags are set -- the exact state on a join timeout -- but both
  // roles are still attached, so it is NOT detached.
  // A 0-byte admission fails only on a receiver stop.
  EXPECT_TRUE(imr.queue()->acquire_admission(0, Envelope_path::MEMORY))
      << "the receiver was asked to stop";
  EXPECT_TRUE(imr.queue()->applier_stop_flag()->load())
      << "the applier was asked to stop";
  EXPECT_TRUE(imr.is_attached(kReceiver));
  EXPECT_TRUE(imr.is_attached(kApplier));
  EXPECT_FALSE(imr.fully_detached())
      << "stop requested but roles still attached must not be detached";

  // Detaching one role is still not enough.
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  EXPECT_FALSE(imr.fully_detached()) << "receiver still attached";

  // Detaching the last role (in its thread's exit path, in the real server)
  // makes the runtime fully detached.
  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_TRUE(imr.fully_detached());
}

// Over random attach/detach interleavings the queue address never changes. At
// every step each role's attachment (Imr_runtime::is_attached()) and
// ownership by this thread (is_role_owner()) match the expected state, and
// fully_detached() equals "neither role attached". Attaching an attached role
// is refused and changes nothing; detaching a detached role is a no-op.
TEST(ImrAttachmentIndependenceTest, QueueSurvivesArbitraryInterleavings) {
  std::mt19937 rng(20260916u);
  std::uniform_int_distribution<int> role_dist(0, 1);  // 0 receiver, 1 applier
  std::uniform_int_distribution<int> action_dist(0, 1);  // 0 detach, 1 attach

  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  const Trx_envelope_queue *const q = imr.queue();

  bool receiver_attached = false;
  bool applier_attached = false;

  for (int i = 0; i < kIterations; ++i) {
    const bool attach = action_dist(rng) != 0;
    const Scope scope = role_dist(rng) == 0 ? kReceiver : kApplier;
    bool &attached = scope == kReceiver ? receiver_attached : applier_attached;

    if (attach) {
      // attach_role() returns true (refused) exactly when already attached.
      EXPECT_EQ(imr.attach_role(scope), attached) << "iteration " << i;
      attached = true;
    } else {
      // detach_role() returns true exactly when this call detached the role.
      EXPECT_EQ(imr.detach_role(scope, nullptr), attached) << "iteration " << i;
      attached = false;
    }

    const bool any_attached = receiver_attached || applier_attached;

    // The queue object is stable regardless of the roles' comings and goings
    // (attach/detach never allocates or destroys the queue).
    EXPECT_EQ(imr.queue(), q) << "iteration " << i;

    // Each role's attachment and owner match the expected state.
    EXPECT_EQ(imr.is_attached(kReceiver), receiver_attached)
        << "iteration " << i;
    EXPECT_EQ(imr.is_attached(kApplier), applier_attached) << "iteration " << i;
    EXPECT_EQ(imr.is_role_owner(kReceiver), receiver_attached)
        << "iteration " << i;
    EXPECT_EQ(imr.is_role_owner(kApplier), applier_attached)
        << "iteration " << i;

    // fully_detached() is true exactly when neither role is attached; false
    // while at least one is.
    EXPECT_EQ(imr.fully_detached(), !any_attached) << "iteration " << i;
  }

  // Detach both roles before the runtime goes out of scope, as the threads'
  // exit paths would: the runtime's destructor asserts fully_detached().
  EXPECT_EQ(imr.detach_role(kReceiver, nullptr), receiver_attached);
  EXPECT_EQ(imr.detach_role(kApplier, nullptr), applier_attached);
  EXPECT_TRUE(imr.fully_detached());
}

// A role has at most one owner: a second attach of an attached role is refused
// and changes nothing, and the one detach of the owner detaches. The refused
// attach is made first from another thread (a second SQL thread in the start
// window), then again from the owner.
TEST(ImrAttachmentIndependenceTest, SecondAttachIsRefused) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  ASSERT_FALSE(imr.attach_role(kApplier));
  ASSERT_TRUE(imr.is_attached(kApplier));

  bool other_refused = false;
  std::thread other([&] {
    other_refused = imr.attach_role(kApplier);
    // On a regression this thread now owns the role. Detach_guard releases
    // only the test thread's roles, so give it back here: the case then fails
    // instead of tripping ~Imr_runtime.
    if (!other_refused) (void)imr.detach_role(kApplier, nullptr);
  });
  other.join();
  EXPECT_TRUE(other_refused) << "a second thread's attach must be refused";
  ASSERT_TRUE(imr.is_role_owner(kApplier)) << "owner must not change";
  EXPECT_TRUE(imr.is_attached(kApplier));

  EXPECT_TRUE(imr.attach_role(kApplier)) << "second attach must be refused";
  EXPECT_TRUE(imr.is_attached(kApplier));
  EXPECT_TRUE(imr.is_role_owner(kApplier)) << "owner must not change";
  EXPECT_FALSE(imr.fully_detached());

  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  EXPECT_FALSE(imr.is_attached(kApplier));
  EXPECT_FALSE(imr.is_role_owner(kApplier));
  EXPECT_TRUE(imr.fully_detached());
}

// Only the owner detaches: a detach from another thread (the exit path of a
// thread whose attach was refused) is a no-op and leaves the role attached.
TEST(ImrAttachmentIndependenceTest, NonOwnerDetachIsNoOp) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  ASSERT_FALSE(imr.attach_role(kApplier));

  bool other_detached = true;
  bool other_is_owner = true;
  std::thread other([&] {
    other_is_owner = imr.is_role_owner(kApplier);
    other_detached = imr.detach_role(kApplier, nullptr);
  });
  other.join();

  EXPECT_FALSE(other_is_owner);
  EXPECT_FALSE(other_detached) << "a non-owner detach must be a no-op";
  EXPECT_TRUE(imr.is_attached(kApplier));
  EXPECT_TRUE(imr.is_role_owner(kApplier));
  EXPECT_FALSE(imr.fully_detached());

  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  EXPECT_TRUE(imr.fully_detached());
}

// ---------------------------------------------------------------------------
// Receiver sink. The IO thread that owns RECEIVER opens a group with
// open_transaction(), and receiver_sink() is then the group's sink.
// open_transaction(), append_event() and receiver_sink() debug-assert
// ownership, so the cases call them only on the test thread, which owns
// RECEIVER. The other std::thread calls only attach_role(), which refuses on
// the owner, and truncate_receiver_group(), detach_role() and is_role_owner(),
// which check ownership in all builds. The live receiver refusal cannot be
// reached from MTR, so these cases are its only test coverage.
// ---------------------------------------------------------------------------

// detach_role(RECEIVER) truncates a group the IO thread left open (a stop or
// an error mid-transaction) before it clears the owner, so the applier rolls
// the partial transaction back instead of waiting on it, and drops the sink.
TEST(ImrAttachmentIndependenceTest, ReceiverDetachTruncatesOpenGroup) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  Format_description_log_event fde;
  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(fde);
  ASSERT_FALSE(imr.open_transaction(fde, kTrxLength));
  EXPECT_NE(imr.receiver_sink(), nullptr) << "the group must be open";
  EXPECT_EQ(imr.queue()->queue_length(), 1u);

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_FALSE(imr.is_attached(kReceiver));
  EXPECT_TRUE(imr.fully_detached());

  // Look at the queued envelope through the applier: attaching it arms
  // dispatch_next(), which hands out the one undispatched envelope at once.
  // The applier's detach then sweeps the dispatched, truncated envelope, so
  // env must not be used after it.
  ASSERT_FALSE(imr.attach_role(kApplier));
  Transaction_envelope *env = imr.queue()->dispatch_next();
  ASSERT_NE(env, nullptr);
  EXPECT_TRUE(env->is_truncated()) << "detach must truncate the open group";
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));

  // The sink was dropped: the next receiver starts with none (attach_role()
  // also asserts this in debug builds).
  ASSERT_FALSE(imr.attach_role(kReceiver));
  EXPECT_EQ(imr.receiver_sink(), nullptr);
  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));

  // ~Trx_envelope_queue asserts an empty deque.
  imr.queue()->reset();
}

// Only the RECEIVER owner truncates: truncate_receiver_group() from another
// thread (the exit path of an IO thread whose attach was refused) leaves the
// owner's open group alone.
TEST(ImrAttachmentIndependenceTest, NonOwnerTruncateIsNoOp) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  Format_description_log_event fde;
  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(fde);
  ASSERT_FALSE(imr.open_transaction(fde, kTrxLength));
  Streaming_event_sink *const sink = imr.receiver_sink();
  ASSERT_NE(sink, nullptr);

  std::thread other([&] { imr.truncate_receiver_group(); });
  other.join();

  EXPECT_EQ(imr.receiver_sink(), sink) << "the owner's group must stay open";

  // The queued envelope is not truncated. Attaching the applier arms
  // dispatch_next(), which hands it out at once.
  ASSERT_FALSE(imr.attach_role(kApplier));
  Transaction_envelope *env = imr.queue()->dispatch_next();
  ASSERT_NE(env, nullptr);
  EXPECT_FALSE(env->is_truncated())
      << "a non-owner truncate must leave the group intact";

  // The owner's detach truncates the group, and the applier's detach then
  // sweeps it. env must not be used after that.
  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));

  // ~Trx_envelope_queue asserts an empty deque.
  imr.queue()->reset();
}

// last_write_failure() describes only the last receiver write call.
// It is reset at the start of each call and when a receiver attaches.
TEST(ImrAttachmentIndependenceTest, ReceiverWriteFailureRecordResets) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  Format_description_log_event fde;
  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(fde);
  imr.request_stop(kReceiver);
  EXPECT_TRUE(imr.open_transaction(fde, kTrxLength));
  EXPECT_TRUE(imr.last_write_failure().stopped);
  EXPECT_TRUE(imr.last_write_failure().reason.empty());

  // The next call starts with a clean record: no group is open.
  const char buf[1] = {0};
  EXPECT_TRUE(imr.append_event(buf, sizeof(buf), /*is_terminal=*/false));
  EXPECT_FALSE(imr.last_write_failure().stopped);
  EXPECT_EQ(imr.last_write_failure().reason,
            "[In-memory relay log] no open transaction group for the event");

  // A new receiver starts with a clean record.
  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  ASSERT_FALSE(imr.attach_role(kReceiver));
  EXPECT_FALSE(imr.last_write_failure().stopped);
  EXPECT_TRUE(imr.last_write_failure().reason.empty());

  // The detach released the snapshot.
  imr.set_receiver_fde(fde);
  ASSERT_FALSE(imr.open_transaction(fde, kTrxLength));
  EXPECT_FALSE(imr.last_write_failure().stopped);
  EXPECT_TRUE(imr.last_write_failure().reason.empty());

  // The detach truncates the open group; ~Trx_envelope_queue asserts an empty
  // deque.
  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  imr.queue()->reset();
}

// The receiver has at most one owner: a second attach_role(RECEIVER) is
// refused and changes nothing. The refused attach is made first from another
// thread (a second IO thread in the start window), then from the owner with a
// group open, which must stay open.
TEST(ImrAttachmentIndependenceTest, SecondReceiverAttachIsRefused) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  Format_description_log_event fde;
  ASSERT_FALSE(imr.attach_role(kReceiver));
  ASSERT_TRUE(imr.is_attached(kReceiver));
  imr.set_receiver_fde(fde);

  // Before open_transaction(): on a regression the other thread's attach would
  // succeed, and attach_role() debug-asserts that no sink is open.
  bool other_refused = false;
  std::thread other([&] {
    other_refused = imr.attach_role(kReceiver);
    // On a regression this thread now owns the role. Detach_guard releases
    // only the test thread's roles, so give it back here: the case then fails
    // instead of tripping ~Imr_runtime.
    if (!other_refused) (void)imr.detach_role(kReceiver, nullptr);
  });
  other.join();
  EXPECT_TRUE(other_refused) << "a second thread's attach must be refused";
  ASSERT_TRUE(imr.is_role_owner(kReceiver)) << "owner must not change";
  EXPECT_TRUE(imr.is_attached(kReceiver));

  ASSERT_FALSE(imr.open_transaction(fde, kTrxLength));
  Streaming_event_sink *const sink = imr.receiver_sink();
  ASSERT_NE(sink, nullptr);

  EXPECT_TRUE(imr.attach_role(kReceiver)) << "second attach must be refused";
  EXPECT_TRUE(imr.is_attached(kReceiver));
  // receiver_sink() debug-asserts ownership, so check the owner first.
  ASSERT_TRUE(imr.is_role_owner(kReceiver)) << "owner must not change";
  EXPECT_EQ(imr.receiver_sink(), sink) << "the open group must not change";
  EXPECT_FALSE(imr.fully_detached());

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_FALSE(imr.is_attached(kReceiver));
  EXPECT_FALSE(imr.is_role_owner(kReceiver));
  EXPECT_TRUE(imr.fully_detached());

  // ~Trx_envelope_queue asserts an empty deque.
  imr.queue()->reset();
}

// Only the RECEIVER owner detaches: detach_role(RECEIVER) from another thread
// (the exit path of an IO thread whose attach was refused) is a no-op. The
// role stays attached and the owner's open group stays open.
TEST(ImrAttachmentIndependenceTest, NonOwnerReceiverDetachIsNoOp) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  Format_description_log_event fde;
  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(fde);
  ASSERT_FALSE(imr.open_transaction(fde, kTrxLength));
  Streaming_event_sink *const sink = imr.receiver_sink();
  ASSERT_NE(sink, nullptr);

  bool other_detached = true;
  bool other_is_owner = true;
  std::thread other([&] {
    other_is_owner = imr.is_role_owner(kReceiver);
    other_detached = imr.detach_role(kReceiver, nullptr);
  });
  other.join();

  EXPECT_FALSE(other_is_owner);
  EXPECT_FALSE(other_detached) << "a non-owner detach must be a no-op";
  EXPECT_TRUE(imr.is_attached(kReceiver));
  // receiver_sink() debug-asserts ownership, so check the owner first.
  ASSERT_TRUE(imr.is_role_owner(kReceiver));
  EXPECT_EQ(imr.receiver_sink(), sink) << "the owner's group must stay open";
  EXPECT_FALSE(imr.fully_detached());

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_TRUE(imr.fully_detached());

  // ~Trx_envelope_queue asserts an empty deque.
  imr.queue()->reset();
}

// ---------------------------------------------------------------------------
// Admission pre-wait. wait_for_admission() debug-asserts RECEIVER
// ownership, so the cases call it on the test thread; other threads only
// release bytes or request the stop.
// ---------------------------------------------------------------------------

namespace {
/// A runtime config with a small, enforced memory limit; transactions of
/// kPreWaitTrx stay on the memory path.
constexpr std::size_t kPreWaitLimit = 10000;
constexpr std::size_t kPreWaitTrx = 4000;
Imr_config make_limited_config() {
  Imr_config cfg = make_config();
  cfg.memory_limit = kPreWaitLimit;
  return cfg;
}
}  // namespace

// When the next transaction fits, wait_for_admission() returns at once and
// leaves the open group alone. When it does not fit, it truncates the open
// group first (the receiver then holds no envelope while it waits), waits,
// and returns once another thread frees enough memory. It reserves nothing.
TEST(ImrAttachmentIndependenceTest, WaitForAdmissionTruncatesOnlyWhenWaiting) {
  Imr_runtime imr(make_limited_config(), "",
                  test_memory_resource(kPreWaitLimit));
  Detach_guard guard(imr);

  Format_description_log_event fde;
  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(fde);
  ASSERT_FALSE(imr.open_transaction(fde, kPreWaitTrx));
  Streaming_event_sink *const sink = imr.receiver_sink();
  ASSERT_NE(sink, nullptr);
  ASSERT_EQ(imr.bytes_used(), kPreWaitTrx);

  // Fits (4000 + 4000 <= 10000): no wait, the open group stays open.
  EXPECT_TRUE(imr.is_admissible(kPreWaitTrx));
  EXPECT_FALSE(imr.wait_for_admission(kPreWaitTrx));
  EXPECT_EQ(imr.receiver_sink(), sink) << "a fitting trx must not truncate";
  EXPECT_EQ(imr.bytes_used(), kPreWaitTrx) << "the pre-wait reserves nothing";

  // Saturate: the open group's 4000 plus 6000 reserved out of band.
  imr.queue()->add_bytes(kPreWaitLimit - kPreWaitTrx);
  EXPECT_FALSE(imr.is_admissible(kPreWaitTrx));
  std::atomic<bool> released{false};
  std::thread releaser([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    released.store(true);
    imr.queue()->release_bytes(kPreWaitLimit - kPreWaitTrx);
  });
  EXPECT_FALSE(imr.wait_for_admission(kPreWaitTrx));
  EXPECT_TRUE(released.load()) << "it must have waited for the release";
  releaser.join();
  EXPECT_EQ(imr.receiver_sink(), nullptr) << "the open group was truncated";
  EXPECT_EQ(imr.bytes_used(), kPreWaitTrx) << "the pre-wait reserves nothing";

  // The truncated envelope is no longer held by the receiver: the applier
  // dispatches it truncated, and its detach sweeps it, freeing its bytes.
  ASSERT_FALSE(imr.attach_role(kApplier));
  Transaction_envelope *env = imr.queue()->dispatch_next();
  ASSERT_NE(env, nullptr);
  EXPECT_TRUE(env->is_truncated());
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  EXPECT_EQ(imr.bytes_used(), 0u);

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  imr.queue()->reset();
}

// A receiver stop ends the wait (true); nothing is reserved, and the
// following open_transaction() reports the stop.
TEST(ImrAttachmentIndependenceTest, WaitForAdmissionEndsOnReceiverStop) {
  Imr_runtime imr(make_limited_config(), "",
                  test_memory_resource(kPreWaitLimit));
  Detach_guard guard(imr);

  Format_description_log_event fde;
  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(fde);
  imr.queue()->add_bytes(kPreWaitLimit);
  std::thread stopper([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    imr.request_stop(kReceiver);
  });
  EXPECT_TRUE(imr.wait_for_admission(kPreWaitTrx));
  stopper.join();
  EXPECT_EQ(imr.bytes_used(), kPreWaitLimit);
  EXPECT_TRUE(imr.open_transaction(fde, kPreWaitTrx))
      << "open_transaction() after a stopped pre-wait reports the stop";
  EXPECT_EQ(imr.receiver_sink(), nullptr);

  imr.queue()->release_bytes(kPreWaitLimit);
  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  imr.queue()->reset();
}

// ---------------------------------------------------------------------------
// FDE snapshot. set_receiver_fde() copies the receiver's FDE, which the server
// writes in place under LOCK_log while workers decode. Every
// open_transaction() hands the copy out until the next set_receiver_fde(),
// and detach_role(RECEIVER) releases it.
// ---------------------------------------------------------------------------

namespace {
using mysql::binlog::event::BINLOG_CHECKSUM_ALG_CRC32;
using mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
using mysql::binlog::event::Log_event_type;

/// A raw checksum-free event: a 19-byte v4 common header and @p body.
std::vector<unsigned char> raw_event(Log_event_type type,
                                     const std::vector<unsigned char> &body) {
  std::vector<unsigned char> bytes(LOG_EVENT_HEADER_LEN, 0);
  int4store(bytes.data(), 1700000000);  // timestamp
  bytes[EVENT_TYPE_OFFSET] = type;
  int4store(bytes.data() + SERVER_ID_OFFSET, 1);
  bytes.insert(bytes.end(), body.begin(), body.end());
  int4store(bytes.data() + EVENT_LEN_OFFSET,
            static_cast<uint32_t>(bytes.size()));
  int4store(bytes.data() + LOG_POS_OFFSET, 0);
  return bytes;
}

/// An Xid event without checksum, xid = kXid.
constexpr uint64_t kXid = 0x1122334455667788ULL;
std::vector<unsigned char> xid_bytes() {
  std::vector<unsigned char> b(8);
  int8store(b.data(), kXid);
  return raw_event(mysql::binlog::event::XID_EVENT, b);
}

/// A Query event written by the server serializer, without checksum.
std::vector<unsigned char> query_event_bytes(THD *thd,
                                             const std::string &query) {
  Query_log_event qe(thd, query.c_str(), query.length(),
                     /*using_trans=*/true, /*immediate=*/false,
                     /*suppress_use=*/false, /*error=*/0);
  qe.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;
  StringBuffer_ostream<4096> os;
  EXPECT_FALSE(qe.write(&os));
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

/// A Table_map event for test.t1 (INT, VARCHAR(64)) with 9 bytes of optional
/// metadata (SIGNEDNESS and COLUMN_NAME), as an 8.x source writes it.
constexpr unsigned kTableMapOptionalMetadataLen = 9;
std::vector<unsigned char> table_map_event_bytes() {
  std::vector<unsigned char> b = {
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00,  // table id = 1
      0x01, 0x00,                          // flags
      0x04, 't',  'e',  's',  't',  0x00,  // db
      0x02, 't',  '1',  0x00,              // table
      0x02,                                // colcnt
      0x03, 0x0f,                          // LONG, VARCHAR
      0x02, 0x40, 0x00,                    // metadata: varchar max 64
      0x02,                                // null bits (col 2 nullable)
      0x01, 0x01, 0x00,                    // opt: SIGNEDNESS len 1
      0x04, 0x04, 0x01, 'a',  0x01, 'b'};  // opt: COLUMN_NAME len 4
  return raw_event(mysql::binlog::event::TABLE_MAP_EVENT, b);
}

/// The row images of write_rows_event_bytes(): two rows (INT, VARCHAR).
const std::vector<unsigned char> kRowImages = {
    // row 1: null bits, int 1, varchar "hello"
    0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o',
    // row 2: null bits, int 2, varchar "world"
    0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 'w', 'o', 'r', 'l', 'd'};

/// A Write_rows v2 event for table id 1 with the rows in kRowImages.
std::vector<unsigned char> write_rows_event_bytes() {
  // Table id 1, flags STMT_END_F, var header length 2 (itself), width 2,
  // columns bitmap 0b11.
  std::vector<unsigned char> b = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x01, 0x00, 0x02, 0x00, 0x02, 0x03};
  b.insert(b.end(), kRowImages.begin(), kRowImages.end());
  return raw_event(mysql::binlog::event::WRITE_ROWS_EVENT, b);
}

/// Access to the protected Rows_event::row (the decoded row images).
struct Rows_row_peek : mysql::binlog::event::Rows_event {
  static std::vector<uint8_t> mysql::binlog::event::Rows_event::*row_ptr() {
    return &Rows_row_peek::row;
  }
};

/// Imr_runtime::append_event() for an event in a byte vector.
bool append(Imr_runtime &imr, const std::vector<unsigned char> &bytes,
            bool is_terminal) {
  return imr.append_event(reinterpret_cast<const char *>(bytes.data()),
                          bytes.size(), is_terminal);
}
}  // namespace

// A transaction gets a different FDE object from the receiver's, equal to it
// in every field that decoding and applying an event read. A later in-place
// change of the receiver's object does not reach the copy.
TEST(ImrFdeSnapshotTest, TransactionGetsCopyOfReceiverFde) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);

  // Non-default values: a checksum-free source of another version, which
  // knows one event type less.
  Format_description_log_event rfde;
  rfde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;
  strcpy(rfde.server_version, "8.0.36-imr-snapshot");
  rfde.calc_server_version_split();
  rfde.post_header_len.resize(rfde.post_header_len.size() - 1);
  rfde.number_of_event_types =
      static_cast<uint8_t>(rfde.post_header_len.size());

  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(rfde);
  ASSERT_FALSE(imr.open_transaction(rfde, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));

  ASSERT_FALSE(imr.attach_role(kApplier));
  Transaction_envelope *env = imr.queue()->dispatch_next();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  const std::shared_ptr<Format_description_log_event> snap =
      env->payload()->fde();
  ASSERT_NE(snap, nullptr);

  EXPECT_NE(snap.get(), &rfde) << "must not be the receiver's object";
  EXPECT_EQ(snap->common_footer->checksum_alg, BINLOG_CHECKSUM_ALG_OFF);
  EXPECT_EQ(snap->number_of_event_types, rfde.number_of_event_types);
  EXPECT_EQ(snap->post_header_len, rfde.post_header_len);
  EXPECT_EQ(snap->common_header_len, rfde.common_header_len);
  EXPECT_EQ(snap->binlog_version, rfde.binlog_version);
  EXPECT_STREQ(snap->server_version, "8.0.36-imr-snapshot");
  EXPECT_EQ(memcmp(snap->server_version_split, rfde.server_version_split,
                   sizeof(snap->server_version_split)),
            0);
  EXPECT_EQ(snap->get_product_version(), rfde.get_product_version());
  EXPECT_EQ(snap->get_product_version(), (8u * 256 + 0) * 256 + 36);
  // The copy's Log_event pointers refer to its own header and footer.
  EXPECT_EQ(snap->common_footer, snap->footer());
  EXPECT_EQ(snap->common_header, snap->header());

  // The state Format_description_log_event::write() leaves the receiver's
  // object in while it writes it: the copy keeps its own value.
  rfde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_CRC32;
  EXPECT_EQ(snap->common_footer->checksum_alg, BINLOG_CHECKSUM_ALG_OFF);
  rfde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  imr.queue()->reset();
}

// Transactions opened between two set_receiver_fde() calls share one
// snapshot. Each set_receiver_fde() takes a new copy, even of the same object,
// as the receiver does for each source FDE event. A snapshot is never the
// receiver's object.
TEST(ImrFdeSnapshotTest, SnapshotReusedUntilNextSetReceiverFde) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  Format_description_log_event f1;
  Format_description_log_event f2;

  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(f1);
  ASSERT_FALSE(imr.open_transaction(f1, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));
  ASSERT_FALSE(imr.open_transaction(f1, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));
  imr.set_receiver_fde(f2);
  ASSERT_FALSE(imr.open_transaction(f2, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));
  imr.set_receiver_fde(f2);
  ASSERT_FALSE(imr.open_transaction(f2, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));

  ASSERT_FALSE(imr.attach_role(kApplier));
  std::vector<std::shared_ptr<Format_description_log_event>> snaps;
  for (int i = 0; i < 4; ++i) {
    Transaction_envelope *env = imr.queue()->dispatch_next();
    ASSERT_NE(env, nullptr);
    ASSERT_NE(env->payload(), nullptr);
    snaps.push_back(env->payload()->fde());
  }

  EXPECT_EQ(snaps[0].get(), snaps[1].get())
      << "no set_receiver_fde() between them: the snapshot is reused";
  EXPECT_NE(snaps[2].get(), snaps[1].get())
      << "set_receiver_fde() takes a new snapshot";
  EXPECT_NE(snaps[3].get(), snaps[2].get())
      << "set_receiver_fde() of the same object takes a new snapshot";
  for (const std::shared_ptr<Format_description_log_event> &snap : snaps) {
    EXPECT_NE(snap.get(), &f1);
    EXPECT_NE(snap.get(), &f2);
  }

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  imr.queue()->reset();
}

// While attached, the runtime holds the snapshot; detach_role(RECEIVER)
// releases it. A queued transaction keeps its copy until the transaction is
// gone.
TEST(ImrFdeSnapshotTest, ReceiverDetachReleasesFdeSnapshot) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  Format_description_log_event f1;

  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(f1);
  ASSERT_FALSE(imr.open_transaction(f1, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));

  ASSERT_FALSE(imr.attach_role(kApplier));
  Transaction_envelope *env = imr.queue()->dispatch_next();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  const std::weak_ptr<Format_description_log_event> snap =
      env->payload()->fde();
  // The runtime's reference and the transaction's (payload and byte source).
  const long held = snap.use_count();

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_EQ(snap.use_count(), held - 1) << "the receiver detach releases it";
  EXPECT_FALSE(snap.expired()) << "the queued transaction keeps its copy";

  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  imr.queue()->reset();
  EXPECT_TRUE(snap.expired()) << "nothing else holds the copy";
}

#ifndef NDEBUG
// open_transaction() asserts that set_receiver_fde() ran first.
TEST(ImrFdeSnapshotDeathTest, OpenTransactionWithoutSnapshotAsserts) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  Format_description_log_event fde;

  ASSERT_FALSE(imr.attach_role(kReceiver));
  EXPECT_DEATH_IF_SUPPORTED(imr.open_transaction(fde, kTrxLength),
                            ".*Assertion .*m_fde_snapshot != nullptr.*");

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  imr.queue()->reset();
}

// open_transaction() asserts that the receiver's decoding fields did not change
// in place since set_receiver_fde().
TEST(ImrFdeSnapshotDeathTest, InPlaceChangeOfReceiverFdeAsserts) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  Format_description_log_event fde;
  fde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;

  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(fde);
  fde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_CRC32;
  EXPECT_DEATH_IF_SUPPORTED(imr.open_transaction(fde, kTrxLength),
                            ".*Assertion .*same_decoding_fields.*");
  fde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  imr.queue()->reset();
}
#else
// Release builds: a transaction opened before any set_receiver_fde() gets a
// copy of the receiver's FDE, which later transactions reuse.
TEST(ImrFdeSnapshotTest, OpenTransactionWithoutSnapshotCopiesReceiverFde) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  Format_description_log_event rfde;
  rfde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;
  strcpy(rfde.server_version, "8.0.36-imr-fallback");
  rfde.calc_server_version_split();

  ASSERT_FALSE(imr.attach_role(kReceiver));
  ASSERT_FALSE(imr.open_transaction(rfde, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));
  ASSERT_FALSE(imr.open_transaction(rfde, kTrxLength));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));

  ASSERT_FALSE(imr.attach_role(kApplier));
  std::vector<std::shared_ptr<Format_description_log_event>> snaps;
  for (int i = 0; i < 2; ++i) {
    Transaction_envelope *env = imr.queue()->dispatch_next();
    ASSERT_NE(env, nullptr);
    ASSERT_NE(env->payload(), nullptr);
    snaps.push_back(env->payload()->fde());
  }

  ASSERT_NE(snaps[0], nullptr);
  EXPECT_EQ(snaps[0].get(), snaps[1].get()) << "the fallback copy is reused";
  EXPECT_NE(snaps[0].get(), &rfde) << "must not be the receiver's object";
  EXPECT_EQ(snaps[0]->common_footer->checksum_alg, BINLOG_CHECKSUM_ALG_OFF);
  EXPECT_EQ(snaps[0]->post_header_len, rfde.post_header_len);
  EXPECT_EQ(snaps[0]->common_header_len, rfde.common_header_len);
  EXPECT_EQ(snaps[0]->binlog_version, rfde.binlog_version);
  EXPECT_STREQ(snaps[0]->server_version, "8.0.36-imr-fallback");

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  imr.queue()->reset();
}
#endif

/// For the cases that build real Query events, which need a THD.
class ImrFdeSnapshotDecodeTest : public ::testing::Test {
 protected:
  void SetUp() override { m_srv.SetUp(); }
  void TearDown() override { m_srv.TearDown(); }
  THD *thd() const { return m_srv.thd(); }
  my_testing::Server_initializer m_srv;
};

// Events from a checksum-free source decode correctly while the receiver's
// FDE is in the state Format_description_log_event::write() leaves it in
// during the write (checksum algorithm CRC32 instead of OFF). Decoding with
// the receiver's object in that state cuts 4 bytes off each event: BEGIN
// becomes "B", rows are cut and the Xid event fails to decode.
TEST_F(ImrFdeSnapshotDecodeTest,
       QueuedEventsDecodeWhileReceiverFdeIsBeingWritten) {
  Imr_runtime imr(make_config(), "", /*memory_resource=*/nullptr);
  Detach_guard guard(imr);
  Format_description_log_event rfde;
  rfde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;

  ASSERT_FALSE(imr.attach_role(kReceiver));
  imr.set_receiver_fde(rfde);
  ASSERT_FALSE(imr.open_transaction(rfde, kTrxLength));
  ASSERT_FALSE(append(imr, query_event_bytes(thd(), "BEGIN"), false));
  ASSERT_FALSE(append(imr, table_map_event_bytes(), false));
  ASSERT_FALSE(append(imr, write_rows_event_bytes(), false));
  ASSERT_FALSE(append(imr, xid_bytes(), /*is_terminal=*/true));

  // The write window on the receiver's object, while the workers decode.
  rfde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_CRC32;

  ASSERT_FALSE(imr.attach_role(kApplier));
  Transaction_envelope *env = imr.queue()->dispatch_next();
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  {
    std::shared_ptr<Fetchable_transaction> fetchable =
        env->payload()->fetchable();
    ASSERT_NE(fetchable, nullptr);
    // Check every event that decodes, so a failure shows each damaged one.
    std::vector<std::shared_ptr<Log_event>> events;
    while (fetchable->wait_next()) {
      auto managed = fetchable->fetch_next();
      if (!managed.has_value()) {
        ADD_FAILURE() << fetchable->get_fetch_error_msg();
        break;
      }
      events.push_back(managed->get_event());
    }
    EXPECT_FALSE(fetchable->is_fetching_error())
        << fetchable->get_fetch_error_msg();
    EXPECT_EQ(events.size(), 4u);
    const auto event_of_type = [&events](std::size_t index,
                                         Log_event_type type) -> Log_event * {
      if (index >= events.size()) return nullptr;
      EXPECT_EQ(events[index]->get_type_code(), type) << "event " << index;
      return events[index]->get_type_code() == type ? events[index].get()
                                                    : nullptr;
    };

    if (auto *ev = event_of_type(0, mysql::binlog::event::QUERY_EVENT)) {
      auto *query = static_cast<Query_log_event *>(ev);
      EXPECT_EQ(query->q_len, 5u);
      EXPECT_EQ(std::string(query->query, query->q_len), "BEGIN");
    }
    if (auto *ev = event_of_type(1, mysql::binlog::event::TABLE_MAP_EVENT)) {
      EXPECT_EQ(static_cast<Table_map_log_event *>(ev)->m_optional_metadata_len,
                kTableMapOptionalMetadataLen);
    }
    if (auto *ev = event_of_type(2, mysql::binlog::event::WRITE_ROWS_EVENT)) {
      mysql::binlog::event::Rows_event &rows =
          *static_cast<Write_rows_log_event *>(ev);
      const std::vector<uint8_t> &row = rows.*Rows_row_peek::row_ptr();
      // The decoder appends one trailing 0 to the row images.
      EXPECT_EQ(row.size(), kRowImages.size() + 1);
      EXPECT_TRUE(row.size() == kRowImages.size() + 1 &&
                  std::equal(kRowImages.begin(), kRowImages.end(), row.begin()))
          << "the row images must be intact";
    }
    if (auto *ev = event_of_type(3, mysql::binlog::event::XID_EVENT)) {
      EXPECT_EQ(static_cast<Xid_log_event *>(ev)->xid, kXid);
    }
  }
  rfde.common_footer->checksum_alg = BINLOG_CHECKSUM_ALG_OFF;

  EXPECT_TRUE(imr.detach_role(kReceiver, nullptr));
  EXPECT_TRUE(imr.detach_role(kApplier, nullptr));
  imr.queue()->reset();
}

}  // namespace mysql::csa::unittests
