#include "sql/binlog/large_trx_commit.h"

#include <array>
#include <atomic>
#include <memory>
#include <string_view>

#include "mutex_lock.h"  // MUTEX_LOCK
#include "my_dbug.h"
#include "my_dir.h"
#include "my_sys.h"
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
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/derror.h"      // ER_THD
#include "sql/handler.h"     // ha_flush_logs
#include "sql/log_event.h"
#include "sql/mysqld.h"
#include "sql/rpl_commit_stage_manager.h"          // Commit_stage_manager
#include "sql/rpl_gtid.h"                          // gtid_state
#include "sql/rpl_handler.h"                       // RUN_HOOK
#include "sql/rpl_log_encryption.h"                // rpl_encryption
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
constexpr std::array<std::string_view, 13> kFallbackDetails{
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
    // kRewriteFailed
    "the binary log cache could not be rewritten into the binary log file "
    "format",
    // kPromotionFailed
    "the spilled binary log cache file could not be promoted; the "
    "transaction was written into the binary log instead",
};

static_assert(
    kFallbackDetails.size() ==
        static_cast<size_t>(Large_trx_fallback_reason::kPromotionFailed) + 1,
    "add a message when adding a fallback reason");

/**
  Whether the transaction is a promotion candidate, that is, whether its
  spilled file may become a binary log file.

  @param cache_mngr  The session's binlog cache manager.

  @retval true   The transaction is a promotion candidate: it was eligible at
                 its first event (see
                 binlog_cache_data::latch_large_trx_optimization()), its
                 cache was rewritten into binlog file form at the first spill,
                 and the spilled size exceeds the threshold captured at the
                 first event.
  @retval false  It is not, so it commits through the standard path without
                 being counted as a missed optimization.
*/
bool is_large_trx_commit_candidate(binlog_cache_mngr *cache_mngr) {
  binlog_cache_data *trx_cache = &cache_mngr->trx_cache;
  return trx_cache->large_trx_optimization_enabled() &&
         trx_cache->get_cache()->is_spilled() &&
         trx_cache->get_cache()->is_large_trx_rewritten() &&
         /*
           Only a named spilled file can be promoted.
         */
         trx_cache->get_cache()->tmp_file_name() != nullptr &&
         trx_cache->get_byte_position() >
             trx_cache->large_trx_optimization_threshold();
}

/**
  Whether the transaction cache's physical bytes are using the same
  checksum algorithm as binlog_checksum_options.
*/
bool large_trx_cache_checksum_matches_binlog(binlog_cache_data *trx_cache) {
  const auto active_checksum =
      static_cast<enum_binlog_checksum_alg>(binlog_checksum_options);
  if (active_checksum == mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF)
    return !trx_cache->get_cache()->physical_events_have_checksum();
  return trx_cache->get_cache()->physical_checksum_alg() == active_checksum;
}

/**
  For a candidate transaction, the first condition that prevents it from
  committing through the BOLT path, or kNone when none does. When several
  conditions apply only the first one checked is returned; the reason is a
  diagnostic hint, not an exhaustive list.

  @param cache_mngr  The session's binlog cache manager.

  @return The blocking reason, or Large_trx_fallback_reason::kNone when the
          transaction's spilled file may be promoted.
*/
Large_trx_fallback_reason large_trx_commit_blocker(
    binlog_cache_mngr *cache_mngr) {
  binlog_cache_data *trx_cache = &cache_mngr->trx_cache;
  if (!cache_mngr->stmt_cache.is_binlog_empty())
    return Large_trx_fallback_reason::kStatementCache;
  /*
    A pending incident replaces the transaction in the flush stage:
    binlog_cache_mngr::flush() resets both caches, writes an incident event
    instead, and forces a rotation. The spilled file then holds nothing to
    promote, so the transaction falls back here, before its spilled file is
    synced.
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
  if (!is_large_trx_commit_candidate(cache_mngr)) {
    binlog_cache_data *trx_cache = &cache_mngr->trx_cache;
    const Large_trx_fallback_reason reason =
        trx_cache->get_cache()->large_trx_rewrite_failed()
            ? Large_trx_fallback_reason::kRewriteFailed
            : trx_cache->large_trx_ineligible_reason();
    // Only a transaction whose cache spilled could have its file promoted.
    if (reason != Large_trx_fallback_reason::kNone &&
        trx_cache->get_cache()->is_spilled() &&
        trx_cache->get_byte_position() >
            trx_cache->large_trx_optimization_threshold())
      record_large_trx_fallback(reason);
    return nullptr;
  }

  DBUG_EXECUTE_IF("force_bolt_compression_fallback", {
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

bool MYSQL_BIN_LOG::reopen_promoted_binlog_file() {
  DBUG_TRACE;
  bool ret = false;
  const myf flags = MY_WME | MY_NABP | MY_WAIT_IF_FULL;
  /* LOCK_sync: no thread may sync m_binlog_file while it is reopened. */
  mysql_mutex_lock(&LOCK_sync);
  m_binlog_file->close();
  ret = m_binlog_file->open(m_log_file_key, log_file_name, flags,
                            true /*existing*/);
  if (!ret) {
    MY_STAT info;
    if (mysql_file_stat(m_log_file_key, log_file_name, &info, MYF(MY_WME)) ==
            nullptr ||
        m_binlog_file->position_at(info.st_size))
      ret = true;
  }
  mysql_mutex_unlock(&LOCK_sync);
  return ret;
}

