#include "sql/binlog/large_trx_commit.h"
#include "sql/binlog/transaction_commit_helper.h"

#include <atomic>
#include <memory>

#include "my_dbug.h"
#include "my_dir.h"
#include "my_sys.h"
#include "my_systime.h"  // my_micro_time
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/psi/mysql_file.h"
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/binlog.h"
#include "sql/current_thd.h"
#include "sql/binlog/cache_data.h"  // binlog_cache_data
#include "sql/binlog/binlog_ofile.h"  // MYSQL_BIN_LOG::Binlog_ofile
#include "sql/binlog_ostream.h"
#include "sql/handler.h"  // ha_flush_logs
#include "sql/log_event.h"
#include "sql/mysqld.h"
#include "sql/rpl_gtid.h"     // gtid_state
#include "sql/rpl_handler.h"  // RUN_HOOK
#include "sql/rpl_trx_tracking.h"  // Transaction_dependency_tracker
#include "sql/sql_class.h"
#include "sql/transaction_info.h"

using mysql::binlog::event::enum_binlog_checksum_alg;

void record_large_trx_fallback(Large_trx_fallback_reason reason) {
  const char *detail = nullptr;
  switch (reason) {
    case Large_trx_fallback_reason::statement_cache:
      detail = "the statement cache is nonempty";
      break;
    case Large_trx_fallback_reason::non_row_format:
      detail = "the transaction contains non-ROW events";
      break;
    case Large_trx_fallback_reason::encryption:
      detail = "binary log encryption is enabled";
      break;
    case Large_trx_fallback_reason::compression:
      detail = "binary log transaction compression is enabled";
      break;
    case Large_trx_fallback_reason::checksum_change:
      detail = "binlog_checksum changed during the transaction";
      break;
    case Large_trx_fallback_reason::reserved_header_space:
      detail = "the reserved header region is too small";
      break;
    case Large_trx_fallback_reason::gtid_persistence:
      detail =
          "GTID persistence prevented promotion; attempting the standard "
          "commit path";
      break;
    case Large_trx_fallback_reason::incident:
      detail = "the transaction has a logging incident to report";
      break;
  }
  /* Atomic: record_large_trx_fallback runs in the commit path without
     LOCK_log, so concurrent sessions can increment this counter. */
  binlog_large_transaction_optimization_missed_count.fetch_add(
      1, std::memory_order_relaxed);
  LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_LARGE_TRX_FALLBACK, detail);
}

bool is_large_trx_promotion_eligible(binlog_cache_mngr *cache_mngr) {
  binlog_cache_data *trx_cache = &cache_mngr->trx_cache;
  return trx_cache->large_trx_optimization_enabled() &&
         trx_cache->get_cache()->is_spilled() &&
         trx_cache->get_byte_position() >
             trx_cache->large_trx_optimization_threshold() &&
         cache_mngr->stmt_cache.is_binlog_empty() &&
         !cache_mngr->has_incident() && !trx_cache->may_have_sbr_stmts() &&
         !trx_cache->get_cache()->is_encrypted() &&
         trx_cache->get_compression_type() ==
             mysql::binlog::event::compression::NONE &&
         trx_cache->checksum_trx_start() ==
             static_cast<enum_binlog_checksum_alg>(binlog_checksum_options);
}

