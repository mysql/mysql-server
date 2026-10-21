# Fallbacks and error handling

This document covers when the binlog large transaction optimization (BOLT) does not promote a large transaction's spilled file (6.1), how those fallbacks are counted and logged (6.2), and how each failure is handled (6.3).

## 6.1 When the spilled file is not promoted

At commit, BOLT decides whether to promote the transaction's spilled file. A transaction that fails one of the checks below commits through the standard commit path, and the fallback reason is recorded (section 6.2). The spilled file is promoted only when the transaction passes all three checks, which run in this order:

1. The transaction is a candidate. `get_cache_for_large_trx_commit()` calls `is_large_trx_commit_candidate()`. A candidate meets all of these conditions:
   - it was eligible at its first event (see the low-level design);
   - its cache was rewritten into binlog file form and spilled into a named file;
   - its logical size exceeds the threshold captured at its first event.

   A transaction that is not a candidate is usually not counted as a missed optimization. It is counted only in two cases:
   - it was not eligible at its first event, because a BOLT blocker applied, and its cache later spilled above the threshold;
   - its cache spilled above the threshold, but the rewrite into binlog file form failed (`kRewriteFailed`).

   A transaction that started with the optimization OFF is never counted.
2. The transaction passes the commit-time checks. Before the `before_commit` hook, `large_trx_commit_blocker()` checks for the BOLT blockers in this order. The first blocker that applies is the fallback reason:
   - the statement cache is not empty (`kStatementCache`);
   - an incident is pending (`kIncident`);
   - the cache may hold statement-format events (`kNonRowFormat`);
   - the spilled file is encrypted, or binlog encryption is ON (`kEncryption`).

   `MYSQL_BIN_LOG::commit()` then checks that the binlog is open (`kLogClosed`).
3. The checks still pass under `LOCK_log`. In the flush stage, `rotate_and_promote_spilled_file()` checks again before the rotation. The binlog must be open (`kLogClosed`), encryption must be OFF (`kEncryption`), and the cache's checksum algorithm must match `binlog_checksum` (`kChecksumMismatch`). A transaction that fails one of these is written into the active binlog file. Inside the rotation, `promote_spilled_file()` checks encryption and the checksum once more (section 6.3, row 13).

## 6.2 Fallback reasons and their logging

Every fallback is counted, and its reason is logged, so that a user can see why a large transaction did not use the optimization.

`record_large_trx_fallback()` counts every fallback in `Binlog_large_transaction_optimization_missed_count` and logs its reason:

- A configuration reason (`kEncryption`, `kNonRowFormat`, `kCompression`, `kStatementCache`) repeats for every large transaction with that configuration. So it is logged only once per server run, with `ER_BINLOG_BOLT_LARGE_TRX_FALLBACK_ONCE`, to avoid flooding the error log. An atomic flag per reason decides which session logs it, without a lock.
- Every other reason is logged every time a transaction falls back to the standard commit path, with `ER_BINLOG_BOLT_LARGE_TRX_FALLBACK`.

| Reason | Logged detail |
| --- | --- |
| `kStatementCache` | the transaction also changed a non-transactional table |
| `kNonRowFormat` | the transaction contains non-ROW events |
| `kEncryption` | binlog encryption is enabled |
| `kCompression` | binlog transaction compression is enabled |
| `kChecksumMismatch` | binlog_checksum changed while the transaction was running |
| `kReservedHeaderSpace` | the header events of the new binlog file do not fit in the region reserved at the front of the spill binlog file |
| `kGtidPersistence` | mysql.gtid_executed could not be updated at the rotation |
| `kIncident` | the transaction has a logging incident to report |
| `kLogClosed` | the binlog was closed while the transaction was running |
| `kRewriteFailed` | the binlog cache could not be rewritten into the spill binlog file |
| `kPromotionFailed` | the spill binlog file could not be promoted |

An error that the promotion's rotation raised is logged with `ER_BINLOG_BOLT_PROMOTION_ROTATION_ERROR` before it is cleared from the session (section 6.3, rows 12 and 13), for both a fallback and a rotation error.

## 6.3 Error handling during the transaction, commit and recovery

