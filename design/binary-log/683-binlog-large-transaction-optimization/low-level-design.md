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

A new event type, `LARGE_TRANSACTION_HEADER_EVENT = 43`, is added to `Log_event_type`. As described in the high-level design, it serves two purposes: it records where the transaction's terminating event sits, so recovery can seek to it, and its padding fills whatever the fixed header events leave unused in the reserved region. Its body is `version` (1 byte), `terminating_event_offset` (8 bytes), `terminating_event_type` (1 byte), and variable-length padding.

Following the usual MySQL event split, it is implemented as two classes:

- `mysql::binlog::event::Large_transaction_header_event`, the binlog-events library class, handles deserialization. It exposes the format constants (`kVersion = 1`, `kFixedBodyLength = 1 + 8 + 1 = 10`), validates the version, reads the terminating event's offset and type, and treats the remainder as padding.
- `Large_transaction_header_log_event`, the server class, inherits from the library event and from `Log_event` and handles serialization (`write_data_body`), `pack_info`/`print`, and `do_apply_event`, which is a no-op since the event carries recovery metadata only. Its constructor always sets `LOG_EVENT_IGNORABLE_F`, so the event is transparent to anything that does not understand it, preserving replication and tooling compatibility (NFR8, NFR10).

### Promotable Binlog Cache

During query processing, the server generates the query's binlog events and writes them to the per-thread cache (`binlog_cache_data`). The first write to the transaction cache latches the BOLT decision for the whole transaction (`latch_large_trx_optimization`), which captures the value of the knob and threshold once, so any mid-transaction change to those settings is ignored. At the same time, when the knob is enabled, it marks the cache's future spill file as a named file (it sets `IO_CACHE::named_file`), which is what later allows the file to be renamed on the filesystem and promoted into a binary log file. If that flag is left unset (the knob was off, or this is the statement cache), the spill file is created as an anonymous, unlinked file with no name in the filesystem, so it can never be renamed or promoted.

The current implementation of binlog cache is layered: `binlog_cache_data` (event buffer) -> `Binlog_cache_storage` (byte-container facade) -> `IO_CACHE_binlog_cache_storage` (mysys-backed implementation over one `IO_CACHE`). BOLT changes three things in this path:

- **Where the binlog spill file lives.** While the physical file is still created lazily on the first spill by the existing `IO_CACHE`, it is created in this predefined temp directory (`Binlog_cache_storage::open` passes `binlog_temp_files_dir.path()`) rather than the general temp dir. This holds whether or not the BOLT knob is on. Every other `IO_CACHE` user (for example filesort and internal temporary tables) is unchanged and still spills anonymously into `--tmpdir`.
- **A reserved region at the front.** Opening the cache also shifts its write start position past a reserved header region at the front of the file (`IO_CACHE_binlog_cache_storage::open`). Every binlog cache reserves the region even when the BOLT knob is off. A cache that is never promoted simply leaves the region empty and never copies it into the binary log, so it is invisible in the output.

  - Why is the region reserved unconditionally, rather than by checking the knob at cache initialization: the cache is initialized once per session and reset between transactions. If the decision on whether to reserve space at init were based on the BOLT knob, we would need to ensure that reset checks the same value that init checked. Since the knob is a dynamic global that can change at any time, gating on it would effectively fix the knob per connection: enabling the optimization at runtime would have no effect on existing sessions until they reconnect.

<!-- -->

- **Naming the file for promotion.** The first write that actually spills to disk renames the file from its generic mkstemp name to the `bolt_<server_start_time hex>_<serial hex>` form (`IO_CACHE_binlog_cache_storage::write` -> `rename_spilled_file`), but only for a cache marked as named. A cache that was not marked named (the knob was off, or it is the statement cache) keeps its anonymous, unlinked file and is never promoted.

  - A `bolt_` name only records that the binary log cache created the file; it is not a promise the file will be promoted. Every spilled transaction cache gets the name while the optimization is enabled, including transactions that eventually commit through the standard code path because they fall below the threshold or hit a fallback condition at commit.

The first event offset written into the spilled file does not start at zero. Instead, the cache reserves a block of bytes at the front of the file when it is opened (`binlog_cache_data::open`), sized as the current `Previous_gtids` size plus 32 KB of headroom, rounded up to a 64 KB quantum (`get_binlog_temp_file_reserved_bytes`). The cache then places all of its data after this reserved region.

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

