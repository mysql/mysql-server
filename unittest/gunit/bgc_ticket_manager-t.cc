/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "sql/binlog/group_commit/bgc_ticket_manager.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <random>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace binlog {
namespace unittests {

using ticket_t = binlog::BgcTicket::ValueType;

/**
  Test for Bgc_ticket_manager API.

  The purpose of the test is for each created thread to be assigned to a
  given ticket, wait for such ticket to be the front ticket and have it's
  processing window active, add itself to the ticket's processed sessions
  and either end the thread task or finish the current active processing
  window. If a thread does one or the other, depends on a sequential number
  that is passed on to each thread. Logic is as follows:

  1. Creates set of threads and assigns to each a sequential number,
     `n_thread`.

  2. If the remainder of the integer division of `n_thread` by 10 is not zero,
     the thread will:
     1. Assign it self to the current back ticket.
     2. Wait for it's own ticket to be active as front ticket.
     3. Add itself to the front ticket processed sessions.

  3. If the remainder of the integer division of `n_thread` by 10 is zero, the
     thread will:
     1. Wait for a random number (between 25 and 200) of micro-seconds, in
        order to allow for different sizes in the active front ticket
        processing window.
     2. Atomically, assign it self to the current back ticket, close the
        current back ticket to assignments and create a new back ticket.
     3. Wait for it's own ticket to be active as front ticket.
     4. Add itself to the front ticket processed sessions.
     5. Finish the front ticket processing window.
     6. Notify all waiting threads that a window has been closed.

*/
class Bgc_ticket_manager_test : public ::testing::Test {
 protected:
  Bgc_ticket_manager_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Bgc_ticket_manager_test, Several_tickets_test) {
  size_t total_threads{300};
  std::vector<std::thread> threads;
  std::mutex mtx;
  std::condition_variable condition;
  std::random_device r_device;
  std::default_random_engine r_engine{r_device()};
  std::uniform_int_distribution<int> uniform_dist{25, 200};
  size_t thread_test_loops = 100;
  size_t max_ticket = (total_threads / 10) * thread_test_loops;
  std::vector<std::uint64_t> tickets;
  std::mutex mutex_tickets;

  auto &ticket_manager = binlog::Bgc_ticket_manager::instance();

  for (size_t idx = 0; idx != total_threads; ++idx) {
    threads.emplace_back(
        [&](size_t n_thread) -> void {
          size_t thread_cnt = 0;
          while (thread_cnt++ < thread_test_loops) {
            if (n_thread % 10 != 0) {
              auto this_thread_ticket =
                  ticket_manager.assign_session_to_ticket();
              {
                std::unique_lock lock{mtx};
                while (this_thread_ticket !=
                       ticket_manager.get_front_ticket()) {
                  condition.wait(lock);
                }
              }
              mutex_tickets.lock();
              tickets.push_back(this_thread_ticket.get());
              mutex_tickets.unlock();
              ticket_manager.add_processed_sessions_to_front_ticket(
                  1, this_thread_ticket);
            } else {
              std::this_thread::sleep_for(
                  std::chrono::duration<int, std::micro>{
                      uniform_dist(r_engine)});
              auto [this_thread_ticket, _] = ticket_manager.push_new_ticket(
                  binlog::BgcTmOptions::inc_session_count);

              while (this_thread_ticket != ticket_manager.get_front_ticket()) {
                std::this_thread::yield();
              }
              mutex_tickets.lock();
              tickets.push_back(this_thread_ticket.get());
              mutex_tickets.unlock();

              ticket_manager.add_processed_sessions_to_front_ticket(
                  1, this_thread_ticket);

              while (std::get<1>(ticket_manager.pop_front_ticket()) ==
                     this_thread_ticket) {
                std::this_thread::yield();
              }
              {
                std::unique_lock lock{mtx};
                condition.notify_all();
              }
            }
          }
        },
        idx + 1);
  }

  for (size_t idx = 0; idx != total_threads; ++idx) {
    threads[idx].join();
  }

  // additional coalescing
  // to ensure that m_back_ticket_sessions_count is always 0 (testing purposes)
  ticket_manager.coalesce();

  std::ostringstream expected;
  expected << "Bgc_ticket_manager (" << std::hex << &ticket_manager << std::dec
           << "):" << std::endl
           << " · m_back_ticket: " << (max_ticket + 2) << "/0" << std::endl
           << " · m_front_ticket: " << (max_ticket + 2) << "/0" << std::endl
           << " · m_coalesced_ticket: " << (max_ticket + 1) << "/0" << std::endl
           << " · m_back_ticket_sessions_count: 0" << std::endl
           << " · m_front_ticket_processed_sessions_count: 0" << std::endl
           << " · m_sessions_per_ticket: EOF" << std::flush;
  std::ostringstream observed;
  observed << ticket_manager << std::flush;
  EXPECT_EQ(observed.str(), expected.str());
  EXPECT_EQ(ticket_manager.to_string(), expected.str());
  EXPECT_EQ(tickets.size(), total_threads * thread_test_loops);
  EXPECT_EQ(std::is_sorted(tickets.begin(), tickets.end()), true);
}

}  // namespace unittests
}  // namespace binlog
