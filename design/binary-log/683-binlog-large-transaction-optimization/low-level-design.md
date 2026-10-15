# Low-Level Design

The low-level design describes how the large transaction optimization (BOLT) is implemented in the server, complementing the high-level design, which covers the mechanism and rationale. It is organized by implementation area: the infrastructure the optimization builds on (including its system and status variables), followed by the commit and recovery paths, and finally testing. Throughout, it references the requirements (FR/NFR).

## Section 1: Infra Setup

This section covers the infrastructure the optimization is built on, including the new binary log event `Large_transaction_header_event`, the changes that let a binlog cache's spill file become a binary log file, the dedicated directory those files live in, and the system and status variables that control and observe it. This infra provides the setup needed to implement the commit and recovery paths, which are covered in Section 2.

### System and Status Variables

Two global, dynamic system variables control the optimization, namely `binlog_large_transaction_optimization_enabled` (boolean, default ON) and `binlog_large_transaction_optimization_threshold` (bytes, default 128 MB, minimum 10 MB). Their full specification (default, scope, privileges, persistence) is in the User Interface section of the HLD, so only the implementation is described here.

Both are defined in `sys_vars.cc` as GLOBAL, `NOT_IN_BINLOG` variables. The 10 MB minimum (FR8) is enforced by the threshold's `VALID_RANGE`. The one implementation subtlety is the interaction with `binlog_cache_size` (FR9.3 to FR9.5). While the optimization is enabled, the effective threshold is held at `max(threshold, binlog_cache_size)`. This is enforced in three places:

- At startup, `update_binlog_large_transaction_optimization_threshold()` (called from `init_common_variables()`).
- When the threshold is set (`check_binlog_large_transaction_optimization_threshold`).
- When `binlog_cache_size` is set (`fix_binlog_cache_size`).

In all three cases, if the requested threshold is below `binlog_cache_size` it is raised to match, and, when the optimization is enabled, a warning is emitted to both the client and the error log (`ER_BINLOG_BOLT_THRESHOLD_ADJUSTED`, `ER_BINLOG_BOLT_THRESHOLD_ADJUSTED_SQL_WARNING`).

In addition to the system variables, we also introduce two atomic status counters that report how often the optimization runs: `binlog_large_transaction_optimization_count` is incremented once per successful promotion, and `binlog_large_transaction_optimization_missed_count` is incremented during fallback. Both are exposed through `SHOW GLOBAL STATUS` and `performance_schema.global_status`.

### `Large_transaction_header_event`

A new event type, `LARGE_TRANSACTION_HEADER_EVENT = 43`, is added to `Log_event_type`. As described in the high-level design, it serves two purposes: it records where the transaction's terminating event sits, so recovery can seek to it, and its padding fills whatever the fixed header events leave unused in the reserved region. Its body is `version` (1 byte), `terminating_event_offset` (8 bytes), `terminating_event_type` (1 byte), `terminating_event_body_length` (1 byte), `terminating_event_body` (at most 141 bytes: the terminating event's body, that is, its XID), and variable-length padding.

Following the usual MySQL event split, it is implemented as two classes:

- `mysql::binlog::event::Large_transaction_header_event`, the binlog-events library class, handles deserialization. It exposes the format constants (`kVersion = 1`, `kFixedBodyLength = 1 + 8 + 1 + 1 = 11`, `kMaxTerminatingEventBodyLength = 141`), validates the version, reads the terminating event's offset, type and body, and treats the remainder as padding.
- `Large_transaction_header_log_event`, the server class, inherits from the library event and from `Log_event` and handles serialization (`write_data_body`), `pack_info`/`print`, and `do_apply_event`, which is a no-op since the event carries recovery metadata only. Its constructor always sets `LOG_EVENT_IGNORABLE_F`, so the event is transparent to anything that does not understand it, preserving replication and tooling compatibility (NFR8, NFR10).

### Promotable Binlog Cache