binlog_cache_data *get_cache_to_promote(binlog_cache_mngr *cache_mngr) {
  binlog_cache_data *trx_cache = &cache_mngr->trx_cache;
  if (!trx_cache->large_trx_optimization_enabled()) return nullptr;

  DBUG_EXECUTE_IF("force_large_trx_compression_fallback", {
    record_large_trx_fallback(Large_trx_fallback_reason::compression);
    return nullptr;
  });

  if (!trx_cache->get_cache()->is_spilled() ||
      trx_cache->get_byte_position() <=
          trx_cache->large_trx_optimization_threshold())
    return nullptr;

  DBUG_EXECUTE_IF("force_large_trx_statement_cache_fallback", {
    record_large_trx_fallback(Large_trx_fallback_reason::statement_cache);
    return nullptr;
  });
  if (!cache_mngr->stmt_cache.is_binlog_empty()) {
    record_large_trx_fallback(Large_trx_fallback_reason::statement_cache);
    return nullptr;
  }
  /*
    A pending incident is normally materialized and force-rotated by the
    group-commit flush, which the BOLT path bypasses. Fall back to the standard
    commit path so the incident is written and the replica is notified.
  */
  if (cache_mngr->has_incident()) {
    record_large_trx_fallback(Large_trx_fallback_reason::incident);
    return nullptr;
  }
  if (trx_cache->may_have_sbr_stmts()) {
    record_large_trx_fallback(Large_trx_fallback_reason::non_row_format);
    return nullptr;
  }
  /*
    Consult the spilled file's actual encryption, not the live global: a file
    that spilled while binlog_encryption was ON stays encrypted even if the
    global was turned OFF before commit, and must never be promoted (its
    plaintext header would front an encrypted body). It commits through the
    standard path, which decrypts on read.
  */
  if (trx_cache->get_cache()->is_encrypted()) {
    record_large_trx_fallback(Large_trx_fallback_reason::encryption);
    return nullptr;
  }
  if (trx_cache->get_compression_type() !=
      mysql::binlog::event::compression::NONE) {
    record_large_trx_fallback(Large_trx_fallback_reason::compression);
    return nullptr;
  }
  if (trx_cache->checksum_trx_start() !=
      static_cast<enum_binlog_checksum_alg>(binlog_checksum_options)) {
    record_large_trx_fallback(Large_trx_fallback_reason::checksum_change);
    return nullptr;
  }
  return trx_cache;
}

