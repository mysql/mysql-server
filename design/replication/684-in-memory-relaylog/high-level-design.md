### Summary of the Approach

Overview

The CSA receiver writes every transaction's events to a relaylog file and the coordinator reads them back to build a `Job_applier`. The feature instead has the receiver construct the `Transaction_envelope` in memory and hand it to the coordinator to build `Job_applier` directly, keeping the exact same source stream so the worker apply path is untouched.

Workflow

There are four roles in the end-to-end workflow. The Receiver (IO thread) reads events off the network, parses the GTID header, assembles each transaction into a `Transaction_envelope` object, and enqueues it into a queue, replacing the `write_buffer()` write to the relay log files.
The queue (`Trx_envelope_queue`) is a deque implementation with `Transaction_envelope` in source order. The queue retains each transaction until it commits. The coordinator (CSA / SQL thread) reads `Transaction_envelope` in order, builds them into `Job_applier`, schedules and dispatches them to the worker pool without decoding. The workers decode and apply each transaction's events, unchanged from today.

Envelope structure: `Transaction_envelope` and `Trx_payload`
The envelope is split into two objects with different lifetimes, so that memory can be reclaimed as soon as a transaction commits rather than when its entry is dequeued.

```
Transaction_envelope {             // queue entry — lightweight, lives until dequeued
  mutex
  trx_length                       // header metadata (from GTID event)
  path                             // MEMORY | SPILL
  state                            // open | committed | truncated
  unique_ptr<Trx_payload> payload  // reset at commit (memory path)
}

Trx_payload {                      // heavy — carries the encoded events, lives until commit
  shared_ptr<Fetchable_transaction> // the CSA trx object consumed by workers
}
```

`Transaction_envelope` is the lightweight entry used for ordering and re-dispatch. `Trx_payload` is the heavy object that wraps the existing CSA `Fetchable_transaction`. The encoded bytes are held in the `Event_set_fetchable` batch. The per-channel memory-usage counter depends on the lifecycle of `Trx_payload`. Its constructor reserves `trx_length` against the memory limit and its destructor releases those bytes, and wakes the blocked receiver.
The `Transaction_envelope` slot holds its `payload` reference until commit. After commit, the memory allocation ends, and the payload is reset while the empty `Transaction_envelope` slot lingers in the queue until the commit low-water mark advances. This avoids head-of-line memory blocking when transactions commit with `replica_preserve_commit_order=OFF`.

Receiver (IO thread)

At the GTID event, the receiver reads `trx_length`, chooses the path, creates the envelope, and enqueues it immediately. As body events arrive, it appends successive events to the envelope `Trx_payload` and publishes the new end position, waking any waiting worker. At the transaction boundary it stops appending and marks the underlying `Event_set_fetchable` `sealed`. Control events that are not part of a transaction (Format_description, Rotate, heartbeat) are not turned into envelopes. GTID tracking is unchanged. A GTID enters `Retrieved_Gtid_Set` only after the transaction's last event is published.
The change to the IO thread is localized. Instead of writing each event to the relaylog file (`queue_event()` → `write_buffer()`), the receiver now passes transaction events to a different destination. Encoded events are appended into a new in-memory byte source, `Event_set_fetchable_memory`, an implementation similar to `Event_set_fetchable_cache`, that reuses the streaming and synchronization functions (`append_event()` / `seal_stream()`, `wait_next()` / `fetch_next()`) and adds a commit hook in `set_success()` to mark the envelope committed, and release the transaction payload. Because it exposes the same `Event_set_fetchable` interface, the decode and apply path is unchanged.

Queue

The queue (`Trx_envelope_queue`) is an in-memory queue of `Transaction_envelope` in source order plus a byte counter for the memory usage. 
`stream_seqno` is a monotonically increasing sequence number the queue assigns to each transaction envelope at enqueue time, in the order the receiver inserts them (source order). It is an internal, per-channel logical sequence to address a position within the queue.
Three cursors track the queue progress, similar to how the GAQ (Global Apply Queue) tracks a low-water mark in classic MTA (Multi-threaded Applier). Each cursor is expressed as a `stream_seqno` value:
`commit_seqno`: the head of the queue, and the commit low-water mark.
`insert_seqno`: the tail of the queue, where the receiver inserts new envelopes.
`dispatch_seqno`: the next envelope to dispatch by the coordinator.
Queue admission of `Transaction_envelope` is decided at the GTID event using `trx_length`:

```cpp
trx_length > disk_path_threshold      -> spill path
queue_bytes + trx_length <= limit     -> memory path
otherwise                             -> block until commits free space, then memory path
```

Reserving the exact `trx_length` up front guarantees a memory-path transaction fits before streaming begins. A transaction larger than the `IN_MEMORY_RELAYLOG_SPILL_THRESHOLD` always takes the spill path.

Thread safety
The queue uses a three-tier locking structure so that its shared structure is safe while per-transaction streaming stays parallel.
A single queue-level mutex guards the deque, the three cursors, and envelope properties. Every structural mutation requires this mutex, so the receiver, coordinator, and workers never modify the queue concurrently. These critical sections are small and are taken only at transaction boundaries.
A per-envelope mutex guards each envelope's status flags (committed / truncated) and its payload reference.
The per-transaction event streaming is synchronized separately, by `Fetchable_transaction`'s own mutex and condition variable. This mechanism already exists in the current code, so different transactions stream fully in parallel.
The memory usage counter is an atomic variable. Modifying the variable is a lock-free update. A mutex plus a condition variable is used only when the memory counter is at the `IN_MEMORY_RELAYLOG_LIMIT` and the receiver must block until commits free enough memory.

Applier (SQL thread)

The coordinator consumes transactions through the existing `Transaction_provider` interface, so the `Csa_service::run` loop, scheduler, dependency tracking, and worker pool are all reused unchanged. The changes on the coordinator side are limited to the `Reader` interface.
A new reader `Queued_transaction_reader` is introduced as a memory path counterpart of `Relay_log_adaptive_reader`. Its `read()` takes the envelope at `dispatch_seqno` from the `Trx_envelope_queue`, advances the cursor, and wraps the envelope's `Fetchable_transaction` into a fresh `Job_applier` using the same constructor the relaylog reader uses today.
Everything downstream is untouched. The coordinator obtains the next transaction from the provider, computes scheduling dependencies from the logical clock and commit-order inputs, and dispatches it to the worker thread pool exactly as today.
The coordinator also tries to advance `commit_seqno` in the queue over a contiguous run of committed transactions at the head and dequeues each. A transaction that commits behind an uncommitted head is only marked committed and swept later when the head commits.
Workers apply exactly as today. Each worker pulls events from the transaction byte source via `wait_next()` / `fetch_next()`, decodes them, and applies them, blocking only when it catches up to the last published event of a still-receiving transaction. For memory path, the bytes come from the in-memory segment, and for spill path, they read from the disk file. The decode and execution path remain unchanged. On commit, the worker's `set_success()` fires the commit hook that marks the envelope committed and releases its `Trx_payload`.

Ownership

Trx_payload
`Trx_payload` carries the byte stream `Fetchable_transaction` of each transaction with `RAII`(Resource Acquisition Is Initialization) memory-usage accounting. The owner of the `Fetchable_transaction` changes as the transaction moves through different phases:
Open (IO thread still receiving): held by the receiver and the `Transaction_envelope` entry in the queue.
Sealed, not yet dispatched (fully received, waiting in the deque): held by the `Transaction_envelope` entry only; the receiver dropped its reference after marking the envelope `sealed`.
Dispatched, applying (in-flight): held by the `Transaction_envelope` entry and the `Job_applier` (a copy taken at dispatch).
Committed: the `Transaction_envelope` entry resets its `Trx_payload` pointer, and the Job is destroyed. With the drop of the last reference, `Fetchable_transaction` is freed while the empty `Transaction_envelope` entry lingers behind an uncommitted head.
Rolled back (stop or retry): the `Job_applier` is dropped by the workers but the `Transaction_envelope` entry keeps its reference, so the uncommitted payload stays alive and re-dispatch can hand it to a fresh `Job_applier`.