During query processing, the server generates the query's binlog events and writes them to the per-thread cache (`binlog_cache_data`). The first write to the transaction cache latches the BOLT decision for the whole transaction (`latch_large_trx_optimization`), which captures the value of the knob and threshold once, so any mid-transaction change to those settings is ignored. At the same time, when the knob is enabled, it marks the cache's future spill file as a named file (it sets `IO_CACHE::named_file`), which is what later allows the file to be renamed on the filesystem and promoted into a binary log file. If that flag is left unset (the knob was off, or this is the statement cache), the spill file is created as an anonymous, unlinked file with no name in the filesystem, so it can never be renamed or promoted.

The current implementation of binlog cache is layered: `binlog_cache_data` (event buffer) -> `Binlog_cache_storage` (byte-container facade) -> `IO_CACHE_binlog_cache_storage` (mysys-backed implementation over one `IO_CACHE`). BOLT changes three things in this path:

- **Where the binlog spill file lives.** While the physical file is still created lazily on the first spill by the existing `IO_CACHE`, it is created in this predefined temp directory (`Binlog_cache_storage::open` passes `binlog_temp_files_dir.path()`) rather than the general temp dir. This holds whether or not the BOLT knob is on. Every other `IO_CACHE` user (for example filesort and internal temporary tables) is unchanged and still spills anonymously into `--tmpdir`.
- **A reserved region at the front.** Opening the cache also shifts its write start position past a reserved header region at the front of the file (`IO_CACHE_binlog_cache_storage::open`). Every binlog cache reserves the region even when the BOLT knob is off. A cache whose spilled file is never promoted simply leaves the region empty and never copies it into the binary log, so it is invisible in the output.

  - Why is the region reserved unconditionally, rather than by checking the knob at cache initialization: the cache is initialized once per session and reset between transactions. If the decision on whether to reserve space at init were based on the BOLT knob, we would need to ensure that reset checks the same value that init checked. Since the knob is a dynamic global that can change at any time, gating on it would effectively fix the knob per connection: enabling the optimization at runtime would have no effect on existing sessions until they reconnect.

<!-- -->

- **Naming the file for promotion.** The first write that actually spills to disk renames the file from its generic mkstemp name to the `bolt_<server_start_time hex>_<serial hex>` form (`IO_CACHE_binlog_cache_storage::write` -> `rename_spilled_file`), but only for a cache marked as named. A cache that was not marked named (the knob was off, or it is the statement cache) keeps its anonymous, unlinked file and is never promoted.

  - A `bolt_` name only records that the binary log cache created the file; it is not a promise the file will be promoted. Every spilled transaction cache gets the name while the optimization is enabled, including transactions that eventually commit through the standard code path because they fall below the threshold or hit a fallback condition at commit.

The first event offset written into the spilled file does not start at zero. Instead, the cache reserves a block of bytes at the front of the file when it is opened (`binlog_cache_data::open`), sized as the length of the active binary log file's header (every event written before its first transaction) plus 32 KB of headroom, rounded up to a 64 KB quantum. The size is computed once when a binary log file is opened (`set_binlog_cache_reserved_bytes`) and read at each transaction's first event (`get_binlog_cache_reserved_bytes`), so long-lived sessions follow the current header. The cache then places all of its data after this reserved region.

### Temp-files Directory

Every binlog cache spill file is created in a dedicated directory next to the binary logs, `#binlog_temp_files`. At startup, `Binlog_temp_files_dir::init()` creates the directory, or, if it already exists, clears leftover `bolt_` files from a previous run (executed by `temp_files_dir_clear_files`). The cleanup rejects a symlinked directory and does not delete anything that is not a regular file it recognizes by the `bolt_` naming (which is validated through the function `is_bolt_temp_file`).

A promoted file becomes a real binary log file, so it must carry the same permissions a normal binary log file would. Normal binlog files are created honoring `my_umask` via `my_open`, but the temp file is created by mkstemp (`mysql_file_create_temp`), which forces 0600 and ignores the umask. BOLT therefore reconstructs the umask derived permission set (`binlog_temp_file_permissions`) and calls `my_chmod` on the promoted file to match. This reproduces the regular binlog file permissions rather than inventing a new scheme.

