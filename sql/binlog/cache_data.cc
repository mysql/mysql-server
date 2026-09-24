/**
  @file
  @brief Lifecycle and bookkeeping for the per-session binary log caches: the
  transaction and statement caches a session accumulates events into before
  commit.
*/

#include "sql/binlog/cache_data.h"
#include "sql/rpl_group_replication.h"  // is_group_replication_running

/*
  binlog_cache_data
*/

void binlog_cache_data::latch_large_trx_optimization() {
  if (!m_large_trx_optimization_latched) {
    /*
      The binlog large transaction optimization is unavailable while Group
      Replication is running. This check is folded in here, rather than
      reported as a fallback reason, because Group Replication status must be
      known before the first event is serialized.
    */
    m_large_trx_optimization_enabled =
        opt_binlog_large_transaction_optimization_enabled &&
        !is_group_replication_running();
    m_large_trx_optimization_threshold =
        opt_binlog_large_transaction_optimization_threshold;
    m_large_trx_optimization_latched = true;
    /*
      Give the spill file a bolt_* format name only when the
      optimization is enabled for this transaction, so a disabled knob leaves
      no bolt_* files. This uses the same knob value captured here for
      the promotion decision, so naming and eligibility always agree. The
      reserved header region is applied regardless (see
      IO_CACHE_binlog_cache_storage::open); only the naming is gated.
    */
    m_cache.set_named_file(m_large_trx_optimization_enabled);
  }
}

void binlog_cache_data::cache_state_checkpoint(my_off_t pos_to_checkpoint) {
  // We only need to store the cache state for pos > 0
  if (pos_to_checkpoint) {
    cache_state state;
    state.with_rbr = flags.with_rbr;
    state.with_sbr = flags.with_sbr;
    state.with_start = flags.with_start;
    state.with_end = flags.with_end;
    state.with_content = flags.with_content;
    state.event_counter = m_event_counter;
    cache_state_map[pos_to_checkpoint] = state;
  }
}

void binlog_cache_data::cache_state_rollback(my_off_t pos_to_rollback) {
  if (pos_to_rollback) {
    std::map<my_off_t, cache_state>::iterator it;
    it = cache_state_map.find(pos_to_rollback);
    if (it != cache_state_map.end()) {
      flags.with_rbr = it->second.with_rbr;
      flags.with_sbr = it->second.with_sbr;
      flags.with_start = it->second.with_start;
      flags.with_end = it->second.with_end;
      flags.with_content = it->second.with_content;
      m_event_counter = it->second.event_counter;
    } else
      assert(it == cache_state_map.end());
  }
  // Rolling back to pos == 0 means cleaning up the cache.
  else {
    flags.with_rbr = false;
    flags.with_sbr = false;
    flags.with_start = false;
    flags.with_end = false;
    flags.with_content = false;
    m_event_counter = 0;
    m_large_trx_optimization_latched = false;
    m_large_trx_optimization_enabled = false;
    m_large_trx_optimization_threshold = 0;
    m_checksum_alg_in_cache = mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;
    m_terminating_event_offset = 0;
    m_terminating_event_type = mysql::binlog::event::UNKNOWN_EVENT;
  }
}

void binlog_cache_data::reset(bool preserve_spilled_file) {
  compute_statistics();
  remove_pending_event();

  if (m_cache.reset(preserve_spilled_file)) {
    LogErr(WARNING_LEVEL, ER_BINLOG_CANT_RESIZE_CACHE);
  }

  flags.with_xid = false;
  flags.immediate = false;
  flags.finalized = false;
  flags.with_sbr = false;
  flags.with_rbr = false;
  flags.with_start = false;
  flags.with_end = false;
  flags.with_content = false;

  /*
    The truncate function calls reinit_io_cache that calls my_b_flush_io_cache
    which may increase disk_writes. This breaks the disk_writes use by the
    binary log which aims to compute the ratio between in-memory cache usage
    and disk cache usage. To avoid this undesirable behavior, we reset the
    variable after truncating the cache.
  */
  cache_state_map.clear();
  m_event_counter = 0;
  m_checksum_alg_in_cache = mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;
  m_large_trx_optimization_latched = false;
  m_large_trx_optimization_enabled = false;
  m_large_trx_optimization_threshold = 0;
  m_terminating_event_offset = 0;
  m_terminating_event_type = mysql::binlog::event::UNKNOWN_EVENT;
  m_compressed_size = 0;
  m_decompressed_size = 0;
  m_compression_type = mysql::binlog::event::compression::NONE;
  assert(is_binlog_empty());
}