bool MYSQL_BIN_LOG::write_promoted_binlog_header(THD *thd,
                                                 binlog_cache_data *cache_data,
                                                 File file, bool *fits) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_log);
  *fits = true;

  const enum_binlog_checksum_alg checksum_alg =
      cache_data->checksum_trx_start();
  assert(checksum_alg != mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF);
  const my_off_t checksum_len =
      checksum_alg != mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF
          ? BINLOG_CHECKSUM_LEN
          : 0;
  const my_off_t reserved = cache_data->get_cache()->reserved_bytes();

  /* The header events are serialized into memory first. */
  StringBuffer_ostream<1024> block;

  if (block.write(pointer_cast<const uchar *>(BINLOG_MAGIC),
                  BIN_LOG_HEADER_SIZE))
    return true;

  /* The Format_description event, with rotation semantics (created = 0),
     so replicas do not treat the promoted file as a server restart. */
  Format_description_log_event fde;
  fde.common_header->flags |= LOG_EVENT_BINLOG_IN_USE_F;
  fde.dont_set_created = true;
  if (!fde.is_valid()) return true;
  fde.common_footer->checksum_alg = checksum_alg;
  fde.common_header->log_pos = block.length();
  if (binary_event_serialize(&fde, &block)) return true;

  /* The Previous_gtids event. The snapshot is stable: transactions commit
     under LOCK_commit, which the caller holds. */
  {
    Gtid_set logged_gtids_binlog(global_tsid_map, global_tsid_lock);
    global_tsid_lock->wrlock();
    const Gtid_set *executed_gtids = gtid_state->get_executed_gtids();
    const Gtid_set *gtids_only_in_table =
        gtid_state->get_gtids_only_in_table();
    if (logged_gtids_binlog.add_gtid_set(executed_gtids) != RETURN_STATUS_OK) {
      global_tsid_lock->unlock();
      return true;
    }
    logged_gtids_binlog.remove_gtid_set(gtids_only_in_table);
    Previous_gtids_log_event prev_gtids_ev(&logged_gtids_binlog);
    global_tsid_lock->unlock();
    const my_off_t previous_gtids_start = block.length();
    prev_gtids_ev.common_footer->checksum_alg = checksum_alg;
    prev_gtids_ev.common_header->log_pos = previous_gtids_start;
    if (binary_event_serialize(&prev_gtids_ev, &block)) return true;
    update_binlog_temp_file_previous_gtids_size_estimate(
        block.length() - previous_gtids_start);
  }

  /*
    Check that the reserved region fits the remaining header events: the
    Large_transaction_header event at its minimum size and the Gtid event
    at its maximum. If not, nothing has been assigned or written yet and
    the transaction can still fall back to the standard commit path.
  */
  const my_off_t lth_min_length = LOG_EVENT_HEADER_LEN +
      mysql::binlog::event::Large_transaction_header_event::kFixedBodyLength +
      checksum_len;
  const bool header_events_fit =
      block.length() + lth_min_length +
          mysql::binlog::event::Gtid_event::get_max_event_length() +
          checksum_len <= reserved;
  /*
    Debug hook to force the "reserved region too small" fallback in tests.
    Placed here, before any GTID assignment or file I/O, so it takes the same
    no-side-effects fallback path as the real check just below.
  */
  DBUG_EXECUTE_IF("force_large_trx_reserved_header_fallback", {
    *fits = false;
    return false;
  });
  if (!header_events_fit) {
    *fits = false;
    return false;
  }

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
  if (assign_automatic_gtids_to_flush_group(thd)) return true;

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
  const my_off_t gtid_length = gtid_event.get_event_length() + checksum_len;
  const my_off_t lth_length = reserved - gtid_length - block.length();
  assert(lth_length >= lth_min_length);

  Large_transaction_header_log_event lth_event(
      thd, cache_data->terminating_event_offset(),
      cache_data->terminating_event_type(),
      lth_length - lth_min_length /*padding_size*/);
  lth_event.common_footer->checksum_alg = checksum_alg;
  lth_event.common_header->log_pos = block.length();
  if (binary_event_serialize(&lth_event, &block)) return true;

  gtid_event.common_footer->checksum_alg = checksum_alg;
  gtid_event.common_header->log_pos = block.length();
  if (binary_event_serialize(&gtid_event, &block)) return true;
  assert(block.length() == reserved);

  /* Fill the reserved region and make the complete file durable. */
  if (mysql_file_pwrite(file, pointer_cast<const uchar *>(block.ptr()),
                        block.length(), 0, MYF(MY_WME + MY_NABP)) != 0)
    return true;
  if (mysql_file_sync(file, MYF(MY_WME)) != 0) return true;
  DBUG_EXECUTE_IF("crash_bolt_after_header_sync", DBUG_SUICIDE(););
  return false;
}

