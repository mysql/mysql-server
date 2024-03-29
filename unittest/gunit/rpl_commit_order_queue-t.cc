/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <array>
#include <chrono>
#include <thread>
#include <vector>

#include "sql/changestreams/apply/commit_order_queue.h"
#include "sql/memory/aligned_atomic.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace rpl {
namespace unittests {

class Rpl_commit_order_queue_test : public ::testing::Test {
 protected:
  Rpl_commit_order_queue_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}

  std::atomic<bool> m_go{false};
  std::atomic<size_t> m_count{0};
};

TEST_F(Rpl_commit_order_queue_test, Simulate_mts) {
  constexpr cs::apply::Commit_order_queue::value_type total_workers{32};
  constexpr size_t total_transactions{25000};
  cs::apply::Commit_order_queue scheduled{total_workers};
  cs::apply::Commit_order_queue free{total_workers};
  std::array<std::atomic_flag, total_workers> context;
  std::atomic<cs::apply::Commit_order_queue::value_type> transactions{
      total_transactions};

  for (auto n_wrk = 0; n_wrk != total_workers; ++n_wrk) {
    context[n_wrk].test_and_set();
    free.push(n_wrk);
  }

  std::vector<std::thread> threads;
  for (auto n_wrk = 0; n_wrk != total_workers; ++n_wrk) {
    threads.emplace_back(
        [&](cs::apply::Commit_order_queue::value_type worker_id) -> void {
          assert(worker_id != cs::apply::Commit_order_queue::NO_WORKER);

          for (; transactions > 0;) {
            if (scheduled[worker_id].m_stage ==
                cs::apply::Commit_order_queue::enum_worker_stage::FINISHED) {
              scheduled[worker_id].m_stage =
                  cs::apply::Commit_order_queue::enum_worker_stage::REGISTERED;

              for (; context[worker_id]
                         .test_and_set();) {  // Wait for coordinator to
                                              // schedule the worker in
                std::this_thread::yield();
              }
              if (transactions <= 0) break;
            }

            // Worker would apply the transaction here

            // Worker enters the wait on the commit order
            scheduled[worker_id].m_stage = cs::apply::Commit_order_queue::
                enum_worker_stage::FINISHED_APPLYING;

            if (worker_id == scheduled.front()) {  // Is the head of the queue
              scheduled[worker_id].m_stage =
                  cs::apply::Commit_order_queue::enum_worker_stage::WAITED;

              auto this_worker{cs::apply::Commit_order_queue::NO_WORKER};
              auto this_seq_nr{0};
              std::tie(this_worker, this_seq_nr) =
                  scheduled.pop();                 // Pops the head of the
                                                   // queue and gets own
                                                   // commit sequence
                                                   // number
              auto next_seq_nr = this_seq_nr + 1;  // Calculates which is
                                                   // the next sequence
                                                   // number
              EXPECT_EQ(worker_id, this_worker);
              EXPECT_NE(this_seq_nr, 0);  // NO_SEQUENCE_NR
              EXPECT_NE(this_seq_nr, 1);  // SEQUENCE_NR_FROZEN

              auto next_worker =
                  scheduled.front();  // Gets the next worker and checks if it
                                      // needs to release it
              if (next_worker != cs::apply::Commit_order_queue::NO_WORKER &&
                  (scheduled[next_worker].m_stage ==
                       cs::apply::Commit_order_queue::enum_worker_stage::
                           FINISHED_APPLYING ||
                   scheduled[next_worker].m_stage ==
                       cs::apply::Commit_order_queue::enum_worker_stage::
                           REQUESTED_GRANT) &&
                  scheduled[next_worker].freeze_commit_sequence_nr(
                      next_seq_nr)) {
                context[next_worker].clear();  // Releases the next worker
                scheduled[next_worker].unfreeze_commit_sequence_nr(next_seq_nr);
              }

              --transactions;  // One less transaction in the workload

              scheduled[worker_id].m_stage =
                  cs::apply::Commit_order_queue::enum_worker_stage::FINISHED;
              context[worker_id].test_and_set();
              free.push(worker_id);  // Finishes the work and pushes itself
                                     // into the free worker queue

            } else {
              scheduled[worker_id].m_stage =
                  cs::apply::Commit_order_queue::enum_worker_stage::
                      REQUESTED_GRANT;  // Starts the commit order wait

              for (; context[worker_id].test_and_set();) {  // Wait for previous
                                                            // worker to release
                std::this_thread::yield();
              }
            }
          }
          for (cs::apply::Commit_order_queue::value_type sib = 0;
               sib != total_workers; ++sib) {  // Work is finished
            context[sib].clear();              // Release all waiting workers
          }
        },
        total_workers - n_wrk - 1);
  }

  std::thread coordinator{[&]() -> void {
    for (size_t n_trx = 0; n_trx != total_transactions; ++n_trx) {
      auto w{cs::apply::Commit_order_queue::NO_WORKER};
      for (std::tie(w, std::ignore) = free.pop();
           w == cs::apply::Commit_order_queue::NO_WORKER;
           std::tie(w, std::ignore) =
               free.pop())  // Get a free worker to schedule
        std::this_thread::yield();
      scheduled.push(w);   // Schedule the worker
      context[w].clear();  // Signal the worker
    }
  }};

  coordinator.join();
  for (auto n_wrk = 0; n_wrk != total_workers; ++n_wrk) {
    threads[n_wrk].join();
  }
}

