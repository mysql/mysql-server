### Glossary

**Change Stream Applier (CSA)**
The applier introduced by WL#10500, selected per channel with `APPLIER_VERSION = 2`. The in-memory relaylog is available only on CSA channels.

**In-memory relaylog (IMR)**
The feature described in this document. On a CSA channel, the receiver passes each transaction to the applier through a per-channel in-memory queue instead of writing it to the relaylog files and reading it back.

**Disk path**
The classic relaylog path: the receiver writes events to the relaylog files with `write_buffer()` and the CSA applier reads them back with `Relay_log_adaptive_reader`. Channels that do not use the feature stay on the disk path.

**Memory path**
The store path of a transaction whose events are kept in process memory, in an `Event_set_fetchable_memory` byte source.

**Spill path**
The store path of a transaction whose `trx_length` exceeds `IN_MEMORY_RELAYLOG_SPILL_THRESHOLD`. Its events are written to a private file in standard relaylog format, read through an `Event_set_fetchable_spill` byte source.

**trx_length**
The size of a transaction in bytes as declared by its GTID event (`Gtid_log_event::get_trx_length()`). For a compressed transaction it covers the compressed `Transaction_payload_event`. Admission, the memory-usage counter and the byte metrics all use this value.

**Transaction envelope**
A `Transaction_envelope`: the lightweight queue entry of one transaction. It carries `trx_length`, the store path, the commit and truncate flags, the receiver hold, and the reference to its payload.

**Payload**
A `Trx_payload`: the heavy part of a transaction. It owns the `Fetchable_transaction` that holds the encoded events and, on the memory path, the memory reservation of the transaction.

**Byte source**
The `Event_set_fetchable` object that stores the events of a transaction and serves them to a worker: `Event_set_fetchable_memory` on the memory path, `Event_set_fetchable_spill` on the spill path. Each byte source is also the `Streaming_event_sink` the receiver writes into.

**Sealed**
The state of a byte source after the receiver has appended the last event of its transaction. A sealed transaction is fully received.

**Truncated**
The state of a transaction that the receiver could not finish (receiver stop, a new GTID event or a reconnect before the last event, or a write failure). A truncated transaction is never committed; the worker that applies it rolls back.

**Queue**
The `Trx_envelope_queue` of a channel: a `std::deque` of envelopes in source order with three cursors.

**Commit low-water mark**
`commit_seqno`, the head of the queue. All envelopes before it are committed or truncated and have been removed from the queue.

**Sweep**
The coordinator operation that removes the contiguous run of committed or truncated envelopes at the head of the queue and advances `commit_seqno` (`Trx_envelope_queue::sweep_committed()`).

**Admission**
The decision taken at the GTID event of every transaction: choose the store path, then reserve `trx_length` against the memory limit (memory path) or the spill disk limit (spill path), waiting while the limit is reached.

**Memory-usage counter**
The per-channel count of bytes reserved by memory path payloads. The count is stored in the channel's existing CSA resource monitor (`Resource_instance_monitor`), as resource `in_memory_relaylog_memory` with limit `IN_MEMORY_RELAYLOG_LIMIT`.

**Role**
The receiver role (IO thread, producer) or the applier role (SQL thread, consumer) on a channel's queue. A thread attaches its role when it starts and detaches it when it exits.

**Queue reset**
The operation run by the last role to detach: clear `Retrieved_Gtid_Set` and the receiver's GTID state, drop every envelope (which releases the payloads and deletes the spill files), and zero the cursors.

**Runtime**
The per-channel `Imr_runtime` object. It owns the queue and the receiver's write state. It exists exactly when `is_in_memory_relaylog() && is_csa_enabled()` holds for the channel: the stored `IN_MEMORY_RELAYLOG_ENABLED` setting is ON, the channel is on CSA, and the channel is not `group_replication_applier`.

**Reconcile**
The `Imr_service::reconcile()` operation that creates, rebuilds, destroys or leaves a channel's runtime so that it matches the channel's configuration.

**Spill directory**
The directory that holds spill files, `#in_memory_relaylog_temp_files` (`Spill_file_writer::kTempSubdirName`), under the relaylog directory.

### Summary of the Approach

#### Layers

The feature is built in five layers. The byte-source, queue, runtime, and service layers are new IMR code, almost all under `sql/changestreams/apply/storage/in_memory/`. The integration layer is `Queued_transaction_reader` plus changes to existing server and CSA files.

1. **Byte-source layer.** Stores the encoded events of one transaction and serves them to one worker. Memory path: `Event_set_fetchable_memory` and `Cached_event_memory`. Spill path: `Event_set_fetchable_spill`, `Spill_file_writer`, the spill directory functions and the spill encryption pass.
2. **Queue layer.** Orders transactions, enforces the memory/spill bounds and tracks progress: `Trx_payload`, `Transaction_envelope`, `Trx_envelope_queue`, and the two IMR resources of the channel's `Resource_instance_monitor`.
3. **Runtime layer.** One `Imr_runtime` per channel. It owns the queue, controls which thread holds each role, runs queue reset, and keeps the receiver's open transaction. The receiver write functions (`open_transaction()`, `append_transaction_event()`, `truncate_transaction()`) belong to this layer.
4. **Service layer.** One `Imr_service` per server. It validates the CRST options, owns the runtimes and reconciles them with the channel configuration, forwards the role attach and detach calls of the replication threads, and provisions the spill storage at startup.
5. **Integration layer.** The server side (receiver `handle_slave_io()` and `queue_event()`, `CHANGE REPLICATION SOURCE TO`, START and STOP, `Relay_log_info` persistence, `SHOW REPLICA STATUS`, key rotation) and the CSA side (`Sync_transaction_provider`, `Queued_transaction_reader`, `Job_applier`, the commit hook, statistics).

#### Layer-component mapping

| Layer | Components | Source files |
|---|---|---|
| Byte source | `Event_set_fetchable_memory`, `Cached_event_memory` | `in_memory/event_set_fetchable_memory.{h,cc}`, `in_memory/cached_event_memory.{h,cc}` |
| Byte source | `Event_set_fetchable_spill`, `Spill_file_writer` | `in_memory/event_set_fetchable_spill.{h,cc}`, `in_memory/spill_file_writer.{h,cc}` |
| Byte source | spill directory, spill encryption | `in_memory/spill_directory.{h,cc}`, `in_memory/spill_encryption.{h,cc}` |
| Queue | `Trx_payload`, `Transaction_envelope`, `Trx_envelope_queue`, `Envelope_path` | `in_memory/trx_payload.{h,cc}`, `in_memory/transaction_envelope.{h,cc}`, `in_memory/trx_envelope_queue.{h,cc}`, `in_memory/imr_types.h` |
| Queue | IMR resources | `apply/resource/resource_map.h` (`in_memory_relaylog_memory`, `in_memory_relaylog_spill_disk`), `apply/resource/resource_monitor.h` |
| Runtime | `Imr_runtime`, `Open_group`, `Receiver_write_failure`, writer functions | `in_memory/imr_runtime.{h,cc}`, `in_memory/queued_transaction_writer.{h,cc}` |
| Service | `Imr_service`, `Imr_config`, `Imr_options_validation` | `in_memory/imr_service.{h,cc}`, `in_memory/imr_types.h` |
| Integration (CSA) | `Queued_transaction_reader`, `Sync_transaction_provider` (IMR constructor), `Csa_service::run`, `Csa_service::get_imr_applier_stats`, `Statistics_map::count_imr_completed`, `Imr_applier_stats` | `in_memory/queued_transaction_reader.{h,cc}`, `apply/storage/relay_log/sync_transaction_provider.{h,cpp}`, `apply/service/csa_service.{h,cpp}`, `apply/resource/statistics_map.{h,cpp}`, `in_memory/imr_applier_stats.h` |
| Integration (server) | receiver adapters, CRST validation, START/STOP, status, key rotation | `sql/rpl_replica.cc`, `sql/rpl_log_encryption.cc` |
| Integration (server) | persistence and accessors | `sql/rpl_rli.{h,cc}`, `scripts/mysql_system_tables.sql`, `scripts/mysql_system_tables_fix.sql` |
| Integration (server) | observability | `storage/perfschema/table_replication_applier_configuration.cc`, `sql/server_component/applier_metrics_service_imp.cc`, `components/replication_applier_metrics/applier_metrics_pfs_table.cc` |

Paths starting with `in_memory/` are under `sql/changestreams/apply/storage/`; paths starting with `apply/` are under `sql/changestreams/`.

#### Component diagram

```
+------------------------------------------------------------------------------+
| Integration layer: server (sql/rpl_replica.cc, sql/rpl_rli.cc, ...)          |
|  handle_slave_io: imr_wait_for_admission     change_master(): CRST checks    |
|  queue_event: imr_on_gtid_event,             START / STOP, RESET REPLICA     |
|    imr_on_body_event, imr_on_truncate        SHOW REPLICA STATUS, metrics    |
|  handle_slave_io / handle_slave_sql:         key rotation                    |
|    attach and detach roles                                                   |
+--------------+---------------------------------------+-----------------------+
               | rli->imr() (cached, non-owning)       | Imr_service calls
               |                                       v
               |   +-------------------------------------------------------+
               |   | Service layer: Imr_service (one per server)           |
               |   |  validate_options  reconcile  remove                  |
               |   |  attach_/request_/detach_receiver, ..._applier        |
               |   |  init_spill_storage                                   |
               |   |  m_runtimes: channel id -> unique_ptr<runtime>        |
               |   +---------------------------+---------------------------+
               |                               | owns
               v                               v
+------------------------------------------------------------------------------+
| Runtime layer: Imr_runtime (one per channel)                                 |
|  attach_role / detach_role / request_stop   m_role_mutex   reset()           |
| receiver write path: open_transaction, append_event, truncate_receiver_group |
|  Open_group m_receiver_group                                                 |
+-------------------------------------+----------------------------------------+
                                      | owns by value
+-------------------------------------v----------------------------------------+
| Queue layer: Trx_envelope_queue                                              |
|  deque<unique_ptr<Transaction_envelope>>  commit <= dispatch <= insert       |
|  admission -> Resource_entry in_memory_relaylog_memory / _spill_disk         |
|  Transaction_envelope -> unique_ptr<Trx_payload>                             |
|                           -> shared_ptr<Fetchable_transaction> (one batch)   |
+-------------------------------------+----------------------------------------+
                                      | the batch
+-------------------------------------v----------------------------------------+
| Byte-source layer                                                            |
|  Event_set_fetchable_memory -> vector<Cached_event_memory>                   |
|  Event_set_fetchable_spill  -> Spill_file_writer (imr_sp_* file)             |
|                             -> Relaylog_file_reader (worker side)            |
|  spill_directory: ensure_spill_directory, purge_orphan_spill_files           |
|  spill_encryption: imr_reencrypt_spill_files                                 |
+------------------------------------------------------------------------------+
               ^
               | workers: Event_set_fetchable interface (wait_next / fetch_next,
               | set_success); coordinator: Trx_envelope_queue::sweep_and_dispatch
+--------------+---------------------------------------------------------------+
| Integration layer: CSA                                                       |
|  Csa_service::run -> Sync_transaction_provider -> Queued_transaction_reader  |
|  -> Job_applier -> scheduler -> worker pool -> set_success (commit hook)     |
+------------------------------------------------------------------------------+
```

#### Data flow

```
 source --(binlog dump)--> receiver (IO thread)
   handle_slave_io()   GTID event: read trx_length, imr_wait_for_admission()
                       (no lock held; waits while the limit is reached)
   queue_event()       holds LOCK_log
     FDE event    -> Imr_runtime::set_receiver_fde (copy of the new receiver FDE)
     GTID event   -> imr_on_gtid_event -> Imr_runtime::open_transaction
                     -> Trx_envelope_queue::enqueue:
                        classify -> create_payload (factory: acquire_admission)
                        -> validate_payload -> hold_for_receiver
                        -> push_back, ++insert_seqno,
                        notify m_not_empty_cv
     every event  -> imr_on_body_event -> sink->append_event
                     (memory: copy into Cached_event_memory;
                      spill: write + flush to imr_sp_* file)
     last event   -> seal, release_receiver,
                     GTID added to Retrieved_Gtid_Set
                               |
                               v
   Trx_envelope_queue   [commit_seqno ... dispatch_seqno ... insert_seqno)
                               |
                               v
 coordinator (SQL thread)
   Csa_service::run -> Sync_transaction_provider -> Queued_transaction_reader::read
     sweep_and_dispatch(): sweep reclaimable head, wait (100 ms poll),
                           ++dispatch_seqno, skip committed / truncated
     new Job_applier(shared_ptr<Fetchable_transaction>)
     dependencies -> scheduler -> worker pool          (unchanged CSA code)
                               |
                               v
 worker
   Fetchable_transaction::wait_next / fetch_next -> byte source -> decode, apply
   commit -> Job_binlog::set_success -> Fetchable_transaction::set_success
          -> Event_set_fetchable_*::set_success -> Transaction_envelope::commit
             (payload reset: memory released or spill file deleted; counters)
                               |
                               v
 coordinator, next read(): sweep pops the committed head, ++commit_seqno
```