int MYSQL_BIN_LOG::commit_large_transaction(THD *thd, bool all,
                                               bool skip_commit,
                                               binlog_cache_data *cache_data) {
  DBUG_TRACE;
  int error = 0;
  bool fits = true;
  bool lock_index_acquired = false;
  bool purge_index_opened = false;
  bool purge_index_registered = false;
  bool promoted_file_renamed = false;
  char new_name[FN_REFLEN], *old_name;
  const char *temp_file_name = nullptr;
  bool keep_current_binlog = false;
  Large_trx_fallback_reason fallback_reason;

  /* Make the spilled file durable before entering the critical section. */
  if (cache_data->get_cache()->flush_and_sync_spilled_file() ||
      DBUG_EVALUATE_IF("fail_bolt_spill_file_sync", true, false)) {
    thd->commit_error = THD::CE_FLUSH_ERROR;
    cache_data->reset();
    return finish_commit(thd);
  }

  /* Initialize private THD state before entering the global lock section. */
  init_thd_variables(thd, all, skip_commit, true /*ready_preempt*/);

  /* Critical section */
  mysql_mutex_lock(&LOCK_log);
  wait_for_prep_xids();
  mysql_mutex_lock(&LOCK_commit);

  /*
    Stage #0: Ensure new binlog file is complete and previous binlog
              file is durable.
  */
  if ((error = generate_new_name(new_name, name))) goto err;

  /*
    Persist the current binary log's GTIDs into mysql.gtid_executed, exactly
    as a normal rotation does. If that table is temporarily read-only,
    keep_current_binlog is set and we fall back to the standard commit path.
  */
  m_binlog_index_monitor.lock();
  lock_index_acquired = true;
  if ((error = persist_gtids_on_rotate(true /*use_dedicated_thd*/,
                             &keep_current_binlog))) {
    if (keep_current_binlog) {
      DBUG_EXECUTE_IF("gtid_executed_readonly", { DBUG_SET("-d,gtid_executed_readonly"); });
      fallback_reason = Large_trx_fallback_reason::gtid_persistence;
      goto fallback_to_ordered_commit;
    }
    thd->commit_error = THD::CE_FLUSH_ERROR;
    goto err;
  }

  /*
    Write the promoted file's header events into the reserved region at the
    front of the spilled file. The transaction body lives past that region,
    so this does not disturb the cache's data; if the header does not fit,
    the fit check reports it (fits == false) and we fall back.
  */
  if (write_promoted_binlog_header(
          thd, cache_data, cache_data->get_cache()->spilled_file(), &fits))
    goto err;
  if (!fits) {
    /*
      The header events no longer fit the reserved region
      (gtid_executed grew since the spill): fall back to the standard
      commit path.
    */
    fallback_reason = Large_trx_fallback_reason::reserved_header_space;
    goto fallback_to_ordered_commit;
  }

  /*
    Register the new binary log name in the purge index and sync it, before
    the rename. This is the file's crash-recovery record until it is added to
    the main index below: if we crash after this point but before that,
    startup uses the purge index to delete the not-yet-published file.
  */
  if (m_binlog_index_monitor.open_purge_index_file(true)) goto err;
  purge_index_opened = true;
  if (m_binlog_index_monitor.register_create_index_entry(new_name) ||
      m_binlog_index_monitor.sync_purge_index_file()) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FAILED_TO_SYNC_INDEX_FILE_IN_OPEN);
    goto err;
  }
  purge_index_registered = true;
  DBUG_EXECUTE_IF("crash_bolt_after_purge_index_sync", DBUG_SUICIDE(););

  /* The cache keeps the open descriptor across the rename; sync the renamed
     file so it is durable under its new binary-log name. */
  temp_file_name = cache_data->get_cache()->tmp_file_name();
  if (temp_file_name == nullptr) goto err;
  if (my_rename(temp_file_name, new_name, MYF(MY_WME))) {
    LogErr(ERROR_LEVEL, ER_BINLOG_CANT_USE_FOR_LOGGING, new_name, errno);
    goto err;
  }
  promoted_file_renamed = true;
  if (mysql_file_sync(cache_data->get_cache()->spilled_file(), MYF(MY_WME)) !=
      0)
    goto err;
  DBUG_EXECUTE_IF("crash_bolt_after_promote_rename", DBUG_SUICIDE(););

  /*
    Stage 2 (sync): chain the current binary log to the promoted file and
    sync that rotate event before opening the promoted file as active.
  */
  {
    Rotate_log_event r(new_name + dirname_length(new_name), 0,
                       LOG_EVENT_OFFSET, 0 /*flags*/);
    if ((error = write_event_to_binlog(&r) ? 1 : 0)) goto err;
  }
  if ((error = m_binlog_file->flush_and_sync() ? 1 : 0)) goto err;

  /*
    The promoted file is durable and the old binlog's Rotate event points to
    it. Close the old log file but keep the index open: this stops startup
    recovery from acting on the purge record before open_binlog() adds the
    promoted file to the main index, just below.
  */
  old_name = name;
  name = nullptr;  // Don't free name; open_binlog reassigns it.
  close(LOG_CLOSE_TO_BE_OPENED, false /*need_lock_log*/,
        false /*need_lock_index*/);

  error = open_binlog(old_name, new_name, max_size,
                      true /*null_created_arg*/, false /*need_lock_index*/,
                      true /*need_tsid_lock*/,
                      nullptr /*extra_description_event*/,
                      0 /*new_index_number*/, new_name,
                      true /*promoted_file_is_renamed*/) ? 1 : 0;
  my_free(old_name);
  /* open_binlog() now owns purge-index cleanup on success and failure. */
  purge_index_opened = false;
  purge_index_registered = false;
  if (error) goto err;

  /* The renamed file is now indexed; reset can release the cache descriptor. */
  m_binlog_index_monitor.unlock();
  lock_index_acquired = false;

  /* Atomic for consistency with the missed counter; not strictly required
     here since this increment always runs under LOCK_log. */
  binlog_large_transaction_optimization_count.fetch_add(
      1, std::memory_order_relaxed);

  {
    const my_off_t end_pos = m_binlog_file->position();
    thd->set_trans_pos(log_file_name, end_pos);
    thd->set_next_event_pos(log_file_name, end_pos);
  }
  if (cache_data->has_xid() && thd->get_transaction()->m_flags.commit_low)
    inc_prep_xids(thd);

  if (RUN_HOOK(binlog_storage, after_flush,
               (thd, log_file_name + dirname_length(log_file_name),
                m_binlog_file->position()))) {
    error = 1;
    thd->commit_error = THD::CE_FLUSH_ERROR;
    goto err;
  }

  /*
    Stage 3 (commit): the binary log is durable and visible, so commit the
    transaction in the storage engines before releasing LOCK_commit.
  */
  DBUG_EXECUTE_IF("crash_bolt_before_engine_commit", DBUG_SUICIDE(););
  mysql_mutex_unlock(&LOCK_log);

  // The cache retains the renamed file while resetting its transaction state.
  cache_data->reset(true /*preserve_spilled_file*/);
  (void)finish_commit(thd);
  mysql_mutex_unlock(&LOCK_commit);

  /* Post-commit: rotate when the promoted active file exceeds max_size. */
  if (rotate_if_needed()) thd->commit_error = THD::CE_COMMIT_ERROR;
  return thd->commit_error == THD::CE_COMMIT_ERROR;

