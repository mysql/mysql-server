## Requirements

### Functional requirements

#### The binlog temp files directory and the spilled files

**FR1.** The optimization must store the spilled files that may be promoted in a dedicated directory, `<binlog directory>/#binlog_temp_files/<binlog base name>`, so that promoting a file into the binlog sequence is a rename on the same filesystem. Servers that share a binlog directory have different base names, so each server uses only its own subdirectory. A binlog cache that is not rewritten into the binlog file form spills into `tmpdir`, as without the optimization.

**FR2.** At startup, when the binlog is ON, the server must check the directory and clean up leftover spill files according to FR5.5 through FR5.8, regardless of the optimization's setting. When the optimization is ON, the server must create the directory before it accepts connections.

**FR2.1.** Setting `binlog_large_transaction_optimization_enabled` to ON must create the directory if it does not exist.

**FR3.** The `#binlog_temp_files` directory must not appear as a schema, for example in `SHOW DATABASES` or `information_schema`.

**FR4.** Every spilled file must get a unique name.

**FR5.** The spilled files must be on the same filesystem as the binlog files.

**FR5.1.** Operators must account for the binlog volume space that concurrent large transactions use. Without the optimization, these spilled files are in `tmpdir`.

**FR5.2.** At startup, if the directory path is not a directory or is a symbolic link, the server must log an error and turn the optimization OFF. When the optimization is OFF, the server logs a warning instead.

**FR5.3.** If the directory cannot be created or used at startup, the server must log an error and turn the optimization OFF. When the optimization is OFF, the server logs a warning instead.

**FR5.4.** A write failure on a spilled file during the transaction, such as a full disk, must be handled as on the standard binlog cache. A failure to create the spilled file when the cache is rewritten must fall back to the standard form.

**FR5.5.** A spilled file must be named `bolt_<server start time>_<serial>`, with lowercase hexadecimal numbers.

**FR5.6.** Startup cleanup must delete only regular files whose names match `bolt_` followed by letters, digits and `_`. Every other entry must be left in place with a warning.

**FR5.7.** Startup cleanup must delete each matching file and log how many it removed.

**FR5.8.** If startup cleanup cannot delete a matching file, the server must log a warning and leave the file in place. The file is never reused, and startup continues.

#### System and status variables

**FR6.** The optimization must provide a global, dynamic boolean system variable, `binlog_large_transaction_optimization_enabled`, with a default of ON. A transaction must use the value it has at the transaction's first event.

**FR6.1.** When `binlog_large_transaction_optimization_enabled` is OFF, every transaction must commit through the standard commit path.

**FR6.2.** Setting `binlog_large_transaction_optimization_enabled` to ON must fail, with a warning, when the binlog temp files directory cannot be used.

**FR7.** The optimization must provide a global, dynamic threshold, `binlog_large_transaction_optimization_threshold`. A transaction whose binlog cache size, without checksums, exceeds the threshold commits through the optimization's path. A transaction must use the value it has at the transaction's first event.

**FR7.1.** The threshold has no effect while `binlog_large_transaction_optimization_enabled` is OFF.

**FR8.** The default threshold must be 128 MiB.

**FR9.** The minimum threshold must be 10 MiB. A smaller value must be raised to 10 MiB, with a warning.

**FR9.1.** The threshold must never be smaller than `binlog_cache_size`, whether the optimization is ON or OFF. This must hold at startup and after every change to either variable.

**FR9.2.** Setting the threshold below `binlog_cache_size` must raise it to `binlog_cache_size`. While the optimization is ON, the server must also emit a warning to the client and the error log.

**FR9.3.** Setting `binlog_cache_size` above the threshold must raise the threshold to match. While the optimization is ON, the server must also emit the same warning.

**FR10.** The optimization must expose the status variable `Binlog_large_transaction_optimization_count`: the number of spilled files promoted since server startup.

**FR11.** The optimization must expose the status variable `Binlog_large_transaction_optimization_missed_count`: the number of transactions that exceeded the threshold with the optimization ON but committed through the standard commit path, since server startup.

**FR12.** Both status variables must appear in `SHOW GLOBAL STATUS` and `performance_schema.global_status`.

#### Eligibility and the binlog cache

**FR13.** A transaction must be eligible for the optimization at its first event only when the optimization is ON, no incident is pending, its first statement is logged in row format, binlog encryption is OFF and binlog transaction compression is OFF. Only an eligible transaction's cache is rewritten at its first spill.

**FR14.** `max_binlog_cache_size` must limit the bytes a transaction's binlog cache stores, including the checksum footers of a rewritten cache. The statement that crosses the limit must fail as on the standard commit path.

#### Commit and promotion

