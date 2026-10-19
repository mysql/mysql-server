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

/**
  @file
  @brief The commit path for the binary log large transaction optimization:
  deciding whether a transaction may use the optimization, promoting the
  transaction's spilled file to a binary log file, and reporting why a
  candidate transaction falls back to the standard commit path.
*/

#include "sql/binlog/large_trx_commit.h"

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#include <array>
#include <atomic>
#include <memory>
#include <string_view>

#include "my_dbug.h"
#include "my_dir.h"
#include "my_sys.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"       // ER_BINLOG_BOLT_*
#include "mysys_err.h"          // EE_SYNC
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/binlog.h"
#include "sql/binlog/binlog_ofile.h"          // MYSQL_BIN_LOG::Binlog_ofile
#include "sql/binlog/cache_data.h"            // binlog_cache_data
#include "sql/binlog/cache_representation.h"  // is_checksum_alg_in_use
#include "sql/binlog/global.h"                // binlog::global_context
#include "sql/binlog_ostream.h"
#include "sql/binlog_reader.h"  // IBasic_binlog_file_reader
#include "sql/log_event.h"
#include "sql/mysqld.h"
#include "sql/rpl_log_encryption.h"  // rpl_encryption
#include "sql/sql_class.h"
#include "sql/transaction_info.h"

using mysql::binlog::event::enum_binlog_checksum_alg;

namespace {

/**
  The detail logged for each Large_trx_fallback_reason, indexed by the
  reason. Entries must be string literals, so that .data() is NUL-terminated
  for the %s in ER_BINLOG_BOLT_LARGE_TRX_FALLBACK.
*/
constexpr std::array<std::string_view, 12> kFallbackDetails{
    // kNone (never logged, record_large_trx_fallback() rejects it)
    "no fallback",
    // kStatementCache
    "the transaction also changed a non-transactional table",
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
    "mysql.gtid_executed could not be updated at the rotation",
    // kIncident
    "the transaction has a logging incident to report",
    // kLogClosed
    "the binary log was closed while the transaction was running",
    // kRewriteFailed
    "the binary log cache could not be rewritten into the binary log file "
    "form",
    // kPromotionFailed
    "the spilled binary log cache file could not be promoted",
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
         trx_cache->get_cache()->named_file_name() != nullptr &&
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
    Only promote when the spilled file is not encrypted and binlog_encryption
    is OFF. binlog_encryption can be turned ON before the promotion, so
    promote_spilled_file() checks binlog_encryption again under LOCK_log.
  */
  if (trx_cache->get_cache()->is_encrypted() || rpl_encryption.is_enabled())
    return Large_trx_fallback_reason::kEncryption;
  /* Binlog_cache_compressor never compresses a rewritten cache. */
  assert(trx_cache->get_compression_type() ==
         mysql::binlog::event::compression::NONE);
  return Large_trx_fallback_reason::kNone;
}

/**
  Makes the directory entries of the file at `path` durable: fsyncs the
  directory that contains it. A rename is durable only once both the source
  and the destination directories are synced. On Windows this is a no-op.

  @param path  Path of a file in the directory to sync.

  @retval false  Success.
  @retval true   The directory could not be opened or synced.
*/
bool sync_directory_of(const char *path) {
#ifdef _WIN32
  (void)path;
  return false;
#else
  char dir[FN_REFLEN];
  size_t dir_length = 0;
  dirname_part(dir, path, &dir_length);
  const char *const dir_name = dir_length == 0 ? "." : dir;
  const int fd = ::open(dir_name, O_RDONLY);
  if (fd < 0) {
    set_my_errno(errno);
    MyOsError(my_errno(), EE_SYNC, MYF(0), dir_name);
    return true;
  }
  const bool error = ::fsync(fd) != 0;
  if (error) {
    set_my_errno(errno);
    MyOsError(my_errno(), EE_SYNC, MYF(0), dir_name);
  }
  ::close(fd);
  return error;
#endif
}

/*
  True for a fallback reason that the server's configuration causes. Such a
  fallback repeats for every large transaction, so it is logged once per
  server run.
*/
bool is_configuration_reason(Large_trx_fallback_reason reason) {
  return reason == Large_trx_fallback_reason::kEncryption ||
         reason == Large_trx_fallback_reason::kNonRowFormat ||
         reason == Large_trx_fallback_reason::kCompression ||
         reason == Large_trx_fallback_reason::kStatementCache;
}

/* Whether a configuration reason was logged in this server run. */
std::array<std::atomic<bool>, kFallbackDetails.size()>
    configuration_reason_logged{};

}  // namespace

