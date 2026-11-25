// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "mysql/concurrency/thread.h"
#include "sql/changestreams/apply/csa_worker_context.h"
#include "sql/changestreams/apply/service/transaction_conflict_manager.h"
#include "sql/debug_sync.h"
#include "sql/raii/sentry.h"
#include "sql/rpl_info_factory.h"
#include "sql/rpl_replica_commit_order_manager.h"
#include "sql/rpl_rli.h"
#include "unittest/gunit/test_utils.h"

namespace {

class CsaWorkerContextTest : public ::testing::Test {
 protected:
  void SetUp() override { m_initializer.SetUp(); }
  void TearDown() override { m_initializer.TearDown(); }

  my_testing::Server_initializer m_initializer;
};

TEST_F(CsaWorkerContextTest, NewTransactionStartsPreparing) {
  auto *thd = m_initializer.thd();
  cs::apply::Csa_worker_context context(1, 0, "channel", thd, 0, 10);
  context.set_applied();
  context.set_committing();

  // The session pool reuses this context for another transaction.
  context.update(2, 0, "channel", thd, 0, 10);
  EXPECT_FALSE(context.found_commit_order_deadlock());
  context.report_commit_order_deadlock();

  // During apply, record the conflict for prepare to handle. There is no
  // commit owner to notify and no detached transaction to rescue yet.
  EXPECT_TRUE(context.found_commit_order_deadlock());
  EXPECT_FALSE(context.is_rollback_requested());
  EXPECT_EQ(MDL_wait::WS_EMPTY, thd->mdl_context.m_wait.get_status());
}

TEST_F(CsaWorkerContextTest, RegistrationUpdatePreservesPreparedState) {
  auto *thd = m_initializer.thd();
  cs::apply::Csa_worker_context context(1, 0, "channel", thd, 0, 10);
  context.set_applied();

  // Selecting the commit worker does not start a new transaction.
  context.set_worker_id(1);
  context.set_committing();
  context.report_commit_order_deadlock();

  EXPECT_FALSE(context.is_rollback_requested());
  EXPECT_EQ(MDL_wait::VICTIM, thd->mdl_context.m_wait.get_status());
}

TEST_F(CsaWorkerContextTest, OwnCommitFailureDoesNotRequestRescue) {
  auto *thd = m_initializer.thd();
  cs::apply::Csa_worker_context context(1, 0, "channel", thd, 0, 10);
  context.set_applied();
  context.set_committing();
  context.report_commit_order_deadlock(true);

  EXPECT_TRUE(context.found_commit_order_deadlock());
  EXPECT_FALSE(context.is_rollback_requested());
  EXPECT_EQ(MDL_wait::WS_EMPTY, thd->mdl_context.m_wait.get_status());
}

class CsaWorkerContextRetryTest : public CsaWorkerContextTest,
                                  public ::testing::WithParamInterface<int> {};

TEST_P(CsaWorkerContextRetryTest, EligibilityChecksDoNotConsumeRetries) {
  auto *thd = m_initializer.thd();
  cs::apply::Csa_worker_context context(1, 0, "channel", thd, 0, GetParam());
  context.set_applied();
  context.set_committing();

  for (int retry = 0; retry < GetParam(); ++retry) {
    context.report_commit_order_deadlock(true);
    // COM and the job check the same failed attempt before accepting a retry.
    ASSERT_TRUE(context.can_be_retried(thd));
    ASSERT_TRUE(context.can_be_retried(thd));
    context.reset_commit_order_deadlock();
    context.set_retry_count(retry + 1);
  }

  context.report_commit_order_deadlock(true);
  EXPECT_FALSE(context.can_be_retried(thd));
}

INSTANTIATE_TEST_SUITE_P(RetryLimits, CsaWorkerContextRetryTest,
                         ::testing::Values(0, 1, 2));

#if defined(ENABLED_DEBUG_SYNC)
class CsaWorkerContextHandoffTest : public CsaWorkerContextTest {
 protected:
  void SetUp() override {
    opt_debug_sync_timeout = 30;
    ASSERT_EQ(0, debug_sync_init());
    CsaWorkerContextTest::SetUp();
  }