## Section 2: Commit and Recovery

This section walks the three code paths a transaction exercises around commit: the **write code path**, where events are generated and staged in the binlog cache; the **commit code path**, where the transaction is either promoted or committed through the standard path; and the **recovery code path**, where a promoted file is read back after a crash. Each is described below with a diagram of its flow.

### Write code path

```
        Event generated for the transaction
                      |
                      v
        write to binlog_cache_data
        (raw event)
                      |
            exceeds binlog_cache_size?
              /                    \
            no                      yes
             |                       |
             v                       v
        stays in memory      spill to temp file in
                             #binlog_temp_files

   log_pos and checksum are attached when the cache is
   written out to a binary log file:
     standard path -> during the copy at commit
                      (Binlog_event_writer)
     BOLT on       -> as content is written to the spill
                      file, after the reserved region
```

As a transaction runs, its events are written into the transaction cache (`binlog_cache_data`) as raw events. `log_pos` and the checksum are attached when the cached bytes are written out to a binary log file, not while they sit in the cache.

On the standard path this happens at commit: `Binlog_event_writer` copies the cache into the active binary log and stamps each event's `log_pos` (its destination offset) and checksum as it copies.

With BOLT on, the spilled file itself becomes the binary log file, so there is no later copy step. `log_pos` and checksum are attached as the cache content is written to the spill file, with events placed after the reserved header region (Section 1). Because the reserved region fixes where the first event begins, each `log_pos` is final and the file is already in binary-log shape, so promotion at commit needs no rewrite.

#### Checksum overhead

Standord binary logging computes an event's CRC once, when the cache is copied into the active binary log (the same pass that also assigns its final `log_pos`); cached events carry no CRC. BOLT adds a new point at which the CRC is calculated. With BOLT enabled, the CRC is attached when writing to the transaction cache, which shifts the computation earlier and changes the existing behavior, where now the CRC can be computed twice. This happens in three of the four cases:

- Small transactions that never spill: computed twice, once into the cache and once into the binary log.
- Transactions that spill but stay below the promotion threshold (128 MB by default): computed twice, once at the spill write and again when the standard commit path copies the spilled file into the active binary log and reassigns `log_pos`.
- Large transactions that spills and commits through BOLT computes the CRC exactly once, when writing into the cache.
- Transactions that spill above the threshold but fall back to the standard path: computed twice, for the same reason.

We chose this implementation because deferring the checksum computation until spill time would require substantial changes. The main blocker is savepoint handling. When a savepoint is set, the current position in the cache is recorded. If the checksum were added later, at spill time, that recorded position would no longer match the file: the position was recorded without the checksum bytes, while the file now contains checksums. This gap is fixable, but the position is stored in two places (the savepoint's slot in the SQL layer, and as a key in the cache's own state map), so correcting it means we first need to determine what the correction should be and then apply it in both places, which is a substantial change.

**Interaction with compression**

Compression also decides whether a checksum is written into the cache, because events inside a compressed payload must not carry one. When `binlog_transaction_compression` is on at the transaction's first event, `BINLOG_CHECKSUM_ALG_OFF` is recorded and no CRC enters the cache. Compression and promotion are then mutually exclusive outcomes: a transaction that does get compressed is not promotable, and falls back with `kCompression`. As a defence, `shall_compress()` also declines compression outright if the cache already holds checksums, since that combination would produce a malformed payload; that requires compression to have been enabled after the first event, which the session-variable semantics prevent, so it is unreachable in practice.

This gives three cases:

1. **Compression on at the first event, and compression succeeds.** No checksum is in the cache, and BOLT rejects the transaction because it is compressed.
2. **Compression on at the first event, but** `shall_compress()`** declines for another reason.** The cache holds no checksum, and promotion requires the recorded algorithm to match `binlog_checksum`. With `NONE` the two agree, so the transaction can commit through BOLT if the remaining checks pass, and the promoted file is checksum-free, matching the configuration. With `CRC32` they disagree, so BOLT rejects it and it commits through the standard path, where the checksum is computed while copying into the binary log.
3. **Compression off throughout.** `shall_compress()` returns at its first check, and the defensive check above is never reached.

