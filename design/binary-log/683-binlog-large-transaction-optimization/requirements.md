## Requirements

### Functional requirements

**FR1**. The optimization must create a dedicated directory named `#binlog_temp_files` for storing session binlog cache temp files, located in the same directory as the binlog files so that promoting a temp file into the binlog sequence is a metadata-only operation on the same filesystem.

**FR2.** The optimization must create the `#binlog_temp_files` directory during binlog initialization at startup, after the binlog directory is known and before the server accepts connections.

**FR2.1.** If `#binlog_temp_files` already exists at startup, the server must clean up its managed temp files according to FR28.5 through FR28.8.

**FR3**. The optimization must exclude the `#binlog_temp_files` directory from schema-visible listings, such as `SHOW DATABASES` and `information_schema`, so that it does not appear as a schema.

**FR4.** The optimization must assign temp files unique filenames to avoid conflicts.

**FR5.** The optimization must provide a global, dynamically settable system variable of boolean type, `binlog_large_transaction_optimization_enabled`, that turns the optimization ON or OFF, with a default of ON. Changes must take effect for subsequent transactions across all existing and new sessions without a server restart.

**FR5.1.** When `binlog_large_transaction_optimization_enabled` is set to OFF, every transaction must commit through the standard code path.

**FR6**. The optimization must provide a global, dynamically settable threshold, `binlog_large_transaction_optimization_threshold`, that controls the spilled size above which a transaction is committed through the optimization's code path. Changes must take effect for subsequent transactions across all existing and new sessions without a server restart.

**FR6.1.** The threshold is only evaluated when `binlog_large_transaction_optimization_enabled` is set to ON. When the optimization is disabled, no transaction qualifies for the optimized code path regardless of the threshold value.

**FR7**. The optimization must use a default `binlog_large_transaction_optimization_threshold` of 128 MB, which must be appropriate for most workloads without further configuration.

**FR8**. The optimization must enforce a minimum `binlog_large_transaction_optimization_threshold` of 10 MB. Values below 10 MB must be rejected.

**FR9.** The optimization must commit a transaction through the optimization's code path when the transaction has spilled its binlog cache to a temp file and the spilled size exceeds `binlog_large_transaction_optimization_threshold`.

**FR9.1.** When a transaction qualifies for the optimization, the commit must be finalized by promoting the temp file into the binlog sequence. The promoted file is renamed to the next sequential binlog file name (e.g., if the active binlog is `mysql_bin.000005`, the promoted file becomes `mysql_bin.000006`). The promoted file must then become the active binlog file and must use the existing `max_binlog_size` rotation behavior.

**FR9.2.** Promoted files must be treated identically to standard binlog files with respect to `FLUSH BINARY LOGS`, `PURGE BINARY LOGS`, and `RESET BINARY LOGS AND GTIDS` . They appear in `SHOW BINARY LOGS`, are eligible for purge based on retention policy, and are removed on reset.

**FR9.3.** When the optimization is enabled, the effective value of `binlog_large_transaction_optimization_threshold` is set to the maximum of `binlog_large_transaction_optimization_threshold` and `binlog_cache_size`. This constraint is enforced upon engine startup and on every SET statement that alters either variable.

**FR9.4.** When the optimization is enabled, if `binlog_large_transaction_optimization_threshold` is set to a value less than `binlog_cache_size`, the threshold is adjusted to match `binlog_cache_size` and the server must emit a warning indicating the adjustment.

**FR9.5**. When the optimization is enabled, if `binlog_cache_size` is set to a value greater than `binlog_large_transaction_optimization_threshold`, the threshold is adjusted to match `binlog_cache_size` and the server must emit a warning indicating the adjustment.

**FR10.** When a transaction has not spilled to a temp file, or its spilled size does not exceed `binlog_large_transaction_optimization_threshold`, the optimization must commit it through the standard code path and produce a binary log identical to what the standard path produces today.

**FR11.** The optimization must not affect GTID generation or sequencing. The promoted file's transaction retains the GTID sequencing, which is assigned at commit time.

**FR12**. The optimization must work correctly with both XA transactions (external XID) and non-XA transactions (internal XID).

**FR13.** The optimization must introduce a `Large_transaction_header_event` binlog event type whose purpose is to speed up recovery for large transactions by allowing the server to locate the terminating event directly, skipping reading the payload of the large transaction.