  void TearDown() override {
    CsaWorkerContextTest::TearDown();
    debug_sync_end();
    opt_debug_sync_timeout = 0;
  }
};

TEST_F(CsaWorkerContextHandoffTest, CommitOwnershipRejectsDelayedRescue) {
  auto *thd = m_initializer.thd();
  THD earlier_thd(false);
  cs::apply::Csa_worker_context earlier(1, 0, "channel", &earlier_thd, 0, 10);
  cs::apply::Csa_worker_context worker(2, 1, "channel", thd, 0, 10);
  Commit_order_manager commit_order(2);
  std::unique_ptr<Relay_log_info> rli(
      Rpl_info_factory::create_rli(INFO_REPOSITORY_DUMMY, false, ""));
  ASSERT_NE(nullptr, rli);

  auto &rescue = mysql::csa::Transaction_conflict_monitor::get(0);
  auto previous_rescue = std::move(rescue);
  rescue = std::make_unique<mysql::csa::Transaction_conflict_manager>();
  // Let an erroneous submission complete without running engine rollback.
  rescue->start();
  rescue->stop();
  const auto previous_system_thread = thd->system_thread;
  raii::Sentry cleanup([&] {
    rescue = std::move(previous_rescue);
    thd->system_thread = previous_system_thread;
    thd->rli_slave = nullptr;
    rli->info_thd = nullptr;
    rli->set_csa_worker_context(nullptr);
    rli->set_commit_order_manager(nullptr);
  });
  rli->info_thd = thd;
  rli->set_csa_worker_context(&worker);
  rli->set_commit_order_manager(&commit_order);
  thd->rli_slave = rli.get();
  thd->system_thread = SYSTEM_THREAD_SLAVE_WORKER;

  ASSERT_FALSE(commit_order.init_worker_context(0, &earlier_thd));
  commit_order.register_trx(&earlier);
  worker.set_applied();

  {
    mysql::concurrency::Thread reporter([&] {
      my_testing::Server_initializer initializer;
      initializer.SetUp();
      {
        my_testing::Mock_error_handler no_warnings(initializer.thd(), 0);
        const char pause[] =
            "csa_report_observed_prepared SIGNAL csa_report_paused "
            "WAIT_FOR csa_report_resume";
        EXPECT_FALSE(
            debug_sync_set_action(initializer.thd(), STRING_WITH_LEN(pause)));
        worker.report_commit_order_deadlock();
      }
      initializer.TearDown();
    });
    ASSERT_TRUE(reporter.joinable());
    // Release and join even if an assertion fails while the reporter waits.
    raii::Sentry resume_reporter([&] {
      const char resume[] = "now SIGNAL csa_report_resume";
      EXPECT_FALSE(debug_sync_set_action(thd, STRING_WITH_LEN(resume)));
      reporter.join();
    });
    my_testing::Mock_error_handler no_warnings(thd, 0);
    const char reached[] = "now WAIT_FOR csa_report_paused";
    ASSERT_FALSE(debug_sync_set_action(thd, STRING_WITH_LEN(reached)));
    ASSERT_TRUE(worker.found_commit_order_deadlock());

    // Replay takeover after the CSA scheduler admits the commit phase.
    // Registration precedes ownership; the earlier transaction is still
    // ahead of this one in the Commit Order Manager's queue.
    ASSERT_FALSE(commit_order.init_worker_context(1, thd));
    commit_order.register_trx(&worker);
    worker.set_committing();
    EXPECT_FALSE(worker.is_rollback_requested());
  }

  EXPECT_FALSE(worker.is_rollback_requested());
  ASSERT_TRUE(worker.found_commit_order_deadlock());
  // COM resets the MDL wait status, but the persistent report must still
  // make this commit attempt return for rollback/retry without waiting.
  EXPECT_TRUE(Commit_order_manager::wait(thd));
  EXPECT_EQ(THD::CE_COMMIT_ERROR, thd->commit_error);
  EXPECT_FALSE(worker.is_rollback_requested());
}
#endif  // ENABLED_DEBUG_SYNC

}  // namespace
