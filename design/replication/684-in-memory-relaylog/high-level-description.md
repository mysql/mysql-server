### Executive Summary

After this feature is implemented, MySQL users running replicas with Change Stream Applier (CSA) can achieve higher throughput and less disk usage, while preserving correctness, recovery, and applier parallelism. In-memory relaylog removes the relaylog disk round trip from the CSA by passing each transaction from the receiver to the applier through an in-memory queue instead.

In the current CSA code path (referred to as the disk path in this document), a transaction needs to go through disk I/O twice before it can be applied. The receiver (IO thread) writes encoded binlog events received over the network to relaylog files, and the applier reads them back to schedule and apply the transaction.

In-memory relaylog keeps the transaction in memory instead. As a new transaction arrives, the receiver wraps the transaction metadata and incoming encoded events into a data structure, pushes it to an in-memory queue. The coordinator reads from it and dispatches the transaction to workers, without relaylog file write/read during the process (referred to as the memory path in the document).

The queue has a hard memory limit per channel. A transaction larger than a configured threshold is re-routed to a temporary file in standard relaylog format (referred to as the spill path in the document). On replication stop, server restart, or crash, the queue is emptied and any uncommitted transactions are re-fetched by GTID auto-positioning.

### User / Developer Stories

As a MySQL user, I want lower replication lag on my CSA replica, so that changes on the source propagate faster with high throughput.

As a MySQL user, I want the replication process to minimize disk I/O due to the relaylog write/read, so that it uses less storage and frees up I/O throughput for my workload.

As a MySQL user, I want per-channel metrics for queue memory usage, large transaction handling, and receiver-side waiting, so that I can monitor and assess the performance of each channel.

As a MySQL user, I want recovery to work just like it does on any GTID-based replication today (re-fetch missing transactions by GTID), so that I can rely on the same operational procedures I already know.

As a MySQL user, I want the feature to be set to `ON` by default for eligible CSA channels, so that I get performance improvement on top of CSA.

As a MySQL user, I want clear actionable replication errors, so that I can identify the failing transaction by its GTID and still inspect its events through a retained diagnostic file to determine the cause and fix it.

### Scope

**In Scope**

Introduce a per-channel in-memory relaylog queue between the receiver and the CSA applier, that replaces the relaylog write and read path on the disk, for CSA channels only.

Bound memory per channel with a hard limit. The enqueue decision is determined before streaming the body, and oversized transactions are stored through spill path.

Recover on replication stop, server restart and crash via GTID auto-positioning, without relying on a durable relay log.

Provide per-channel `CHANGE REPLICATION SOURCE TO`(CRST) knobs to enable the feature and adjust the memory limit and spill path threshold.

Expose per-channel observability metrics for memory path usage, spill path usage, and receiver-side block.

Report apply failures through GTID-based diagnostics `performance_schema.replication_applier_status_by_worker`, and write the failed transaction to a diagnostic file in relaylog format for easy inspection.

**Out of Scope / Limitations**

The feature is effective only on CSA channels. It is not supported for `Group Replication` channels, or `Semisynchronous` replication. Enabling this feature on such channels shall be rejected.

`Semisynchronous` replication acknowledges a transaction to the source only after it is durably written to the relaylog, so a memory-only copy would break the durability requirement.

`Group Replication` channel has no IO thread. So the streaming and recovery logic is not feasible in GR setup.

With In-memory relaylog, file based relaylog-related surfaces are reduced.

`SHOW REPLICA STATUS` relaylog position fields (`Relay_Log_File`, `Relay_Log_Pos`, `Relay_Log_Space`) are not applicable

`sync_relay_log` is not applicable

`SHOW RELAYLOG EVENTS` is not applicable

`FLUSH RELAY LOGS` is a no-op.

Because in-flight transactions are memory-only, replication restart (`STOP REPLICA` then `START REPLICA`) may need to re-fetch more transactions from the source than a file-based relaylog replica.

