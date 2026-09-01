#ifndef BINLOG_TRANSACTION_COMMIT_HELPER_H_INCLUDED
#define BINLOG_TRANSACTION_COMMIT_HELPER_H_INCLUDED

#include <cstdint>

#include "my_inttypes.h"

/**
  @file
  @brief State shared by the group commit path and the large transaction
  optimization's commit path: the per-transaction THD setup both perform, and
  the fields both use to build a transaction's Gtid event. Extracted so the two
  paths derive them from the same code rather than from two copies.
*/

class THD;
class Transaction_dependency_tracker;

/**
  Initializes THD state shared by group commit and the binlog large transaction
  optimization. Resets the per-commit error and queue fields, and records on
  the transaction which follow-up work the commit still owes.

  @param thd           Session about to enter commit processing.
  @param all           True when committing a whole transaction, false for a
                       single statement. Stored as the real_commit flag.
  @param skip_commit   True when the caller commits in the engines itself, so
                       the commit_low and run_hooks flags are cleared.
  @param ready_preempt Debug-only flag used by the group commit preemption
                       tests. Ignored in release builds.
*/
void init_thd_variables(THD *thd, bool all, bool skip_commit,
                        bool ready_preempt = false);

/**
  This class collects the fields used to create a transaction's GTID event.
  It is used by both group and large transaction commit codepaths.
*/
class Transaction_gtid_header {
 public:
  /**
    Collects every field of the transaction's Gtid event. Constructing this
    consumes state from the session: it takes logical timestamps from the
    dependency tracker, and clears the session's original commit timestamp and
    server version overrides so a later transaction does not inherit them. It
    is therefore built once per transaction, immediately before the Gtid event
    is serialized.

    @param thd                     Session whose transaction is committing.
    @param parallelization_barrier  True when the transaction must not be
           applied in parallel with its neighbours, which makes the tracker
           assign it a last_committed equal to its own sequence_number. A
           promoted transaction sets this.
    @param dependency_tracker      Supplies the sequence_number and
           last_committed pair that lets a replica decide what may be applied
           in parallel.
  */
  Transaction_gtid_header(THD *thd, bool parallelization_barrier,
                          Transaction_dependency_tracker *dependency_tracker);

  /// @return Sequence number of the last transaction this one depends on.
  int64 last_committed() const { return m_last_committed; }

  /// @return This transaction's own logical commit sequence number.
  int64 sequence_number() const { return m_sequence_number; }

  /**
    @return When the transaction committed on the server that originated it, in
            microseconds. Zero for a replica-applied transaction whose source
            did not supply one.
  */
  ulonglong original_commit_timestamp() const {
    return m_original_commit_timestamp;
  }

  /// @return When the transaction committed on this server, in microseconds.
  ulonglong immediate_commit_timestamp() const {
    return m_immediate_commit_timestamp;
  }

  /**
    @return Version of the server that originated the transaction, or
            UNKNOWN_SERVER_VERSION when a replica applies one that carried
            none.
  */
  uint32_t original_server_version() const { return m_original_server_version; }

  /// @return Version of this server.
  uint32_t immediate_server_version() const {
    return m_immediate_server_version;
  }

 private:
  /// Sequence number of the last transaction this one depends on.
  int64 m_last_committed;
  /// This transaction's own logical commit sequence number.
  int64 m_sequence_number;
  /// Commit time on the originating server, in microseconds.
  ulonglong m_original_commit_timestamp;
  /// Commit time on this server, in microseconds.
  ulonglong m_immediate_commit_timestamp;
  /// Version of the server that originated the transaction.
  uint32_t m_original_server_version;
  /// Version of this server.
  uint32_t m_immediate_server_version;
};

#endif  // BINLOG_TRANSACTION_COMMIT_HELPER_H_INCLUDED