Queue
The in-memory queue `Trx_envelope_queue` is owned by the channel's `Master_info`. `mi` lives in `channel_map`, owned by the server, so it outlives the lifecycle of either IO or SQL thread. When IO or SQL thread starts, it attaches to the queue, and when stopped, detaches from it.
`mi` holds only a pointer to the `Trx_envelope_queue`. A channel that does not use the feature carries only a null pointer.
A queue instance is created when an eligible channel is created either through `CHANGE REPLICATION SOURCE` or when `mi` is reconstructed at server startup.
A `CHANGE REPLICATION SOURCE` that turns off the feature, or that makes the channel ineligible, deletes the queue instance and restores the `mi` queue pointer to `nullptr`.
`RESET REPLICA ALL` / `shutdown` destroys the `mi` along with the `queue` under the `channel_map` write lock.
The queue instance exists for the whole enabled period. The queue interacts with replication threads:
START — the receiver attaches as producer and the coordinator attaches as consumer to the already-present queue instance in the channel.
STOP of one thread only — that replication thread detaches while the queue instance stays. On `STOP REPLICA SQL_THREAD`, the receiver still running, uncommitted envelopes are retained for re-dispatch; on `STOP REPLICA IO_THREAD`, the applier still running, the coordinator drains what remains and idles.
STOP to full idle — when the stop leaves the channel with no active replication thread, the queue is reset. The queue instance itself is not destroyed while the feature is still enabled.

Commit

When a worker finishes applying and commits a transaction, it invokes the existing CSA success callback (`Job::set_success` →`Fetchable_transaction::set_success`). In in-memory relaylog, that callback does two operations: mark the transaction's `Transaction_envelope` as committed, and reset its `payload` to free the transaction memory.
With `replica_preserve_commit_order` set to `ON`, commits occur in source order handled by `Commit_order_manager`.

Recovery

The queue is purely an in-memory struct that cannot persist through replication stop, server restart, or crash. The recovery process therefore relies only on the durably recorded `gtid_executed`. Any uncommitted transaction can be re-obtained by GTID, either re-dispatched from the queue if it still holds the transaction (in the case of `STOP REPLICA SQL_THREAD`), or re-fetched from the source by auto-positioning. Duplicates are harmless because a worker skips any transaction whose GTID is already in `gtid_executed` (`is_already_logged_transaction`), and an interrupted transaction is always rolled back and re-fetched during recovery.
`STOP REPLICA SQL_THREAD` (applier only; receiver keeps running). The queue and all uncommitted envelopes still stay in memory. In-flight jobs are driven to a terminal state (commit, or roll back), so no transaction is left half-applied. With no advance of `commit_seqno`, all uncommitted transactions remain in the queue. The IO thread keeps enqueuing new envelopes until the memory usage counter reaches the `IN_MEMORY_RELAYLOG_LIMIT`. On next `START REPLICA SQL_THREAD`, the coordinator rewinds `dispatch_seqno` to `commit_seqno` and re-dispatches all the retained uncommitted transactions in order; committed-but-not-yet-swept envelopes are skipped because their payload is freed.
`STOP REPLICA IO_THREAD` (receiver only; applier keeps running). No new transactions are produced, and the queue is retained while the SQL thread runs. The coordinator drains and commits every fully-received transaction in the queue, then idles. A transaction that was only half-received when the receiver stopped is marked as truncated, and the worker applying it simply stops and rolls back. On the next `START REPLICA IO_THREAD`, the receiver simply re-fetches that transaction by GTID into the next slot in the queue and resumes normal operation.
`STOP REPLICA` (both threads). Production and consumption both stop: in-flight jobs commit or roll back, every payload reference drops, the queue is reset. Committed transactions stay in `gtid_executed`, and everything uncommitted is discarded. On the next `START REPLICA`, auto-positioning re-fetches the gap transactions from the source by GTID and continues as normal.
Start single thread after a full stop. The queue was reset at the full stop.
On `START REPLICA IO_THREAD`, the receiver reconnects, auto-positions from `gtid_executed`, and refills the queue from the source. With no worker to apply the changes, the queue fills to the memory limit and the receiver blocks.
On `START REPLICA SQL_THREAD`, the coordinator finds an empty queue and no producer, so the coordinator has nothing to dequeue and simply idles until the receiver starts enqueuing.
Server restart or crash. All in-memory state is gone, including the queue and the retrieved GTID set. Auto-positioning resumes from durable `gtid_executed` and re-fetches everything unapplied from the source — the same outcome as `STOP REPLICA` followed by `START REPLICA`.

