# Binlog Parallel Flush

- Primary issue: mysql/mysql-server#832
- Related issues: mysql/mysql-server#833 (depends on this proposal),
  mysql/mysql-community#114 (original proposal), mysql/mysql-community#126
  (discussion)
- Design: `design/binary-log/832-binlog-parallel-flush.md`
- Contributors: Libing Song, Genze Wu
---

## 1. High Level Design (HLD)

### 1.1 Executive Summary

On large servers with many concurrent writers, MySQL commit throughput
stops growing because the binary log is written for one group of
transactions at a time by a single session. Binlog Parallel Flush lets
many groups prepare their binary log data and finish their commits at
the same time, which improves write performance at high concurrency.

### 1.2 Motivation and User Stories

With the binary log enabled, the commit throughput of a large server stops
growing long before its CPUs and storage are busy: at high concurrency
the binary log is written for one group of transactions at a time, and
every other committing transaction waits for it.

Users need more throughput without giving up anything the binary log
guarantees today: the same content, durability and crash recovery, and
replication and tools that keep working unchanged.

- As a database administrator (DBA) I want MySQL to deliver higher write
  throughput with 128 or more concurrent connections on a large server so
  that I use the hardware I pay for.
- As a DBA I want every workload to benefit without configuration so
  that I do not have to know which transactions the feature supports.
- As a replication operator I want the binary log to keep its content
  and order so that replicas, `mysqlbinlog` and backup tools keep working.

### 1.3 Scope

Components and functional modules extended or modified:

- Binary log: the commit path (grouping transactions, writing them to the
  binary log file, syncing the file, committing them), which replaces
  today's binary log group commit for every transaction.
- Server configuration: new system variables and their observability,
  and the deprecation of the sync delay variables.

Out of scope:

- Windows, where the feature has not been tested.

### 1.4 Related Work

- Issue mysql/mysql-server#833, "Deferred Binlog Sync with Redo Protection",
  depends on this proposal. It addresses the second bottleneck, the two
  disk syncs in a row, by protecting binary log data with the InnoDB redo
  log so that the binary log file sync can leave the commit path.

### 1.5 References

- Dimitri Kravtchuk, "MySQL Performance: 8.0 and binlog group commit"
  (analysis showing the binary log as the scalability limit).

---

## 2. Requirements

### 2.1 Functional Requirements

- FR-1 The feature MUST apply to every commit that writes the binary log,
  whatever the transaction and the server settings, without a setting to
  enable it.
- FR-2 There MUST be no observable change in the behaviour of the binary
  log, including:
  - its content;
  - durability and crash recovery, except when the binary log is synced
    with `sync_binlog` other than 1;
  - reading a binary log file while it is being written;
  - replicas, `mysqlbinlog`, binlog dump clients and semisynchronous
    replication;
  - Group Replication;
  - the commit order kept by `binlog_order_commits` and
    `replica_preserve_commit_order`;
  - binary log administration statements;
  - error handling under `binlog_error_action`;
  - clean shutdown.

### 2.2 Non-Functional Requirements

- NFR-1 Performance: on the reference setup of issue #832 (64-core server,
  sysbench `oltp_write_only` and `oltp_update_non_index`, 8 tables x 1M
  rows, `sync_binlog=1`, `innodb_flush_log_at_trx_commit=1`,
  `binlog_order_commits=OFF`) the feature
  MUST deliver higher transactions per second than today at 128 threads and
  above, MUST NOT reduce transactions per second by more than 3% at
  32 threads or below, and the 99th percentile commit latency at 256
  threads MUST NOT be higher than today's.
- NFR-2 Performance where groups gain less: with
  `binlog_order_commits=ON`, and with transactions larger than
  `binlog_buffer_size`, throughput and commit latency MUST NOT be worse
  than today's.

---

## 3. High Level Specification (HLS)

### 3.1 Technical Summary

Today `ordered_commit()` runs three serialized stages, each led by one
session at a time:

```
flush stage  (LOCK_log)    : flush engine redo, assign GTIDs, format every
                             transaction cache into the binlog file
sync stage   (LOCK_sync)   : fsync according to sync_binlog
commit stage (LOCK_commit) : engine commit, GTID state, after_commit hooks
```

The stages form a pipeline: a group can sync or commit while the next
group is in the flush stage. But only one group at a time is in the
flush stage, and its leader formats and writes every transaction cache
under `LOCK_log`. That serialization is what the new path removes.

The new path keeps `LOCK_log` only for what fixes the binlog order, and
lets many groups do everything else concurrently. Groups A, B and C run
at the same time. The only step they take one after another is flush
prepare, because it decides the binlog order: it gives each group its
binlog position, global transaction identifiers (GTIDs) and sequence
numbers. It runs under `LOCK_log`;
every other step runs in parallel:

```
 Committing sessions (groups A, B, C)               Background
 ====================================               ==========

   group A      group B      group C
 ┌─────────┐  ┌─────────┐  ┌─────────┐
 │grouping │  │grouping │  │grouping │ flush stage queue:
 └────┬────┘  └────┬────┘  └────┬────┘ leader + followers
      │            │            │
      ▼            ▼            ▼
 ┌───────────────────────────────────┐
 │ flush prepare                     │
 │ (LOCK_log, one group at a time)   │
 └────┬────────────┬────────────┬────┘
      │            │            │
      ▼            ▼            ▼
 ┌─────────┐  ┌─────────┐  ┌─────────┐    flush     ┌──────────────────┐
 │ flush   │  │ flush   │  │ flush   │ ───────────► │ ring buffer      │
 └────┬────┘  └────┬────┘  └────┬────┘ in parallel  │ [ A | B | C ... ]│
      │            │            │                   └─────────┬────────┘
      ▼            ▼            ▼                             │ contiguous
 ┌─────────┐  ┌─────────┐  ┌─────────┐                        ▼ published groups
 │ wait    │  │ wait    │  │ wait    │    wake      ┌──────────────────┐
 │ flushed │  │ flushed │  │ flushed │ ◄─────────── │ flush thread     │
 │ /synced │  │ /synced │  │ /synced │ sync_binlog  └─────────┬────────┘
 │         │  │         │  │         │ != 1                   │ wake every
 │         │  │         │  │         │                        │ sync_binlog
 │         │  │         │  │         │                        │ groups
 │         │  │         │  │         │    wake                ▼
 │         │  │         │  │         │ ◄─────────── ┌──────────────────┐
 │         │  │         │  │         │ sync_binlog  │ sync thread      │
 └────┬────┘  └────┬────┘  └────┬────┘ = 1          └──────────────────┘
      │            │            │
      ▼            ▼            ▼
 ┌─────────┐  ┌─────────┐  ┌─────────┐
 │ commit  │  │ commit  │  │ commit  │ engine commit + GTID state,
 │ group   │  │ group   │  │ group   │ release followers
 └─────────┘  └─────────┘  └─────────┘
```

The boxes in the diagram, top to bottom, are the steps every group goes
through; the right column is shared by all groups:

1. **grouping.** Groups are formed by the flush stage queue of today's
   binary log group commit: committing sessions enter the queue, the first
   one becomes the leader and the others its followers.
2. **flush prepare** (`LOCK_log`, one group at a time). The leader
   computes the exact byte length of the group's events, assigns
   automatic GTIDs, sequence numbers and `last_committed`, and allocates
   the group's binlog position `(start, length)`.
3. **flush** (in parallel). Without holding `LOCK_log`, the leader flushes all
   caches of the group into the ring buffer. It then flushes the engine log, so
   that every transaction of the group is durably prepared before its events can
   reach the file, and publishes the group. Groups flush their caches and
   the engine log in parallel.
4. **ring buffer, flush thread, sync thread** (right column). The flush
   thread takes the longest contiguous run of published groups and writes it
   to the file. The sync thread `fsync`s the flushed data. The new end
   position is published to dump threads by the flush thread when
   `sync_binlog != 1`, and by the sync thread after the `fsync` when
   `sync_binlog = 1`, so replicas never receive events that are not yet
   durable on the source, as today.
5. **wait flushed / synced.** The leader waits until the flush thread has
   written its group or, if the group waits for the sync, until the sync
   thread has synced it.
6. **commit group.** The leader commits the whole group in the engine,
   updates GTID state, runs `after_commit`, and releases its followers,
   which finish their own commits. Each group commits as soon as its wait
   ends, so groups may commit out of binlog order. When the commit order
   is required (`binlog_order_commits=ON`, the default, or a clone needs
   it), groups instead enter today's commit stage in binlog order, and its
   leader commits them in that order.