This part analyzes how BOLT handles each failure during a transaction and its commit, in three parts: before the externalization point, after it, and recovery. The externalization point is the Group Replication `before_commit` hook in `MYSQL_BIN_LOG::commit()`, because it is the earliest point in the commit at which the transaction can leave the server. Group Replication, and any other `before_commit` observer, may broadcast and certify the transaction there. Binlog replication sends the transaction later, when the binlog end position is published past it: after the flush stage when `sync_binlog` is not 1, or after the sync stage when `sync_binlog=1`.

Every case is handled by one of four rules. Which rule applies depends on whether a BOLT blocker applies, whether the error is fatal, whether the cache is still usable, and on which side of the externalization point the failure happens.

1. When a BOLT blocker applies, the transaction uses the standard commit path. This is not a failure. The blocker is counted and logged as a fallback reason (section 6.2).
2. When an error is not fatal and the cache is still usable, before or after the externalization point, the transaction falls back to the standard commit path. Any session error is cleared or kept out of the session, and the fallback reason is recorded. In recovery, the fallback is reading the transaction sequentially.
3. Before the externalization point, a fatal error or a cache that cannot be trusted ends the statement or the transaction with the error. A transaction rollback is safe, because nothing has left the server.
   - When the cache cannot be truncated back at a rollback, after a failed statement or at ROLLBACK TO SAVEPOINT, an incident is recorded instead. The cache's content is discarded, and an incident event is written in place of the transaction.
4. After the externalization point:
   - A non-fatal rotation failure is handled by `binlog_error_action`, as any flush-stage error is. The standard commit path then has no binlog file to write into, so no fallback is used. No transaction rollback is used either, because the transaction may already have left the server.
   - A fatal error aborts the server, whatever `binlog_error_action` is. Neither a fallback nor a transaction rollback is safe after a fatal error.

The tables below list every failure in the order the steps run, with the rule that applies, the outcome, and how the standard commit path handles the same kind of failure.

Before the externalization point:

| # | When | Failure | Rule | Outcome | Standard commit path |
|---|---|---|---|---|---|
| 1 | First event | A BOLT blocker applies: the optimization is OFF, or there is an incident, statement format, encryption or compression. | 1 | Not a failure. The transaction uses the standard commit path. When the optimization is OFF, nothing is counted. For the other blockers, the fallback is counted if the cache spilled and the transaction ends above the threshold. Statement format, encryption and compression are logged once per server run; an incident is logged each time. | No such step. |
| 2 | First event | A cache left in the binlog file form by a failed reset cannot be replaced (out of memory). | 3 | The statement fails with out of memory. | Out of memory while writing the binlog cache also fails the statement. |
| 3 | Rewrite of the cache into binlog file form, at the first spill | The spill file cannot be created, or the copy fails. | 2 | The transaction falls back. The original cache is intact and spills to `tmpdir`. `ER_BINLOG_BOLT_REWRITE_FAILED` logs the cause. | No rewrite step. A failure of the spill to `tmpdir` is handled as on the standard commit path. |
| 4 | Rewrite of the cache into binlog file form, at the first spill | A fatal error. | 3 | The statement fails with the error. | A fatal error while writing the binlog cache also fails the statement. |
| 5 | During the transaction, after the cache was rewritten into binlog file form | A write to the rewritten cache fails (size limit, disk full, I/O). | 3 | The statement fails. There is no fallback, because the rewritten cache holds the only copy of the events. | The same code path is used. `report_cache_write_error()` fails the statement. |
| 6 | During the transaction, after the cache was rewritten into binlog file form | A truncation at a rollback fails. | 3 | An incident is recorded. At commit, the file is not promoted. The cache's content is discarded, and an incident event is written instead of the transaction. Rolled-back events never reach the binlog, and replicas stop at the incident. | `binlog_cache_data::truncate()` ignores the failure. BOLT is stricter on purpose, because a rewritten cache may be promoted. |
| 7 | Commit, before the `before_commit` hook | A BOLT commit-time blocker applies. | 1 | Not a failure. The transaction uses the standard commit path. | No such step. |
| 8 | Commit, before the `before_commit` hook | The spilled file's flush, truncation or fsync fails. | 3 | The commit returns `RESULT_ABORTED`. The caller rolls the transaction back, and COMMIT fails with the error (`ER_ERROR_ON_WRITE` if none was raised). The error log has `ER_BINLOG_BOLT_SPILL_FILE_SYNC_FAILED`, which names the file and says what to check. | The same as a failed `finalize()`, which returns `RESULT_ABORTED`. |