void record_large_trx_fallback(Large_trx_fallback_reason reason) {
  assert(reason != Large_trx_fallback_reason::kNone);
  if (reason == Large_trx_fallback_reason::kNone) return;

  /* Atomic so concurrent sessions can increment this counter. */
  binlog_large_transaction_optimization_missed_count.fetch_add(
      1, std::memory_order_relaxed);
  const size_t index = static_cast<size_t>(reason);
  if (is_configuration_reason(reason)) {
    /* One session logs the reason. Later fallbacks are only counted. */
    std::atomic<bool> &logged = configuration_reason_logged[index];
    if (logged.load(std::memory_order_relaxed) ||
        logged.exchange(true, std::memory_order_relaxed))
      return;
    LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_LARGE_TRX_FALLBACK_ONCE,
           kFallbackDetails[index].data());
    return;
  }
  LogErr(WARNING_LEVEL, ER_BINLOG_BOLT_LARGE_TRX_FALLBACK,
         kFallbackDetails[index].data());
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

my_off_t large_trx_header_event_min_length(my_off_t checksum_len,
                                           my_off_t body_length) {
  return LOG_EVENT_HEADER_LEN +
         mysql::binlog::event::Large_transaction_header_event::
             kFixedBodyLength +
         body_length + checksum_len;
}

bool large_trx_header_events_fit(my_off_t prefix_length, my_off_t checksum_len,
                                 my_off_t reserved) {
  return prefix_length +
             large_trx_header_event_min_length(
                 checksum_len,
                 mysql::binlog::event::Large_transaction_header_event::
                     kMaxTerminatingEventBodyLength) +
             mysql::binlog::event::Gtid_event::get_max_event_length() +
             checksum_len <=
         reserved;
}

my_off_t large_trx_header_event_padding(my_off_t reserved,
                                        my_off_t gtid_event_length,
                                        my_off_t prefix_length,
                                        my_off_t checksum_len,
                                        my_off_t body_length) {
  const my_off_t min_length =
      large_trx_header_event_min_length(checksum_len, body_length);
  const my_off_t lth_length =
      reserved - (gtid_event_length + checksum_len) - prefix_length;
  /*
    Cannot underflow: large_trx_header_events_fit() budgeted
    Gtid_event::get_max_event_length(), which is a compile-time upper bound on
    gtid_event_length, and the longest terminating event body, so lth_length
    is at least min_length.
  */
  assert(lth_length >= min_length);
  return lth_length - min_length;
}

Large_trx_header_check find_large_trx_terminating_event(
    IBasic_binlog_file_reader &reader, my_off_t file_size,
    const mysql::binlog::event::Large_transaction_header_event &lth,
    my_off_t *end) {
  /*
    A header body in a newer format version is not applicable. A malformed
    header body is invalid. In both cases the transaction is read
    sequentially.
  */
  using Body_state =
      mysql::binlog::event::Large_transaction_header_event::Body_state;
  switch (lth.get_body_state()) {
    case Body_state::kDecoded:
      break;
    case Body_state::kNewerVersion:
      return Large_trx_header_check::kNotApplicable;
    case Body_state::kMalformed:
      return Large_trx_header_check::kInvalid;
  }
  const uint8_t type = lth.get_terminating_event_type();
  /*
    The optimized recovery skips the transaction body only after it verifies
    the terminating event against the XID that the header records. Only a
    transaction that ends with an Xid or XA_prepare event has an XID to verify
    against and can use the optimized recovery. Any other transaction is read
    sequentially.
  */
  if (type != mysql::binlog::event::XID_EVENT &&
      type != mysql::binlog::event::XA_PREPARE_LOG_EVENT)
    return Large_trx_header_check::kNotApplicable;

  const my_off_t position = reader.position();
  const my_off_t offset = lth.get_terminating_event_offset();
  const std::string &body = lth.get_terminating_event_body();
  if (body.empty() || offset <= position || offset >= file_size ||
      file_size - offset < LOG_EVENT_HEADER_LEN)
    return Large_trx_header_check::kInvalid;
  /*
    If the terminating event cannot be sought or read, the header is invalid,
    and the transaction is read sequentially. Only a failed seek back is a
    read error.
  */
  std::unique_ptr<Log_event> event;
  my_off_t event_end = 0;
  if (!reader.seek(offset)) {
    event.reset(reader.read_event_object());
    event_end = reader.position();
  }
  if (reader.seek(position)) return Large_trx_header_check::kReadError;
  if (event == nullptr) return Large_trx_header_check::kInvalid;

  const my_off_t checksum_len =
      binlog::is_checksum_alg_in_use(event->common_footer->checksum_alg)
          ? BINLOG_CHECKSUM_LEN
          : 0;
  const my_off_t length = event->common_header->data_written;
  if (static_cast<uint8_t>(event->get_type_code()) != type ||
      event_end - offset != length ||
      length != LOG_EVENT_HEADER_LEN + body.size() + checksum_len ||
      memcmp(event->temp_buf + LOG_EVENT_HEADER_LEN, body.data(),
             body.size()) != 0)
    return Large_trx_header_check::kInvalid;
  *end = event_end;
  return Large_trx_header_check::kVerified;
}