### Detailed Description of Implemented Layers

This section describes the layers bottom-up. The byte-source layer knows nothing about queues or channels. The queue layer knows nothing about replication threads. The runtime layer adds the thread roles, the service layer adds the channel configuration, and the integration layer connects them to the server and to CSA. Class-level detail is in the Low-Level Design section.

#### Byte-source layer

Purpose. Hold the encoded events of one transaction and serve them to a worker through the existing `Event_set_fetchable` interface, while the receiver may still be appending.

Classes. `Event_set_fetchable_memory` keeps the events in memory as a vector of `Cached_event_memory`, each an owning copy of the received bytes. `Event_set_fetchable_spill` streams the events to a private spill file through `Spill_file_writer` and reads them back through a `Relaylog_file_reader`. The free functions `ensure_spill_directory()`, `purge_orphan_spill_files()` and `spill_directory_path()` manage the spill directory. `imr_reencrypt_spill_files()` handles master key rotation for encrypted spill files.

Interfaces. Each byte source implements two interfaces. As `Streaming_event_sink`, the byte source offers `append_event()`, `seal_stream()` and `set_stream_truncated()`. The receiver seals with `append_event(..., seal_after = true)` on the last event, truncates with `set_stream_truncated()`, and reads `has_io_error()` / `io_error_message()`. As `Event_set_fetchable`, a worker calls `wait_next()`, `fetch_next()`, `is_done()`, `is_error()`, `get_error_str()` and `reset()`, and the commit path calls `set_success()`. Both byte sources decompress a `Transaction_payload_event` on the read side, as the disk path does.

Use by the layer above. The queue layer creates exactly one byte source per transaction, wraps it as the single batch of a new `Fetchable_transaction`, and links the byte source back to its `Fetchable_transaction` (`set_owning_fetchable()`) and to its envelope. The byte source uses those back-pointers to mark the transaction complete or truncated and to commit the envelope.

#### Queue layer

Purpose. Keep the transactions of a channel in source order from their GTID event until they are committed, bound the memory they hold, and track how far the coordinator and the workers have got.

Classes. `Trx_payload` owns the `Fetchable_transaction` and the memory reservation of one transaction (RAII). `Transaction_envelope` is the queue entry; it owns the payload through a `unique_ptr` and keeps the commit, truncate and receiver-hold flags under its own mutex. `Trx_envelope_queue` owns the envelopes in a `std::deque<std::unique_ptr<Transaction_envelope>>`, keeps the three cursors under one queue mutex, and runs admission against two `Resource_entry` objects of the channel's `Resource_instance_monitor`.

Interfaces. Producer: `classify()`, `is_admissible()`, `wait_until_admissible()`, `acquire_admission()`, `enqueue()`. Consumer: `sweep_and_dispatch()`, `dispatch_next()`, `sweep_committed()`. Accounting: `release_bytes()`, `bytes_used()`, `release_spill_reservation()`, the spill gauges. Lifecycle: `stop()`, `resume()`, `reset()`.

Use by the layer above. The runtime holds the queue by value. The receiver write functions call `enqueue()`; the CSA reader calls `sweep_and_dispatch()`; the runtime calls the lifecycle methods when roles attach and detach.

#### Runtime layer

Purpose. Represent the in-memory relaylog of one channel for the whole period the feature is enabled on it, independently of thread start and stop (HLD Ownership).

Classes. `Imr_runtime` holds the queue, the role mutex, the owner thread id of each role, the receiver's open transaction (`Open_group`: envelope and sink), the last receiver write failure (`Receiver_write_failure`) and a read-only snapshot of the receiver's FDE. The writer functions in `queued_transaction_writer.cc` implement the receiver steps on an `Open_group`; the runtime forwards to them.

Interfaces. Roles: `attach_role()`, `detach_role()`, `request_stop()`, `is_role_owner()`, `is_attached()`, `fully_detached()`. Receiver write path: `wait_for_admission()`, `is_admissible()`, `set_receiver_fde()`, `open_transaction()`, `append_event()`, `truncate_receiver_group()`, `receiver_sink()`, `last_write_failure()`. Status: `bytes_used()`, `queue_length()`, `spill_file_count()`, `spill_bytes_used()`. Queue reset: `reset()` and `clear_received_gtid_state()`. Reconcile decision: `reconcile_action()`.

Use by the layer above. `Imr_service` creates and destroys runtimes and calls the role methods. The server receiver code calls the write path through `Relay_log_info::imr()`. The CSA reader reaches the queue through `imr()->queue()`.

#### Service layer

Purpose. Hold the IMR policy and the ownership of runtimes outside the replication command code, so the command code only asks the service.

Classes. `Imr_service` is a single server-lifetime object reached through the global `imr_service` (`std::unique_ptr<mysql::csa::Imr_service>` in `sql/rpl_replica.h`). It is created in `init_replica()` and destroyed after all channels are gone. `Imr_config` carries the validated memory limit and spill threshold to the runtime constructor. `Imr_options_validation` is the result of option validation.

Interfaces. Static: `validate_options()`, and the six role calls `attach_receiver()`, `request_receiver_stop()`, `detach_receiver()`, `attach_applier()`, `request_applier_stop()` and `detach_applier()`, which reach the runtime through `mi->rli->imr()` and need no service instance. Instance methods: `reconcile()`, `remove()`, `init_spill_storage()`, `spill_cleanup_failed()`, `spill_dir_unusable()`, `runtime_count()`.

Use by the layer above. The server calls `reconcile()` at the end of `load_mi_and_rli_from_repositories()` (channel load, `START REPLICA`, every CRST) and at the end of `change_execute_options()` (every CRST with an applier option), `remove()` from `~Relay_log_info()`, the attach and detach calls from the replication threads, the request calls from `terminate_slave_threads()`, and `init_spill_storage()` once at startup. The service is never called per event: the receiver and the applier use the pointer cached in `Relay_log_info`.

#### Integration layer

Server side. The receiver code in `sql/rpl_replica.cc` keeps the disk path unchanged and adds a branch for channels whose `rli->imr()` is not null. Thin adapters (`imr_wait_for_admission()`, `imr_on_gtid_event()`, `imr_on_body_event()`, `imr_on_truncate()`, `imr_report_queue_event_failure()`, `imr_check_semisync_before_dump()`, `after_write_to_in_memory_relay_log()`) translate between `Master_info` / `QUEUE_EVENT_RESULT` and the runtime. CRST validation, START and STOP, `SHOW REPLICA STATUS`, key rotation and relaylog purging are described in their own sections.

CSA side. `Csa_service::run` uses the IMR constructor of `Sync_transaction_provider` when `rli->imr()` is not null. That provider owns a `Queued_transaction_reader`, the memory path counterpart of `Relay_log_adaptive_reader`. The reader builds a `Job_applier` from the envelope's `Fetchable_transaction` with the same constructor arguments as the disk reader. The scheduler, the dependency adapter, the worker pool, the session service and the commit order manager are unchanged. The commit hook is the existing `set_success()` chain.

### Ownership and Lifetimes

```
Imr_service (server lifetime, global imr_service)
  └─ m_runtimes[channel id] : unique_ptr<Imr_runtime>                    (channel + config lifetime)
        └─ m_queue : Trx_envelope_queue (by value)
              └─ m_envelopes : deque<unique_ptr<Transaction_envelope>>   (GTID event -> sweep)
                    └─ m_payload : unique_ptr<Trx_payload>               (admission -> commit)
                          └─ m_trx : shared_ptr<Fetchable_transaction>   (shared with Job_applier)
                                └─ batch : unique_ptr<Event_set_fetchable_memory | _spill>
                                      └─ (spill) m_writer : unique_ptr<Spill_file_writer>
                                            └─ imr_sp_* file on disk
Relay_log_info::m_imr : non-owning pointer to the runtime (nullptr without IMR)
```

Runtime. Owned by `Imr_service`. Created and destroyed only by `reconcile()` and `remove()`, under the `channel_map` write lock and with both roles detached (see the shutdown case below). Thread start and stop only attach and detach roles. `Relay_log_info` caches a non-owning pointer, set through `set_imr_ptr()`, and every receiver, applier and status path reaches the runtime through `rli->imr()`. The pointer cannot change while a replication thread of the channel runs, because every option that changes the selection or the bounds needs both threads stopped. A stop request only sets the stop flags. The queue is reset only by the last owner thread to detach, and `reconcile()` leaves the runtime in place if a role is still attached. At server shutdown, `delete_slave_info_objects()` destroys the `Relay_log_info`, and with it the runtime, only after `wait_till_no_thd()` has seen every replication thread leave the thread list.

Queue. A member of the runtime, so its address is stable for the runtime's lifetime. Every `Trx_payload` keeps a plain back-pointer to the queue to release its bytes; the queue is destroyed only after it has been reset, when no payload is left.

Envelope. Owned by the queue from `enqueue()` until a sweep or a queue reset pops it. The receiver (`Open_group::envelope`), the coordinator (the return value of `sweep_and_dispatch()`) and the byte source (`m_envelope`) hold non-owning pointers. The receiver keeps the envelope in the queue while the receiver writes; a sweep removes only envelopes that are committed or truncated and that the coordinator has already dispatched past, and a queue reset runs only after both roles have detached. A committed envelope without a payload can stay in the queue behind an uncommitted head. This allows memory to be released out of source order. The coordinator is the only sweeper, so the envelope it gets from `sweep_and_dispatch()` stays valid until its next call.

Payload and Fetchable_transaction. The payload is owned by the envelope and reset at commit (by `commit()`, or by `release_receiver()` if the commit came while the receiver held the envelope), at sweep, or at queue reset. The `Fetchable_transaction` is shared between the payload and the `Job_applier`:

| Phase | Holders of the `Fetchable_transaction` |
|---|---|
| Open, receiving | the payload; the receiver writes through a non-owning sink pointer kept alive by the receiver |
| Sealed, not dispatched | the payload |
| Dispatched, applying | the payload and the `Job_applier` |
| Committed while the receiver holds the envelope | the payload, until `release_receiver()` resets it, and the `Job_applier`, until `Job_binlog::set_success()` drops its reference after the hook. The object is freed with whichever reference goes last |
| Committed, not held | the `Job_applier` only: `commit()` reset the payload, and `Job_binlog::set_success()` drops the job's reference right after the hook. The object is freed there |
| Truncated, not swept yet | the payload, and the `Job_applier` if a worker is applying it |
| Truncated and swept while a job applies it | the `Job_applier` only: the sweep destroyed the payload. The object is freed when the job drops it |
| Rolled back by an applier stop | the payload; the job is dropped, and a later dispatch builds a new `Job_applier` on the same object |
| Retried after a transient error | the payload and the same `Job_applier`, which resets the byte source and reads it again from the start |

Memory reservation. Taken at admission and owned by the payload; released by `~Trx_payload()`. Every way an envelope ends (commit, sweep, queue reset, runtime destruction) destroys the payload, so a reservation is released exactly once. The encoded bytes belong to the byte source and are freed with the last reference to the `Fetchable_transaction`. That is usually the same moment, but when a truncated envelope is swept while a job applies it, the reservation is released while the job still holds the bytes.

