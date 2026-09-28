#include "sql/binlog/large_trx_commit.h"
#include "sql/binlog/transaction_commit_helper.h"

#include <array>
#include <atomic>
#include <memory>
#include <string_view>

#include "mutex_lock.h"  // MUTEX_LOCK
#include "my_dbug.h"
#include "my_dir.h"
#include "my_sys.h"
#include "my_systime.h"  // my_micro_time
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"       // ER_GNO_EXHAUSTED
#include "scope_guard.h"        // create_scope_guard
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/binlog.h"
#include "sql/binlog/binlog_ofile.h"  // MYSQL_BIN_LOG::Binlog_ofile
#include "sql/binlog/cache_data.h"    // binlog_cache_data
#include "sql/binlog_ostream.h"
#include "sql/current_thd.h"
#include "sql/derror.h"   // ER_THD
#include "sql/handler.h"  // ha_flush_logs
#include "sql/log_event.h"
#include "sql/mysqld.h"
#include "sql/rpl_group_replication.h"  // is_group_replication_running
#include "sql/rpl_gtid.h"               // gtid_state
#include "sql/rpl_handler.h"            // RUN_HOOK
#include "sql/rpl_log_encryption.h"     // rpl_encryption
#include "sql/rpl_replica_commit_order_manager.h"  // Commit_order_manager
#include "sql/rpl_trx_tracking.h"  // Transaction_dependency_tracker
#include "sql/sql_class.h"
#include "sql/transaction_info.h"

/**
  @file
  @brief The commit path for the binary log large transaction optimization:
  deciding whether a transaction may use it, promoting its spilled temporary
  file into the binary log sequence, and reporting the reason when a candidate
  transaction has to fall back to the standard commit path.
*/

using mysql::binlog::event::enum_binlog_checksum_alg;

namespace {

/**
  Slot 0 corresponds to kNone and is never logged to the error log;
  record_large_trx_fallback() rejects that value. Entries must be string
  literals so .data() is NUL-terminated for the %s in
  ER_BINLOG_BOLT_LARGE_TRX_FALLBACK.
*/
constexpr std::array<std::string_view, 11> kFallbackDetails{
    // kNone (never logged, record_large_trx_fallback() rejects it)
    "no fallback",
    // kStatementCache
    "the statement cache is nonempty",
    // kNonRowFormat
    "the transaction contains non-ROW events",
    // kEncryption
    "binary log encryption was enabled while the transaction was running",
    // kCompression
    "binary log transaction compression is enabled",
    // kChecksumMismatch
    "the transaction's checksum algorithm does not match binlog_checksum",
    // kReservedHeaderSpace
    "the reserved header region is too small",
    // kGtidPersistence
    "GTID persistence prevented promotion; attempting the standard "
    "commit path",
    // kIncident
    "the transaction has a logging incident to report",
    // kGroupReplication
    "Group Replication started while the transaction was open",
    // kLogClosed
    "the binary log was closed while the transaction was running",
};

static_assert(kFallbackDetails.size() ==
                  static_cast<size_t>(Large_trx_fallback_reason::kLogClosed) +
                      1,
              "add a message when adding a fallback reason");

/**
  Assess whether this transaction can use the optimized commit path.
  The BOLT knob and threshold are captured at the
  transaction's first event, so a mid-transaction change to either is ignored.

  @param cache_mngr  The session's binlog cache manager.

  @retval true   The transaction is a promotion candidate: BOLT was enabled at
                 its first event, its transaction cache spilled, and the
                 spilled size exceeds the captured threshold.
  @retval false  It is not, so it commits through the standard path without
                 being counted as a missed optimization.
*/
bool is_large_trx_commit_candidate(binlog_cache_mngr *cache_mngr) {
  binlog_cache_data *trx_cache = &cache_mngr->trx_cache;
  return trx_cache->large_trx_optimization_enabled() &&
         trx_cache->get_cache()->is_spilled() &&
         /*
           Only a named spilled file can be promoted.
         */
         trx_cache->get_cache()->tmp_file_name() != nullptr &&
         trx_cache->get_byte_position() >
             trx_cache->large_trx_optimization_threshold();
}

/**
  For a candidate transaction, the first condition that prevents it from
  committing through the BOLT path, or kNone when none does. When several
  conditions apply only the first one checked is returned; the reason is a
  diagnostic hint, not an exhaustive list.

  @param cache_mngr  The session's binlog cache manager.

  @return The blocking reason, or Large_trx_fallback_reason::kNone when the
          transaction may be promoted.
*/
Large_trx_fallback_reason large_trx_commit_blocker(
    binlog_cache_mngr *cache_mngr) {
  binlog_cache_data *trx_cache = &cache_mngr->trx_cache;
  if (!cache_mngr->stmt_cache.is_binlog_empty())
    return Large_trx_fallback_reason::kStatementCache;
  /*
    A pending incident is normally materialized and force-rotated by the
    group-commit flush, which the optimized path bypasses. Fall back to the
    standard commit path so the incident is written and the replica is notified.
  */
  if (cache_mngr->has_incident()) return Large_trx_fallback_reason::kIncident;
  if (trx_cache->may_have_sbr_stmts())
    return Large_trx_fallback_reason::kNonRowFormat;
  /*
    Only promote when the spilled file is not encrypted and binlog_encryption is
    OFF. binlog_encryption can be turned on between this check and when the
    promotion actually happens. promote_spilled_file() checks it again under
    LOCK_log to double confirm the encryption is not enabled.
  */
  if (trx_cache->get_cache()->is_encrypted() || rpl_encryption.is_enabled())
    return Large_trx_fallback_reason::kEncryption;
  if (trx_cache->get_compression_type() !=
      mysql::binlog::event::compression::NONE)
    return Large_trx_fallback_reason::kCompression;
  if (trx_cache->checksum_alg_in_cache() !=
      static_cast<enum_binlog_checksum_alg>(binlog_checksum_options))
    return Large_trx_fallback_reason::kChecksumMismatch;
  /*
    Reached only when Group Replication started after this transaction's first
    event. Falling back prevents the local promotion, but it does not make the
    transaction safe for Group Replication to send: its cached events already
    carry checksums, and the before_commit observer copies the cache exactly
    as it is into the message it broadcasts, so those checksums travel with it.
  */
  if (is_group_replication_running())
    return Large_trx_fallback_reason::kGroupReplication;
  return Large_trx_fallback_reason::kNone;
}

}  // namespace