bool binlog_cache_data::has_empty_transaction() {
  /*
    The empty transaction has two events in trx/stmt binlog cache
    and no changes: one is a transaction start and other is a transaction
    end (there should be no SBR changing content and no RBR events).
  */
  if (flags.with_start &&   // Has transaction start statement
      flags.with_end &&     // Has transaction end statement
      !flags.with_content)  // Has no other content than START/END
  {
    assert(m_event_counter == 2);  // Two events in the cache only
    assert(!flags.with_sbr);       // No statements changing content
    assert(!flags.with_rbr);       // No rows changing content
    assert(!flags.immediate);      // Not a DDL
    assert(!flags.with_xid);       // Not a XID trx and not an atomic DDL Query
    return true;
  }
  return false;
}

void binlog_cache_data::truncate(my_off_t pos) {
  DBUG_PRINT("info", ("truncating to position %lu", (ulong)pos));
  remove_pending_event();

  // TODO: check the return value.
  (void)m_cache.truncate(pos);
}

int binlog_cache_data::flush_pending_event(THD *thd) {
  if (m_pending) {
    m_pending->set_flags(Rows_log_event::STMT_END_F);
    if (int error = write_event(m_pending)) return error;
    thd->clear_binlog_table_maps();
  }
  return 0;
}

int binlog_cache_data::remove_pending_event() {
  delete m_pending;
  m_pending = nullptr;
  return 0;
}

void binlog_cache_data::compute_statistics() {
  if (!is_binlog_empty()) {
    (*ptr_binlog_cache_use)++;
    if (m_cache.disk_writes() != 0) (*ptr_binlog_cache_disk_use)++;
  }
}

/*
  binlog_trx_cache_data
*/

void binlog_trx_cache_data::reset(bool preserve_spilled_file) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  m_cannot_rollback = false;
  before_stmt_pos = MY_OFF_T_UNDEF;
  binlog_cache_data::reset(preserve_spilled_file);
  DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  return;
}

void binlog_trx_cache_data::set_prev_position(my_off_t pos) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  before_stmt_pos = pos;
  cache_state_checkpoint(before_stmt_pos);
  DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  return;
}

void binlog_trx_cache_data::restore_prev_position() {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  binlog_cache_data::truncate(before_stmt_pos);
  cache_state_rollback(before_stmt_pos);
  before_stmt_pos = MY_OFF_T_UNDEF;
  /*
    Binlog statement rollback clears with_xid now as the atomic DDL statement
    marker which can be set as early as at event creation and caching.
  */
  flags.with_xid = false;
  DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  return;
}

void binlog_trx_cache_data::restore_savepoint(my_off_t pos) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  binlog_cache_data::truncate(pos);
  if (pos <= before_stmt_pos) before_stmt_pos = MY_OFF_T_UNDEF;
  cache_state_rollback(pos);
  DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
  return;
}

/*
  binlog_cache_mngr
*/

bool binlog_cache_mngr::init() {
  return stmt_cache.open(binlog_stmt_cache_size, max_binlog_stmt_cache_size) ||
         trx_cache.open(binlog_cache_size, max_binlog_cache_size);
}

void binlog_cache_mngr::reset() {
  if (!stmt_cache.is_binlog_empty()) stmt_cache.reset();
  if (!trx_cache.is_binlog_empty()) trx_cache.reset();
}

int binlog_cache_mngr::flush(THD *thd, my_off_t *bytes_written,
                             bool *wrote_xid) {
  my_off_t stmt_bytes = 0;
  my_off_t trx_bytes = 0;
  assert(stmt_cache.has_xid() == 0);

  bool parallelization_barrier = false;
  if (has_incident()) {
    if (int error = handle_deferred_cache_write_incident(thd)) return error;
    // Request force rotate
    thd->rpl_thd_ctx.binlog_group_commit_ctx().set_force_rotate();
    // Set as parallelization_barrier so that dependency tracker marks all
    // subsequent transactions to depend on it.
    parallelization_barrier = true;
  }

  int error =
      stmt_cache.flush(thd, &stmt_bytes, wrote_xid, parallelization_barrier);
  if (error) return error;
  DEBUG_SYNC(thd, "after_flush_stm_cache_before_flush_trx_cache");
  error = trx_cache.flush(thd, &trx_bytes, wrote_xid, parallelization_barrier);
  if (error) return error;
  *bytes_written = stmt_bytes + trx_bytes;
  return 0;
}

bool binlog_cache_mngr::has_empty_transaction() {
  return (trx_cache.is_empty_or_has_empty_transaction() &&
          stmt_cache.is_empty_or_has_empty_transaction() && !is_binlog_empty());
}
