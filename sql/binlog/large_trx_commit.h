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

#ifndef BINLOG_LARGE_TRX_COMMIT_H_INCLUDED
#define BINLOG_LARGE_TRX_COMMIT_H_INCLUDED

/**
  @file

  The commit path of the binlog large transaction optimization (BOLT).
  When a transaction's binlog cache has spilled to a file larger than
  binlog_large_transaction_optimization_threshold, the cache is not copied
  into the binary log. Instead, the binary log is rotated in the flush stage,
  and the spilled file becomes the new binary log file. See
  MYSQL_BIN_LOG::rotate_and_promote_spilled_file().
*/

#include "my_inttypes.h"  // my_off_t

class IBasic_binlog_file_reader;
class THD;
class binlog_cache_data;
class binlog_cache_mngr;
namespace mysql::binlog::event {
class Large_transaction_header_event;
}  // namespace mysql::binlog::event

/**
  Why a transaction cannot commit through the large transaction
  optimization's commit path.
*/
enum class Large_trx_fallback_reason {
  /** No fallback: the spilled file can be promoted. */
  kNone,
  /** The transaction also changed a non-transactional table. */
  kStatementCache,
  /** The transaction contains events that are not in ROW format. */
  kNonRowFormat,
  /** Binlog encryption is enabled. */
  kEncryption,
  /** Binary log transaction compression is enabled. */
  kCompression,
  /** The cache's checksum algorithm does not match binlog_checksum. */
  kChecksumMismatch,
  /** The reserved header region cannot hold the header events. */
  kReservedHeaderSpace,
  /** mysql.gtid_executed could not be updated at the rotation. */
  kGtidPersistence,
  /** The transaction has a logging incident to report. */
  kIncident,
  /** The binary log was closed while the transaction ran. */
  kLogClosed,
  /** The cache could not be rewritten into binlog file form. */
  kRewriteFailed,
  /** The spilled file could not be promoted. */
  kPromotionFailed,
};

/** The outcome of MYSQL_BIN_LOG::rotate_and_promote_spilled_file(). */
enum class Spilled_file_promotion_result {
  /** The spilled file became the new binary log file. */
  kPromoted,
  /** The transaction falls back and is written as on the standard path. */
  kFellBack,
  /** The rotation failed. The flush stage handles it as a flush error. */
  kRotationError,
};

/**
  Returns the transaction cache to commit through the large transaction
  optimization, that is, by promoting its spilled temporary file into the
  binary log sequence, or nullptr when the transaction commits through the
  standard path. When a candidate transaction is blocked, the blocking
  condition is recorded (see record_large_trx_fallback()).

  @param cache_mngr  The session's binlog cache manager.

  @return The cache whose spilled file is to be promoted, or nullptr when the
          transaction commits through the standard path.
*/
binlog_cache_data *get_cache_for_large_trx_commit(
    binlog_cache_mngr *cache_mngr);

/**
  Records that a candidate transaction had to use the standard commit path:
  increments binlog_large_transaction_optimization_missed_count and writes a
  diagnostic naming the blocking condition.

  @param reason  The blocking condition. Must not be
                 Large_trx_fallback_reason::kNone.
*/
void record_large_trx_fallback(Large_trx_fallback_reason reason);

/*
  The promoted file's header events are written into a region reserved at the
  front of the spilled file. The three functions below are that region's
  boundary arithmetic. They are declared here so that unit tests can exercise
  them.
*/

/**
  The smallest on-disk length a Large_transaction_header event can have, that
  is, with no padding.

  @param checksum_len  BINLOG_CHECKSUM_LEN when the cache carries per-event
                       checksums, otherwise 0.
  @param body_length   Length of the terminating event body that the header
                       records.

  @return The length in bytes.
*/
my_off_t large_trx_header_event_min_length(my_off_t checksum_len,
                                           my_off_t body_length);

/**
  Whether the reserved region can hold the promoted file's header events: the
  bytes already serialized, a minimal Large_transaction_header event recording
  the longest terminating event body, and a Gtid event at its maximum possible
  size.

  @param prefix_length  Bytes already serialized into the region, that is the
                        magic, the Format_description event and the
                        Previous_gtids event.
  @param checksum_len   As for large_trx_header_event_min_length().
  @param reserved       The reserved region's size.

  @retval true   The header events fit and the spilled file may be promoted.
  @retval false  They do not, so the transaction falls back with
                 Large_trx_fallback_reason::kReservedHeaderSpace.
*/
bool large_trx_header_events_fit(my_off_t prefix_length, my_off_t checksum_len,
                                 my_off_t reserved);

/**
  The padding the Large_transaction_header event needs so that the Gtid event
  after it ends exactly at the end of the reserved region. The transaction's
  first event starts there.

  Call this function only after large_trx_header_events_fit() returned true for
  the same reserved region and prefix. large_trx_header_events_fit() budgets
  the Gtid event's maximum length, so the padding cannot be negative.

  @param reserved           The reserved region's size.
  @param gtid_event_length  The Gtid event's actual length, excluding its
                            checksum.
  @param prefix_length      As for large_trx_header_events_fit().
  @param checksum_len       As for large_trx_header_event_min_length().
  @param body_length        As for large_trx_header_event_min_length().

  @return The padding size, in bytes.
*/
my_off_t large_trx_header_event_padding(my_off_t reserved,
                                        my_off_t gtid_event_length,
                                        my_off_t prefix_length,
                                        my_off_t checksum_len,
                                        my_off_t body_length);

/**
  The result of checking a Large_transaction_header event against the binary
  log file that holds it (see find_large_trx_terminating_event()).
*/
enum class Large_trx_header_check {
  /** The header describes the terminating event: the body can be skipped. */
  kVerified,
  /**
    The header body is in a newer format version, or the transaction does not
    end with an Xid or XA_prepare event, so there is no XID to verify the
    terminating event against: the transaction is read sequentially.
  */
  kNotApplicable,
  /**
    The header body is malformed, or the terminating event that the header
    records cannot be read at the recorded offset: the transaction is read
    sequentially.
  */
  kInvalid,
  /**
    The reader could not seek back to its position after the header. The
    reader's position is unknown, so the caller cannot go on reading the
    file.
  */
  kReadError,
};

/**
  Finds the terminating event that a Large_transaction_header event records,
  so that binary log crash recovery and GTID reconstruction can seek past the
  transaction body. The event at the recorded offset must be ahead of the
  reader's position. The function reads that event and checks that the event
  has the recorded type, length and body (its XID).

  @param      reader     The reader of the binary log file that holds the
                         header, positioned after the header.
  @param      file_size  The size of that file.
  @param      lth        The Large_transaction_header event.
  @param[out] end        The end of the terminating event, set only when the
                         result is kVerified. The end is the reader's position
                         after reading the event, not the event's log_pos,
                         which wraps beyond 4 GiB.

  @return The result of the check.
*/
Large_trx_header_check find_large_trx_terminating_event(
    IBasic_binlog_file_reader &reader, my_off_t file_size,
    const mysql::binlog::event::Large_transaction_header_event &lth,
    my_off_t *end);

#endif  // BINLOG_LARGE_TRX_COMMIT_H_INCLUDED