FDE. Each transaction's payload holds a `shared_ptr` to a read-only snapshot of the receiver's FDE (`Format_description_log_event`). The runtime takes the snapshot in `set_receiver_fde()`, which `queue_event()` calls under `LOCK_log` right after it installs each source Format_description event as the receiver's FDE. The source sends one at the start of every binlog file, so every connection and every source binlog rotation has one before its first transaction. `open_transaction()` shares the snapshot with every transaction until the next one. Workers decode and apply with the snapshot and never with the receiver's object, which is written in place under `LOCK_log` during relaylog rotation and while a source Rotate is decoded. `detach_role(RECEIVER)` releases the runtime's references; each transaction keeps its own.
Spill file and spill disk reservation. Owned by the `Spill_file_writer`, which is owned by the spill byte source, which is owned by the `Fetchable_transaction`. The writer deletes the file and returns the reservation when it is destroyed, that is when the last reference to the `Fetchable_transaction` goes away after commit, sweep or queue reset. Files left by a crash are deleted at the next startup.

### Threads and Synchronization

#### Threads and their work

| Thread | Work on IMR objects |
|---|---|
| Receiver (IO thread) | owns the receiver role; admission; `enqueue()`; appends to the open byte source; truncates; detaches the receiver role and, if last out, runs queue reset. The only thread that reserves on the memory and spill disk resources |
| Coordinator (SQL thread) | owns the applier role; `sweep_and_dispatch()`; builds `Job_applier`; detaches the applier role (with a final sweep) and, if last out, runs queue reset |
| Workers | read a byte source (`wait_next()`, `fetch_next()`) and decode with the transaction's FDE snapshot; run the commit hook (`commit()` on the envelope); destroy payloads, which releases memory and deletes spill files. Never take the queue mutex |
| Client session (STOP, CRST, RESET, START) | requests stops; reconciles runtimes; validates options; never touches queue contents |
| Status readers (`SHOW REPLICA STATUS`, metrics) | read `bytes_used()`, `queue_length()` and the spill gauges |
| Encryption key rotation | rotates the relaylog of each IMR channel under `LOCK_log`; rewraps spill headers |

#### Locks, condition variables and atomics

| Primitive | Owner | Guards | Taken by |
|---|---|---|---|
| `mi->run_lock`, `rli->run_lock` | `Master_info`, `Relay_log_info` | thread start and stop (existing) | each thread in its first and exit sections, where it attaches and detaches its role; START, STOP, CRST, RESET |
| `Imr_runtime::m_role_mutex` | runtime | role owner fields, attach versus detach, the "last role out" decision and queue reset | `attach_role()`, `detach_role()` only |
| `Trx_envelope_queue::m_queue_mutex` | queue | deque, cursors, `m_applier_error`, and every write of `m_applier_stopped` | receiver (push), coordinator (sweep, dispatch), `stop(APPLIER)`, `resume(APPLIER)`, `reset()`, status readers (`queue_length()`) |
| `Trx_envelope_queue::m_not_empty_cv` | queue | wait for an undispatched envelope | coordinator, with a 100 ms timeout |
| `Transaction_envelope::m_mutex` | envelope | flags and payload reference | receiver, coordinator, worker commit hook |
| `Fetchable_transaction::m_mutex` / `m_cv` | `Fetchable_transaction` (existing) | batch list, completion, truncation | worker; receiver through `set_fetching_complete()` and `set_fetching_truncated()`; commit hook caller |
| `m_stream_mutex` / `m_stream_cv` | each byte source | event list or published end position; sealed and truncated flags | receiver (append, seal, truncate), worker (wait, fetch) |
| `LOCK_log` | channel relaylog (existing) | receiver event processing in `queue_event()`; spill file open | receiver; key rotation through `rotate_relay_log()` |
| `Master_info::data_lock`, relaylog TSID lock | existing | receiver positions, GTID monitoring info, `Retrieved_Gtid_Set` | receiver; queue reset; reconcile |
| `channel_map` lock | existing | channel map; `Imr_service::m_runtimes` | write lock: reconcile, remove, startup; read lock: metrics, spill directory listing |
| `Resource_entry::m_available` (atomic) | `Resource_instance_monitor` | memory and spill disk budgets | receiver reserves (memory path: `fetch_sub`, undone with `fetch_add` if it does not fit; spill path: CAS loop in `try_lock_resource_soft()`); payload and writer destructors release (`fetch_add`) |
| `m_receiver_stopped`, `m_applier_stopped` (atomic) | queue | role stop flags; both start set. `m_applier_stopped` is written only under `m_queue_mutex` and is also set on a structural error | receiver admission poll; coordinator dispatch wait (under `m_queue_mutex`); byte source wait poll and the reader's job drop check (lock-free) |
| `m_receiver_owner`, `m_applier_owner` (atomic thread id) | runtime | role ownership, the only record of whether a role is attached | written by the owner under the role mutex, read lock-free |
| spill gauges, `m_writer_failed` (atomic) | queue, spill byte source | metrics; writer error publication | spill writer; metrics readers; consumers |
| `m_spill_delete_failed` (atomic) | queue | a failed run-time spill file delete | spill writer destructor on any thread, sometimes under `m_queue_mutex`; read and cleared by the coordinator's reader; cleared by `resume(APPLIER)` and `reset()` |
| statistics counters | `Statistics_monitor` (existing) | cumulative counters | commit hook; metrics reader |

#### Lock order

The nested acquisitions are:

* `run_lock` -> `m_role_mutex` -> `m_queue_mutex` -> `Transaction_envelope::m_mutex`: attach, detach and queue reset in a thread's run-lock section. The applier's final sweep checks each head under the envelope mutex. Under `m_role_mutex`, the receiver's detach also truncates its open group (`Fetchable_transaction::m_mutex`, the envelope mutex and `m_stream_mutex`, one at a time), and queue reset takes `Master_info::data_lock` and the relaylog TSID write lock, one at a time.
* `m_queue_mutex` -> `Transaction_envelope::m_mutex`: the coordinator's sweep and dispatch.
* `Fetchable_transaction::m_mutex` -> `Transaction_envelope::m_mutex`: the commit hook (`commit()`).
* `Fetchable_transaction::m_mutex` -> `m_stream_mutex`: `reset_fetching()`, which resets each batch for a retry or a new job.
* `LOCK_log` -> the IMR mutexes: the receiver in `queue_event()`.
* `Master_info::data_lock`, `Relay_log_info::data_lock` and the error locks -> `m_queue_mutex`: `SHOW REPLICA STATUS` reads `queue_length()`.

No path takes two of these in the reverse order. Of the IMR mutexes, only `m_role_mutex` is ever held while `Master_info::data_lock` or the relaylog TSID lock is taken; no path takes them while a queue, envelope, `Fetchable_transaction` or stream mutex is held. The byte sources call `update_max_event_length()`, `set_fetching_complete()` and `set_fetching_truncated()` while they do not hold `m_stream_mutex`. The envelope mutex is the innermost IMR lock: a payload destroyed under it (by `commit()` or `release_receiver()`) or under the queue mutex (sweep, reset) returns its reservations with atomic operations and takes no lock. The SQL thread detaches its role only after `csa_service->remove(rli)` has torn down the worker pool in `handle_slave_sql()`, so no commit hook runs during the final sweep or the queue reset.

### State Machines

#### Transaction envelope

An envelope normally goes through OPEN, SEALED, COMMITTED, SWEPT. A transaction that the receiver cannot finish goes to TRUNCATED and is swept the same way. The tag before each step names the thread that makes it: the receiver (IO thread), the coordinator (SQL thread) or a worker. The arrow on the left shows a worker that commits right after the last event, before the receiver has released its hold.

```
            [receiver] enqueue() at the GTID event
                           |
                           v
   +--------------------- OPEN ---------------------------+
   |   receiver appends events; the coordinator may       |
   |   dispatch it; a worker applies events as they       |
   |   arrive and waits for the rest                      |
   |                       |                              |
   |                       | [receiver] last event        | [receiver] stops, or a new GTID
   |                       | appended: seal, release      | event or reconnect arrives
   |                       | the hold                     | mid-transaction, or a write or
   |                       v                              | provisioning fails
   | [worker]           SEALED                            v
   | commit()   all events received; the              TRUNCATED
   | before     coordinator dispatches it if          [worker] rolls back if applying
   | the hold   not yet done; a worker applies        [coordinator] skips it if not
   | is         it and may retry                      dispatched yet
   | released              |                              |
   |                       | [worker] commit()            |
   |                       v                              |
   +-------------------> COMMITTED                        |
              payload freed: at commit, or at             |
              release_receiver() if still held            |
                           |                              |
                           +--------------+---------------+
                                          | [coordinator] sweep: at the queue head,
                                          | not held, already dispatched past
                                          v
                                        SWEPT
                     removed from the queue, envelope destroyed
```

| State | Flags | Payload | Coordinator and workers |
|---|---|---|---|
| OPEN | `m_state` ACTIVE; receiver hold set; byte source not sealed | kept | can be dispatched; the worker waits for events that have not arrived yet, and commits only after the last one |
| SEALED | `m_state` ACTIVE; receiver hold clear; byte source sealed | kept | dispatched or waiting for dispatch; a worker applies it, and may retry from the first event |
| COMMITTED | `m_state` COMMITTED | reset at commit, which releases the memory reservation | done; waits at the head for the sweep |
| TRUNCATED | `m_state` TRUNCATED | kept until the sweep | skipped at dispatch; a worker already applying it rolls back |
| SWEPT | none: the envelope is destroyed | destroyed | none |

The states are derived from the envelope's `m_state` (`Envelope_state`: ACTIVE, COMMITTED, TRUNCATED), its receiver hold (`m_receiver_held`) and the byte source's sealed state. OPEN and SEALED both have `m_state` ACTIVE.

Rules:

* COMMITTED and TRUNCATED are final and exclusive.
* When a worker commits before the receiver has released its hold, the envelope is COMMITTED but keeps its payload until `release_receiver()` resets it.
* Worker actions that do not change the state: a dispatch, a retry after a transient error (the worker reads the byte source again from the start), and a rollback because the applier stops. After an applier stop the envelope stays OPEN or SEALED, keeps its payload, and is dispatched again after the next `START REPLICA SQL_THREAD`.
* A TRUNCATED transaction is fetched again by GTID auto-positioning when the receiver restarts. An envelope truncated at provisioning (`validate_payload()`) is never dispatched.

#### Roles and queue reset

Each role (receiver and applier) goes through three states. A role is the replication thread that uses the queue from one side:

```
           thread starts                   STOP REPLICA or shutdown
           attach_role()                   request_stop()
 STOPPED ----------------> RUNNING ------------------------------> STOPPING
    ^                         |                                       |
    |                         | thread exits by itself                |
    |                         | (error, UNTIL condition reached)      |
    |                         v                                       |
    +--------------------- detach_role() <----------------------------+
                           in the thread's exit path
```

| State | Thread | Role owner | Stop flag | Queue access |
|---|---|---|---|---|
| STOPPED | none | clear | set | none; the role rests stopped between sessions |
| RUNNING | running | set | clear | normal: the receiver enqueues, the coordinator dispatches |
| STOPPING | still running its exit path | set | set | waits return at once; the thread may still touch the queue until it detaches |

The role owner (the thread id recorded by `attach_role()`) tells whether a thread is in the role; queue reset needs both owners clear (`fully_detached()`). The stop flag tells the thread to stop working; it is checked by the admission and dispatch waits. A stop request does not end the role: only `detach_role()` does, when the thread leaves.

* `attach_role()`, under `m_role_mutex`: records the calling thread as the role's owner and clears its stop flag (`resume()`). A new receiver also clears its last write failure. If another thread already owns the role, the call is refused and changes nothing.
* `request_stop()` may come from another thread (`STOP REPLICA`, shutdown). It sets the role's stop flag, so a thread blocked in a queue wait for that role returns at once: the IO thread in the admission wait, or the coordinator waiting for the next envelope. It does not detach the role or reset the queue.
* `detach_role()`, under `m_role_mutex`, acts only when called by the role's owner, the thread whose `attach_role()` succeeded. A thread whose attach was refused, such as a second IO thread in the start window, also calls it on exit, and the call then does nothing. Otherwise it runs these steps in order:
  1. Receiver: marks the open transaction truncated and closes it, which releases its hold on the envelope. Applier: sweeps the committed head of the queue.
  2. Sets the stop flag (already set if `request_stop()` ran).
  3. Clears the owner.
  4. If the other role is detached too, this thread is the last role out and runs queue reset (`reset(mi)`).