fallback_to_ordered_commit:
  m_binlog_index_monitor.unlock();
  lock_index_acquired = false;
  mysql_mutex_unlock(&LOCK_commit);
  mysql_mutex_unlock(&LOCK_log);
  record_large_trx_fallback(fallback_reason);
  return ordered_commit(thd, all, skip_commit);

err:
  /*
    Until the file is in the main index, the purge index owns it. On an
    ordinary error, delete the renamed file and its purge-index entry here;
    after a crash, startup performs the same cleanup from the purge index.
  */
  if (purge_index_opened) {
    if (purge_index_registered && promoted_file_renamed)
      (void)purge_index_entry(nullptr, nullptr, false /*need_lock_index*/);
    m_binlog_index_monitor.close_purge_index_file();
  }
  if (lock_index_acquired) m_binlog_index_monitor.unlock();
  if (thd->commit_error == THD::CE_NONE)
    thd->commit_error = THD::CE_FLUSH_ERROR;
  cache_data->reset(promoted_file_renamed /*preserve_spilled_file*/);
  /*
    Apply binlog_error_action semantics: either the server aborts, or
    binary logging is disabled and the commit proceeds in the engines.
  */
  handle_binlog_flush_or_sync_error(thd, false /*need_lock_log*/, nullptr);
  mysql_mutex_unlock(&LOCK_commit);
  mysql_mutex_unlock(&LOCK_log);
  return finish_commit(thd);
}

int MYSQL_BIN_LOG::rotate_if_needed() {
  if (m_binlog_file->get_real_file_size() <=
      static_cast<my_off_t>(max_size))
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