**FR15.** A transaction must commit through the optimization's path when it was eligible at its first event (FR13), its binlog cache was rewritten into the binlog file form at its first spill, its binlog cache size, without checksums, exceeds the threshold, and no fallback condition applies (FR26.1).

**FR15.1.** Such a transaction's commit must promote its spilled file into the binlog sequence as the next binlog file (for example, if the active binlog is `mysql_bin.000005`, the promoted file becomes `mysql_bin.000006`). The promoted file then becomes the active binlog file, and the existing `max_binlog_size` rotation applies to it.

**FR15.2.** Promoted files must be treated like any binlog file by `FLUSH BINARY LOGS`, `PURGE BINARY LOGS` and `RESET BINARY LOGS AND GTIDS`. They appear in `SHOW BINARY LOGS`, are purged by the retention policy, and are removed on reset.

**FR15.3.** A promotion rotates the binlog, so it must purge expired binlog files as any rotation does, also when the promoted file is below `max_binlog_size`.

**FR16.** A transaction that never spilled, or whose binlog cache size, without checksums, does not exceed the threshold, must commit through the standard commit path and produce the binlog that the standard commit path produces.

**FR17.** The optimization must not affect GTID generation or ordering. The GTID of a transaction whose spilled file is promoted is assigned at commit.

**FR18.** The optimization must work with XA transactions (external XID) and with transactions that use an internal XID.

**FR19.** The optimization must write the promoted file's `Previous_gtids_log_event` and the transaction's `Gtid_log_event` at commit, and set the dependency tracking (`last_committed` / `sequence_number`) as for a new binlog file.

**FR20.** The promotion must be safe against concurrent rotation. It must run inside the binlog rotation, under `LOCK_log`.

**FR21.** The spilled file must be flushed and synced before the `before_commit` hook. A failure must roll the transaction back, return the error to the client, and log a warning that names the file. No other server may receive the transaction.

**FR22.** A promotion rotates the binlog, so the previously active binlog file can be closed below `max_binlog_size`.

**FR22.1.** Concurrent large transactions each get their own promoted binlog file.

**FR23.** A promoted binlog file can hold other transactions after the large transaction: the other members of its commit group, and later transactions until the next rotation.

#### Rollback

**FR24.** When a transaction is rolled back, its spilled file must be deleted, with no trace in the binlog or the binlog index.

**FR25.** `ROLLBACK TO SAVEPOINT` and the rollback of a failed statement must truncate the cache to the rollback point, also after the cache was rewritten.

**FR25.1.** Rollback points must be logical positions in the standard form. A rewritten cache must map a rollback point to its physical offset without scanning the cache.

**FR25.2.** Whether a transaction's spilled file is promoted after a rollback must be decided at commit, by its binlog cache size, without checksums.

**FR25.3.** When a rewritten cache cannot be truncated at a rollback, the server must record an incident, so that rolled-back events never reach the binlog.

**FR25.4.** After a rollback to the start of the transaction, the cache must be as at the start of a transaction: the next event decides eligibility again.

#### Fallbacks and error handling

**FR26.** When a fallback condition applies, the transaction must commit through the standard commit path, and the server must increment `Binlog_large_transaction_optimization_missed_count` and log the reason. The one exception is binlog transaction compression turned ON in the middle of a statement, before the cache spills. The transaction is then compressed and not counted.

**FR26.1.** The fallback conditions are:
1. the transaction contains statement-format events;
2. the transaction also changed a non-transactional table;
3. binlog encryption is ON;
4. binlog transaction compression is ON;
5. `binlog_checksum` changed while the transaction was running;
6. the reserved header region cannot hold the header events;
7. a logging incident is pending;
8. the binlog was closed while the transaction was running;
9. the cache could not be rewritten into the binlog file form;
10. `mysql.gtid_executed` cannot be written at the rotation, and the rotation keeps the active binlog file;
11. a non-fatal error before the spilled file replaces the new binlog file.

**FR26.1.1.** If binlog transaction compression is turned ON after the cache was rewritten into the binlog file form, the transaction is written to the binlog uncompressed. This is a known limitation.

**FR26.2.** A transaction that falls back must produce the same events in the binlog as on the standard commit path. Its cache is read back in the standard form, and the binlog writer sets `log_pos` and the checksum.

**FR26.3.** A fallback must not raise a client error or warning.

**FR27.** A fallback for a configuration reason (statement-format events, a non-transactional change, binlog encryption, binlog transaction compression) must be logged once per server run. Later fallbacks for the same reason must only be counted.