Channel view:

| Receiver | Applier | Queue |
|---|---|---|
| attached | attached | normal operation |
| attached | detached | receiver keeps enqueuing up to the memory limit; uncommitted envelopes are retained |
| detached | attached | coordinator drains every fully received envelope, then idles; a truncated envelope is skipped |
| detached | detached | the last thread to detach has run queue reset; the queue is empty and the runtime remains |

A role is attached only while its thread runs; the thread detaches it in its exit run-lock section. An attach is refused if another thread already owns the role. The refused thread reports an error and exits.

#### Runtime reconcile

`reconcile(rli)` computes:

* `want_runtime = rli->is_in_memory_relaylog() && rli->is_csa_enabled()`;
* `has_runtime = rli->imr() != nullptr`;
* `config_matches` = the existing queue's `memory_limit()` and `spill_threshold()` equal `rli->get_in_memory_relaylog_limit()` and `rli->get_in_memory_relaylog_spill_threshold()`. The queue's `relay_log_dir()` is always the directory of `relay_log_basename`, which is read-only, so it never causes a rebuild.

| want_runtime | has_runtime | config_matches | Action |
|---|---|---|---|
| false | any | any | `kDestroy`: clear the cached pointer, erase the entry |
| true | true | true | `kLeave`: nothing changes |
| true | true | false | `kRebuild`: erase the old runtime, then as `kCreate` |
| true | false | n/a | `kCreate`: register `in_memory_relaylog_memory` with the limit and, if `relay_log_space_limit` is not 0, `in_memory_relaylog_spill_disk` with `relay_log_space_limit`; construct the runtime; cache the pointer; clear `Retrieved_Gtid_Set` and the receiver GTID state |

`kDestroy` and `kRebuild` happen only when the existing runtime is fully detached, and then its queue is already empty. If a role is still attached, reconcile leaves the runtime in place. Callers: the end of `load_mi_and_rli_from_repositories()` (server start, `START REPLICA`, `CHANGE REPLICATION SOURCE TO`) and the end of `change_execute_options()` (every CRST with an applier option). These callers hold the `channel_map` write lock. The classic applier's temporary-error retry in `exec_relay_log_event()` also reloads the channel through `load_mi_and_rli_from_repositories()`, on the SQL thread and without that lock. Only a classic channel takes that retry, and it has no runtime and wants none, so reconcile returns before it asserts the lock or touches `m_runtimes`.

### Lifecycle Flows

#### Server startup

`ReplicaInitializer` (steps 1-4 run in `init_replica()`, step 5 after it returns):

1. creates `imr_service`;
2. loads every channel (`create_slave_info_objects()`); for each channel, `load_mi_and_rli_from_repositories()` ends with `reconcile()`, which creates the runtime of each IMR channel with an empty queue and an empty `Retrieved_Gtid_Set`;
3. for each IMR channel, `imr_purge_stale_relay_logs()` purges the relaylog files except the active one (only if `relay_log_purge` is ON);
4. `imr_service->init_spill_storage()` deletes orphan spill files in the spill directory and ensures it exists;
5. starts the replication threads of the channels configured to start.

#### Crash recovery

The queue does not survive a restart or a crash. After a restart, the server clears `Retrieved_Gtid_Set` of each IMR channel and deletes the spill files left behind. The receiver then connects with GTID auto-positioning from `gtid_executed` and fetches every unapplied transaction again, if the source still has it. A transaction that was applied but whose envelope had not been swept is in `gtid_executed`, so it is not sent again or, if it is, the worker skips it.

#### Recovery boundary

A full stop, a restart and a crash discard every received transaction that was not applied. GTID auto-positioning fetches them again only if the source still has them in its binary log. If the source has purged one of them, the receiver stops with `ER_SERVER_SOURCE_FATAL_ERROR_READING_BINLOG` (the source reports `ER_SOURCE_HAS_PURGED_REQUIRED_GTIDS`), as on any auto-positioned channel, and the replica must be provisioned again or connected to a source that still has the transactions. Disabling the feature does not bring them back. This applies to every IMR channel: the binary log retention of the source must cover the transactions the replica has received but not applied. `IN_MEMORY_RELAYLOG_ENABLED` is ON by default, so a channel switched to CSA is an IMR channel unless the feature is turned off or the channel is `group_replication_applier`.

#### START REPLICA

1. `start_slave()` reloads the channel with `load_mi_and_rli_from_repositories()`, which reconciles the runtime.
2. If both threads were stopped, `imr_purge_stale_relay_logs()` runs.
3. `start_slave_threads()` calls `check_replica_configuration_errors()`. It rejects the start of an IMR channel with `ER_REPLICA_IN_MEMORY_RELAYLOG_SPILL_STORAGE_UNUSABLE` if startup could not purge the spill files or found the spill directory unusable, and rejects an IO thread start with `ER_REPLICA_IN_MEMORY_RELAYLOG_INCOMPATIBLE_CONFIGURATION` if semisync is enabled.
4. Each thread, in its first run-lock section, attaches its role (`attach_receiver()`, `attach_applier()`). `attach_role()` calls `resume()`, so a coordinator that attaches rewinds `dispatch_seqno` to `commit_seqno`.

#### STOP REPLICA SQL_THREAD only

`terminate_slave_threads()` sets `abort_slave`, calls `request_applier_stop()` (sets the applier stop flag and wakes the coordinator) and stops the CSA service. Workers waiting in a byte source see the stop within 100 ms; their jobs roll back and are marked aborted, so the commit hook does not commit them. The SQL thread removes its worker pool and then, in its exit section, calls `detach_applier()`, which runs a final sweep and clears the applier owner. The receiver is still attached, so the queue keeps every uncommitted envelope with its payload and `Retrieved_Gtid_Set` is kept. The receiver goes on enqueuing until the memory limit is reached.

On the next `START REPLICA SQL_THREAD`, `attach_applier()` resumes the applier role with `dispatch_seqno = commit_seqno`. The reader dispatches every retained envelope again in order; envelopes already committed behind an uncommitted head are skipped.

#### STOP REPLICA IO_THREAD only

`terminate_slave_threads()` sets `abort_slave` and calls `request_receiver_stop()`. A receiver waiting in admission sees the flag within 1 ms and returns; `queue_event()` reports the stop, which is logged at information level. In its exit path the IO thread truncates the open transaction, then `detach_receiver()` truncates any group still open (a no-op by then) and clears the receiver owner. The applier is still attached, so the queue stays. The coordinator applies every fully received envelope and then idles. A worker applying the truncated transaction sees the truncation, rolls back and stops working on it; the sweep removes its envelope.

On the next `START REPLICA IO_THREAD`, the receiver connects with GTID auto-positioning. The truncated transaction's GTID was never added to `Retrieved_Gtid_Set` or `gtid_executed`, so the source sends it again and it gets a new envelope.

#### STOP REPLICA (both threads)

Both stops are requested. In-flight jobs commit or roll back. Whichever thread detaches last runs queue reset under its run lock and `m_role_mutex`, before it signals that it has stopped: `Retrieved_Gtid_Set` is cleared, the envelopes are dropped, the payloads release their memory, the spill files are deleted, and the cursors are zeroed. The runtime stays. On the next `START REPLICA`, every unapplied transaction is fetched again by GTID auto-positioning, if the source still has it. Starting only one thread after a full stop gives an empty queue: a lone receiver fills it up to the memory limit; a lone coordinator idles.

#### RESET REPLICA

`RESET REPLICA` requires both threads to be stopped, so the last thread to stop has already run queue reset and deleted the spill files. `gtid_executed` is not changed, so the next `START REPLICA` fetches by GTID auto-positioning. The three IMR settings are kept: `Relay_log_info::clear_info()` writes them back with the other applier settings. `RESET REPLICA ALL` deletes the channel's `Relay_log_info`; its destructor calls `Imr_service::remove()`, which destroys the runtime.

#### CHANGE REPLICATION SOURCE TO

The three IMR options are classified as options that affect both the receiver and the applier. In `change_master()`:

1. If either thread runs and the statement sets any IMR option, the statement fails with `ER_REPLICA_CHANNEL_MUST_STOP`. The same applies to `APPLIER_VERSION` when the stored IMR setting is ON and the channel is on CSA before or will be on CSA after the statement, because the switch turns the feature on or off.
2. `validate_in_memory_relaylog_options()` checks the IMR options (see "Configuration and Persistence").
3. `change_execute_options()` stores the options in `Relay_log_info` and calls `reconcile()`.

The results of reconcile:

* Enabling the feature (`IN_MEMORY_RELAYLOG_ENABLED = ON` on CSA, or `APPLIER_VERSION = 2` with the setting ON): `kCreate`. `Retrieved_Gtid_Set` is cleared, and the stale relaylog files are purged at the next start.
* Disabling it (`IN_MEMORY_RELAYLOG_ENABLED = OFF`, or `APPLIER_VERSION = 1`): `kDestroy`. The queue was already reset when the threads stopped; the runtime is destroyed. The next `START REPLICA` uses the disk path and fetches the unapplied transactions by GTID auto-positioning.
* Changing `IN_MEMORY_RELAYLOG_LIMIT` or `IN_MEMORY_RELAYLOG_SPILL_THRESHOLD`: `kRebuild` with the new bounds.
* Any other applier option: `kLeave`.

The stored IMR settings are kept when `APPLIER_VERSION` changes; they take effect again when the channel is back on CSA.

### Spill Path

#### Directory

The spill directory is named `#in_memory_relaylog_temp_files` on disk. It lives in the relaylog directory, the directory part of `relay_log_basename`, so it is on the same filesystem as the relaylog files. `Imr_service::init_spill_storage()` creates it at startup whether or not any channel uses the feature, because a CRST can enable the feature later. `Spill_file_writer::open()` never creates it. If the path is unusable, the cause and a summary are logged and every IMR channel is refused at START with `ER_REPLICA_IN_MEMORY_RELAYLOG_SPILL_STORAGE_UNUSABLE` until the server is restarted with the path fixed. The directory is not a schema: `SHOW DATABASES` and `information_schema` list the schemas of the data dictionary, and IMR adds no schema.

#### File naming and format

Each spill path transaction has its own file, named `imr_sp_<channel>_<id>` (`imr_sp_<id>` for the default channel). `<channel>` is the channel name encoded in the filename charset, as in relaylog file names (`ch-1` gives `imr_sp_ch@002d1_<id>`), which keeps the files of concurrent channels apart in the shared directory, and `<id>` is a lowercase hex number from a process-global counter. The file is in standard relaylog format: `BINLOG_MAGIC`, the transaction's FDE marked as a relaylog event, then the received events as raw bytes. A worker reads it with the normal `Relaylog_file_reader`, so the decode path is the disk path's.

#### Write, read and publish

The receiver writes and flushes each event before it publishes the new end position under the byte source's `m_stream_mutex`. The worker opens the file at the first event and reads up to the published end, waiting while it has caught up with an open transaction. The flush is a `write(2)` from the `IO_CACHE`; no fsync is done, because a spill file is never read after a restart.

#### Encryption and key rotation

A spill file is encrypted if `binlog_encryption` is ON when `Spill_file_writer::open()` creates it, and keeps that state for its life. An encrypted file has the format of an encrypted relaylog: the 512-byte replication encryption header (`Rpl_encryption_header_v1`), then the AES-CTR encrypted data. Positions stay logical and exclude the header. The reader detects the header and decrypts with the keyring, as for a relaylog file; a keyring failure on open is reported as a read failure. The memory path never writes a file.

`ALTER INSTANCE ROTATE BINLOG MASTER KEY` runs, after the new key is current, `flush_relay_logs_cmd()`, `reencrypt_relay_logs()` and then `imr_reencrypt_spill_files()`. `reencrypt_relay_logs()` rotates the relaylog of each IMR channel, which takes its `LOCK_log`. A spill writer copies the master key and flushes the header while it holds the same `LOCK_log`. So either the rewrap pass sees the new file's header or the writer has already taken the new key. After that flush the writer never writes below offset 512, so the pass and the writer touch disjoint bytes. When the pass is done, every live encrypted spill file names the current key, and the rotation purges the older keys. A file that cannot be rewrapped fails the statement with `ER_BINLOG_MASTER_KEY_ROTATION_FAIL_TO_REENCRYPT_LOG`. No IMR lock is added for this.

