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

class THD;
class binlog_cache_data;
class binlog_cache_mngr;

/**
  Returns the transaction cache to commit through the large transaction
  optimization — promotion of its spilled temporary file into the binary
  log sequence — or nullptr when the transaction commits through the
  standard path. A transaction qualifies when its spilled size exceeds
  binlog_large_transaction_optimization_threshold and no condition forces
  the standard path: non-ROW events in the cache, binary log encryption,
  transaction compression, a binlog_checksum change since the
  transaction's first event, or a non-empty statement cache.

  @param cache_mngr  The session's binlog cache manager.

  @return the cache to promote, or nullptr.
*/
binlog_cache_data *get_cache_to_promote(binlog_cache_mngr *cache_mngr);

enum class Large_trx_fallback_reason {
  statement_cache,
  non_row_format,
  encryption,
  compression,
  checksum_change,
  reserved_header_space,
  gtid_persistence,
  incident,
};

/**
 * Returns whether a transaction is eligible for BOLT promotion without
 * recording fallback telemetry. Use before prepare when the durability policy
 * must be selected without changing commit-path accounting.
 */
bool is_large_trx_promotion_eligible(binlog_cache_mngr *cache_mngr);

/** Record an attempted promotion that must use the standard commit path. */
void record_large_trx_fallback(Large_trx_fallback_reason reason);

#endif  // BINLOG_LARGE_TRX_COMMIT_H_INCLUDED