void record_large_trx_fallback(Large_trx_fallback_reason reason) {
  assert(reason != Large_trx_fallback_reason::kNone);
  if (reason == Large_trx_fallback_reason::kNone) return;

  /* Atomic so concurrent sessions can increment this counter. */
  binlog_large_transaction_optimization_missed_count.fetch_add(
      1, std::memory_order_relaxed);
  LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_LARGE_TRX_FALLBACK,
         kFallbackDetails[static_cast<size_t>(reason)].data());
}

binlog_cache_data *get_cache_for_large_trx_commit(
    binlog_cache_mngr *cache_mngr) {
  /*
    This is the fast path out for small transactions. Such a transaction was
    never a candidate, so it is not a missed optimization and nothing is
    recorded here, unlike the checks below.
  */
  if (!is_large_trx_commit_candidate(cache_mngr)) return nullptr;

  DBUG_EXECUTE_IF("force_large_trx_compression_fallback", {
    record_large_trx_fallback(Large_trx_fallback_reason::kCompression);
    return nullptr;
  });

  const Large_trx_fallback_reason reason = large_trx_commit_blocker(cache_mngr);
  if (reason != Large_trx_fallback_reason::kNone) {
    record_large_trx_fallback(reason);
    return nullptr;
  }
  return &cache_mngr->trx_cache;
}

my_off_t large_trx_header_event_min_length(my_off_t checksum_len) {
  return LOG_EVENT_HEADER_LEN +
         mysql::binlog::event::Large_transaction_header_event::
             kFixedBodyLength +
         checksum_len;
}

bool large_trx_header_events_fit(my_off_t prefix_length, my_off_t checksum_len,
                                 my_off_t reserved) {
  return prefix_length + large_trx_header_event_min_length(checksum_len) +
             mysql::binlog::event::Gtid_event::get_max_event_length() +
             checksum_len <=
         reserved;
}

