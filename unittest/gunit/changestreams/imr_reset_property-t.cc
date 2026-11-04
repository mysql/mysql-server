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
/// Property test for reset completeness.
///
/// **Property: Reset clears every element of the volatile receiver state.**
///
/// The Volatile_Receiver_State discarded at a full stop has two parts: the
/// queue contents/cursors/reserved bytes, and the receiver GTID state
/// (Retrieved_Gtid_Set + TSID map, boundary parser). Imr_runtime::reset()
/// clears both, but the GTID half is reached through a Master_info, which is
/// not constructible in a unit harness (no gunit test builds a running server).
/// This test therefore validates the queue half -- discarding the
/// Transaction_Queue contents -- directly
/// against Trx_envelope_queue::reset(), which is exactly what
/// Imr_runtime::reset() calls to empty the queue. The Retrieved_Gtid_Set /
/// TSID-map / boundary-parser clearing is covered by the rpl_csa_imr_* MTR
/// integration tests.
///
/// Over queues seeded with a random mix of committed, truncated, in-flight, and
/// undispatched envelopes, this asserts that after reset() the queue is empty,
/// all three cursors are zero, and reserved bytes are zero.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/log_event.h"  // Format_description_log_event
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {
/// Bounds large enough that every enqueue takes the non-blocking memory path.
constexpr std::size_t kMemoryLimit = std::size_t{1} << 30;
constexpr std::size_t kSpillThreshold = std::size_t{1} << 20;

/// enqueue() only asserts the FDE is non-null; these tests never build a real
/// byte source.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

constexpr int kIterations = 200;
}  // namespace

// After reset(), regardless of the mix of committed / truncated / in-flight /
// undispatched envelopes the queue held, it is empty with all cursors and
// reserved bytes at zero, and the role stop-flags are untouched.
TEST(ImrResetPropertyTest, ResetClearsQueueContentsFromAnyState) {
  std::mt19937 rng(20260916u);
  std::uniform_int_distribution<int> count_dist(0, 8);
  std::uniform_int_distribution<std::size_t> len_dist(1, 4096);
  std::uniform_int_distribution<int> pick_dist(0, 2);

  for (int iter = 0; iter < kIterations; ++iter) {
    Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                             test_memory_resource(kMemoryLimit));
    // arm the receiver and the applier for the session
    queue.resume(Trx_envelope_queue::Scope::RECEIVER);
    queue.resume(Trx_envelope_queue::Scope::APPLIER);

    const int n = count_dist(rng);
    std::vector<Transaction_envelope *> envs;
    envs.reserve(n);
    for (int i = 0; i < n; ++i) {
      Transaction_envelope *e =
          queue.enqueue(len_dist(rng), /*is_trx=*/true, make_fde());
      ASSERT_NE(e, nullptr);
      envs.push_back(e);
    }

    // Dispatch a random prefix (dispatch_next() is strictly FIFO), giving each
    // dispatched envelope a random terminal state, and truncate some of the
    // still-undispatched ones (the receiver-truncated-before-dispatch case).
    const int dispatched = count_dist(rng) % (n + 1);
    for (int i = 0; i < n; ++i) {
      if (i < dispatched) {
        Transaction_envelope *d = queue.dispatch_next();
        ASSERT_EQ(d, envs[i]);
        switch (pick_dist(rng)) {
          case 0:
            ASSERT_FALSE(d->commit());  // committed
            break;
          case 1:
            d->set_truncated();  // dispatched then truncated
            break;
          default:
            break;  // left in-flight
        }
      } else if (pick_dist(rng) == 0) {
        envs[i]->set_truncated();  // undispatched, truncated
      }
    }

    // Optionally advance the commit low-water mark over the finalized head.
    if (pick_dist(rng) == 0) (void)queue.sweep_committed();

    // The operation under test.
    queue.reset();

    EXPECT_EQ(queue.queue_length(), 0u) << "iteration " << iter;
    EXPECT_EQ(queue.bytes_used(), 0u) << "iteration " << iter;
    EXPECT_EQ(queue.insert_seqno(), 0u) << "iteration " << iter;
    EXPECT_EQ(queue.dispatch_seqno(), 0u) << "iteration " << iter;
    EXPECT_EQ(queue.commit_seqno(), 0u) << "iteration " << iter;
    // reset() leaves the role stop-flags alone: both were armed, still armed.
    // A 0-byte admission fails only on a receiver stop.
    EXPECT_FALSE(queue.acquire_admission(0, Envelope_path::MEMORY))
        << "iteration " << iter;
    EXPECT_FALSE(queue.applier_stop_flag()->load()) << "iteration " << iter;

    // The queue is empty here, so its destructor's empty-queue invariant holds.
  }
}

}  // namespace mysql::csa::unittests
