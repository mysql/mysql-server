# Binlog Optimization for Large Transaction - Proposal

**Main Contributor:** Xueting Wu\
**Secondary Contributors:** Omar Farhat, Ali Bhagat, Michael Dowling, Bob Yang, Kevin Yang, Derrick Yang, Zongkai Xia

## 1. High-Level Description

### Executive summary

With this feature, the MySQL server no longer stalls when it commits a large transaction, and binlog crash recovery of a large transaction takes constant time. Applications whose workloads contain large transactions get lower and more predictable commit latency and shorter recovery.

Large transactions are a performance challenge for the binlog. When a transaction exceeds the in-memory binlog cache (`binlog_cache_size`), its events spill to a temp file. At commit, the server copies the whole temp file into the binlog file while it holds `LOCK_log`. The copy takes longer as the transaction grows. It inflates commit latency, stalls concurrent commits, and widens the crash window. If the server crashes during the copy, recovery must scan the whole binlog file to find incomplete transactions. That scan grows with the transaction size and is bounded by disk read throughput: in our benchmark, 50 GiB took about 15 minutes on HDD and about 4 minutes on SSD (see the performance benchmark).

We propose that a large transaction's spilled temp file takes the binlog file form while the transaction runs, and that the commit promotes that file into the binlog sequence instead of copying it. The copy under `LOCK_log` disappears. Recovery also stops depending on the transaction's size, because it skips the large transaction's body.

This optimization has run in production in Amazon Aurora MySQL since 2020. There it reduced large transaction commit latency by 6x and brought P99 binlog crash recovery time under one minute. MariaDB, inspired by Aurora MySQL, implemented a similar optimization (MDEV-32014), which supports the design.

### User / developer stories

As a MySQL user whose workload produces large transactions, such as bulk loads or large batch updates, I want the binlog to add no extra penalty to a large transaction's commit, so that the commit does not delay other concurrent commits.

As a MySQL user whose workload produces large transactions, I want binlog recovery of a large transaction to stay fast, so that my downtime stays short and predictable regardless of the transaction size.

As a MySQL user with a mixed workload, I want large transactions to benefit from the optimization while other transactions keep their latency and behaviour, so that I can adopt the feature without risk to my everyday workload.

As a MySQL user, I want my applications to stay unchanged when the feature is ON. The binlog it produces must stay compatible with my replicas, point-in-time recovery and binlog tools, so that I can adopt it without changing my topology or workflows.

As a MySQL user, I want to observe the optimization: whether it is active, how often it runs and why it was not used, so that I can check its behaviour against the performance I see.

### In scope

- A commit path for large transactions that runs inside the standard binlog group commit, together with transactions on the standard path.
- A new ignorable event, `Large_transaction_header_event`, that lets binlog crash recovery and GTID reconstruction skip a large transaction's body.
- A system variable to turn the optimization ON and OFF, and one for its threshold.
- Status variables that count promotions and missed optimizations.
- Error log messages for fallbacks, failures and recovery decisions.

### Out of scope / Limitations

The optimization does not apply in these configurations:

- `binlog_format` is `STATEMENT` or `MIXED`, or the transaction contains statement-format events;
- binlog encryption is ON;
- binlog transaction compression is ON;
- the transaction also changed a non-transactional table.

A transaction that is larger than `binlog_large_transaction_optimization_threshold` in such a configuration commits through the standard commit path. The server counts it in `Binlog_large_transaction_optimization_missed_count`. It logs a warning with the reason the first time in each server run. Later fallbacks for the same reason are only counted.

Other limitations:

- With `source_verify_checksum` ON, recovery reads large transactions in full, so that every checksum is verified. Recovery time then depends on the transaction size again.
- Relay log recovery and GTID reconstruction from relay logs read large transactions in full.
- Each large transaction that uses the optimization starts a new binlog file, so binlog files around it can be smaller than `max_binlog_size`.
- If a stored function turns binlog transaction compression ON in the middle of a large transaction, the transaction can be written to the binlog uncompressed.
- With `binlog_checksum=CRC32`, the event checksums count against `max_binlog_cache_size`, so a large transaction of many small events can reach the limit with fewer events than with the optimization OFF.

## 2. High-Level Design

When a transaction's binlog cache spills to disk, the optimization makes the spilled file look like a binlog file. At commit, the file is promoted into the binlog sequence as the next binlog file, so the commit does not copy the transaction under `LOCK_log`. At recovery, the server skips the transaction body with one seek.

