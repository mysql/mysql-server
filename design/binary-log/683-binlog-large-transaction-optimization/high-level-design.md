# Binlog Optimization for Large Transaction - Proposal

**Main Contributor:** Xueting(Hillary) Wu\
**Secondary Contributors:** Omar Farhat, Ali Bhagat, Michael Dowling, Bob Yang, Kevin Yang, Derrick Yang, Zongkai Xia

## 1. High-Level Description

### Executive summary

After this feature is implemented, MySQL server will stop exhibiting commit stalls when committing large transactions and will also achieve constant time recovery for large transactions. This allows applications with workloads containing large transactions to have an improved MySQL experience.

Large transactions present a significant performance challenge for MySQL's binary logging subsystem. When a transaction exceeds the in-memory binlog cache (controlled by `binlog-cache-size`), events spill to a local temp file. At commit time, copying the entire file contents to the binlog file while holding `LOCK_log` takes longer as the transaction size increases. This inflates commit latency, stalls concurrent commits, and widens the crash window. If the server crashes while copying the transaction, recovery must scan the entire binlog file to identify incomplete transactions, a process that can take hours for multi-gigabyte transactions.\
\
We propose an optimization where large transactions spill binlog events directly to a dedicated temp file that is structurally identical to a binlog file, and finalize the commit by promoting that file into the binlog sequence, eliminating the bottleneck entirely. Crash recovery also becomes constant time regardless of the last binlog file's size, since recovery skips scanning the large transaction's payload.\
\
This optimization has been running in production in Amazon Aurora MySQL since 2020, where it reduced large transaction commit latency by 6x and brought P99 binlog crash recovery time to under one minute. MariaDB, inspired by Aurora MySQL, implemented a similar optimization ([MDEV-32014](https://github.com/MariaDB/server/commit/fba09f8ccb9ad55a86349bc9586b048c775d5205)), validating the design.\


### User / developer stories

As a MySQL user performing workloads that produce large transactions, such as bulk data loads or large batch updates, I want binary logging to not impose an additional performance penalty on large transaction commits, so that a large transaction's commit does not delay the commits of other concurrent transactions.

As a MySQL user performing workloads that produce large transactions, such as bulk data loads or large batch updates, I want binlog recovery of a large transaction to stay fast, so that my downtime stays short and predictable regardless of transaction size.

As a MySQL user running a mixed workload, I want large transactions to benefit from the optimization while non large transactions continue committing with the same latency and behavior, so that I can adopt this feature without risk to my everyday workload.

As a MySQL user, I want my applications to remain unchanged when this feature is enabled. The binary log produced by this optimization must remain fully compatible with my replicas, point in time recovery, and binlog based tools, so that I can adopt it without changing my replication topology or existing workflows.

As a MySQL user, I want observability into the optimization, including whether it is active and how often it is triggered, so that I can confirm it is behaving as expected and correlate it with the performance I am seeing.

### In scope

- Introduce a new commit code path for large transactions, which can execute concurrently alongside transactions using the standard commit path.
- Improve recovery performance through the introduction of a new event, `Large_transaction_header_event`, that allows recovery to skip scanning the transaction payload.
- Provide a knob to enable and disable the optimization.
- Expose observability metrics for the feature.
- Emit error logs when the optimization encounters failures.

### Out of scope/Limitations

Setting `binlog_format` to `STATEMENT` or `MIXED`, enabling binlog encryption, enabling group replication or enabling binlog transaction compression is not supported by the optimization. When a transaction that would otherwise qualify for the optimization (its spilled size exceeds `binlog_large_transaction_optimization_threshold`) runs under any of these configurations, it falls back to the standard commit path, the server emits a warning to the error log indicating that the optimization is disabled for that transaction, and the `binlog_large_transaction_optimization_missed_count` status variable is incremented.

## 2. High-Level Design