#### Durability policy

In the standard commit code path, every transaction prepares with `HA_IGNORE_DURABILITY`, so the engine writes its prepare record to its own log (for example InnoDB's redo log) without persisting it during prepare. That debt is repaid once per group in the binlog flush stage, which calls `ha_flush_logs(true)` before any cache reaches the binary log.

A BOLT transaction commits through the same group commit, and its file is promoted in the flush stage, after that call, so the same flush covers it.

The durability property is deliberately not chosen per transaction during prepare, because it is not yet known at that point whether the transaction's spilled file will be promoted. During prepare, the terminal event has not been appended, so the cache is short of its final size, and a transaction just below the threshold at that point can still cross it and have its spilled file promoted.

### Commit code path

```
     COMMIT: get_cache_for_large_trx_commit()
                          |
          +---------------+----------------+
          |                                |
   candidate, no blocker               otherwise
   (6 checks)                              |
          |                                |
   sync the spilled file; mark the         |
   cache for promotion; the session        |
   must lead its flush group               |
          |                                |
          v                                v
                    ordered_commit()
   flush stage, the BOLT transaction first in its group:
     rotate_and_promote_spilled_file()
       encryption, checksum, log open?  -- no --> written as usual,
          | yes                                  in the active file
       new_file_impl(promotion): wait for prepared XIDs,
         persist GTIDs, Rotate event into N, close N
         open_binlog(N+1): write N+1's header events, then
           promote_spilled_file():
             copy N+1's header into the reserved region,
             add the Large_transaction_header and Gtid events,
             sync, rename the spilled file to replace N+1, reopen it
             (a failure before the rename keeps the fresh N+1,
              and the transaction is written into it)
           add N+1 to the index, publish its header end
     the rest of the group is written into N+1
   sync stage, commit stage, hooks: as for any group
```

When `get_cache_for_large_trx_commit()` returns a cache (§2.4), `MYSQL_BIN_LOG::commit()` marks it for promotion and the transaction commits through `ordered_commit()`, like any other. The promotion is a binary log rotation in the flush stage that opens the spilled file as the new binary log file N+1, so BOLT reuses the existing rotation: GTID persistence, the Rotate event, the purge index, the main index, the dependency tracker reset. Commit order (replica preserve-commit-order), the binlog group commit tickets of Group Replication, and the `after_flush`, `after_sync` and `after_commit` hooks all come from `ordered_commit()`.

First in its flush group. A rotation needs every transaction prepared in the old file to be committed in the engines first (`wait_for_prep_xids()`), because crash recovery scans only the last binary log. Members of the same flush group written before the BOLT transaction would be prepared in N and committed later by the same leader, so the BOLT transaction must be the first transaction of its group. It is always made the leader of its flush group (`Commit_stage_manager::append_to()`, `Binlog_group_commit_ctx::must_lead_flush_group()`). If the flush queue is empty, it enrolls as the leader. Otherwise it waits in a first-in, first-out candidate list, and the leader that fetches the current flush queue appends the first candidate to the emptied queue, under the flush queue lock, as the leader of the next group (`Commit_stage_manager::assign_next_flush_leader()`). Transactions that arrive after it join its group behind it, in the binary log after it. The flush stage lets only the group's first transaction promote its spilled file (`set_flushing_group_leader()`).

The mapping to code:

```
// MYSQL_BIN_LOG::commit()
get_cache_for_large_trx_commit(cache_mngr);   // candidate and blockers (§2.4)
flush_and_sync_spilled_file();                // the body, outside LOCK_log
cache->request_spilled_file_promotion();
binlog_group_commit_ctx().set_must_lead_flush_group(true);
ordered_commit(thd, all, skip_commit);

// flush stage: binlog_cache_data::flush(), first transaction of the group
rotate_and_promote_spilled_file(thd, this, &promoted);
  // checks before the rotation: log open, encryption, checksum algorithm
  new_file_impl(false, nullptr, &promotion);  // the standard rotation
    open_binlog(..., &promotion);
      // after N+1's header events, before N+1 is added to the index:
      promote_spilled_file(&promotion);
        // recheck encryption and checksum under LOCK_log
        // read N+1's header events; check the reserved region fits
        m_dependency_tracker.rotate(); step(); // logical timestamps restart
        // write header copy + Large_transaction_header + Gtid, sync
        my_rename(temp_file_name, log_file_name);  // point of no return
        reopen_promoted_binlog_file();         // m_binlog_file at its end
      add_log_to_index(); close_purge_index_file();
      atomic_binlog_end_pos = header end;      // the rest after after_flush
reset(true /*preserve_spilled_file*/);         // the cache keeps the file
```

Header + LTH sizing. The reserved region holds a copy of N+1's header events (magic, Format_description, Previous_gtids) as `open_binlog()` wrote them, then the `Large_transaction_header_event`, whose padding is sized so the following `Gtid_log_event` ends exactly at reserved_bytes, where the first spilled event was placed. If the GTID state grew since the spill and the header no longer fits, the transaction is written into the fresh N+1 (§2.4).

GTID + dependency reset. A promoted file starts a new binary log file, so the dependency tracker is rotated before the transaction's logical timestamps are generated. This makes the transaction a parallelization barrier (serialization point) for the replica's parallel applier.

Crash safety. `open_binlog()` registers N+1 in the purge index before it writes N+1's header, so until N+1 is in the main index, a crash leaves either file owned by the purge index and removed at startup, and the prepared transaction is rolled back. After the main-index update, recovery finds the transaction in N+1 and commits it.

#### Fallback

BOLT checks whether promotion is possible at two points along the commit path. If any check fails, the transaction falls back to the standard commit path, and for the recorded reasons it increments `binlog_large_transaction_optimization_missed_count`.

Check 1. Before promotion starts (`get_cache_for_large_trx_commit`, called from `MYSQL_BIN_LOG::commit`).

First a basic gate decides whether the transaction is a candidate: the knob is on, the cache actually spilled, and the spilled size is above the threshold. For a candidate, all six of the following must hold, otherwise it falls back:

- the statement cache is empty,
- the transaction has no pending incident,
- it contains only row-format events,
- its spilled file is not encrypted,
- it is not compressed,
- `binlog_checksum` did not change during the transaction.

Check 2. In the flush stage (`rotate_and_promote_spilled_file()`), before the rotation, under `LOCK_log`: the binary log is open, `binlog_encryption` is OFF, and the cache's checksum algorithm is `binlog_checksum`. A transaction that fails these is written into the active file as usual.

Check 3. During the rotation (`promote_spilled_file()`), later conditions can still force a fallback:

- `gtid_persistence`: persisting the outgoing log's GTIDs to `mysql.gtid_executed` fails because that table is read-only; the rotation keeps the current binary log, and the transaction is written into it,
- `reserved_header_space`: the header events no longer fit the reserved region, and
- `promotion_failed`: an I/O error before the rename (reading N+1's header, writing the header, the rename itself). A failure to sync the spilled file is detected earlier, in `MYSQL_BIN_LOG::commit()`, which syncs it before the transaction enters `ordered_commit()`; the transaction is then written into the active file, with the same reason.

For the last two, the rotation has opened the fresh N+1, and the transaction is written into it as on the standard path. Nothing irreversible has happened at any of these points.

#### Fallback vs. failure

All of the above are clean fallbacks: the transaction still commits, just through the standard path. Only failures after the rename, the point of no return, fail the transaction; they are rotation errors (`open_binlog()` removes N+1 and `new_file_impl()` applies `binlog_error_action`: abort the server, or disable binary logging and let the commit proceed in the engine).

A fallback writes the transaction into the binary log through the standard flush (`write_transaction()` → `do_write_cache` → `Binlog_cache_storage::copy_to`), which copies the spilled cache out of the temp file in the standard form. The binary log writer sets each event's `log_pos` for its new position and adds the checksum, and the `Gtid_log_event`'s transaction length is recomputed for the destination's checksum setting by `set_trx_length_by_cache_size()`. The reserved region at the front of the temp file is skipped by the read cursor, and the temp file is released when the cache is reset.

### Recovery code path

```
   Recovery scan of the last binary log file
                     |
                     v
     Large_transaction_header_event encountered
                     |
                 relay log?
            /                            \
          yes                             no
           |                               |
           v                               v
   ignore the header;              read the event at the recorded
   fall back to ordinary           offset (checksum verified): does
   sequential scan                 it have the recorded type and
                                   body (XID)?
                                       /                \
                                     yes                 no
                                      |                   |
                                      v                   v
                           seek to it; the          ignore the header;
                           transaction is           read the transaction
                           complete; collect        sequentially
                           its XID / GTID
```

The recovery behavior on a binlog file promoted through the BOLT code path mostly reuses the existing recovery, aside from adding one extra optimization.

While recovery scans the last binary log file, encountering a `Large_transaction_header_event` lets it skip the transaction body. The header carries the offset, type and body (the XID) of the transaction's terminating event. `process_large_trx_header_event()` calls `find_large_trx_terminating_event_end()`, which reads the event at that offset with a checksum-verifying reader and checks its type and body against the header. Only then does it seek to the event, which the scan reads next and collects as usual. If the offset is not ahead of the current position, or the event does not match, a warning is logged and the transaction is read sequentially (FR19.6).

Relay logs skip this shortcut and use the ordinary sequential scan, because the header's offset refers to the source's binary log rather than the relay log (gated by `is_relay_log_recovery()` in crash recovery and `is_relay_log` in GTID reconstruction). Binary logs use it whatever their `binlog_checksum` and whatever `source_verify_checksum`: that option verifies the checksum of each event that is read, and the terminating event is read through a checksum-verifying reader. The five crash window cases are covered in the HLD and not repeated here.

The same validate-then-seek optimization is applied at both places recovery reads a binary log file, through `find_large_trx_terminating_event_end()`: when the GTID set is collected (`read_gtids_from_binlog`, which also checks that the event ends where the transaction length of the following Gtid event says), and during 2PC crash recovery (`Log_sanitizer::process_large_trx_header_event()`). Neither derives an offset from an event's 4-byte `log_pos`, so a terminating event beyond 4 GiB is found.

### Savepoint handling

BOLT adds no new savepoint logic; it reuses MySQL's existing `ROLLBACK TO SAVEPOINT` handling (`binlog_savepoint_rollback`). If the transaction has touched a non transactional table, the server writes a `ROLLBACK TO SAVEPOINT` event into the cache as it does today; otherwise, it truncates the cache back to the savepoint position (`restore_savepoint`). The only BOLT specific detail is that the truncation is reserved region aware: the storage layer shifts the requested offset past the reserved header bytes (`IO_CACHE_binlog_cache_storage::truncate`), and when the spilled file is later synced (`flush_and_sync_spilled_file`), any stale bytes past the new logical end are cut with `my_chsize`, so a promoted file still ends exactly at its terminating event. Whether the spilled file of the (possibly shrunk) transaction is promoted is decided normally at commit, by comparing its spilled size against the threshold.

### Refactoring / modularization

- `MYSQL_BIN_LOG::create_gtid_event()` in `binlog.cc` builds a transaction's `Gtid_log_event` from the THD and the dependency tracker (its logical timestamps, commit timestamps and server versions). Both `write_transaction()` and the promotion use it.
- `large_trx_commit.{h,cc}` holds the BOLT-only eligibility and promotion code (`get_cache_for_large_trx_commit`, `rotate_and_promote_spilled_file`, `promote_spilled_file`).
- `cache_data.h` holds `binlog_cache_data` and the cache manager, moved out of `binlog.cc` so the separate `large_trx_commit.cc` translation unit can use them.

## Section 3: Feature compatibility

### Semi-sync replication (compatible)

BOLT works with semi synchronous replication: a transaction whose spilled file is promoted commits through `ordered_commit()`, which runs `after_flush`, `after_sync` and `after_commit` at their standard places, with the transaction's position in the promoted file (the end of the file, set by the flush stage). The rotation that promotes the file publishes only the end of the file's header events, under the binlog-index lock as for any new file; the transaction is published by the flush stage after the `after_flush` hook, so a dump thread cannot send it before the hook registered it.

### Group replication (incompatible)

BOLT is not compatible with Group Replication and is disabled while Group Replication is running.

The incompatibility comes from the fact that BOLT writes a checksum into the binlog cache. On the standard path, a transaction's events are serialized into the cache without a checksum, and the checksum is added later, when the cache is copied into the binary log. Unlike the standard path, BOLT promotes the temp file itself into a binary log file, so there is no copy step at which to add the checksum.

Group Replication reads the same binlog cache through the `before_commit` observer and broadcasts its bytes to the group as is, expecting the cache to have no per event checksum. A cache written with BOLT enabled carries checksums and breaks that contract.

With Group Replication running, the optimization is off for the transaction from its first event, so no checksum is written into the cache. Two checks guard this: (1) at the first write into the binlog cache, and (2) when deciding whether to commit through the optimized path. The first check ensures that no checksum is written into the cache, while the second catches a transaction that started before Group Replication began.

There is no inherent challenge in adding BOLT support for Group Replication, but it is out of scope for this PR.

### Encryption (incompatible)

BOLT does not work with encryption. A large transaction's spilled file is promoted only when neither its spilled file is encryption nor the `binlog_encryption` is set to ON:

| No. | **spilled file** | `binlog_encryption` | **outcome** |
| --- | --- | --- | --- |
| 1 | plaintext | OFF | promote |
| 2 | plaintext | ON | fall back |
| 3 | encrypted | OFF | fall back |
| 4 | encrypted | ON | fall back |

Two checks guard the correctness of this table.

The first is in `get_cache_for_large_trx_commit`, during commit, when we decide whether to use BOLT:

```
Large_trx_fallback_reason large_trx_commit_blocker() {
  if (trx_cache->get_cache()->is_encrypted() || rpl_encryption.is_enabled())
    return Large_trx_fallback_reason::kEncryption;
}
```

The second check is in the flush stage, under `LOCK_log`, before the rotation (`rotate_and_promote_spilled_file()`), and again inside the rotation (`promote_spilled_file()`):

```cpp
int MYSQL_BIN_LOG::rotate_and_promote_spilled_file(...) {
  ...
  } else if (rpl_encryption.is_enabled()) {
    promotion.fallback_reason = Large_trx_fallback_reason::kEncryption;
  ...
}
```

`binlog_encryption` is a dynamic variable, so it can change between `get_cache_for_large_trx_commit` and the actual promotion of the temp file. Rechecking under `LOCK_log` guarantees the correctness of the encryption check, regardless of whether `Rpl_encryption::enable` or BOLT acquires `LOCK_log` first.

If `Rpl_encryption::enable` acquires `LOCK_log` first, then the BOLT transaction's flush leader is blocked before it tries to promote the temp file. By the time BOLT acquires `LOCK_log`, `Rpl_encryption::enable` has completed and `Rpl_encryption::is_enabled` returns true, so BOLT sees that encryption is enabled and falls back to the standard commit code path.

If BOLT acquires `LOCK_log` first, there are two sub cases:

- If `Rpl_encryption::enable` has started but not yet finished recovering the master key (`recover_master_key()` is still in progress), `Rpl_encryption::is_enabled` returns false, so BOLT proceeds with writing a plaintext file. The same gap exists in `Binlog_ofile::open()` and BOLT has the same behaviour.
- If `Rpl_encryption::enable` has completed `recover_master_key()` and is blocked on `rotate_logs()` because it needs `LOCK_log` to rotate, then `Rpl_encryption::is_enabled` returns true at this point, so BOLT falls back.

---