In the standard commit code path, every transaction prepares with `HA_IGNORE_DURABILITY`, so the engine writes its prepare record to its own log (for example InnoDB's redo log) without persisting it during prepare. That debt is repaid once per group in the standard commit path's binlog flush stage, which calls `ha_flush_logs(true)` before any cache reaches the binary log.

BOLT bypasses that group flush stage, so it repays the same debt itself, with an unconditional `ha_flush_logs(true)` in `commit_large_transaction()`, before the promotion.

Unlike the standard path, BOLT takes this flush with no binlog lock (no `LOCK_log` and no queue lock) held. No lock is needed as the flush and the promotion happen in the same thread, so this transaction's prepare record is on disk before the transaction reaches the binary log.

The durability property is deliberately not chosen per transaction during prepare, because it is not yet known at that point whether the transaction will be promoted. During prepare, the terminal event has not been appended, so the cache is short of its final size, and a transaction just below the threshold at that point can still cross it and be promoted.

### Commit code path

```
              COMMIT: get_cache_for_large_trx_commit()
                          |
          +---------------+-----------------+
          |                                 |
   spilled, > threshold,                otherwise
   and eligible (6 checks)                  |
          |                                 v
          v                          ordered_commit()
   commit_large_transaction()        (standard group commit:
          |                           copy cache into active
          |                           binlog)
          v                                 ^
   header fits and                          |
   GTIDs persisted?  ------- No ------------+
          |                              (fallback)
         Yes
          |
          v
   Promote: sync body, write header into reserved region,
   register in purge index, rename into binlog sequence,
   Rotate event on old file, add to main index
          |
          v
   Engine commit (finish_commit);
   rotate if > max_binlog_size
```

When `get_cache_for_large_trx_commit()` returns a cache (§2.4), `commit_large_transaction()` promotes it using the optimization commit code path. It performs the HLD's 13 commit steps; steps 6 to 10 reuse MySQL's existing crash-safe rotation (register in the purge index -> rename -> Rotate event -> add to the main index -> clear the purge index), so BOLT does not reimplement rotation. The mapping to code:

```
// sql/binlog/large_trx_commit.cc  commit_large_transaction()
// (comment numbers = the HLD's 13 commit steps)
flush_and_sync_spilled_file();                      // 1   make the body durable
init_thd_variables(...);
LOCK_log; wait_for_prep_xids(); LOCK_commit;        // 2   acquire locks
generate_new_name(new_name, name);                  // 3   next binlog file name
persist_gtids_on_rotate(..., &keep_current_binlog); //     read-only table -> fallback
write_promoted_binlog_header(..., &fits);           // 4,5 write header, sync temp file
  if (!fits) goto fallback_to_ordered_commit;
open_purge_index_file / register_create_index_entry
  / sync_purge_index_file;                          // 6   record file in purge index
my_rename(temp_file_name, new_name);
  mysql_file_sync(spilled_file());                  // 7   promote (rename) + sync
Rotate_log_event r(...); write_event_to_binlog(&r);
  m_binlog_file->flush_and_sync();                  // 8   chain old file -> new file
open_binlog(..., promoted_file_is_renamed = true);  // 9,10 add to index, clear purge index
binlog_large_transaction_optimization_count++;      //     success counter (under LOCK_log)
unlock LOCK_log;                                    // 11  release log lock
cache_data->reset(true /*preserve_spilled_file*/);
finish_commit(thd);                                 // 12  commit in the engine
unlock LOCK_commit;
if (rotate_if_needed()) ...;                        // 13  rotate if > max_binlog_size
```

Three parts are new or worth calling out:

Header + LTH sizing (step 4). `write_promoted_binlog_header()` serializes magic + FDE + Previous_gtids, then sizes the `Large_transaction_header_event`'s padding so the following `Gtid_log_event` ends exactly at reserved_bytes, where the first spilled event was placed. If the GTID state grew since spill and the header no longer fits, it sets \*fits = false and BOLT takes a clean fallback (§2.4).

GTID + dependency reset. A promoted file starts a new binary log file, so the dependency tracker is rotated before the transaction's logical timestamps are generated. This makes the transaction a parallelization barrier (serialization point) for the replica's parallel applier.

Locking. `LOCK_log` is held from step 2 to 11 and `LOCK_commit` through the engine commit, which is the same locks the standard group-commit flush/commit stages use, so no standard commit, other promotion, or rotation can interleave. `LOCK_log` is released before the engine commit (step 11 before 12) so the next transaction's flush can overlap this one's engine commit, exactly as the standard pipeline does.

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

Check 2. After promotion has started (inside `commit_large_transaction`), two late conditions can still force a fallback:

- `gtid_persistence`: persisting the outgoing log's GTIDs to `mysql.gtid_executed` fails because that table is read-only, and
- `reserved_header_space`: the header events no longer fit the reserved region.

Both jump to `fallback_to_ordered_commit`, which cleanly re-runs the standard commit path; nothing irreversible has happened at either point.

#### Fallback vs. failure

All of the above are clean fallbacks: the transaction still commits, just through the standard path. Only genuine I/O errors after the promotion's point of no return fail the transaction; those are handled by `handle_binlog_flush_or_sync_error` per `binlog_error_action` (abort the server, or disable binary logging and let the commit proceed in the engine).

A fallback re-enters the standard commit path (`MYSQL_BIN_LOG::ordered_commit`), which copies the spilled cache out of the temp file and into the active binary log (`do_write_cache` -> `Binlog_cache_storage::copy_to`). As each event is copied, the writer overrides its `log_pos` for the new position, and the `Gtid_log_event`'s transaction length is recomputed for the destination's checksum setting by `set_trx_length_by_cache_size()` (-> `adjust_trx_length_to_checksum_changes()`). Those are the same functions the normal path, and BOLT's own header write, already use, so there is no BOLT-specific fallback handling. The reserved region at the front of the temp file is skipped by the read cursor, and the temp file is released when the cache is reset.

### Recovery code path

```
   Recovery scan of the last binary log file
                     |
                     v
     Large_transaction_header_event encountered
                     |
    relay-log recovery OR source_verify_checksum ON?
            /                            \
          yes                             no
           |                               |
           v                               v
   ignore the header;              validate offset + type,
   fall back to ordinary           then seek directly to the
   sequential scan                 terminating event
                                           |
                                           v
                             event at that offset matches
                             the recorded type?
                                 /                \
                               yes                 no
                                |                   |
                                v                   v
                     transaction is complete;   file marked malformed,
                     collect its XID / GTID      transaction rolled back
```

The recovery behavior on a binlog file promoted through the BOLT code path mostly reuses the existing recovery, aside from adding one extra optimization.

While recovery scans the last binary log file, encountering a `Large_transaction_header_event` lets it skip the transaction body. The header carries the offset and type of the transaction's terminating event. `process_large_trx_header_event()` validates that offset (inside the file, ahead of the current position, with room for an event), seeks to it, and remembers the recorded offset and type. On a later iteration, `validate_large_trx_terminal_event()` (called at the top of the scan loop) confirms that the event now sitting at that offset has the recorded type before the transaction's GTID is added to the executed set. A mismatch marks the file as malformed, and the transaction rolls back.

Two cases deliberately skip this shortcut and fall back to an ordinary sequential scan: relay log recovery, because the header's offset refers to the source's binary log rather than the relay log (gated by `is_relay_log_recovery()`), and any run with `source_verify_checksum` on. The five crash window cases are covered in the HLD and not repeated here.

The same validate-then-seek optimization is applied at both places recovery reads a binary log file: when the GTID set is collected (`read_gtids_from_binlog`), and during 2PC crash recovery (`Log_sanitizer`, via the `process_large_trx_header_event()` / `validate_large_trx_terminal_event()` pair named above).

### Savepoint handling

BOLT adds no new savepoint logic; it reuses MySQL's existing `ROLLBACK TO SAVEPOINT` handling (`binlog_savepoint_rollback`). If the transaction has touched a non transactional table, the server writes a `ROLLBACK TO SAVEPOINT` event into the cache as it does today; otherwise, it truncates the cache back to the savepoint position (`restore_savepoint`). The only BOLT specific detail is that the truncation is reserved region aware: the storage layer shifts the requested offset past the reserved header bytes (`IO_CACHE_binlog_cache_storage::truncate`), and when the spilled file is later synced (`flush_and_sync_spilled_file`), any stale bytes past the new logical end are cut with `my_chsize`, so a promoted file still ends exactly at its terminating event. Whether the (possibly shrunk) transaction is promoted is decided normally at commit, by comparing its spilled size against the threshold.

### Refactoring / modularization

The BOLT commit path shares a lot with the existing group-commit path (THD commit-state setup, GTID-event field derivation) and with the existing binlog cache. Rather than duplicate that logic, and to keep the new code self-contained, the shared and BOLT-specific pieces were split into dedicated files:

- `transaction_commit_helper.{h,cc}` holds two pieces that the code both commit paths use: `init_thd_variables()` and `Transaction_gtid_header`.

  - `init_thd_variables()` sets up the THD's commit state (commit error, \`next_to_commit\` linkage, and in debug builds the preempt flag).
  - \`Transaction_gtid_header\` collects the six values that a transaction's \`Gtid_log_event\` is built from: \`last_committed\`, \`sequence_number\`, the original and immediate commit timestamps, and the original and immediate server versions, which are derived from the THD and the dependency tracker.

<!-- -->

- `large_trx_commit.{h,cc}` holds the BOLT-only eligibility and promotion entry points (`get_cache_for_large_trx_commit`, `commit_large_transaction`, `write_promoted_binlog_header`).
- `cache_data.h` holds `binlog_cache_data` and the cache manager, moved out of `binlog.cc` so the separate `large_trx_commit.cc` translation unit can use them.

## Section 3: Feature compatibility

### Semi-sync replication (compatible)

BOLT works with semi synchronous replication: the optimized commit path invokes the same replication hooks as `ordered_commit()`, so a promoted transaction is acknowledged by a replica like any other. Three semi sync hooks sit on the BOLT commit path: `after_flush`, `after_sync`, and `after_commit`.

- `after_flush`**:** runs inside `promote_spilled_file()`, as the second to last step of the promotion, immediately before the binary log end position is published. This differs slightly from `ordered_commit()`: in `promote_spilled_file`, `after_flush` is not strictly called after the flush stage, but rather after the file has already been promoted, which means it has already been synced. The hook is placed here because:

  - It must run after `open_binlog()` has made the promoted file the active binary log. The hook needs to report the binlog file that contains the transaction, and it reports it through the `log_file_name` variable, which is the server's current binary log, so we must run the hook after the promoted file becomes the active one.
  - It must run before the end position is published, as in the standard commit path.
- `after_sync`**:** runs after the `after_flush` observer and before the transaction is committed, with `LOCK_log` released and `LOCK_commit` still held. This is the same lock state and the same relative position as in `ordered_commit()`.
- `after_commit`**:** runs inside `finish_commit()`, under `LOCK_commit`. The standard path releases `LOCK_commit` and acquires `LOCK_after_commit` before running this hook, so BOLT holds the commit lock longer for this hook. This was chosen deliberately, for two reasons:

  - It only extends the lock hold time; it does not introduce any deadlock. The semi sync plugin acquires no server side binlog mutex, so nothing in the acknowledgement path can deadlock against the commit lock. The wait is also bounded: it is a timed wait against `rpl_semi_sync_source_timeout`.
  - The extra hold time only materializes when `rpl_semi_sync_source_wait_point` is AFTER_COMMIT, which is the non default setting.

### Group replication (incompatible)

BOLT is not compatible with Group Replication and is disabled while Group Replication is running.

The incompatibility comes from the fact that BOLT writes a checksum into the binlog cache. On the standard path, a transaction's events are serialized into the cache without a checksum, and the checksum is added later, when the cache is copied into the binary log. Unlike the standard path, BOLT promotes the temp file itself into a binary log file, so there is no copy step at which to add the checksum.

Group Replication reads the same binlog cache through the `before_commit` observer and broadcasts its bytes to the group as is, expecting the cache to have no per event checksum. A cache written with BOLT enabled carries checksums and breaks that contract.

With Group Replication running, the optimization is off for the transaction from its first event, so no checksum is written into the cache. Two checks guard this: (1) at the first write into the binlog cache, and (2) when deciding whether to commit through the optimized path. The first check ensures that no checksum is written into the cache, while the second catches a transaction that started before Group Replication began.

There is no inherent challenge in adding BOLT support for Group Replication, but it is out of scope for this PR.

### Encryption (incompatible)

BOLT does not work with encryption. A large transaction is promoted only when neither its spilled file is encryption nor the `binlog_encryption` is set to ON:

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

The second check is in `promote_spilled_file`, which rechecks the system variable again after we acquire `LOCK_log`:

```cpp
MYSQL_BIN_LOG::Promote_outcome MYSQL_BIN_LOG::promote_spilled_file() {
  if (rpl_encryption.is_enabled())
    return fallback_outcome(Large_trx_fallback_reason::kEncryption);
}
```

`binlog_encryption` is a dynamic variable, so it can change between `get_cache_for_large_trx_commit` and the actual promotion of the temp file. Rechecking under `LOCK_log` guarantees the correctness of the encryption check, regardless of whether `Rpl_encryption::enable` or BOLT acquires `LOCK_log` first.

If `Rpl_encryption::enable` acquires `LOCK_log` first, then BOLT is blocked in `commit_large_transaction` before it tries to promote the temp file. By the time BOLT acquires `LOCK_log`, `Rpl_encryption::enable` has completed and `Rpl_encryption::is_enabled` returns true, so BOLT sees that encryption is enabled and falls back to the standard commit code path.

If BOLT acquires `LOCK_log` first, there are two sub cases:

- If `Rpl_encryption::enable` has started but not yet finished recovering the master key (`recover_master_key()` is still in progress), `Rpl_encryption::is_enabled` returns false, so BOLT proceeds with writing a plaintext file. The same gap exists in `Binlog_ofile::open()` and BOLT has the same behaviour. 
- If `Rpl_encryption::enable` has completed `recover_master_key()` and is blocked on `rotate_logs()` because it needs `LOCK_log` to rotate, then `Rpl_encryption::is_enabled` returns true at this point, so BOLT falls back.

---