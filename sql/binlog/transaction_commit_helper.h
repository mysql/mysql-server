#ifndef BINLOG_TRANSACTION_COMMIT_HELPER_H_INCLUDED
#define BINLOG_TRANSACTION_COMMIT_HELPER_H_INCLUDED

#include <cstdint>

#include "my_inttypes.h"

class THD;
class Transaction_dependency_tracker;

/** Initializes THD state shared by group commit and BOLT. */
void init_thd_variables(THD *thd, bool all, bool skip_commit,
                        bool ready_preempt = false);

/**
  This class collects the fields used to create a transaction's GTID event.
  It is used by both group and large transaction commit codepaths.
*/
class Transaction_gtid_header {
 public:
  Transaction_gtid_header(
      THD *thd, bool parallelization_barrier,
      Transaction_dependency_tracker *dependency_tracker);

  int64 last_committed() const { return m_last_committed; }
  int64 sequence_number() const { return m_sequence_number; }
  ulonglong original_commit_timestamp() const {
    return m_original_commit_timestamp;
  }
  ulonglong immediate_commit_timestamp() const {
    return m_immediate_commit_timestamp;
  }
  uint32_t original_server_version() const { return m_original_server_version; }
  uint32_t immediate_server_version() const {
    return m_immediate_server_version;
  }

 private:
  int64 m_last_committed;
  int64 m_sequence_number;
  ulonglong m_original_commit_timestamp;
  ulonglong m_immediate_commit_timestamp;
  uint32_t m_original_server_version;
  uint32_t m_immediate_server_version;
};

#endif  // BINLOG_TRANSACTION_COMMIT_HELPER_H_INCLUDED