After the externalization point:

Rows 10 to 14 happen during the rotation that promotes the spilled file.

| # | When | Failure | Rule | Outcome | Standard commit path |
|---|---|---|---|---|---|
| 9 | Flush stage, before the rotation | The binlog is closed, encryption is ON, or the checksum changed. | 1 | Not a failure. The binlog is not rotated, and the transaction is written into the active binlog file through the standard commit path. The fallback is counted and logged. | No such step. |
| 10 | Flush stage, rotation, saving the GTIDs | The GTID table cannot be written. | 2 | Fall back to writing into the active binlog file, which the rotation keeps when the file is below `max_binlog_size`. At or above it, the rotation fails (row 11). | The error is also logged, and the active file is kept when it is below `max_binlog_size`. |
| 11 | Flush stage, rotation, before the new binlog file is opened | A non-fatal failure that row 10 does not cover, for example a failure to flush the active binlog file, or to save the GTIDs when the active binlog file is at `max_binlog_size`. | 4 | It is a rotation error, and `binlog_error_action` applies. | The same as any flush-stage error. |
| 12 | Flush stage, rotation, any step | A fatal error. This includes a failure to write the Rotate event into the active binlog file, and every failure while the new binlog file is opened and prepared (row 14). `new_file_impl()` raises these failures as fatal. | 4 | The server aborts through `exec_binlog_error_action_abort()`, whatever `binlog_error_action` is. The error log message gives the fatal error's text, says why the server aborts, and says to fix the cause before restarting the server. | No such rule. On the standard commit path, every flush-stage error follows `binlog_error_action`. |
| 13 | Flush stage, rotation, in `promote_spilled_file()`, before the rename | A non-fatal failure: binlog encryption was turned ON or the checksum changed, the reserved region is too small, the new file's header events cannot be flushed or read, the header block cannot be built, written or synced, or the rename fails. | 2 | The transaction falls back and is written into the new binlog file. The cache is still usable, because none of these failures changes the data region of the spilled file, which the fallback copies. The session error is logged (`ER_BINLOG_BOLT_PROMOTION_ROTATION_ERROR`) and cleared first. | No such step. |
| 14 | Flush stage, rotation, in `promote_spilled_file()`, after the rename | The directory sync or the reopen fails. | 4 | `new_file_impl()` raises the failure as fatal, so the server aborts, as in row 12. | The same error follows `binlog_error_action` on the standard commit path. |
| 15 | After commit, reset | The replacement standard-form cache cannot be allocated. | – | The transaction has committed. `Dummy_error_handler` keeps the error out of the session, and `ER_BINLOG_BOLT_CACHE_STANDARD_FORM_RESTORE_FAILED` is logged. The THD's fatal flag stays set until the next command. The next transaction's first event retries the replacement. If it fails again, that statement fails with out of memory (row 2), so the error is still reported, and there is no inconsistency or data loss. | The behaviour is the same. A failed reset logs `ER_BINLOG_CANT_RESIZE_CACHE`. |

Recovery and the GTID scan, outside the commit:

| # | When | Failure | Rule | Outcome | Standard commit path |
|---|---|---|---|---|---|
| 16 | Recovery and GTID scan | The header body is not decoded, or the terminating event cannot be sought, read or matched. | 2 | The transaction is read sequentially. For an invalid header, crash recovery logs `ER_BINLOG_BOLT_INVALID_LARGE_TRX_HEADER`; the GTID scan logs nothing. A header in a newer format version is read past without a message. | No such step. |
| 17 | Recovery and GTID scan | The reader cannot seek back after the header, or cannot seek to the verified terminating event. | – | It is a read error. Crash recovery treats the binlog as malformed, and the GTID scan fails. | `Log_sanitizer` also treats a read error as a malformed binlog. |