#### Disk full

The spill file's `IO_CACHE` is opened with `MY_WAIT_IF_FULL | MY_REPORT_WAITING_IF_FULL`, the flags of the relaylog file. If the disk is full, the receiver waits in the write as it does for the relaylog: the IO thread state is "Waiting for disk space", `SHOW REPLICA STATUS` does not block, and writing resumes when space is freed. The receiver holds `LOCK_log` during that wait, as it does on the disk path. Workers reading the same file do not take `LOCK_log`.

#### Disk limit

On an IMR channel `relay_log_space_limit` bounds the spill files instead of the relaylog. If it is not 0, reconcile registers the `in_memory_relaylog_spill_disk` resource with that value. A spill path admission reserves the transaction's `trx_length` with the soft rule (`try_lock_resource_soft()`): it is admitted while the reserved total is below the limit, whatever its own size, so the current transaction's file is always finished; the next spill path transaction waits until the writers of committed files are destroyed and the total drops below the limit. The reservation is returned when the writer is destroyed, after its delete attempt.

#### Deletion

A spill file is deleted when its `Spill_file_writer` is destroyed: after commit (when the last reference to the `Fetchable_transaction` goes), when a truncated envelope is swept, and at queue reset. Files left by a crash are deleted at startup by `purge_orphan_spill_files()`, which matches only names starting with `imr_sp_`.

If a spill file cannot be deleted at run time, a warning with the path and errno is logged. The writer has already returned the file's bytes, its file count and its spill disk reservation, so the file no longer counts toward `relay_log_space_limit` or the `SPILL_FILE_COUNT` and `SPILL_BYTES_USED` columns. The writer then sets the queue's `m_spill_delete_failed`, whichever thread destroys it, and the coordinator's next `read()` stops the applier with `ER_REPLICA_RELAY_LOG_READ_FAILURE`, as the relay log does when it cannot purge a relay log file. The record is cleared when the applier starts, so START REPLICA SQL_THREAD works again, and the startup purge removes the file. A failed delete during queue reset or after a spill-file setup failure (in `open()` or the first `flush()`) only logs the warning.

### Memory Accounting

Two budgets apply to an IMR channel. Both count sizes taken from the replication stream (the declared `trx_length`, and the length of each received event).

`IN_MEMORY_RELAYLOG_LIMIT` is a ledger of the `trx_length` reserved by memory path payloads: the reservation is taken at admission and returned by `~Trx_payload()`. It does not count the envelope, container and FDE overhead, the events the worker decodes and decompresses, the spill reader and writer buffers, or the encoded bytes that a job still holds after the sweep destroyed a truncated payload.

`APPLIER_EVENT_MEMORY_LIMIT` is the existing CSA budget (resource `declared_channel_memory`), unchanged by IMR. Before each fetch, `Job_applier::run_phase()` reserves the largest on-wire event length received so far for the transaction (`Fetchable_transaction::get_max_event_length()`, which both byte sources update on append) and releases it at the end of that loop iteration, so the reservation is per fetched event, not per transaction. For a compressed transaction that length is the compressed `Transaction_payload_event`, not the decompressed size. Its rule admits a request larger than the limit when the whole budget is free.

#### Admission at the GTID event

`handle_slave_io()` reads `trx_length` from each GTID event before it calls `queue_event()`, so admission is decided before any body event is buffered:

```
trx_length > IN_MEMORY_RELAYLOG_SPILL_THRESHOLD   -> spill path   (reserve on the spill disk entry, try_lock_resource_soft())
otherwise                                         -> memory path  (reserve on the memory entry, try_lock_resource())
reservation does not fit                          -> receiver polls every 1 ms until it fits, a stop is requested, or the IO thread is killed
```
A GTID or anonymous GTID event whose `trx_length` is smaller than the received GTID event (0 included: sources before MySQL 8.0.2, or a relay that rewrites GTID events without commit timestamps) stops the receiver in `handle_slave_io()` before the pre-wait and `queue_event()`, with `ER_REPLICA_RELAY_LOG_WRITE_FAILURE`. Nothing is reserved and no group is opened. Such a source replicates to the channel with `IN_MEMORY_RELAYLOG_ENABLED = OFF`.

The memory entry `in_memory_relaylog_memory` is registered by reconcile with `IN_MEMORY_RELAYLOG_LIMIT`. Its rule (`try_lock_resource()`) admits a reservation while the available amount is at least `trx_length`, or when the whole limit is available. Only memory path transactions reserve on it, and their `trx_length` is at most the threshold, which is below the limit. So a memory path transaction always fits once enough memory has been released, the oversize exception never applies, and the reserved total never exceeds `IN_MEMORY_RELAYLOG_LIMIT`. Spill path transactions reserve no memory.

#### Release at commit

The reservation is part of the payload. `~Trx_payload()` releases exactly `trx_length` with one atomic operation when the payload is destroyed, normally at commit. A committed envelope behind an uncommitted head no longer holds memory, so out-of-order commits with `replica_preserve_commit_order = OFF` free memory at once.

### Configuration and Persistence

#### Options

| Option | Values | Default | Stored in `Relay_log_info` as |
|---|---|---|---|
| `IN_MEMORY_RELAYLOG_ENABLED` | ON, OFF | ON | `m_in_memory_relaylog` (bool, default `true`) |
| `IN_MEMORY_RELAYLOG_LIMIT` | [33554432, 4294967295] bytes | 134217728 | `m_in_memory_relaylog_limit` (0 means the default) |
| `IN_MEMORY_RELAYLOG_SPILL_THRESHOLD` | [8388608, `IN_MEMORY_RELAYLOG_LIMIT`) bytes | 16777216 | `m_in_memory_relaylog_spill_threshold` (0 means the default) |

Accessors: `set_in_memory_relaylog()`, `is_in_memory_relaylog()`, `set_in_memory_relaylog_limit()`, `get_in_memory_relaylog_limit()`, `set_in_memory_relaylog_spill_threshold()`, `get_in_memory_relaylog_spill_threshold()`. `is_in_memory_relaylog()` returns false for the `group_replication_applier` channel whatever is stored. The getters return the default when the stored value is 0. The feature is active on a channel exactly when `is_in_memory_relaylog() && is_csa_enabled()`, which is when the channel has a runtime.

#### Validation rules

In `change_master()`, in this order:

1. Any IMR option while either thread runs: `ER_REPLICA_CHANNEL_MUST_STOP`.
2. In `validate_in_memory_relaylog_options()`, run only if the statement sets an IMR option:
   * the channel is not on CSA after the statement (`is_or_will_csa_be_enabled()` is false): `ER_CRST_IN_MEMORY_RELAYLOG_ONLY_FOR_CSA`, for any of the three options and any value;
   * `IN_MEMORY_RELAYLOG_ENABLED = ON` on `group_replication_applier`: `ER_REPLICA_IN_MEMORY_RELAYLOG_INCOMPATIBLE_CONFIGURATION` with "the group_replication_applier channel";
   * a bound is set: the effective pair, where an unset bound takes the channel's current value, goes to `Imr_service::validate_options()`; a failure gives `ER_CRST_IN_MEMORY_RELAYLOG_INVALID_CONFIG` and changes nothing.

Semisync is checked at start, not at CRST: `START REPLICA` and `START REPLICA IO_THREAD` fail with `ER_REPLICA_IN_MEMORY_RELAYLOG_INCOMPATIBLE_CONFIGURATION` ("semi-synchronous replication") if the channel has a runtime and `rpl_semi_sync_replica_enabled` is ON. `START REPLICA SQL_THREAD` is not affected. The receiver also checks before each dump request, which covers auto-start at server startup and reconnects.

#### Persistence

The three settings are persisted with the applier metadata, in `mysql.slave_relay_log_info`:

| Column | Type | Default |
|---|---|---|
| `In_memory_relaylog` | `BOOLEAN NOT NULL` | 1 |
| `In_memory_relaylog_limit` | `BIGINT UNSIGNED NOT NULL` | 134217728 |
| `In_memory_relaylog_spill_threshold` | `BIGINT UNSIGNED NOT NULL` | 16777216 |

They are fields 18, 19 and 20 of the applier metadata (`APPLIER_METADATA_LINES_WITH_IN_MEMORY_RELAYLOG`, `..._LIMIT`, `..._SPILL_THRESHOLD`), written by `Relay_log_info::write_info()` and `clear_info()` and read by `read_info()`. A new channel gets `IN_MEMORY_RELAYLOG_ENABLED = ON` from both the member default (`m_in_memory_relaylog{true}`) and the column default. Metadata written by a server without these fields reads `IN_MEMORY_RELAYLOG_ENABLED` as OFF and the bounds as their defaults, so an upgraded channel keeps its running configuration. The settings are kept across `APPLIER_VERSION` changes and server restarts.

`performance_schema.replication_applier_configuration` shows them as `IN_MEMORY_RELAYLOG_ENABLED` (`YES`/`NO`, from `is_in_memory_relaylog()`), `IN_MEMORY_RELAYLOG_LIMIT` and `IN_MEMORY_RELAYLOG_SPILL_THRESHOLD`.

### Error Handling and Reporting

#### Receiver side

Receiver and queue errors stop the IO thread and are reported through `Last_IO_Errno`, `Last_IO_Error`, `Last_IO_Error_Timestamp` and the error log, as other IO thread errors.

| Condition | Recoverable | Reported as |
|---|---|---|
| receiver stop while waiting in admission | yes, normal stop | information line "[In-memory relay log] receiver stopped while waiting for queue admission"; no error |
| KILL of the IO thread while waiting in admission | yes, START REPLICA IO_THREAD | error line `ER_RPL_REPLICA_IO_THREAD_ABORTED_WAITING_FOR_RELAY_LOG_SPACE` "Replica I/O thread aborted while waiting for relay log space", as on the disk path; `Last_IO_Errno` stays 0 |
| spill file cannot be created or its prefix written | after the cause is fixed, START | `ER_REPLICA_RELAY_LOG_WRITE_FAILURE` with "[In-memory relay log] failed to admit incoming transaction: " and the writer's reason; the envelope is truncated before it is published |
| spill file write or flush fails mid-transaction | after the cause is fixed, START | `ER_REPLICA_RELAY_LOG_WRITE_FAILURE` with "[In-memory relay log] failed to append event to incoming transaction: " and the reason; the transaction is truncated |
| disk full during a spill write | yes, waits | "Waiting for disk space", as for the relaylog |
| semisync enabled at dump request | after semisync is disabled, START | `ER_REPLICA_IN_MEMORY_RELAYLOG_INCOMPATIBLE_CONFIGURATION` |
| GTID event without a valid `trx_length` (source older than MySQL 8.0.2) | after `IN_MEMORY_RELAYLOG_ENABLED = OFF`, START | `ER_REPLICA_RELAY_LOG_WRITE_FAILURE` with "[In-memory relay log] the source does not send a valid transaction length ..."; nothing is reserved or queued |

In every failure the transaction's GTID is not added to `Retrieved_Gtid_Set`, so it is fetched again at the next start.

#### Applier side

| Condition | Reported as |
|---|---|
| event cannot be decoded (memory path) | byte source error "In-memory relay log: could not decode event #N of the transaction: <cause>"; the worker reports `ER_REPLICA_RELAY_LOG_READ_FAILURE` |
| spill file cannot be opened or read (includes keyring failures) | "Spill event set: could not open spill file: ..." or the stream error; `ER_REPLICA_RELAY_LOG_READ_FAILURE` |
| queue structure inconsistent (deque size and cursors disagree) | error log line with the counters; the reader reports `ER_REPLICA_RELAY_LOG_READ_FAILURE` "in-memory relay log queue structural inconsistency ..." and the coordinator stops; workers waiting in a byte source see the applier stop flag and end their wait within 100 ms, as on a stop |
| spill file of a finished transaction cannot be deleted | warning "[In-memory relay log] could not delete spill file '<path>' (errno N - text) ..."; the reader reports `ER_REPLICA_RELAY_LOG_READ_FAILURE` "could not delete an in-memory relay log spill file; see the error log" and the coordinator stops |
| apply error | unchanged CSA reporting |