Spill Path

Transactions stored through spill path are written in standard relaylog format to a dedicated directory, so the entire read/decode path is reused. Each spill path `Transaction_envelope` carries the file payload in `Event_set_fetchable_spill`. Spill path transactions are large by definition and therefore rare. One self-contained file per transaction (Format_description header plus events) keeps the lifecycle trivial. The receiver writes events to the file as they stream and marks it `sealed` at the last event. The worker can begin applying a transaction from the spill file while it is still open.
Dedicated directory. Following the same pattern as the binlog optimization for large transactions (BOLT), spill files live in a dedicated directory named `#in_memory_relaylog_temp_files`, created in the same directory as the channel's relaylog files. Placing it alongside the relaylog keeps the files on the same filesystem and lets them inherit the relaylog's file permissions, ownership, and encryption. The directory is created during server initialization, and is excluded from schema-visible listings (`SHOW DATABASES`, `information_schema`). If the path exists but is not a directory, or cannot be created or secured, the feature logs an error and rejects enabling the feature through CRST.
File lifecycle. Each spill file has a unique name to avoid conflicts. It's retained until its transaction commits, and then deleted. On startup, any leftover uncommitted spill path files in `#in_memory_relaylog_temp_files` are discarded and re-fetched by GTID auto-positioning. Workers read and decode the spill path file exactly as they read the relaylog today.

Diagnosability

The memory path is weak on diagnosability, because there is no relaylog on disk to inspect when a transaction failed and the in-memory byte stream is volatile. To improve this,
A failed transaction is identified by its GTID. The existing `performance_schema.replication_applier_status_by_worker` fields (`LAST_ERROR_NUMBER` / `LAST_ERROR_MESSAGE`, `APPLYING_TRANSACTION` and its timestamps) continue displaying the erroneous transaction GTID with error message.
The feature writes the erroneous transaction to a diagnostic file in relaylog format in the dedicated directory `#in_memory_relaylog_temp_files`. The file can be inspected with existing event-dump tooling, restoring the capability to investigate the relaylog as disk path. The retained diagnostic file survives server startup, and is only removed when the replication channel restarts or the channel is reset.

### User Interface

### Configuration / Knobs — New configuration clauses or options

All In-Memory relaylog settings are per channel. They are exposed as `CHANGE REPLICATION SOURCE TO` clauses. Each clause is a per-channel value held in the channel's in-memory `Master_info` and backed by the persisted replication metadata repository. They are effective only on a CSA channel with asynchronous replication. Setting them on any other channel type is rejected with an error.

To enable In-Memory relaylog on a CSA channel:

```sql
CHANGE REPLICATION SOURCE TO
    IN_MEMORY_RELAYLOG_ENABLED         = 1,
    IN_MEMORY_RELAYLOG_LIMIT           = 134217728,   -- bytes (128 MB)
    IN_MEMORY_RELAYLOG_SPILL_THRESHOLD = 16777216     -- bytes (16 MB)
  FOR CHANNEL 'ch1';
```

**NAME**: `IN_MEMORY_RELAYLOG_ENABLED`

**VALUES**: 0 \| 1 DEFAULT: 1 (ON)

**PERSISTED**: YES

**PRIVILEGES REQUIRED**: REPLICATION_SLAVE_ADMIN

**DESCRIPTION**: Enables the in-memory relaylog for the channel, replacing the standard relaylog files on the disk path. When set to 0, the feature is off, and the channel uses the standard relaylog disk path with no change in behavior. Effective only on CSA channels; enabling it elsewhere is rejected.



**NAME**: `IN_MEMORY_RELAYLOG_LIMIT`