TEST_F(Rpl_commit_order_queue_test, Pushing_while_poping_test) {
  constexpr cs::apply::Commit_order_queue::value_type total_workers{32};
  cs::apply::Commit_order_queue q{total_workers};
  cs::apply::Commit_order_queue f{total_workers};

  std::vector<std::thread> threads;
  for (auto idx = 0; idx != total_workers; ++idx) {
    threads.emplace_back(
        [&](size_t) -> void {
          for (; true;) {
            auto v{cs::apply::Commit_order_queue::NO_WORKER};
            std::tie(v, std::ignore) = q.pop();
            if (v != cs::apply::Commit_order_queue::NO_WORKER) {
              ++m_count;
              f.push(v);
              break;
            }
            std::this_thread::yield();
          }
        },
        idx);
  }

  std::thread producer{[&]() -> void {
    for (cs::apply::Commit_order_queue::value_type idx = 0;
         idx != total_workers; ++idx) {
      q.push(total_workers - idx - 1);
    }
  }};

  producer.join();
  for (auto idx = 0; idx != total_workers; ++idx) {
    threads[idx].join();
  }

  EXPECT_EQ(q.is_empty(), true);
  EXPECT_EQ(m_count.load(), total_workers);

  EXPECT_EQ(q.to_string(), "EOF");
  std::ostringstream oss;
  for (int k = total_workers - 1; k != -1; --k) {
    oss << k << ", " << std::flush;
  }
  oss << "EOF" << std::flush;
  EXPECT_EQ(f.to_string().length(), oss.str().length());

  std::map<cs::apply::Commit_order_queue::value_type,
           cs::apply::Commit_order_queue::value_type>
      dup;
  for (auto v : f) {
    if (v == nullptr) continue;
    bool inserted{false};
    std::tie(std::ignore, inserted) =
        dup.insert(std::make_pair(v->m_worker_id, v->m_worker_id));
    EXPECT_EQ(inserted, true);
  }
  EXPECT_EQ(dup.size(), total_workers);

  EXPECT_EQ(f.is_empty(), false);
  f.clear();
  EXPECT_EQ(f.is_empty(), true);
}

TEST_F(Rpl_commit_order_queue_test, Pushing_then_poping_test) {
  constexpr cs::apply::Commit_order_queue::value_type total_workers{32};
  cs::apply::Commit_order_queue q{total_workers};
  cs::apply::Commit_order_queue f{total_workers};
  m_go.store(false);

  std::vector<std::thread> threads;
  for (auto idx = 0; idx != total_workers; ++idx) {
    threads.emplace_back(
        [&](size_t) -> void {
          for (; true;) {
            if (m_go.load()) break;
            std::this_thread::yield();
          }

          for (; true;) {
            auto v{cs::apply::Commit_order_queue::NO_WORKER};
            std::tie(v, std::ignore) = q.pop();
            if (v != cs::apply::Commit_order_queue::NO_WORKER) {
              ++m_count;
              f.push(v);
              break;
            }
            std::this_thread::yield();
          }
        },
        idx);
  }

  std::thread producer{[&]() -> void {
    for (cs::apply::Commit_order_queue::value_type idx = 0;
         idx != total_workers; ++idx) {
      q.push(total_workers - idx - 1);
    }
    m_go.store(true);
  }};

  producer.join();
  for (auto idx = 0; idx != total_workers; ++idx) {
    threads[idx].join();
  }

  EXPECT_EQ(q.is_empty(), true);
  EXPECT_EQ(
      q.get_state(),
      cs::apply::Commit_order_queue::queue_type::enum_queue_state::SUCCESS);
  EXPECT_EQ(m_count.load(), total_workers);

  EXPECT_EQ(q.to_string(), "EOF");
  std::ostringstream oss;
  for (int k = total_workers - 1; k != -1; --k) {
    oss << k << ", " << std::flush;
  }
  oss << "EOF" << std::flush;
  EXPECT_EQ(f.to_string().length(), oss.str().length());

  auto it = f.begin();
  auto it1{it};
  auto it2 = it1++;
  auto it3 = std::move(it2);
  auto it4{std::move(it3)};
  it2 = it4;
  it3 = std::move(it1);
  EXPECT_NE(*it4, nullptr);
  EXPECT_EQ(it4->m_worker_id, it2->m_worker_id);

  std::map<cs::apply::Commit_order_queue::value_type,
           cs::apply::Commit_order_queue::value_type>
      dup;
  for (auto v : f) {
    if (v == nullptr) continue;
    bool inserted{false};
    std::tie(std::ignore, inserted) =
        dup.insert(std::make_pair(v->m_worker_id, v->m_worker_id));
    EXPECT_EQ(inserted, true);
  }
  EXPECT_EQ(dup.size(), total_workers);

  EXPECT_EQ(f.is_empty(), false);
  f.clear();
  EXPECT_EQ(f.is_empty(), true);
}

}  // namespace unittests
}  // namespace rpl