The idea behind this optimization is simple: when the in-memory binlog cache spills to a temp file on disk, instead of copying that temp file back into the binlog at commit, we make the temp file look like a real binlog file from the start and, at commit, promote it directly into the binlog sequence.

The expensive copy under `LOCK_log` disappears. The standard code path also has a crash-recovery cost: its lengthy copy widens the window in which a restart or crash can occur during commit, and when a crash does happen, recovery must scan the entire transaction to locate its end, making recovery time proportional to transaction size. The optimization does the opposite. It shrinks that window, and even when a large transaction is present, recovery can skip scanning its payload entirely, keeping recovery time constant regardless of transaction size.

### Write and Commit Code Path

As a transaction runs, its binlog events are generated and held in the binlog cache. When the cache grows past `binlog_cache_size`, it spills to a temp file on disk. This temp file contains one or more binlog events belonging to that transaction. This already happens today. Our approach reuses that spilled file directly: instead of copying it into the binlog at commit, we treat the spilled file itself as a binlog file and promote it into the sequence.\
\
There is a problem with treating it as-is. Every binlog event header carries a `log_pos`, the offset of the end of that event within its binlog file. Today, when events are written to the cache and spilled to the temp file, `log_pos` is not filled in, because an event's final position is not known until commit, when the cache is copied into the active binlog file and each event's `log_pos` is computed against its destination offset. The copy is where positions get assigned.\
\
So the spilled events are not finalized until the positions are filled-in. To promote it as-is, the optimization would have to revisit every event in the file and rewrite its `log_pos`. This is an expensive operation and will take O(n). This will likely cause the transaction execution to experience a stall when `binlog_large_transaction_optimization_threshold` is exceeded.\
\
Instead, because the promoted binlog file would always have the large transaction as the first transaction, we can assign each event its `log_pos` as it spills. The first event has to begin right after the file's header events: the `Format_description_log_event`, the `Previous_gtids_log_event`, and the transaction's `Gtid_log_event`. We do not know their exact combined size when spilling starts, so we reserve an estimate (the current `Previous_gtids_log_event` size plus about 32 KB of headroom) and assume the first event begins at that offset.\
\
Later on, at commit, the real header events are written into the reserved region. If the estimate was exact, they fill it. If we overestimated, the headers are smaller than the reserved region and leave a gap. To keep the file contiguous and preserve the `log_pos` values we already assigned, we fill that gap with a `Large_transaction_header_event` sized to occupy exactly the remaining bytes, placed so that the `Gtid_log_event` ends right at the reserved offset and the first transaction event begins there.\
\
When a large transaction reaches commit, its events are already in the temp file in final binlog form, so the commit stage copies no data. It performs a fixed amount of work regardless of transaction size.

```
1. Sync the temp file to durable storage (body).
2. Acquire LOCK_log.
3. Compute the binlog sequence number for the temp file.
4. Write the header events into the reserved region of the temp file.
5. Sync the temp file to durable storage.
6. Record the computed binlog sequence number in the purge_index_file.
7. Promote the temp file into the binlog sequence
8. Append a Rotate event to the currently active binlog file, pointing to the promoted file.
9. Add the promoted file to the index file.
10. Delete promoted file entry from purge_index_file.
11. Release LOCK_log.
12. Commit the large transaction in InnoDB.
13. If the promoted binlog file exceeds max_binlog_size, rotate to a new binlog file.
```

Steps 6 through 10 are the same sequence the server already performs for an ordinary binlog rotation. The optimization code path deliberately reuses that existing rotation mechanism so the change stays small and builds on well-tested code.

#### Anatomy of a promoted binlog file

After promotion, the new binlog file is a normal binlog file. Its only distinguishing feature is the `Large_transaction_header_event` at the front and the reserved region it sits in:\


