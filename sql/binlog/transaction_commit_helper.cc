#include "sql/binlog/transaction_commit_helper.h"

#include "dur_prop.h"
#include "my_dbug.h"
#include "my_systime.h"
#include "sql/mysqld.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_trx_tracking.h"
#include "sql/sql_class.h"
#include "sql/transaction_info.h"

/**
  @file
  @brief Implementations for sql/binlog/transaction_commit_helper.h.
*/

void init_thd_variables(THD *thd, bool all, bool skip_commit,
                        [[maybe_unused]] bool ready_preempt) {
  /* These values are reset before a transaction enters commit processing. */
  thd->tx_commit_pending = true;
  thd->commit_error = THD::CE_NONE;
  thd->next_to_commit = nullptr;
  thd->durability_property = HA_IGNORE_DURABILITY;
  thd->get_transaction()->m_flags.real_commit = all;
  thd->get_transaction()->m_flags.xid_written = false;
  thd->get_transaction()->m_flags.commit_low = !skip_commit;
  thd->get_transaction()->m_flags.run_hooks = !skip_commit;
#ifndef NDEBUG
  thd->get_transaction()->m_flags.ready_preempt = ready_preempt;
#endif
}

Transaction_gtid_header::Transaction_gtid_header(
    THD *thd, bool parallelization_barrier,
    Transaction_dependency_tracker *dependency_tracker) {
  dependency_tracker->get_dependency(thd, parallelization_barrier,
                                     m_sequence_number, m_last_committed);

  /* Preserve commit ordering when the statement cache follows this cache. */
  thd->get_transaction()->last_committed = SEQ_UNINIT;

  m_immediate_commit_timestamp = my_micro_time();
  m_original_commit_timestamp = thd->variables.original_commit_timestamp;
  if (m_original_commit_timestamp == UNDEFINED_COMMIT_TIMESTAMP) {
    if (thd->slave_thread || thd->is_binlog_applier()) {
      m_original_commit_timestamp = 0;
    } else {
      DBUG_EXECUTE_IF("rpl_invalid_gtid_timestamp",
                      m_immediate_commit_timestamp += 3600000000;);
      m_original_commit_timestamp = m_immediate_commit_timestamp;
    }
  } else {
    thd->variables.original_commit_timestamp = UNDEFINED_COMMIT_TIMESTAMP;
  }

  m_immediate_server_version = do_server_version_int(::server_version);
  thd->variables.immediate_server_version = UNDEFINED_SERVER_VERSION;
  DBUG_EXECUTE_IF("fixed_server_version", m_immediate_server_version = 888888;);
  DBUG_EXECUTE_IF("gr_fixed_server_version",
                  m_immediate_server_version = 777777;);

  m_original_server_version = thd->variables.original_server_version;
  if (m_original_server_version == UNDEFINED_SERVER_VERSION) {
    if (thd->slave_thread || thd->is_binlog_applier()) {
      m_original_server_version = UNKNOWN_SERVER_VERSION;
    } else {
      m_original_server_version = m_immediate_server_version;
    }
  } else {
    thd->variables.original_server_version = UNDEFINED_SERVER_VERSION;
  }
}