### Terms

This section uses these terms:

- **Spilled file.** The temporary file that a transaction's binlog cache writes to when the cache no longer fits in its memory buffer, which is `binlog_cache_size` bytes.
- **Standard form.** How the binlog cache stores events without the optimization. An event has no checksum, and its `log_pos`, the field that holds the event's end offset in the binlog file, is 0. The server fills in both when it copies the cache into the binlog at commit.
- **Binlog file form.** How events are stored in a binlog file. Each event has its real `log_pos` and, with `binlog_checksum=CRC32`, a checksum.
- **Rewrite.** The one-time conversion of a transaction's cache from the standard form into the binlog file form, when the cache first spills.
- **Promotion.** Turning a transaction's spilled file into the next binlog file at commit, instead of copying the cache into the active binlog file. A file that was promoted is a promoted file.
- **Active binlog file and new binlog file.** The binlog file in use before a rotation, and the file that the rotation creates.
- **Header events.** The magic number and the events that start every binlog file: the `Format_description_log_event` and the `Previous_gtids_log_event`.
- **Terminating event.** The event that ends a transaction, for example its `Xid_log_event`.
- **Blocker.** A condition under which the optimization does not apply, such as statement-format events or binlog encryption. The transaction then commits through the standard commit path.
- **Standard commit path.** How a transaction commits without the optimization. The flush stage copies its cache into the active binlog file.

### Write path: the binlog cache and the rewrite at the first spill

When a transaction writes its first event, the transaction is checked once for the optimization. The optimization must be ON, the event must not be in statement format, binlog encryption and binlog transaction compression must be OFF, and no incident may be pending. When the cache of such a transaction first overflows its memory buffer, the cache is rewritten into the binlog file form. A named `bolt_` file is created as the spilled file, every cached event gets its real `log_pos` and checksum, and later events are written in that form directly. A transaction that never spills is never rewritten. It is copied into the binlog at commit, as without the optimization.

A promoted file always holds the large transaction as its first transaction. So `log_pos` can be set while events spill, if the offset of the transaction's first event is fixed in advance. In the promoted file, the first event follows the header events, then the transaction's `Large_transaction_header_event` and `Gtid_log_event`. Their combined size is not known when the cache first spills, so the cache reserves a region at the front of the spilled file. The region is the length of the active binlog file's header events plus 32 KiB of headroom, rounded up to a multiple of 64 KiB. At commit, the new binlog file's header events fill the front of the region. The `Large_transaction_header_event` pads the rest, so that the `Gtid_log_event` ends exactly where the transaction's first event begins.

The rest of the server still reads the cache in the standard form, as before the optimization. That includes the standard binlog copy, compression, savepoints and Group Replication. A rewritten cache that is not promoted is read back in the standard form and copied into the binlog as usual.

### Commit path: promotion in the flush stage of `ordered_commit()`

A transaction whose spilled file is promoted commits through the standard binlog group commit, `ordered_commit()`. It goes through the flush, sync and commit stages with the other members of its group. Only the flush stage differs, as the steps below show.

1. Before `ordered_commit()`, the session decides on the promotion. The transaction's cache was rewritten, its size exceeds the threshold, and no blocker applies at commit. The session flushes and syncs the spilled file outside `LOCK_log`, before the `before_commit` hook, so that a failure rolls the transaction back before any other server has it. After the hook, the session is marked to lead its flush group.
2. In the flush stage, under `LOCK_log`, the leader rotates the binlog instead of copying its cache into the active binlog file. The rotation is the standard rotation. It waits for the transactions prepared in the active binlog file, writes the Rotate event, and creates the new binlog file with its header events. It does not save the GTIDs of the active binlog file into `mysql.gtid_executed`, because the large transaction and the other members of its group can hold locks on that table until they commit.
3. Before the new binlog file is added to the binlog index, the spilled file replaces it. The header events are copied into the reserved region, followed by the `Large_transaction_header_event` and the `Gtid_log_event`. The spilled file is synced and renamed over the new binlog file's name, and both directories are synced.
4. The rest of the group is written into the promoted file, after the large transaction. The sync and commit stages run as for any group.
5. After the commit stage, the server saves the GTIDs of the previous binlog file into `mysql.gtid_executed`, then purges expired binlog files, as after any rotation. It rotates again if the promoted file exceeds `max_binlog_size`.