Every commit takes this path: today's flush and sync stages are removed.
A group that does not fit the buffer is not copied into it: once every
earlier group is written, its leader writes the group straight into the
file under `LOCK_log`, and the group then waits and commits like the
others.

Guarantees that follow:

- Binlog order, GTID order, sequence numbers and `last_committed` are
  fixed in the flush prepare step under `LOCK_log`, so they are
  identical to today (FR-2).
- The file is always a prefix: the flush thread writes groups in
  allocation order and only when contiguous, and a leader writes its
  group straight into the file only after every earlier group (FR-2).
- A transaction is visible in the file only after it is durably prepared
  in the engine, and committed in the engine only after its group is
  flushed or synced, so recovery is unchanged (FR-2).

#### Alternatives Considered

- Keeping the existing three-stage protocol: split its flush stage into
  two (flush prepare and a parallel flush) and let the leaders write and
  sync the file themselves. Coordinating many leaders that write and sync
  one file in order is complex, and Deferred Binlog Sync with Redo
  Protection (mysql/mysql-server#833) needs flush and sync to run
  separately from the leaders. Separate background flush and sync threads
  serve both.
- Committing each transaction on its own, without groups: every
  transaction does flush prepare, the flush and the wait by itself. At
  high concurrency this means many more threads competing for CPU, much
  more contention on `LOCK_log` and the other shared mutexes, and many
  small writes instead of a few large ones. Tests showed it performs worse
  than today's binary log group commit.


### 3.2 User Interface

#### Controls

New system variables, one subsection each.

##### sync_binlog_interval

- NAME: `sync_binlog_interval`
- VALUES: 1 to 100000000 (microseconds)
- DEFAULT: 1000000
- SCOPE: GLOBAL
- REPLICATED: No
- DYNAMIC: Yes
- PERSIST: PERSIST
- PRIVILEGES: read: none; update: `SYSTEM_VARIABLES_ADMIN`
- COMMAND LINE: Yes, `--sync-binlog-interval=#`
- DESCRIPTION: When `sync_binlog` is not 1, the longest time the server
  waits after a binary log sync completes before it starts the next one.
  With `sync_binlog=N`, a sync also starts after `N` groups. The time
  between two syncs can exceed this interval by the duration of a sync
  and by scheduling delays.
- OBSERVABILITY: `SHOW GLOBAL VARIABLES` and
  `performance_schema.global_variables` show the runtime value, which is
  always the configured value; `performance_schema.persisted_variables`
  shows a persisted value.

##### binlog_buffer_size

- NAME: `binlog_buffer_size`
- VALUES: 1048576 to 4294967295 (bytes)
- DEFAULT: 1048576 (1 MiB)
- SCOPE: GLOBAL
- REPLICATED: No
- DYNAMIC: Yes
- PERSIST: PERSIST
- PRIVILEGES: read: none; update: `SYSTEM_VARIABLES_ADMIN`
- COMMAND LINE: Yes, `--binlog-buffer-size=#`
- DESCRIPTION: Size of the memory buffer that holds binary log data before
  it is written to the binary log file.
- OBSERVABILITY: `SHOW GLOBAL VARIABLES` and
  `performance_schema.global_variables` show the runtime value, which is
  always the configured value; `performance_schema.persisted_variables`
  shows a persisted value. The allocated buffer is shown by the
  Performance Schema memory instrument
  `memory/sql/Binlog_parallel_flush::Async_writer::binlog_buffer`.

##### sync_binlog (changed)

- `1`: unchanged. A commit returns after its group is synced to disk.
- `0`: today the server never syncs the binary log and leaves it to the
  operating system. Now the server also syncs it at most
  `sync_binlog_interval` microseconds after the previous sync completes.
- `N` greater than 1: today the commit that completes every `N` groups
  waits for the sync. Now no commit waits for a sync: a commit returns
  once its group is written to the binary log file, and the server syncs
  the file after every `N` groups or `sync_binlog_interval` microseconds
  after the previous sync completes, whichever comes first. Commits
  continue while a sync is slow. As today, the number of transactions an
  operating system crash may lose is not fixed, because groups have no
  fixed size.

##### Sync delay variables (deprecated)

`binlog_group_commit_sync_delay` and
`binlog_group_commit_sync_no_delay_count` are deprecated and have no
effect, but setting them is still accepted, with a deprecation warning,
as is reading them. No commit waits before the sync: the server syncs
every group written since the previous sync with one `fsync`, so groups
are batched without a delay.

#### Observability

- Threads `binlog_flush_thread` and `binlog_sync_thread` in
  `performance_schema.threads`. Both are created at start-up when the
  binary log is enabled and run until shutdown.

- New session stages, shown as the `State` column of `SHOW PROCESSLIST`
  and in `performance_schema.threads.PROCESSLIST_STATE` and
  `events_stages_*`.
  - `Waiting for binlog write buffer available`: the leader waits for
    the flush thread to free the buffer space of its group. Seen when the
    buffer is too small for the write rate.
  - `Waiting for binlog write queue node available`: the leader waits for
    a free queue node. Seen only under extreme backlog.
  - `Waiting for binlog events to be written into binlog file`: the
    leader waits for the flush thread to write its group
    (`sync_binlog != 1`).
  - `Waiting for binlog events to be synced`: the leader waits for the
    sync thread to sync its group (`sync_binlog = 1`).
  - `Waiting for earlier binlog groups to commit`: with the commit order
    required, the leader waits until the previous group has entered the
    commit stage.
  - `Waiting for binlog write queue empty`: a session waits until every
    buffered group is written and synced, before a rotation, a reset or
    shutdown; or only until every buffered group is written, before a
    leader writes a group that does not fit the buffer, before a buffer
    resize and before reading the current binary log position
    (`SHOW BINARY LOG STATUS`, `SHOW BINARY LOGS`,
    `performance_schema.log_status`, clone).
  - Followers keep the existing `waiting for handler commit` state.
- Status variables: none added or removed.
- Performance Schema tables: none added, removed or modified.
- New instrumentation keys, all in the `sql` category:
  - threads: `binlog_flush_thread`, `binlog_sync_thread`;
  - mutexes: `Binlog_parallel_flush::Async_writer::m_flush_mutex`,
    `Binlog_parallel_flush::Async_writer::m_sync_mutex`,
    `Binlog_parallel_flush::Async_writer::m_commit_mutex`,
    `Binlog_parallel_flush::Async_writer::Write_state::mutex` (the 64
    leader wait shards);
  - conditions: `Binlog_parallel_flush::Async_writer::m_flush_cond`,
    `m_flushed_cond`, `m_sync_cond`, `m_synced_cond` and
    `Write_state::cond`, under the same prefix;
  - memory: `Binlog_parallel_flush::Async_writer::binlog_buffer`.
  The 256 follower shards of the flush stage queue reuse the existing
  `MYSQL_BIN_LOG::LOCK_done` and `MYSQL_BIN_LOG::COND_done` keys. The
  mutex `MYSQL_BIN_LOG::LOCK_sync_queue` is removed with the sync stage.
- Error log messages, server component:
  - A failed background write or sync logs the reason "Could not write
    the binary log file." or "Could not sync the binary log file." in
    the existing messages of `binlog_error_action`:
    `ER_BINLOG_LOGGING_NOT_POSSIBLE` with `ABORT_SERVER`,
    `ER_TURNING_LOGGING_OFF_FOR_THE_DURATION` with `IGNORE_ERROR`.
  - New `ER_BINLOG_BUFFER_ALLOCATION_FAILED`, error: "Could not allocate
    %u bytes for binlog_buffer_size. Lower binlog_buffer_size or make
    more memory available." The server does not start.
  - New `ER_BINLOG_WRITER_THREAD_CREATE_FAILED`, error: "Could not create
    the binary log %s thread. Check the operating system limits on
    threads." The server does not start.
  - A failed `SET GLOBAL binlog_buffer_size` returns the existing client
    error `ER_OUTOFMEMORY`.

#### Usage

```sql
-- full durability, highest concurrency: no commit order
SET PERSIST binlog_order_commits = OFF;
SET PERSIST sync_binlog = 1;

-- smaller loss window: sync again at most 10 ms after the last sync
SET PERSIST sync_binlog = 0;
SET PERSIST sync_binlog_interval = 10000;
```

### 3.3 Security

Not applicable.

### 3.4 System Resources

- Memory: one buffer of `binlog_buffer_size` bytes (1 MiB by default),
  allocated at start-up whenever the binary log is enabled, and replaced
  only when the variable is set. The
  rest is fixed in size: 1024 queue nodes and 64-entry shard arrays.
- Threads: two background threads, `binlog_flush_thread` and
  `binlog_sync_thread`, for the life of the server when the binary log is
  enabled.
- CPU: the flush thread runs only when a group is published. The sync
  thread waits at most `sync_binlog_interval` (1 s by default) after a
  sync and does nothing if no data waits for a sync, so an idle server
  pays one wake-up per interval.
- Disk: the same bytes as today, in fewer and larger writes; the number
  of syncs follows `sync_binlog`, plus at most one per
  `sync_binlog_interval`.

### 3.5 Deployment and Installation

Not applicable.

### 3.6 Protocols

Not applicable.

### 3.7 Failure Model

Abnormal restart, write and sync errors, disk full and network: same
as the original.

New in this feature:

- Normal restart: shutdown writes and syncs every queued group before
  the flush and sync threads stop.
- Memory: a `binlog_buffer_size` buffer that cannot be allocated at
  startup stops the server from starting.

### 3.8 Upgrade/Downgrade

Not applicable.

### 3.9 Cross-version Replication

Not applicable.

---

## 4. Low Level Design (LLD)

### 4.1 Block Diagram

```
 sql/rpl_commit_stage_manager               sql/binlog.cc
 ┌─────────────────────────────────┐        ┌─────────────────────────────────┐
 │ flush stage queue forms groups; │        │ ordered_commit(): the leader    │
 │ follower done shards; commit    │ ─────► │ holding LOCK_log hands the      │
 │ stages for ordered groups       │ leader │ group to parallel flush;        │
 │                                 │        │ rotation, reset drain           │
 └─────────────────────────────────┘        └────────────────┬────────────────┘
                                                             │ commit()
                                                             ▼
 sql/binlog_event_writer.h (new)            sql/binlog_parallel_flush (new)
 ┌─────────────────────────────────┐        ┌─────────────────────────────────┐
 │ writes events to any stream     │        │ flush prepare, parallel flush,  │
 │ at a given start position       │ ◄───── │ serial write, ring buffer,      │
 └─────────────────────────────────┘  write │ write queue, flush and sync     │
                                            │ threads, commit of the group    │
 sql/rpl_gtid                               └────────────────┬────────────────┘
 ┌─────────────────────────────────┐                         │
 │ GTID commit without LOCK_commit │                         │
 │ shared by concurrent groups     │ ◄───────────────────────┘
 └─────────────────────────────────┘  concurrent groups
```

- `sql/rpl_commit_stage_manager`: the flush stage queue of today's binary
  log group commit (BGC) still forms the groups; followers wait on 256
  shards. The commit and after-commit stages remain for groups that
  commit in binlog order; the sync stage is removed. The commit order
  flush stage is split out of the binary log commit.
- `sql/binlog.cc`: `ordered_commit()` hands every group to
  `Binlog_parallel_flush::commit()`; the code of today's flush, sync and
  commit stages is removed from it. Rotation and reset drain first.
- `sql/binlog_parallel_flush` (new): flush prepare under `LOCK_log`,
  parallel flush into the ring buffer, serial writes of groups that do
  not fit it, the flush and sync threads, and the commit of the group.
- `sql/binlog_event_writer.h` (new): the event writer, moved out of
  `binlog.cc`, writes into the buffer at the group's position.
- `sql/rpl_gtid`: GTID state accepts concurrent groups.

#### 4.1.1 Grouping with the Flush Stage Queue

Groups are formed by the flush stage queue (`BINLOG_FLUSH_STAGE`) of
today's binary log group commit. `ordered_commit()` is unchanged up to
the flush stage: a replica applier preserving the commit order waits for
its turn (Stage #0), and every session enters the flush stage queue. The
first session in the queue is the leader and takes `LOCK_log`; the others
are followers and wait for it, as today. The leader then calls
`Binlog_parallel_flush::commit()`. Because the queue is the same, Group
Replication tickets and the existing sync points keep working.

#### 4.1.2 Flush Prepare

Once it holds `LOCK_log` and has taken the queue, the leader prepares the
group:

1. Check that the group fits the buffer, with an upper bound of its size.
2. Assign automatic GTIDs to the members.
3. Assign sequence numbers to the members and compute their
   `last_committed` from the dependency trackers, as today's flush stage
   did. The trackers need binlog order: the writeset history is reset
   on rotation or when it is full, and a reset must never move back.
   `write_transaction()` later writes the stored values.
4. Compute the exact size of the group's events.
5. Count one prepared transaction ID (XID) for the group, so that
   rotation waits until the group has committed.
6. Allocate the group's binlog position and queue index.

Then it releases `LOCK_log`.

#### 4.1.3 Group Size and Tagged GTIDs

A group must write exactly the `length` bytes at `start` allocated to it; a
gap or an overlap with the next group makes the binary log unreadable.
The GTID event of a tagged GTID (`GTID_TAGGED_LOG_EVENT`) encodes the
GNO, `last_committed`, `sequence_number` and the commit timestamps as
variable-length integers, so its length is known only after these
values are assigned, and it changes when a value crosses a width
boundary (for example 63 to 64, or 8191 to 8192). Flush prepare
therefore sizes the group twice:

- Before anything is assigned, the fit check uses an upper bound: each
  member's cache bytes and the largest possible GTID event
  (`Gtid_log_event::get_max_event_length()`), plus one checksum per event
  when `binlog_checksum` is on. It must come first, because GTIDs and
  sequence numbers cannot be given back when the group is written
  serially instead.
- After the GTIDs and logical timestamps are assigned, the exact size
  is computed from the GTID event that `write_transaction()` will write,
  built from the same values, so they cannot disagree. A member whose
  GTID could not be assigned writes nothing and is not counted.

#### 4.1.4 Binlog Buffer

The binlog buffer is a circular buffer of `binlog_buffer_size` bytes.

##### Mapping between the buffer and the binlog file

Two positions describe the state; both are binlog file positions:

- `m_flushed_pos`: everything before it has been written to the binlog
  file. It equals the file's end position.
- `m_allocated_pos`: everything before it has been allocated to a group.
  Allocation runs ahead of writing, and may run ahead by more than the
  buffer size.

The file is logically cut into blocks of `binlog_buffer_size` bytes, and
every block maps onto the whole buffer at a fixed offset:

```
buffer_offset = binlog_pos mod binlog_buffer_size
```

So the buffer divides the allocated part of the file into three regions:

```
                               ┌───────────────────┐
                               ▼                   │
                              ┌─────────────┐      │
             Circular Buffer  │ 2nd  ┆ 1st  │      │
                              │ part ┆ part │      │
                              └─────────────┘      │
Current binlog  ┆             ┆      ┆      ┆      │      ┆             ┆
  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓┆▓▓▓▓▓▓▓▓▓▓▓▓▓┆▓▓▓▓▓▓░░░░░░░┆░░░░░░░░░░░░░┆░░░░░░░░░░ ──►
  ▲                                  ▲             ▲                ▲
  │                                  │             │                │
  │                                  │             │                │
  0                            m_flushed_pos m_flushed_pos   m_allocated_pos
                                             + buffer size

          └──── data written into file ─────┘└─ available ─┘
                                             └────── allocated buffer ──────┘
```

- Written to the file: positions below `m_flushed_pos`. Their buffer
  region is free and will be reused by later positions.
- Available buffer: positions in `[m_flushed_pos, m_flushed_pos +
  binlog_buffer_size)`. A group may write them into the buffer now.
- Allocated but not writable: positions at or beyond
  `m_flushed_pos + binlog_buffer_size`. Their buffer region still holds
  data the flush thread has not written; the group must wait.

##### Allocating space to a group (`allocate_buffer`)

Under `LOCK_log` the leader takes a contiguous range
`[m_allocated_pos, m_allocated_pos + length)` for its group and advances
`m_allocated_pos`; the group's first byte lands at the old
`m_allocated_pos`, which is also the position used for `end_log_pos` in
the event headers. One queue slot is taken at the same time (slot index =
allocation order). Allocation is separate from writing, so it is not
limited by the buffer size; the check happens before the write.

##### Checking that the buffer can be written (`wait_for_buffer_available`)

Before writing, the leader waits until

```
start + length <= m_flushed_pos + binlog_buffer_size
```

i.e. until its whole range lies inside the available buffer. The flush
thread wakes it each time it advances `m_flushed_pos`. Because allocation is in
`LOCK_log` order and the flush thread writes positions in that same order, the
condition always becomes true once the older groups are written; a group never
exceeds the buffer size, so it can never wait forever.

#### 4.1.5 Parallel Flush

After flush prepare, without holding `LOCK_log`, the leader:

1. waits until the buffer region of its group is free;
2. writes the events of every member into the buffer, and records the
   member's end in the group's range as its binary log coordinates,
   which the semisynchronous hooks and the NDB injector read; the file
   position belongs to earlier groups and is moved by the flush thread;
3. waits until its queue node is free, so that it cannot overwrite the
   `end_pos` of a group that is still unwritten;
4. flushes the engine log, so that the group's transactions are durably
   prepared before its events can reach the file;
5. publishes the group by storing its end position in the node's
   `end_pos`, and wakes the flush thread.

Groups do all of this at the same time.

##### Writing into the buffer (`write_to_buffer`)

The group's events are copied to `buffer_offset = start mod
binlog_buffer_size`. Several groups write their
own ranges at the same time without any lock, since the ranges are
disjoint by construction.

#### 4.1.6 Flush Thread

Leaders finish writing their parts of the buffer in any order, but the
file must be written in binlog order. The write state queue tells the
flush thread which groups are complete and in which order.

##### Write state queue

- `m_write_queue_nodes`: `QUEUE_NODE_COUNT` nodes, each holding one
  atomic `end_pos`, the binlog end position of its group.
- `m_allocated_index`, `m_flushed_index`, `m_synced_index`: the queue
  index of the last allocated, written and synced group.
- Every group gets a queue index when its binlog position is allocated
  (allocation order, increasing forever) and the node
  `node[index mod QUEUE_NODE_COUNT]` (`node` is short for
  `m_write_queue_nodes`). In the timelines below, `node[a]` is short for
  `node[a mod QUEUE_NODE_COUNT]`.
- `end_pos` is atomic, so publishing a group needs no mutex.
- A node with `end_pos` beyond `m_flushed_pos` is "published"; the flush
  thread writes published nodes in index order only.
- `QUEUE_NODE_COUNT` is 1024: each node holds a whole group, and far
  fewer groups are in flight even with thousands of sessions. A node is
  reused as soon as the flush thread has written it; the sync thread
  does not read nodes.

The queue maps onto the index space exactly like the buffer maps onto
the file:

```
                                ┌───────────────────┐
                            ┌───┼───────────────┐   │
                        ┌───┼───┼───────────┐   │   │
                        ▼   ▼   ▼           │   │   │
                      ┌───────────────────┐ │   │   │
      Circular Queue  │   │   │   ┆   │   │ │   │   │
                      └───────────────────┘ │   │   │
  ┆                   ┆           ┆ ▲     ┆ │   │   │         ┆
  │▓▓▓│▓▓▓│▓▓▓│▓▓▓│▒▒▒┆▒▒▒│▒▒▒│▒▒▒┆   │   ┆   │   │   │   │   ┆   │   │   │ ──►
  ▲               ▲               ▲                   ▲                   ▲
  │               │               │                   │                   │
  │               │               │                   │                   │
  0        m_synced_index  m_flushed_index     m_flushed_index      m_allocated_index
                                               + QUEUE_NODE_COUNT

  └─── synced ───┘└── flushed ───┘└─ available nodes ─┘
                                  └─────────── allocated nodes ───────────┘
```

- Nodes before `m_synced_index` are synced; nodes before
  `m_flushed_index` are written to the file; `m_synced_index <=
  m_flushed_index <= m_allocated_index`.
- `m_flushed_index == m_allocated_index`: the write queue is empty, every
  allocated group is in the file. `m_synced_index == m_allocated_index`:
  the sync queue is empty as well.
- A node is free when `index - m_flushed_index < QUEUE_NODE_COUNT`,
  i.e. the group that used it 1024 indexes earlier has been written.

##### Waking the flush thread

The flush thread sleeps on a condition variable, `m_flush_cond`, guarded
by `m_flush_mutex`. A leader that has published its group takes
`m_flush_mutex`, sets `m_flush_pending`, signals `m_flush_cond`, and
releases the mutex. The flush thread, after each flush, takes the mutex
and waits on the condition only if `m_flush_pending` is not set, then
clears it. The flag keeps a wake-up that arrives while the flush thread
is writing, so it is never lost.

##### One flush (`flush`)

Starting at `m_flushed_index + 1`, take the longest run of consecutive
published nodes. The run stops at the first node that is not published.
Write the buffer region `[m_flushed_pos, end of the run)` to the file,
then flush the file. Then, in this order:

1. publish the end position to dump threads if `sync_binlog` is not 1;
2. advance `m_flushed_pos`, then `m_flushed_index` to the last node of
   the run (this frees buffer space and queue nodes);
3. wake the leaders waiting for the flush of the run: broadcast the
   shards of indexes `head` to `min(last, m_flush_wait_max)`;
4. wake the sync thread every `sync_binlog` flushed groups.

A write or flush error stops the thread; the indexes are not advanced.

##### Waiting for the flush (`wait_for_binlog_flushed`)

The leader returns at once if `m_flushed_index` has reached its queue
index. Otherwise it waits on condition shard `queue_index mod 64` until
it has.

##### Synchronisation between leaders and the flush thread

Three things connect a leader and the flush thread: the node's `end_pos`
(leader to flush thread, "my group is ready"), `m_flush_pending` and
`m_flush_cond` (leader to flush thread, "wake up"), and
`m_flushed_index` plus one of the 64 condition shards (flush thread to
leader, "your group is written"). The timeline below shows two leaders,
A then B, with the flush thread asleep when A publishes:

```
 time Leader A                 Leader B                    Flush thread
  │  ────────                  ────────                    ────────────
  │
  │  events written to buffer                              asleep (m_flush_cond)
  │  node[a].end_pos = end_a
  │
  │  lock m_flush_mutex
  │  m_flush_pending = true
  │  signal m_flush_cond ────────────────────────────────► wakes
  │  unlock m_flush_mutex
  │
  │                            events written to buffer    flush():
  │                            node[b].end_pos = end_b       run = a, b
  │                            lock, set pending,            write up to end_b
  │                            signal, unlock                flush file
  │
  │  wait_for_binlog_flushed(a)                            m_flushed_pos = end_b
  │    a <= m_flushed_index? no                            m_flushed_index = b
  │
  │   ┌ WAIT cond[a mod 64] ┐
  │   │                     │  ◄─ broadcast ────────────── wake shards of a .. b
  │   └─────────────────────┘                              (to m_flush_wait_max)
  │
  │  after_sync, commit ...    wait_for_binlog_flushed(b): lock m_flush_mutex
  │                              b <= m_flushed_index? yes pending? yes: flush()
  │                              no wait                   nothing new
  │                                                        pending? no
  │                                                        wait on m_flush_cond
  ▼
```

#### 4.1.7 Sync Thread

The sync thread is the only writer of `m_synced_index`, the index of the
last node whose data is known to be on storage. It decides when to call
`fsync` from two inputs: the flush thread's progress and the
`sync_binlog_interval` timer.

##### State

- `m_synced_index`: nodes up to it are on storage.
- `m_sync_mutex` and `m_sync_cond`: wake the sync thread;
  `m_sync_pending`, under `m_sync_mutex`, records a wake-up that arrived
  while the thread was busy.
- `m_synced_cond`: wakes a session draining the queue.

Leaders waiting for their group to be synced do not use `m_sync_mutex`:
they sleep on the same 64 condition shards as leaders waiting for the
flush, so that an `fsync` wakes only the leaders whose groups it
covered.

##### One sync (`sync`)

1. Snapshot `m_flushed_index` and then `m_flushed_pos`. If the index is
   not beyond `m_synced_index` there is nothing to do.
2. `fsync` the binlog file. Everything the flush thread had written up to
   the snapshot was in the file before the `fsync` started, so it is
   covered.
3. Publish the snapshot position to dump threads if it is beyond the
   position already published.
4. Set `m_synced_index` to the snapshot index.
5. Wake the leaders waiting for the sync: broadcast the shards of
   indexes `old m_synced_index + 1` to `min(snapshot, m_sync_wait_max)`.

##### Main loop (`run_sync`)

1. Call `sync()`, and wake a session waiting for the drain, if any.
2. Go to 1 at once if `m_sync_pending` is set: a wake-up arrived while
   the `fsync` was running, and waiting for another one would add a whole
   `fsync` of latency to groups that are already written.
3. Otherwise wait on `m_sync_cond` for at most `sync_binlog_interval` µs,
   then go to 1. The timeout bounds the wait between the end of one
   `fsync` and the start of the next. It serves `sync_binlog = 0`, where
   nothing else wakes the thread, and `sync_binlog = N` when fewer than
   N groups arrive.

##### Who wakes the sync thread

All wakers set `m_sync_pending` and signal `m_sync_cond` under
`m_sync_mutex`.

- The flush thread, every `sync_binlog` flushed groups.
- `SET GLOBAL sync_binlog` and `SET GLOBAL sync_binlog_interval`, so a
  new setting takes effect without waiting out the old timeout.
- A session draining the queue, which then waits on `m_synced_cond`
  until `m_synced_index == m_allocated_index`.

##### Waiting for the sync (`wait_for_binlog_synced`)

Like the flush wait, with `m_synced_index` in place of
`m_flushed_index` and on the same shard.

```
 time Leader 1          Leader 2           Flush thread       Sync thread
  │  ────────           ────────           ────────────       ───────────
  │
  │  publish node 1                        write node 1       asleep
  │                                        flushed_index = 1
  │                                        sync_binlog reached
  │                                        signal ──────────► wakes
  │
  │  wait synced(1)                                           sync():
  │    1 <= synced? no                                          flushed = 1
  │                                                             fsync ...
  │   ┌ WAIT cond[1] ┐
  │   │              │  publish node 2
  │   │              │                     write node 2
  │   │              │                     flushed_index = 2
  │   │              │                     signal ──────────► (in fsync: sets
  │   │              │                                        m_sync_pending)
  │   │              │                                        fsync done
  │   │              │                                        synced_index = 1
  │   │              │ ◄─ broadcast ───────────────────────── wake shards 1..1
  │   └──────────────┘
  │  after_sync,        wait synced(2)                        pending: sync()
  │  commit ...           2 <= synced? no                     fsync,
  │                                                           synced_index = 2
  │                      ┌ WAIT cond[2] ┐
  │                      │              │ ◄─ broadcast ────── wake shards 2..2
  │                      └──────────────┘
  │                     after_sync,                           timed wait on
  │                     commit ...                            m_sync_cond
  ▼
```

#### 4.1.8 Committing After Flush or Sync and Changing `sync_binlog`

The leaders, the flush thread and the sync thread run at the same time
and may each read a different value of `sync_binlog`, so if each simply
used the value it read, a change could:

- leave a group that waits for the sync without a sync, because the sync
  thread now runs only every `sync_binlog_interval`;
- let a leader re-read the value and wait for something other than what
  its group was prepared for;
- leave data flushed under `sync_binlog = 1`, which the flush thread did
  not publish, unseen by dump threads until a later write.

The design below avoids these without pausing commits.

##### Which wait a group uses

A group is committed in the engine only after it is written to the file
(flush) or written and `fsync`ed (sync). The choice is made once per
group, in flush prepare under `LOCK_log`: the group waits for the sync if
`sync_binlog` is 1, otherwise for the flush, and keeps that choice to the
end even if `sync_binlog` changes meanwhile. Since flush prepare runs in
binlog order, after a change every group before some position has the
old choice and every group after it the new one.

The leader records the choice in one of two watermarks, which only the
`LOCK_log` holder writes:

- `m_flush_wait_max`: the highest queue index whose group waits for the
  flush;
- `m_sync_wait_max`: the highest queue index whose group waits for the
  sync.

The store is ordered before the release store of the node's `end_pos`,
so the flush thread sees the new watermark when it takes the node, and
the sync thread when it sees the node flushed.

##### Why the wait guarantees it

- A group waiting for the flush returns only when `m_flushed_index` has
  reached its queue index. The flush thread advances `m_flushed_index`
  only after the write of the run has returned, so the group's bytes are
  in the file.
- A group waiting for the sync returns only when `m_synced_index` has
  reached its queue index. The sync thread sets `m_synced_index` to a
  snapshot of `m_flushed_index` taken before the `fsync`, so the group's
  bytes were in the file before the `fsync` started.

The leader runs the `after_sync` hook and the engine commit only after
the wait returns. Both indexes only grow, so a wake-up can be early or
spurious but can never let a leader through before its group is done.

##### Who is woken

The flush thread broadcasts the shards of the flushed indexes up to
`m_flush_wait_max`, and the sync thread those of the synced indexes up
to `m_sync_wait_max`. With `sync_binlog = 1` no group waits for the
flush, so the flush thread wakes nobody; with any other value no group
waits for the sync, so the sync thread wakes nobody.

##### Driving the sync thread

The sync thread is driven by the groups waiting for the sync, not only by
the current value of `sync_binlog`. While
`m_sync_wait_max > m_synced_index`, the flush thread wakes it after each
flush, besides every `sync_binlog` groups, and the sync thread goes round
its loop again without sleeping while flushed groups are unsynced and one
of them waits for the sync. A group that waits for the sync is therefore
never left to `sync_binlog_interval`, even after `sync_binlog` has
changed from 1.

##### Publishing the end position

With `sync_binlog = 1` the sync thread publishes the end of the data
covered by each `fsync`, as today. With any other value the flush thread
publishes the end of each run it writes, as today.

The flush thread reads `sync_binlog` once per flush to decide whether to
publish, and the sync thread publishes the synced position whenever it is
beyond the published one, whatever the current value. Data flushed while
the value was 1, which the flush thread did not publish, is therefore
published after the next `fsync`.

##### `sync_binlog` from 0 to 1

The same applies to any value other than 1 changed to 1.

- Groups prepared before the change wait for the flush. They commit after
  the flush and, if the flush thread still read the old value, are
  published by it, as before the change.
- Groups prepared after the change wait for the sync and raise
  `m_sync_wait_max`. After each flush the flush thread sees a leader
  waiting for an unfinished sync and wakes the sync thread, which keeps
  syncing without waiting for `sync_binlog_interval`. The flush thread
  reads `sync_binlog` after it sees such a group published, so it reads
  1 too and no longer publishes; the sync thread publishes after each
  `fsync`.

Group 5 is prepared before the change and group 6 after it:

```
 time Leader A          Leader B           Flush thread       Sync thread
  │  ────────           ────────           ────────────       ───────────
  │
  │  prepare group 5:
  │    sync_binlog = 0
  │    flush_wait_max=5
  │
  │  publish node 5
  │
  │  WAIT flushed_index                    write node 5
  │    >= 5
  │                                        flushed_index = 5
  │                                        sync_binlog = 0:
  │                                          publish end of 5
  │
  │  ◄── wake shards 5..5 ───────────────
  │
  │  commit
  │
  │                     SET sync_binlog=1
  │
  │                     prepare group 6:
  │                       sync_binlog = 1
  │                       sync_wait_max=6
  │
  │                     publish node 6
  │
  │                     WAIT synced_index  write node 6
  │                       >= 6
  │                                        flushed_index = 6
  │                                        sync_binlog = 1:
  │                                          no publish
  │                                        wakes no leader
  │
  │                                        sync_wait_max 6
  │                                          > synced:
  │                                          signal ────────► sync():
  │                                                             flushed = 6
  │                                                             fsync
  │                                                             synced_index = 6
  │                                                             publish end of 6
  │
  │                     ◄── wake shards 6..6 ───────────────
  │
  │                     commit
  ▼
```

##### `sync_binlog` from 1 to 0

The same applies to 1 changed to any other value.

- Groups prepared before the change still wait for the sync. While
  `m_sync_wait_max > m_synced_index` the flush thread wakes the sync
  thread after each flush and the sync thread does not sleep, so their
  leaders do not wait for `sync_binlog_interval` although the new value
  asks for no syncs.
- Data flushed while the value was 1 was not published by the flush
  thread. The sync thread publishes it after the next `fsync`, since its
  position is beyond the published one.
- Groups prepared after the change commit after the flush, and the flush
  thread publishes them after each write. Their position can include
  bytes of older groups still waiting for the sync, which is what today's
  group commit does too when `sync_binlog` changes from 1 to another
  value.

Group 5 is prepared before the change and group 6 after it; the sync
thread runs for group 5 although `sync_binlog` is already 0:

```
 time Leader A          Leader B           Flush thread       Sync thread
  │  ────────           ────────           ────────────       ───────────
  │
  │  prepare group 5:
  │    sync_binlog = 1
  │    sync_wait_max=5
  │
  │  publish node 5
  │
  │  WAIT synced_index
  │    >= 5
  │
  │                     SET sync_binlog=0
  │
  │                                        write node 5
  │                                        flushed_index = 5
  │                                        sync_binlog = 0:
  │                                          publish end of 5
  │                                        wakes no leader
  │
  │                                        sync_wait_max 5
  │                                          > synced:
  │                                          signal ────────► sync():
  │                     prepare group 6:                        flushed = 5
  │                       sync_binlog = 0                       fsync ...
  │                       flush_wait_max=6
  │
  │                     publish node 6
  │
  │                     WAIT flushed_index write node 6
  │                       >= 6
  │                                        flushed_index = 6
  │                                        sync_binlog = 0:
  │                                          publish end of 6
  │
  │                     ◄── wake 6..6 ───
  │
  │                     commit
  │
  │                                                           ... fsync done
  │                                                           synced_index = 5
  │                                                           published end of
  │                                                           6 is beyond:
  │                                                           no publish
  │
  │  ◄── wake shards 5..5 ──────────────────────────────────
  │
  │  commit                                                   no waiter left:
  │                                                           timed wait
  ▼
```

#### 4.1.9 Switching Between Sync and Async Mode

The flush thread writes the file in the background, at positions handed
out from `m_allocated_pos`, while rotation and reset write and close the
file through the file object at its current position. The two must never
run at the same time. The writer therefore has two modes, recorded in
`m_async_mode`, and the flag is changed only under `LOCK_log`:

- **sync mode**: the file object's position is the truth; nothing is
  buffered; `m_allocated_pos` and `m_flushed_pos` are stale.
- **async mode**: `m_allocated_pos` is the truth; the file object's
  position equals `m_flushed_pos` and lags behind by whatever is still
  in the buffer.

```
                 first parallel allocation (allocate_buffer, under LOCK_log):
                 m_allocated_pos = m_flushed_pos = file position; no wait
   ┌───────────┐ ─────────────────────────────────────────────► ┌────────────┐
   │ sync mode │                                                │ async mode │
   │ file pos  │ ◄───────────────────────────────────────────── │ positions  │
   │ is truth  │   switch_to_sync_mode (under LOCK_log):        │ handed out │
   └───────────┘   wait write queue empty, then sync queue empty│ from       │
                                                                │ m_alloc... │
                                                                └────────────┘
```

##### Entering async mode

The first group that allocates a position while the writer is in sync
mode does the switch, also for a serial write: under `LOCK_sync` (so
that no sync or close of the file object is in progress) it reads the
file object's position into `m_allocated_pos` and `m_flushed_pos`, then
sets `m_async_mode`.

##### Leaving async mode (`switch_to_sync_mode`)

The caller holds `LOCK_log`, so no new position can be allocated while
it runs. If the writer is in async mode it drains the queue
(`wait_for_queue_empty`):

1. Wait until the write queue is empty, `m_flushed_index ==
   m_allocated_index`. Every allocated byte is now in the file and the
   file object's position equals `m_allocated_pos`.
2. Wake the sync thread and wait until the sync queue is empty,
   `m_synced_index == m_allocated_index`. Every allocated byte is now on
   storage.

Then it clears `m_async_mode`. From here a leader can write at the file
object's position, and the next parallel group will re-enter
async mode by re-reading that position.

##### Who switches to sync mode

- Binary log rotation (`new_file_impl`), after `wait_for_prep_xids()`,
  so that the old file is complete and synced before it is closed.
- `RESET BINARY LOGS AND GTIDS` (`reset_logs`), before the files are
  deleted.
- Shutdown, before the two threads are stopped.

##### Rotation and the queue

After a rotation the writer is in sync mode and the rotation has
written the new file's header. `open_binlog_file()` then clears
`end_pos` in every queue node, so that a position left over from the old
file can never be taken for a published node in the new one. The three
indexes are not reset; they keep increasing across files, which is why
`m_synced_index <= m_flushed_index <= m_allocated_index` stays true. The
first parallel group in the new file re-enters async mode and reads the
position after the header.

##### Reading the current position (`raw_get_current_log`)

`SHOW BINARY LOG STATUS`, `SHOW BINARY LOGS`,
`performance_schema.log_status` and clone read the current position
under `LOCK_log`, and a backup or clone records it as the point a
replica continues from, so it must end a complete group. In async mode
the file object's position is moved by the flush thread without
`LOCK_log` and can stop inside a group.

In async mode `raw_get_current_log()` therefore first waits until the
write queue is empty (`m_flushed_index == m_allocated_index`), then
reads the file position as today. `LOCK_log` stops new allocations
meanwhile, so the position ends the last group, as after the removed
flush stage. Writing needs no lock the reader holds, so the wait cannot
deadlock.

#### 4.1.10 Start-up and Shutdown

The buffer and the flush and sync threads are set up with the binary
log and stay until shutdown:

- At start-up, `init()` allocates `binlog_buffer_size` bytes and creates
  the two threads, before replication starts, so every commit finds
  them. The server does not start if either fails.
- At shutdown, `cleanup()` drains the queue under `LOCK_log`, then stops
  the threads.

##### Changing `binlog_buffer_size`

The `ON_UPDATE` function `update_binlog_buffer_size()` replaces the
buffer under the global variable locks, so concurrent `SET` statements
are serialized and the value shown is the size of the buffer in use:

1. allocate the new buffer without `LOCK_log`; on failure the old value
   is restored and `SET` returns `ER_OUTOFMEMORY`;
2. under `LOCK_log`, which stops new allocations, wait until every
   allocated group is written, then install the new buffer;
3. free the old buffer after releasing `LOCK_log`.

Taking `LOCK_log` under the global variable locks follows the existing
order of `max_binlog_size`.

#### 4.1.11 Condition and Mutex Shards

Waiting sessions are split into mutex/condition shards, and a wake-up
broadcasts only the shards of the sessions that are done, to avoid the
thundering herd of a single condition.

##### Flush Stage Shards

The flush stage queue of `Commit_stage_manager` replaces today's single
`m_lock_done`/`m_stage_cond_binlog` pair for binlog sessions with
`m_done_shards[256]`:

- Condition index: `enroll_for(BINLOG_FLUSH_STAGE)` gives each session
  `THD::binlog_flush_cond_index`. A new index starts with the first
  session in the empty queue and after every `FLUSH_COND_MEMBERS` (6)
  sessions of a group (`m_flush_cond_index`, `m_flush_cond_members`). It
  runs under the flush queue lock, so the indexes follow the queue order
  and the sessions sharing an index are consecutive.
- Wait: a session that is not the leader of a stage it enrolls for
  (flush, commit or after commit) waits in
  `wait_for_group_done()` on shard `binlog_flush_cond_index mod 256`
  until `tx_commit_pending` is false. A flush leader that becomes a
  follower in the commit stage therefore waits on its own shard too.
- Committing in slices: the leader of a group that is not ordered cuts
  it where the condition index changes. For each slice, in enroll order
  and its own first, it does what a commit stage leader does for its
  queue, and `signal_done()` wakes only that slice before the next one.
  The commit stage leader of ordered groups passes its whole queue to
  `signal_done()`, which broadcasts once per condition index.

##### Group Leaders

A leader waits on shard `queue_index mod 64` of the writer until the
flush thread or the sync thread has advanced the flushed or synced index
to its group. The thread stores the index, then locks each shard of the
range and broadcasts; the leader checks the index under the shard mutex,
so no wake-up is lost. An ordered leader waits the same way, on the
shard of the previous queue index, until that group has entered the
commit stage.

Rare waits (buffer space, a free queue node, draining the queue) and the
replica appliers in the commit order queue keep a single condition each.

#### 4.1.12 Replica Preserve Commit Order

Appliers that write the binary log need nothing new: an applier gives
the turn to the next one when it enters the flush stage queue, and the
queue order is the binlog order, so the replica's binary log keeps the
source's order.

Appliers that do not write the binary log (binary log or
`log_replica_updates` off) commit in the engine in order and then use
the commit order flush stage (`COMMIT_ORDER_FLUSH_STAGE`).
The two stages are merged only so that one engine log flush
serves both an applier group and a binary log group. There is no
commit order between them: `log_replica_updates` makes all appliers
take the same path, and local sessions are not ordered with appliers.

So COMMIT_ORDER_FLUSH_STAGE is separate from the binary log commit:

- The stage has its own queue lock. The first applier in the empty
  queue is the leader and enters with `LOCK_commit`; it takes the
  queue, flushes the engine log, adds the GTIDs, releases
  `LOCK_commit` and wakes the followers.
- `LOCK_commit` serializes the groups, so their GTIDs are added in
  commit order; an applier still lets the next one commit when it
  enters the queue.
- The binary log flush stage leader does not take this queue. The
  leader hand-over between the two queues is removed.

The split costs at most one more engine log flush, and the binary log
leader does not have to commit applier groups under `LOCK_log`.

#### 4.1.13 Semisynchronous Replication

Two points change; the rest works as today:

- `after_flush` is called by the leader in flush prepare, under
  `LOCK_log`, with the group's end position, before the group is
  written. `LOCK_log` keeps the calls in binlog order.
- The end position that dump threads read up to is published by the
  flush thread after the write, or by the sync thread after the
  `fsync` when `sync_binlog=1`.

#### 4.1.14 Write and Sync Errors

Errors of the flush thread (write), the sync thread (`fsync`) and the
leaders (a member's events, `after_flush`, `after_sync`) all call
`Async_writer::fail()`. With `ABORT_SERVER` it aborts the server as
today. With `IGNORE_ERROR` the binary log must be closed, but not as
`handle_binlog_flush_or_sync_error()` does: it takes `LOCK_log`, and a
`LOCK_log` holder (flush prepare, a drain) may be waiting for the very
group that failed. So no leader or writer thread takes `LOCK_log` for
the error:

```
Leader A                Drain, holds LOCK_log       Sync thread
│                       │                           │
copy, publish,          │                           │
copy_done()             │                           │
│                       │                           │
┌ WAIT synced ────┐     ┌ WAIT queue empty ────┐    fsync fails
│ or failed       │     │ or failed            │    │
└─────────────────┘     └──────────────────────┘    fail(): set
│                       │                           m_failure, wake
│◄──────────────────────┼◄──────────────────────────┘ every wait, exit
│                       │
not synced: commit      close_after_failure():
without binary log,     ┌ WAIT both threads ───┐
no after_sync           │ exited and no        │
│                       │ leader copying       │
│                       └──────────────────────┘
                        truncate the file to
                        m_flushed_pos, close
                        the binary log
```

1. `fail()` sets `m_failure` once; it is never cleared. The flush and
   sync threads stop, and every wait of a leader or a drain (buffer
   space, queue node, flush, sync, queue empty) ends on the failure as
   well as on progress. `fail()` broadcasts all their conditions.
2. A leader that already has its range checks the failure before it
   writes each member; after it, the member's cache is
   discarded and the group is not published. A leader whose wait ended
   on the failure therefore never writes into the buffer.
3. The indexes advance only after the file operation succeeded, so
   `group_done()` tells a leader whether its group was written, and
   synced when it waits for the sync. A group that was not commits in
   the engines without the binary log and does not call `after_sync`,
   as the removed stages did with `IGNORE_ERROR`.
4. The next `LOCK_log` holder (a flush stage leader, rotation, `RESET
   BINARY LOGS AND GTIDS`, `SET binlog_buffer_size`, shutdown) drains
   and finds the failure. `close_after_failure()` waits until both writer
   threads have exited and no leader still copies into the buffer
   (`m_copying_groups`, from `allocate_buffer()` to `copy_done()`);
   neither needs `LOCK_log`. It truncates the file to `m_flushed_pos`,
   the end of the last group written, and closes the binary log as the
   removed flush stage did. Later groups commit without the binary log;
   rotation and reset that find it closed stop as for a closed binary
   log.

##### Errors of a Serial Write

A leader that writes its group straight into the file handles a write
error with `handle_binlog_flush_or_sync_error()` while it still holds
`LOCK_log`, as the removed flush stage did. `stop_before_close()` first
sets the failure and stops the writer threads, so the file is cut at
the end of the last group written before the binary log is closed. The
sync of the group is done by the sync thread, so a sync error is
handled as for any other group.

#### 4.1.15 Replacing Binary Log Group Commit

Every commit that writes the binary log goes through
`Binlog_parallel_flush::commit()`. The groups that needed today's
stages are handled as follows; the sync stage is removed.

##### Groups that do not fit the buffer

A group is written serially when the upper bound of its size is not
smaller than the buffer, when a member has an incident, or when it has
nothing to log. Under `LOCK_log` its leader:

1. waits until every allocated group is written, so that the file
   position is the end of the last group and the flush thread has
   nothing to write; the sync is not waited for;
2. takes the next queue index at the file position, entering async mode
   if needed;
3. writes the group straight into the file with the code of the removed
   flush stage, which also flushes the engine log;
4. marks the group written, as the flush thread does after a write: it
   advances `m_allocated_pos`, `m_flushed_pos` and `m_flushed_index` to
   the end of the group, publishes the end position if `sync_binlog` is
   not 1, and counts the group toward the `sync_binlog` groups after
   which the sync thread is woken, or wakes it at once if the group
   waits for the sync.

From then on the group is like any other: it waits on its queue index
for the sync if needed, and commits, in binlog order when that is
required. A transaction of any size is therefore supported;
`binlog_buffer_size` only decides which groups are copied.

##### Committing in binlog order

With `binlog_order_commits=ON` or when a clone needs the commit order,
groups commit in the engines in binlog order through the commit and
after-commit stages of BGC:

1. In flush prepare, before it counts the group's XID, the leader
   decides whether the group is ordered. The first ordered group after
   unordered ones, or after a drain, first waits until every earlier
   group has committed, since those did not wait for each other.
2. After its flush or sync wait, the leader of an ordered group waits
   until the group before it in binlog order has entered the commit
   stage, then enters it itself and wakes the next group. Entering is
   serialized by a mutex, so the commit stage queue is in binlog order.
3. The first leader in the commit stage queue leads the stage as BGC's
   commit stage leader did: it commits the queue under `LOCK_commit`,
   runs the after-commit stage and releases every session of the queue.
   The other leaders return once released.

Turning the commit order off needs no wait, since unordered groups wait
for no other group.

##### Sessions with data in both caches

This happens only to anonymous transactions: a statement that would fill
both caches is refused as GTID-unsafe when the transaction commits with
a GTID. Such a session is logged as two transactions, the statement
cache first, each with its own anonymous GTID event, and the second
depends on the first. Flush prepare computes two pairs of logical
timestamps for it, one after the other, as the removed flush stage did,
and keeps the second pair for the second transaction. The size bound and
the exact size count both transactions. Transactions on
non-transactional tables need nothing more: their events are in one of
the two caches.

### 4.2 Interface Specification

Not applicable.

### 4.3 Implementation Steps

1. Binlog writer: move `Binlog_event_writer` to its own header; add a
   `binlog_cache_data::flush()` overload that writes through a given writer
   with the group's `immediate_commit_timestamp` (so all members of a group
   share one timestamp); `write_transaction()` takes that timestamp.
2. `sql/binlog_parallel_flush.{h,cc}`: `Async_writer`, the two threads,
   `Binlog_parallel_flush::commit()`, Performance Schema instrumentation
   registration,
   variable globals.
3. `MYSQL_BIN_LOG` integration: create the object for non-relay logs;
   at start-up after recovery, allocate the buffer and start the two
   threads, which run until shutdown; `ordered_commit()` leaves the
   whole commit to `commit()` once the leader holds `LOCK_log`, and the
   code of today's flush, sync and commit stages is removed from it;
   `new_file_impl()` and `reset_logs()` drain first and stop if the
   drain closed the binary log after an error; `close_binlog_on_error()`
   is split out of `handle_binlog_flush_or_sync_error()`; `cleanup()`
   tears down. `raw_get_current_log()` waits for the write queue
   to be empty in async mode. Remove the `LOCK_log` assertion in
   `write_cache()` and the `LOCK_commit` assertion in
   `process_commit_stage_queue()`; `write_cache()` no longer records the
   session's next event position from the file position, which
   `flush_thread_caches()` does for a serial write.
4. GTID state: `update_commit_group()` without `LOCK_commit` and a
   per-call `commit_group_sidnos`. Logical timestamps: computed in flush
   prepare and stored in `Transaction_ctx`, a second pair for a session
   with data in both caches; `write_transaction()` uses them.
5. `Commit_stage_manager`: the sync stage, its queue lock
   `LOCK_sync_queue` and `wait_count_or_timeout()` are removed.
   Commit order flush stage: its own queue lock, entered with
   `LOCK_commit`; remove the leader hand-over, `m_stage_cond_leader`
   and `process_final_stage_for_ordered_commit_group()`.
6. `sys_vars.cc`: two new variables; `binlog_group_commit_sync_delay`
   and `binlog_group_commit_sync_no_delay_count` are deprecated.
   `sync_binlog` and `sync_binlog_interval` wake the sync thread on
   update.
   `binlog_buffer_size` calls `set_buffer_size()` from its `ON_UPDATE`
   function, under the global variable locks.
7. MySQL Test Run (MTR) tests.

Invariants the code relies on:

- Only the `LOCK_log` holder advances the allocated position and index;
  only the flush thread, or a leader after a serial write, advances the
  flushed position and index; only the sync thread advances the synced
  index.
- A slot is "published" when its `end_pos` is beyond the flushed position;
  slots are cleared on rotation so a position from the previous file is
  never mistaken for a published slot.
- Async mode is entered and left only under `LOCK_log`; entering re-reads
  the file position because a rotation may have written meanwhile.
- `m_flush_wait_max` and `m_sync_wait_max` are written only under
  `LOCK_log` and only grow.
- Each group copied into the buffer holds one prepared XID from flush
  prepare until the whole group has committed, so `wait_for_prep_xids()`
  waits for every such group to commit.
- A serial write starts only when the write queue is empty, at the
  file position, and takes the next queue index; it ends with the
  allocated and the flushed position at the end of the group.
- Ordered groups enter the commit stage in queue index order.

### 4.4 Test Plan

Written with the implementation, which is not part of this design-only
pull request:

- `binlog.binlog_parallel_flush_basic`: both wait modes (FR-1, FR-2).
- `binlog.binlog_parallel_flush_buffer_size`: the threads and the buffer
  exist; a new `binlog_buffer_size` replaces the buffer after its groups
  are written; a concurrent `SET` ends with the variable equal to the
  buffer in use; a failed allocation is refused and keeps the current
  buffer; a transaction larger than the buffer keeps the binary log
  open (FR-1).
- `binlog.binlog_parallel_flush_status_position`: `SHOW BINARY LOG STATUS`
  and `SHOW BINARY LOGS` wait for a group being written and show the end
  of the file (FR-2).
- `binlog.binlog_parallel_flush_group`: grouping and followers (FR-2).
- `binlog.binlog_parallel_flush_next_event_pos`: with the flush thread
  held, a group copied behind an unwritten one records the end of its
  own transaction as `binlog_next_event_pos`, not the file position
  (FR-2).
- `binlog.binlog_parallel_flush_tagged_gtid`: groups of tagged GTIDs
  whose GNO and sequence numbers cross the widths 63/64 and 8191/8192,
  mixed with untagged and assigned GTIDs; every event is readable with
  `mysqlbinlog` and `SHOW BINLOG EVENTS` (FR-2).
- `binlog.binlog_parallel_flush_order_commits`: with
  `binlog_order_commits=ON` a synced group is not visible before the
  earlier group commits; with OFF it does not wait; an ordered group
  waits for an unordered group allocated before the change; groups
  larger than the buffer before and after an ordered group (FR-2).
- `binlog.binlog_parallel_flush_direct_write`: a group larger than the
  buffer follows a group held in the buffer, waits for the sync thread
  with `sync_binlog=1`, and with `IGNORE_ERROR` a failed write leaves
  nothing of it in the closed binary log (FR-1, FR-2).
- `binlog.binlog_parallel_flush_both_caches`: a statement with data in
  both caches, a MyISAM `INSERT` and a mixed transaction get the events
  and logical timestamps of the removed flush stage (FR-1, FR-2).
- `binlog.binlog_parallel_flush_large_trx` (big test): a transaction
  larger than 4 GiB is written serially (FR-1).
- `binlog.binlog_parallel_flush_writeset_order`: a group that formats
  its events before an earlier group still gets dependencies in binlog
  order after both reset the writeset history (FR-2).
- `binlog.binlog_parallel_flush_rotate_wait`: rotation waits for the
  followers' commit (FR-2).
- `binlog.binlog_parallel_flush_sync_switch`: `sync_binlog` changed
  between 1 and 0 while groups are in flight (FR-2).
- `binlog.binlog_parallel_flush_write_error`: with `IGNORE_ERROR`, a
  write error and a sync error while `FLUSH BINARY LOGS` drains the
  queue under `LOCK_log`: the transactions commit, the binary log is
  closed and keeps only the complete groups; with `ABORT_SERVER` the
  server stops (FR-2).
- `binlog.binlog_parallel_flush_xa`: XA transactions (FR-2).
- `rpl.rpl_binlog_parallel_flush_dynamic`: `binlog_buffer_size`,
  `sync_binlog` and `binlog_order_commits` changed under load on a
  replicating source, with large transactions (FR-2).
- `rpl.rpl_binlog_parallel_flush_semisync`: semisynchronous replication
  (FR-2).
- `rpl.rpl_binlog_parallel_flush_end_pos`: with `sync_binlog=1` the
  replica receives events only after the sync (FR-2).
- `rpl.rpl_binlog_parallel_flush_commit_order`: with
  `replica_preserve_commit_order=ON` the replica's binary log keeps the
  source's order, also when an earlier group is written serially
  (FR-2).
- `rpl_gtid.rpl_mts_spco_nobinlog_parallel_flush` and
  `rpl_gtid.rpl_mts_replica_preserve_commit_order_with_binlog`: a local
  transaction commits while a commit order leader holds its stage; with
  `binlog_order_commits=ON` it waits for `LOCK_commit` (FR-2).
- Existing binary log group commit tests that tested the sync delay are
  removed; `binlog_error_action` loses the two cases of a group closing
  the binary log without `LOCK_log`, which no longer exists.

To add before merge. Most items run the same workload on a server
without the patch and with it and compare the results; each cites what
it covers.

- Sync delay: nonzero `binlog_group_commit_sync_delay` and
  `binlog_group_commit_sync_no_delay_count` are accepted with a
  deprecation warning and delay no commit (FR-2).
- Content: same events in the same order, same positions, checksums,
  GTIDs and dependency information, compared with `mysqlbinlog`, for every
  combination of checksum, encryption, transaction compression and
  dependency tracking settings (FR-2).
- Durability and crash recovery: crash before a group is published,
  after publish but before sync, and after sync but before engine commit,
  for every
  combination of `sync_binlog` in {0,1,2} and
  `innodb_flush_log_at_trx_commit` in {0,1,2}. After restart, the binary
  log and the engine MUST agree exactly as without the patch; with both
  set to 1, no transaction whose COMMIT returned success is lost (FR-2).
- Reading while writing: dump threads and `mysqlbinlog` read the active
  file under concurrent commits and buffer wrap-around (minimal
  `binlog_buffer_size`, many large transactions); everything before the
  end position is complete and has no gaps (FR-2).
- Group Replication: the full `group_replication` suite, and
  single-primary and multi-primary groups under concurrent writes, with
  `binlog_order_commits` ON and OFF; the same result as without the
  patch (FR-2).
- Consumers: single-threaded and multi-threaded replicas with writeset
  dependency tracking, GTID ownership with many UUIDs, semisynchronous
  source with `AFTER_SYNC` and `AFTER_COMMIT` (FR-2).
- Commit order: clone under load; the same result as without the patch
  (FR-2).
- Mixed groups: non-transactional tables, data in both caches,
  incidents and transactions larger than the buffer in one group with
  other transactions; the same result as without the patch (FR-1,
  FR-2).
- Administration: rotation at `max_binlog_size`, `FLUSH BINARY LOGS`,
  `PURGE BINARY LOGS`, `RESET BINARY LOGS AND GTIDS`,
  `SHOW BINARY LOG STATUS` and `SHOW BINLOG EVENTS` under concurrent
  commits (FR-2).
- Shutdown: clean shutdown with queued groups loses no committed
  transaction; start-up without `--log-bin` (FR-2).
- `sync_binlog` changes: switch between 0 and 1 repeatedly under load
  with a semisync `AFTER_SYNC` replica; no commit stalls for
  `sync_binlog_interval` (FR-2).
- `sync_binlog` greater than 1: with `sync_binlog=2` and the sync held,
  commits continue, and a sync starts after two groups and after
  `sync_binlog_interval`; with `sync_binlog=0` a sync starts after
  `sync_binlog_interval`; with each `fsync` delayed longer than the
  interval, the next sync starts within `sync_binlog_interval` after the
  delayed one completes (FR-2).
- Variables: `sys_vars` tests for the two new variables.
- Regression: full `binlog`, `rpl`, `rpl_gtid`, `innodb`, `sys_vars` suites
  with `binlog_order_commits` ON and OFF (FR-2).
- Resources: buffer memory visible in Performance Schema; no CPU on an
  idle server.
- Performance: sysbench `oltp_write_only` / `oltp_update_non_index`, 8 to
  512 threads, without and with the patch, `sync_binlog` 1 and 0,
  `binlog_order_commits=OFF`; results to issue #832 (NFR-1).
- Performance where groups gain less: the same sysbench runs with
  `binlog_order_commits=ON`, and with transactions larger than
  `binlog_buffer_size` alone and mixed with small ones; with the patch
  MUST NOT be slower (NFR-2).

Deferred: status counters for groups, serial writes and
background-thread activity; long-running crash injection stress in
release QA.

## References

- Primary GitHub Issue: mysql/mysql-server#832
- Related GitHub Issues: mysql/mysql-server#833,
  mysql/mysql-community#114, mysql/mysql-community#126
- Design review PR: to be added when the pull request is opened
- Implementation PRs: the same pull request
- Related WorkLogs and designs: none