Worker errors fill `performance_schema.replication_applier_status_by_worker` (`LAST_ERROR_NUMBER`, `LAST_ERROR_MESSAGE`, `APPLYING_TRANSACTION`) as on the disk path, so the failing transaction is identified by its GTID. An applier error leaves the queue in place; the next `START REPLICA SQL_THREAD` dispatches the uncommitted envelopes again. To fall back to the disk path after a non-recoverable error, the user stops both threads and disables the feature. The unapplied transactions are then fetched again by GTID auto-positioning.

#### Startup and START

| Condition | Effect |
|---|---|
| stale spill file cannot be deleted | each file logged as `ER_BINLOG_CANT_DELETE_FILE`, then a summary error; START of every IMR channel fails with `ER_REPLICA_IN_MEMORY_RELAYLOG_SPILL_STORAGE_UNUSABLE` until restart |
| spill directory unusable | cause and summary logged; same START failure |
| spill directory unreadable at purge | logged as an error, then a summary error; START of every IMR channel fails with `ER_REPLICA_IN_MEMORY_RELAYLOG_SPILL_STORAGE_UNUSABLE` until restart |
| stale relaylog files cannot be purged | warning only; START goes on |

The server always starts; only IMR channels are refused.

### Observability

#### performance_schema.replication_applier_metrics

The columns are added by `component_replication_applier_metrics` and are visible only while it is installed. The server side fills them in `applier_metrics_service_imp.cc`.

| Column | Kind | Source |
|---|---|---|
| `IN_MEMORY_BYTES_USED` | current | `rli->imr()->bytes_used()` |
| `IN_MEMORY_QUEUE_LENGTH` | current | `rli->imr()->queue_length()` |
| `IN_MEMORY_TRANSACTIONS_COUNT` | cumulative | statistic `imr_mem_trx_cnt` |
| `IN_MEMORY_TRANSACTIONS_BYTES` | cumulative | statistic `imr_mem_trx_len` |
| `SPILL_TRANSACTIONS_COUNT` | cumulative | statistic `imr_spl_trx_cnt` |
| `SPILL_TRANSACTIONS_BYTES` | cumulative | statistic `imr_spl_trx_len` |
| `SPILL_FILE_COUNT` | current | `rli->imr()->spill_file_count()` |
| `SPILL_BYTES_USED` | current | `rli->imr()->spill_bytes_used()` |

Current values pipeline. The gauges are read from the runtime when the row is built; a channel without a runtime reads 0. After a full stop the queue is empty, so they read 0. `SPILL_BYTES_USED` counts the bytes on disk, including the relaylog prefix and the encryption header.

Cumulative counters pipeline. `Statistics_map::init_statistics()` registers the four statistics when the applier starts, enabled if `Master_info::is_metric_collection_enabled()`. The commit hook adds 1 and `trx_length` with `Statistics_map::count_imr_completed()`, once per transaction, including transactions skipped because their GTID was already executed. `Csa_service::get_imr_applier_stats()` reads them under the CSA service read lock and returns zeros when the applier is not running. They are kept in memory only and start from 0 at each applier start.

#### SHOW REPLICA STATUS

Two columns are appended at the end of the result, so no existing column moves: `In_Memory_Relay_Log_Space` (`bytes_used()`) and `In_Memory_Queue_Length` (`queue_length()`, which counts undispatched, in-flight and committed-but-not-swept envelopes). Both read 0 for a channel without a runtime.

#### Thread stages

While the receiver waits for queue space, its stage is "Waiting for the replica SQL thread to free relay log space", as on the disk path when `relay_log_space_limit` is reached. While the coordinator waits for an envelope, its stage is "Replica has read all relay log; waiting for more updates".

### Low-Level Design

#### Byte-source layer classes

##### Cached_event_memory

An `IReader_event` that keeps an owning copy of one event's bytes (`std::vector<unsigned char> m_bytes`) and the FDE needed to deserialize it. `decode()` builds a fresh `Log_event` from the copy on every call, so a worker retry or a re-dispatch decodes the same bytes again. `decode(std::string *error)` also returns the cause of a failure. `reset()` is a no-op because the copy is immutable. The memory byte source creates it with `verify_checksum = false`.

##### Event_set_fetchable_memory

The memory path byte source, modeled on `Event_set_fetchable_cache`.

Key members:

* `m_events`: the vector of `IReader_event_ptr` appended by the receiver.
* `m_stream_mutex`, `m_stream_cv`, `m_stream_open`, `m_stream_sealed`, `m_stream_truncated`: the streaming state shared by the receiver and the worker.
* `m_event_id`, `m_is_done`, `m_status`, `m_failure_msg`, and the decompression state: consumer state, used by the worker that reads the batch.
* `m_envelope`, `m_fetchable_trx`: non-owning back-pointers to the owning envelope and `Fetchable_transaction`.
* `m_stats_instance_id`: the channel instance id for the statistics.
* `m_applier_stop`: pointer to the queue's atomic applier stop flag.

Methods:

* `append_event(buf, len, seal_after)`: wraps a copy of the bytes in a `Cached_event_memory` and calls `append_reader_event()`. It always returns false, because a memory append has no write that can fail.
* `append_reader_event()`: ignores the event if the stream is closed; calls `Fetchable_transaction::update_max_event_length(len)`; on the last event calls `Fetchable_transaction::set_fetching_complete()`; then, under `m_stream_mutex`, pushes the event and sets `m_stream_sealed`; then notifies `m_stream_cv`. The transaction is marked complete before its last event becomes visible, so a worker that fetches the last event already sees it as the last event of the transaction, as on the disk path.
* `seal_stream()`: seals without an event; marks the transaction complete only if the stream is not truncated.
* `set_stream_truncated()`: marks the `Fetchable_transaction` truncated, then the envelope, then sets `m_stream_truncated` and `m_stream_sealed`, and notifies. A truncated stream never marks the transaction complete, so the worker rolls it back.
* `wait_next()`: waits on `m_stream_cv` while no unread event exists and the stream is not sealed or truncated. With an applier stop flag, the wait is `wait_for(kApplierStopPollInterval)` (100 ms) and it returns false when the flag is set and the batch is not done; `Fetchable_transaction::wait_next()` reads that as an applier stop.
* `fetch_next()`: takes the next event under `m_stream_mutex`, decodes it outside the lock, starts decompression on a `Transaction_payload_event`, and returns a `Managed_event`. A decode failure sets the error "In-memory relay log: could not decode event #N of the transaction", followed by the cause.
* `set_success()`: the commit hook, described in "Commit hook".
* `reset(reset_events)`: resets the consumer state for a retry or a new job. It keeps `m_stream_truncated`, which is receiver state.

##### Event_set_fetchable_spill

The spill path byte source. It has the same streaming state and back-pointers as the memory byte source, plus:

* `m_writer`: `std::unique_ptr<Spill_file_writer>`, the owner of the file.
* `m_start_file_pos`: the logical offset of the first event, after the relaylog prefix.
* `m_published_end_file_pos`: the logical end position the worker may read up to; it advances after each written and flushed event.
* `m_reader`: a `Relaylog_file_reader` created with `opt_replica_sql_verify_checksum`, and `m_input_stream`, the decompressing stream over it.
* `m_writer_failed` (atomic) and `m_writer_failure_msg`: writer-only error state, written once by the receiver. The consumer keeps its own `m_status` and `m_failure_msg`. A reader error therefore never stops the writer, and a writer error is still visible to the consumer through `is_error()` and `get_error_str()`.

Methods:

* Constructor: creates the `Spill_file_writer`, hands it the spill disk reservation, and calls `open()` and `flush()`. On failure it records the writer failure, deletes the file with `discard_after_setup_failure()`, and leaves the stream closed. `Transaction_envelope::validate_payload()` and `open_transaction()` then detect it through `has_io_error()`.
* `append_event(buf, len, seal_after)`: calls `Spill_file_writer::append_raw()` and `flush()`. On failure it truncates the stream, then records the writer failure, and returns true: consumers see the writer failure through `is_error()`, so a worker that sees it must already see the truncation and roll back, not report a read error. On success it calls `update_max_event_length(len)`, marks the transaction complete on the last event, publishes the new end position and the seal under `m_stream_mutex`, and notifies.
* `wait_next()`: opens the reader on first use at `m_start_file_pos`, then waits until the reader position is below the published end, or the stream is sealed or truncated, with the same 100 ms applier stop poll as the memory byte source.
* `fetch_next()`: reads the next event through the decompressing stream.
* `reset()`: closes the reader and clears the consumer state, so a retry reads the file from the start; the published position and the receiver flags are kept.
* `set_success()`: the commit hook, as for the memory byte source but counted as spill path.

A worker can start applying a spill path transaction while the receiver is still writing its file.

##### Spill_file_writer

Owns one spill file. It does not touch the filesystem until `open()`.

* `kTempSubdirName = "#in_memory_relaylog_temp_files"` and `kFileNamePrefix = "imr_sp_"`.
* `open()`: composes the path under the spill directory (which must exist) and creates the file with `create_unique_file()`. The file name is `imr_sp_<channel>_<id>` for a named channel and `imr_sp_<id>` for the default channel, where `<channel>` is the channel name converted with `strconvert()` from `system_charset_info` to `my_charset_filename` (as `Relay_log_info::add_channel_to_relay_log_name()` does) and `<id>` is the next value of a process-global atomic counter in lowercase hex. The file is created with `O_CREAT | O_EXCL`; a name that exists is skipped, up to 1024 attempts. `open()` then opens an `IO_CACHE_ostream` with `MY_WME | MY_NABP | MY_WAIT_IF_FULL | MY_REPORT_WAITING_IF_FULL`, the flags of the relaylog file. If `rpl_encryption.is_enabled()`, it puts a `Binlog_encryption_ostream` on top, which writes the 512-byte encryption header, and flushes the header. Then it writes `BINLOG_MAGIC` and a private copy of the FDE marked as a relaylog event, and registers the file in the channel's spill gauges.
* `append_raw(buf, len)`: writes the bytes and advances `m_end_pos` and the byte gauge.
* `flush()`: flushes the `IO_CACHE` to the file so a concurrent reader sees the bytes. It does not fsync.
* `close_stream()`: closes the output stream (the `IO_CACHE`, and the encryption stream on top of it) and keeps the file, the gauges and the disk reservation, which the destructor still releases. `Event_set_fetchable_spill::append_event()` calls it after the last event and after a write failure, so a transaction waiting in the queue holds no file descriptor; readers and the key rotation open the file by name. A second call does nothing.
* `end_position()`: the logical size (prefix and appended bytes). It excludes the encryption header, which is counted separately in `m_physical_overhead`.
* Destructor: returns the file's bytes and slot to the spill gauges, closes the stream if `close_stream()` has not, deletes the file with `my_delete()`, and then returns the spill disk reservation to the queue. A failed delete logs a warning with the path and errno, leaves the file for the next startup purge, and calls `m_owner_queue->set_spill_delete_failed()`. The setup error paths delete the file the same way but do not set the record: `open()` deletes it after an `IO_CACHE` open or encryption header failure, and the `Event_set_fetchable_spill` constructor calls `discard_after_setup_failure()` after any other `open()` or `flush()` failure, so the destructor finds no file. On every setup failure, `discard_after_setup_failure()` also returns the spill disk reservation and, if `open()` had counted the file, its gauges right away, so the destructor releases nothing a second time.

`open()` runs under the channel's relaylog `LOCK_log`, because it is reached only from `queue_event()`. That lock is the barrier against a concurrent master key rotation (see "Spill Path").

##### Spill directory functions

* `spill_directory_path(relay_log_dir)`: the relaylog directory joined with `kTempSubdirName`.
* `ensure_spill_directory(relay_log_dir)`: creates the directory with mode `S_IRWXU | S_IRGRP | S_IXGRP` (the mode `mysqld --initialize` gives the data directory) if it is absent. The path is unusable if it cannot be created, is a symbolic link, is not a directory, or lacks read, write and search permission for the server. An unusable path is logged as an error naming the directory and the cause, and returned to the caller.
* `purge_orphan_spill_files(relay_log_dir)`: deletes every entry whose name starts with `imr_sp_`, never enters or removes a directory, and leaves every other entry alone. It lists the directory without `MY_WANT_STAT`, because with it `my_dir()` drops entries that lack the owner-read bit, and calls `my_stat()` on each prefixed entry to skip directories. A file that cannot be deleted is logged as `ER_BINLOG_CANT_DELETE_FILE` and the purge goes on. A missing directory is not an error. A listing failure other than a missing or non-directory path is logged as an error with the errno and counts as a purge failure. A spill path that is a symbolic link is skipped without reading it and is not an error; `ensure_spill_directory()`, which runs after the purge, rejects it.