my_off_t large_trx_header_event_padding(my_off_t reserved,
                                        my_off_t gtid_event_length,
                                        my_off_t prefix_length,
                                        my_off_t checksum_len) {
  const my_off_t min_length = large_trx_header_event_min_length(checksum_len);
  const my_off_t lth_length =
      reserved - (gtid_event_length + checksum_len) - prefix_length;
  /*
    Cannot underflow: large_trx_header_events_fit() budgeted
    Gtid_event::get_max_event_length(), which is a compile-time upper bound on
    gtid_event_length, so lth_length is at least min_length.
  */
  assert(lth_length >= min_length);
  return lth_length - min_length;
}

std::pair<bool, bool> MYSQL_BIN_LOG::write_promoted_binlog_header(
    THD *thd, binlog_cache_data *cache_data, File file) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_log);

  const enum_binlog_checksum_alg checksum_alg =
      cache_data->checksum_alg_in_cache();
  assert(checksum_alg != mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF);
  const my_off_t checksum_len =
      checksum_alg != mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF
          ? BINLOG_CHECKSUM_LEN
          : 0;
  const my_off_t reserved = cache_data->get_cache()->reserved_bytes();

  /*
    The header events are serialized into memory first.
  */
  constexpr int kPromotedHeaderBlockInitialSize = 1024;
  StringBuffer_ostream<kPromotedHeaderBlockInitialSize> block;

  if (block.write(pointer_cast<const uchar *>(BINLOG_MAGIC),
                  BIN_LOG_HEADER_SIZE))
    return {true, false};

  /* The Format_description event, with rotation semantics (created = 0),
     so replicas do not treat the promoted file as a server restart. */
  Format_description_log_event fde;
  fde.common_header->flags |= LOG_EVENT_BINLOG_IN_USE_F;
  fde.dont_set_created = true;
  if (!fde.is_valid()) return {true, false};
  fde.common_footer->checksum_alg = checksum_alg;
  fde.common_header->log_pos = block.length();
  if (binary_event_serialize(&fde, &block)) return {true, false};

  /* The Previous_gtids event. The snapshot is stable: transactions commit
     under LOCK_commit, which the caller holds. */
  {
    Gtid_set logged_gtids_binlog(global_tsid_map, global_tsid_lock);
    global_tsid_lock->wrlock();
    const Gtid_set *executed_gtids = gtid_state->get_executed_gtids();
    const Gtid_set *gtids_only_in_table = gtid_state->get_gtids_only_in_table();
    if (logged_gtids_binlog.add_gtid_set(executed_gtids) != RETURN_STATUS_OK) {
      global_tsid_lock->unlock();
      return {true, false};
    }
    logged_gtids_binlog.remove_gtid_set(gtids_only_in_table);
    Previous_gtids_log_event prev_gtids_ev(&logged_gtids_binlog);
    global_tsid_lock->unlock();
    const my_off_t previous_gtids_start = block.length();
    prev_gtids_ev.common_footer->checksum_alg = checksum_alg;
    prev_gtids_ev.common_header->log_pos = previous_gtids_start;
    if (binary_event_serialize(&prev_gtids_ev, &block)) return {true, false};
    update_binlog_temp_file_previous_gtids_size_estimate(block.length() -
                                                         previous_gtids_start);
  }

  /*
    Check that the reserved region fits the remaining header events: the
    Large_transaction_header event at its minimum size and the Gtid event
    at its maximum. If not, nothing has been assigned or written yet and
    the transaction can still fall back to the standard commit path.
  */
  bool header_events_fit =
      large_trx_header_events_fit(block.length(), checksum_len, reserved);
  /*
    Debug hook to force the "reserved region too small" fallback in tests.
    Placed here, before any GTID assignment or file I/O, so it takes the same
    no-side-effects fallback path as the real check just below.
  */
  DBUG_EXECUTE_IF("force_large_trx_reserved_header_fallback",
                  header_events_fit = false;);
  if (!header_events_fit) return {false, false};

  /*
    Commit point of the promotion. The promoted file starts a new binary
    log file: rotate the dependency tracker before generating the
    transaction's logical timestamps, so they restart for the new file
    (open_binlog() must then not rotate it again). The transaction is a
    parallelization barrier for the replica's parallel applier.
  */
  m_dependency_tracker.rotate();
  thd->get_transaction()->sequence_number = m_dependency_tracker.step();

  assert(thd->next_to_commit == nullptr);
  if (assign_automatic_gtids_to_flush_group(thd)) return {true, false};

  Transaction_gtid_header metadata{thd, true /*parallelization_barrier*/,
                                   &m_dependency_tracker};

  Gtid_log_event gtid_event(
      thd, cache_data->is_trx_cache(), metadata.last_committed(),
      metadata.sequence_number(), cache_data->may_have_sbr_stmts(),
      metadata.original_commit_timestamp(),
      metadata.immediate_commit_timestamp(), metadata.original_server_version(),
      metadata.immediate_server_version());
  gtid_event.set_trx_length_by_cache_size(
      cache_data->get_byte_position(), checksum_len != 0,
      cache_data->is_checksum_computed(), cache_data->get_event_counter());

  /*
    The Large_transaction_header event's padding is sized so the Gtid
    event ends exactly at the reserved offset, where the transaction's
    first event was placed at spill time.
  */
  Large_transaction_header_log_event lth_event(
      thd, cache_data->terminating_event_offset(),
      cache_data->terminating_event_type(),
      large_trx_header_event_padding(reserved, gtid_event.get_event_length(),
                                     block.length(),
                                     checksum_len) /*padding_size*/);
  lth_event.common_footer->checksum_alg = checksum_alg;
  lth_event.common_header->log_pos = block.length();
  if (binary_event_serialize(&lth_event, &block)) return {true, false};

  gtid_event.common_footer->checksum_alg = checksum_alg;
  gtid_event.common_header->log_pos = block.length();
  if (binary_event_serialize(&gtid_event, &block)) return {true, false};
  assert(block.length() == reserved);

  /* Fill the reserved region and make the complete file durable. */
  if (mysql_file_pwrite(file, pointer_cast<const uchar *>(block.ptr()),
                        block.length(), 0, MYF(MY_WME + MY_NABP)) != 0)
    return {true, false};
  if (mysql_file_sync(file, MYF(MY_WME)) != 0) return {true, false};
  DBUG_EXECUTE_IF("crash_bolt_after_header_sync", DBUG_SUICIDE(););
  return {false, true};
}