```
+---------------------------------------------------------+
| Binlog magic number                                     |
| Format_description_log_event                            |
| Previous_gtids_log_event                                |
| Large_transaction_header_event (ignorable)              | # new event
| Gtid_log_event                                          |
| Begin                                                   |
| ...                                                     |
| Terminating event (internal or external XID)            |
+---------------------------------------------------------+
```

The fixed header events occupy the front of the reserved region, the `Large_transaction_header_event` consumes whatever they did not use, and the `Gtid_log_event` follows it so that the transaction body begins at exactly the offset assumed while the events were spilling. The wire format of `Large_transaction_header_event` is shown below: [LTO-V3-R2]

```
    +----------------------------------------------------------------+
    |              Common Binlog Event Header (19 bytes)             |
    +----------------------------------------------------------------+
    | timestamp (4)    | type_code (1)     | server_id (4)           |
    | event_length (4) | next_position (4) | flags (2)               |
    |                                      | 0x80 = IGNORABLE_F      |
    +----------------------------------------------------------------+
    |                Post-header (0 bytes)                           |
    +----------------------------------------------------------------+
    |                Event Data Body (variable)                      |
    +---------+------------------+------------------------+----------+
    | Version | XID Event Offset | Terminating Event Type |  Padding |
    | (1 byte)|    (8 bytes)     |       (1 byte)         |  (var)   |
    +---------+------------------+------------------------+----------+
```

Term Event Type (1 byte): The `type_code` of the transaction's terminating event located at XID Event Offset. Valid values are:

- 0x02(2) - `QUERY_EVENT`: DDL transaction.
- 0x10(16) - `XID_EVENT`: DML transaction committed via implicit XA.
- 0x26(38) - `XA_PREPARE_LOG_EVENT`: XA PREPARE or XA COMMIT ONE PHASE transaction.

During crash recovery, the engine seeks to XID Event Offset and compares the event header's `type_code` against the stored Terminating Event Type. A match confirms the transaction is complete and its GTID is added to `gtid_executed`; a mismatch indicates corruption or incomplete write, and the transaction is rolled back.

#### Reserved bytes

Because we assign each event its `log_pos` as it spills, we must finalize the offset where the transaction's first event begins before we know the exact size of the header events that will precede it. We reserve a padding region at the top of the temp file and place the first event just past it. The reservation must hold everything a binlog file begins with: the magic number (4 bytes), the `Format_description_log_event` (about 120 bytes, effectively fixed) and the `Previous_gtids_log_event`, which is the only part that grows with the deployment, `Large_transaction_header_event` and `Gtid_log_event`.\
\
The `Previous_gtids_log_event` encodes the set of GTIDs already present in the binlog. Each GTID records each source UUID with a single contiguous range costs 40 bytes (16 for the UUID, 8 for the range count, 16 for one range), plus 8 bytes for the overall count. A deployment that has seen 10 distinct sources therefore encodes to about 408 bytes, growing with additional ranges per source or with tagged GTIDs.\
\
Because this size is deployment-dependent and changes over time, the reservation is not a static number. We size it as the current `Previous_gtids_log_event` size plus 32 KB of headroom. The current size captures the actual GTID state, and the 32 KB absorbs any growth between the moment the reservation is sized and the moment the transaction commits, along with the small fixed events. The unused portion is padded into the file by the `Large_transaction_header_event`. This overhead is negligible, since a transaction executing the optimization's code path has by definition exceeded `binlog_large_transaction_optimization_threshold` (at least 10 MB), so a few KB of padding is a tiny fraction of the file. If the header still does not fit the reservation, the transaction falls back to the standard code path.

#### Integration with Binary Log Group Commit

When a spilled transaction commits, it takes a dedicated single-commit path that promotes the temp file to a real binlog file. This path acquires and holds `LOCK_log` during promotion, which is the same lock that the standard group commit pipeline acquires during its flush and commit stages, which guarantees that no standard commit, no other spilled commit, and no binlog rotation can interleave with the promotion.

