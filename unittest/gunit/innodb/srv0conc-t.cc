/*****************************************************************************

Copyright (c) 2020, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include <gtest/gtest.h>
#include <random>
#include <thread>
#include "row0mysql.h"
#include "sql_class.h"
#include "srv0conc.h"

/** RAII wrapper for the mock prebuilt object, avoids memory management stuff in
 * actual tests. */
class prebuilt_guard {
 public:
  prebuilt_guard() : thd(false) {
    ptr = static_cast<row_prebuilt_t *>(malloc(sizeof(*ptr)));
    ptr->trx = &trx;
    ptr->trx->mysql_thd = &thd;
  }
  ~prebuilt_guard() { free(ptr); }

 public:
  row_prebuilt_t *ptr;

 private:
  THD thd;
  trx_t trx{};
};

void run_threads(std::function<void()> thread, int count) {
  std::vector<std::thread> threads;

  for (int i = 0; i < count; ++i) {
    threads.push_back(std::thread(thread));
  }

  for (auto &t : threads) {
    t.join();
  }
}

/** This is a user thread simulation which performs an enter/sleep/exit multiple
 * times. The thread will sleep for a random duration in [1-MaxSleep]
 * microseconds. It will repeat this multiple times, controlled by Iterations
 */
template <int Iterations, int MaxSleep>
void user_thread_simulation() {
  prebuilt_guard prebuilt_guard;
  auto prebuilt = prebuilt_guard.ptr;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distr(1, MaxSleep);

  for (int i = 0; i < Iterations; ++i) {
    srv_conc_enter_innodb(prebuilt);
    std::this_thread::sleep_for(std::chrono::microseconds(distr(gen)));
    srv_conc_force_exit_innodb(prebuilt->trx);
  }
}

class srv0conc : public ::testing::Test {
 protected:
  static void TearDownTestCase() { srv_thread_concurrency = 0; }
};

// TODO(tdidriks) DISABLED some tests until this is fixed:
// Bug #32855166 BROKEN INNODB UNIT TESTS
TEST_F(srv0conc, DISABLED_no_concurrency_limit) {
  const int THREADS = std::thread::hardware_concurrency();
  run_threads(user_thread_simulation<100, 100>, THREADS);
  EXPECT_EQ(srv_conc_get_waiting_threads(), 0);
  EXPECT_EQ(srv_conc_get_active_threads(), 0);
}

TEST_F(srv0conc, DISABLED_concurrency_limit_equals_hw_threads) {
  const int THREADS = std::thread::hardware_concurrency();
  srv_thread_concurrency = THREADS;
  run_threads(user_thread_simulation<100, 100>, THREADS);
  EXPECT_EQ(srv_conc_get_waiting_threads(), 0);
  EXPECT_EQ(srv_conc_get_active_threads(), 0);
}

TEST_F(srv0conc, DISABLED_concurrency_limit_half_hw_threads) {
  const int THREADS = std::thread::hardware_concurrency();
  srv_thread_concurrency = THREADS / 2;
  run_threads(user_thread_simulation<100, 100>, THREADS);
  EXPECT_EQ(srv_conc_get_waiting_threads(), 0);
  EXPECT_EQ(srv_conc_get_active_threads(), 0);
}

TEST_F(srv0conc, DISABLED_concurrency_limit_2) {
  const int THREADS = std::thread::hardware_concurrency();
  srv_thread_concurrency = 2;
  run_threads(user_thread_simulation<100, 100>, THREADS);
  EXPECT_EQ(srv_conc_get_waiting_threads(), 0);
  EXPECT_EQ(srv_conc_get_active_threads(), 0);
}

TEST_F(srv0conc, DISABLED_concurrency_limit_1) {
  const int THREADS = std::thread::hardware_concurrency();
  srv_thread_concurrency = 1;
  run_threads(user_thread_simulation<100, 100>, THREADS);
  EXPECT_EQ(srv_conc_get_waiting_threads(), 0);
  EXPECT_EQ(srv_conc_get_active_threads(), 0);
}

/* This test case simulates the situation where the transaction is interrupted
while waiting for n_active threads to drop below the concurrency limit. */
TEST_F(srv0conc, DISABLED_trx_interrupted) {
  srv_thread_concurrency = 1;
  prebuilt_guard active_prebuilt_guard;
  prebuilt_guard interrupted_prebuilt_guard;
  auto active_prebuilt = active_prebuilt_guard.ptr;
  auto interrupted_prebuilt = interrupted_prebuilt_guard.ptr;

  /* Active trx enters InnoDB without issues. */
  srv_conc_enter_innodb(active_prebuilt);
  EXPECT_TRUE(active_prebuilt->trx->declared_to_be_inside_innodb);
  EXPECT_EQ(srv_conc_get_waiting_threads(), 0);
  EXPECT_EQ(srv_conc_get_active_threads(), 1);

  /* Set up the mock trx in such way it simulates "interrupted" state */
  interrupted_prebuilt->trx->mysql_thd->killed =
      THD::killed_state::KILL_CONNECTION;

  /* Thread fails to enter InnoDB, number of waiters must remain 0, and number
  of active is 1 */
  srv_conc_enter_innodb(interrupted_prebuilt);
  EXPECT_TRUE(active_prebuilt->trx->declared_to_be_inside_innodb);
  EXPECT_FALSE(interrupted_prebuilt->trx->declared_to_be_inside_innodb);
  EXPECT_EQ(srv_conc_get_waiting_threads(), 0);
  EXPECT_EQ(srv_conc_get_active_threads(), 1);
}