##### Spill encryption functions

`imr_reencrypt_spill_files()` is declared in `sql/rpl_replica.h` and called by `ALTER INSTANCE ROTATE BINLOG MASTER KEY` right after `reencrypt_relay_logs()`. It lists the spill directories of the channels that have a runtime (`spill_dirs_of_live_runtimes()`, under the `channel_map` read lock) and calls `reencrypt_spill_files_in_dirs()`. For every `imr_sp_*` file whose header names a key other than the current master key (`parse_spill_header()` reads the key id, because `Rpl_encryption_header` has no accessor for it), the pass decrypts the file password with that key and writes a new 512-byte header with the same password, the current master key and a new initialization vector, in place at offset 0 (`Header_rewrapper`). It opens files without `O_CREAT`, so a file deleted meanwhile is skipped. It reads and writes only bytes [0, 512). A header that is corrupt, or names a key that is not in the keyring (which includes a key of another server), is skipped with a warning.

#### Queue layer classes

##### Envelope_path

`enum class Envelope_path { MEMORY, SPILL }` is the store path of an envelope, fixed at envelope construction. `Trx_envelope_queue::classify()` returns `Envelope_path` (`MEMORY` or `SPILL`).

##### Envelope_state

`enum class Envelope_state { ACTIVE, COMMITTED, TRUNCATED }` is the final result of an envelope, kept in `Transaction_envelope::m_state` under the envelope's `m_mutex`. It is ACTIVE until `commit()` or `set_truncated()` runs; COMMITTED and TRUNCATED are final, and the first one set wins.

##### Trx_payload