The large transaction must lead its flush group. A rotation waits for every transaction prepared in the active binlog file, and a member written before the large transaction would commit only after the flush stage, so the rotation would wait forever. A large transaction therefore enrolls as the leader of a new group, or waits until the current leader hands it the next group.

The promotion starts a new binlog file, so the replica's parallel applier treats the large transaction as a barrier. The applier does not run it in parallel with the transactions before or after it.

#### Anatomy of a promoted binlog file

```
+---------------------------------------------------------+
| Binlog magic number                                     |
| Format_description_log_event                            |
| Previous_gtids_log_event                                |
| Large_transaction_header_event (ignorable)              | # new event
| Gtid_log_event                                          |
| BEGIN                                                   |
| ... the transaction's events ...                        |
| Terminating event (for example Xid or XA_prepare)      |
| ... later transactions of the same group, if any ...    |
+---------------------------------------------------------+
```

The `Large_transaction_header_event` (event type 43) is ignorable, so replicas and tools that do not know it skip it. The wire format of the `Large_transaction_header_event` is shown below:

```
+--------------------------------------------------------------------------------+
|                     Common binlog event header (19 bytes)                      |
+--------------------------------------------------------------------------------+
| timestamp (4) | type_code (1) = 43 | server_id (4)                             |
| event_length (4) | log_pos (4) | flags (2), with 0x80 = LOG_EVENT_IGNORABLE_F  |
+--------------------------------------------------------------------------------+
|                             Post-header (0 bytes)                              |
+--------------------------------------------------------------------------------+
|                             Event body (variable)                              |
+----------+--------------+-------------+-------------+-------------+------------+
| Version  | Terminating  | Terminating | Terminating | Terminating | Padding    |
| (1 byte) | Event Offset | Event Type  | Event Body  | Event Body  | (variable) |
|          | (8 bytes)    | (1 byte)    | Length (1)  | (<= 141)    |            |
+----------+--------------+-------------+-------------+-------------+------------+
|         Checksum (4 bytes with binlog_checksum=CRC32, none with NONE)          |
+--------------------------------------------------------------------------------+
```

- The terminating event offset is the 64-bit file offset where the transaction's terminating event starts.
- The terminating event type is the event type of the terminating event.
- The terminating event body is a copy of that event's body, without its header and checksum. It is the transaction's identity: the 8-byte XID of an `Xid_log_event`, or the one-phase flag and the external XID of an `XA_prepare_log_event`. It is empty for any other event type.
- The padding fills the rest of the reserved region.

Only a transaction that ends with an `Xid_log_event` or an `XA_prepare_log_event` has an XID that recovery can verify, so only such a transaction uses the optimized recovery. For any other terminating event, recovery reads the transaction sequentially.

### Recovery

After a restart, binlog crash recovery and GTID reconstruction read binlog files from the start. When they read a `Large_transaction_header_event`, they read the event at the recorded offset and check it. That event must decode as an event, have the recorded type, length and body, and lie ahead of the reader. If it matches, the reader seeks to the terminating event and skips the body. If it does not match, the header is ignored and the transaction is read sequentially, as without the optimization. Crash recovery logs a warning for a header that does not match.

The header is also ignored in these cases, and the transaction is read sequentially:

- `source_verify_checksum` is ON, so that every event's checksum is verified, as without the optimization;
- the file is a relay log, because the header's offset refers to the source's binlog;
- the header body is in a newer format version, or is malformed;
- GTID reconstruction needs only the file's first GTID.

### Crash safety

The promotion runs inside the standard rotation, so it inherits the rotation's crash safety. The new file's name is in the purge index from the moment the new file is created until it is added to the binlog index.

| Crash point | At restart |
|---|---|
| Before the new file is in the binlog index | Startup removes the file at the new name through the purge index and removes leftover `bolt_` files. No binlog file holds the transaction's XID, so the storage engine rolls the transaction back. |
| After the new file is in the binlog index | Recovery reads the promoted file, skips the transaction body using the `Large_transaction_header_event`, finds the XID, and commits the transaction. |
| After the engine commit | The transaction is committed in the engine and the binlog. |

### Error handling

The externalization point is the Group Replication `before_commit` hook, because it is the earliest point in the commit at which the transaction can leave the server. Errors are handled by four rules:

1. When a blocker applies, the transaction commits through the standard commit path. This is not a failure. The blocker is counted and logged.
2. When an error is not fatal and the cache is still usable, the transaction falls back to the standard commit path, before or after the externalization point. The fallback reason is counted and logged.
3. Before the externalization point, a fatal error or a cache that cannot be trusted ends the statement or the transaction. A rollback is safe, because the transaction has not left the server.
4. After the externalization point, a rotation failure that is not fatal is handled by `binlog_error_action`, as any flush-stage error is. A fatal error aborts the server, regardless of the value of `binlog_error_action`, with a message that says what to check. A fatal error includes a failure to write the Rotate event and a failure to open the new binlog file.

The low-level design lists every case.

### Performance advantage

At commit, the standard path copies the whole cache into the active binlog file under `LOCK_log`: O(n) work. The promotion does a fixed amount of work under `LOCK_log`, because the file was written while the transaction ran and synced before the flush stage. The large transaction's commit latency stops growing with its size, and the transactions queued behind it are no longer delayed by a copy.

At recovery, the standard path scans the whole transaction to find its end: O(n) reads. With `source_verify_checksum` OFF, the optimized recovery seeks past the body, so recovery time does not depend on the transaction's size.

## 3. User Interface

### New system variables

#### `binlog_large_transaction_optimization_enabled`

| Property | Value |
| --- | --- |
| **Values** | boolean (ON \| OFF) |
| **Default** | ON |
| **Scope** | GLOBAL |
| **Dynamic** | Yes. A transaction uses the value at its first event. |
| **Replicated** | No |
| **Persist** | PERSIST, PERSIST_ONLY |
| **Command line** | Yes |
| **Privileges required** | `SYSTEM_VARIABLES_ADMIN` |

**Description**: Turns the large transaction optimization ON or OFF. When ON, a transaction whose binlog cache exceeds `binlog_large_transaction_optimization_threshold` commits by promoting its spilled file into the binlog sequence. When OFF, every transaction commits through the standard commit path. Setting it ON fails, with a warning, when the binlog temp files directory cannot be used. At startup, the server turns it OFF, with an error log message, for the same reason.

#### `binlog_large_transaction_optimization_threshold`

| Property | Value |
| --- | --- |
| **Values** | unsigned integer, bytes. Range [10485760 (10 MiB), 2^64-1]. |
| **Default** | 134217728 (128 MiB) |
| **Scope** | GLOBAL |
| **Dynamic** | Yes. A transaction uses the value at its first event. |
| **Replicated** | No |
| **Persist** | PERSIST, PERSIST_ONLY |
| **Command line** | Yes |
| **Privileges required** | `SYSTEM_VARIABLES_ADMIN` |

**Description**: The size of a transaction's binlog cache, without checksums, above which the transaction qualifies for the optimization. It has no effect while `binlog_large_transaction_optimization_enabled` is OFF. The threshold is never below `binlog_cache_size`. A lower value is raised to `binlog_cache_size`, with a warning while the optimization is ON.

### New status variables

#### `Binlog_large_transaction_optimization_count`

| Property | Value |
| --- | --- |
| **Values** | unsigned integer (monotonic counter) |
| **Scope** | GLOBAL |

**Description**: The number of transactions whose spilled file was promoted since server startup.

#### `Binlog_large_transaction_optimization_missed_count`

| Property | Value |
| --- | --- |
| **Values** | unsigned integer (monotonic counter) |
| **Scope** | GLOBAL |

**Description**: The number of transactions that exceeded the threshold with the optimization ON, but committed through the standard commit path, since server startup.

### Error log messages

The optimization logs:

- a warning with the reason when a large transaction falls back (once per server run for a configuration reason);
- a warning with the cause when a cache cannot be rewritten, or when the rotation of a promotion reports an error;
- notes during crash recovery when a header is used, or when `source_verify_checksum` makes recovery read the transaction sequentially, and a warning when a header does not match the file;
- messages about the binlog temp files directory at startup: leftover files removed or skipped, and why the directory cannot be used.

### Security context

The spilled files live in `#binlog_temp_files/<binlog base name>`, next to the binlog files. They hold uncommitted binlog data, and they are created with the permissions of binlog files. Startup deletes only regular files whose names match the optimization's `bolt_` naming in the server's own subdirectory. Anything else is left in place with a warning.

No new SQL privilege is introduced. Both system variables require `SYSTEM_VARIABLES_ADMIN`.

---