MYSQL_BIN_LOG::Promote_outcome MYSQL_BIN_LOG::promote_spilled_file(
    THD *thd, binlog_cache_data *cache_data) {
  mysql_mutex_assert_owner(&LOCK_log);
  mysql_mutex_assert_owner(&LOCK_commit);

  char new_name[FN_REFLEN];
  /*
    file_renamed is captured by reference, so both helpers always report its
    current value rather than the value at the point they were defined.
  */
  bool file_renamed = false;
  bool purge_index_registered = false;

  auto error_outcome = [&] {
    return Promote_outcome{Promote_result::kError,
                           Large_trx_fallback_reason::kNone, file_renamed};
  };
  auto fallback_outcome = [&](Large_trx_fallback_reason reason) {
    return Promote_outcome{Promote_result::kFallback, reason, file_renamed};
  };

  /*
    Re-check the encryption policy under LOCK_log.
  */
  if (rpl_encryption.is_enabled())
    return fallback_outcome(Large_trx_fallback_reason::kEncryption);

  /*
    Another session may have turned binary logging off since this transaction
    started, which closes the log and leaves MYSQL_BIN_LOG::name null.
    Fall back so the transaction reaches ordered_commit(), which handles a
    closed log by skipping the flush and sync stages.
  */
  if (!is_open())
    return fallback_outcome(Large_trx_fallback_reason::kLogClosed);

  /* Ensure new binlog file is complete and previous binlog file is durable. */
  if (generate_new_name(new_name, name)) return error_outcome();

  /*
    The binlog-index lock covers this block: the purge-index record, the rename
    and the main-index update must not interleave with another rotation, nor
    with a purge, which runs without LOCK_log by design (see the note in
    ordered_commit) and takes this lock itself.
  */
  {
    MUTEX_LOCK(index_guard, get_index_lock());

    /*
      Persist the current binary log's GTIDs into mysql.gtid_executed, exactly
      as a normal rotation does. If that table is temporarily read-only,
      keep_current_binlog is set and we fall back to the standard commit path.

      Using a dedicated thread (Gtid_persist_thd::kDedicated) is required
      because this runs inside the committing session's own commit; the
      gtid_executed write ends in ha_commit_trans() on whichever THD performs
      it.
    */
    const auto [persist_error, keep_current_binlog] =
        persist_gtids_on_rotate(Gtid_persist_thd::kDedicated);
    if (persist_error != 0) {
      if (keep_current_binlog) {
        DBUG_EXECUTE_IF("gtid_executed_readonly",
                        { DBUG_SET("-d,gtid_executed_readonly"); });
        return fallback_outcome(Large_trx_fallback_reason::kGtidPersistence);
      }
      thd->commit_error = THD::CE_FLUSH_ERROR;
      return error_outcome();
    }

    /*
      Write the promoted file's header events into the reserved region at the
      front of the spilled file. The transaction body lives past that region, so
      this does not disturb the cache's data. A header that no longer fits is a
      clean fallback: no GTID has been assigned and nothing has been written.
    */
    const auto [header_error, fits] = write_promoted_binlog_header(
        thd, cache_data, cache_data->get_cache()->spilled_file());
    if (header_error) return error_outcome();
    if (!fits)
      return fallback_outcome(Large_trx_fallback_reason::kReservedHeaderSpace);

    /*
      From here on the promotion is visible outside this session, so failure
      must undo it. Register the new name in the purge index first: that record
      is the file's crash-recovery owner until open_binlog() adds it to the
      main index.
    */
    if (m_binlog_index_monitor.open_purge_index_file(true))
      return error_outcome();

    auto rollback_promotion = create_scope_guard([&] {
      if (purge_index_registered && file_renamed)
        (void)purge_index_entry(nullptr, nullptr, false /*need_lock_index*/);
      m_binlog_index_monitor.close_purge_index_file();
    });

    if (m_binlog_index_monitor.register_create_index_entry(new_name) ||
        m_binlog_index_monitor.sync_purge_index_file()) {
      LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_SYNC_INDEX_FILE_IN_OPEN);
      return error_outcome();
    }
    purge_index_registered = true;
    DBUG_EXECUTE_IF("crash_bolt_after_purge_index_sync", DBUG_SUICIDE(););

    /*
      Promote the file. The cache keeps its open descriptor across the rename,
      so the sync below makes the file durable under its new binary-log name.
    */
    const char *temp_file_name = cache_data->get_cache()->tmp_file_name();
    if (temp_file_name == nullptr) return error_outcome();
    if (my_rename(temp_file_name, new_name, MYF(MY_WME))) {
      LogErr(ERROR_LEVEL, ER_BINLOG_CANT_USE_FOR_LOGGING, new_name, errno);
      return error_outcome();
    }
    file_renamed = true;
    if (mysql_file_sync(cache_data->get_cache()->spilled_file(), MYF(MY_WME)) !=
        0)
      return error_outcome();
    DBUG_EXECUTE_IF("crash_bolt_after_promote_rename", DBUG_SUICIDE(););

    /*
      Chain the current binary log to the promoted file and sync
      that Rotate event before opening the promoted file as active.
    */
    {
      Rotate_log_event r(new_name + dirname_length(new_name), 0,
                         LOG_EVENT_OFFSET, 0 /*flags*/);
      if (write_event_to_binlog(&r)) return error_outcome();
    }
    if (m_binlog_file->flush_and_sync()) return error_outcome();

    DBUG_EXECUTE_IF("crash_bolt_after_rotate_event_sync", DBUG_SUICIDE(););

    /*
      The promoted file is durable and the old log's Rotate event points to it.
      Close the old log file but keep the index open, so startup recovery cannot
      act on the purge record before open_binlog() adds the promoted file to the
      main index just below.
    */
    char *old_name = name;
    name = nullptr;  // open_binlog() reassigns it; do not free here.
    close(LOG_CLOSE_TO_BE_OPENED, false /*need_lock_log*/,
          false /*need_lock_index*/);

    const bool open_failed =
        open_binlog(old_name, new_name, max_size, true /*null_created_arg*/,
                    false /*need_lock_index*/, true /*need_tsid_lock*/,
                    nullptr /*extra_description_event*/, 0 /*new_index_number*/,
                    new_name /*promoted_log_name*/);
    my_free(old_name);

    /* open_binlog() owns purge-index cleanup from here, on success and
       failure. */
    rollback_promotion.release();
    if (open_failed) return error_outcome();
  }  // index_guard destroyed: the binlog-index lock is released here

  {
    const my_off_t end_pos = m_binlog_file->position();
    thd->set_trans_pos(log_file_name, end_pos);
    thd->set_next_event_pos(log_file_name, end_pos);
  }
  if (cache_data->has_xid()) inc_prep_xids(thd);

  bool after_flush_failed = false;
  if (RUN_HOOK(binlog_storage, after_flush,
               (thd, log_file_name + dirname_length(log_file_name),
                m_binlog_file->position()))) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_RUN_AFTER_FLUSH_HOOK);
    after_flush_failed = true;
  }

  /*
    Publish the end position after the after_flush hook.
  */
  update_binlog_end_pos();

  /* LOCK_log is held; the binlog-index lock was released above. */
  if (after_flush_failed)
    handle_binlog_flush_or_sync_error(thd, false /*need_lock_log*/, nullptr);

  /* Counted once the promotion is complete. Atomic for consistency with the
     missed counter; not strictly required here since this always runs under
     LOCK_log. */
  binlog_large_transaction_optimization_count.fetch_add(
      1, std::memory_order_relaxed);

  return {Promote_result::kPromoted, Large_trx_fallback_reason::kNone, true};
}