bool MYSQL_BIN_LOG::reopen_promoted_binlog_file() {
  DBUG_TRACE;
  const myf flags = MY_WME | MY_NABP | MY_WAIT_IF_FULL;
  /*
    Open the renamed file through a new handle first. If the open fails, the
    current handle stays open for the rotation's error path to close.
  */
  auto renamed_file = std::make_unique<Binlog_ofile>();
  MY_STAT info;
  if (DBUG_EVALUATE_IF("fail_bolt_promoted_file_reopen", true, false) ||
      renamed_file->open(m_log_file_key, log_file_name, flags,
                         true /*existing*/) ||
      mysql_file_stat(m_log_file_key, log_file_name, &info, MYF(MY_WME)) ==
          nullptr ||
      renamed_file->position_at(info.st_size))
    return true;

  /* LOCK_sync: no thread may sync m_binlog_file while it is replaced. */
  mysql_mutex_lock(&LOCK_sync);
  m_binlog_file->close();
  delete m_binlog_file;
  m_binlog_file = renamed_file.release();
  mysql_mutex_unlock(&LOCK_sync);
  return false;
}

bool MYSQL_BIN_LOG::promote_spilled_file(Spilled_file_rotation *promotion) {
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

  /*
    1. Encryption and the checksum algorithm, under LOCK_log.
    rotate_and_promote_spilled_file() also checks both before the rotation.
  */
  if (rpl_encryption.is_enabled())
    return fall_back(Large_trx_fallback_reason::kEncryption);
  if (!large_trx_cache_checksum_matches_binlog(cache_data))
    return fall_back(Large_trx_fallback_reason::kChecksumMismatch);

  assert(cache->is_large_trx_rewritten());
  const enum_binlog_checksum_alg checksum_alg = cache->physical_checksum_alg();
  const my_off_t checksum_len = cache->footer_length();
  const my_off_t reserved = cache->reserved_bytes();

  /*
    2. The new file's header events, and whether the reserved region holds
    the header events with the Large_transaction_header and Gtid events.
  */
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
  const std::string &body = cache_data->terminating_event_body();
  Large_transaction_header_log_event lth_event(
      thd, cache_data->terminating_event_offset(),
      cache_data->terminating_event_type(), body,
      large_trx_header_event_padding(reserved, gtid_event.get_event_length(),
                                     block.length(), checksum_len,
                                     body.size()) /*padding_size*/);
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
    descriptor across the rename. The cache's reset gets kKeep, so the reset
    does not delete the renamed file.
  */
  const char *spilled_file_name = cache->named_file_name();
  if (spilled_file_name == nullptr ||
      DBUG_EVALUATE_IF("fail_bolt_promotion_rename", true, false) ||
      my_rename(spilled_file_name, log_file_name, MYF(MY_WME)))
    return fall_back_after_gtid(Large_trx_fallback_reason::kPromotionFailed);
  promotion->promoted = true;
  promotion->header_end = header_end;
  /*
    Make the rename durable before open_binlog() adds the new file to the
    binary log index: sync the binary log directory, so the new file's name
    points to the promoted file after a crash, and the temp files directory,
    so the spilled file's old name does not come back.
  */
  if (DBUG_EVALUATE_IF("fail_bolt_directory_sync", true, false) ||
      sync_directory_of(log_file_name) || sync_directory_of(spilled_file_name))
    return true;
  DBUG_EXECUTE_IF("crash_bolt_after_promote_rename", DBUG_SUICIDE(););

  if (reopen_promoted_binlog_file()) return true;

  /*
    Count the transaction in the compression statistics, as
    write_transaction() does for a transaction that it writes into the
    binary log. A transaction whose spilled file is promoted does not go
    through write_transaction().
  */
  binlog::global_context.monitoring_context().transaction_compression().update(
      binlog::monitoring::log_type::BINARY, cache_data->get_compression_type(),
      thd->owned_gtid, gtid_event.immediate_commit_timestamp,
      cache_data->get_compressed_size(), cache_data->get_decompressed_size());
  return false;
}

Spilled_file_promotion_result MYSQL_BIN_LOG::rotate_and_promote_spilled_file(
    THD *thd, binlog_cache_data *cache_data) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_log);
  Spilled_file_rotation promotion;
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
                          nullptr /*extra_description_event*/, promotion);
    /* The GTID table could not be written, and the rotation kept the active
       binary log file in use, as for any rotation: the transaction falls back
       and is written into that file. */
    if (error != 0 && promotion.fallback_reason ==
                          Large_trx_fallback_reason::kGtidPersistence)
      error = 0;
    /*
      A fatal error stops the transaction: it is handled as a failed
      rotation, not by a fallback.
    */
    if (thd->is_fatal_error()) error = 1;
    /*
      A rotation reports its errors to the client, as for FLUSH LOGS. Here it
      runs in the flush stage. The flush stage handles a failed rotation
      through binlog_error_action, which reports to the client itself, and a
      fallback writes the transaction as usual. So the session must not keep
      the rotation's error.
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