**FR28.** The optimization must follow these error-handling rules. The externalization point is the `before_commit` hook.
1. When a fallback condition of FR26.1 applies, the transaction must use the standard commit path, and the blocker must be counted and logged as a fallback reason.
2. When the error is not fatal and the cache is still usable, the transaction must fall back to the standard commit path, and the fallback reason must be recorded.
3. Before the externalization point, a fatal error or a cache that cannot be trusted must end the statement or the transaction with the error.
4. After the externalization point, a non-fatal rotation failure must be handled by `binlog_error_action`, and a fatal error must abort the server, regardless of the value of `binlog_error_action`. A failure to write the Rotate event, or to open and prepare the new binlog file, is fatal.

**FR28.1.** Every error-path message must say what happened, what the server did, and what the user can check.

#### The large transaction header event and recovery

**FR29.** The optimization must introduce an ignorable `Large_transaction_header_event` binlog event type that lets recovery locate a large transaction's terminating event and skip its body.

**FR29.1.** The optimization must write a `Large_transaction_header_event` into the promoted binlog file, recording the offset, type and body of the transaction's terminating event. The body is the internal XID of an `Xid_log_event`, or the one-phase flag and the external XID of an `XA_prepare_log_event`. For any other terminating event the body is empty, and recovery reads the transaction sequentially.

**FR30.** The optimization must not change the binlog crash recovery flow, except as FR30.1 allows.

**FR30.1.** Recovery may skip a large transaction's body by using the terminating event offset in `Large_transaction_header_event`.

**FR30.2.** If the server crashes before the promoted file is added to the binlog index, recovery must roll the transaction back, and startup must remove the spilled file.

**FR30.3.** If the server crashes after the promoted file is added to the binlog index and before the engine commit completes, recovery must commit the transaction.

**FR30.4.** If the server crashes after the engine commit, the transaction must be durable with no data loss.

**FR30.5.** Before it skips a body, recovery must verify that the event at the recorded offset is complete and has the recorded type, length and body.

**FR30.6.** If the verification of FR30.5 fails, recovery must read the transaction sequentially. If the reader cannot return to its position after the header, recovery must report an error, because it cannot continue from an unknown position.

**FR30.7.** The skip applies to binlog crash recovery and to GTID reconstruction from binlogs, only when `source_verify_checksum` is OFF. With the option ON, recovery reads the transaction sequentially, so every event's checksum is verified. GTID reconstruction skips only when it collects all GTIDs of the file. Relay log recovery and GTID reconstruction from relay logs read transactions sequentially, because the header's offset refers to the source's binlog.

**FR30.8.** A `Large_transaction_header_event` whose body is in a newer format version, or is malformed, must not stop any reader. Recovery must read the transaction sequentially.

#### Replication and compatibility

**FR31.** A replica must ignore the `Large_transaction_header_event`. The event is marked ignorable.

**FR32.** `START REPLICA UNTIL` must work the same for transactions in promoted binlog files and in other binlog files.

**FR33.** Promoted files must appear in `SHOW BINARY LOGS` and `SHOW BINARY LOG STATUS` like any binlog file. `SHOW BINLOG EVENTS` shows a promoted file's `Large_transaction_header_event`.

**FR34.** The optimization must not change the output of `performance_schema.log_status`.

**FR35.** The optimization must work with Group Replication and semi-synchronous replication. Readers of the binlog cache must see the standard form.

### Non-functional requirements

#### Durability and crash safety

**NFR1.** InnoDB must never mark a transaction whose spilled file is promoted as committed unless the promoted binlog file is durable.

**NFR2.** The optimization must preserve `sync_binlog` durability. The spilled file must be synced before the flush stage, and the header block before the rename. After the rename, the binlog directory and the temp files directory must be synced before the promoted file is added to the binlog index.

**NFR3.** Recovery must stay consistent if users delete files from the temp files directory. Any file there belongs to a transaction whose promotion did not complete, so the transaction was not committed.

#### Performance

**NFR4.** The promotion must do work under `LOCK_log` that does not depend on the transaction's size.

**NFR5.** With `source_verify_checksum` OFF, binlog crash recovery must not depend on the size of a transaction whose spilled file is promoted.

**NFR6.** The optimization must not degrade performance for transactions that do not exceed the threshold.

#### Concurrency

**NFR7.** The optimization must be safe under concurrent access: many sessions spilling to their own files, concurrent commits on both paths, and concurrent rotations.

#### Compatibility

**NFR8.** The optimization must not change the client-server protocol or the replication protocol. The `Large_transaction_header_event` must be ignorable, so that existing replicas and binlog tools work.

**NFR9.** Promoted binlog files must be readable by backup and binlog tools like any binlog file.

**NFR10.** A server version that does not know `Large_transaction_header_event` must skip it, because it is ignorable.

**NFR11.** The optimization must need no migration on upgrade. Binlog files written before the upgrade stay unchanged.

#### Security

**NFR12.** Spilled files must have the permissions and ownership of binlog files, because they hold equally sensitive data.