bool MYSQL_BIN_LOG::promote_spilled_file(Spilled_file_promotion *promotion) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_log);
  m_binlog_index_monitor.assert_owner();

  THD *const thd = promotion->thd;
  binlog_cache_data *const cache_data = promotion->cache_data;
  Binlog_cache_storage *const cache = cache_data->get_cache();
  auto fall_back = [promotion](Large_trx_fallback_reason reason) {
    promotion->fallback_reason = reason;
    return false;
  };

  DBUG_EXECUTE_IF("crash_bolt_after_purge_index_sync", DBUG_SUICIDE(););

  /* 1. Encryption and the checksum algorithm, under LOCK_log (also checked
     before the rotation, in rotate_and_promote_spilled_file()). */
  if (rpl_encryption.is_enabled())
    return fall_back(Large_trx_fallback_reason::kEncryption);
  if (!large_trx_cache_checksum_matches_binlog(cache_data))
    return fall_back(Large_trx_fallback_reason::kChecksumMismatch);

  assert(cache->is_large_trx_rewritten());
  const enum_binlog_checksum_alg checksum_alg = cache->physical_checksum_alg();
  const my_off_t checksum_len = cache->footer_length();
  const my_off_t reserved = cache->reserved_bytes();

  /* 2. The new file's header events, and whether the reserved region holds
     them with the Large_transaction_header and Gtid events. */
  const my_off_t header_end = m_binlog_file->position();
  if (!large_trx_header_events_fit(header_end, checksum_len, reserved))
    return fall_back(Large_trx_fallback_reason::kReservedHeaderSpace);

  /* Write the header events into the new file, so they can be read back. */
  if (m_binlog_file->flush())
    return fall_back(Large_trx_fallback_reason::kPromotionFailed);

  std::string header(header_end, '\0');
  {
    const File new_file =
        mysql_file_open(m_log_file_key, log_file_name, O_RDONLY, MYF(MY_WME));
    if (new_file < 0)
      return fall_back(Large_trx_fallback_reason::kPromotionFailed);
    const bool read_error =
        mysql_file_pread(new_file, pointer_cast<uchar *>(header.data()),
                         header_end, 0, MYF(MY_WME + MY_NABP)) != 0;
    mysql_file_close(new_file, MYF(0));
    if (read_error)
      return fall_back(Large_trx_fallback_reason::kPromotionFailed);
  }

  /*
    3. The header block: the new file's header events, then the
    Large_transaction_header and Gtid events. The promoted file starts a new
    binary log file: rotate the dependency tracker before generating the
    transaction's logical timestamps (open_binlog() then does not rotate it
    again). The transaction is a parallelization barrier for the replica's
    parallel applier.
  */
  constexpr int kPromotedHeaderBlockInitialSize = 1024;
  StringBuffer_ostream<kPromotedHeaderBlockInitialSize> block;
  if (block.write(pointer_cast<const uchar *>(header.data()), header_end))
    return fall_back(Large_trx_fallback_reason::kPromotionFailed);

  m_dependency_tracker.rotate();
  Transaction_ctx *trn_ctx = thd->get_transaction();
  trn_ctx->sequence_number = m_dependency_tracker.step();
  if (trn_ctx->last_committed == SEQ_UNINIT)
    trn_ctx->last_committed = trn_ctx->sequence_number - 1;
  /* A fallback below writes the transaction as usual, which creates its Gtid
     event again from these session values. */
  const ulonglong saved_original_commit_timestamp =
      thd->variables.original_commit_timestamp;
  const uint32_t saved_original_server_version =
      thd->variables.original_server_version;
  const uint32_t saved_immediate_server_version =
      thd->variables.immediate_server_version;
  auto fall_back_after_gtid = [&](Large_trx_fallback_reason reason) {
    thd->variables.original_commit_timestamp = saved_original_commit_timestamp;
    thd->variables.original_server_version = saved_original_server_version;
    thd->variables.immediate_server_version = saved_immediate_server_version;
    return fall_back(reason);
  };
  Gtid_log_event gtid_event =
      create_gtid_event(thd, cache_data, true /*parallelization_barrier*/);
  gtid_event.set_trx_length_by_cache_size(
      cache->physical_length(), checksum_len != 0,
      cache_data->is_checksum_computed(), cache_data->get_event_counter());

  /* The padding makes the Gtid event end where the transaction's first event
     starts, at the end of the reserved region. */
  Large_transaction_header_log_event lth_event(
      thd, cache_data->terminating_event_offset(),
      cache_data->terminating_event_type(),
      large_trx_header_event_padding(reserved, gtid_event.get_event_length(),
                                     block.length(),
                                     checksum_len) /*padding_size*/);
  lth_event.common_footer->checksum_alg = checksum_alg;
  lth_event.common_header->log_pos = block.length();
  if (binary_event_serialize(&lth_event, &block))
    return fall_back_after_gtid(Large_trx_fallback_reason::kPromotionFailed);
  gtid_event.common_footer->checksum_alg = checksum_alg;
  gtid_event.common_header->log_pos = block.length();
  if (binary_event_serialize(&gtid_event, &block))
    return fall_back_after_gtid(Large_trx_fallback_reason::kPromotionFailed);
  assert(block.length() == reserved);

  const File spilled_file = cache->spilled_file();
  if (mysql_file_pwrite(spilled_file, pointer_cast<const uchar *>(block.ptr()),
                        block.length(), 0, MYF(MY_WME + MY_NABP)) != 0 ||
      mysql_file_sync(spilled_file, MYF(MY_WME)) != 0)
    return fall_back_after_gtid(Large_trx_fallback_reason::kPromotionFailed);
  DBUG_EXECUTE_IF("crash_bolt_after_header_sync", DBUG_SUICIDE(););

  /*
    4. Replace the new file with the spilled file. The cache keeps its open
    descriptor across the rename, and keeps the file at its reset.
  */
  const char *temp_file_name = cache->tmp_file_name();
  if (temp_file_name == nullptr ||
      DBUG_EVALUATE_IF("fail_bolt_promotion_rename", true, false) ||
      my_rename(temp_file_name, log_file_name, MYF(MY_WME)))
    return fall_back_after_gtid(Large_trx_fallback_reason::kPromotionFailed);
  promotion->promoted = true;
  promotion->header_end = header_end;
  DBUG_EXECUTE_IF("crash_bolt_after_promote_rename", DBUG_SUICIDE(););

  if (reopen_promoted_binlog_file() ||
      DBUG_EVALUATE_IF("fail_bolt_promoted_file_reopen", true, false))
    return true;
  DBUG_EXECUTE_IF("crash_bolt_after_rotate_event_sync", DBUG_SUICIDE(););
  return false;
}