**GTID and Dependency Tracking.** GTID is assigned during the promotion while `LOCK_log` is held. Furthermore, because promoting a spilled transaction creates a new binlog file, the logical clock (used by the replica's parallel applier to determine which transactions can run concurrently) is reset. The spilled transaction is assigned sequence values that indicate the start of a new dependency group. This tells the replica that all prior transactions must finish before this one can be applied, effectively a serialization point.

**Fallback**: When the optimization falls back to the standard commit, the already spilled temp file content is copied back into the active binlog file, following the standard code path.

#### Optimized Recovery

Crash recovery proceeds exactly as it does today, with one addition. When recovery encounters a `Large_transaction_header_event`, it learns two things at once: (1) the transaction is complete and was committed through the optimization's code path, and (2) the offset of the terminating event. Recovery uses that offset to seek directly to the terminating event, collect its XID, and continue, without reading the transaction body. A multi-gigabyte transaction is skipped in a single seek instead of a full scan, which is what makes recovery time independent of transaction size. Everything else in the file is recovered normally.

**Validation**: Before seeking, the offset is checked against the file length. If it exceeds the file length, the fast path is abandoned and the reader continues scanning sequentially from its current position. The event at the seeked position is read through the standard reader, which validates the event header, length, and checksum. Any subsequent transactions in the same file are scanned sequentially after the seek, and a partial tail from a crash is handled identically to the non-optimized case. A malformed or truncated target event terminates scanning of that file, and the transaction's GTID is not added to the executed set, as the transaction cannot be confirmed complete.

#### Crash-Safety Analysis

The recovery behavior is aligned with all major operations that interact with the index file, including rotate, purge, and resetting binary logs and GTIDs. Note that the complete binlog content, header and body, is durable before writing the file sequence number into the `purge_index_file` (step 6).  Every case below therefore rolls forward or back against a fully durable file. Below is the crash-safety analysis based on the commit code path steps above.

- If MySQL restarts or crashes before the new file name is recorded in `purge_index_file` (steps 1 - 5), then nothing was written to `purge_index_file`, no file exists under the new name, and nothing is in the index. The temp file is cleaned from `#binlog_temp_files`, and InnoDB finds no XID in binlog and rolls the transaction back.
- If MySQL restarts or crashes after recording the file in `purge_index_file` but before adding it to the index (steps 6 - 8), then the file is deleted on recovery following the mechanism that exists today; the trailing Rotate event in the previously active file is handled exactly as it is for an interrupted ordinary rotation. InnoDB finds no XID in binlog and rolls the transaction back.
- If MySQL restarts or crashes after adding the file to the binlog index (step 9) but before completing the remaining steps, the file is already discoverable in the index. Recovery scans the binlog, finds the XID, and InnoDB rolls the transaction forward. The `purge_index_file` entry, if still present, is cleaned up as a no-op since the file is already indexed.
- If MySQL restarts or crashes after the InnoDB commit while the promoted file is the active binlog file (not yet rotated away by step 13), then the transaction is durable in both the binlog and InnoDB. Recovery encounters the promoted file during its normal scan of the last binlog file and collects the transaction's XID using the optimized recovery algorithm, seeking straight to the terminating event through the `Large_transaction_header_event` instead of reading the transaction body. This lets ordinary crash recovery skip a multi-gigabyte transaction in a single seek, keeping recovery of the last file constant-time regardless of transaction size.
- If MySQL restarts or crashes after the promoted file has already been rotated away, the promoted file is an earlier, fully durable binlog file that recovery does not need to reprocess. The transaction is committed.

#### Filesystem Durability and Error Handling

The promotion reuses the same filesystem durability mechanisms as standard binlog rotation (index file sync, purge index sync, directory fsync). No new durability ordering is introduced. If any step fails, the commit is aborted with a flush error, the cache is reset, and the server either logs an error and continues (if `binlog_error_action = IGNORE_ERROR`) or aborts the server process (if `binlog_error_action = ABORT_SERVER`). No partial state is left visible to clients because the `LOCK_log` is held throughout and the engine commit has not yet occurred.

#### Performance Advantage

The optimization improves the two costs that scale with transaction size today: commit latency and crash recovery time.\
\
At commit, the standard path does O(n) work under `LOCK_log`, because it copies the entire binlog cache into the active file. The promotion path does a fixed amount of work, since the data was written to the temp file during the transaction's lifetime. The large transaction's own commit latency stops growing with its size, and because the commit stage no longer holds `LOCK_log` for the duration of a multi-gigabyte copy, the transactions queued behind it are no longer delayed proportionally to its size.\
\
At recovery, the standard path may have to scan an entire multi-gigabyte transaction to find where it ends, which is an O(n) operation. The optimization's code path seeks past it using the recorded offset, making recovery constant-time regardless of transaction size.\

## 3. User Interface

### New system variables

#### `binlog_large_transaction_optimization_enabled`

| Property | Value |
| --- | --- |
| **Values** | boolean (ON \| OFF) |
| **Default** | ON |
| **Scope** | GLOBAL |
| **Dynamic** | Yes. Takes effect for transactions that begin after the change; in-flight transactions are unaffected. |
| **Replicated** (written to the binary log) | No |
| **Persist** | PERSIST, PERSIST_ONLY |
| **Command line** | Yes |
| **Privileges required** | `SYSTEM_VARIABLES_ADMIN` |

**Description**: Enables the large transaction optimization, which keeps large-transaction commit latency low, avoids stalling concurrent commits, and keeps crash recovery fast regardless of transaction size. When ON (the default), a transaction whose spilled size exceeds `binlog_large_transaction_optimization_threshold` is committed through the optimized path. When OFF, all transactions commit through the standard code path and `binlog_large_transaction_optimization_threshold` is ignored.

#### `binlog_large_transaction_optimization_threshold`

| Property | Value |
| --- | --- |
| **Values** | unsigned integer, bytes. Minimum 10485760 (10 MB); values below the minimum are rejected. Range [10485760 - 2^64-1]. |
| **Default** | 134217728 (128 MB) |
| **Scope** | GLOBAL |
| **Dynamic** | Yes. Takes effect for transactions that begin after the change. |
| **Replicated** (written to the binary log) | No |
| **Persist** | PERSIST, PERSIST_ONLY |
| **Command line** | Yes |
| **Privileges required** | `SYSTEM_VARIABLES_ADMIN` |

**Description**: The spilled size above which a transaction qualifies for the large transaction optimization. Used together with `binlog_large_transaction_optimization_enabled`: the optimization must be enabled for this threshold to take effect, and it has no effect while `binlog_large_transaction_optimization_enabled` is OFF.

### New status variables (Observability)

#### `binlog_large_transaction_optimization_count`

| Property | Value |
| --- | --- |
| **Values** | unsigned integer (monotonic counter) |
| **Scope** | GLOBAL |

**Description**: The number of transactions committed through the large transaction optimization since server startup.

#### `binlog_large_transaction_optimization_missed_count`

| Property | Value |
| --- | --- |
| **Values** | unsigned integer (monotonic counter) |
| **Scope** | GLOBAL |

**Description**: The number of transactions that exceeded `binlog_large_transaction_optimization_threshold` but could not be optimized because they were incompatible with the optimization, since server startup.

### Security Context

The `#binlog_temp_files` directory is server managed storage of uncommitted binlog data. Files within it contain binlog equivalent data and use the permissions required by NFR12. Cleanup at startup follows the existing prefix filtered deletion pattern to ensure only files managed by the optimization are removed.

No new SQL privilege is introduced. Both new system variables `binlog_large_transaction_optimization_enabled` and `binlog_large_transaction_optimization_threshold` are controlled by the existing `SYSTEM_VARIABLES_ADMIN` privilege.

---