/**
  @return 0 on success, non-zero on error.
*/
int MYSQL_BIN_LOG::commit_large_transaction(THD *thd, bool all,
                                            bool skip_commit,
                                            binlog_cache_data *cache_data) {
  DBUG_TRACE;

  init_thd_variables(thd, all, skip_commit, true /*ready_preempt*/);

  /* Make the spilled file durable before entering the critical section. */
  if (cache_data->get_cache()->flush_and_sync_spilled_file() ||
      DBUG_EVALUATE_IF("fail_bolt_spill_file_sync", true, false)) {
    thd->commit_error = THD::CE_FLUSH_ERROR;
    cache_data->reset();
    handle_binlog_flush_or_sync_error(thd, true /*need_lock_log*/, nullptr);
    return finish_commit(thd);
  }

  /*
    We flush prepared records of the large transaction to the log of storage
    engine (for example, InnoDB redo log) right before promoting the temp file
    into a binlog file.
  */
  (void)ha_flush_logs(true);

  /*
    When a replication worker thread commits a large transaction with log_bin,
    log_replica_updates, replica_preserve_commit_order, and BOLT all enabled,
    it should wait for its turn before writing anything into the binary log.

    A transaction that falls back to the standard commit path waits again inside
    ordered_commit(), which is harmless: the first wait leaves the worker at the
    head of the commit-order queue, so the second one returns without waiting.

    On a non replica session, Commit_order_manager::wait() returns immediately,
    so this costs one call per commit, exactly as ordered_commit() already does.
  */
  if (is_persistence_enabled() &&
      (Commit_order_manager::wait_for_its_turn_before_flush_stage(thd) ||
       ending_trans(thd, all) ||
       Commit_order_manager::get_rollback_status(thd))) {
    if (Commit_order_manager::wait(thd)) return thd->commit_error;
  }

  /* Critical section */
  mysql_mutex_lock(&LOCK_log);
  wait_for_prep_xids();
  mysql_mutex_lock(&LOCK_commit);

  const Promote_outcome outcome = promote_spilled_file(thd, cache_data);

  switch (outcome.result) {
    case Promote_result::kPromoted:
      /*
        In BOLT we let the next worker in the commit order proceed only after
        the promotion has succeeded, rather than at enqueue time as the
        standard path does. Releasing earlier would be wrong: a transaction
        that releases the next worker and then falls back could re-enter the
        flush queue behind the worker it just released, inverting the order.
      */
      Commit_order_manager::finish_one(thd);

      /*
        The binary log is durable and visible, so commit in
        the storage engines. LOCK_log is released first so the next
        transaction's flush can overlap this engine commit, exactly as the
        standard pipeline does; LOCK_commit is held across it to preserve commit
        order.
      */
      DBUG_EXECUTE_IF("crash_bolt_before_engine_commit", DBUG_SUICIDE(););
      mysql_mutex_unlock(&LOCK_log);

      {
        /*
          Run the after_sync hook after leaving the sync stage and before
          committing the engines, holding the commit lock only (see
          ordered_commit()).
        */
        const int sync_error = call_after_sync_hook(thd);

        /* The cache keeps the renamed file while resetting its state. */
        cache_data->reset(true /*preserve_spilled_file*/);

        /*
          finish_commit() is called with LOCK_commit held, so the after_commit
          hook that finish_commit() runs also happens under LOCK_commit, whereas
          ordered_commit() swaps to LOCK_after_commit first. This is fine, since
          it only adds extra lock hold time when rpl_semi_sync_source_wait_point
          is set to AFTER_COMMIT, where that hook waits for a replica
          acknowledgement.
          */
        (void)finish_commit(thd);
        mysql_mutex_unlock(&LOCK_commit);

        /*
          Handled only after both locks are released, as the ordered_commit
          does.
        */
        if (sync_error)
          handle_binlog_flush_or_sync_error(thd, true /*need_lock_log*/,
                                            nullptr);
      }

      /*
        Post-commit: rotate when the promoted active file exceeds max_size.

        Skipped once the commit has failed, as ordered_commit() does.
      */
      if (thd->commit_error == THD::CE_NONE && rotate_if_needed())
        thd->commit_error = THD::CE_COMMIT_ERROR;
      return thd->commit_error == THD::CE_COMMIT_ERROR ? 1 : 0;

    case Promote_result::kFallback:
      /*
        Nothing irreversible happened; commit through the standard path. No
        commit-order release is needed: ordered_commit()'s own wait() is a no-op
        because this session's stage is not REGISTERED, and its enroll_for()
        then calls finish_one().
      */
      mysql_mutex_unlock(&LOCK_commit);
      mysql_mutex_unlock(&LOCK_log);
      record_large_trx_fallback(outcome.reason);
      return ordered_commit(thd, all, skip_commit);

    case Promote_result::kError:
      if (thd->commit_error == THD::CE_NONE)
        thd->commit_error = THD::CE_FLUSH_ERROR;
      cache_data->reset(outcome.file_renamed /*preserve_spilled_file*/);
      /*
        Apply binlog_error_action semantics: either the server aborts, or binary
        logging is disabled and the commit proceeds in the engines. Called with
        LOCK_log still held (need_lock_log = false).

        The next worker in commit order is released by the unconditional
        end-of-group wait_and_finish() in
        Slave_worker::slave_worker_ends_group(), as for any transaction that
        binlogs nothing.
      */
      /*
        Name the GNO-exhausted error explicitly, as ordered_commit() does.
      */
      handle_binlog_flush_or_sync_error(
          thd, false /*need_lock_log*/,
          (thd->commit_error == THD::CE_FLUSH_GNO_EXHAUSTED_ERROR)
              ? ER_THD(thd, ER_GNO_EXHAUSTED)
              : nullptr);
      mysql_mutex_unlock(&LOCK_commit);
      mysql_mutex_unlock(&LOCK_log);
      return finish_commit(thd);
  }

  /*
    Unreachable: every Promote_result value returns above.
  */
  assert(false);
  mysql_mutex_unlock(&LOCK_commit);
  mysql_mutex_unlock(&LOCK_log);
  return finish_commit(thd);
}

int MYSQL_BIN_LOG::rotate_if_needed() {
  if (m_binlog_file->get_real_file_size() < static_cast<my_off_t>(max_size))
    return 0;

  bool check_purge = false;
  mysql_mutex_lock(&LOCK_log);

  DBUG_EXECUTE_IF("crash_bolt_before_max_size_rotate", DBUG_SUICIDE(););

  int error = rotate(false /*force_rotate*/, &check_purge);
  /* Match the normal group-commit rotation boundary. */
  if (!error)
    DBUG_EXECUTE_IF("crash_bolt_after_max_size_rotate", DBUG_SUICIDE(););

  mysql_mutex_unlock(&LOCK_log);

  if (!error && check_purge) auto_purge();
  return error;
}