**FR13.1.** The optimization must write a `Large_transaction_header_event` into the promoted binlog file, recording the offset, type, and identity of the large transaction's terminating event. The terminating event contains either an internal XID (for DML and DDL transactions) or an external XID (for XA PREPARE and XA COMMIT ONE PHASE transactions).

**FR14**. The optimization must compute the transaction's GTID at commit time and write the `Gtid_log_event`, write the `Previous_gtids_log_event` at the start of the file, and set the commit dependency tracking (`last_committed` / `sequence_number`) as when rotating to a new binlog file.

**FR15**. The optimization must delete the temp file with no trace left in the binlog or the binlog index when a transaction is rolled back before promotion, since a rolled-back transaction is never written to the binlog file.

**FR16.** The optimization must handle `ROLLBACK TO SAVEPOINT` which truncates the temp file to the byte offset recorded when the savepoint was established.

**FR16.1.** When the optimization spills a large transaction to a temp file, all existing savepoint offsets must be adjusted to account for the reserved header space.

**FR16.2.** When a savepoint rollback is activated and the remaining transaction size is larger than `binlog_large_transaction_optimization_threshold` then the spilled file is maintained and the optimization continues. Otherwise, it falls back to the standard code path of copying the transaction into the existing binlog file.

**FR17.** The optimization must evaluate fallback conditions and commit the transaction through the standard code path if any of the conditions are met, emit a diagnostic message, and increment `binlog_large_transaction_optimization_missed_count`.

**FR17.1.** The fallback conditions are: (1)`binlog_format` is not ROW; (2) the reserved header space is insufficient for the required header events; (3) binlog encryption is enabled;(4) binlog transaction compression is enabled; (5) `binlog_checksum` changed while the transaction was in progress, so the transaction's cached events do not match the current checksum configuration; (6) a single statement updated both transactional and non-transactional tables.

**FR17.2**. A fallback transaction must produce the same event sequence in the active binlog as a transaction written through the standard code path. It achieves this by overriding the spilled `log_pos` and recomputing the checksum.

**FR18.** The optimization must be safe against concurrent binlog rotation. The promotion workflow computes the binlog sequence number and writes header events while holding `LOCK_log`, ensuring no concurrent rotation can invalidate the promoted file's position in the sequence.

**FR19.** The optimization must not change the existing binlog crash recovery flow.

**FR19.1.** As an exception to FR19 during recovery the optimization may skip reading a large transaction's payload by using the terminating-event offset recorded in `Large_transaction_header_event` to locate the terminating event directly. This reduces recovery time to constant time regardless of transaction size.

**FR19.2.** If the server crashes before promotion completes, recovery must discard the temp file and roll back the transaction.

**FR19.3.** If the server crashes after promotion completes but before InnoDB commit completes, recovery must roll forward the transaction.

**FR19.4.** If the server crashes after both promotion and InnoDB commit complete, the transaction must be durable with no data loss.

**FR19.5.** Before adding a GTID to the executed set during optimized recovery, recovery must verify that the recorded offset identifies a complete binlog event whose event type and XID match the values recorded in `Large_transaction_header_event`.

**FR19.6.** If the verification required by FR19.5 fails, recovery must use the standard sequential scan for a large transaction.

**FR19.7.** When `source_verify_checksum` is ON, recovery must use the standard sequential scan for a large transaction.

**FR20.** The optimization must ensure that a replica ignores the `Large_transaction_header_event` when it receives it, since the event is not relevant to the replica. This is achieved by marking `Large_transaction_header_event` as an ignorable event per the MySQL replication protocol.

**FR21.** `START REPLICA UNTIL` must function identically regardless of whether the transaction was committed through the optimization or the standard code path.

**FR22.** When `binlog_format` is set to STATEMENT or MIXED, binlog encryption is enabled, or binlog transaction compression is enabled, the optimization must emit a warning to the error log indicating that the optimization is disabled and transactions will fall back to the standard code path.

**FR23**. The optimization must expose a status variable, `binlog_large_transaction_optimization_count`, reporting the number of transactions that successfully executed the optimization's code path since server startup.

**FR24**. The optimization must expose a status variable, `binlog_large_transaction_optimization_missed_count`, reporting the number of transactions that exceeded `binlog_large_transaction_optimization_threshold` but could not execute the optimization's code path since server startup.

**FR25**. The optimization must expose `binlog_large_transaction_optimization_count` and `binlog_large_transaction_optimization_missed_count` through `SHOW GLOBAL STATUS` and `performance_schema.global_status`.