Key members: `m_trx` (`std::shared_ptr<Fetchable_transaction>`), `m_trx_length` (the bytes reserved; 0 for a spill payload), `m_owner_queue` (non-owning), `m_sink` (non-owning pointer to the byte source as `Streaming_event_sink`), `m_fde` (the runtime's read-only FDE snapshot when the transaction was opened). The class is neither copyable nor movable.

Factories:

* `create_memory(is_trx, fde, trx_length, owner_queue, owner_envelope)`: calls `owner_queue->acquire_admission(trx_length, MEMORY)`, which reserves `trx_length` on the memory resource. It then builds an empty `Event_set_fetchable_memory`, wraps it as the single batch of a new `Fetchable_transaction`, links them, and returns the payload. It returns null if a receiver stop was requested during admission; nothing is then reserved.
* `create_spill(...)`: calls `acquire_admission(trx_length, SPILL)`, which reserves `trx_length` on the spill disk resource, builds an `Event_set_fetchable_spill` that takes over that reservation, and returns a payload that reserves 0 memory bytes.

Destructor: `m_owner_queue->release_bytes(m_trx_length)`. The release is a single atomic operation with no lock and no notify; a receiver waiting for space sees it on its next poll. The payload is the RAII owner of the memory reservation.

`fetchable()` returns a copy of the `shared_ptr`, which the CSA reader puts in the `Job_applier`.

##### Transaction_envelope

Key members: `m_mutex`, `m_trx_length`, `m_path`, `m_state` (`Envelope_state`: ACTIVE, COMMITTED or TRUNCATED), `m_receiver_held`, `m_payload` (`std::unique_ptr<Trx_payload>`). `trx_length()` and `path()` are immutable and read without the lock. `m_state`, `m_receiver_held` and the payload reference are read and written only under `m_mutex`.

Methods:

* `create_payload(is_trx, fde, owner_queue)`: calls the factory of `m_path`, then stores the payload under `m_mutex`. Returns true on a receiver stop during admission.
* `validate_payload()`: marks the envelope truncated if the new sink reports `has_io_error()` (only a spill file can fail to provision). It runs before the envelope is published, so the coordinator never dispatches a broken transaction.
* `hold_for_receiver()`: sets `m_receiver_held`; called by `enqueue()` before the push.
* `release_receiver()`: clears the hold and, if the envelope is already committed, resets the payload. This is the receiver's last access to the envelope.
* `commit()`: if `m_state` is not ACTIVE (already committed or truncated), returns true and changes nothing. Otherwise sets `m_state` to COMMITTED and resets the payload unless the receiver still holds the envelope. Returns false on success. It takes only `m_mutex`; a committing worker never takes the queue mutex.
* `set_truncated()`: sets `m_state` to TRUNCATED only if it is ACTIVE; a committed envelope stays committed. The first final state wins.
* `is_reclaimable()`: `!m_receiver_held && m_state != ACTIVE`, under one lock.
* `state()`, `is_committed()`, `is_truncated()`: read `m_state` under `m_mutex`. Dispatch skips an envelope whose `state()` is not ACTIVE.
* `payload()`, `reset_payload()`, `current_sink()`: payload access under `m_mutex`.

##### Trx_envelope_queue

Key members:

* `m_queue_mutex`, `m_not_empty_cv`: the queue mutex and the consumer condition variable.
* `m_envelopes`: `std::deque<std::unique_ptr<Transaction_envelope>>`. The `unique_ptr` keeps each envelope's address stable from enqueue until it is popped.
* `m_commit_seqno`, `m_dispatch_seqno`, `m_insert_seqno`: the cursors, counted in envelopes since the last reset.
* `m_memory_limit`, `m_spill_threshold`, `m_relay_log_dir`, `m_channel_name`, `m_channel_instance_id`: immutable after construction.
* `m_memory_resource`, `m_spill_disk_resource`: `Resource_entry *const`, the channel's `in_memory_relaylog_memory` and `in_memory_relaylog_spill_disk` entries; null means the limit is off.
* `m_receiver_stopped` (atomic), `m_applier_stopped` (atomic, starts set, written only under the queue mutex), `m_applier_error` (under the queue mutex): stop and error flags. `applier_stop_flag()` returns `&m_applier_stopped` for the byte sources and the reader.
* `m_spill_file_count`, `m_spill_bytes_used` (atomics): the live spill gauges.
* `m_spill_delete_failed` (atomic): a spill file delete failed. Set by `Spill_file_writer` without a lock, since the writer can be destroyed under `m_queue_mutex` in `reset()`; read and cleared by the reader with `take_spill_delete_failed()`.

Invariants, held under `m_queue_mutex`. The first two are asserted by `assert_cursor_invariant()`; the third follows from them and is how `dispatch_next_locked()` finds the envelope:

* `commit_seqno <= dispatch_seqno <= insert_seqno`;
* `m_envelopes.size() == insert_seqno - commit_seqno`;
* the envelope at `dispatch_seqno` is `m_envelopes[dispatch_seqno - commit_seqno]`.

Producer methods:

* `classify(trx_length)`: `SPILL` if `trx_length > spill_threshold`, else `MEMORY`. Routing only.
* `acquire_admission(trx_length, path)`: rejects at once if the receiver stop flag is set. Memory path: one `Resource_instance_monitor::try_lock_resource()` (a single atomic operation on success); while it fails, sleep 1 ms and retry, checking the receiver stop flag each time. Spill path: the same loop with `try_lock_resource_soft()` on the spill disk entry. No mutex and no condition variable is involved.
* `is_admissible(trx_length)` and `wait_until_admissible(trx_length)`: the same rules, read-only (`would_lock()`, `would_lock_soft()`). The receiver calls the wait before it takes `LOCK_log`. The wait also ends when the calling thread is killed (`THD::is_killed()` of `current_thd`); `acquire_admission()` checks only the stop flag.
* `enqueue(trx_length, is_trx, fde, receiver_held)`: classify; build the envelope; `create_payload()` (admission, outside the queue mutex); `validate_payload()`; `hold_for_receiver()` if requested; then under `m_queue_mutex` push the envelope and increment `insert_seqno`; then `notify_one()` on `m_not_empty_cv`. Returns null on a receiver stop.

Consumer methods:

* `sweep_committed(need_lock)`: while the head is reclaimable and `commit_seqno < dispatch_seqno`, pop it (which destroys the envelope and its payload) and increment `commit_seqno`. If the deque size disagrees with the cursors, it logs the counters, sets `m_applier_error` and returns true.
* `dispatch_next_locked(lock)`: waits on `m_not_empty_cv` with a 100 ms timeout (`kDispatchPollInterval`) until `dispatch_seqno < insert_seqno` or the applier is stopped. On a timeout or a stop it returns null without moving a cursor. Otherwise it returns the envelope at `dispatch_seqno` and increments the cursor.
* `sweep_and_dispatch()`: under one hold of `m_queue_mutex`, sweeps, dispatches, and skips any dispatched envelope that is already committed or truncated, until it finds a live one or returns null. On a structural error it sets `m_applier_stopped` and returns null; workers waiting in a byte source see the flag on their next poll and end, as on a stop. The CSA reader is its only caller, which makes the coordinator the only sweeper and dispatcher.

Lifecycle methods:

* `stop(scope)`: RECEIVER sets `m_receiver_stopped` (release store). APPLIER sets `m_applier_stopped` under the queue mutex and then calls `notify_all()`; workers waiting in a byte source read the same flag on their next poll. A stop of one role does not disturb the other. `scope` is `RECEIVER` or `APPLIER`; every caller names one role, and there is no default.
* `resume(scope)`: RECEIVER clears its flag. APPLIER clears `m_applier_stopped`, `m_applier_error` and `m_spill_delete_failed`, and sets `dispatch_seqno = commit_seqno`, all under the queue mutex, so every retained uncommitted envelope is dispatched again.
* `reset()`: clears the deque and zeroes the cursors. Destroying the envelopes destroys the payloads, which releases the memory reservations and deletes the spill files. It then clears `m_spill_delete_failed`.

Accounting methods: `release_bytes(n)` releases on the memory entry; `bytes_used()` returns `clamp(limit - available, 0, limit)` from the memory entry, so there is no second counter to keep in sync; `release_spill_reservation(n)` releases on the spill disk entry; `add_spill_bytes()`, `release_spill_bytes()`, `inc_spill_files()`, `dec_spill_files()` maintain the gauges, called by `Spill_file_writer`; `set_spill_delete_failed()` and `take_spill_delete_failed()` set and consume `m_spill_delete_failed`.

#### Runtime layer classes

##### Open_group and Receiver_write_failure

`Open_group { Transaction_envelope *envelope; Streaming_event_sink *sink; }` is the receiver's open transaction; both fields are null outside a transaction. `Receiver_write_failure { bool stopped; std::string reason; }` records why the last receiver write call failed: `stopped` for a stop during admission, `reason` for a write failure.

##### Receiver write functions

* `open_transaction(queue, group, fde, trx_length, is_trx, failure)`: truncates a group that is still open (a new GTID event means that transaction will not be continued); calls `queue.enqueue(..., receiver_held = true)`; stores the envelope and its sink in `group`. If the new sink reports a provisioning failure, it records the reason "[In-memory relay log] failed to admit incoming transaction: ..." and closes the group. Returns true on a stop or a failure.
* `append_transaction_event(group, buf, len, is_terminal, failure)`: forwards the bytes to `group.sink->append_event(buf, len, is_terminal)`. On a write failure it records "[In-memory relay log] failed to append event to incoming transaction: ..." and closes the group. On the last event it closes the group after the append returns.
* `truncate_transaction(group)`: calls `set_stream_truncated()` on the sink and closes the group.
* Closing a group clears both fields first and then calls `release_receiver()` on the envelope.

The receiver holds at most one envelope, and it releases that envelope before it waits in admission for the next one. It never waits for space that it holds itself.

##### Imr_runtime

Key members: `m_queue` (`Trx_envelope_queue`, by value), `m_role_mutex`, `m_receiver_group`, `m_last_write_failure`, `m_fde_snapshot` (the read-only copy of the last source FDE), `m_receiver_owner` and `m_applier_owner` (`std::atomic<std::thread::id>`).

The constructor takes an `Imr_config`, the relaylog directory, the memory resource entry, the channel name, the spill disk resource entry and the channel instance id, and constructs the queue with them. The destructor asserts that both roles are detached and no group is open.

Role methods:

* `attach_role(scope)`: under `m_role_mutex`, refuses (returns true) if the role already has an owner; otherwise records the calling thread as owner, calls `m_queue.resume(scope)`, and for the receiver clears `m_last_write_failure`.
* `detach_role(scope, mi)`: under `m_role_mutex`, a no-op unless the role is attached and the calling thread owns it. RECEIVER: truncates and closes the open group and releases `m_fde_snapshot`. APPLIER: runs `sweep_committed()` (the workers are gone by then). Then `m_queue.stop(scope)`, clears the owner, and calls `reset(mi)` if no role is attached any more.
* `request_stop(scope)`: `m_queue.stop(scope)`. It takes no lock of its own (not `m_role_mutex`); for the applier, `Trx_envelope_queue::stop()` takes `m_queue_mutex` briefly to set `m_applier_stopped` and wake the coordinator. It may be called before the role's thread has joined.
* `is_role_owner(scope)`: compares the owner id with the calling thread's id (relaxed load).
* `is_attached(scope)`: whether the role has an owner (acquire load).
* `fully_detached()`: neither role has an owner.

Receiver write methods, callable only by the receiver owner: `wait_for_admission(trx_length)` (returns at once if the transaction fits; otherwise truncates a still-open group and calls `wait_until_admissible()`), `set_receiver_fde()`, `open_transaction()`, `append_event()`, `truncate_receiver_group()` (checked for ownership in all builds), `receiver_sink()`, `last_write_failure()`. These use `m_receiver_group` without a lock, because only the owner thread touches it. `set_receiver_fde(fde)` copies the `Format_description_event` part of `fde` into a new object, as `Spill_file_writer::open()` does, and keeps it in `m_fde_snapshot`. `open_transaction(fde, ...)` hands the new transaction `m_fde_snapshot`, never `fde`. In debug builds it asserts that a snapshot exists and that its decoding fields (checksum algorithm, binlog version, server version, post-header lengths, common header length) equal those of `fde`. In release builds, a transaction that arrives before any FDE event gets a copy of `fde`. The connect-time FDE only decodes the fake Rotate and the first source FDE event.

Queue reset methods:

* `reset(mi)`: requires `fully_detached()`. Calls `clear_received_gtid_state(mi)`, then `m_queue.reset()`. The two always go together: the queue is volatile, so `Retrieved_Gtid_Set` must not keep naming transactions that no longer exist on the server.
* `clear_received_gtid_state(mi)` (static): resets `mi->transaction_parser`; clears the receiver GTID monitoring info under `Master_info::data_lock`; clears `Retrieved_Gtid_Set` and its TSID map under the relaylog TSID write lock.

`reconcile_action(want_runtime, has_runtime, config_matches)` (static) returns the reconcile transition (see "State Machines").

#### Service layer classes

##### Imr_service

Key members: `m_runtimes` (`std::unordered_map<std::size_t, std::unique_ptr<Imr_runtime>>`, keyed by `rli->get_channel_instance_id()`), `m_spill_cleanup_failed`, `m_spill_dir_unusable`. `m_runtimes` has no lock of its own: it is changed only by `reconcile()` and `remove()`, under the `channel_map` write lock (a `reconcile()` with no runtime and none wanted returns before it touches the map, see "Runtime reconcile"), and it is never read on another path.

Option bounds: `kLimitMin = 32 MiB`, `kLimitMax = 4294967295`, `kSpillThresholdMin = 8 MiB`.

Methods:

* `validate_options(limit_specified, effective_limit, spill_threshold_specified, effective_spill_threshold)`: checks, first failure wins, (1) a specified limit lies in [`kLimitMin`, `kLimitMax`], (2) a specified threshold is at least `kSpillThresholdMin`, (3) the effective threshold is below the effective limit. A failure returns `ER_CRST_IN_MEMORY_RELAYLOG_INVALID_CONFIG` with a reason naming the values and the violated bound.
* `reconcile(rli)`: see "Runtime reconcile" under "State Machines".
* `remove(rli)`: clears `rli`'s cached pointer and erases its entry. Called only from `~Relay_log_info()`, so every path that deletes a channel (`RESET REPLICA ALL`, shutdown) removes its runtime.
* `attach_receiver(mi)`, `attach_applier(mi)` (static): `rli->imr()->attach_role(...)`; no-op (false) on a channel without a runtime.
* `request_receiver_stop(mi)`, `request_applier_stop(mi)` (static): `rli->imr()->request_stop(...)`.
* `detach_receiver(mi)`, `detach_applier(mi)` (static): `rli->imr()->detach_role(..., mi)`.
* `init_spill_storage()`: at startup, purges orphan spill files in the spill directory derived from the server-global `relay_log_basename`, then ensures that directory exists. A purge failure sets `m_spill_cleanup_failed`; an unusable directory sets `m_spill_dir_unusable`. Each also logs a summary error. Both flags are written once before any replication thread runs and never cleared, so they are read without a lock.

#### Integration layer classes and functions

##### Queued_transaction_reader

Implements the CSA `Reader` interface (`read()`, `is_stopped()`, `is_error()`, `stop()`). Members: `m_instance_id`, `m_rli`, `m_queue` (non-owning), `m_channel` (the `Channel` given to each job), `m_stat_monitor`, `m_resource_monitor`, `m_prev_seq` and `m_prev_fde` (logical clock check), `m_is_error`, `m_stopped`.

`read()`:

1. Return null if stopped. If `m_queue->take_spill_delete_failed()` returns true, report `ER_REPLICA_RELAY_LOG_READ_FAILURE`, set `m_is_error` and return null. This runs on every call, including idle polls, so a failure from the last transaction stops the applier too. Set the coordinator stage to "Replica has read all relay log; waiting for more updates".
2. `m_queue->sweep_and_dispatch()`. On null: if the queue reports `applier_error()`, report `ER_REPLICA_RELAY_LOG_READ_FAILURE` once; if a `START REPLICA UNTIL` condition is met with an empty queue, stop for UNTIL; return null. The run loop calls `read()` again, so the coordinator wakes at least every 100 ms.
3. Set the stage back to working. Take `envelope->payload()->fetchable()` and build `new Job_applier(m_channel.get(), slave_trans_retries, fetchable, m_stat_monitor, m_resource_monitor)`.
4. Drop the job without running it if the transaction is already truncated, if the applier stop flag is set, or if `START REPLICA UNTIL` excludes it. The envelope stays in the queue. If the UNTIL condition is met after this transaction, the reader returns the job and stops for UNTIL after it, as the disk reader does.
5. Run the same logical clock consistency check as the disk reader, with "in-memory relay log" as the file name; a new FDE snapshot (taken for each source Format_description event) resets it.
6. Return the job.

`stop()` sets `m_stopped` and calls `m_queue->stop(APPLIER)`. `is_stopped()` is true on an error, a local stop, or `sql_slave_killed()`, as for the disk reader. It does not read the queue's stop flags: `stop()` sets `m_stopped` first, STOP and shutdown set `abort_slave` before they set the applier flag, and a structural error sets `m_is_error` in the same `read()`.

##### Commit hook

When a worker finishes a transaction successfully, `Task_exec_job` calls `Job::set_success()`. For a job in phase `done` whose transaction is not truncated, `Job_binlog::set_success()` calls `Fetchable_transaction::set_success()`, which calls `set_success()` on each batch under `Fetchable_transaction::m_mutex`. The IMR byte source's `set_success()`:

1. returns if the worker's THD is killed;
2. returns if the `Fetchable_transaction` is truncated or aborted (an applier stop rolled the job back; the envelope stays for re-dispatch);
3. reads `trx_length` and the instance id, then calls `m_envelope->commit()`;
4. if `commit()` returned false, calls `Statistics_map::count_imr_completed(instance_id, spill_path, trx_length)`.

`commit()` resets the payload. That releases the memory reservation at once (`~Trx_payload()`) and drops the envelope's reference to the `Fetchable_transaction`. Right after the hook returns, `Job_binlog::set_success()` drops the job's reference (`m_fetch_metadata.reset()`). That is normally the last reference, so the `Fetchable_transaction` and its byte source are destroyed there, on the worker thread. On the spill path this deletes the spill file and returns the spill disk reservation (`~Spill_file_writer()`). If the receiver still holds the envelope, `commit()` keeps the payload, and `release_receiver()` resets it later. A transaction that the worker skips because its GTID is already in `gtid_executed` (`is_already_logged_transaction()`) still ends successfully and runs the commit hook, so its envelope is committed and counted too.

##### Server receiver adapters

All in `sql/rpl_replica.cc`:

* `imr_wait_for_admission(mi, trx_length)`: called by `handle_slave_io()` for a GTID or anonymous GTID event, with no lock held. If the transaction does not fit, sets the IO thread stage to `stage_waiting_for_relay_log_space` ("Waiting for the replica SQL thread to free relay log space"), waits in `Imr_runtime::wait_for_admission()`, and restores the stage. Returns true if the wait ended on a stop or a KILL. If the IO thread is killed, `handle_slave_io()` then logs `ER_RPL_REPLICA_IO_THREAD_ABORTED_WAITING_FOR_RELAY_LOG_SPACE` and exits, as after the disk path's relay-log-space wait; after a stop it goes on to `queue_event()`, which reports the stop.
* `imr_on_gtid_event(mi, trx_length)`: `imr->open_transaction(*mi->get_mi_description_event(), trx_length)`, under `LOCK_log`; the transaction gets the runtime's FDE snapshot. `QUEUE_EVENT_ERROR_QUEUING` on failure.
* `imr_on_body_event(mi, buf, len, is_terminal)`: `imr->append_event()`.
* `imr_on_truncate(mi)`: `imr->truncate_receiver_group()`.
* `after_write_to_in_memory_relay_log(mi)`: at the end of a transaction, adds the GTID to `Retrieved_Gtid_Set` under the TSID read lock and updates the receiver monitoring info, as `MYSQL_BIN_LOG::after_write_to_relay_log()` does on the disk path.
* `imr_clear_ignored_events_info(mi)`: after an event is queued successfully, clears the ignored-events source position (`rli->ign_master_log_name_end`) that `queue_event()` records when the server-id filter drops events. It runs with `LOCK_log` held and takes the binlog end position lock for the write. `MYSQL_BIN_LOG::after_write_to_relay_log()` does the same on the disk path.
* `imr_report_queue_event_failure(thd, mi)`: see "Error Handling and Reporting".
* `imr_check_semisync_before_dump(thd, mi)`: see "Configuration and Persistence / Validation rules".
* `imr_purge_stale_relay_logs(thd, mi)`: see "Lifecycle Flows / Server startup".

`queue_event()` on a channel with a runtime:

1. Takes `LOCK_log`, feeds the transaction boundary parser and handles the event type as on the disk path. A Rotate event (real or the fake Rotate of a reconnect) calls `imr_on_truncate()`: an open transaction at that point is left over from a previous connection and will be sent again. A GTID event sets `mi->m_queueing_transaction_size` to `trx_length`. A source Format_description event also refreshes the runtime's FDE snapshot (`set_receiver_fde()`).
2. After the server-id filter, a GTID event calls `imr_on_gtid_event()`. A transaction that the filter drops opens nothing.
3. If a transaction is open (`receiver_sink()` not null), the event, including the GTID event itself, is appended with `imr_on_body_event()`; `is_terminal` is true when the parser is no longer inside a transaction.
4. Events outside a transaction (Format_description, heartbeat, Rotate, Previous_gtids) are not appended. A source Format_description event that changes the checksum algorithm is written to the relaylog file, so the replica's own events in that file verify.
5. On success the receiver positions are updated under `Master_info::data_lock` and, at the end of a transaction, `after_write_to_in_memory_relay_log()` runs. If an append fails, the open transaction is truncated, the parser is rolled back and the error goes to `handle_slave_io()`. If opening the transaction fails at the GTID event (a stop during admission, or a spill file that could not be created), `open_transaction()` leaves no group open, and the error goes to `handle_slave_io()` directly, with no parser rollback.

On a channel with a runtime, `handle_slave_io()` skips the disk path `relay_log_space_limit` wait, `process_io_rotate()` does not rotate the relaylog, `write_rotate_to_master_pos_into_relay_log()` does not write the Rotate event, and `flush_relay_logs()` returns at once.