**VALUES:** unsigned integer, bytes. Range [33554432 (32 MB), 4294967296 (4 GB)]; values outside the range are rejected.

**DEFAULT**: 134217728 (128 MB)

**PERSISTED**: YES

**PRIVILEGES REQUIRED**: REPLICATION_SLAVE_ADMIN

**DESCRIPTION**: Hard per-channel limit on the bytes held by memory path transactions. Bounds only memory path transactions. Disk-path transactions live on disk and do not count against it. When the limit is reached, the receiver blocks on enqueue until committing transactions free space.



**NAME**: `IN_MEMORY_RELAYLOG_SPILL_THRESHOLD`

**VALUES**: unsigned integer, bytes, Range [8388608 (8MB) , `IN_MEMORY_RELAYLOG_LIMIT` )

**DEFAULT**: 16777216 (16MB) 

**PERSISTED**: YES

**PRIVILEGES REQUIRED**: REPLICATION_SLAVE_ADMIN

**DESCRIPTION**: A transaction whose `trx_length` exceeds this threshold is stored through the spill path instead of the memory path.

### Configuration / Knobs — New system variables or command-line options

_TBD_

### Configuration / Knobs — New command-line options for utilities

_TBD_

### Configuration / Knobs — New UDFs or similar extension points

_TBD_

### New Statements

The In-Memory Relaylog feature can be turned on or off using the CHANGE REPLICATION SOURCE TO command. The configuration is set per user-defined channel. The CHANGE REPLICATION SOURCE TO command is extended with new options: * IN_MEMORY_RELAYLOG_ENABLED, accepting either "0" or "1" value, * IN_MEMORY_RELAYLOG_LIMIT, accepting a number within the range <33554432, 4294967296>. * IN_MEMORY_RELAYLOG_SPILL_THRESHOLD, accepting a number within the range <8388608, IN_MEMORY_RELAYLOG_LIMIT>.

### Observability

Each channel reports the following runtime per-channel metrics through the `performance_schema` replication tables. All of these metrics are collected through the existing CSA statistics infrastructure (`Statistics_map` / `Statistics_monitor`, per channel). New keys are added and updated inline at their relevant points. No new collection framework is introduced.

Memory Path:
`memory_bytes_used` — current memory path bytes held (the accountant value).
`memory_bytes_limit` — the configured `IN_MEMORY_RELAYLOG_LIMIT`.
`memory_trx_count` — cumulative transactions admitted to the memory path.
`memory_trx_bytes` — cumulative bytes of transactions admitted to the memory path.

Spill Path:
`disk_trx_count` — cumulative transactions to the spill path.
`disk_trx_bytes` — cumulative bytes of transactions to the spill path.
`disk_file_count` — current number of temp files to the spill path.
`disk_bytes_used` — current bytes of temp files to the spill path.
`disk_bytes_limit` — the soft limit of total bytes of temp files to the spill path.

Queue:
`queue_length` — number of transactions currently in the queue.
`block_count` — number of times the receiver blocked waiting for memory.
`block_time_total` — total time the receiver spent blocked waiting for memory.

### User Procedure

N/A

### Security Context

The `#in_memory_relaylog_temp_files` directory is server-managed storage of uncommitted transaction data. Files within it contain relaylog-equivalent data and use the same file permissions, ownership, and encryption as the channel's relaylog files. Cleanup at startup follows the existing directory-scoped deletion pattern, so only files managed by the feature are removed. Retain-on-error diagnostic files live in the same store under the same protections and are released when the error is cleared or the channel is reset.

On the memory path, transaction data resides in process memory and is never persisted. It is discarded on replication stop, server restart, or crash, so the feature does not widen on-disk exposure of replicated data.

No new SQL privilege is introduced. The new `CHANGE REPLICATION SOURCE TO` clauses (`IN_MEMORY_RELAYLOG_ENABLED`, `IN_MEMORY_RELAYLOG_LIMIT`, `IN_MEMORY_RELAYLOG_SPILL_THRESHOLD`) are controlled by the existing `REPLICATION_SLAVE_ADMIN` privilege already required for `CHANGE REPLICATION SOURCE TO`.

### Compatibility and Behavior Change