Spilled_file_promotion_result MYSQL_BIN_LOG::rotate_and_promote_spilled_file(
    THD *thd, binlog_cache_data *cache_data) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_log);
  Spilled_file_promotion promotion;
  promotion.thd = thd;
  promotion.cache_data = cache_data;

  /*
    Conditions that need no new file are checked before the rotation, so a
    transaction that falls back for them stays in the active binary log.
  */
  int error = 0;
  if (!is_open()) {
    promotion.fallback_reason = Large_trx_fallback_reason::kLogClosed;
  } else if (rpl_encryption.is_enabled()) {
    promotion.fallback_reason = Large_trx_fallback_reason::kEncryption;
  } else if (!large_trx_cache_checksum_matches_binlog(cache_data)) {
    promotion.fallback_reason = Large_trx_fallback_reason::kChecksumMismatch;
  } else if (DBUG_EVALUATE_IF("force_bolt_reserved_header_fallback", true,
                              false)) {
    promotion.fallback_reason = Large_trx_fallback_reason::kReservedHeaderSpace;
  } else {
    error = new_file_impl(false /*need_lock_log*/,
                          nullptr /*extra_description_event*/, &promotion);
    /* The GTID table could not be written, and the rotation kept the active
       binary log file in use, as for any rotation: the transaction falls back
       and is written into that file. */
    if (error != 0 && promotion.fallback_reason ==
                          Large_trx_fallback_reason::kGtidPersistence)
      error = 0;
    /*
      A rotation reports its errors to the client, as for FLUSH LOGS. Here it
      runs in the flush stage, which handles a failure through
      binlog_error_action: the session must not keep that error.
    */
    if (thd->is_error()) thd->clear_error();
  }

  if (error != 0) return Spilled_file_promotion_result::kRotationError;
  if (promotion.promoted) {
    binlog_large_transaction_optimization_count.fetch_add(
        1, std::memory_order_relaxed);
    return Spilled_file_promotion_result::kPromoted;
  }
  if (promotion.fallback_reason != Large_trx_fallback_reason::kNone)
    record_large_trx_fallback(promotion.fallback_reason);
  return Spilled_file_promotion_result::kFellBack;
}
