#ifndef BINLOG_LARGE_TRX_COMMIT_H_INCLUDED
#define BINLOG_LARGE_TRX_COMMIT_H_INCLUDED

/**
  @file

  The binlog large transaction optimization's commit path: a transaction
  whose spilled binlog cache exceeds
  binlog_large_transaction_optimization_threshold commits by promoting
  its temporary file into the binary log sequence as the next binary log
  file (MYSQL_BIN_LOG::commit_large_transaction), instead of copying
  the cache into the active binary log.
*/

#include "my_inttypes.h"  // my_off_t

class THD;
class binlog_cache_data;
class binlog_cache_mngr;

/**
  Why a transaction cannot commit through the large transaction
  optimization's commit path.
*/
enum class Large_trx_fallback_reason {
  kNone,
  kStatementCache,
  kNonRowFormat,
  kEncryption,
  kCompression,
  kChecksumMismatch,
  kReservedHeaderSpace,
  kGtidPersistence,
  kIncident,
  kGroupReplication,
  kLogClosed,
};

/**
  Returns the transaction cache to commit through the large transaction
  optimization, that is, by promoting its spilled temporary file into the
  binary log sequence, or nullptr when the transaction commits through the
  standard path. When a candidate transaction is blocked, the blocking
  condition is recorded (see record_large_trx_fallback).

  @param cache_mngr  The session's binlog cache manager.

  @return the cache whose spilled file is to be promoted, or nullptr when the
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

/**
  The promoted file's header events are written into a region reserved at the
  front of the spilled file. The three functions below are that region's
  boundary arithmetic.

  They are declared here, rather than kept local to
  write_promoted_binlog_header(), so a unit test can exercise the boundary. No
  MTR test can: the region is sized from the serialized Previous_gtids estimate
  at spill time, so no configuration makes it too small while leaving the
  transaction otherwise promotable.

  @param checksum_len  BINLOG_CHECKSUM_LEN when the cache carries per-event
                       checksums, otherwise 0.

  @return the smallest on-disk length a Large_transaction_header event can have,
          that is, with no padding.
*/
my_off_t large_trx_header_event_min_length(my_off_t checksum_len);

/**
  Whether the reserved region can hold the promoted file's header events: the
  bytes already serialized, a minimal Large_transaction_header event, and a
  Gtid event at its maximum possible size.

  @param prefix_length  Bytes already serialized into the region, that is the
                        magic, the Format_description event and the
                        Previous_gtids event.
  @param checksum_len   As for large_trx_header_event_min_length().
  @param reserved       The reserved region's size.

  @retval true   The header events fit and the transaction may be promoted.
  @retval false  They do not, so the transaction falls back with
                 Large_trx_fallback_reason::kReservedHeaderSpace.
*/
bool large_trx_header_events_fit(my_off_t prefix_length, my_off_t checksum_len,
                                 my_off_t reserved);

/**
  The padding the Large_transaction_header event needs so that the Gtid event
  after it ends exactly at the reserved offset, where the transaction's first
  event was placed at spill time.

  Only meaningful when large_trx_header_events_fit() returned true for the same
  reserved region and prefix; that call is what makes the subtraction below
  safe, since it budgets the Gtid event's maximum length.

  @param prefix_length      As for large_trx_header_events_fit().
  @param gtid_event_length  The Gtid event's actual length, excluding its
                            checksum.
  @param checksum_len       As for large_trx_header_event_min_length().
  @param reserved           The reserved region's size.

  @return the padding size, in bytes.
*/
my_off_t large_trx_header_event_padding(my_off_t reserved,
                                        my_off_t gtid_event_length,
                                        my_off_t prefix_length,
                                        my_off_t checksum_len);

#endif  // BINLOG_LARGE_TRX_COMMIT_H_INCLUDED