**FR26.** The optimization must not change the output of `SHOW BINARY LOGS`, `SHOW BINLOG EVENTS`, or `SHOW BINARY LOG STATUS`. Promoted files appear in these statements identically to standard binlog files.

**FR27.** The optimization must not change the output of the `performance_schema.log_status` table.

**FR28.** The optimization must store its temp files on the same filesystem as the binlog files. [LTO-V3-R1]

**FR28.1.** Operators must account for the additional binlog-volume space consumed by concurrent large transactions, which previously resided in the system `tmpdir`.

**FR28.2.** At startup, if the `#binlog_temp_files` path exists but is not a directory (e.g., a symlink or regular file), the server must log an error and disable the optimization.

**FR28.3.** If directory creation or permission acquisition fails at startup, the server must log an error and disable the optimization.

**FR28.4.** If disk space is exhausted while writing to a temp file during transaction execution, the write error is handled identically to a disk-full error on the standard binlog cache temp file.

**FR28.5.** A temp file created by the optimization must have a name matching the pattern `bolt_<unique_id>`, where `<unique_id>` is a lowercase identifier unique within `#binlog_temp_files` (BOLT stands for "Binlog Optimization for Large Transaction").

**FR28.6.** Startup cleanup must accept for deletion only regular files whose basename matches the optimization's temp file naming pattern and must reject all other directory entries.

**FR28.7.** Startup cleanup must delete each accepted file.

**FR28.8.** If startup cleanup cannot delete an accepted file, the server must log `ER_BINLOG_CANT_DELETE_FILE` from MYSQL_BIN_LOG and disable the optimization.

**FR29**. The optimization may cause binlog files to be rotated before they reach `max_binlog_size`. Because promoting a temp file inserts it into the binlog sequence as a new binlog file and forces a rotation, the previously active binlog file is closed early, potentially well below `max_binlog_size`, if a large transaction is issued while little or no concurrent workload is writing to the active file. This results in smaller than configured binlog files around large transactions.

**FR29.1.** When multiple concurrent transactions each qualify for the optimization, each transaction is committed to its own promoted binlog file. The promoted files may be smaller than `max_binlog_size`. Each promoted file becomes part of the binlog sequence independently.

**FR30**. The optimization may produce a binlog file that contains transactions other than the large transaction. Although the large transaction is generally the only transaction in the promoted binlog file, subsequent small transactions can also be written into it if the file's size is still below `max_binlog_size` before the next rotation occurs.

### Non-functional requirements

**NFR1**. The optimization must not impact the durability invariant: InnoDB must never mark a large transaction executing on the optimization's code path as committed unless the corresponding promoted binlog file is durable on disk.

**NFR2**. The optimization must preserve `sync_binlog` durability semantics. Before a temp file is promoted into the binlog sequence, it must be synced to durable storage, so that promotion does not weaken the durability guarantees provided by `sync_binlog`.

**NFR3**. The optimization must still recover consistently if MySQL users manually delete files from `#binlog_temp_files` at recovery time, since any file remaining in that directory is a temp file whose promotion into the binlog sequence did not complete, so its transaction was never committed.

**NFR4**. The optimization must complete commit in constant time under `LOCK_log` regardless of transaction size.

**NFR5**. The optimization must complete binlog crash recovery in constant time regardless of transaction size.

**NFR6**. The optimization must not degrade performance for transactions that do not exceed `binlog_large_transaction_optimization_threshold`.

**NFR7**. The optimization must be safe under concurrent access, including multiple sessions spilling to their own temp files, concurrent commits from both the optimized and standard paths, and concurrent rotations.

**NFR8**. The optimization must not require changes to the MySQL client-server protocol or the replication protocol. The `Large_transaction_header_event` must be of ignorable event type, so it does not impact compatibility with existing replicas or binlog tooling.

**NFR9.** The optimization must be transparent to backup tools. Promoted binlog files must be readable and processable by these tools identically to standard binlog files, with no special handling required.

**NFR10.** The optimization must ensure that on downgrade to a version that does not recognize `Large_transaction_header_event`, the server safely skips the event because it is marked as ignorable.

**NFR11.** The optimization must not require special migration steps on upgrade, since it only produces `Large_transaction_header_event` for new transactions committed after the upgrade. Binlog files produced before the upgrade remain unchanged and fully compatible.

**NFR12**. The optimization must create temp files with the same file permissions and ownership as binlog files, since they contain equally sensitive data and reside alongside the binlog files.
